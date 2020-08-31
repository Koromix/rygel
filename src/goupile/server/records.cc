// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "config.hh"
#include "goupile.hh"
#include "ports.hh"
#include "user.hh"
#include "../../core/libwrap/json.hh"
#include "../../core/libwrap/sqlite.hh"

namespace RG {

static void ExportRecord(sq_Statement *stmt, Span<const char> table_name, json_Writer *json)
{
    const char *id = (const char *)sqlite3_column_text(*stmt, 0);

    json->StartObject();

    json->Key("table"); json->String(table_name.ptr, (int)table_name.len);
    json->Key("id"); json->String(id);
    json->Key("sequence"); json->Int(sqlite3_column_int(*stmt, 1));

    json->Key("fragments"); json->StartArray();
    if (sqlite3_column_type(*stmt, 2) != SQLITE_NULL) {
        do {
            json->StartObject();

            json->Key("mtime"); json->String((const char *)sqlite3_column_text(*stmt, 2));
            json->Key("username"); json->String((const char *)sqlite3_column_text(*stmt, 3));
            if (sqlite3_column_type(*stmt, 4) != SQLITE_NULL) {
                json->Key("page"); json->String((const char *)sqlite3_column_text(*stmt, 4));
            } else {
                json->Key("page"); json->Null();
            }
            json->Key("complete"); json->Bool(sqlite3_column_int(*stmt, 5));
            json->Key("values"); json->Raw((const char *)sqlite3_column_text(*stmt, 6));
            json->Key("anchor"); json->Int64(sqlite3_column_int64(*stmt, 7));

            json->EndObject();
        } while (stmt->Next() && TestStr((const char *)sqlite3_column_text(*stmt, 0), id));
    } else {
        stmt->Next();
    }
    json->EndArray();

    json->EndObject();
}

void HandleRecordGet(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    if (!session) {
        LogError("User is not allowed to view data");
        io->AttachError(403);
        return;
    }

    Span<const char> table_name;
    Span<const char> id;
    {
        Span<const char> remain = request.url + 1;

        SplitStr(remain, '/', &remain);
        table_name = SplitStr(remain, '/', &remain);
        id = SplitStr(remain, '/', &remain);

        if (!table_name.len || remain.len) {
            LogError("URL must contain table and optional ID (and nothing more)");
            io->AttachError(422);
            return;
        }

        if (table_name == ".." || id == "..") {
            LogError("URL must not contain '..' components");
            io->AttachError(422);
            return;
        }
    }

    if (id.len) {
        sq_Statement stmt;
        if (!goupile_db.Prepare(R"(SELECT r.id, r.sequence, f.mtime, f.username, f.page, f.complete, f.json, f.anchor
                                   FROM rec_entries r
                                   LEFT JOIN rec_fragments f ON (f.table_name = r.table_name)
                                   WHERE r.table_name = ? AND r.id = ?)", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, table_name.ptr, (int)table_name.len, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, id.ptr, (int)id.len, SQLITE_STATIC);

        if (!stmt.Next()) {
            if (stmt.IsValid()) {
                LogError("Record does not exist");
                io->AttachError(404);
            }
            return;
        }

        // Export data
        http_JsonPageBuilder json(request.compression_type);

        ExportRecord(&stmt, table_name, &json);

        json.Finish(io);
    } else {
        sq_Statement stmt;
        if (!goupile_db.Prepare(R"(SELECT r.id, r.sequence, f.mtime, f.username, f.page, f.complete, f.json, f.anchor
                                   FROM rec_entries r
                                   LEFT JOIN rec_fragments f ON (f.table_name = r.table_name)
                                   WHERE r.table_name = ?
                                   ORDER BY r.id)", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, table_name.ptr, (int)table_name.len, SQLITE_STATIC);

        // Export data
        http_JsonPageBuilder json(request.compression_type);

        json.StartArray();
        if (stmt.Next()) {
            do {
                ExportRecord(&stmt, table_name, &json);
            } while (stmt.IsRow());
        }
        if (!stmt.IsValid())
            return;
        json.EndArray();

        json.Finish(io);
    }
}

void HandleRecordPut(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    // XXX: Check new/edit permissions correctly
    if (!session || !session->HasPermission(UserPermission::Edit)) {
        LogError("User is not allowed to push data");
        io->AttachError(403);
        return;
    }

    Span<const char> table_name;
    Span<const char> id;
    {
        Span<const char> remain = request.url + 1;

        SplitStr(remain, '/', &remain);
        table_name = SplitStr(remain, '/', &remain);
        id = SplitStr(remain, '/', &remain);

        if (!table_name.len || !id.len || remain.len) {
            LogError("URL must contain table and ID (and nothing more)");
            io->AttachError(422);
            return;
        }

        if (table_name == ".." || id == "..") {
            LogError("URL must not contain '..' components");
            io->AttachError(422);
            return;
        }
    }

    io->RunAsync([=]() {
        // Find appropriate port
        ScriptPort *port = LockPort();
        RG_DEFER { UnlockPort(port); };

        // Parse request body (JSON)
        ScriptHandle handle;
        {
            StreamReader st;
            if (!io->OpenForRead(&st))
                return;

            if (!port->ParseFragments(&st, &handle))
                return;
        }

        HeapArray<ScriptFragment> fragments;
        if (!port->RunRecord(handle, &fragments))
            return;

        bool success = goupile_db.Transaction([&]() {
            int sequence;
            {
                sq_Statement stmt;
                if (!goupile_db.Prepare(R"(SELECT sequence
                                           FROM rec_sequences
                                           WHERE table_name = ?)", &stmt))
                    return false;
                sqlite3_bind_text(stmt, 1, table_name.ptr, (int)table_name.len, SQLITE_STATIC);

                if (stmt.Next()) {
                    sequence = sqlite3_column_int(stmt, 0);
                } else if (stmt.IsValid()) {
                    sequence = 1;
                } else {
                    return false;
                }
            }

            // Update sequence number
            if (!goupile_db.Run(R"(INSERT INTO rec_sequences (table_name, sequence)
                                   VALUES (?, ?)
                                   ON CONFLICT(table_name) DO UPDATE SET sequence = excluded.sequence)",
                                table_name, sequence + 1))
                return false;

            // Save record entry
            if (!goupile_db.Run(R"(INSERT INTO rec_entries (table_name, id, sequence)
                                   VALUES (?, ?, ?)
                                   ON CONFLICT DO NOTHING)",
                                table_name, id, sequence))
                return false;
            if (!goupile_db.Run(R"(DELETE FROM rec_fragments WHERE id = ?)", id))
                return false;

            // Save record fragments (and variables)
            for (const ScriptFragment &frag: fragments) {
                if (!goupile_db.Run(R"(INSERT INTO rec_fragments (table_name, id, username, mtime, page, complete, json)
                                       VALUES (?, ?, ?, ?, ?, 0, ?))",
                                    table_name, id, session->username, frag.mtime, frag.page, frag.values))
                    return false;

                sq_Statement stmt;
                if (!goupile_db.Prepare(R"(INSERT INTO rec_columns (table_name, page, key, prop, before, after)
                                           VALUES (?, ?, ?, ?, ?, ?)
                                           ON CONFLICT(table_name, page, key, prop) DO UPDATE SET prop = excluded.prop,
                                                                                                  before = excluded.before,
                                                                                                  after = excluded.after)", &stmt))
                    return false;
                sqlite3_bind_text(stmt, 1, table_name.ptr, (int)table_name.len, SQLITE_STATIC);

                for (Size i = 0; i < frag.columns.len; i++) {
                    const ScriptFragment::Column &col = frag.columns[i];
                    const char *before = i ? frag.columns[i - 1].key : nullptr;
                    const char *after = (i + 1 < frag.columns.len) ? frag.columns[i + 1].key : nullptr;

                    stmt.Reset();
                    sqlite3_bind_text(stmt, 2, frag.page, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 3, col.key, -1, SQLITE_STATIC);
                    if (col.prop) {
                        sqlite3_bind_text(stmt, 4, col.prop, -1, SQLITE_STATIC);
                    } else {
                        sqlite3_bind_null(stmt, 4);
                    }
                    sqlite3_bind_text(stmt, 5, before, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 6, after, -1, SQLITE_STATIC);

                    if (!stmt.Run())
                        return false;
                }
            }

            return true;
        });
        if (!success)
            return;

        io->AttachText(200, "Done!");
    });
}

void HandleRecordColumns(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    if (!session) {
        LogError("User is not allowed to view data");
        io->AttachError(403);
        return;
    }

    const char *table_name = request.GetQueryValue("table");
    if (!table_name) {
        LogError("Missing 'table' parameter'");
        io->AttachError(422);
        return;
    }

    sq_Statement stmt;
    if (!goupile_db.Prepare(R"(SELECT page, key, prop, before, after
                               FROM rec_columns
                               WHERE table_name = ?)", &stmt))
        return;
    sqlite3_bind_text(stmt, 1, table_name, -1, SQLITE_STATIC);

    // Export data
    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    while (stmt.Next()) {
        json.StartObject();
        json.Key("page"); json.String((const char *)sqlite3_column_text(stmt, 0));
        json.Key("key"); json.String((const char *)sqlite3_column_text(stmt, 1));
        if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
            json.Key("prop"); json.Raw((const char *)sqlite3_column_text(stmt, 2));
        }
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            json.Key("before"); json.String((const char *)sqlite3_column_text(stmt, 3));
        } else {
            json.Key("before"); json.Null();
        }
        if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
            json.Key("after"); json.String((const char *)sqlite3_column_text(stmt, 4));
        } else {
            json.Key("after"); json.Null();
        }
        json.EndObject();
    }
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish(io);
}

}
