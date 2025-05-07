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

const int DatabaseVersion = 13;

bool MigrateDatabase(sq_Database *db, const char *vault_directory)
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
            } // [[fallthrough]];

            static_assert(DatabaseVersion == 13);
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
