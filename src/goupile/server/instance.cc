// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "domain.hh"
#include "goupile.hh"
#include "instance.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

// If you change InstanceVersion, don't forget to update the migration switch!
const int InstanceVersion = 27;

static std::atomic_int64_t next_unique;

bool InstanceHolder::Open(const char *key, const char *filename)
{
    RG_DEFER_N(err_guard) { Close(); };
    Close();

    unique = next_unique++;
    LogDebug("Open instance '%1' (%2)", key, unique);

    this->key = DuplicateString(key, &str_alloc);
    this->filename = DuplicateString(filename, &str_alloc).ptr;

    // Open database
    if (!db.Open(filename, SQLITE_OPEN_READWRITE))
        return false;

    // Check schema version
    {
        int version;
        if (!db.GetUserVersion(&version))
            return false;

        if (version > InstanceVersion) {
            LogError("Schema of '%1' is too recent (%2, expected %3)", filename, version, InstanceVersion);
            return false;
        } else if (version < InstanceVersion) {
            LogError("Schema of '%1' is outdated", filename);
            return false;
        }
    }

    // Load configuration
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT key, value FROM fs_settings", &stmt))
            return false;

        bool valid = true;

        while (stmt.Next()) {
            const char *key = (const char *)sqlite3_column_text(stmt, 0);
            const char *value = (const char *)sqlite3_column_text(stmt, 1);

            if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
                if (TestStr(key, "Title")) {
                    config.title = DuplicateString(value, &str_alloc).ptr;
                } else if (TestStr(key, "UseOffline")) {
                    valid &= ParseBool(value, &config.use_offline);
                } else if (TestStr(key, "MaxFileSize")) {
                    valid &= ParseInt(value, &config.max_file_size);
                } else if (TestStr(key, "BackupKey")) {
                    config.backup_key = DuplicateString(value, &str_alloc).ptr;
                } else {
                    LogError("Unknown setting '%1'", key);
                    valid = false;
                }
            }
        }
        if (!stmt.IsValid() || !valid)
            return false;
    }

    // Check configuration
    if (!Validate())
        return false;

    err_guard.Disable();
    return true;
}

bool InstanceHolder::Validate()
{
    bool valid = true;

    // Settings
    if (!config.title) {
        config.title = key.ptr;
    }
    if (config.max_file_size <= 0) {
        LogError("Maximum file size must be >= 0");
        valid = false;
    }

    return valid;
}

void InstanceHolder::Close()
{
    if (filename) {
        LogDebug("Close instance '%1' (%2)", key, unique);
    }

    key = {};
    filename = nullptr;
    unique = -1;
    db.Close();
    config = {};
    str_alloc.ReleaseAll();
}

bool InstanceHolder::Checkpoint()
{
    return db.Checkpoint();
}

bool MigrateInstance(sq_Database *db)
{
    BlockAllocator temp_alloc;

    // Database filename
    const char *filename;
    {
        sq_Statement stmt;
        if (!db->Prepare("PRAGMA database_list", &stmt))
            return false;
        if (!stmt.Next())
            return false;

        filename = (const char *)sqlite3_column_text(stmt, 2);
        filename = filename && filename[0] ? DuplicateString(filename, &temp_alloc).ptr : nullptr;
    }

    int version;
    if (!db->GetUserVersion(&version))
        return false;

    if (version > InstanceVersion) {
        LogError("Schema of '%1' is too recent (%2, expected %3)", filename, version, InstanceVersion);
        return false;
    } else if (version == InstanceVersion) {
        return true;
    }

    LogInfo("Migrate instance '%1': %2 to %3",
            SplitStrReverseAny(filename, RG_PATH_SEPARATORS), version + 1, InstanceVersion);

    bool success = db->Transaction([&]() {
        switch (version) {
            case 0: {
                bool success = db->RunMany(R"(
                    CREATE TABLE rec_entries (
                        table_name TEXT NOT NULL,
                        id TEXT NOT NULL,
                        sequence INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX rec_entries_ti ON rec_entries (table_name, id);

                    CREATE TABLE rec_fragments (
                        table_name TEXT NOT NULL,
                        id TEXT NOT NULL,
                        page TEXT,
                        username TEXT NOT NULL,
                        mtime TEXT NOT NULL,
                        complete INTEGER CHECK(complete IN (0, 1)) NOT NULL,
                        json TEXT
                    );
                    CREATE INDEX rec_fragments_tip ON rec_fragments(table_name, id, page);

                    CREATE TABLE rec_columns (
                        table_name TEXT NOT NULL,
                        page TEXT NOT NULL,
                        key TEXT NOT NULL,
                        prop TEXT,
                        before TEXT,
                        after TEXT
                    );
                    CREATE UNIQUE INDEX rec_columns_tpkp ON rec_columns (table_name, page, key, prop);

                    CREATE TABLE rec_sequences (
                        table_name TEXT NOT NULL,
                        sequence INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX rec_sequences_t ON rec_sequences (table_name);

                    CREATE TABLE sched_resources (
                        schedule TEXT NOT NULL,
                        date TEXT NOT NULL,
                        time INTEGER NOT NULL,

                        slots INTEGER NOT NULL,
                        overbook INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX sched_resources_sdt ON sched_resources (schedule, date, time);

                    CREATE TABLE sched_meetings (
                        schedule TEXT NOT NULL,
                        date TEXT NOT NULL,
                        time INTEGER NOT NULL,

                        identity TEXT NOT NULL
                    );
                    CREATE INDEX sched_meetings_sd ON sched_meetings (schedule, date, time);

                    CREATE TABLE usr_users (
                        username TEXT NOT NULL,
                        password_hash TEXT NOT NULL,

                        permissions INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX usr_users_u ON usr_users (username);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 1: {
                bool success = db->RunMany(R"(
                    ALTER TABLE rec_fragments RENAME TO rec_fragments_BAK;
                    DROP INDEX rec_fragments_tip;

                    CREATE TABLE rec_fragments (
                        table_name TEXT NOT NULL,
                        id TEXT NOT NULL,
                        page TEXT,
                        username TEXT NOT NULL,
                        mtime TEXT NOT NULL,
                        complete INTEGER CHECK(complete IN (0, 1)) NOT NULL,
                        json TEXT,
                        anchor INTEGER PRIMARY KEY AUTOINCREMENT
                    );

                    INSERT INTO rec_fragments (table_name, id, page, username, mtime, complete, json)
                        SELECT table_name, id, page, username, mtime, complete, json FROM rec_fragments_BAK;
                    DROP TABLE rec_fragments_BAK;

                    CREATE INDEX rec_fragments_tip ON rec_fragments(table_name, id, page);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 2: {
                bool success = db->RunMany(R"(
                    DROP INDEX rec_entries_ti;
                    DROP INDEX rec_fragments_tip;
                    DROP INDEX rec_columns_tpkp;
                    DROP INDEX rec_sequences_t;

                    ALTER TABLE rec_entries RENAME COLUMN table_name TO store;
                    ALTER TABLE rec_fragments RENAME COLUMN table_name TO store;
                    ALTER TABLE rec_columns RENAME COLUMN table_name TO store;
                    ALTER TABLE rec_sequences RENAME COLUMN table_name TO store;

                    CREATE UNIQUE INDEX rec_entries_si ON rec_entries (store, id);
                    CREATE INDEX rec_fragments_sip ON rec_fragments(store, id, page);
                    CREATE UNIQUE INDEX rec_columns_spkp ON rec_columns (store, page, key, prop);
                    CREATE UNIQUE INDEX rec_sequences_s ON rec_sequences (store);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 3: {
                bool success = db->RunMany(R"(
                    CREATE TABLE adm_migrations (
                        version INTEGER NOT NULL,
                        build TEXT NOT NULL,
                        time INTEGER NOT NULL
                    );
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 4: {
                if (!db->RunMany("UPDATE usr_users SET permissions = 31 WHERE permissions == 7"))
                    return false;
            } [[fallthrough]];

            case 5: {
                // Incomplete migration that breaks down (because NOT NULL constraint)
                // if there is any fragment, which is not ever the case yet.
                bool success = db->RunMany(R"(
                    ALTER TABLE rec_entries ADD COLUMN json TEXT NOT NULL;
                    ALTER TABLE rec_entries ADD COLUMN version INTEGER NOT NULL;
                    ALTER TABLE rec_fragments ADD COLUMN version INEGER NOT NULL;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 6: {
                bool success = db->RunMany(R"(
                    DROP INDEX rec_columns_spkp;
                    ALTER TABLE rec_columns RENAME COLUMN key TO variable;
                    CREATE UNIQUE INDEX rec_fragments_siv ON rec_fragments(store, id, version);
                    CREATE UNIQUE INDEX rec_columns_spvp ON rec_columns (store, page, variable, IFNULL(prop, 0));
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 7: {
                bool success = db->RunMany(R"(
                    ALTER TABLE rec_columns RENAME TO rec_columns_BAK;
                    DROP INDEX rec_columns_spvp;

                    CREATE TABLE rec_columns (
                        store TEXT NOT NULL,
                        page TEXT NOT NULL,
                        variable TEXT NOT NULL,
                        prop TEXT,
                        before TEXT,
                        after TEXT,
                        anchor INTEGER NOT NULL
                    );

                    INSERT INTO rec_columns (store, page, variable, prop, before, after, anchor)
                        SELECT store, page, variable, prop, before, after, 0 FROM rec_columns_BAK;
                    CREATE UNIQUE INDEX rec_columns_spvp ON rec_columns (store, page, variable, IFNULL(prop, 0));

                    DROP TABLE rec_columns_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 8: {
                if (!db->RunMany("UPDATE usr_users SET permissions = 63 WHERE permissions == 31"))
                    return false;
            } [[fallthrough]];

            case 9: {
                bool success = db->RunMany(R"(
                    DROP TABLE rec_columns;

                    CREATE TABLE rec_columns (
                        key TEXT NOT NULL,

                        store TEXT NOT NULL,
                        page TEXT NOT NULL,
                        variable TEXT NOT NULL,
                        type TEXT NOT NULL,
                        prop TEXT,
                        before TEXT,
                        after TEXT,

                        anchor INTEGER NOT NULL
                    );

                    CREATE UNIQUE INDEX rec_columns_k ON rec_columns (key);
                    CREATE INDEX rec_columns_sp ON rec_columns (store, page);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 10: {
                bool success = db->RunMany(R"(
                    ALTER TABLE rec_entries ADD COLUMN zone TEXT;
                    ALTER TABLE usr_users ADD COLUMN zone TEXT;

                    CREATE INDEX rec_entries_z ON rec_entries (zone);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 11: {
                bool success = db->RunMany(R"(
                    CREATE TABLE adm_events (
                        time INTEGER NOT NULL,
                        address TEXT,
                        type TEXT NOT NULL,
                        details TEXT NOT NULL
                    );
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 12: {
                bool success = db->RunMany(R"(
                    ALTER TABLE adm_events RENAME COLUMN details TO username;
                    ALTER TABLE adm_events ADD COLUMN zone TEXT;
                    ALTER TABLE adm_events ADD COLUMN details TEXT;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 13: {
                bool success = db->RunMany(R"(
                    CREATE TABLE fs_files (
                        path TEXT NOT NULL,
                        blob BLOB,
                        compression TEXT,
                        sha256 TEXT
                    );

                    CREATE INDEX fs_files_p ON fs_files (path);
                )");
                if (!success)
                    return false;

                if (version && filename) {
                    Span<const char> root_directory = GetPathDirectory(filename);
                    const char *files_directory = Fmt(&temp_alloc, "%1%/files", root_directory).ptr;

                    HeapArray<const char *> filenames;
                    if (!EnumerateFiles(files_directory, nullptr, -1, -1, &temp_alloc, &filenames))
                        return false;

                    Size relative_offset = strlen(files_directory);

                    for (const char *filename: filenames) {
                        HeapArray<uint8_t> gzip;
                        char sha256[65];
                        {
                            StreamReader reader(filename);
                            StreamWriter writer(&gzip, "<gzip>", CompressionType::Gzip);

                            crypto_hash_sha256_state state;
                            crypto_hash_sha256_init(&state);

                            while (!reader.IsEOF()) {
                                LocalArray<uint8_t, 16384> buf;
                                buf.len = reader.Read(buf.data);
                                if (buf.len < 0)
                                    return false;

                                writer.Write(buf);
                                crypto_hash_sha256_update(&state, buf.data, buf.len);
                            }

                            bool success = writer.Close();
                            RG_ASSERT(success);

                            uint8_t hash[crypto_hash_sha256_BYTES];
                            crypto_hash_sha256_final(&state, hash);
                            FormatSha256(hash, sha256);
                        }

                        Span<char> path = Fmt(&temp_alloc, "/files%1", filename + relative_offset);
#ifdef _WIN32
                        for (char &c: path) {
                            c = (c == '\\') ? '/' : c;
                        }
#endif

                        if (!db->Run(R"(INSERT INTO fs_files (path, blob, compression, sha256)
                                        VALUES (?, ?, ?, ?))", path, gzip, "Gzip", sha256))
                            return false;
                    }
                }
            } [[fallthrough]];

            case 14: {
                bool success = db->RunMany(R"(
                    CREATE TABLE fs_settings (
                        key TEXT NOT NULL,
                        value TEXT
                    );

                    CREATE UNIQUE INDEX fs_settings_k ON fs_settings (key);
                )");

                // Default settings
                {
                    decltype(DomainHolder::config) fake1;
                    decltype(InstanceHolder::config) fake2;

                    const char *sql = "INSERT INTO fs_settings (key, value) VALUES (?, ?)";
                    success &= db->Run(sql, "Application.Name", fake2.title);
                    success &= db->Run(sql, "Application.ClientKey", nullptr);
                    success &= db->Run(sql, "Application.UseOffline", 0 + fake2.use_offline);
                    success &= db->Run(sql, "Application.MaxFileSize", fake2.max_file_size);
                    success &= db->Run(sql, "Application.SyncMode", "Offline");
                    success &= db->Run(sql, "Application.DemoUser", fake1.demo_user);
                    success &= db->Run(sql, "HTTP.SocketType", SocketTypeNames[(int)fake1.http.sock_type]);
                    success &= db->Run(sql, "HTTP.Port", fake1.http.port);
                    success &= db->Run(sql, "HTTP.MaxConnections", fake1.http.max_connections);
                    success &= db->Run(sql, "HTTP.IdleTimeout", fake1.http.idle_timeout);
                    success &= db->Run(sql, "HTTP.Threads", fake1.http.threads);
                    success &= db->Run(sql, "HTTP.AsyncThreads", fake1.http.async_threads);
                    success &= db->Run(sql, "HTTP.BaseUrl", nullptr);
                    success &= db->Run(sql, "HTTP.MaxAge", fake1.max_age);
                }

                // Convert INI settings (if any)
                if (version && filename) {
                    Span<const char> directory = GetPathDirectory(filename);
                    const char *ini_filename = Fmt(&temp_alloc, "%1%/goupile.ini", directory).ptr;

                    StreamReader st(ini_filename);

                    IniParser ini(&st);
                    ini.PushLogFilter();
                    RG_DEFER { PopLogFilter(); };

                    const char *sql = R"(INSERT INTO fs_settings (key, value) VALUES (?, ?)
                                         ON CONFLICT DO UPDATE SET value = excluded.value)";

                    IniProperty prop;
                    while (ini.Next(&prop)) {
                        if (prop.section == "Application") {
                            do {
                                if (prop.key == "Key") {
                                    success &= db->Run(sql, "Application.ClientKey", prop.value);
                                } else if (prop.key == "Name") {
                                    success &= db->Run(sql, "Application.Name", prop.value);
                                } else {
                                    LogError("Unknown attribute '%1'", prop.key);
                                    success = false;
                                }
                            } while (ini.NextInSection(&prop));
                        } else if (prop.section == "Data") {
                            do {
                                if (prop.key == "FilesDirectory") {
                                    // Ignored
                                } else if (prop.key == "DatabaseFile") {
                                    // Ignored
                                } else {
                                    LogError("Unknown attribute '%1'", prop.key);
                                    success = false;
                                }
                            } while (ini.NextInSection(&prop));
                        } else if (prop.section == "Sync") {
                            do {
                                if (prop.key == "UseOffline") {
                                    bool value;
                                    success &= ParseBool(prop.value, &value);
                                    success &= db->Run(sql, "Application.UseOffline", 0 + value);
                                } else if (prop.key == "MaxFileSize") {
                                    int value;
                                    success &= ParseInt(prop.value, &value);
                                    success &= db->Run(sql, "Application.MaxFileSize", value);
                                } else if (prop.key == "SyncMode") {
                                    success &= db->Run(sql, "Application.SyncMode", prop.value);
                                } else {
                                    LogError("Unknown attribute '%1'", prop.key);
                                    success = false;
                                }
                            } while (ini.NextInSection(&prop));
                        } else if (prop.section == "HTTP") {
                            do {
                                if (prop.key == "SocketType") {
                                    success &= db->Run(sql, "HTTP.SocketType", prop.value);
                                } else if (prop.key == "Port") {
                                    int value;
                                    success &= ParseInt(prop.value, &value);
                                    success &= db->Run(sql, "HTTP.Port", value);
                                } else if (prop.key == "MaxConnections") {
                                    int value;
                                    success &= ParseInt(prop.value, &value);
                                    success &= db->Run(sql, "HTTP.MaxConnections", value);
                                } else if (prop.key == "IdleTimeout") {
                                    int value;
                                    success &= ParseInt(prop.value, &value);
                                    success &= db->Run(sql, "HTTP.IdleTimeout", value);
                                } else if (prop.key == "Threads") {
                                    int value;
                                    success &= ParseInt(prop.value, &value);
                                    success &= db->Run(sql, "HTTP.Threads", value);
                                } else if (prop.key == "AsyncThreads") {
                                    int value;
                                    success &= ParseInt(prop.value, &value);
                                    success &= db->Run(sql, "HTTP.AsyncThreads", value);
                                } else if (prop.key == "BaseUrl") {
                                    success &= db->Run(sql, "HTTP.BaseUrl", prop.value);
                                } else if (prop.key == "MaxAge") {
                                    int value;
                                    success &= ParseInt(prop.value, &value);
                                    success &= db->Run(sql, "HTTP.MaxAge", value);
                                } else {
                                    LogError("Unknown attribute '%1'", prop.key);
                                    success = false;
                                }
                            } while (ini.NextInSection(&prop));
                        } else {
                            LogError("Unknown section '%1'", prop.section);
                            while (ini.NextInSection(&prop));
                            success = false;
                        }
                    }
                }

                if (!success)
                    return false;
            } [[fallthrough]];

            case 15: {
                bool success = db->RunMany(R"(
                    DROP INDEX sched_resources_sdt;
                    DROP INDEX sched_meetings_sd;
                    DROP TABLE sched_meetings;
                    DROP TABLE sched_resources;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 16: {
                bool success = db->RunMany(R"(
                    ALTER TABLE fs_files ADD COLUMN size INTEGER;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 17: {
                bool success = db->RunMany(R"(
                    UPDATE fs_settings SET key = 'Application.BaseUrl' WHERE key = 'HTTP.BaseUrl';
                    UPDATE fs_settings SET key = 'Application.AppKey' WHERE key = 'Application.ClientKey';
                    UPDATE fs_settings SET key = 'Application.AppName' WHERE key = 'Application.Name';
                    DELETE FROM fs_settings WHERE key NOT LIKE 'Application.%' OR key = 'Application.DemoUser';
                    UPDATE fs_settings SET key = REPLACE(key, 'Application.', '');

                    CREATE TABLE dom_permissions (
                        username TEXT NOT NULL,
                        permissions INTEGER NOT NULL,
                        zone TEXT
                    );
                    INSERT INTO dom_permissions (username, permissions, zone)
                        SELECT username, permissions, zone FROM usr_users;
                    CREATE UNIQUE INDEX dom_permissions_u ON dom_permissions (username);

                    DROP TABLE adm_events;
                    DROP TABLE usr_users;
                )");

                if (version) {
                    LogInfo("Existing instance users must be recreated on main database");
                }

                if (!success)
                    return false;
            } [[fallthrough]];

            case 18: {
                bool success = db->RunMany(R"(
                    DROP TABLE dom_permissions;
                )");

                if (version) {
                    LogInfo("Existing instance permissions must be recreated on main database");
                }

                if (!success)
                    return false;
            } [[fallthrough]];

            case 19: {
                bool success = db->RunMany(R"(
                    ALTER TABLE fs_files RENAME TO fs_files_BAK;
                    DROP INDEX fs_files_p;

                    CREATE TABLE fs_files (
                        path TEXT NOT NULL,
                        active INTEGER CHECK(active IN (0, 1)) NOT NULL,
                        mtime INTEGER NOT NULL,
                        blob BLOB NOT NULL,
                        compression TEXT NOT NULL,
                        sha256 TEXT NOT NULL,
                        size INTEGER NOT NULL
                    );
                    INSERT INTO fs_files (path, active, mtime, blob, compression, sha256, size)
                        SELECT path, 1, 0, blob, compression, sha256, 0 FROM fs_files_BAK
                        WHERE sha256 IS NOT NULL;
                    CREATE INDEX fs_files_pa ON fs_files (path, active);

                    DROP TABLE fs_files_BAK;
                )");
                if (!success)
                    return false;

                sq_Statement stmt;
                if (!db->Prepare("SELECT rowid, path, compression FROM fs_files", &stmt))
                    return false;

                int64_t mtime = GetUnixTime();

                while (stmt.Next()) {
                    int64_t rowid = sqlite3_column_int64(stmt, 0);
                    const char *path = (const char *)sqlite3_column_text(stmt, 1);

                    CompressionType compression_type;
                    {
                        const char *name = (const char *)sqlite3_column_text(stmt, 2);
                        if (!name || !OptionToEnum(CompressionTypeNames, name, &compression_type)) {
                            LogError("Unknown compression type '%1'", name);
                            return false;
                        }
                    }

                    sqlite3_blob *blob;
                    if (sqlite3_blob_open(*db, "main", "fs_files", "blob", rowid, 0, &blob) != SQLITE_OK) {
                        LogError("SQLite Error: %1", sqlite3_errmsg(*db));
                        return false;
                    }
                    RG_DEFER { sqlite3_blob_close(blob); };

                    Size real_len = 0;
                    if (compression_type == CompressionType::None) {
                        real_len = sqlite3_blob_bytes(blob);
                    } else {
                        Size offset = 0;
                        Size blob_len = sqlite3_blob_bytes(blob);

                        StreamReader reader([&](Span<uint8_t> buf) {
                            Size copy_len = std::min(blob_len - offset, buf.len);

                            if (sqlite3_blob_read(blob, buf.ptr, (int)copy_len, (int)offset) != SQLITE_OK) {
                                LogError("SQLite Error: %1", sqlite3_errmsg(*db));
                                return (Size)-1;
                            }

                            offset += copy_len;
                            return copy_len;
                        }, path, compression_type);

                        while (!reader.IsEOF()) {
                            LocalArray<uint8_t, 16384> buf;
                            buf.len = reader.Read(buf.data);
                            if (buf.len < 0)
                                return false;

                            real_len += buf.len;
                        }
                    }

                    if (!db->Run(R"(UPDATE fs_files SET mtime = ?, size = ?
                                    WHERE active = 1 AND path = ?)", mtime, real_len, path))
                        return false;
                }
                if (!stmt.IsValid())
                    return false;
            } [[fallthrough]];

            case 20: {
                bool success = db->RunMany(R"(
                    DELETE FROM fs_settings WHERE key = 'BaseUrl';
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 21: {
                bool success = db->RunMany(R"(
                    UPDATE fs_settings SET key = 'Title' WHERE key = 'AppName';
                    DELETE FROM fs_settings WHERE key = 'AppKey';
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 22: {
                bool success = db->RunMany(R"(
                    DROP INDEX fs_files_pa;
                    ALTER TABLE fs_files RENAME COLUMN path TO url;
                    CREATE INDEX fs_files_ua ON fs_files (url, active);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 23: {
                bool success = db->RunMany(R"(
                    DROP INDEX fs_files_ua;
                    ALTER TABLE fs_files RENAME COLUMN url TO filename;
                    UPDATE fs_files SET filename = SUBSTR(filename, 8);
                    CREATE INDEX fs_files_fa ON fs_files (filename, active);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 24: {
                bool success = db->RunMany(R"(
                    INSERT INTO fs_settings (key) VALUES ('BackupKey')
                        ON CONFLICT DO NOTHING;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 25: {
                bool success = db->RunMany(R"(
                    DELETE FROM fs_settings WHERE key = 'SyncMode';

                    DROP TABLE rec_columns;
                    DROP TABLE rec_entries;
                    DROP TABLE rec_fragments;
                    DROP TABLE rec_sequences;

                    CREATE TABLE rec_entries (
                        ulid TEXT NOT NULL,
                        hid TEXT,
                        form TEXT NOT NULL,
                        parent_ulid TEXT,
                        parent_version INTEGER,
                        version INTEGER NOT NULL,
                        zone TEXT,
                        anchor INTEGER NOT NULL
                    );
                    CREATE INDEX rec_entries_fz ON rec_entries (form, zone);
                    CREATE UNIQUE INDEX rec_entries_u ON rec_entries (ulid);
                    CREATE UNIQUE INDEX rec_entries_uz ON rec_entries (ulid, zone);

                    CREATE TABLE rec_fragments (
                        anchor INTEGER PRIMARY KEY AUTOINCREMENT,
                        ulid TEXT NOT NULL REFERENCES rec_entries (ulid) DEFERRABLE INITIALLY DEFERRED,
                        version INTEGER NOT NULL,
                        type TEXT NOT NULL,
                        userid INTEGER NOT NULL,
                        username TEXT NOT NULL,
                        mtime TEXT NOT NULL,
                        page TEXT,
                        json BLOB
                    );

                    CREATE UNIQUE INDEX rec_fragments_uv ON rec_fragments (ulid, version);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 26: {
                bool success = db->RunMany(R"(
                    DROP INDEX rec_entries_fz;
                    DROP INDEX rec_entries_u;
                    DROP INDEX rec_entries_uz;
                    DROP INDEX rec_fragments_uv;

                    ALTER TABLE rec_entries RENAME TO rec_entries_BAK;
                    ALTER TABLE rec_fragments RENAME TO rec_fragments_BAK;

                    CREATE TABLE rec_entries (
                        ulid TEXT NOT NULL,
                        hid TEXT,
                        form TEXT NOT NULL,
                        anchor INTEGER NOT NULL,
                        parent_ulid TEXT,
                        parent_version INTEGER
                    );
                    CREATE INDEX rec_entries_f ON rec_entries (form);
                    CREATE UNIQUE INDEX rec_entries_u ON rec_entries (ulid);

                    CREATE TABLE rec_fragments (
                        anchor INTEGER PRIMARY KEY AUTOINCREMENT,
                        ulid TEXT NOT NULL REFERENCES rec_entries (ulid) DEFERRABLE INITIALLY DEFERRED,
                        version INTEGER NOT NULL,
                        type TEXT NOT NULL,
                        userid INTEGER NOT NULL,
                        username TEXT NOT NULL,
                        mtime TEXT NOT NULL,
                        page TEXT,
                        json BLOB
                    );
                    CREATE UNIQUE INDEX rec_fragments_uv ON rec_fragments (ulid, version);

                    INSERT INTO rec_entries (ulid, hid, form, parent_ulid, parent_version, anchor)
                        SELECT ulid, hid, form, parent_ulid, parent_version, anchor FROM rec_entries_BAK;
                    INSERT INTO rec_fragments (anchor, ulid, type, userid, username, mtime, page, json)
                        SELECT anchor, ulid, type, userid, username, mtime, page, json FROM rec_fragments_BAK;
                )");
                if (!success)
                    return false;
            } // [[fallthrough]];

            RG_STATIC_ASSERT(InstanceVersion == 27);
        }

        int64_t time = GetUnixTime();
        if (!db->Run("INSERT INTO adm_migrations (version, build, time) VALUES (?, ?, ?)",
                     InstanceVersion, FelixVersion, time))
            return false;
        if (!db->SetUserVersion(InstanceVersion))
            return false;

        return true;
    });

    return success;
}

bool MigrateInstance(const char *filename)
{
    sq_Database db;

    if (!db.Open(filename, SQLITE_OPEN_READWRITE))
        return false;
    if (!MigrateInstance(&db))
        return false;
    if (!db.Close())
        return false;

    return true;
}

}
