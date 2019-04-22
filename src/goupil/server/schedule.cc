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

static int GetQuerySchedule(const http_Request &request, const char *key,
                            http_Response *out_response, const char **out_schedule_name)
{
    const char *str = request.GetQueryValue(key);
    if (!str) {
        LogError("Missing '%1' argument", key);
        return http_ProduceErrorPage(422, out_response);
    }

    if (!std::any_of(std::begin(ScheduleNames), std::end(ScheduleNames),
                     [&](const char *name) { return TestStr(str, name); })) {
        LogError("Invalid schedule name '%1'", str);
        return http_ProduceErrorPage(422, out_response);
    }

    *out_schedule_name = str;
    return 0;
}

static int GetQueryDate(const http_Request &request, const char *key,
                        http_Response *out_response, Date *out_date)
{
    const char *str = request.GetQueryValue(key);
    if (!str) {
        LogError("Missing '%1' parameter", key);
        return http_ProduceErrorPage(422, out_response);
    }

    Date date = Date::FromString(str);
    if (!date.value)
        return http_ProduceErrorPage(422, out_response);

    *out_date = date;
    return 0;
}

int ProduceScheduleResources(const http_Request &request, http_Response *out_response)
{
    out_response->flags |= (int)http_Response::Flag::DisableCache;

    // Get query parameters
    const char *schedule_name;
    Date dates[2];
    if (int code = GetQuerySchedule(request, "schedule", out_response, &schedule_name); code)
        return code;
    if (request.GetQueryValue("date")) {
        if (int code = GetQueryDate(request, "date", out_response, &dates[0]); code)
            return code;
        dates[1] = dates[0] + 1;
    } else {
        if (int code = GetQueryDate(request, "start", out_response, &dates[0]); code)
            return code;
        if (int code = GetQueryDate(request, "end", out_response, &dates[1]); code)
            return code;
    }

    static const char *sql = R"(
        SELECT date, time, slots, overbook
        FROM sc_resources
        WHERE schedule = ? AND date >= ? AND date < ?
        ORDER BY date, time
    )";

    sqlite3_stmt *stmt = nullptr;
    RG_DEFER { sqlite3_finalize(stmt); };
    {
        char buf[32];

        if (sqlite3_prepare_v2(goupil_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            LogError("SQLite Error: %1", sqlite3_errmsg(goupil_db));
            return http_ProduceErrorPage(500, out_response);
        }

        sqlite3_bind_text(stmt, 1, schedule_name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, Fmt(buf, "%1", dates[0]).ptr, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, Fmt(buf, "%1", dates[1]).ptr, -1, SQLITE_TRANSIENT);
    }

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
            return http_ProduceErrorPage(500, out_response);
        }
    }
    json.EndObject();

    return json.Finish(out_response);
}

int ProduceScheduleMeetings(const http_Request &request, http_Response *out_response)
{
    out_response->flags |= (int)http_Response::Flag::DisableCache;

    // Get query parameters
    const char *schedule_name;
    Date dates[2];
    if (int code = GetQuerySchedule(request, "schedule", out_response, &schedule_name); code)
        return code;
    if (request.GetQueryValue("date")) {
        if (int code = GetQueryDate(request, "date", out_response, &dates[0]); code)
            return code;
        dates[1] = dates[0] + 1;
    } else {
        if (int code = GetQueryDate(request, "start", out_response, &dates[0]); code)
            return code;
        if (int code = GetQueryDate(request, "end", out_response, &dates[1]); code)
            return code;
    }

    static const char *sql = R"(
        SELECT m.date, m.time, PRINTF('%s %s', i.first_name, i.last_name) AS identity
        FROM sc_meetings m
        INNER JOIN sc_identities i ON (i.id = m.consultant_id)
        WHERE m.schedule = ? AND m.date >= ? AND m.date < ?
        ORDER BY m.date, m.time
    )";

    sqlite3_stmt *stmt = nullptr;
    RG_DEFER { sqlite3_finalize(stmt); };
    {
        char buf[32];

        if (sqlite3_prepare_v2(goupil_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            LogError("SQLite Error: %1", sqlite3_errmsg(goupil_db));
            return http_ProduceErrorPage(500, out_response);
        }

        sqlite3_bind_text(stmt, 1, schedule_name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, Fmt(buf, "%1", dates[0]).ptr, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, Fmt(buf, "%1", dates[1]).ptr, -1, SQLITE_TRANSIENT);
    }

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
            return http_ProduceErrorPage(500, out_response);
        }
    }
    json.EndObject();

    return json.Finish(out_response);
}

}
