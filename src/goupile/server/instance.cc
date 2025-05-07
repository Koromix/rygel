// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "domain.hh"
#include "file.hh"
#include "goupile.hh"
#include "instance.hh"
#include "vm.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "vendor/miniz/miniz.h"

namespace RG {

// If you change InstanceVersion, don't forget to update the migration switch!
const int InstanceVersion = 131;
const int LegacyVersion = 60;

bool InstanceHolder::Open(int64_t unique, InstanceHolder *master, const char *key, sq_Database *db, bool migrate)
{
    master = master ? master : this;

    this->unique = unique;
    this->master = master;
    this->key = DuplicateString(key, &str_alloc);
    this->db = db;

    // Check schema version
    {
        int version;
        if (!db->GetUserVersion(&version))
            return false;

        if (version > InstanceVersion) {
            const char *filename = sqlite3_db_filename(*db, "main");
            LogError("Schema of '%1' is too recent (%2, expected %3)", filename, version, InstanceVersion);

            return false;
        } else {
            legacy = (version <= LegacyVersion);

            int target = legacy ? LegacyVersion : InstanceVersion;

            if (version < target) {
                if (migrate) {
                    if (!MigrateInstance(db, target))
                        return false;
                } else {
                    const char *filename = sqlite3_db_filename(*db, "main");

                    LogError("Schema of '%1' is outdated", filename);
                    return false;
                }
            }
        }
    }

    // Get whole project settings
    {
        sq_Statement stmt;
        if (!master->db->Prepare("SELECT key, value FROM fs_settings", &stmt))
            return false;

        bool valid = true;

        while (stmt.Step()) {
            const char *setting = (const char *)sqlite3_column_text(stmt, 0);
            const char *value = (const char *)sqlite3_column_text(stmt, 1);

            if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
                if (TestStr(setting, "UseOffline")) {
                    valid &= ParseBool(value, &config.use_offline);
                } else if (TestStr(setting, "DataRemote")) {
                    valid &= ParseBool(value, &config.data_remote);
                } else if (TestStr(setting, "MaxFileSize")) {
                    valid &= ParseSize(value, &config.max_file_size);
                } else if (TestStr(setting, "TokenKey")) {
                    size_t key_len;
                    int ret = sodium_base642bin(config.token_skey, RG_SIZE(config.token_skey),
                                                value, strlen(value), nullptr, &key_len,
                                                nullptr, sodium_base64_VARIANT_ORIGINAL);
                    if (!ret && key_len == 32) {
                        static_assert(RG_SIZE(config.token_pkey) == crypto_scalarmult_BYTES);
                        crypto_scalarmult_base(config.token_pkey, config.token_skey);

                        config.token_key = DuplicateString(value, &str_alloc).ptr;
                    } else {
                        LogError("Malformed TokenKey value");
                        valid = false;
                    }
                } else if (TestStr(setting, "AutoKey")) {
                    config.auto_key = DuplicateString(value, &str_alloc).ptr;
                } else if (TestStr(setting, "AllowGuests")) {
                    valid &= ParseBool(value, &config.allow_guests);
                } else if (TestStr(setting, "FsVersion")) {
                    int version = -1;
                    valid &= ParseInt(value, &version);
                    fs_version = version;
                }
            }
        }
        if (!stmt.IsValid() || !valid)
            return false;
    }

    // Get instance-specific settings
    {
        sq_Statement stmt;
        if (!db->Prepare("SELECT key, value FROM fs_settings", &stmt))
            return false;

        bool valid = true;

        while (stmt.Step()) {
            const char *setting = (const char *)sqlite3_column_text(stmt, 0);
            const char *value = (const char *)sqlite3_column_text(stmt, 1);

            if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
                if (TestStr(setting, "Name")) {
                    config.name = DuplicateString(value, &str_alloc).ptr;
                } else if (TestStr(setting, "LockKey")) {
                    config.lock_key = DuplicateString(value, &str_alloc).ptr;
                } else if (TestStr(setting, "SharedKey")) {
                    config.shared_key = DuplicateString(value, &str_alloc).ptr;
                }
            }
        }
        if (!stmt.IsValid() || !valid)
            return false;
    }

    // Check configuration
    {
        bool valid = true;

        if (!config.name) {
            LogError("Missing instance name");
            valid = false;
        }
        if (config.max_file_size <= 0) {
            LogError("Maximum file size must be >= 0");
            valid = false;
        }

        if (!valid)
            return false;
    }

    // Instance title
    if (master != this) {
        title = Fmt(&str_alloc, "%1 (%2)", master->title, config.name).ptr;
    } else {
        title = config.name;
    }

    // Create challenge key
    static_assert(RG_SIZE(challenge_key) == crypto_secretbox_KEYBYTES);
    randombytes_buf(challenge_key, RG_SIZE(challenge_key));

    return true;
}

bool InstanceHolder::Checkpoint()
{
    return db->Checkpoint();
}

bool InstanceHolder::SyncViews(const char *directory)
{
    RG_ASSERT(master == this);

    BlockAllocator temp_alloc;
    bool logged = false;

    sq_Statement stmt;
    if (!db->Prepare("SELECT version, mtime FROM fs_versions", &stmt))
        return false;

    while (stmt.Step()) {
        int64_t version = sqlite3_column_int64(stmt, 0);
        MZ_TIME_T mtime = (MZ_TIME_T)(sqlite3_column_int64(stmt, 1) / 1000);

        const char *zip_filename = Fmt(&temp_alloc, "%1%/%2_%3.zip", directory, key, version).ptr;

        if (!TestFile(zip_filename, FileType::File)) {
            if (!logged) {
                LogInfo("Exporting new FS views of '%1'", key);
                logged = true;
            }
            LogDebug("Exporting '%1' view for FS version %2", key, version);

            mz_zip_archive zip;
            mz_zip_zero_struct(&zip);
            if (!mz_zip_writer_init_file(&zip, zip_filename, 0)) {
                LogError("Failed to create ZIP archive '%1': %2", zip_filename, mz_zip_get_error_string(zip.m_last_error));
                return false;
            }
            RG_DEFER_N(err_guard) {
                mz_zip_writer_end(&zip);
                UnlinkFile(zip_filename);
            };

            sq_Statement stmt;
            if (!db->Prepare(R"(SELECT o.rowid, i.filename, o.size, o.compression FROM fs_index i
                                INNER JOIN fs_objects o ON (o.sha256 = i.sha256)
                                WHERE i.version = ?1
                                ORDER BY i.filename)", &stmt))
                return false;
            sqlite3_bind_int64(stmt, 1, version);

            while (stmt.Step()) {
                int64_t rowid = sqlite3_column_int64(stmt, 0);
                const char *filename = (const char *)sqlite3_column_text(stmt, 1);
                int64_t size = sqlite3_column_int64(stmt, 2);

                // Simple heuristic, non-compressible files are probably not scripts and
                // JS processes probably don't need them. Probably dumb but it works for now.
                if (!CanCompressFile(filename))
                    continue;

                CompressionType src_encoding;
                {
                    const char *name = (const char *)sqlite3_column_text(stmt, 3);
                    if (!name || !OptionToEnumI(CompressionTypeNames, name, &src_encoding)) {
                        LogError("Unknown compression type '%1'", name);
                        return true;
                    }
                }

                sqlite3_blob *src_blob;
                Size src_len;
                if (sqlite3_blob_open(*db, "main", "fs_objects", "blob", rowid, 0, &src_blob) != SQLITE_OK) {
                    LogError("SQLite Error: %1", sqlite3_errmsg(*db));
                    return false;
                }
                src_len = sqlite3_blob_bytes(src_blob);
                RG_DEFER { sqlite3_blob_close(src_blob); };

                Size offset = 0;
                StreamReader reader([&](Span<uint8_t> buf) {
                    Size copy_len = std::min(src_len - offset, buf.len);

                    if (sqlite3_blob_read(src_blob, buf.ptr, (int)copy_len, (int)offset) != SQLITE_OK) {
                        LogError("SQLite Error: %1", sqlite3_errmsg(*db));
                        return (Size)-1;
                    }

                    offset += copy_len;
                    return copy_len;
                }, filename, src_encoding);

                bool success = mz_zip_writer_add_read_buf_callback(&zip, filename, [](void *ctx, mz_uint64, void *buf, size_t len) {
                    StreamReader *reader = (StreamReader *)ctx;
                    return (size_t)reader->Read(len, buf);
                }, &reader, size, &mtime, nullptr, 0, 0, nullptr, 0, nullptr, 0);
                if (!success)
                    return false;
            }
            if (!stmt.IsValid())
                return false;

            if (!mz_zip_writer_finalize_archive(&zip)) {
                LogError("Failed to finalize ZIP archive '%1': %2", zip_filename, mz_zip_get_error_string(zip.m_last_error));
                return false;
            }
            if (!mz_zip_writer_end(&zip)) {
                LogError("Failed to end ZIP archive '%1': %2", zip_filename, mz_zip_get_error_string(zip.m_last_error));
                return false;
            }

            err_guard.Disable();
        }
    }
    if (!stmt.IsValid())
        return false;

    return true;
}

bool MigrateInstance(sq_Database *db, int target)
{
    RG_ASSERT(!target || target == LegacyVersion || target == InstanceVersion);

    BlockAllocator temp_alloc;

    // Database filename
    const char *filename;
    {
        sq_Statement stmt;
        if (!db->Prepare("PRAGMA database_list", &stmt))
            return false;
        if (!stmt.Step())
            return false;

        filename = (const char *)sqlite3_column_text(stmt, 2);
        filename = filename && filename[0] ? DuplicateString(filename, &temp_alloc).ptr : nullptr;
    }

    int version;
    if (!db->GetUserVersion(&version))
        return false;

    if (!target) {
        if (version > LegacyVersion) {
            target = InstanceVersion;
        } else {
            target = LegacyVersion;
        }
    }

    if (version > target) {
        LogError("Schema of '%1' is too recent (%2, expected %3)", filename, version, InstanceVersion);
        return false;
    } else if (version == target) {
        return true;
    }

    LogInfo("Migrate instance database '%1': %2 to %3",
            SplitStrReverseAny(filename, RG_PATH_SEPARATORS), version, target);

    bool success = db->Transaction([&]() {
        int64_t time = GetUnixTime();

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
                        complete INTEGER CHECK (complete IN (0, 1)) NOT NULL,
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
                        complete INTEGER CHECK (complete IN (0, 1)) NOT NULL,
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

                            do {
                                LocalArray<uint8_t, 16384> buf;
                                buf.len = reader.Read(buf.data);
                                if (buf.len < 0)
                                    return false;

                                writer.Write(buf);
                                crypto_hash_sha256_update(&state, buf.data, buf.len);
                            } while (!reader.IsEOF());

                            bool success2 = writer.Close();
                            RG_ASSERT(success2);

                            uint8_t hash[crypto_hash_sha256_BYTES];
                            crypto_hash_sha256_final(&state, hash);
                            FormatSha256(hash, sha256);
                        }

                        Span<char> path = Fmt(&temp_alloc, "/files%1", filename + relative_offset);
#if defined(_WIN32)
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
                    success &= db->Run(sql, "Application.Name", fake2.name);
                    success &= db->Run(sql, "Application.ClientKey", nullptr);
                    success &= db->Run(sql, "Application.UseOffline", 0 + fake2.use_offline);
                    success &= db->Run(sql, "Application.MaxFileSize", fake2.max_file_size);
                    success &= db->Run(sql, "Application.SyncMode", fake2.data_remote ? "Online" : "Offline");
                    success &= db->Run(sql, "Application.DemoUser", nullptr);
                    success &= db->Run(sql, "HTTP.SocketType", SocketTypeNames[(int)fake1.http.sock_type]);
                    success &= db->Run(sql, "HTTP.Port", fake1.http.port);
                    success &= db->Run(sql, "HTTP.MaxConnections", -1);
                    success &= db->Run(sql, "HTTP.IdleTimeout", fake1.http.idle_timeout);
                    success &= db->Run(sql, "HTTP.Threads", -1);
                    success &= db->Run(sql, "HTTP.AsyncThreads", -1);
                    success &= db->Run(sql, "HTTP.BaseUrl", nullptr);
                    success &= db->Run(sql, "HTTP.MaxAge", 900);
                }

                // Convert INI settings (if any)
                if (version && filename) {
                    Span<const char> directory = GetPathDirectory(filename);
                    const char *ini_filename = Fmt(&temp_alloc, "%1%/goupile.ini", directory).ptr;

                    StreamReader st(ini_filename);

                    IniParser ini(&st);
                    ini.PushLogFilter();
                    RG_DEFER { PopLogFilter(); };

                    const char *sql = R"(INSERT INTO fs_settings (key, value) VALUES (?1, ?2)
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
                        active INTEGER CHECK (active IN (0, 1)) NOT NULL,
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

                while (stmt.Step()) {
                    int64_t rowid = sqlite3_column_int64(stmt, 0);
                    const char *path = (const char *)sqlite3_column_text(stmt, 1);

                    CompressionType compression_type;
                    {
                        const char *name = (const char *)sqlite3_column_text(stmt, 2);
                        if (!name || !OptionToEnumI(CompressionTypeNames, name, &compression_type)) {
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

                        do {
                            LocalArray<uint8_t, 16384> buf;
                            buf.len = reader.Read(buf.data);
                            if (buf.len < 0)
                                return false;

                            real_len += buf.len;
                        } while (!reader.IsEOF());
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
                    INSERT INTO rec_fragments (anchor, ulid, version, type, userid, username, mtime, page, json)
                        SELECT anchor, ulid, version, type, userid, username, mtime, page, json FROM rec_fragments_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 27: {
                decltype(InstanceHolder::config) fake;
                if (!db->Run("INSERT INTO fs_settings (key, value) VALUES ('SyncMode', ?1)",
                             fake.data_remote ? "Online" : "Offline"))
                    return false;
            } [[fallthrough]];

            case 28: {
                bool success = db->RunMany(R"(
                    CREATE INDEX rec_entries_a ON rec_entries (anchor);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 29: {
                char shared_key[45];
                {
                    uint8_t buf[32];
                    FillRandomSafe(buf);
                    sodium_bin2base64(shared_key, RG_SIZE(shared_key), buf, RG_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
                }

                if (!db->Run("INSERT INTO fs_settings (key, value) VALUES ('SharedKey', ?1);", shared_key))
                    return false;
            } [[fallthrough]];

            case 30: {
                bool success = db->RunMany(R"(
                    DROP TABLE IF EXISTS rec_fragments_BAK;
                    DROP TABLE IF EXISTS rec_entries_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 31: {
                sq_Statement stmt;
                if (!db->Prepare("SELECT rowid, filename, size, compression FROM fs_files", &stmt))
                    return false;

                while (stmt.Step()) {
                    int64_t rowid = sqlite3_column_int64(stmt, 0);
                    const char *filename = (const char *)sqlite3_column_text(stmt, 1);
                    Size total_len = (Size)sqlite3_column_int64(stmt, 2);

                    CompressionType compression_type;
                    {
                        const char *name = (const char *)sqlite3_column_text(stmt, 3);
                        if (!name || !OptionToEnumI(CompressionTypeNames, name, &compression_type)) {
                            LogError("Unknown compression type '%1'", name);
                            return false;
                        }
                    }

                    // Do we need to uncompress this entry? If not, skip!
                    if (compression_type == CompressionType::None)
                        continue;
                    if (CanCompressFile(filename))
                        continue;

                    // Open source blob
                    sqlite3_blob *src_blob;
                    Size src_len;
                    if (sqlite3_blob_open(*db, "main", "fs_files", "blob", rowid, 0, &src_blob) != SQLITE_OK) {
                        LogError("SQLite Error: %1", sqlite3_errmsg(*db));
                        return false;
                    }
                    src_len = (Size)sqlite3_blob_bytes(src_blob);
                    RG_DEFER { sqlite3_blob_close(src_blob); };

                    // Insert new entry
                    sqlite3_blob *dest_blob;
                    {
                        if (!db->Run(R"(INSERT INTO fs_files
                                        SELECT * FROM fs_files WHERE rowid = ?1)", rowid))
                            return false;

                        int64_t dest_rowid = sqlite3_last_insert_rowid(*db);

                        if (!db->Run(R"(UPDATE fs_files SET compression = 'None', blob = ?2
                                        WHERE rowid = ?1)", dest_rowid, sq_Binding::Zeroblob(total_len)))
                            return false;
                        if (sqlite3_blob_open(*db, "main", "fs_files", "blob", dest_rowid, 1, &dest_blob) != SQLITE_OK) {
                            LogError("SQLite Error: %1", sqlite3_errmsg(*db));
                            return false;
                        }
                    }
                    RG_DEFER { sqlite3_blob_close(dest_blob); };

                    // Init decompressor
                    StreamReader reader;
                    {
                        Size offset = 0;

                        reader.Open([&](Span<uint8_t> buf) {
                            Size copy_len = std::min(src_len - offset, buf.len);

                            if (sqlite3_blob_read(src_blob, buf.ptr, (int)copy_len, (int)offset) != SQLITE_OK) {
                                LogError("SQLite Error: %1", sqlite3_errmsg(*db));
                                return (Size)-1;
                            }

                            offset += copy_len;
                            return copy_len;
                        }, filename, compression_type);
                        if (!reader.IsValid())
                            return false;
                    }

                    // Uncompress!
                    {
                        Size read_len = 0;

                        do {
                            LocalArray<uint8_t, 16384> buf;
                            buf.len = reader.Read(buf.data);
                            if (buf.len < 0)
                                return false;

                            if (buf.len + read_len > total_len) {
                                LogError("Total file size has changed (bigger)");
                                return false;
                            }
                            if (sqlite3_blob_write(dest_blob, buf.data, (int)buf.len, (int)read_len) != SQLITE_OK) {
                                LogError("SQLite Error: %1", sqlite3_errmsg(*db));
                                return false;
                            }

                            read_len += buf.len;
                        } while (!reader.IsEOF());
                        if (read_len < total_len) {
                            LogError("Total file size has changed (truncated)");
                            return false;
                        }
                    }

                    // Delete old entry
                    if (!db->Run("DELETE FROM fs_files WHERE rowid = ?1", rowid))
                        return false;
                }
                if (!stmt.IsValid())
                    return false;
            } [[fallthrough]];

            case 32: {
                char token_key[45];
                {
                    uint8_t buf[32];
                    FillRandomSafe(buf);
                    sodium_bin2base64(token_key, RG_SIZE(token_key), buf, RG_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
                }

                if (!db->Run("INSERT INTO fs_settings (key, value) VALUES ('TokenKey', ?1);", token_key))
                    return false;
            } [[fallthrough]];

            case 33: {
                bool success = db->RunMany(R"(
                    INSERT INTO fs_settings (key) VALUES ('AutoUserID');
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 34: {
                bool success = db->RunMany(R"(
                    UPDATE fs_settings SET key = 'Name' WHERE key = 'Title';
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 35: {
                bool success = db->RunMany(R"(
                    UPDATE fs_settings SET value = NULL WHERE key = 'SharedKey';
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 36: {
                bool success = db->RunMany(R"(
                    DROP INDEX rec_entries_a;
                    DROP INDEX rec_entries_f;
                    DROP INDEX rec_entries_u;
                    DROP INDEX rec_fragments_uv;

                    ALTER TABLE rec_entries RENAME TO rec_entries_BAK;
                    ALTER TABLE rec_fragments RENAME TO rec_fragments_BAK;

                    CREATE TABLE rec_entries (
                        ulid TEXT NOT NULL,
                        hid TEXT,
                        form TEXT NOT NULL,
                        parent_ulid TEXT,
                        parent_version INTEGER,
                        anchor INTEGER NOT NULL,
                        root_ulid TEXT NOT NULL
                    );
                    CREATE INDEX rec_entries_f ON rec_entries (form);
                    CREATE UNIQUE INDEX rec_entries_u ON rec_entries (ulid);
                    CREATE INDEX rec_entries_a ON rec_entries (anchor);

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

                    INSERT INTO rec_entries (ulid, hid, form, parent_ulid, parent_version, anchor, root_ulid)
                        SELECT ulid, hid, form, parent_ulid, parent_version, anchor, ulid FROM rec_entries_BAK;
                    INSERT INTO rec_fragments (anchor, ulid, version, type, userid, username, mtime, page, json)
                        SELECT anchor, ulid, version, type, userid, username, mtime, page, json FROM rec_fragments_BAK;

                    -- I guess some kind of recursive CTE would be better but I'm too lazy
                    UPDATE rec_entries SET root_ulid = p.root_ulid
                        FROM (SELECT ulid, root_ulid FROM rec_entries) AS p
                        WHERE parent_ulid IS NOT NULL AND parent_ulid = p.ulid;
                    UPDATE rec_entries SET root_ulid = p.root_ulid
                        FROM (SELECT ulid, root_ulid FROM rec_entries) AS p
                        WHERE parent_ulid IS NOT NULL AND parent_ulid = p.ulid;
                    UPDATE rec_entries SET root_ulid = p.root_ulid
                        FROM (SELECT ulid, root_ulid FROM rec_entries) AS p
                        WHERE parent_ulid IS NOT NULL AND parent_ulid = p.ulid;
                    UPDATE rec_entries SET root_ulid = p.root_ulid
                        FROM (SELECT ulid, root_ulid FROM rec_entries) AS p
                        WHERE parent_ulid IS NOT NULL AND parent_ulid = p.ulid;
                    UPDATE rec_entries SET root_ulid = p.root_ulid
                        FROM (SELECT ulid, root_ulid FROM rec_entries) AS p
                        WHERE parent_ulid IS NOT NULL AND parent_ulid = p.ulid;
                    UPDATE rec_entries SET root_ulid = p.root_ulid
                        FROM (SELECT ulid, root_ulid FROM rec_entries) AS p
                        WHERE parent_ulid IS NOT NULL AND parent_ulid = p.ulid;
                    UPDATE rec_entries SET root_ulid = p.root_ulid
                        FROM (SELECT ulid, root_ulid FROM rec_entries) AS p
                        WHERE parent_ulid IS NOT NULL AND parent_ulid = p.ulid;
                    UPDATE rec_entries SET root_ulid = p.root_ulid
                        FROM (SELECT ulid, root_ulid FROM rec_entries) AS p
                        WHERE parent_ulid IS NOT NULL AND parent_ulid = p.ulid;

                    DROP TABLE rec_fragments_BAK;
                    DROP TABLE rec_entries_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 37: {
                bool success = db->RunMany(R"(
                    INSERT INTO fs_settings (key) VALUES ('AutoKey');
                    UPDATE fs_settings SET key = 'AutoUser' WHERE key = 'AutoUserID';

                    CREATE TABLE usr_auto (
                        key TEXT NOT NULL,
                        userid INTEGER PRIMARY KEY AUTOINCREMENT,
                        local_key TEXT NOT NULL,
                        ulid TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX usr_auto_k ON usr_auto (key);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 38: {
                bool success = db->RunMany(R"(
                    CREATE TABLE rec_sequences (
                        form TEXT NOT NULL,
                        counter INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX rec_sequences_f ON rec_sequences (form);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 39: {
                bool success = db->RunMany(R"(
                    DROP INDEX rec_entries_a;
                    DROP INDEX rec_entries_f;
                    DROP INDEX rec_entries_u;
                    DROP INDEX rec_fragments_uv;

                    ALTER TABLE rec_entries RENAME TO rec_entries_BAK;
                    ALTER TABLE rec_fragments RENAME TO rec_fragments_BAK;

                    CREATE TABLE rec_entries (
                        ulid TEXT NOT NULL,
                        hid BLOB,
                        form TEXT NOT NULL,
                        parent_ulid TEXT,
                        parent_version INTEGER,
                        anchor INTEGER NOT NULL,
                        root_ulid TEXT NOT NULL
                    );
                    CREATE INDEX rec_entries_f ON rec_entries (form);
                    CREATE UNIQUE INDEX rec_entries_u ON rec_entries (ulid);
                    CREATE INDEX rec_entries_a ON rec_entries (anchor);

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

                    INSERT INTO rec_entries (ulid, hid, form, parent_ulid, parent_version, anchor, root_ulid)
                        SELECT ulid, hid, form, parent_ulid, parent_version, anchor, root_ulid FROM rec_entries_BAK;
                    INSERT INTO rec_fragments (anchor, ulid, version, type, userid, username, mtime, page, json)
                        SELECT anchor, ulid, version, type, userid, username, mtime, page, json FROM rec_fragments_BAK;

                    DROP TABLE rec_fragments_BAK;
                    DROP TABLE rec_entries_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 40: {
                bool success = db->RunMany(R"(
                    CREATE TABLE fs_versions (
                        version INTEGER PRIMARY KEY AUTOINCREMENT,
                        mtime INTEGER NOT NULL,
                        userid INTEGER NOT NULL,
                        username TEXT NOT NULL,
                        atomic CHECK (atomic IN (0, 1)) NOT NULL
                    );

                    CREATE TABLE fs_objects (
                        sha256 TEXT PRIMARY KEY NOT NULL,
                        mtime INTEGER NOT NULL,
                        compression TEXT NOT NULL,
                        size INTEGER NOT NULL,
                        blob BLOB NOT NULL
                    );

                    CREATE TABLE fs_index (
                        version REFERENCES fs_versions (version),
                        filename TEXT NOT NULL,
                        sha256 TEXT NOT NULL REFERENCES fs_objects (sha256)
                    );
                    CREATE UNIQUE INDEX fs_index_vf ON fs_index (version, filename);
                )");
                if (!success)
                    return false;

                if (version) {
                    // Migrate from old fs_files table to new schema
                    if (!db->Run(R"(INSERT INTO fs_versions (version, mtime, userid, username, atomic)
                                    VALUES (1, ?1, 0, 'goupile', 1))", time))
                        return false;
                    if (!db->Run(R"(INSERT INTO fs_objects (sha256, mtime, compression, size, blob)
                                    SELECT sha256, mtime, compression, size, blob FROM fs_files WHERE active = 1)"))
                        return false;
                    if (!db->Run(R"(INSERT INTO fs_index (version, filename, sha256)
                                    SELECT 1, filename, sha256 FROM fs_files WHERE active = 1)"))
                        return false;
                    if (!db->Run("DROP TABLE fs_files"))
                        return false;

                    if (!db->Run("INSERT INTO fs_settings (key, value) VALUES ('FsVersion', 1)"))
                        return false;
                } else {
                    if (!db->Run("INSERT INTO fs_settings (key, value) VALUES ('FsVersion', 0)"))
                        return false;
                }
            } [[fallthrough]];

            case 41: {
                bool success = db->RunMany(R"(
                    UPDATE fs_settings SET key = 'DefaultUser' WHERE key = 'AutoUser';
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 42: {
                bool success = db->RunMany(R"(
                    DROP INDEX usr_auto_k;

                    ALTER TABLE usr_auto RENAME TO ins_users;
                    CREATE UNIQUE INDEX ins_users_k ON ins_users (key);

                    CREATE TABLE ins_claims (
                        userid INTEGER NOT NULL REFERENCES ins_users (userid),
                        ulid TEXT NOT NULL REFERENCES rec_entries (ulid) DEFERRABLE INITIALLY DEFERRED
                    );
                    CREATE UNIQUE INDEX ins_claims_uu ON ins_claims (userid, ulid);

                    INSERT INTO ins_claims (userid, ulid)
                        SELECT userid, ulid FROM ins_users
                        WHERE ulid IN (SELECT ulid FROM rec_entries WHERE parent_ulid IS NULL);
                    ALTER TABLE ins_users DROP COLUMN ulid;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 43: {
                bool success = db->RunMany(R"(
                    INSERT INTO fs_settings (key, value) VALUES ('AllowGuests', 0);
                    DELETE FROM fs_settings WHERE key = 'DefaultUser';
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 44: {
                bool success = db->RunMany(R"(
                    DROP TABLE IF EXISTS fs_files;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 45: {
                bool success = db->RunMany(R"(
                    DROP INDEX rec_entries_a;
                    DROP INDEX rec_entries_f;
                    DROP INDEX rec_entries_u;
                    DROP INDEX rec_fragments_uv;

                    ALTER TABLE rec_entries RENAME TO rec_entries_BAK;
                    ALTER TABLE rec_fragments RENAME TO rec_fragments_BAK;

                    CREATE TABLE rec_entries (
                        ulid TEXT NOT NULL,
                        hid BLOB,
                        form TEXT NOT NULL,
                        parent_ulid TEXT,
                        parent_version INTEGER,
                        anchor INTEGER NOT NULL,
                        root_ulid TEXT NOT NULL,
                        deleted INTEGER CHECK (deleted IN (0, 1)) NOT NULL
                    );
                    CREATE INDEX rec_entries_f ON rec_entries (form);
                    CREATE UNIQUE INDEX rec_entries_u ON rec_entries (ulid);
                    CREATE INDEX rec_entries_a ON rec_entries (anchor);

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

                    INSERT INTO rec_entries (ulid, hid, form, parent_ulid, parent_version, anchor, root_ulid, deleted)
                        SELECT e.ulid, e.hid, e.form, e.parent_ulid, e.parent_version, e.anchor, e.root_ulid,
                               IIF(f.type = 'delete', 1, 0) FROM rec_entries_BAK e
                        INNER JOIN rec_fragments_BAK f ON (f.anchor = e.anchor);
                    INSERT INTO rec_fragments (anchor, ulid, version, type, userid, username, mtime, page, json)
                        SELECT anchor, ulid, version, type, userid, username, mtime, page, json FROM rec_fragments_BAK;

                    DROP TABLE rec_fragments_BAK;
                    DROP TABLE rec_entries_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 46: {
                bool success = db->RunMany(R"(
                    DROP INDEX ins_claims_uu;

                    ALTER TABLE ins_claims RENAME TO ins_claims_BAK;

                    CREATE TABLE ins_claims (
                        userid INTEGER NOT NULL REFERENCES ins_users (userid),
                        ulid TEXT NOT NULL REFERENCES rec_entries (ulid) DEFERRABLE INITIALLY DEFERRED
                    );
                    CREATE UNIQUE INDEX ins_claims_uu ON ins_claims (userid, ulid);

                    INSERT INTO ins_claims (userid, ulid)
                        SELECT userid, ulid FROM ins_claims_BAK;

                    DROP TABLE ins_claims_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 47: {
                bool success = db->RunMany(R"(
                    CREATE TABLE seq_counters (
                        type TEXT NOT NULL,
                        key TEXT NOT NULL,
                        counter INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX seq_counters_tk ON seq_counters (type, key);

                    INSERT INTO seq_counters (type, key, counter)
                        SELECT 'hid', form, counter FROM rec_sequences;

                    DROP INDEX rec_sequences_f;
                    DROP TABLE rec_sequences;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 48: {
                if (version) {
                    sq_Statement stmt;
                    if (!db->Prepare("SELECT value FROM fs_settings WHERE key = 'FsVersion'", &stmt))
                        return false;

                    if (!stmt.Step()) {
                        if (stmt.IsValid()) {
                            LogError("Missing 'FsVersion' setting");
                        }
                        return false;
                    }

                    int64_t version = sqlite3_column_int64(stmt, 0);

                    if (!db->Run(R"(INSERT INTO fs_versions (version, mtime, userid, username, atomic)
                                        SELECT 0, v.mtime, v.userid, v.username, 0 FROM fs_versions v
                                        WHERE v.version = ?1)", version))
                        return false;
                    if (!db->Run(R"(INSERT INTO fs_index (version, filename, sha256)
                                        SELECT 0, i.filename, i.sha256 FROM fs_index i
                                        WHERE i.version = ?1)", version))
                        return false;
                }
            } [[fallthrough]];

            case 49: {
                bool success = db->RunMany(R"(
                    DROP INDEX rec_fragments_uv;

                    ALTER TABLE rec_fragments RENAME TO rec_fragments_BAK;

                    CREATE TABLE rec_fragments (
                        anchor INTEGER PRIMARY KEY AUTOINCREMENT,
                        ulid TEXT NOT NULL REFERENCES rec_entries (ulid) DEFERRABLE INITIALLY DEFERRED,
                        version INTEGER NOT NULL,
                        type TEXT NOT NULL,
                        userid INTEGER NOT NULL,
                        username TEXT NOT NULL,
                        mtime TEXT NOT NULL,
                        fs INTEGER NOT NULL,
                        page TEXT,
                        json BLOB
                    );
                    CREATE UNIQUE INDEX rec_fragments_uv ON rec_fragments (ulid, version);

                    INSERT INTO rec_fragments (anchor, ulid, version, type, userid, username, mtime, fs, page, json)
                        SELECT f.anchor, f.ulid, f.version, f.type, f.userid,
                               f.username, f.mtime, s.value, f.page, f.json FROM rec_fragments_BAK f
                        INNER JOIN fs_settings s ON (s.key = 'FsVersion');

                    DROP TABLE rec_fragments_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 50: {
                char lock_key[45];
                {
                    uint8_t buf[32];
                    FillRandomSafe(buf);
                    sodium_bin2base64(lock_key, RG_SIZE(lock_key), buf, RG_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
                }

                if (!db->Run("INSERT INTO fs_settings (key, value) VALUES ('LockKey', ?1);", lock_key))
                    return false;
            } [[fallthrough]];

            case 51: {
                bool success = db->RunMany(R"(
                    DROP INDEX ins_claims_uu;

                    ALTER TABLE ins_claims RENAME TO ins_claims_BAK;

                    CREATE TABLE ins_claims (
                        userid INTEGER NOT NULL,
                        ulid TEXT NOT NULL REFERENCES rec_entries (ulid) DEFERRABLE INITIALLY DEFERRED
                    );
                    CREATE UNIQUE INDEX ins_claims_uu ON ins_claims (userid, ulid);

                    INSERT INTO ins_claims (userid, ulid)
                        SELECT userid, ulid FROM ins_claims_BAK;

                    DROP TABLE ins_claims_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 52: {
                bool success = db->RunMany(R"(
                    DROP INDEX rec_entries_a;
                    DROP INDEX rec_entries_f;
                    DROP INDEX rec_entries_u;
                    DROP INDEX rec_fragments_uv;

                    ALTER TABLE rec_entries RENAME TO rec_entries_BAK;
                    ALTER TABLE rec_fragments RENAME TO rec_fragments_BAK;

                    CREATE TABLE rec_entries (
                        ulid TEXT NOT NULL,
                        form TEXT NOT NULL,
                        sequence INTEGER NOT NULL,
                        hid BLOB,
                        parent_ulid TEXT,
                        parent_version INTEGER,
                        anchor INTEGER NOT NULL,
                        root_ulid TEXT NOT NULL,
                        deleted INTEGER CHECK (deleted IN (0, 1)) NOT NULL
                    );
                    CREATE INDEX rec_entries_f ON rec_entries (form);
                    CREATE UNIQUE INDEX rec_entries_fs ON rec_entries (form, sequence);
                    CREATE UNIQUE INDEX rec_entries_u ON rec_entries (ulid);
                    CREATE INDEX rec_entries_a ON rec_entries (anchor);

                    CREATE TABLE rec_fragments (
                        anchor INTEGER PRIMARY KEY AUTOINCREMENT,
                        ulid TEXT NOT NULL REFERENCES rec_entries (ulid) DEFERRABLE INITIALLY DEFERRED,
                        version INTEGER NOT NULL,
                        type TEXT NOT NULL,
                        userid INTEGER NOT NULL,
                        username TEXT NOT NULL,
                        mtime TEXT NOT NULL,
                        fs INTEGER NOT NULL,
                        page TEXT,
                        json BLOB
                    );
                    CREATE UNIQUE INDEX rec_fragments_uv ON rec_fragments (ulid, version);

                    INSERT INTO rec_entries (ulid, form, sequence, hid, parent_ulid, parent_version, anchor, root_ulid, deleted)
                        SELECT ulid, form, IIF(typeof(hid) = 'integer', hid, ulid), hid, parent_ulid, parent_version, anchor, root_ulid, deleted FROM rec_entries_BAK;
                    INSERT INTO rec_fragments (anchor, ulid, version, type, userid, username, mtime, fs, page, json)
                        SELECT anchor, ulid, version, type, userid, username, mtime, fs, page, json FROM rec_fragments_BAK;

                    DROP TABLE rec_fragments_BAK;
                    DROP TABLE rec_entries_BAK;

                    UPDATE seq_counters SET type = 'record' WHERE type = 'hid';
                )");
                if (!success)
                    return false;

                sq_Statement stmt;
                if (!db->Prepare(R"(SELECT rowid, form, sequence
                                    FROM rec_entries ORDER BY rowid)", &stmt))
                    return false;

                HashMap<const char *, int64_t> sequences;

                while (stmt.Step()) {
                    int64_t rowid = sqlite3_column_int64(stmt, 0);
                    const char *form = (const char *)sqlite3_column_text(stmt, 1);
                    int type = sqlite3_column_type(stmt, 2);

                    auto ptr = sequences.table.TrySetDefault(form);

                    if (!ptr->value) {
                        ptr->key = DuplicateString(form, &temp_alloc).ptr;
                    }

                    if (type == SQLITE_TEXT) {
                        for (;;) {
                            int64_t counter = ++ptr->value;

                            PushLogFilter([](LogLevel, const char *, const char *, FunctionRef<LogFunc>) {});
                            RG_DEFER { PopLogFilter(); };

                            if (db->Run("UPDATE rec_entries SET sequence = ?2 WHERE rowid = ?1", rowid, counter))
                                break;
                            if (sqlite3_errcode(*db) != SQLITE_CONSTRAINT) {
                                LogError("SQLite Error: %1", sqlite3_errmsg(*db));
                                return false;
                            }
                        }
                    } else if (type == SQLITE_INTEGER) {
                        int64_t value = sqlite3_column_int64(stmt, 2);
                        ptr->value = std::max(ptr->value, value);
                    }
                }
                if (!stmt.IsValid())
                    return false;

                for (const auto &it: sequences.table) {
                    if (!db->Run("UPDATE seq_counters SET counter = max(counter, ?2) WHERE type = 'record' AND key = ?1", it.key, it.value))
                        return false;
                }
            } [[fallthrough]];

            case 53: {
                bool success = db->RunMany(R"(
                    DROP INDEX ins_claims_uu;

                    ALTER TABLE ins_claims RENAME TO ins_claims_BAK;

                    CREATE TABLE ins_claims (
                        userid INTEGER NOT NULL,
                        ulid TEXT NOT NULL REFERENCES rec_entries (ulid) DEFERRABLE INITIALLY DEFERRED
                    );
                    CREATE UNIQUE INDEX ins_claims_uu ON ins_claims (userid, ulid);

                    INSERT INTO ins_claims (userid, ulid)
                        SELECT userid, ulid FROM ins_claims_BAK;

                    DROP TABLE ins_claims_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 54: {
                bool success = db->RunMany(R"(
                    UPDATE rec_entries SET deleted = 1
                        FROM (SELECT MAX(anchor) AS anchor FROM rec_fragments
                              WHERE type = 'delete' GROUP BY ulid) AS deleted
                        WHERE rec_entries.anchor = deleted.anchor;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 55: {
                bool success = db->RunMany(R"(
                    UPDATE fs_objects SET mtime = mtime * 1000 WHERE mtime < 10000000000;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 56: {
                bool success = db->RunMany(R"(
                    ALTER TABLE rec_fragments ADD COLUMN tags TEXT;
                    UPDATE rec_fragments SET tags = '[]' WHERE type = 'save';
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 57: {
                bool success = db->RunMany(R"(
                    INSERT INTO seq_counters (type, key, counter)
                        SELECT 'record', form, MAX(sequence) AS sequence FROM rec_entries GROUP BY 2
                        ON CONFLICT (type, key) DO UPDATE SET counter = excluded.counter;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 58: {
                bool success = db->RunMany(R"(
                    INSERT INTO seq_counters (type, key, counter)
                        SELECT 'record', form, MAX(sequence) AS sequence FROM rec_entries GROUP BY 2
                        ON CONFLICT (type, key) DO UPDATE SET counter = excluded.counter;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 59: {
                bool success = db->RunMany(R"(
                    INSERT INTO seq_counters (type, key, counter)
                        SELECT 'record', form, MAX(sequence) AS sequence FROM rec_entries GROUP BY 2
                        ON CONFLICT (type, key) DO UPDATE SET counter = excluded.counter;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 60: {
                if (target == LegacyVersion)
                    break;
            } [[fallthrough]];

            case 100: {
                bool success = db->RunMany(R"(
                    DROP INDEX rec_entries_a;
                    DROP INDEX rec_entries_f;
                    DROP INDEX rec_entries_fs;
                    DROP INDEX rec_entries_u;
                    DROP INDEX rec_fragments_uv;
                    DROP INDEX ins_claims_uu;

                    ALTER TABLE rec_entries RENAME TO rec_entries_BAK;
                    ALTER TABLE rec_fragments RENAME TO rec_fragments_BAK;
                    ALTER TABLE ins_claims RENAME TO ins_claims_BAK;

                    CREATE TABLE rec_threads (
                        tid TEXT NOT NULL,
                        deleted INTEGER CHECK (deleted IN (0, 1)) NOT NULL
                    );
                    CREATE UNIQUE INDEX rec_threads_t ON rec_threads (tid);

                    CREATE TABLE rec_entries (
                        tid TEXT NOT NULL REFERENCES rec_threads (tid),
                        eid TEXT NOT NULL,
                        anchor INTEGER NOT NULL,
                        ctime INTEGER NOT NULL,
                        mtime INTEGER NOT NULL,
                        store TEXT NOT NULL,
                        context TEXT NOT NULL,
                        sequence INTEGER NOT NULL,
                        hid BLOB,
                        data BLOB
                    );
                    CREATE UNIQUE INDEX rec_entries_ts ON rec_entries (tid, store);
                    CREATE UNIQUE INDEX rec_entries_e ON rec_entries (eid);
                    CREATE UNIQUE INDEX rec_entries_cs ON rec_entries (context, sequence);

                    CREATE TABLE rec_fragments (
                        anchor INTEGER PRIMARY KEY AUTOINCREMENT,
                        previous INTEGER REFERENCES rec_fragments (anchor),
                        tid TEXT NOT NULL REFERENCES rec_threads (tid),
                        eid TEXT NOT NULL REFERENCES rec_entries (eid),
                        userid INTEGER NOT NULL,
                        username TEXT NOT NULL,
                        mtime INTEGER NOT NULL,
                        fs INTEGER NOT NULL,
                        data BLOB,
                        page TEXT
                    );
                    CREATE INDEX rec_fragments_t ON rec_fragments (tid);
                    CREATE INDEX rec_fragments_r ON rec_fragments (eid);

                    CREATE TABLE ins_claims (
                        userid INTEGER NOT NULL,
                        tid TEXT NOT NULL REFERENCES rec_threads (tid)
                    );
                    CREATE UNIQUE INDEX ins_claims_ut ON ins_claims (userid, tid);

                    CREATE TABLE mig_threads (
                        tid TEXT NOT NULL,
                        sequence INTEGER NOT NULL,
                        hid TEXT
                    );

                    CREATE TABLE mig_deletions (
                        tid TEXT NOT NULL
                    );

                    INSERT INTO rec_threads (tid, deleted)
                        SELECT root_ulid, deleted FROM rec_entries_BAK WHERE ulid = root_ulid;
                    INSERT INTO rec_entries (tid, eid, anchor, ctime, mtime, store, context, sequence, hid, data)
                        SELECT root_ulid, ulid, anchor, -1, -1, form, IIF(ulid <> root_ulid, parent_ulid || '/', '') || form,
                               sequence, hid, '{}' FROM rec_entries_BAK;
                    INSERT INTO rec_fragments (anchor, previous, tid, eid, userid, username, mtime, fs, data, page)
                        SELECT f.anchor, p.anchor, e.root_ulid, f.ulid, f.userid,
                               f.username, CAST(strftime('%s', f.mtime) AS INTEGER) * 1000 +
                                           MOD(CAST(strftime('%f', f.mtime) AS REAL) * 1000, 1000),
                               f.fs, IIF(f.type = 'save', json_patch('{}', f.json), NULL), f.page FROM rec_fragments_BAK f
                        INNER JOIN rec_entries_BAK e ON (e.ulid = f.ulid)
                        LEFT JOIN rec_fragments_BAK p ON (p.ulid = f.ulid AND p.version = f.version - 1);
                    INSERT INTO ins_claims (userid, tid)
                        SELECT userid, ulid FROM ins_claims_BAK;

                    INSERT INTO mig_threads (tid, sequence, hid)
                        SELECT root_ulid, sequence, hid FROM rec_entries_BAK
                        WHERE ulid = root_ulid AND deleted = 0;

                    INSERT INTO mig_deletions (tid)
                        SELECT ulid FROM rec_entries_BAK WHERE deleted = 1;

                    DROP TABLE rec_fragments_BAK;
                    DROP TABLE rec_entries_BAK;
                    DROP TABLE ins_claims_BAK;
                )");
                if (!success)
                    return false;

                sq_Statement stmt;
                if (!db->Prepare("SELECT eid, mtime, data FROM rec_fragments WHERE data IS NOT NULL ORDER BY anchor", &stmt))
                    return false;

                while (stmt.Step()) {
                    const char *eid = (const char *)sqlite3_column_text(stmt, 0);
                    int64_t mtime = sqlite3_column_int64(stmt, 1);
                    const char *json = (const char *)sqlite3_column_text(stmt, 2);

                    if (!db->Run("UPDATE rec_entries SET ctime = ?2 WHERE eid = ?1 AND ctime < 0", eid, mtime))
                        return false;
                    if (!db->Run("UPDATE rec_entries SET mtime = ?2 WHERE eid = ?1", eid, mtime))
                        return false;
                    if (json && !db->Run("UPDATE rec_entries SET data = json_patch(data, ?2) WHERE eid = ?1", eid, json))
                        return false;
                }
                if (!stmt.IsValid())
                    return false;

                if (!db->Run("DELETE FROM rec_fragments WHERE eid IN (SELECT eid FROM rec_entries WHERE ctime < 0)"))
                    return false;
                if (!db->Run("DELETE FROM rec_entries WHERE ctime < 0"))
                    return false;
            } [[fallthrough]];

            case 101: {
                bool success = db->RunMany(R"(
                    DROP INDEX rec_fragments_t;
                    DROP INDEX rec_fragments_r;

                    ALTER TABLE rec_fragments RENAME TO rec_fragments_BAK;

                    CREATE TABLE rec_fragments (
                        anchor INTEGER PRIMARY KEY AUTOINCREMENT,
                        previous INTEGER REFERENCES rec_fragments (anchor),
                        tid TEXT NOT NULL REFERENCES rec_threads (tid),
                        eid TEXT NOT NULL REFERENCES rec_entries (eid) DEFERRABLE INITIALLY DEFERRED,
                        userid INTEGER NOT NULL,
                        username TEXT NOT NULL,
                        mtime INTEGER NOT NULL,
                        fs INTEGER NOT NULL,
                        data BLOB,
                        page TEXT
                    );
                    CREATE INDEX rec_fragments_t ON rec_fragments (tid);
                    CREATE INDEX rec_fragments_r ON rec_fragments (eid);

                    INSERT INTO rec_fragments
                        SELECT * FROM rec_fragments_BAK;

                    DROP TABLE rec_fragments_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 102: {
                bool success = db->RunMany(R"(
                    DELETE FROM fs_settings WHERE key = 'SharedKey';
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 103: {
                bool success = db->RunMany(R"(
                    DELETE FROM seq_counters WHERE type = 'record';
                    INSERT INTO seq_counters (type, key, counter)
                        SELECT 'record', store, MAX(sequence) AS sequence FROM rec_entries GROUP BY 2;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 104: {
                bool success = db->RunMany(R"(
                    DROP INDEX rec_threads_t;
                    DROP INDEX rec_entries_ts;
                    DROP INDEX rec_entries_e;
                    DROP INDEX rec_entries_cs;
                    DROP INDEX rec_fragments_t;
                    DROP INDEX rec_fragments_r;
                    DROP INDEX ins_claims_ut;

                    ALTER TABLE rec_threads RENAME TO rec_threads_BAK;
                    ALTER TABLE rec_entries RENAME TO rec_entries_BAK;
                    ALTER TABLE rec_fragments RENAME TO rec_fragments_BAK;
                    ALTER TABLE ins_claims RENAME TO ins_claims_BAK;

                    CREATE TABLE rec_threads (
                        tid TEXT NOT NULL,
                        stores TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX rec_threads_t ON rec_threads (tid);

                    CREATE TABLE rec_entries (
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED,
                        eid TEXT NOT NULL,
                        anchor INTEGER NOT NULL,
                        ctime INTEGER NOT NULL,
                        mtime INTEGER NOT NULL,
                        store TEXT NOT NULL,
                        sequence INTEGER NOT NULL,
                        deleted INTEGER CHECK (deleted IN (0, 1)) NOT NULL,
                        data BLOB
                    );
                    CREATE UNIQUE INDEX rec_entries_ts ON rec_entries (tid, store);
                    CREATE UNIQUE INDEX rec_entries_e ON rec_entries (eid);
                    CREATE UNIQUE INDEX rec_entries_ss ON rec_entries (store, sequence);

                    CREATE TABLE rec_fragments (
                        anchor INTEGER PRIMARY KEY AUTOINCREMENT,
                        previous INTEGER REFERENCES rec_fragments (anchor),
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED,
                        eid TEXT NOT NULL REFERENCES rec_entries (eid) DEFERRABLE INITIALLY DEFERRED,
                        userid INTEGER NOT NULL,
                        username TEXT NOT NULL,
                        mtime INTEGER NOT NULL,
                        fs INTEGER NOT NULL,
                        data BLOB,
                        page TEXT
                    );
                    CREATE INDEX rec_fragments_t ON rec_fragments (tid);
                    CREATE INDEX rec_fragments_r ON rec_fragments (eid);

                    CREATE TABLE ins_claims (
                        userid INTEGER NOT NULL,
                        tid TEXT NOT NULL REFERENCES rec_threads (tid)
                    );
                    CREATE UNIQUE INDEX ins_claims_ut ON ins_claims (userid, tid);

                    INSERT INTO rec_threads (tid, stores)
                        SELECT tid, '{}' FROM rec_threads_BAK;
                    INSERT INTO rec_entries (tid, eid, anchor, ctime, mtime, store, sequence, deleted, data)
                        SELECT e.tid, e.eid, e.anchor, e.ctime, e.mtime, e.store,
                               e.sequence, IIF(f.data IS NULL, 1, 0), e.data FROM rec_entries_BAK e
                        INNER JOIN rec_fragments_BAK f ON (f.anchor = e.anchor);
                    INSERT INTO rec_fragments
                        SELECT * FROM rec_fragments_BAK;
                    INSERT INTO ins_claims
                        SELECT * FROM ins_claims_BAK;

                    DROP TABLE ins_claims_BAK;
                    DROP TABLE rec_fragments_BAK;
                    DROP TABLE rec_entries_BAK;
                    DROP TABLE rec_threads_BAK;

                    DELETE FROM seq_counters WHERE type = 'record';
                    INSERT INTO seq_counters (type, key, counter)
                        SELECT 'record', store, MAX(sequence) AS sequence FROM rec_entries GROUP BY 2;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 105: {
                bool success = db->RunMany(R"(
                    ALTER TABLE rec_entries ADD COLUMN new_data TEXT;
                    ALTER TABLE rec_fragments ADD COLUMN new_data TEXT;
                    UPDATE rec_entries SET new_data = data;
                    UPDATE rec_fragments SET new_data = data;
                    ALTER TABLE rec_entries DROP COLUMN data;
                    ALTER TABLE rec_entries RENAME COLUMN new_data TO data;
                    ALTER TABLE rec_fragments DROP COLUMN data;
                    ALTER TABLE rec_fragments RENAME COLUMN new_data TO data;

                    ALTER TABLE rec_entries ADD COLUMN notes TEXT;
                    ALTER TABLE rec_fragments ADD COLUMN notes TEXT;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 106: {
                bool success = db->RunMany(R"(
                    CREATE TABLE rec_tags (
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED,
                        eid TEXT NOT NULL REFERENCES rec_entries (eid) DEFERRABLE INITIALLY DEFERRED,
                        name TEXT NOT NULL
                    );

                    CREATE INDEX rec_tags_t ON rec_tags (tid);
                    CREATE INDEX rec_tags_n ON rec_tags (name);
                    CREATE UNIQUE INDEX rec_tags_en ON rec_tags (eid, name);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 107: {
                bool success = db->RunMany(R"(
                    ALTER TABLE rec_entries ADD COLUMN tags TEXT;
                    ALTER TABLE rec_fragments ADD COLUMN tags TEXT;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 108: {
                bool success = db->RunMany(R"(
                    ALTER TABLE rec_entries RENAME COLUMN notes TO meta;
                    ALTER TABLE rec_fragments RENAME COLUMN notes TO meta;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 109: {
                bool success = db->RunMany(R"(
                    CREATE TABLE rec_constraints (
                        eid TEXT NOT NULL REFERENCES rec_entries (eid) DEFERRABLE INITIALLY DEFERRED,
                        store TEXT NOT NULL,
                        key TEXT NOT NULL,
                        mandatory INTEGER CHECK (mandatory IN (0, 1)) NOT NULL,
                        value TEXT,

                        CHECK ((value IS NOT NULL AND value <> '') OR mandatory = 0)
                    );

                    CREATE UNIQUE INDEX rec_constraints_skv ON rec_constraints (store, key, value);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 110: {
                bool success = db->RunMany(R"(
                    DROP INDEX rec_constraints_skv;
                    ALTER TABLE rec_constraints RENAME TO seq_constraints;
                    CREATE UNIQUE INDEX seq_constraints_skv ON seq_constraints (store, key, value);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 111: {
                bool success = db->RunMany(R"(
                    ALTER TABLE rec_threads ADD COLUMN locked NOT NULL DEFAULT 0;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 112: {
                bool success = db->RunMany(R"(
                    DROP INDEX ins_claims_ut;

                    ALTER TABLE ins_claims RENAME TO ins_claims_BAK;

                    CREATE TABLE ins_claims (
                        userid INTEGER NOT NULL,
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED
                    );
                    CREATE UNIQUE INDEX ins_claims_ut ON ins_claims (userid, tid);

                    INSERT INTO ins_claims (userid, tid)
                        SELECT userid, tid FROM ins_claims_BAK;

                    DROP TABLE ins_claims_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 113: {
                bool success = db->RunMany(R"(
                    ALTER TABLE rec_threads DROP COLUMN stores;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 114: {
                if (!db->ColumnExists("rec_fragments", "page") &&
                        !db->Run("ALTER TABLE rec_fragments ADD COLUMN page TEXT"))
                    return false;

                bool success = db->RunMany(R"(
                    DROP INDEX rec_fragments_t;
                    DROP INDEX rec_fragments_r;

                    ALTER TABLE rec_fragments RENAME TO rec_fragments_BAK;

                    CREATE TABLE rec_fragments (
                        anchor INTEGER PRIMARY KEY AUTOINCREMENT,
                        previous INTEGER REFERENCES rec_fragments (anchor),
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED,
                        eid TEXT NOT NULL REFERENCES rec_entries (eid) DEFERRABLE INITIALLY DEFERRED,
                        userid INTEGER NOT NULL,
                        username TEXT NOT NULL,
                        mtime INTEGER NOT NULL,
                        fs INTEGER,
                        data TEXT,
                        meta TEXT,
                        tags TEXT,
                        page TEXT
                    );
                    CREATE INDEX rec_fragments_t ON rec_fragments (tid);
                    CREATE INDEX rec_fragments_r ON rec_fragments (eid);

                    INSERT INTO rec_fragments (anchor, previous, tid, eid, userid, username, mtime, fs, data, meta, tags, page)
                        SELECT anchor, previous, tid, eid, userid, username, mtime, fs, data, meta, tags, page FROM rec_fragments_BAK;

                    DROP TABLE rec_fragments_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 115: {
                bool success = db->RunMany(R"(
                    UPDATE fs_settings SET key = 'DataRemote', value = IIF(value <> 'Offline', 1, 0) WHERE key = 'SyncMode';
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 116: {
                bool success = db->RunMany(R"(
                    DELETE FROM fs_settings WHERE key = 'BackupKey';
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 117: {
                bool success = db->RunMany(R"(
                    UPDATE rec_fragments SET meta = '{ "children": {}, "notes": { "variables": {} } }' WHERE meta IS NULL;
                    UPDATE rec_fragments SET tags = '[]' WHERE tags IS NULL;

                    UPDATE rec_entries SET meta = '{ "children": {}, "notes": { "variables": {} } }' WHERE meta IS NULL;
                    UPDATE rec_entries SET tags = '[]' WHERE tags IS NULL;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 118: {
                if (!db->ColumnExists("rec_fragments", "page") &&
                        !db->Run("ALTER TABLE rec_fragments ADD COLUMN page TEXT"))
                    return false;

                bool success = db->RunMany(R"(
                    DROP INDEX rec_entries_ts;
                    DROP INDEX rec_entries_e;
                    DROP INDEX rec_entries_ss;
                    DROP INDEX rec_fragments_t;
                    DROP INDEX rec_fragments_r;
                    DROP INDEX rec_tags_t;
                    DROP INDEX rec_tags_n;
                    DROP INDEX rec_tags_en;
                    DROP INDEX seq_constraints_skv;

                    ALTER TABLE rec_entries RENAME TO rec_entries_BAK;
                    ALTER TABLE rec_fragments RENAME TO rec_fragments_BAK;
                    ALTER TABLE rec_tags RENAME TO rec_tags_BAK;
                    ALTER TABLE seq_constraints RENAME TO seq_constraints_BAK;

                    CREATE TABLE rec_entries (
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED,
                        eid TEXT NOT NULL,
                        anchor INTEGER NOT NULL,
                        ctime INTEGER NOT NULL,
                        mtime INTEGER NOT NULL,
                        store TEXT NOT NULL,
                        sequence INTEGER NOT NULL,
                        deleted INTEGER CHECK (deleted IN (0, 1)) NOT NULL,
                        summary TEXT,
                        data TEXT,
                        meta TEXT,
                        tags TEXT
                    );
                    CREATE UNIQUE INDEX rec_entries_ts ON rec_entries (tid, store);
                    CREATE UNIQUE INDEX rec_entries_e ON rec_entries (eid);
                    CREATE UNIQUE INDEX rec_entries_ss ON rec_entries (store, sequence);

                    CREATE TABLE rec_fragments (
                        anchor INTEGER PRIMARY KEY AUTOINCREMENT,
                        previous INTEGER REFERENCES rec_fragments (anchor),
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED,
                        eid TEXT NOT NULL REFERENCES rec_entries (eid) DEFERRABLE INITIALLY DEFERRED,
                        userid INTEGER NOT NULL,
                        username TEXT NOT NULL,
                        mtime INTEGER NOT NULL,
                        fs INTEGER,
                        summary TEXT,
                        data TEXT,
                        meta TEXT,
                        tags TEXT,
                        page TEXT
                    );
                    CREATE INDEX rec_fragments_t ON rec_fragments (tid);
                    CREATE INDEX rec_fragments_r ON rec_fragments (eid);

                    CREATE TABLE rec_tags (
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED,
                        eid TEXT NOT NULL REFERENCES rec_entries (eid) DEFERRABLE INITIALLY DEFERRED,
                        name TEXT NOT NULL
                    );
                    CREATE INDEX rec_tags_t ON rec_tags (tid);
                    CREATE INDEX rec_tags_n ON rec_tags (name);
                    CREATE UNIQUE INDEX rec_tags_en ON rec_tags (eid, name);

                    CREATE TABLE seq_constraints (
                        eid TEXT NOT NULL REFERENCES rec_entries (eid) DEFERRABLE INITIALLY DEFERRED,
                        store TEXT NOT NULL,
                        key TEXT NOT NULL,
                        mandatory INTEGER CHECK (mandatory IN (0, 1)) NOT NULL,
                        value TEXT,

                        CHECK ((value IS NOT NULL AND value <> '') OR mandatory = 0)
                    );
                    CREATE UNIQUE INDEX seq_constraints_skv ON seq_constraints (store, key, value);

                    INSERT INTO rec_entries (tid, eid, anchor, ctime, mtime, store, sequence, deleted, data, meta, tags)
                        SELECT tid, eid, anchor, ctime, mtime, store, sequence, deleted, data, meta, tags FROM rec_entries_BAK;
                    INSERT INTO rec_fragments (anchor, previous, tid, eid, userid, username, mtime, fs, data, meta, tags, page)
                        SELECT anchor, previous, tid, eid, userid, username, mtime, fs, data, meta, tags, page FROM rec_fragments_BAK;
                    INSERT INTO rec_tags (tid, eid, name)
                        SELECT tid, eid, name FROM rec_tags_BAK;
                    INSERT INTO seq_constraints (eid, store, key, mandatory, value)
                        SELECT eid, store, key, mandatory, value FROM seq_constraints_BAK;

                    DROP TABLE rec_entries_BAK;
                    DROP TABLE rec_fragments_BAK;
                    DROP TABLE rec_tags_BAK;
                    DROP TABLE seq_constraints_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 119: {
                bool success = db->RunMany(R"(
                    DROP INDEX rec_fragments_t;
                    DROP INDEX rec_fragments_r;

                    ALTER TABLE rec_fragments RENAME TO rec_fragments_BAK;

                    CREATE TABLE rec_fragments (
                        anchor INTEGER PRIMARY KEY AUTOINCREMENT,
                        previous INTEGER REFERENCES rec_fragments (anchor),
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED,
                        eid TEXT NOT NULL REFERENCES rec_entries (eid) DEFERRABLE INITIALLY DEFERRED,
                        userid INTEGER NOT NULL,
                        username TEXT NOT NULL,
                        mtime INTEGER NOT NULL,
                        fs INTEGER,
                        summary TEXT,
                        data TEXT,
                        meta TEXT,
                        tags TEXT,
                        page TEXT
                    );
                    CREATE INDEX rec_fragments_t ON rec_fragments (tid);
                    CREATE INDEX rec_fragments_r ON rec_fragments (eid);

                    INSERT INTO rec_fragments (anchor, previous, tid, eid, userid, username, mtime, fs, summary, data, meta, tags, page)
                        SELECT anchor, previous, tid, eid, userid, username, mtime, fs, summary, data, meta, tags, page FROM rec_fragments_BAK;

                    DROP TABLE rec_fragments_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 120: {
                bool success = db->RunMany(R"(
                    CREATE TABLE ins_devices (
                        credential_id TEXT NOT NULL,
                        public_key TEXT NOT NULL,
                        algorithm INTEGER NOT NULL,
                        userid INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX ins_devices_c ON ins_devices (credential_id);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 121: {
                bool success = db->RunMany(R"(
                    DROP INDEX rec_threads_t;
                    DROP INDEX rec_entries_ts;
                    DROP INDEX rec_entries_e;
                    DROP INDEX rec_entries_ss;
                    DROP INDEX rec_fragments_t;
                    DROP INDEX rec_fragments_r;
                    DROP INDEX rec_tags_t;
                    DROP INDEX rec_tags_n;
                    DROP INDEX rec_tags_en;
                    DROP INDEX seq_constraints_skv;

                    ALTER TABLE rec_threads RENAME TO rec_threads_BAK;
                    ALTER TABLE rec_entries RENAME TO rec_entries_BAK;
                    ALTER TABLE rec_fragments RENAME TO rec_fragments_BAK;
                    ALTER TABLE rec_tags RENAME TO rec_tags_BAK;
                    ALTER TABLE seq_constraints RENAME TO seq_constraints_BAK;

                    CREATE TABLE rec_threads (
                        sequence INTEGER PRIMARY KEY AUTOINCREMENT,
                        tid TEXT NOT NULL,
                        hid TEXT,
                        locked NOT NULL
                    );
                    CREATE UNIQUE INDEX rec_threads_t ON rec_threads (tid);

                    CREATE TABLE rec_entries (
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED,
                        eid TEXT NOT NULL,
                        anchor INTEGER NOT NULL,
                        ctime INTEGER NOT NULL,
                        mtime INTEGER NOT NULL,
                        store TEXT NOT NULL,
                        sequence INTEGER NOT NULL,
                        deleted INTEGER CHECK (deleted IN (0, 1)) NOT NULL,
                        summary TEXT,
                        data TEXT,
                        meta TEXT,
                        tags TEXT
                    );
                    CREATE UNIQUE INDEX rec_entries_ts ON rec_entries (tid, store);
                    CREATE UNIQUE INDEX rec_entries_e ON rec_entries (eid);
                    CREATE UNIQUE INDEX rec_entries_ss ON rec_entries (store, sequence);

                    CREATE TABLE rec_fragments (
                        anchor INTEGER PRIMARY KEY AUTOINCREMENT,
                        previous INTEGER REFERENCES rec_fragments (anchor),
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED,
                        eid TEXT NOT NULL REFERENCES rec_entries (eid) DEFERRABLE INITIALLY DEFERRED,
                        userid INTEGER NOT NULL,
                        username TEXT NOT NULL,
                        mtime INTEGER NOT NULL,
                        fs INTEGER,
                        summary TEXT,
                        data TEXT,
                        meta TEXT,
                        tags TEXT,
                        page TEXT
                    );
                    CREATE INDEX rec_fragments_t ON rec_fragments (tid);
                    CREATE INDEX rec_fragments_r ON rec_fragments (eid);

                    CREATE TABLE rec_tags (
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED,
                        eid TEXT NOT NULL REFERENCES rec_entries (eid) DEFERRABLE INITIALLY DEFERRED,
                        name TEXT NOT NULL
                    );
                    CREATE INDEX rec_tags_t ON rec_tags (tid);
                    CREATE INDEX rec_tags_n ON rec_tags (name);
                    CREATE UNIQUE INDEX rec_tags_en ON rec_tags (eid, name);

                    CREATE TABLE seq_constraints (
                        eid TEXT NOT NULL REFERENCES rec_entries (eid) DEFERRABLE INITIALLY DEFERRED,
                        store TEXT NOT NULL,
                        key TEXT NOT NULL,
                        mandatory INTEGER CHECK (mandatory IN (0, 1)) NOT NULL,
                        value TEXT,

                        CHECK ((value IS NOT NULL AND value <> '') OR mandatory = 0)
                    );
                    CREATE UNIQUE INDEX seq_constraints_skv ON seq_constraints (store, key, value);

                    INSERT INTO rec_threads (tid, locked)
                        SELECT tid, locked FROM rec_threads_BAK;
                    INSERT INTO rec_entries (tid, eid, anchor, ctime, mtime, store, sequence, deleted, summary, data, meta, tags)
                        SELECT tid, eid, anchor, ctime, mtime, store, sequence, deleted, summary, data, meta, tags FROM rec_entries_BAK;
                    INSERT INTO rec_fragments (anchor, previous, tid, eid, userid, username, mtime, fs, summary, data, meta, tags, page)
                        SELECT anchor, previous, tid, eid, userid, username, mtime, fs, summary, data, meta, tags, page FROM rec_fragments_BAK;
                    INSERT INTO rec_tags (tid, eid, name)
                        SELECT tid, eid, name FROM rec_tags_BAK;
                    INSERT INTO seq_constraints (eid, store, key, mandatory, value)
                        SELECT eid, store, key, mandatory, value FROM seq_constraints_BAK;

                    DROP TABLE rec_threads_BAK;
                    DROP TABLE rec_entries_BAK;
                    DROP TABLE rec_fragments_BAK;
                    DROP TABLE rec_tags_BAK;
                    DROP TABLE seq_constraints_BAK;
                )");
                if (!success)
                    return false;

                if (!db->Run("UPDATE rec_threads SET sequence = -sequence"))
                    return false;

                // Migrate old sequence values
                if (db->TableExists("mig_threads")) {
                    sq_Statement stmt;
                    if (!db->Prepare("SELECT tid, sequence, hid FROM mig_threads ORDER BY sequence", &stmt))
                        return false;

                    while (stmt.Step()) {
                        const char *tid = (const char *)sqlite3_column_text(stmt, 0);
                        int64_t sequence = sqlite3_column_int64(stmt, 1);
                        const char *hid = (const char *)sqlite3_column_text(stmt, 2);

                        if (!db->Run("UPDATE rec_threads SET sequence = ?2, hid = ?3 WHERE tid = ?1", tid, sequence, hid))
                            return false;
                    }
                    if (!stmt.IsValid())
                        return false;
                }

                // Try to keep recent entry sequence numbers
                {
                    sq_Statement stmt;
                    if (!db->Prepare(R"(SELECT t.tid, e.sequence
                                        FROM rec_threads t
                                        INNER JOIN rec_entries e ON (e.tid = t.tid)
                                        ORDER BY t.sequence, e.store)", &stmt))
                        return false;

                    while (stmt.Step()) {
                        const char *tid = (const char *)sqlite3_column_text(stmt, 0);
                        int64_t sequence = sqlite3_column_int64(stmt, 1);

                        if (!tid)
                            continue;

                        if (!db->Run(R"(UPDATE OR IGNORE rec_threads SET sequence = ?2
                                        WHERE tid = ?1 AND sequence < 0)", tid, sequence))
                            return false;
                    }
                    if (!stmt.IsValid())
                        return false;
                }

                int64_t counter;
                {
                    sq_Statement stmt;
                    if (!db->Prepare("SELECT MAX(sequence) FROM rec_threads", &stmt))
                        return false;
                    if (!stmt.GetSingleValue(&counter))
                        return false;

                    counter = std::max((int64_t)1, counter + 1);
                }

                // Renumber recent records
                {
                    sq_Statement stmt;
                    if (!db->Prepare("SELECT sequence FROM rec_threads WHERE sequence < 0", &stmt))
                        return false;

                    while (stmt.Step()) {
                        int64_t sequence = sqlite3_column_int64(stmt, 0);

                        if (!db->Run("UPDATE rec_threads SET sequence = ?2 WHERE sequence = ?1", sequence, counter))
                            return false;

                        counter++;
                    }
                    if (!stmt.IsValid())
                        return false;
                }

                if (!db->Run("UPDATE sqlite_sequence SET seq = ?1 WHERE name = 'rec_threads'", counter - 1))
                    return false;
            } [[fallthrough]];

            case 122: {
                bool success = db->RunMany(R"(
                    DROP INDEX ins_claims_ut;

                    ALTER TABLE ins_claims RENAME TO ins_claims_BAK;

                    CREATE TABLE ins_claims (
                        userid INTEGER NOT NULL,
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED
                    );
                    CREATE UNIQUE INDEX ins_claims_ut ON ins_claims (userid, tid);

                    INSERT INTO ins_claims (userid, tid)
                        SELECT userid, tid FROM ins_claims_BAK;

                    DROP TABLE ins_claims_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 123: {
                bool success = db->RunMany(R"(
                    CREATE TABLE mig_sequences (
                        eid TEXT NOT NULL,
                        sequence INTEGER NOT NULL
                    );

                    INSERT INTO mig_sequences (eid, sequence)
                        SELECT eid, sequence FROM rec_entries;

                    DROP INDEX rec_entries_ss;
                    ALTER TABLE rec_entries DROP COLUMN sequence;

                    DROP TABLE seq_counters;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 124: {
                bool success = db->RunMany(R"(
                    DROP INDEX rec_threads_t;
                    DROP INDEX rec_entries_ts;
                    DROP INDEX rec_entries_e;
                    DROP INDEX rec_fragments_t;
                    DROP INDEX rec_fragments_r;
                    DROP INDEX rec_tags_t;
                    DROP INDEX rec_tags_n;
                    DROP INDEX rec_tags_en;
                    DROP INDEX ins_claims_ut;
                    DROP INDEX seq_constraints_skv;

                    ALTER TABLE rec_threads RENAME TO rec_threads_BAK;
                    ALTER TABLE rec_entries RENAME TO rec_entries_BAK;
                    ALTER TABLE rec_fragments RENAME TO rec_fragments_BAK;
                    ALTER TABLE rec_tags RENAME TO rec_tags_BAK;
                    ALTER TABLE ins_claims RENAME TO ins_claims_BAK;
                    ALTER TABLE seq_constraints RENAME TO seq_constraints_BAK;

                    CREATE TABLE rec_threads (
                        sequence INTEGER PRIMARY KEY AUTOINCREMENT,
                        tid TEXT NOT NULL,
                        hid TEXT,
                        counters TEXT NOT NULL,
                        secrets TEXT NOT NULL,
                        locked NOT NULL
                    );
                    CREATE UNIQUE INDEX rec_threads_t ON rec_threads (tid);

                    CREATE TABLE rec_entries (
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED,
                        eid TEXT NOT NULL,
                        anchor INTEGER NOT NULL,
                        ctime INTEGER NOT NULL,
                        mtime INTEGER NOT NULL,
                        store TEXT NOT NULL,
                        deleted INTEGER CHECK (deleted IN (0, 1)) NOT NULL,
                        summary TEXT,
                        data TEXT,
                        meta TEXT,
                        tags TEXT
                    );
                    CREATE UNIQUE INDEX rec_entries_ts ON rec_entries (tid, store);
                    CREATE UNIQUE INDEX rec_entries_e ON rec_entries (eid);

                    CREATE TABLE rec_fragments (
                        anchor INTEGER PRIMARY KEY AUTOINCREMENT,
                        previous INTEGER REFERENCES rec_fragments (anchor),
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED,
                        eid TEXT NOT NULL REFERENCES rec_entries (eid) DEFERRABLE INITIALLY DEFERRED,
                        userid INTEGER NOT NULL,
                        username TEXT NOT NULL,
                        mtime INTEGER NOT NULL,
                        fs INTEGER,
                        summary TEXT,
                        data TEXT,
                        meta TEXT,
                        tags TEXT,
                        page TEXT
                    );
                    CREATE INDEX rec_fragments_t ON rec_fragments (tid);
                    CREATE INDEX rec_fragments_r ON rec_fragments (eid);

                    CREATE TABLE rec_tags (
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED,
                        eid TEXT NOT NULL REFERENCES rec_entries (eid) DEFERRABLE INITIALLY DEFERRED,
                        name TEXT NOT NULL
                    );
                    CREATE INDEX rec_tags_t ON rec_tags (tid);
                    CREATE INDEX rec_tags_n ON rec_tags (name);
                    CREATE UNIQUE INDEX rec_tags_en ON rec_tags (eid, name);

                    CREATE TABLE ins_claims (
                        userid INTEGER NOT NULL,
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED
                    );
                    CREATE UNIQUE INDEX ins_claims_ut ON ins_claims (userid, tid);

                    CREATE TABLE seq_constraints (
                        eid TEXT NOT NULL REFERENCES rec_entries (eid) DEFERRABLE INITIALLY DEFERRED,
                        store TEXT NOT NULL,
                        key TEXT NOT NULL,
                        mandatory INTEGER CHECK (mandatory IN (0, 1)) NOT NULL,
                        value TEXT,

                        CHECK ((value IS NOT NULL AND value <> '') OR mandatory = 0)
                    );
                    CREATE UNIQUE INDEX seq_constraints_skv ON seq_constraints (store, key, value);

                    INSERT INTO rec_threads (sequence, tid, hid, counters, secrets, locked)
                        SELECT sequence, tid, hid, '{}', '{}', locked FROM rec_threads_BAK;
                    INSERT INTO rec_entries (tid, eid, anchor, ctime, mtime, store, deleted, summary, data, meta, tags)
                        SELECT tid, eid, anchor, ctime, mtime, store, deleted, summary, data, meta, tags FROM rec_entries_BAK;
                    INSERT INTO rec_fragments (anchor, previous, tid, eid, userid, username, mtime, fs, summary, data, meta, tags, page)
                        SELECT anchor, previous, tid, eid, userid, username, mtime, fs, summary, data, meta, tags, page FROM rec_fragments_BAK;
                    INSERT INTO rec_tags (tid, eid, name)
                        SELECT tid, eid, name FROM rec_tags_BAK;
                    INSERT INTO ins_claims (userid, tid)
                        SELECT userid, tid FROM ins_claims_BAK;
                    INSERT INTO seq_constraints (eid, store, key, mandatory, value)
                        SELECT eid, store, key, mandatory, value FROM seq_constraints_BAK;

                    DROP TABLE rec_threads_BAK;
                    DROP TABLE rec_entries_BAK;
                    DROP TABLE rec_fragments_BAK;
                    DROP TABLE rec_tags_BAK;
                    DROP TABLE ins_claims_BAK;
                    DROP TABLE seq_constraints_BAK;

                    CREATE TABLE seq_counters (
                        key TEXT NOT NULL,
                        state INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX seq_counters_k ON seq_counters (key);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 125: {
                bool success = db->RunMany(R"(
                    UPDATE rec_entries
                        SET summary = f.summary
                        FROM (SELECT anchor, summary FROM rec_fragments) AS f
                        WHERE rec_entries.anchor = f.anchor;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 126: {
                bool success = db->RunMany(R"(
                    CREATE TABLE rec_exports (
                        export INTEGER PRIMARY KEY AUTOINCREMENT,
                        ctime INTEGER NOT NULL,
                        sequence INTEGER NOT NULL,
                        anchor INTEGER NOT NULL
                    );
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 127: {
                // New feature, not used by anyone yet, don't bother keeping old export list

                bool success = db->RunMany(R"(
                    DROP TABLE rec_exports;

                    CREATE TABLE rec_exports (
                        export INTEGER PRIMARY KEY AUTOINCREMENT,
                        ctime INTEGER NOT NULL,
                        userid INTEGER NOT NULL,
                        username TEXT NOT NULL,
                        sequence INTEGER NOT NULL,
                        anchor INTEGER NOT NULL,
                        threads INTEGER NOT NULL
                    );
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 128: {
                bool success = db->RunMany(R"(
                    CREATE TABLE rec_files (
                        tid TEXT NOT NULL REFERENCES rec_threads (tid) DEFERRABLE INITIALLY DEFERRED,
                        eid TEXT NOT NULL REFERENCES rec_entries (eid) DEFERRABLE INITIALLY DEFERRED,
                        anchor INTEGER NOT NULL,
                        name TEXT NOT NULL,
                        sha256 TEXT NOT NULL REFERENCES fs_objects (sha256)
                    );
                    CREATE INDEX rec_files_t ON rec_files (tid);
                    CREATE INDEX rec_files_ea ON rec_files (eid, anchor);
                    CREATE INDEX rec_files_s ON rec_files (sha256);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 129: {
                bool success = db->RunMany(R"(
                    CREATE TABLE mig_meta (
                        eid TEXT,
                        anchor INTEGER,
                        meta TEXT NOT NULL
                    );

                    INSERT INTO mig_meta (eid, meta)
                        SELECT eid, meta FROM rec_entries WHERE meta IS NOT NULL;
                    INSERT INTO mig_meta (eid, anchor, meta)
                        SELECT eid, anchor, meta FROM rec_fragments WHERE meta IS NOT NULL;
                )");
                if (!success)
                    return false;

                // Migrate entries first
                {
                    sq_Statement stmt;
                    if (!db->Prepare(R"(SELECT eid, data, meta
                                        FROM rec_entries
                                        WHERE data IS NOT NULL)", &stmt))
                        return false;

                    while (stmt.Step()) {
                        const char *eid = (const char *)sqlite3_column_text(stmt, 0);
                        Span<const char> data = MakeSpan((const char *)sqlite3_column_text(stmt, 1),
                                                         sqlite3_column_bytes(stmt, 1));
                        Span<const char> meta = MakeSpan((const char *)sqlite3_column_text(stmt, 2),
                                                         sqlite3_column_bytes(stmt, 2));

                        Span<const char> raw = MergeDataMeta(data, meta, &temp_alloc);

                        if (!db->Run("UPDATE rec_entries SET data = ?2, meta = NULL WHERE eid = ?1", eid, raw))
                            return false;
                    }
                }

                // Migrate fragments first
                {
                    sq_Statement stmt;
                    if (!db->Prepare(R"(SELECT anchor, data, meta
                                        FROM rec_fragments
                                        WHERE data IS NOT NULL)", &stmt))
                        return false;

                    while (stmt.Step()) {
                        int64_t anchor = sqlite3_column_int64(stmt, 0);
                        Span<const char> data = MakeSpan((const char *)sqlite3_column_text(stmt, 1),
                                                         sqlite3_column_bytes(stmt, 1));
                        Span<const char> meta = MakeSpan((const char *)sqlite3_column_text(stmt, 2),
                                                         sqlite3_column_bytes(stmt, 2));

                        Span<const char> raw = MergeDataMeta(data, meta, &temp_alloc);

                        if (!db->Run("UPDATE rec_fragments SET data = ?2, meta = NULL WHERE anchor = ?1", anchor, raw))
                            return false;
                    }
                }
            } [[fallthrough]];

            case 130: {
                if (db->TableExists("mig_deletions")) {
                    bool success = db->RunMany(R"(
                        UPDATE rec_entries SET deleted = 1 WHERE tid IN (SELECT tid FROM mig_deletions);
                        DROP TABLE mig_deletions;
                    )");
                    if (!success)
                        return false;
                }
            } // [[fallthrough]];

            static_assert(InstanceVersion == 131);
        }

        if (!db->Run("INSERT INTO adm_migrations (version, build, time) VALUES (?, ?, ?)",
                     target, FelixVersion, time))
            return false;
        if (!db->SetUserVersion(target))
            return false;

        return true;
    });

    return success;
}

bool MigrateInstance(const char *filename, int target)
{
    sq_Database db;

    if (!db.Open(filename, SQLITE_OPEN_READWRITE))
        return false;
    if (!MigrateInstance(&db, target))
        return false;
    if (!db.Close())
        return false;

    return true;
}

}
