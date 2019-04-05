// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../vendor/sqlite/sqlite3.h"
#include "../../libcc/libcc.hh"
#include "data.hh"

bool SQLiteConnection::Open(const char *filename, unsigned int flags)
{
    const char *const sql = R"(
        PRAGMA foreign_keys = ON;
    )";

    Assert(!db);
    DEFER_N(out_guard) { Close(); };

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

bool SQLiteConnection::Close()
{
    if (sqlite3_close(db) != SQLITE_OK)
        return false;
    db = nullptr;

    return true;
}
