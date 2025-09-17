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

bool IsProjectNameSafe(const char *name)
{
    if (PathIsAbsolute(name))
        return false;
    if (PathContainsDotDot(name))
        return false;

    return true;
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

    if (!db->Open(filename, SQLITE_OPEN_READWRITE))
        return false;
    if (!db->SetWAL(true))
        return false;
    if (!MigrateDatabase(db))
        return true;

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
    if (!db.Prepare(R"(SELECT e.entity, e.name,
                              ev.event, ce.domain || '::' || ce.name, ev.timestamp, ev.warning,
                              pe.period, cp.domain || '::' || cp.name, pe.timestamp, pe.duration, pe.color,
                              me.measure, cm.domain || '::' || cm.name, me.timestamp, me.value, me.warning,
                              m.timestamp, IFNULL(m.status, -1), m.comment
                       FROM entities e
                       LEFT JOIN events ev ON (ev.entity = e.entity)
                       LEFT JOIN concepts ce ON (ce.concept = ev.concept)
                       LEFT JOIN periods pe ON (pe.entity = e.entity)
                       LEFT JOIN concepts cp ON (cp.concept = pe.concept)
                       LEFT JOIN measures me ON (me.entity = e.entity)
                       LEFT JOIN concepts cm ON (cm.concept = me.concept)
                       LEFT JOIN marks m ON (m.entity = e.entity)
                       ORDER BY e.name)", &stmt))
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

                json->Key("id"); json->Int64(entity);
                json->Key("name"); json->String(name);

                // Report mark information
                if (sqlite3_column_type(stmt, 16) != SQLITE_NULL) {
                    int64_t time = sqlite3_column_int64(stmt, 16);
                    int status = sqlite3_column_int(stmt, 17);
                    const char *comment = (const char *)sqlite3_column_text(stmt, 18);

                    json->Key("mark"); json->StartObject();

                    json->Key("time"); json->Int64(time);
                    if (status >= 0) {
                        json->Key("status"); json->Bool(status);
                    } else {
                        json->Key("status"); json->Null();
                    }
                    json->Key("comment"); json->String(comment);

                    json->EndObject();
                } else {
                    json->Key("mark"); json->Null();
                }

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

void HandleMark(http_IO *io)
{
    sq_Database db;
    if (!OpenProjectDatabase(io, &db))
        return;

    int64_t entity = -1;
    int status = -1;
    const char *comment = nullptr;
    {
        bool success = http_ParseJson(io, Kibibytes(4), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "entity") {
                    json->ParseInt(&entity);
                } else if (key == "status") {
                    if (json->SkipNull()) {
                        status = -1;
                    } else {
                        bool value;
                        json->ParseBool(&value);
                        status = value;
                    }
                } else if (key == "comment") {
                    json->ParseString(&comment);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (entity < 0) {
                    LogError("Missing or invalid 'entity' parameter");
                    valid = false;
                }

                if (!comment) {
                    LogError("Missing 'comment' parameter");
                    valid = false;
                }
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
    }

    const char *name;
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT name FROM entities WHERE entity = ?1", &stmt, entity))
            return;

        if (stmt.Step()) {
            name = DuplicateString((const char *)sqlite3_column_text(stmt, 0), io->Allocator()).ptr;
        } else {
            if (stmt.IsValid()) {
                LogError("Entity %1 does not exist", entity);
                io->SendError(404);
            }
            return;
        }
    }

    int64_t now = GetUnixTime();

    bool success = db.Transaction([&]() {
        if (!db.Run("UPDATE marks SET entity = NULL WHERE entity = ?1", entity))
            return false;

        if (!db.Run(R"(INSERT INTO marks (entity, name, timestamp, status, comment)
                       VALUES (?1, ?2, ?3, ?4, ?5))",
                    entity, name, now, status >= 0 ? sq_Binding(status) : sq_Binding(), comment)) {
            // Entity could have been deleted in the mean time
            if (sqlite3_errcode(db) == SQLITE_CONSTRAINT) {
                LogError("Entity %1 does not exist", entity);
                io->SendError(404);
            }
            return false;
        }

        return true;
    });
    if (!success)
        return;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        json->Key("time"); json->Int64(now);
        if (status >= 0) {
            json->Key("status"); json->Bool(status);
        } else {
            json->Key("status"); json->Null();
        }
        json->Key("comment"); json->String(comment);

        json->EndObject();
    });
}

}
