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
FilesDirectory = files
DatabaseFile = database.db

[Sync]
# UseOffline = Off
# AllowGuests = Off

[HTTP]
# IPStack = Dual
# Port = 8889
# Threads = 4
# BaseUrl = /
)";
const int DatabaseVersion = 5;

bool MigrateDatabase(sq_Database &database, int version)
{
    bool success = database.Transaction([&]() {
        switch (version) {
            case 0: {
                LogInfo("Running migration 1 of %1", DatabaseVersion);

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
                    return false;
            } [[fallthrough]];

            case 1: {
                LogInfo("Running migration 2 of %1", DatabaseVersion);

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
                    CREATE INDEX rec_fragments_tip ON rec_fragments(table_name, id, page);

                    INSERT INTO rec_fragments (table_name, id, page, username, mtime, complete, json)
                        SELECT table_name, id, page, username, mtime, complete, json FROM rec_fragments_BAK;
                    DROP TABLE rec_fragments_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 2: {
                LogInfo("Running migration 3 of %1", DatabaseVersion);

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
                    return false;
            } [[fallthrough]];

            case 3: {
                LogInfo("Running migration 4 of %1", DatabaseVersion);

                bool success = database.Run(R"(
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
                LogInfo("Running migration 5 of %1", DatabaseVersion);

                if (!database.Run("UPDATE usr_users SET permissions = 31 WHERE permissions == 7;"))
                    return false;
            } // [[fallthrough]];
        }

        int64_t time = GetUnixTime();
        if (!database.Run("INSERT INTO adm_migrations (version, build, time) VALUES (?, ?, ?)",
                          DatabaseVersion, FelixVersion, time))
            return false;

        char buf[128];
        Fmt(buf, "PRAGMA user_version = %1;", DatabaseVersion);
        if (!database.Run(buf))
            return false;

        LogInfo("Migration complete, version: %1", DatabaseVersion);
        return true;
    });

    // If you change DatabaseVersion, don't forget to update the migration switch!
    RG_STATIC_ASSERT(DatabaseVersion == 5);

    return success;
}

}
