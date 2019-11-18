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

static const char *const DemoSQL = R"(
BEGIN TRANSACTION;

INSERT INTO sched_resources VALUES ('pl', '2019-08-01', 730, 1, 1);
INSERT INTO sched_resources VALUES ('pl', '2019-08-01', 1130, 2, 0);
INSERT INTO sched_resources VALUES ('pl', '2019-08-02', 730, 1, 1);
INSERT INTO sched_resources VALUES ('pl', '2019-08-02', 1130, 2, 0);
INSERT INTO sched_resources VALUES ('pl', '2019-08-05', 730, 1, 1);
INSERT INTO sched_resources VALUES ('pl', '2019-08-05', 1130, 2, 0);
INSERT INTO sched_resources VALUES ('pl', '2019-08-06', 730, 1, 1);
INSERT INTO sched_resources VALUES ('pl', '2019-08-06', 1130, 2, 0);
INSERT INTO sched_resources VALUES ('pl', '2019-08-07', 730, 1, 1);
INSERT INTO sched_resources VALUES ('pl', '2019-08-07', 1130, 2, 0);

INSERT INTO sched_meetings VALUES ('pl', '2019-08-01', 730, 'Gwen STACY');
INSERT INTO sched_meetings VALUES ('pl', '2019-08-01', 730, 'Peter PARKER');
INSERT INTO sched_meetings VALUES ('pl', '2019-08-01', 730, 'Mary JANE PARKER');
INSERT INTO sched_meetings VALUES ('pl', '2019-08-02', 730, 'Clark KENT');
INSERT INTO sched_meetings VALUES ('pl', '2019-08-02', 1130, 'Lex LUTHOR');

END TRANSACTION;
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

bool SQLiteDatabase::InsertDemo()
{
    return Execute(DemoSQL);
}

}
