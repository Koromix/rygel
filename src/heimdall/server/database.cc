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
#include "database.hh"

namespace K {

const int DatabaseVersion = 4;

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

                    CREATE TABLE entities (
                        entity INTEGER PRIMARY KEY NOT NULL,
                        name TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX entities_n ON entities (name);

                    CREATE TABLE domains (
                        domain INTEGER PRIMARY KEY NOT NULL,
                        name TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX domains_n ON domains (name);

                    CREATE TABLE concepts (
                        concept INTEGER PRIMARY KEY NOT NULL,
                        domain INTEGER NOT NULL REFERENCES domains (domain) ON DELETE CASCADE,
                        name TEXT NOT NULL,
                        changeset BLOB
                    );
                    CREATE UNIQUE INDEX concepts_dn ON concepts (domain, name);

                    CREATE TABLE views (
                        view INTEGER PRIMARY KEY NOT NULL,
                        name TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX views_n ON views (name);

                    CREATE TABLE items (
                        item INTEGER PRIMARY KEY NOT NULL,
                        view INTEGER NOT NULL REFERENCES views (view) ON DELETE CASCADE,
                        path TEXT NOT NULL,
                        concept INTEGER NOT NULL REFERENCES concepts (concept) ON DELETE SET NULL
                    );

                    CREATE TABLE events (
                        element INTEGER PRIMARY KEY NOT NULL,
                        entity INTEGER NOT NULL REFERENCES entities (entity) ON DELETE CASCADE,
                        concept INTEGER NOT NULL REFERENCES concepts (concept) ON DELETE CASCADE,
                        timestamp INTEGER NOT NULL,
                        warning CHECK (warning IN (0, 1)) NOT NULL
                    );

                    CREATE TABLE periods (
                        period INTEGER PRIMARY KEY NOT NULL,
                        entity INTEGER NOT NULL REFERENCES entities (entity) ON DELETE CASCADE,
                        concept INTEGER NOT NULL REFERENCES concepts (concept) ON DELETE CASCADE,
                        timestamp INTEGER NOT NULL,
                        duration INTEGER NOT NULL
                    );

                    CREATE TABLE measures (
                        measure INTEGER PRIMARY KEY NOT NULL,
                        entity INTEGER NOT NULL REFERENCES entities (entity) ON DELETE CASCADE,
                        concept INTEGER NOT NULL REFERENCES concepts (concept) ON DELETE CASCADE,
                        timestamp INTEGER NOT NULL,
                        value REAL NOT NULL,
                        warning CHECK (warning IN (0, 1)) NOT NULL
                    );
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 1: {
                bool success = db->RunMany(R"(
                    ALTER TABLE periods RENAME TO periods_BAK;

                    CREATE TABLE periods (
                        period INTEGER PRIMARY KEY NOT NULL,
                        entity INTEGER NOT NULL REFERENCES entities (entity) ON DELETE CASCADE,
                        concept INTEGER NOT NULL REFERENCES concepts (concept) ON DELETE CASCADE,
                        timestamp INTEGER NOT NULL,
                        duration INTEGER NOT NULL,
                        warning CHECK (warning IN (0, 1)) NOT NULL
                    );

                    INSERT INTO periods (period, entity, concept, timestamp, duration, warning)
                        SELECT period, entity, concept, timestamp, duration, 0 FROM periods_BAK;

                    DROP TABLE periods_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 2: {
                bool success = db->RunMany(R"(
                    ALTER TABLE events RENAME COLUMN element TO event;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 3: {
                bool success = db->RunMany(R"(
                    CREATE INDEX concepts_d ON concepts (domain);

                    CREATE INDEX items_v ON items (view);
                    CREATE INDEX items_c ON items (concept);

                    CREATE INDEX events_e ON events (entity);
                    CREATE INDEX events_c ON events (concept);
                    CREATE INDEX periods_e ON periods (entity);
                    CREATE INDEX periods_c ON periods (concept);
                    CREATE INDEX measures_e ON measures (entity);
                    CREATE INDEX measures_c ON measures (concept);
                )");
                if (!success)
                    return false;
            } // [[fallthrough]];

            static_assert(DatabaseVersion == 4);
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

}
