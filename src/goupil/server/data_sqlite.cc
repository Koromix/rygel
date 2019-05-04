// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../vendor/sqlite/sqlite3.h"
#include "../../libcc/libcc.hh"
#include "data.hh"

namespace RG {

static const char *const SchemaSQL = R"(
CREATE TABLE af_records (
    id TEXT NOT NULL,
    data TEXT NOT NULL
);
CREATE UNIQUE INDEX af_records_i ON af_records (id);

CREATE TABLE sc_resources (
    schedule TEXT NOT NULL CHECK(schedule IN ('pl', 'entreprise')),
    date TEXT NOT NULL,
    time INTEGER NOT NULL,

    slots INTEGER NOT NULL,
    overbook INTEGER NOT NULL
);
CREATE UNIQUE INDEX sc_resources_sdt ON sc_resources (schedule, date, time);

CREATE TABLE sc_identities (
    id INTEGER PRIMARY KEY,

    first_name TEXT NOT NULL,
    last_name TEXT NOT NULL,
    spouse_name TEXT,
    birthdate TEXT NOT NULL
);

CREATE TABLE sc_meetings (
    schedule TEXT NOT NULL CHECK(schedule IN ('pl', 'entreprise')),
    date TEXT NOT NULL,
    time INTEGER NOT NULL,

    identity_id INTEGER NOT NULL REFERENCES sc_identities(id)
);
CREATE INDEX sc_meetings_sd ON sc_meetings (schedule, date, time);
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
