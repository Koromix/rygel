// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "heimdall.hh"
#include "config.hh"
#include "database.hh"

namespace K {

bool IsProjectNameSafe(const char *project)
{
    return !PathIsAbsolute(project) && !PathContainsDotDot(project);
}

static bool OpenProjectDatabase(http_IO *io, sq_Database *db)
{
    K_ASSERT(!db->IsValid());

    const http_RequestInfo &request = io->Request();
    const char *project = request.GetQueryValue("project");

    K_DEFER_N(err_guard) { db->Close(); };

    if (!project) {
        LogError("Missing project parameter");
        io->SendError(422);
        return false;
    }
    if (!IsProjectNameSafe(project)) {
        LogError("Unsafe project name");
        io->SendError(403);
        return false;
    }

    const char *filename = Fmt(io->Allocator(), "%1%/%2.db", config.project_directory, project).ptr;

    if (!TestFile(filename)) {
        LogError("Unknown project '%1'", project);
        io->SendError(404);
        return false;
    }

    if (!db->Open(filename, SQLITE_OPEN_READONLY))
        return false;
    if (!db->SetWAL(true))
        return false;

    // Make sure we can read this database
    {
        int version = GetDatabaseVersion(db);
        if (version < 0)
            return false;
        if (version != DatabaseVersion) {
            LogError("Cannot read from database schema version %1 (expected %2)", version, DatabaseVersion);
            io->SendError(403);
            return false;
        }
    }

    err_guard.Disable();
    return true;
}

void HandleViews(http_IO *io)
{
    sq_Database db;
    if (!OpenProjectDatabase(io, &db))
        return;

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT v.view, v.name,
                              row_number() OVER (PARTITION BY i.view, i.path ORDER BY i.path),
                              i.path, c.domain || '::' || c.name
                       FROM views v
                       INNER JOIN items i ON (i.view = v.view)
                       INNER JOIN concepts c ON (c.concept = i.concept)
                       ORDER BY v.view, i.path)", &stmt))
        return;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();
        if (stmt.Step()) {
            do {
                int64_t view = sqlite3_column_int64(stmt, 0);
                const char *name = (const char *)sqlite3_column_text(stmt, 1);

                json->StartObject();

                json->Key("name"); json->String(name);
                json->Key("items"); json->StartObject();
                do {
                    int number = sqlite3_column_int(stmt, 2);
                    const char *path = (const char *)sqlite3_column_text(stmt, 3);

                    json->Key(path); json->StartArray();
                    do {
                        const char *name = (const char *)sqlite3_column_text(stmt, 4);
                        json->String(name);
                    } while (stmt.Step() && sqlite3_column_int64(stmt, 2) > number);
                    json->EndArray();
                } while (stmt.IsRow() && sqlite3_column_int64(stmt, 0) == view);
                json->EndObject();

                json->EndObject();
            } while (stmt.IsRow());
        }
        if (!stmt.IsValid())
            return;
        json->EndArray();
    });
}

void HandleEntities(http_IO *io)
{
    sq_Database db;
    if (!OpenProjectDatabase(io, &db))
        return;

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT en.entity, en.name,
                              ev.event, ce.domain || '::' || ce.name, ev.timestamp, ev.warning,
                              p.period, cp.domain || '::' || cp.name, p.timestamp, p.duration, p.color,
                              m.measure, cm.domain || '::' || cm.name, m.timestamp, m.value, m.warning
                       FROM entities en
                       LEFT JOIN events ev ON (ev.entity = en.entity)
                       LEFT JOIN concepts ce ON (ce.concept = ev.concept)
                       LEFT JOIN periods p ON (p.entity = en.entity)
                       LEFT JOIN concepts cp ON (cp.concept = p.concept)
                       LEFT JOIN measures m ON (m.entity = en.entity)
                       LEFT JOIN concepts cm ON (cm.concept = m.concept)
                       ORDER BY en.name)", &stmt))
        return;

    // Reuse for performance
    HeapArray<char> events;
    HeapArray<char> periods;
    HeapArray<char> values;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();
        if (stmt.Step()) {
            do {
                events.RemoveFrom(0);
                periods.RemoveFrom(0);
                values.RemoveFrom(0);

                int64_t entity = sqlite3_column_int64(stmt, 0);
                const char *name = (const char *)sqlite3_column_text(stmt, 1);

                json->StartObject();

                json->Key("name"); json->String(name);

                int64_t start = INT64_MAX;
                int64_t end = INT64_MIN;

                // Walk elements
                {
                    StreamWriter st1(&events, "<json>");
                    StreamWriter st2(&periods, "<json>");
                    StreamWriter st3(&values, "<json>");
                    json_Writer json1(&st1);
                    json_Writer json2(&st2);
                    json_Writer json3(&st3);

                    int64_t prev_event = 0;
                    int64_t prev_period = 0;
                    int64_t prev_measure = 0;

                    json1.StartArray();
                    json2.StartArray();
                    json3.StartArray();
                    do {
                        if (int64_t event = sqlite3_column_int64(stmt, 2); event > prev_event) {
                            prev_event = event;

                            const char *name = (const char *)sqlite3_column_text(stmt, 3);
                            int64_t time = sqlite3_column_int64(stmt, 4);
                            bool warning = sqlite3_column_int(stmt, 5);

                            json1.StartObject();
                            json1.Key("concept"); json1.String(name);
                            json1.Key("time"); json1.Int64(time);
                            json1.Key("warning"); json1.Bool(warning);
                            json1.EndObject();

                            start = std::min(start, time);
                            end = std::max(end, time);
                        }

                        if (int64_t period = sqlite3_column_int64(stmt, 6); period > prev_period) {
                            prev_period = period;

                            const char *name = (const char *)sqlite3_column_text(stmt, 7);
                            int64_t time = sqlite3_column_int64(stmt, 8);
                            int64_t duration = sqlite3_column_int64(stmt, 9);
                            const char *color = (const char *)sqlite3_column_text(stmt, 10);

                            json2.StartObject();
                            json2.Key("concept"); json2.String(name);
                            json2.Key("time"); json2.Int64(time);
                            json2.Key("duration"); json2.Int64(duration);
                            if (color) {
                                json2.Key("color"); json2.String(color);
                            } else {
                                json2.Key("color"); json2.Null();
                            }
                            json2.EndObject();

                            start = std::min(start, time);
                            end = std::max(end, time + duration);
                        }

                        if (int64_t measure = sqlite3_column_int64(stmt, 11); measure > prev_measure) {
                            prev_measure = measure;

                            const char *name = (const char *)sqlite3_column_text(stmt, 12);
                            int64_t time = sqlite3_column_int64(stmt, 13);
                            double value = sqlite3_column_double(stmt, 14);
                            bool warning = sqlite3_column_int(stmt, 15);

                            json3.StartObject();
                            json3.Key("concept"); json3.String(name);
                            json3.Key("time"); json3.Int64(time);
                            json3.Key("value"); json3.Double(value);
                            json3.Key("warning"); json3.Bool(warning);
                            json3.EndObject();

                            start = std::min(start, time);
                            end = std::max(end, time);
                        }
                    } while (stmt.Step() && sqlite3_column_int64(stmt, 0) == entity);
                    if (!stmt.IsValid())
                        return;
                    json1.EndArray();
                    json2.EndArray();
                    json3.EndArray();
                }

                json->Key("events"); json->Raw(events);
                json->Key("periods"); json->Raw(periods);
                json->Key("values"); json->Raw(values);

                if (start < INT64_MAX) {
                    json->Key("start"); json->Int64(start);
                    json->Key("end"); json->Int64(end);
                } else {
                    json->Key("start"); json->Null();
                    json->Key("end"); json->Null();
                }

                json->EndObject();
            } while (stmt.IsRow());
        }
        if (!stmt.IsValid())
            return;
        json->EndArray();
    });
}

}
