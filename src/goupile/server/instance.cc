// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "instance.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

// If you change SchemaVersion, don't forget to update the migration switch!
const int SchemaVersion = 15;

bool InstanceData::Open(const char *directory)
{
    RG_DEFER_N(err_guard) { Close(); };
    Close();

    // Directories
    if (!directory) {
        if (TestFile("database.db", FileType::File)) {
            directory = ".";
        } else {
            LogError("Instance directory must be specified");
            return false;
        }
    }
    root_directory = DuplicateString(directory, &str_alloc).ptr;
    temp_directory = Fmt(&str_alloc, "%1%/tmp", directory).ptr;
    database_filename = Fmt(&str_alloc, "%1%/database.db", directory).ptr;

    // Open database
    if (!db.Open(database_filename, SQLITE_OPEN_READWRITE))
        return false;

    // Schema version
    {
        sq_Statement stmt;
        if (!db.Prepare("PRAGMA user_version;", &stmt))
            return 1;

        bool success = stmt.Next();
        RG_ASSERT(success);

        schema_version = sqlite3_column_int(stmt, 0);
    }

    // Load configuration
    if (schema_version >= 15) {
        if (!LoadDatabaseConfig())
            return false;
    } else if (schema_version) {
        const char *ini_filename = Fmt(&str_alloc, "%1%/goupile.ini", directory).ptr;

        StreamReader st(ini_filename);
        if (!LoadIniConfig(&st))
            return false;
    }

    // Ensure directories exist
    if (!MakeDirectory(temp_directory, false))
        return false;

    err_guard.Disable();
    return true;
}

bool InstanceData::Validate()
{
    RG_ASSERT(schema_version >= 0);

    bool valid = true;

    // Schema version
    if (schema_version > SchemaVersion) {
        LogError("Database schema is too recent (%1, expected %2)", schema_version, SchemaVersion);
        return false;
    } else if (schema_version < SchemaVersion) {
        LogError("Outdated database schema, use %!..+goupile_admin migrate%!0");
        return false;
    }

    // Settings
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
    if (config.max_age < 0) {
        LogError("HTTP MaxAge must be >= 0");
        valid = false;
    }

    return valid;
}

bool InstanceData::Migrate()
{
    BlockAllocator temp_alloc;

    RG_ASSERT(schema_version >= 0);

    if (schema_version > SchemaVersion) {
        LogError("Database schema is too recent (%1, expected %2)", schema_version, SchemaVersion);
        return false;
    } else if (schema_version == SchemaVersion) {
        return true;
    }

    LogInfo("Running migrations %1 to %2", schema_version + 1, SchemaVersion);

    sq_TransactionResult ret = db.Transaction([&]() {
        switch (schema_version) {
            case 0: {
                bool success = db.Run(R"(
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
                bool success = db.Run(R"(
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
                bool success = db.Run(R"(
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
                bool success = db.Run(R"(
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
                if (!db.Run("UPDATE usr_users SET permissions = 31 WHERE permissions == 7;"))
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 5: {
                // Incomplete migration that breaks down (because NOT NULL constraint)
                // if there is any fragment, which is not ever the case yet.
                bool success = db.Run(R"(
                    ALTER TABLE rec_entries ADD COLUMN json TEXT NOT NULL;
                    ALTER TABLE rec_entries ADD COLUMN version INTEGER NOT NULL;
                    ALTER TABLE rec_fragments ADD COLUMN version INEGER NOT NULL;
                )");
                if (!success)
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 6: {
                bool success = db.Run(R"(
                    DROP INDEX rec_columns_spkp;
                    ALTER TABLE rec_columns RENAME COLUMN key TO variable;
                    CREATE UNIQUE INDEX rec_fragments_siv ON rec_fragments(store, id, version);
                    CREATE UNIQUE INDEX rec_columns_spvp ON rec_columns (store, page, variable, IFNULL(prop, 0));
                )");
                if (!success)
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 7: {
                bool success = db.Run(R"(
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
                if (!db.Run("UPDATE usr_users SET permissions = 63 WHERE permissions == 31;"))
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 9: {
                bool success = db.Run(R"(
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
                bool success = db.Run(R"(
                    ALTER TABLE rec_entries ADD COLUMN zone TEXT;
                    ALTER TABLE usr_users ADD COLUMN zone TEXT;

                    CREATE INDEX rec_entries_z ON rec_entries (zone);
                )");
                if (!success)
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 11: {
                bool success = db.Run(R"(
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
                bool success = db.Run(R"(
                    ALTER TABLE adm_events RENAME COLUMN details TO username;
                    ALTER TABLE adm_events ADD COLUMN zone TEXT;
                    ALTER TABLE adm_events ADD COLUMN details TEXT;
                )");
                if (!success)
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 13: {
                bool success = db.Run(R"(
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

                if (schema_version) {
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

                        if (!db.Run(R"(INSERT INTO fs_files (path, blob, compression, sha256)
                                       VALUES (?, ?, ?, ?);)", path, gzip, "Gzip", sha256))
                            return sq_TransactionResult::Error;
                    }
                }
            } [[fallthrough]];

            case 14: {
                bool success = db.Run(R"(
                    CREATE TABLE fs_settings (
                        key TEXT NOT NULL,
                        value TEXT
                    );

                    CREATE UNIQUE INDEX fs_settings_k ON fs_settings (key);
                )");

                const char *sql = "INSERT INTO fs_settings (key, value) VALUES (?, ?)";
                success &= db.Run(sql, "Application.Name", config.app_name);
                success &= db.Run(sql, "Application.ClientKey", config.app_key);
                success &= db.Run(sql, "Application.UseOffline", 0 + config.use_offline);
                success &= db.Run(sql, "Application.MaxFileSize", config.max_file_size);
                success &= db.Run(sql, "Application.SyncMode", SyncModeNames[(int)config.sync_mode]);
                success &= db.Run(sql, "Application.DemoUser", config.demo_user);
                success &= db.Run(sql, "HTTP.IPStack", IPStackNames[(int)config.http.ip_stack]);
                success &= db.Run(sql, "HTTP.Port", config.http.port);
                success &= db.Run(sql, "HTTP.MaxConnections", config.http.max_connections);
                success &= db.Run(sql, "HTTP.IdleTimeout", config.http.idle_timeout);
                success &= db.Run(sql, "HTTP.Threads", config.http.threads);
                success &= db.Run(sql, "HTTP.AsyncThreads", config.http.async_threads);
                success &= db.Run(sql, "HTTP.BaseUrl", config.http.base_url);
                success &= db.Run(sql, "HTTP.MaxAge", config.max_age);

                if (schema_version) {
                    LogInfo("You should remove goupile.ini manually!");
                }

                if (!success)
                    return sq_TransactionResult::Error;
            } // [[fallthrough]];

            RG_STATIC_ASSERT(SchemaVersion == 15);
        }

        int64_t time = GetUnixTime();
        if (!db.Run("INSERT INTO adm_migrations (version, build, time) VALUES (?, ?, ?)",
                              SchemaVersion, FelixVersion, time))
            return sq_TransactionResult::Error;

        char buf[128];
        Fmt(buf, "PRAGMA user_version = %1;", SchemaVersion);
        if (!db.Run(buf))
            return sq_TransactionResult::Error;

        return sq_TransactionResult::Commit;
    });
    if (ret != sq_TransactionResult::Commit)
        return false;

    schema_version = SchemaVersion;
    LogInfo("Migration complete, version: %1", schema_version);

    return true;
}

void InstanceData::Close()
{
    db.Close();
    schema_version = -1;
    config = {};
    str_alloc.ReleaseAll();
}

bool InstanceData::LoadDatabaseConfig()
{
    sq_Statement stmt;
    if (!db.Prepare("SELECT key, value FROM fs_settings;", &stmt))
        return false;

    bool valid = true;

    while (stmt.Next()) {
        const char *key = (const char *)sqlite3_column_text(stmt, 0);
        const char *value = (const char *)sqlite3_column_text(stmt, 1);

        if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
            if (TestStr(key, "Application.Name")) {
                config.app_name = DuplicateString(value, &str_alloc).ptr;
            } else if (TestStr(key, "Application.ClientKey")) {
                config.app_key = DuplicateString(value, &str_alloc).ptr;
            } else if (TestStr(key, "Application.UseOffline")) {
                valid &= IniParser::ParseBoolValue(value, &config.use_offline);
            } else if (TestStr(key, "Application.MaxFileSize")) {
                valid &= ParseDec(value, &config.max_file_size);
            } else if (TestStr(key, "Application.SyncMode")) {
                if (!OptionToEnum(SyncModeNames, value, &config.sync_mode)) {
                    LogError("Unknown sync mode '%1'", value);
                    valid = false;
                }
            } else if (TestStr(key, "Application.DemoUser")) {
                config.demo_user = DuplicateString(value, &str_alloc).ptr;
            } else if (TestStr(key, "HTTP.IPStack")) {
                if (!OptionToEnum(IPStackNames, value, &config.http.ip_stack)) {
                    LogError("Unknown IP stack '%1'", value);
                    valid = false;
                }
            } else if (TestStr(key, "HTTP.Port")) {
                valid &= ParseDec(value, &config.http.port);
            } else if (TestStr(key, "HTTP.MaxConnections")) {
                valid &= ParseDec(value, &config.http.max_connections);
            } else if (TestStr(key, "HTTP.IdleTimeout")) {
                valid &= ParseDec(value, &config.http.idle_timeout);
            } else if (TestStr(key, "HTTP.Threads")) {
                valid &= ParseDec(value, &config.http.threads);
            } else if (TestStr(key, "HTTP.AsyncThreads")) {
                valid &= ParseDec(value, &config.http.async_threads);
            } else if (TestStr(key, "HTTP.BaseUrl")) {
                config.http.base_url = DuplicateString(value, &str_alloc).ptr;
            } else if (TestStr(key, "HTTP.MaxAge")) {
                valid &= ParseDec(value, &config.max_age);
            } else {
                LogError("Unknown setting '%1'", key);
                valid = false;
            }
        }
    }
    if (!stmt.IsValid() || !valid)
        return false;

    return true;
}

bool InstanceData::LoadIniConfig(StreamReader *st)
{
    Span<const char> root_directory;
    SplitStrReverseAny(st->GetFileName(), RG_PATH_SEPARATORS, &root_directory);

    IniParser ini(st);
    ini.PushLogFilter();
    RG_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section == "Application") {
                do {
                    if (prop.key == "Key") {
                        config.app_key = DuplicateString(prop.value, &str_alloc).ptr;
                    } else if (prop.key == "Name") {
                        config.app_name = DuplicateString(prop.value, &str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
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
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "Sync") {
                do {
                    if (prop.key == "UseOffline") {
                        valid &= IniParser::ParseBoolValue(prop.value, &config.use_offline);
                    } else if (prop.key == "MaxFileSize") {
                        valid &= ParseDec(prop.value, &config.max_file_size);
                    } else if (prop.key == "SyncMode") {
                        if (!OptionToEnum(SyncModeNames, prop.value, &config.sync_mode)) {
                            LogError("Unknown sync mode '%1'", prop.value);
                            valid = false;
                        }
                    } else if (prop.key == "DemoUser") {
                        config.demo_user = DuplicateString(prop.value, &str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "HTTP") {
                do {
                    if (prop.key == "IPStack") {
                        if (prop.value == "Dual") {
                            config.http.ip_stack = IPStack::Dual;
                        } else if (prop.value == "IPv4") {
                            config.http.ip_stack = IPStack::IPv4;
                        } else if (prop.value == "IPv6") {
                            config.http.ip_stack = IPStack::IPv6;
                        } else {
                            LogError("Unknown IP version '%1'", prop.value);
                        }
                    } else if (prop.key == "Port") {
                        valid &= ParseDec(prop.value, &config.http.port);
                    } else if (prop.key == "MaxConnections") {
                        valid &= ParseDec(prop.value, &config.http.max_connections);
                    } else if (prop.key == "IdleTimeout") {
                        valid &= ParseDec(prop.value, &config.http.idle_timeout);
                    } else if (prop.key == "Threads") {
                        valid &= ParseDec(prop.value, &config.http.threads);
                    } else if (prop.key == "AsyncThreads") {
                        valid &= ParseDec(prop.value, &config.http.async_threads);
                    } else if (prop.key == "BaseUrl") {
                        config.http.base_url = DuplicateString(prop.value, &str_alloc).ptr;
                    } else if (prop.key == "MaxAge") {
                        valid &= ParseDec(prop.value, &config.max_age);
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else {
                LogError("Unknown section '%1'", prop.section);
                while (ini.NextInSection(&prop));
                valid = false;
            }
        }
    }
    if (!ini.IsValid() || !valid)
        return false;

    return true;
}

}
