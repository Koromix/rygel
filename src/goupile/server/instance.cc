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
static const int InstanceVersion = 19;

bool InstanceData::Open(const char *filename)
{
    RG_DEFER_N(err_guard) { Close(); };
    Close();

    // Instance key
    {
        Span<const char> key = SplitStrReverseAny(filename, RG_PATH_SEPARATORS);
        key = SplitStr(key, '.');

        this->key = DuplicateString(key, &str_alloc).ptr;
        this->filename = DuplicateString(filename, &str_alloc).ptr;
    }

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
        if (!db.Prepare("SELECT key, value FROM fs_settings;", &stmt))
            return false;

        bool valid = true;

        while (stmt.Next()) {
            const char *key = (const char *)sqlite3_column_text(stmt, 0);
            const char *value = (const char *)sqlite3_column_text(stmt, 1);

            if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
                if (TestStr(key, "BaseUrl")) {
                    config.base_url = DuplicateString(value, &str_alloc).ptr;
                } else if (TestStr(key, "AppName")) {
                    config.app_name = DuplicateString(value, &str_alloc).ptr;
                } else if (TestStr(key, "AppKey")) {
                    config.app_key = DuplicateString(value, &str_alloc).ptr;
                } else if (TestStr(key, "UseOffline")) {
                    valid &= ParseBool(value, &config.use_offline);
                } else if (TestStr(key, "MaxFileSize")) {
                    valid &= ParseInt(value, &config.max_file_size);
                } else if (TestStr(key, "SyncMode")) {
                    if (!OptionToEnum(SyncModeNames, value, &config.sync_mode)) {
                        LogError("Unknown sync mode '%1'", value);
                        valid = false;
                    }
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

    InitAssets();

    err_guard.Disable();
    return true;
}

bool InstanceData::Validate()
{
    bool valid = true;

    // Settings
    if (!config.base_url || config.base_url[0] != '/' ||
                            config.base_url[strlen(config.base_url) - 1] != '/') {
        LogError("Base URL '%1' does not start and end with '/'", config.base_url);
        valid = false;
    }
    if (!config.app_key || !config.app_key[0]) {
        LogError("Project key must not be empty");
        valid = false;
    }
    if (!config.app_name) {
        config.app_name = config.app_key;
    }
    if (config.max_file_size <= 0) {
        LogError("Maximum file size must be >= 0");
        valid = false;
    }

    return valid;
}

void InstanceData::InitAssets()
{
    assets.Clear();
    assets_map.Clear();
    assets_alloc.ReleaseAll();

    Span<const AssetInfo> packed_assets = GetPackedAssets();

    // Packed static assets
    for (Size i = 0; i < packed_assets.len; i++) {
        AssetInfo asset = packed_assets[i];

        if (TestStr(asset.name, "goupile.html")) {
            asset.name = "/static/goupile.html";
            asset.data = PatchVariables(asset);
        } else if (TestStr(asset.name, "manifest.json")) {
            if (!config.use_offline)
                continue;

            asset.name = "/manifest.json";
            asset.data = PatchVariables(asset);
        } else if (TestStr(asset.name, "sw.pk.js")) {
            asset.name = "/sw.pk.js";
            asset.data = PatchVariables(asset);
        } else if (TestStr(asset.name, "favicon.png")) {
            asset.name = "/favicon.png";
        } else if (TestStr(asset.name, "server.pk.js")) {
            continue;
        } else {
            asset.name = Fmt(&assets_alloc, "/static/%1", asset.name).ptr;
        }

        assets.Append(asset);
    }
    for (const AssetInfo &asset: assets) {
        assets_map.Set(&asset);
    }

    // We can use a global ETag because everything is in the binary
    {
        uint64_t buf[2];
        randombytes_buf(&buf, RG_SIZE(buf));
        Fmt(etag, "%1%2", FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16));
    }
}

void InstanceData::Close()
{
    key = nullptr;
    filename = nullptr;
    db.Close();
    config = {};
    assets.Clear();
    assets_map.Clear();
    str_alloc.ReleaseAll();
    assets_alloc.ReleaseAll();
}

Span<const uint8_t> InstanceData::PatchVariables(const AssetInfo &asset)
{
    Span<const uint8_t> data = PatchAsset(asset, &assets_alloc,
                                          [&](const char *key, StreamWriter *writer) {
        if (TestStr(key, "VERSION")) {
            writer->Write(FelixVersion);
            return true;
        } else if (TestStr(key, "APP_KEY")) {
            writer->Write(config.app_key);
            return true;
        } else if (TestStr(key, "APP_NAME")) {
            writer->Write(config.app_name);
            return true;
        } else if (TestStr(key, "BASE_URL")) {
            writer->Write(config.base_url);
            return true;
        } else if (TestStr(key, "USE_OFFLINE")) {
            writer->Write(config.use_offline ? "true" : "false");
            return true;
        } else if (TestStr(key, "SYNC_MODE")) {
            char js_name[64];
            ConvertToJsonName(SyncModeNames[(int)config.sync_mode], js_name);

            writer->Write(js_name);
            return true;
        } else if (TestStr(key, "CACHE_KEY")) {
            writer->Write(etag);
            return true;
        } else if (TestStr(key, "LINK_MANIFEST")) {
            if (config.use_offline) {
                Print(writer, "<link rel=\"manifest\" href=\"%1manifest.json\"/>", config.base_url);
            }
            return true;
        } else {
            return false;
        }
    });

    return data;
}

bool MigrateInstance(sq_Database *db)
{
    BlockAllocator temp_alloc;

    // Database filename
    const char *filename;
    {
        sq_Statement stmt;
        if (!db->Prepare("PRAGMA database_list;", &stmt))
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

    sq_TransactionResult ret = db->Transaction([&]() {
        switch (version) {
            case 0: {
                bool success = db->Run(R"(
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
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 1: {
                bool success = db->Run(R"(
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
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 2: {
                bool success = db->Run(R"(
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
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 3: {
                bool success = db->Run(R"(
                    CREATE TABLE adm_migrations (
                        version INTEGER NOT NULL,
                        build TEXT NOT NULL,
                        time INTEGER NOT NULL
                    );
                )");
                if (!success)
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 4: {
                if (!db->Run("UPDATE usr_users SET permissions = 31 WHERE permissions == 7;"))
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 5: {
                // Incomplete migration that breaks down (because NOT NULL constraint)
                // if there is any fragment, which is not ever the case yet.
                bool success = db->Run(R"(
                    ALTER TABLE rec_entries ADD COLUMN json TEXT NOT NULL;
                    ALTER TABLE rec_entries ADD COLUMN version INTEGER NOT NULL;
                    ALTER TABLE rec_fragments ADD COLUMN version INEGER NOT NULL;
                )");
                if (!success)
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 6: {
                bool success = db->Run(R"(
                    DROP INDEX rec_columns_spkp;
                    ALTER TABLE rec_columns RENAME COLUMN key TO variable;
                    CREATE UNIQUE INDEX rec_fragments_siv ON rec_fragments(store, id, version);
                    CREATE UNIQUE INDEX rec_columns_spvp ON rec_columns (store, page, variable, IFNULL(prop, 0));
                )");
                if (!success)
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 7: {
                bool success = db->Run(R"(
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
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 8: {
                if (!db->Run("UPDATE usr_users SET permissions = 63 WHERE permissions == 31;"))
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 9: {
                bool success = db->Run(R"(
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
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 10: {
                bool success = db->Run(R"(
                    ALTER TABLE rec_entries ADD COLUMN zone TEXT;
                    ALTER TABLE usr_users ADD COLUMN zone TEXT;

                    CREATE INDEX rec_entries_z ON rec_entries (zone);
                )");
                if (!success)
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 11: {
                bool success = db->Run(R"(
                    CREATE TABLE adm_events (
                        time INTEGER NOT NULL,
                        address TEXT,
                        type TEXT NOT NULL,
                        details TEXT NOT NULL
                    );
                )");
                if (!success)
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 12: {
                bool success = db->Run(R"(
                    ALTER TABLE adm_events RENAME COLUMN details TO username;
                    ALTER TABLE adm_events ADD COLUMN zone TEXT;
                    ALTER TABLE adm_events ADD COLUMN details TEXT;
                )");
                if (!success)
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 13: {
                bool success = db->Run(R"(
                    CREATE TABLE fs_files (
                        path TEXT NOT NULL,
                        blob BLOB,
                        compression TEXT,
                        sha256 TEXT
                    );

                    CREATE INDEX fs_files_p ON fs_files (path);
                )");
                if (!success)
                    return sq_TransactionResult::Error;

                if (version && filename) {
                    Span<const char> root_directory = GetPathDirectory(filename);
                    const char *files_directory = Fmt(&temp_alloc, "%1%/files", root_directory).ptr;

                    HeapArray<const char *> filenames;
                    if (!EnumerateFiles(files_directory, nullptr, -1, -1, &temp_alloc, &filenames))
                        return sq_TransactionResult::Error;

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
                                    return sq_TransactionResult::Error;

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
                                        VALUES (?, ?, ?, ?);)", path, gzip, "Gzip", sha256))
                            return sq_TransactionResult::Error;
                    }
                }
            } [[fallthrough]];

            case 14: {
                bool success = db->Run(R"(
                    CREATE TABLE fs_settings (
                        key TEXT NOT NULL,
                        value TEXT
                    );

                    CREATE UNIQUE INDEX fs_settings_k ON fs_settings (key);
                )");

                // Default settings
                {
                    decltype(DomainData::config) fake1;
                    decltype(InstanceData::config) fake2;

                    const char *sql = "INSERT INTO fs_settings (key, value) VALUES (?, ?)";
                    success &= db->Run(sql, "Application.Name", fake2.app_name);
                    success &= db->Run(sql, "Application.ClientKey", fake2.app_key);
                    success &= db->Run(sql, "Application.UseOffline", 0 + fake2.use_offline);
                    success &= db->Run(sql, "Application.MaxFileSize", fake2.max_file_size);
                    success &= db->Run(sql, "Application.SyncMode", SyncModeNames[(int)fake2.sync_mode]);
                    success &= db->Run(sql, "Application.DemoUser", fake1.demo_user);
                    success &= db->Run(sql, "HTTP.IPStack", IPStackNames[(int)fake1.http.ip_stack]);
                    success &= db->Run(sql, "HTTP.Port", fake1.http.port);
                    success &= db->Run(sql, "HTTP.MaxConnections", fake1.http.max_connections);
                    success &= db->Run(sql, "HTTP.IdleTimeout", fake1.http.idle_timeout);
                    success &= db->Run(sql, "HTTP.Threads", fake1.http.threads);
                    success &= db->Run(sql, "HTTP.AsyncThreads", fake1.http.async_threads);
                    success &= db->Run(sql, "HTTP.BaseUrl", fake2.base_url);
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
                                    // XXX: Empty strings?
                                    success &= db->Run(sql, "Application.SyncMode", prop.value);
                                } else {
                                    LogError("Unknown attribute '%1'", prop.key);
                                    success = false;
                                }
                            } while (ini.NextInSection(&prop));
                        } else if (prop.section == "HTTP") {
                            do {
                                if (prop.key == "IPStack") {
                                    success &= db->Run(sql, "HTTP.IPStack", prop.value);
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
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 15: {
                bool success = db->Run(R"(
                    DROP INDEX sched_resources_sdt;
                    DROP INDEX sched_meetings_sd;
                    DROP TABLE sched_meetings;
                    DROP TABLE sched_resources;
                )");
                if (!success)
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 16: {
                bool success = db->Run(R"(
                    ALTER TABLE fs_files ADD COLUMN size INTEGER;
                )");
                if (!success)
                    return sq_TransactionResult::Error;

                sq_Statement stmt;
                if (!db->Prepare(R"(SELECT rowid, path, compression FROM fs_files
                                   WHERE sha256 IS NOT NULL;)", &stmt))
                    return sq_TransactionResult::Error;

                while (stmt.Next()) {
                    int64_t rowid = sqlite3_column_int64(stmt, 0);
                    const char *path = (const char *)sqlite3_column_text(stmt, 1);

                    CompressionType compression_type;
                    {
                        const char *name = (const char *)sqlite3_column_text(stmt, 2);
                        if (!name || !OptionToEnum(CompressionTypeNames, name, &compression_type)) {
                            LogError("Unknown compression type '%1'", name);
                            return sq_TransactionResult::Rollback;
                        }
                    }

                    sqlite3_blob *blob;
                    if (sqlite3_blob_open(*db, "main", "fs_files", "blob", rowid, 0, &blob) != SQLITE_OK) {
                        LogError("SQLite Error: %1", sqlite3_errmsg(*db));
                        return sq_TransactionResult::Error;
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
                                return sq_TransactionResult::Error;

                            real_len += buf.len;
                        }
                    }

                    if (!db->Run("UPDATE fs_files SET size = ? WHERE path = ?;", real_len, path))
                        return sq_TransactionResult::Error;
                }
                if (!stmt.IsValid())
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 17: {
                bool success = db->Run(R"(
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
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 18: {
                bool success = db->Run(R"(
                    DROP TABLE dom_permissions;
                )");

                if (version) {
                    LogInfo("Existing instance permissions must be recreated on main database");
                }

                if (!success)
                    return sq_TransactionResult::Error;
            } // [[fallthrough]];

            RG_STATIC_ASSERT(InstanceVersion == 19);
        }

        int64_t time = GetUnixTime();
        if (!db->Run("INSERT INTO adm_migrations (version, build, time) VALUES (?, ?, ?)",
                     InstanceVersion, FelixVersion, time))
            return sq_TransactionResult::Error;
        if (!db->SetUserVersion(InstanceVersion))
            return sq_TransactionResult::Error;

        return sq_TransactionResult::Commit;
    });
    if (ret != sq_TransactionResult::Commit)
        return false;

    return true;
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
