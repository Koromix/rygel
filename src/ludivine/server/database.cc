// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "config.hh"
#include "database.hh"

namespace K {

static const int DatabaseVersion = 19;

int GetDatabaseVersion(sq_Database *db)
{
    int version;
    if (!db->GetUserVersion(&version))
        return -1;
    return version;
}

bool MigrateDatabase(sq_Database *db, const char *vault_directory)
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
                        id BLOB PRIMARY KEY NOT NULL,
                        email TEXT NOT NULL,
                        valid INTEGER CHECK (valid IN (0, 1)) NOT NULL
                    );
                    CREATE UNIQUE INDEX users_i ON users (id);
                    CREATE UNIQUE INDEX users_e ON users (email);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 1: {
                bool success = db->RunMany(R"(
                    DROP TABLE users;

                    CREATE TABLE users (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        uid BLOB NOT NULL,
                        email TEXT NOT NULL,
                        tkey TEXT,
                        token TEXT
                    );
                    CREATE UNIQUE INDEX users_u ON users (uid);
                    CREATE UNIQUE INDEX users_e ON users (email);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 2: {
                bool success = db->RunMany(R"(
                    DROP INDEX users_u;
                    DROP INDEX users_e;

                    ALTER TABLE users RENAME TO users_BAK;

                    CREATE TABLE users (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        uid BLOB NOT NULL,
                        email TEXT NOT NULL,
                        registration INTEGER NOT NULL,
                        token TEXT
                    );
                    CREATE UNIQUE INDEX users_u ON users (uid);
                    CREATE UNIQUE INDEX users_e ON users (email);

                    INSERT INTO users (id, uid, email, registration, token)
                        SELECT id, uid, email, 1, token FROM users_BAK;

                    DROP TABLE users_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 3: {
                bool success = db->RunMany(R"(
                    CREATE TABLE notifications (
                        id INTEGER PRIMARY KEY,
                        user INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
                        study INTEGER NOT NULL,
                        title TEXT NOT NULL,
                        start TEXT NOT NULL,
                        date TEXT NOT NULL,
                        offset INTEGER NOT NULL
                    );
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 4: {
                bool success = db->RunMany(R"(
                    ALTER TABLE notifications RENAME TO notifications_BAK;

                    CREATE TABLE notifications (
                        id INTEGER PRIMARY KEY,
                        user INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
                        study INTEGER NOT NULL,
                        title TEXT NOT NULL,
                        start TEXT NOT NULL,
                        date TEXT NOT NULL,
                        offset INTEGER NOT NULL,
                        partial INTEGER CHECK (partial IN (0, 1)) NOT NULL
                    );

                    INSERT INTO notifications (id, user, study, title, start, date, offset, partial)
                        SELECT id, user, study, title, start, date, offset, 0 FROM notifications_BAK;

                    DROP TABLE notifications_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 5: {
                bool success = db->RunMany(R"(
                    ALTER TABLE notifications RENAME TO events;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 6: {
                bool success = db->RunMany(R"(
                    CREATE TABLE vaults (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        vid BLOB NOT NULL,
                        generation INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX vaults_v ON vaults (vid);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 7: {
                bool success = db->RunMany(R"(
                    CREATE TABLE sets (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        rid BLOB NOT NULL
                    );
                    CREATE UNIQUE INDEX sets_r ON sets (rid);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 8: {
                bool success = db->RunMany(R"(
                    DROP INDEX vaults_v;
                    ALTER TABLE vaults RENAME TO vaults_BAK;

                    CREATE TABLE vaults (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        vid BLOB NOT NULL,
                        generation INTEGER NOT NULL,
                        previous INTEGER
                    );
                    CREATE UNIQUE INDEX vaults_v ON vaults (vid);

                    INSERT INTO vaults (id, vid, generation, previous)
                        SELECT id, vid, generation, IIF(generation > 1, generation - 1, NULL) FROM vaults_BAK;

                    CREATE TABLE generations (
                        id INTEGER PRIMARY KEY,
                        vault INTEGER NOT NULL REFERENCES vaults (id) ON DELETE CASCADE,
                        generation INTEGER NOT NULL,
                        previous INTEGER,
                        size INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX generations_vg ON generations (vault, generation);

                    DROP TABLE vaults_BAK;
                )");
                if (!success)
                    return false;

                sq_Statement stmt;
                if (!db->Prepare("SELECT id, uuid_str(vid), generation FROM vaults", &stmt))
                    return false;

                while (stmt.Step()) {
                    int64_t vault = sqlite3_column_int64(stmt, 0);
                    const char *vid = (const char *)sqlite3_column_text(stmt, 1);
                    int generation = sqlite3_column_int(stmt, 2);

                    for (int i = 1; i <= generation; i++) {
                        const char *filename = Fmt(&temp_alloc, "%1%/%2%/%3.bin", vault_directory, vid, i).ptr;

                        FileInfo file_info;
                        StatResult ret = StatFile(filename, (int)StatFlag::SilentMissing, &file_info);

                        switch (ret) {
                            case StatResult::Success: {
                                if (!db->Run("INSERT INTO generations (vault, generation, previous, size) VALUES (?1, ?2, ?3, ?4)",
                                             vault, i, i > 1 ? sq_Binding(i - 1) : sq_Binding(), file_info.size))
                                    return false;
                            } break;

                            case StatResult::MissingPath: {} break;

                            case StatResult::AccessDenied:
                            case StatResult::OtherError: return false;
                        }
                    }
                }
                if (!stmt.IsValid())
                    return false;
            } [[fallthrough]];

            case 9: {
                bool success = db->RunMany(R"(
                    CREATE TABLE participants (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        rid BLOB NOT NULL
                    );
                    CREATE UNIQUE INDEX participants_r ON participants (rid);

                    CREATE TABLE tests (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        participant INTEGER NOT NULL REFERENCES participants (id) ON DELETE CASCADE,
                        study INTEGER NOT NULL,
                        key TEXT NOT NULL,
                        json TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX tests_psk ON tests (participant, study, key);

                    INSERT INTO participants (id, rid)
                        SELECT id, rid FROM sets;

                    DROP INDEX sets_r;
                    DROP TABLE sets;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 10: {
                bool success = db->RunMany(R"(
                    PRAGMA foreign_keys = 0;

                    DROP INDEX users_u;
                    DROP INDEX users_e;

                    ALTER TABLE users RENAME TO users_BAK;

                    CREATE TABLE users (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        uid BLOB NOT NULL,
                        email TEXT COLLATE NOCASE NOT NULL,
                        registration INTEGER NOT NULL,
                        token TEXT
                    );
                    CREATE UNIQUE INDEX users_u ON users (uid);
                    CREATE UNIQUE INDEX users_e ON users (email);

                    INSERT INTO users (id, uid, email, registration, token)
                        SELECT id, uid, email, registration, token FROM users_BAK;

                    DROP TABLE users_BAK;

                    PRAGMA foreign_keys = 1;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 11: {
                bool success = db->RunMany(R"(
                    ALTER TABLE events RENAME TO events_BAK;

                    CREATE TABLE events (
                        id INTEGER PRIMARY KEY,
                        user INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
                        study INTEGER NOT NULL,
                        title TEXT NOT NULL,
                        start TEXT NOT NULL,
                        date TEXT NOT NULL,
                        offset INTEGER NOT NULL,
                        partial INTEGER CHECK (partial IN (0, 1)) NOT NULL
                    );
                    CREATE INDEX events_u ON events (user);

                    INSERT INTO events (id, user, study, title, start, date, offset, partial)
                        SELECT id, user, study, title, start, date, offset, partial FROM events_BAK
                        WHERE user IN (SELECT id FROM users);

                    DROP TABLE events_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 12: {
                bool success = db->RunMany(R"(
                    ALTER TABLE users RENAME COLUMN email TO mail;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 13: {
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

            case 14: {
                bool success = db->RunMany(R"(
                    CREATE TABLE passwords (
                        id INTEGER PRIMARY KEY NOT NULL,
                        user INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
                        hash TEXT NOT NULL,
                        token TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX passwords_u ON passwords (user);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 15: {
                bool success = db->RunMany(R"(
                    CREATE TABLE tokens (
                        id INTEGER PRIMARY KEY NOT NULL,
                        user INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
                        type TEXT CHECK (type IN ('mail', 'password')) NOT NULL,
                        password_hash TEXT,
                        token TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX tokens_ut ON tokens (user, type);

                    INSERT INTO tokens (user, type, password_hash, token)
                        SELECT id, 'mail', NULL, token FROM users;
                    INSERT INTO tokens (user, type, password_hash, token)
                        SELECT user, 'password', hash, token FROM passwords;

                    DROP INDEX passwords_u;
                    DROP TABLE passwords;

                    UPDATE users SET registration = -registration WHERE token IS NULL;
                    ALTER TABLE users DROP COLUMN token;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 16: {
                bool success = db->RunMany(R"(
                    ALTER TABLE events ADD COLUMN sent INTEGER;
                    ALTER TABLE events ADD COLUMN changeset BLOB;
                    CREATE UNIQUE INDEX events_usd ON events (user, study, date);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 17: {
                int64_t now = GetUnixTime();

                bool success = db->RunMany(R"(
                    CREATE TABLE tests_NEW (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        participant INTEGER NOT NULL REFERENCES participants (id) ON DELETE CASCADE,
                        study INTEGER NOT NULL,
                        key TEXT NOT NULL,
                        ctime INTEGER NOT NULL,
                        mtime INTEGER NOT NULL,
                        json TEXT NOT NULL
                    );

                    INSERT INTO tests_NEW (id, participant, study, key, ctime, mtime, json)
                        SELECT id, participant, study, key, 0, 0, json FROM tests;
                    DROP TABLE tests;
                    ALTER TABLE tests_NEW RENAME TO tests;

                    CREATE UNIQUE INDEX tests_psk ON tests (participant, study, key);
                )");
                if (!success)
                    return false;

                if (!db->Run("UPDATE tests SET ctime = ?1, mtime = ?2", now, now))
                    return false;
            } [[fallthrough]];

            case 18: {
                bool success = db->RunMany(R"(
                    ALTER TABLE events DROP COLUMN changeset;
                    ALTER TABLE events ADD COLUMN ignored INTEGER;
                    ALTER TABLE events ADD COLUMN changeset BLOB;
                )");
                if (!success)
                    return false;
            } // [[fallthrough]];

            static_assert(DatabaseVersion == 19);
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
    if (!MigrateDatabase(&db, config.vault_directory))
        return false;
    if (!db.Close())
        return false;

    return true;
}

}
