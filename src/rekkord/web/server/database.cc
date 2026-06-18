// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "lib/native/kid/sqlite.hh"
#include "config.hh"
#include "database.hh"

namespace K {

const int DatabaseVersion = 40;

bool AddDatabaseFunctions(sq_Database *db)
{
    if (!InitKidSqlite(*db))
        return false;

    return true;
}

int GetDatabaseVersion(sq_Database *db)
{
    int version;
    if (!db->GetUserVersion(&version))
        return -1;
    return version;
}

bool MigrateDatabase(sq_Database *db)
{
    BlockAllocator temp_alloc;

    int version = GetDatabaseVersion(db);
    if (version < 0)
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
            } [[fallthrough]];

            case 26: {
                bool success = db->RunMany(R"(
                    ALTER TABLE plans ADD COLUMN scan INTEGER;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 27: {
                bool success = db->RunMany(R"(
                    CREATE TABLE users_NEW (
                        id INTEGER PRIMARY KEY NOT NULL,
                        mail TEXT COLLATE NOCASE NOT NULL,
                        password_hash TEXT,
                        username TEXT NOT NULL,
                        creation INTEGER NOT NULL,
                        confirmed INTEGER CHECK (confirmed IN (0, 1)) NOT NULL,
                        totp TEXT,
                        picture BLOB,
                        version INTEGER NOT NULL
                    );

                    INSERT INTO users_NEW (id, mail, password_hash, username, creation, confirmed, totp, picture, version)
                        SELECT id, mail, password_hash, username, creation, IIF(password_hash IS NOT NULL, 1, 0), totp, picture, version FROM users;
                    DROP TABLE users;
                    ALTER TABLE users_NEW RENAME TO users;

                    CREATE UNIQUE INDEX users_m ON users (mail);

                    CREATE TABLE identities (
                        id INTEGER PRIMARY KEY NOT NULL,
                        user INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
                        issuer TEXT NOT NULL,
                        sub TEXT NOT NULL,
                        authorized CHECK (authorized IN (0, 1)) NOT NULL
                    );
                    CREATE UNIQUE INDEX identities_is ON identities (issuer, sub);
                    CREATE INDEX identities_u ON identities (user);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 28: {
                bool success = db->RunMany(R"(
                    ALTER TABLE identities RENAME COLUMN authorized TO allowed;

                    CREATE TABLE tokens_NEW (
                        token TEXT NOT NULL,
                        type TEXT CHECK (type IN ('password', 'link')),
                        timestamp INTEGER NOT NULL,
                        user INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
                        identity INTEGER REFERENCES identities (id) ON DELETE CASCADE
                    );

                    INSERT INTO tokens_NEW (token, type, timestamp, user)
                        SELECT token, 'reset', timestamp, user FROM tokens;
                    DROP TABLE tokens;
                    ALTER TABLE tokens_NEW RENAME TO tokens;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 29: {
                bool success = db->RunMany(R"(
                    CREATE UNIQUE INDEX tokens_t ON tokens (token);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 30: {
                bool success = db->RunMany(R"(
                    DROP INDEX repositories_on;
                    ALTER TABLE repositories DROP COLUMN name;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 31: {
                bool success = db->RunMany(R"(
                    CREATE TABLE reports_NEW (
                        id INTEGER PRIMARY KEY NOT NULL,
                        plan INTEGER NOT NULL,
                        repository INTEGER NOT NULL,
                        channel TEXT NOT NULL,
                        timestamp INTEGER NOT NULL,
                        oid TEXT,
                        error TEXT,

                        FOREIGN KEY (plan, channel) REFERENCES items (plan, channel) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
                    );

                    INSERT INTO reports_NEW (id, plan, repository, channel, timestamp, oid, error)
                        SELECT r.id, r.plan, p.repository, r.channel, r.timestamp, r.oid, r.error
                        FROM reports r
                        INNER JOIN plans p ON (p.id = r.plan)
                        WHERE p.repository IS NOT NULL;
                    DROP TABLE reports;
                    ALTER TABLE reports_NEW RENAME TO reports;

                    CREATE INDEX reports_pc ON reports (plan, channel);

                    CREATE UNIQUE INDEX repositories_u ON repositories (url);
                    CREATE INDEX items_r ON items (report);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 32: {
                bool success = db->RunMany(R"(
                    ALTER TABLE repositories ADD COLUMN name TEXT;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 33: {
                bool success = db->RunMany(R"(
                    CREATE TABLE paths_NEW (
                        item INTEGER REFERENCES items (id) ON DELETE CASCADE,
                        path TEXT NOT NULL
                    );

                    INSERT INTO paths_NEW (item, path)
                        SELECT item, path FROM paths;
                    DROP TABLE paths;
                    ALTER TABLE paths_NEW RENAME TO paths;

                    CREATE INDEX paths_ip ON paths (item, path);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 34: {
                bool success = db->RunMany(R"(
                    ALTER TABLE mails RENAME COLUMN sent TO ctime;
                    ALTER TABLE mails ADD COLUMN timestamp INTEGER NOT NULL;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 35: {
                bool success = db->RunMany(R"(
                    ALTER TABLE users ADD COLUMN ckey BLOB;
                    UPDATE users SET ckey = rnd_safe(32);
                    ALTER TABLE users ALTER ckey SET NOT NULL;

                    CREATE TABLE drops (
                        id INTEGER PRIMARY KEY NOT NULL,
                        owner INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
                        kid BLOB NOT NULL,
                        name TEXT NOT NULL,
                        size INTEGER NOT NULL,
                        expire INTEGER,
                        chunk INTEGER NOT NULL,
                        uploaded INTEGER NOT NULL,
                        resume TEXT
                    );

                    CREATE UNIQUE INDEX drops_k ON drops (kid);
                    CREATE INDEX drops_o ON drops (owner);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 36: {
                bool success = db->RunMany(R"(
                    CREATE INDEX plans_o ON plans (owner);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 37: {
                bool success = db->RunMany(R"(
                    DELETE FROM drops;

                    ALTER TABLE drops ADD COLUMN salt TEXT NOT NULL;
                    ALTER TABLE drops ADD COLUMN protect INTEGER CHECK(protect IN (0, 1)) NOT NULL;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 38: {
                bool success = db->RunMany(R"(
                    DROP TABLE drops;

                    CREATE TABLE drops (
                        id INTEGER PRIMARY KEY NOT NULL,
                        owner INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
                        kid BLOB NOT NULL,
                        name TEXT NOT NULL,
                        size INTEGER NOT NULL,
                        expire INTEGER,
                        salt TEXT NOT NULL,
                        body TEXT NOT NULL,
                        nonce TEXT NOT NULL,
                        protect INTEGER CHECK(protect IN (0, 1)) NOT NULL,
                        split INTEGER NOT NULL,
                        uploaded INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX drops_k ON drops (kid);
                    CREATE INDEX drops_o ON drops (owner);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 39: {
                bool success = db->RunMany(R"(
                    ALTER TABLE drops ADD COLUMN deleted INTEGER CHECK(deleted IN (0, 1));
                    UPDATE drops SET deleted = 0;
                    ALTER TABLE drops ALTER COLUMN deleted SET NOT NULL;

                    CREATE INDEX drops_e ON drops (expire);
                    CREATE INDEX drops_d ON drops (deleted);
                )");
                if (!success)
                    return false;
            } // [[fallthrough]];

            static_assert(DatabaseVersion == 40);
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
