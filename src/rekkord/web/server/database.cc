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
#include "config.hh"
#include "database.hh"

namespace RG {

const int DatabaseVersion = 5;

bool MigrateDatabase(sq_Database *db)
{
    BlockAllocator temp_alloc;

    int version;
    if (!db->GetUserVersion(&version))
        return false;

    if (version > DatabaseVersion) {
        LogError("Database schema is too recent (%1, expected %2)", version, DatabaseVersion);
        return false;
    } else if (version == DatabaseVersion) {
        return true;
    }

    LogInfo("Migrate database: %1 to %2", version, DatabaseVersion);

    bool success = db->Transaction([&]() {
        int64_t time = GetUnixTime();

        switch (version) {
            case 0: {
                bool success = db->RunMany(R"(
                    CREATE TABLE migrations (
                        version INTEGER NOT NULL,
                        build TEXT NOT NULL,
                        timestamp INTEGER NOT NULL
                    );

                    CREATE TABLE users (
                        id INTEGER PRIMARY KEY NOT NULL,
                        mail TEXT COLLATE NOCASE NOT NULL,
                        password_hash TEXT,
                        username TEXT NOT NULL,
                        creation INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX users_m ON users (mail);

                    CREATE TABLE tokens (
                        token TEXT NOT NULL,
                        timestamp INTEGER NOT NULL,
                        user INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE
                    );
                    CREATE UNIQUE INDEX tokens_t ON tokens (token);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 1: {
                bool success = db->RunMany(R"(
                    DROP INDEX tokens_t;
                    DROP INDEX users_m;

                    ALTER TABLE tokens RENAME TO tokens_BAK;
                    ALTER TABLE users RENAME TO users_BAK;

                    CREATE TABLE users (
                        id INTEGER PRIMARY KEY NOT NULL,
                        mail TEXT COLLATE NOCASE NOT NULL,
                        password_hash TEXT,
                        username TEXT NOT NULL,
                        creation INTEGER NOT NULL,
                        picture BLOB,
                        version INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX users_m ON users (mail);

                    CREATE TABLE tokens (
                        token TEXT NOT NULL,
                        timestamp INTEGER NOT NULL,
                        user INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE
                    );
                    CREATE UNIQUE INDEX tokens_t ON tokens (token);

                    INSERT INTO users (id, mail, password_hash, username, creation, version)
                        SELECT id, mail, password_hash, username, creation, 1 FROM users_BAK;
                    INSERT INTO tokens (token, timestamp, user)
                        SELECT token, timestamp, user FROM tokens;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 2: {
                bool success = db->RunMany(R"(
                    CREATE TABLE repositories (
                        id INTEGER PRIMARY KEY NOT NULL,
                        owner INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
                        name TEXT NOT NULL,
                        url TEXT NOT NULL,
                        user TEXT NOT NULL,
                        password TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX repositories_un ON repositories (user, name);

                    CREATE TABLE variables (
                        repository INTEGER REFERENCES repositories (id) ON DELETE CASCADE,
                        key TEXT NOT NULL,
                        value TEXT NOT NULL
                    );
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 3: {
                bool success = db->RunMany(R"(
                    DROP INDEX repositories_un;

                    ALTER TABLE repositories RENAME TO repositories_BAK;
                    ALTER TABLE variables RENAME TO variables_BAK;

                    CREATE TABLE repositories (
                        id INTEGER PRIMARY KEY NOT NULL,
                        owner INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
                        name TEXT NOT NULL,
                        url TEXT NOT NULL,
                        user TEXT NOT NULL,
                        password TEXT NOT NULL,
                        checked INTEGER NOT NULL,
                        failed TEXT,
                        errors INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX repositories_un ON repositories (user, name);

                    CREATE TABLE variables (
                        id INTEGER PRIMARY KEY NOT NULL,
                        repository INTEGER REFERENCES repositories (id) ON DELETE CASCADE,
                        key TEXT NOT NULL,
                        value TEXT NOT NULL
                    );

                    CREATE TABLE channels (
                        repository INTEGER REFERENCES repositories (id) ON DELETE CASCADE,
                        name TEXT NOT NULL,
                        hash TEXT NOT NULL,
                        timestamp INTEGER NOT NULL,
                        size INTEGER NOT NULL,
                        count INTEGER NOT NULL,
                        ignore CHECK (ignore IN (0, 1)) NOT NULL
                    );
                    CREATE UNIQUE INDEX channels_rn ON channels (repository, name);

                    INSERT INTO repositories (id, owner, name, url, user, password, checked, failed, errors)
                        SELECT id, owner, name, url, user, password, 0, NULL, 0 FROM repositories_BAK;
                    INSERT INTO variables (repository, key, value)
                        SELECT repository, key, value FROM variables_BAK;

                    DROP TABLE variables_BAK;
                    DROP TABLE repositories_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 4: {
                bool success = db->RunMany(R"(
                    CREATE TABLE snapshots (
                        repository INTEGER REFERENCES repositories (id) ON DELETE CASCADE,
                        hash TEXT NOT NULL,
                        channel TEXT NOT NULL,
                        timestamp INTEGER NOT NULL,
                        size INTEGER NOT NULL,
                        storage INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX snapshots_rh ON snapshots (repository, hash);
                    CREATE INDEX snapshots_c ON snapshots (channel);
                )");
                if (!success)
                    return false;
            } // [[fallthrough]];

            static_assert(DatabaseVersion == 5);
        }

        if (!db->Run("INSERT INTO migrations (version, build, timestamp) VALUES (?, ?, ?)",
                     DatabaseVersion, FelixVersion, time))
            return false;
        if (!db->SetUserVersion(DatabaseVersion))
            return false;

        return true;
    });

    return success;
}

bool MigrateDatabase(const Config &config)
{
    sq_Database db;

    if (!db.Open(config.database_filename, SQLITE_OPEN_READWRITE))
        return false;
    if (!MigrateDatabase(&db))
        return false;
    if (!db.Close())
        return false;

    return true;
}

}
