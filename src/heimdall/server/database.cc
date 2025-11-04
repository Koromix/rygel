// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/native/base/base.hh"
#include "database.hh"

namespace K {

const int DatabaseVersion = 9;

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

        // Maybe someone else has migrated the database...
        // So check again!
        int version = GetDatabaseVersion(db);
        if (version < 0)
            return false;

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
            } [[fallthrough]];

            case 4: {
                bool success = db->RunMany(R"(
                    CREATE INDEX items_p ON items (path);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 5: {
                bool success = db->RunMany(R"(
                    ALTER TABLE periods DROP COLUMN warning;
                    ALTER TABLE periods ADD COLUMN color TEXT;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 6: {
                bool success = db->RunMany(R"(
                    CREATE TABLE marks (
                        mark INTEGER NOT NULL PRIMARY KEY,
                        entity REFERENCES entities (entity) ON DELETE SET NULL,
                        name TEXT NOT NULL,
                        timestamp INTEGER NOT NULL,
                        status TEXT CHECK (status IN ('valid', 'invalid', 'wip')) NOT NULL,
                        comment TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX marks_e ON marks (entity);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 7: {
                bool success = db->RunMany(R"(
                    DROP INDEX marks_e;

                    ALTER TABLE marks RENAME TO marks_BAK;

                    CREATE TABLE marks (
                        mark INTEGER NOT NULL PRIMARY KEY,
                        entity REFERENCES entities (entity) ON DELETE SET NULL,
                        name TEXT NOT NULL,
                        timestamp INTEGER NOT NULL,
                        status INTEGER CHECK (status IN (0, 1)),
                        comment TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX marks_e ON marks (entity);

                    INSERT INTO marks (mark, entity, name, timestamp, status, comment)
                        SELECT mark, entity, name, timestamp, CASE status WHEN 'valid' THEN 1 WHEN 'invalid' THEN 0 END, comment FROM marks_BAK;

                    DROP TABLE marks_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 8: {
                // I just wanted to add a NOT NULL column to the "concepts" table... damn (o_o')

                bool success = db->RunMany(R"(
                    DROP INDEX concepts_d;
                    DROP INDEX concepts_dn;
                    DROP INDEX items_v;
                    DROP INDEX items_c;
                    DROP INDEX IF EXISTS items_p;
                    DROP INDEX events_e;
                    DROP INDEX events_c;
                    DROP INDEX periods_e;
                    DROP INDEX periods_c;
                    DROP INDEX measures_e;
                    DROP INDEX measures_c;

                    ALTER TABLE concepts RENAME TO concepts_BAK;
                    ALTER TABLE items RENAME TO items_BAK;
                    ALTER TABLE events RENAME TO events_BAK;
                    ALTER TABLE periods RENAME TO periods_BAK;
                    ALTER TABLE measures RENAME TO measures_BAK;

                    CREATE TABLE concepts (
                        concept INTEGER PRIMARY KEY NOT NULL,
                        domain INTEGER NOT NULL REFERENCES domains (domain) ON DELETE CASCADE,
                        name TEXT NOT NULL,
                        description TEXT NOT NULL,
                        changeset BLOB
                    );
                    CREATE UNIQUE INDEX concepts_dn ON concepts (domain, name);

                    CREATE TABLE items (
                        item INTEGER PRIMARY KEY NOT NULL,
                        view INTEGER NOT NULL REFERENCES views (view) ON DELETE CASCADE,
                        path TEXT NOT NULL,
                        concept INTEGER NOT NULL REFERENCES concepts (concept) ON DELETE SET NULL
                    );
                    CREATE INDEX items_v ON items (view);
                    CREATE INDEX items_c ON items (concept);
                    CREATE INDEX items_p ON items (path);

                    CREATE TABLE events (
                        event INTEGER PRIMARY KEY NOT NULL,
                        entity INTEGER NOT NULL REFERENCES entities (entity) ON DELETE CASCADE,
                        concept INTEGER NOT NULL REFERENCES concepts (concept) ON DELETE CASCADE,
                        timestamp INTEGER NOT NULL,
                        warning CHECK (warning IN (0, 1)) NOT NULL
                    );
                    CREATE INDEX events_e ON events (entity);
                    CREATE INDEX events_c ON events (concept);

                    CREATE TABLE periods (
                        period INTEGER PRIMARY KEY NOT NULL,
                        entity INTEGER NOT NULL REFERENCES entities (entity) ON DELETE CASCADE,
                        concept INTEGER NOT NULL REFERENCES concepts (concept) ON DELETE CASCADE,
                        timestamp INTEGER NOT NULL,
                        duration INTEGER NOT NULL,
                        color TEXT
                    );
                    CREATE INDEX periods_e ON periods (entity);
                    CREATE INDEX periods_c ON periods (concept);

                    CREATE TABLE measures (
                        measure INTEGER PRIMARY KEY NOT NULL,
                        entity INTEGER NOT NULL REFERENCES entities (entity) ON DELETE CASCADE,
                        concept INTEGER NOT NULL REFERENCES concepts (concept) ON DELETE CASCADE,
                        timestamp INTEGER NOT NULL,
                        value REAL NOT NULL,
                        warning CHECK (warning IN (0, 1)) NOT NULL
                    );
                    CREATE INDEX measures_e ON measures (entity);
                    CREATE INDEX measures_c ON measures (concept);

                    INSERT INTO concepts (concept, domain, name, description)
                        SELECT concept, domain, name, '' FROM concepts_BAK;
                    INSERT INTO items (item, view, path, concept)
                        SELECT item, view, path, concept FROM items_BAK;
                    INSERT INTO events (event, entity, concept, timestamp, warning)
                        SELECT event, entity, concept, timestamp, warning FROM events_BAK;
                    INSERT INTO periods (period, entity, concept, timestamp, duration, color)
                        SELECT period, entity, concept, timestamp, duration, color FROM periods_BAK;
                    INSERT INTO measures (measure, entity, concept, timestamp, value, warning)
                        SELECT measure, entity, concept, timestamp, value, warning FROM measures_BAK;

                    DROP TABLE events_BAK;
                    DROP TABLE periods_BAK;
                    DROP TABLE measures_BAK;
                    DROP TABLE items_BAK;
                    DROP TABLE concepts_BAK;
                )");
                if (!success)
                    return false;
            } // [[fallthrough]];

            static_assert(DatabaseVersion == 9);
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
