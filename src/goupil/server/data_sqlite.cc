// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../vendor/sqlite/sqlite3.h"
#include "../../libcc/libcc.hh"
#include "data.hh"

namespace RG {

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

bool SQLiteDatabase::CreateSchema()
{
    static const char *const sql = R"(
        CREATE TABLE sc_resources (
            schedule TEXT NOT NULL CHECK(schedule IN ('pl', 'entreprise')),
            date TEXT NOT NULL,
            time INTEGER NOT NULL,

            slots INTEGER NOT NULL,
            overbook INTEGER NOT NULL
        );
        CREATE UNIQUE INDEX sc_resources_sdt ON sc_resources (schedule, date, time, slots, overbook);

        CREATE TABLE sc_meetings (
            schedule TEXT NOT NULL CHECK(schedule IN ('pl', 'entreprise')),
            date TEXT NOT NULL,
            time INTEGER NOT NULL,

            name TEXT NOT NULL
        );
        CREATE INDEX sc_meetings_sd ON sc_meetings (schedule, date, time, name);
    )";

    char *error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        LogError("SQLite request failed: %1", error);
        sqlite3_free(error);

        return false;
    }

    return true;
}

}
