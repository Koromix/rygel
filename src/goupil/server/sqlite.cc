// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../lib/sqlite/sqlite3.h"
#include "../../libcc/libcc.hh"
#include "sqlite.hh"

#define SCHEMA_VERSION 1

sqlite3 *OpenDatabase(const char *filename, unsigned int flags)
{
    const char *const sql = R"(
        PRAGMA foreign_keys = ON;
    )";

    sqlite3 *db = nullptr;
    DEFER_N(out_guard) { sqlite3_close(db); };

    if (sqlite3_open_v2(filename, &db, flags, nullptr) != SQLITE_OK) {
        LogError("SQLite failed to open '%1': %2", filename, sqlite3_errmsg(db));
        return nullptr;
    }

    char *error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        LogError("SQLite failed to open '%1': %2", filename, error);
        return nullptr;
    }

    out_guard.disable();
    return db;
}

bool InitDatabase(sqlite3 *db)
{
    const char *const sql = R"(
        CREATE TABLE gp_values (
            id INTEGER PRIMARY KEY,
            table_name TEXT NOT NULL,
            entity_id INTEGER NOT NULL,
            key TEXT NOT NULL,
            value BLOB,
            creation_date INTEGER NOT NULL,
            change_date INTEGER NOT NULL
        );

        CREATE TABLE gp_migrations (
            version INTEGER NOT NULL,
            date INTEGER NOT NULL,
            value_id INTEGER NOT NULL
        );

        INSERT INTO gp_migrations (version, date, value_id) VALUES (
            )" STRINGIFY(SCHEMA_VERSION) R"(,
            date('now'),
            0
        );
    )";

    char *error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        LogError("SQLite request failed: %1", error);
        sqlite3_free(error);

        return false;
    }

    return true;
}
