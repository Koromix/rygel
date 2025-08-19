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

const int DatabaseVersion = 26;

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
            } [[fallthrough]];

            case 5: {
                bool success = db->RunMany(R"(
                    CREATE TABLE failures (
                        repository INTEGER REFERENCES repositories (id) ON DELETE CASCADE,
                        timestamp INTEGER NOT NULL,
                        message TEXT NOT NULL,
                        sent INTEGER,
                        resolved CHECK (resolved IN (0, 1)) NOT NULL
                    );
                    CREATE UNIQUE INDEX failures_r ON failures (repository);

                    CREATE TABLE stales (
                        repository INTEGER REFERENCES repositories (id) ON DELETE CASCADE,
                        channel TEXT NOT NULL,
                        timestamp INTEGER NOT NULL,
                        sent INTEGER,
                        resolved CHECK (resolved IN (0, 1)) NOT NULL
                    );
                    CREATE UNIQUE INDEX stales_rc ON stales (repository, channel);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 6: {
                bool success = db->RunMany(R"(
                    CREATE TABLE mails (
                        id INTEGER PRIMARY KEY NOT NULL,
                        address TEXT NOT NULL,
                        mail TEXT NOT NULL,
                        sent INTEGER NOT NULL,
                        errors INTEGER NOT NULL
                    );
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 7: {
                bool success = db->RunMany(R"(
                    DROP INDEX failures_r;
                    DROP INDEX stales_rc;

                    DROP TABLE failures;
                    DROP TABLE stales;

                    CREATE TABLE failures (
                        id INTEGER PRIMARY KEY NOT NULL,
                        repository INTEGER REFERENCES repositories (id) ON DELETE CASCADE,
                        timestamp INTEGER NOT NULL,
                        message TEXT NOT NULL,
                        sent INTEGER,
                        resolved CHECK (resolved IN (0, 1)) NOT NULL
                    );
                    CREATE UNIQUE INDEX failures_r ON failures (repository);

                    CREATE TABLE stales (
                        id INTEGER PRIMARY KEY NOT NULL,
                        repository INTEGER REFERENCES repositories (id) ON DELETE CASCADE,
                        channel TEXT NOT NULL,
                        timestamp INTEGER NOT NULL,
                        sent INTEGER,
                        resolved CHECK (resolved IN (0, 1)) NOT NULL
                    );
                    CREATE UNIQUE INDEX stales_rc ON stales (repository, channel);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 8: {
                bool success = db->RunMany(R"(
                    ALTER TABLE channels RENAME COLUMN hash TO oid;
                    ALTER TABLE snapshots RENAME COLUMN hash TO oid;

                    UPDATE channels SET oid = 'm' || oid;
                    UPDATE snapshots SET oid = 'm' || oid;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 9: {
                bool success = db->RunMany(R"(
                    UPDATE channels SET oid = UPPER(oid);
                    UPDATE snapshots SET oid = UPPER(oid);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 10: {
                bool success = db->RunMany(R"(
                    DROP TABLE IF EXISTS users_BAK;
                    DROP TABLE IF EXISTS tokens_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 11: {
                bool success = db->RunMany(R"(
                    UPDATE variables SET key = 'S3_ACCESS_KEY_ID' WHERE key = 'AWS_ACCESS_KEY_ID';
                    UPDATE variables SET key = 'S3_SECRET_ACCESS_KEY' WHERE key = 'AWS_SECRET_ACCESS_KEY';
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 12: {
                bool success = db->RunMany(R"(
                    DROP INDEX snapshots_c;
                    DROP INDEX IF EXISTS snapshots_ro;
                    DROP INDEX IF EXISTS snapshots_rh;

                    ALTER TABLE snapshots RENAME TO snapshots_BAK;

                    CREATE TABLE snapshots (
                        repository INTEGER REFERENCES repositories (id) ON DELETE CASCADE,
                        oid TEXT NOT NULL,
                        channel TEXT NOT NULL,
                        timestamp INTEGER NOT NULL,
                        size INTEGER NOT NULL,
                        stored INTEGER NOT NULL,
                        added INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX snapshots_ro ON snapshots (repository, oid);
                    CREATE INDEX snapshots_c ON snapshots (channel);

                    INSERT INTO snapshots (repository, oid, channel, timestamp, size, stored, added)
                        SELECT repository, oid, channel, timestamp, size, storage, 0 FROM snapshots_BAK;

                    DROP TABLE snapshots_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 13: {
                bool success = db->RunMany(R"(
                    CREATE TABLE keys (
                        id INTEGER PRIMARY KEY NOT NULL,
                        repository INTEGER REFERENCES repositories (id) ON DELETE CASCADE,
                        title TEXT NOT NULL,
                        key TEXT NOT NULL,
                        hash TEXT NOT NULL
                    );
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 14: {
                bool success = db->RunMany(R"(
                    ALTER TABLE keys RENAME TO keys_BAK;

                    CREATE TABLE keys (
                        id INTEGER PRIMARY KEY NOT NULL,
                        owner INTEGER REFERENCES users (id) ON DELETE CASCADE,
                        title TEXT NOT NULL,
                        key TEXT NOT NULL,
                        hash TEXT NOT NULL
                    );

                    INSERT INTO keys (id, owner, title, key, hash)
                        SELECT k.id, r.owner, k.title, k.key, k.hash
                        FROM keys_BAK k
                        INNER JOIN repositories r ON (r.id = k.repository);

                    DROP TABLE keys_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 15: {
                bool success = db->RunMany(R"(
                    CREATE UNIQUE INDEX keys_k ON keys (key);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 16: {
                bool success = db->RunMany(R"(
                    ALTER TABLE users ADD COLUMN totp TEXT;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 17: {
                bool success = db->RunMany(R"(
                    CREATE TABLE plans (
                        id INTEGER PRIMARY KEY NOT NULL,
                        owner INTEGER REFERENCES users (id) ON DELETE CASCADE,
                        name TEXT NOT NULL,
                        key TEXT NOT NULL,
                        hash TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX plans_k ON plans (key);

                    CREATE TABLE items (
                        id INTEGER PRIMARY KEY NOT NULL,
                        plan INTEGER REFERENCES plans (id) ON DELETE CASCADE,
                        channel TEXT NOT NULL,
                        days INTEGER NOT NULL,
                        clock INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX items_pc ON items (plan, channel);

                    CREATE TABLE paths (
                        item INTEGER PRIMARY KEY NOT NULL,
                        path TEXT NOT NULL
                    );
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 18: {
                bool success = db->RunMany(R"(
                    DROP INDEX keys_k;
                    DROP TABLE keys;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 19: {
                bool success = db->RunMany(R"(
                    DROP TABLE paths;

                    CREATE TABLE paths (
                        item INTEGER REFERENCES items (id) ON DELETE CASCADE,
                        path TEXT NOT NULL
                    );
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 20: {
                bool success = db->RunMany(R"(
                    CREATE TABLE runs (
                        id INTEGER PRIMARY KEY NOT NULL,
                        item INTEGER REFERENCES items (id) ON DELETE CASCADE,
                        timestamp INTEGER NOT NULL,
                        failed TEXT
                    );

                    ALTER TABLE items ADD COLUMN run INTEGER REFERENCES runs (id) ON DELETE CASCADE;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 21: {
                bool success = db->RunMany(R"(
                    ALTER TABLE items ADD COLUMN changeset BLOB;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 22: {
                bool success = db->RunMany(R"(
                    PRAGMA defer_foreign_keys = ON;

                    DROP TABLE items;
                    DROP TABLE paths;
                    DROP TABLE runs;
                    DROP TABLE plans;

                    CREATE TABLE plans (
                        id INTEGER PRIMARY KEY NOT NULL,
                        owner INTEGER REFERENCES users (id) ON DELETE CASCADE,
                        repository INTEGER REFERENCES repositories (id) ON DELETE CASCADE,
                        name TEXT NOT NULL,
                        key TEXT NOT NULL,
                        hash TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX plans_k ON plans (key);

                    CREATE TABLE items (
                        id INTEGER PRIMARY KEY NOT NULL,
                        plan INTEGER REFERENCES plans (id) ON DELETE CASCADE,
                        channel TEXT NOT NULL,
                        days INTEGER NOT NULL,
                        clock INTEGER NOT NULL,
                        run INTEGER REFERENCES runs (id) ON DELETE CASCADE,
                        changeset BLOB
                    );
                    CREATE UNIQUE INDEX items_pc ON items (plan, channel);

                    CREATE TABLE paths (
                        item INTEGER PRIMARY KEY NOT NULL,
                        path TEXT NOT NULL
                    );

                    CREATE TABLE runs (
                        id INTEGER PRIMARY KEY NOT NULL,
                        item INTEGER REFERENCES items (id) ON DELETE CASCADE,
                        timestamp INTEGER NOT NULL,
                        failed TEXT
                    );

                    PRAGMA defer_foreign_keys = OFF;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 23: {
                bool success = db->RunMany(R"(
                    DROP INDEX repositories_un;
                    CREATE UNIQUE INDEX repositories_on ON repositories (owner, name);

                    ALTER TABLE repositories DROP COLUMN user;
                    ALTER TABLE repositories DROP COLUMN password;

                    DROP TABLE variables;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 24: {
                bool success = db->RunMany(R"(
                    DROP INDEX items_pc;

                    ALTER TABLE items RENAME TO items_BAK;
                    ALTER TABLE paths RENAME TO paths_BAK;
                    ALTER TABLE runs RENAME TO runs_BAK;

                    CREATE TABLE items (
                        id INTEGER PRIMARY KEY NOT NULL,
                        plan INTEGER REFERENCES plans (id) ON DELETE CASCADE,
                        channel TEXT NOT NULL,
                        days INTEGER NOT NULL,
                        clock INTEGER NOT NULL,
                        run INTEGER REFERENCES runs (id) ON DELETE SET NULL,
                        changeset BLOB
                    );
                    CREATE UNIQUE INDEX items_pc ON items (plan, channel);

                    CREATE TABLE paths (
                        item INTEGER PRIMARY KEY NOT NULL,
                        path TEXT NOT NULL
                    );

                    CREATE TABLE runs (
                        id INTEGER PRIMARY KEY NOT NULL,
                        plan INTEGER NOT NULL,
                        channel TEXT NOT NULL,
                        timestamp INTEGER NOT NULL,
                        oid TEXT,
                        error TEXT,

                        FOREIGN KEY (plan, channel) REFERENCES items (plan, channel) ON DELETE CASCADE
                    );

                    INSERT INTO items (id, plan, channel, days, clock)
                        SELECT id, plan, channel, days, clock FROM items_BAK;
                    INSERT INTO paths (item, path)
                        SELECT item, path FROM paths_BAK;

                    DROP TABLE items_BAK;
                    DROP TABLE paths_BAK;
                    DROP TABLE runs_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 25: {
                bool success = db->RunMany(R"(
                    ALTER TABLE runs RENAME TO reports;
                    ALTER TABLE items RENAME COLUMN run TO report;
                )");
                if (!success)
                    return false;
            } // [[fallthrough]];

            static_assert(DatabaseVersion == 26);
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
