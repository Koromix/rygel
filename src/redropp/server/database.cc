// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "lib/native/kid/sqlite.hh"
#include "config.hh"
#include "database.hh"

namespace K {

const int DatabaseVersion = 5;

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

                    CREATE TABLE mails (
                        id INTEGER PRIMARY KEY NOT NULL,
                        address TEXT NOT NULL,
                        mail TEXT NOT NULL,
                        ctime INTEGER NOT NULL,
                        errors INTEGER NOT NULL,
                        timestamp INTEGER NOT NULL
                    );

                    CREATE TABLE users (
                        id INTEGER PRIMARY KEY NOT NULL,
                        mail TEXT COLLATE NOCASE NOT NULL,
                        password_hash TEXT,
                        username TEXT NOT NULL,
                        creation INTEGER NOT NULL,
                        confirmed INTEGER CHECK (confirmed IN (0, 1)) NOT NULL,
                        totp TEXT,
                        picture BLOB,
                        version INTEGER NOT NULL,
                        ckey BLOB NOT NULL
                    );
                    CREATE UNIQUE INDEX users_m ON users (mail);

                    CREATE TABLE identities (
                        id INTEGER PRIMARY KEY NOT NULL,
                        user INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
                        issuer TEXT NOT NULL,
                        sub TEXT NOT NULL,
                        allowed CHECK (allowed IN (0, 1)) NOT NULL
                    );
                    CREATE UNIQUE INDEX identities_is ON identities (issuer, sub);
                    CREATE INDEX identities_u ON identities (user);

                    CREATE TABLE tokens (
                        token TEXT NOT NULL,
                        type TEXT CHECK (type IN ('password', 'link')),
                        timestamp INTEGER NOT NULL,
                        user INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
                        identity INTEGER REFERENCES identities (id) ON DELETE CASCADE
                    );
                    CREATE UNIQUE INDEX tokens_t ON tokens (token);

                    CREATE TABLE drops (
                        id INTEGER PRIMARY KEY NOT NULL,
                        owner INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
                        kid BLOB NOT NULL,
                        name TEXT NOT NULL,
                        size INTEGER NOT NULL,
                        expire INTEGER,
                        protect INTEGER CHECK(protect IN (0, 1)) NOT NULL,
                        header TEXT NOT NULL,
                        nonce TEXT NOT NULL,
                        split INTEGER NOT NULL,
                        uploaded INTEGER NOT NULL,
                        deleted INTEGER CHECK(deleted IN (0, 1)) NOT NULL
                    );
                    CREATE UNIQUE INDEX drops_k ON drops (kid);
                    CREATE INDEX drops_o ON drops (owner);
                    CREATE INDEX drops_e ON drops (expire);
                    CREATE INDEX drops_d ON drops (deleted);

                    CREATE TABLE quotas (
                        user INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
                        total INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX quotas_u ON quotas (user);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 1: {
                bool success = db->RunMany(R"(
                    CREATE TABLE files (
                        id INTEGER PRIMARY KEY NOT NULL,
                        kid BLOB NOT NULL,
                        parent INTEGER NOT NULL REFERENCES drops (id) ON DELETE CASCADE,
                        name TEXT NOT NULL,
                        size INTEGER NOT NULL,
                        split INTEGER NOT NULL,
                        header TEXT NOT NULL,
                        nonce TEXT NOT NULL,
                        sequence INTEGER NOT NULL,
                        uploaded INTEGER NOT NULL,
                        changeset BLOB
                    );
                    CREATE UNIQUE INDEX files_k ON files (kid);
                    CREATE INDEX files_ps ON files (parent, sequence);

                    -- This inserts the wrong KID type but it will work okay and sort
                    -- itself out once these drops expire and/or get deleted.
                    INSERT INTO files (kid, parent, name, size, split, header, nonce, sequence, uploaded)
                        SELECT kid, id, name, size, split, header, nonce, 0, uploaded FROM drops;

                    ALTER TABLE drops ADD COLUMN complete INTEGER CHECK (complete IN (0, 1));
                    UPDATE drops SET complete = IIF(uploaded = size, 1, 0);
                    ALTER TABLE drops ALTER COLUMN complete SET NOT NULL;

                    UPDATE drops SET uploaded = size;
                    ALTER TABLE drops RENAME COLUMN uploaded TO total;

                    ALTER TABLE drops DROP COLUMN split;
                    ALTER TABLE drops DROP COLUMN header;
                    ALTER TABLE drops DROP COLUMN nonce;
                    ALTER TABLE drops DROP COLUMN size;

                    ALTER TABLE quotas RENAME COLUMN total TO usage;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 2: {
                bool success = db->RunMany(R"(
                    ALTER TABLE drops ALTER COLUMN name DROP NOT NULL;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 3: {
                bool success = db->RunMany(R"(
                    -- Use fake time for existing drops

                    ALTER TABLE drops ADD COLUMN ctime INTEGER;
                    UPDATE drops SET ctime = 1785855511000;
                    ALTER TABLE drops ALTER COLUMN ctime SET NOT NULL;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 4: {
                bool success = db->RunMany(R"(
                    UPDATE drops SET name = strftime('%Y%m%dT%H%M%S', ctime / 1000, 'unixepoch') WHERE name IS NULL;
                    ALTER TABLE drops ALTER COLUMN name SET NOT NULL;
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
