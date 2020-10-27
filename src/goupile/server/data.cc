// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "data.hh"
#include "../../core/libwrap/sqlite.hh"

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

// If you change DatabaseVersion, don't forget to update the migration switch!
const int DatabaseVersion = 14;

bool MigrateDatabase(sq_Database &database, int version)
{
    RG_ASSERT(version < DatabaseVersion);

    LogInfo("Running migrations %1 to %2", version + 1, DatabaseVersion);

    sq_TransactionResult ret = database.Transaction([&]() {
        switch (version) {
            case 0: {
                bool success = database.Run(R"(
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
                bool success = database.Run(R"(
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
                bool success = database.Run(R"(
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
                bool success = database.Run(R"(
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
                if (!database.Run("UPDATE usr_users SET permissions = 31 WHERE permissions == 7;"))
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 5: {
                // Incomplete migration that breaks down (because NOT NULL constraint)
                // if there is any fragment, which is not ever the case yet.
                bool success = database.Run(R"(
                    ALTER TABLE rec_entries ADD COLUMN json TEXT NOT NULL;
                    ALTER TABLE rec_entries ADD COLUMN version INTEGER NOT NULL;
                    ALTER TABLE rec_fragments ADD COLUMN version INEGER NOT NULL;
                )");
                if (!success)
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 6: {
                bool success = database.Run(R"(
                    DROP INDEX rec_columns_spkp;
                    ALTER TABLE rec_columns RENAME COLUMN key TO variable;
                    CREATE UNIQUE INDEX rec_fragments_siv ON rec_fragments(store, id, version);
                    CREATE UNIQUE INDEX rec_columns_spvp ON rec_columns (store, page, variable, IFNULL(prop, 0));
                )");
                if (!success)
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 7: {
                bool success = database.Run(R"(
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
                if (!database.Run("UPDATE usr_users SET permissions = 63 WHERE permissions == 31;"))
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 9: {
                bool success = database.Run(R"(
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
                bool success = database.Run(R"(
                    ALTER TABLE rec_entries ADD COLUMN zone TEXT;
                    ALTER TABLE usr_users ADD COLUMN zone TEXT;

                    CREATE INDEX rec_entries_z ON rec_entries (zone);
                )");
                if (!success)
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 11: {
                bool success = database.Run(R"(
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
                bool success = database.Run(R"(
                    ALTER TABLE adm_events RENAME COLUMN details TO username;
                    ALTER TABLE adm_events ADD COLUMN zone TEXT;
                    ALTER TABLE adm_events ADD COLUMN details TEXT;
                )");
                if (!success)
                    return sq_TransactionResult::Error;
            } [[fallthrough]];

            case 13: {
                bool success = database.Run(R"(
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
            } // [[fallthrough]];

            RG_STATIC_ASSERT(DatabaseVersion == 14);
        }

        int64_t time = GetUnixTime();
        if (!database.Run("INSERT INTO adm_migrations (version, build, time) VALUES (?, ?, ?)",
                          DatabaseVersion, FelixVersion, time))
            return sq_TransactionResult::Error;

        char buf[128];
        Fmt(buf, "PRAGMA user_version = %1;", DatabaseVersion);
        if (!database.Run(buf))
            return sq_TransactionResult::Error;

        return sq_TransactionResult::Commit;
    });
    if (ret != sq_TransactionResult::Commit)
        return false;

    LogInfo("Migration complete, version: %1", DatabaseVersion);
    return true;
}

}
