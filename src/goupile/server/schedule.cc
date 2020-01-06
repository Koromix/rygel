// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "goupile.hh"
#include "schedule.hh"
#include "../../core/libwrap/sqlite.hh"

namespace RG {

static bool GetQueryInteger(const http_RequestInfo &request, const char *key,
                            http_IO *io, int *out_value)
{
    const char *str = request.GetQueryValue(key);
    if (!str) {
        LogError("Missing '%1' parameter", key);
        io->AttachError(422);
        return false;
    }

    int value;
    if (!ParseDec(str, &value)) {
        io->AttachError(422);
        return false;
    }

    *out_value = value;
    return true;
}

// SQL must use 3 bind parameters: schedule, start date, end date (in this order)
static bool PrepareMonthQuery(const http_RequestInfo &request, http_IO *io,
                              const char *sql, SQLiteStatement *out_stmt)
{
    // Get query parameters
    const char *schedule_name;
    int year;
    int month;
    schedule_name = request.GetQueryValue("schedule");
    if (!schedule_name) {
        LogError("Missing 'schedule' parameter");
        io->AttachError(422);
        return false;
    }
    if (!GetQueryInteger(request, "year", io, &year))
        return false;
    if (!GetQueryInteger(request, "month", io, &month))
        return false;

    // Check arguments
    // XXX: Check that schedule_name if a valid asset, with the proper mimetype
    if (month < 1 || month > 12) {
        LogError("Invalid month value %1", month);
        io->AttachError(422);
        return false;
    }

    // Determine query range
    Date dates[2];
    dates[0] = Date(year, month, 1);
    dates[1] = month < 12 ? Date(year, month + 1, 1) : Date(year + 1, 1, 1);

    // Prepare statement
    {
        char buf[32];

        if (!goupile_db.Prepare(sql, out_stmt))
            return false;
        sqlite3_bind_text(*out_stmt, 1, schedule_name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(*out_stmt, 2, Fmt(buf, "%1", dates[0]).ptr, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(*out_stmt, 3, Fmt(buf, "%1", dates[1]).ptr, -1, SQLITE_TRANSIENT);
    }

    return true;
}

void HandleScheduleResources(const http_RequestInfo &request, http_IO *io)
{
    SQLiteStatement stmt;
    if (!PrepareMonthQuery(request, io,
                           R"(SELECT date, time, slots, overbook
                              FROM sched_resources
                              WHERE schedule = ? AND date >= ? AND date < ?
                              ORDER BY date, time)", &stmt))
        return;

    // Export data
    http_JsonPageBuilder json(request.compression_type);

    json.StartObject();
    {
        char current_date[32] = {};

        int rc = sqlite3_step(stmt);
        while (rc == SQLITE_ROW) {
            strncpy(current_date, (const char *)sqlite3_column_text(stmt, 0),
                    RG_SIZE(current_date) - 1);

            json.Key(current_date);
            json.StartArray();
            do {
                json.StartObject();
                json.Key("time"); json.Int(sqlite3_column_int(stmt, 1));
                json.Key("slots"); json.Int(sqlite3_column_int(stmt, 2));
                json.Key("overbook"); json.Int(sqlite3_column_int(stmt, 3));
                json.EndObject();
            } while ((rc = sqlite3_step(stmt)) == SQLITE_ROW &&
                     TestStr((const char *)sqlite3_column_text(stmt, 0), current_date));
            json.EndArray();
        }
        if (rc != SQLITE_DONE) {
            LogError("SQLite Error: %1", sqlite3_errmsg(goupile_db));
            return;
        }
    }
    json.EndObject();

    json.Finish(io);
}

void HandleScheduleMeetings(const http_RequestInfo &request, http_IO *io)
{
    SQLiteStatement stmt;
    if (!PrepareMonthQuery(request, io,
                           R"(SELECT date, time, identity
                              FROM sched_meetings
                              WHERE schedule = ? AND date >= ? AND date < ?
                              ORDER BY date, time)", &stmt))
        return;

    // Export data
    http_JsonPageBuilder json(request.compression_type);

    json.StartObject();
    {
        char current_date[32] = {};

        int rc = sqlite3_step(stmt);
        while (rc == SQLITE_ROW) {
            strncpy(current_date, (const char *)sqlite3_column_text(stmt, 0),
                    RG_SIZE(current_date) - 1);

            json.Key(current_date);
            json.StartArray();
            do {
                json.StartObject();
                json.Key("time"); json.Int(sqlite3_column_int(stmt, 1));
                json.Key("identity"); json.String((const char *)sqlite3_column_text(stmt, 2));
                json.EndObject();
            } while ((rc = sqlite3_step(stmt)) == SQLITE_ROW &&
                     TestStr((const char *)sqlite3_column_text(stmt, 0), current_date));
            json.EndArray();
        }
        if (rc != SQLITE_DONE) {
            LogError("SQLite Error: %1", sqlite3_errmsg(goupile_db));
            return;
        }
    }
    json.EndObject();

    json.Finish(io);
}

}
