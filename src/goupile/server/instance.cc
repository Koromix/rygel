// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "instance.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

const char *const DefaultConfig =
R"([Application]
Key = %1
Name = %2

[Data]
DatabaseFile = database.db

[Sync]
# UseOffline = Off
# SyncMode = Offline
# DemoUser =

[HTTP]
# IPStack = Dual
# Port = 8889
# Threads = 4
# BaseUrl = /
)";

// If you change SchemaVersion, don't forget to update the migration switch!
const int SchemaVersion = 14;

static bool LoadConfig(StreamReader *st, InstanceConfig *out_config)
{
    InstanceConfig config;

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
                        config.app_key = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else if (prop.key == "Name") {
                        config.app_name = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "Data") {
                do {
                    if (prop.key == "LiveDirectory" || prop.key == "FilesDirectory") {
                        config.live_directory = NormalizePath(prop.value, root_directory,
                                                              &config.str_alloc).ptr;
                    } else if (prop.key == "TempDirectory") {
                        config.temp_directory = NormalizePath(prop.value, root_directory,
                                                              &config.str_alloc).ptr;
                    } else if (prop.key == "DatabaseFile") {
                        config.database_filename = NormalizePath(prop.value, root_directory,
                                                                 &config.str_alloc).ptr;
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
                        valid &= OptionToEnum(SyncModeNames, prop.value, &config.sync_mode);
                    } else if (prop.key == "DemoUser") {
                        config.demo_user = DuplicateString(prop.value, &config.str_alloc).ptr;
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
                        config.http.base_url = DuplicateString(prop.value, &config.str_alloc).ptr;
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

    // Default values
    config.app_name = config.app_name ? config.app_name : config.app_key;
    if (config.database_filename && !config.temp_directory) {
        config.temp_directory = Fmt(&config.str_alloc, "%1%/tmp", root_directory).ptr;
    }

    std::swap(*out_config, config);
    return true;
}

static bool LoadConfig(const char *filename, InstanceConfig *out_config)
{
    StreamReader st(filename);
    return LoadConfig(&st, out_config);
}

bool InstanceData::Open(const char *filename)
{
    Close();

    if (!filename) {
        filename = "goupile.ini";

        if (!TestFile(filename, FileType::File)) {
            LogError("Configuration file must be specified");
            return false;
        }
    }

    // Load configuration
    if (!LoadConfig(filename, &config))
        return false;

    // Check configuration
    {
        bool valid = true;

        if (!config.app_key || !config.app_key[0]) {
            LogError("Project key must not be empty");
            valid = false;
        }
        if (!config.database_filename) {
            LogError("Database file not specified");
            valid = false;
        }

        if (!valid)
            return 1;
    }

    // Open database
    if (!db.Open(config.database_filename, SQLITE_OPEN_READWRITE))
        return false;

    // Schema version
    {
        sq_Statement stmt;
        if (!db.Prepare("PRAGMA user_version;", &stmt))
            return 1;

        bool success = stmt.Next();
        RG_ASSERT(success);

        version = sqlite3_column_int(stmt, 0);
    }

    return true;
}

bool InstanceData::Migrate()
{
    RG_ASSERT(version < SchemaVersion);

    BlockAllocator temp_alloc;

    LogInfo("Running migrations %1 to %2", version + 1, SchemaVersion);

    sq_TransactionResult ret = db.Transaction([&]() {
        switch (version) {
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

                if (config.live_directory) {
                    HeapArray<const char *> filenames;
                    if (!EnumerateFiles(config.live_directory, nullptr, -1, -1, &temp_alloc, &filenames))
                        return sq_TransactionResult::Error;

                    Size relative_offset = strlen(config.live_directory);

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
            } // [[fallthrough]];

            RG_STATIC_ASSERT(SchemaVersion == 14);
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

    version = SchemaVersion;
    LogInfo("Migration complete, version: %1", version);

    return true;
}

void InstanceData::Close()
{
    config.str_alloc.ReleaseAll();
    db.Close();
}

}
