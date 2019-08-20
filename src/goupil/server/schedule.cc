// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "schedule.hh"
#include "data.hh"
#include "goupil.hh"

namespace RG {

static const char *const ScheduleNames[] = {
    "pl"
};

static bool GetQueryInteger(const http_RequestInfo &request, const char *key,
                            http_IO *io, int *out_value)
{
    const char *str = request.GetQueryValue(key);
    if (!str) {
        LogError("Missing '%1' parameter", key);
        http_ProduceErrorPage(422, io);
        return false;
    }

    int value;
    if (!ParseDec(str, &value)) {
        http_ProduceErrorPage(422, io);
        return false;
    }

    *out_value = value;
    return true;
}

// SQL must use 3 bind parameters: schedule, start date, end date (in this order)
static sqlite3_stmt *PrepareMonthQuery(const http_RequestInfo &request, const char *sql,
                                       http_IO *io)
{
    // Get query parameters
    const char *schedule_name;
    int year;
    int month;
    schedule_name = request.GetQueryValue("schedule");
    if (!GetQueryInteger(request, "year", io, &year))
        return nullptr;
    if (!GetQueryInteger(request, "month", io, &month))
        return nullptr;

    // Check arguments
    if (!std::any_of(std::begin(ScheduleNames), std::end(ScheduleNames),
                     [&](const char *name) { return TestStr(schedule_name, name); })) {
        LogError("Invalid schedule name '%1'", schedule_name);
        http_ProduceErrorPage(422, io);
        return nullptr;
    }
    if (month < 1 || month > 12) {
        LogError("Invalid month value %1", month);
        http_ProduceErrorPage(422, io);
        return nullptr;
    }

    // Determine query range
    Date dates[2];
    dates[0] = Date(year, month, 1);
    dates[1] = month < 12 ? Date(year, month + 1, 1) : Date(year + 1, 1, 1);

    // Prepare statement
    sqlite3_stmt *stmt;
    {
        char buf[32];

        if (sqlite3_prepare_v2(goupil_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            LogError("SQLite Error: %1", sqlite3_errmsg(goupil_db));
            http_ProduceErrorPage(500, io);
            return nullptr;
        }

        sqlite3_bind_text(stmt, 1, schedule_name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, Fmt(buf, "%1", dates[0]).ptr, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, Fmt(buf, "%1", dates[1]).ptr, -1, SQLITE_TRANSIENT);
    }

    return stmt;
}

void ProduceScheduleResources(const http_RequestInfo &request, http_IO *io)
{
    // Execute query
    sqlite3_stmt *stmt;
    {
        static const char *const sql = R"(
            SELECT date, time, slots, overbook
            FROM sched_resources
            WHERE schedule = ? AND date >= ? AND date < ?
            ORDER BY date, time
        )";

        stmt = PrepareMonthQuery(request, sql, io);
        if (!stmt)
            return;
    }
    RG_DEFER { sqlite3_finalize(stmt); };

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
            LogError("SQLite Error: %1", sqlite3_errmsg(goupil_db));
            http_ProduceErrorPage(500, io);
            return;
        }
    }
    json.EndObject();

    json.Finish(io);
}

void ProduceScheduleMeetings(const http_RequestInfo &request, http_IO *io)
{
    // Execute query
    sqlite3_stmt *stmt;
    {
        static const char *const sql = R"(
            SELECT date, time, identity
            FROM sched_meetings
            WHERE schedule = ? AND date >= ? AND date < ?
            ORDER BY date, time
        )";

        stmt = PrepareMonthQuery(request, sql, io);
        if (!stmt)
            return;
    }
    RG_DEFER { sqlite3_finalize(stmt); };

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
            LogError("SQLite Error: %1", sqlite3_errmsg(goupil_db));
            http_ProduceErrorPage(500, io);
            return;
        }
    }
    json.EndObject();

    json.Finish(io);
}

}
