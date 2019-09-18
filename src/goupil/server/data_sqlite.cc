// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../vendor/sqlite/sqlite3.h"
#include "../../libcc/libcc.hh"
#include "data.hh"

namespace RG {

static const char *const SchemaSQL = R"(
CREATE TABLE assets (
    key TEXT NOT NULL,
    mimetype TEXT NOT NULL,
    data BLOB NOT NULL
);
CREATE UNIQUE INDEX assets_k ON assets (key);

CREATE TABLE form_records (
    id TEXT NOT NULL,
    table_name TEXT NOT NULL,
    data TEXT NOT NULL
);
CREATE UNIQUE INDEX form_records_i ON form_records (id);

CREATE TABLE form_variables (
    table_name TEXT NOT NULL,
    key TEXT NOT NULL,
    type TEXT NOT NULL,
    before TEXT,
    after TEXT
);
CREATE UNIQUE INDEX form_variables_tk ON form_variables (table_name, key);

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
)";

bool SQLiteDatabase::Open(const char *filename, unsigned int flags)
{
    static const char *const sql = R"(
        PRAGMA foreign_keys = ON;
    )";

    RG_ASSERT(!db);
    RG_DEFER_N(out_guard) { Close(); };

    if (sqlite3_open_v2(filename, &db, flags, nullptr) != SQLITE_OK) {
        LogError("SQLite failed to open '%1': %2", filename, sqlite3_errmsg(db));
        return false;
    }

    char *error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        LogError("SQLite failed to open '%1': %2", filename, error);
        return false;
    }

    out_guard.Disable();
    return true;
}

bool SQLiteDatabase::Close()
{
    if (sqlite3_close(db) != SQLITE_OK)
        return false;
    db = nullptr;

    return true;
}

bool SQLiteDatabase::Execute(const char *sql)
{
    char *error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        LogError("SQLite request failed: %1", error);
        sqlite3_free(error);

        return false;
    }

    return true;
}

bool SQLiteDatabase::CreateSchema()
{
    return Execute(SchemaSQL);
}

}
