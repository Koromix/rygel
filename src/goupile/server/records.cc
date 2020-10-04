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

static void ExportRecord(sq_Statement *stmt, json_Writer *json)
{
    char id[512];
    strncpy(id, (const char *)sqlite3_column_text(*stmt, 1), RG_SIZE(id));
    id[RG_SIZE(id) - 1] = 0;

    json->StartObject();

    json->Key("table"); json->String((const char *)sqlite3_column_text(*stmt, 0));
    json->Key("id"); json->String(id);
    json->Key("sequence"); json->Int(sqlite3_column_int(*stmt, 2));
    if (sqlite3_column_type(*stmt, 3) != SQLITE_NULL) {
        json->Key("zone"); json->String((const char *)sqlite3_column_text(*stmt, 3));
    } else {
        json->Key("zone"); json->Null();
    }

    json->Key("fragments"); json->StartArray();
    do {
        json->StartObject();

        json->Key("mtime"); json->String((const char *)sqlite3_column_text(*stmt, 4));
        json->Key("username"); json->String((const char *)sqlite3_column_text(*stmt, 5));
        json->Key("version"); json->Int64(sqlite3_column_int64(*stmt, 6));
        if (sqlite3_column_type(*stmt, 7) != SQLITE_NULL) {
            json->Key("page"); json->String((const char *)sqlite3_column_text(*stmt, 7));
            json->Key("complete"); json->Bool(sqlite3_column_int(*stmt, 8));
            json->Key("values"); json->Raw((const char *)sqlite3_column_text(*stmt, 9));
        } else {
            json->Key("page"); json->Null();
            json->Key("complete"); json->Bool(false);
            json->Key("values"); json->Raw("{}");
        }
        json->Key("anchor"); json->Int64(sqlite3_column_int64(*stmt, 10));

        json->EndObject();
    } while (stmt->Next() && TestStr((const char *)sqlite3_column_text(*stmt, 1), id));
    json->EndArray();

    json->EndObject();
}

void HandleRecordLoad(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    if (!session) {
        LogError("User is not allowed to view data");
        io->AttachError(403);
        return;
    }

    const char *table = request.GetQueryValue("table");
    const char *id = request.GetQueryValue("id");
    int64_t anchor = -1;
    {
        const char *anchor_str = request.GetQueryValue("anchor");
        if (anchor_str && !ParseDec(anchor_str, &anchor)) {
            io->AttachError(422);
            return;
        }
    }

    sq_Statement stmt;
    {
        LocalArray<char, 1024> sql;
        int bind_idx = 1;

        sql.len += Fmt(sql.TakeAvailable(), R"(SELECT r.store, r.id, r.sequence, r.zone, f.mtime, f.username, f.version,
                                                      f.page, f.complete, f.json, f.anchor FROM rec_entries r
                                               INNER JOIN rec_fragments f ON (f.id = r.id)
                                               WHERE 1 = 1)").len;
        if (session->zone) {
            sql.len += Fmt(sql.TakeAvailable(), " AND (r.zone IS NULL OR r.zone == ?)").len;
        }
        if (table) {
            sql.len += Fmt(sql.TakeAvailable(), " AND r.store = ?").len;
        }
        if (id) {
            sql.len += Fmt(sql.TakeAvailable(), " AND r.id = ?").len;
        }
        if (anchor) {
            sql.len += Fmt(sql.TakeAvailable(), " AND f.anchor >= ?").len;
        }

        if (!goupile_db.Prepare(sql.data, &stmt))
            return;

        if (session->zone) {
            sqlite3_bind_text(stmt, bind_idx++, session->zone, -1, SQLITE_STATIC);
        }
        if (table) {
            sqlite3_bind_text(stmt, bind_idx++, table, -1, SQLITE_STATIC);
        }
        if (id) {
            sqlite3_bind_text(stmt, bind_idx++, id, -1, SQLITE_STATIC);
        }
        if (anchor) {
            sqlite3_bind_int64(stmt, bind_idx++, anchor);
        }
    }

    // Export data
    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    if (stmt.Next()) {
        do {
            ExportRecord(&stmt, &json);
        } while (stmt.IsRow());
    }
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish(io);
}

void HandleRecordColumns(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    if (!session) {
        LogError("User is not allowed to view data");
        io->AttachError(403);
        return;
    }

    const char *table = request.GetQueryValue("table");
    int64_t anchor = -1;
    {
        const char *anchor_str = request.GetQueryValue("anchor");
        if (anchor_str && !ParseDec(anchor_str, &anchor)) {
            io->AttachError(422);
            return;
        }
    }

    sq_Statement stmt;
    if (table) {
        if (!goupile_db.Prepare(R"(SELECT store, page, variable, type, prop, before, after FROM rec_columns
                                   WHERE store = ? AND anchor >= ?)", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, table, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, anchor);
    } else {
        if (!goupile_db.Prepare(R"(SELECT store, page, variable, type, prop, before, after FROM rec_columns
                                   WHERE anchor >= ?)", &stmt))
            return;
        sqlite3_bind_int64(stmt, 1, anchor);
    }

    // Export data
    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    while (stmt.Next()) {
        json.StartObject();
        json.Key("table"); json.String((const char *)sqlite3_column_text(stmt, 0));
        json.Key("page"); json.String((const char *)sqlite3_column_text(stmt, 1));
        json.Key("variable"); json.String((const char *)sqlite3_column_text(stmt, 2));
        json.Key("type"); json.String((const char *)sqlite3_column_text(stmt, 3));
        if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
            json.Key("prop"); json.Raw((const char *)sqlite3_column_text(stmt, 4));
        }
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
            json.Key("before"); json.String((const char *)sqlite3_column_text(stmt, 5));
        } else {
            json.Key("before"); json.Null();
        }
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
            json.Key("after"); json.String((const char *)sqlite3_column_text(stmt, 6));
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

void HandleRecordSync(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    // XXX: Check new/edit permissions correctly
    if (!session || !session->HasPermission(UserPermission::Edit)) {
        LogError("User is not allowed to sync data");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Find appropriate port
        ScriptPort *port = LockPort();
        RG_DEFER { UnlockPort(port); };

        port->ChangeProfile(*session);

        // Parse request body (JSON)
        HeapArray<ScriptRecord> handles;
        {
            StreamReader st;
            if (!io->OpenForRead(&st))
                return;

            if (!port->ParseFragments(&st, &handles)) {
                io->AttachError(422);
                return;
            }
        }

        bool conflict = false;

        for (const ScriptRecord &handle: handles) {
            // Get existing record data
            sq_Statement stmt;
            int version;
            Span<const char> json;
            {
                if (!goupile_db.Prepare(R"(SELECT zone, version, json FROM rec_entries
                                           WHERE store = ? AND id = ?)", &stmt))
                    return;
                sqlite3_bind_text(stmt, 1, handle.table, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, handle.id, -1, SQLITE_STATIC);

                if (stmt.Next()) {
                    if (session->zone && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                        const char *zone = (const char *)sqlite3_column_text(stmt, 0);
                        if (!TestStr(session->zone, zone)) {
                            LogError("Zone mismatch for %1", handle.id);
                            conflict = true;
                            continue;
                        }
                    }

                    version = sqlite3_column_int(stmt, 1);
                    json.ptr = (const char *)sqlite3_column_text(stmt, 2);
                    json.len = (Size)sqlite3_column_bytes(stmt, 2);
                } else if (stmt.IsValid()) {
                    version = -1;
                    json = "{}";
                } else {
                    return;
                }
            }

            // Run JS validation
            HeapArray<ScriptFragment> fragments;
            if (!port->RunRecord(json, handle, &fragments, &json)) {
                conflict = true;
                continue;
            }

            bool success = goupile_db.Transaction([&]() {
                // Get sequence number
                int sequence;
                {
                    sq_Statement stmt;
                    if (!goupile_db.Prepare(R"(SELECT sequence FROM rec_sequences
                                               WHERE store = ?)", &stmt))
                        return false;
                    sqlite3_bind_text(stmt, 1, handle.table, -1, SQLITE_STATIC);

                    if (stmt.Next()) {
                        sequence = sqlite3_column_int(stmt, 0);
                    } else if (stmt.IsValid()) {
                        sequence = 1;
                    } else {
                        return false;
                    }
                }

                // Insert new entry
                if (!goupile_db.Run(R"(INSERT INTO rec_entries (store, id, zone, sequence, version, json)
                                       VALUES (?, ?, ?, ?, ?, ?)
                                       ON CONFLICT DO NOTHING)",
                                    handle.table, handle.id, handle.zone ? sq_Binding(handle.zone) : sq_Binding(),
                                    sequence, fragments[fragments.len - 1].version, json))
                    return false;

                // Update sequence number of existing entry depending on result
                if (sqlite3_changes(goupile_db)) {
                    if (!goupile_db.Run(R"(INSERT INTO rec_sequences (store, sequence)
                                           VALUES (?, ?)
                                           ON CONFLICT(store)
                                                DO UPDATE SET sequence = excluded.sequence)",
                                        handle.table, sequence + 1))
                        return false;
                } else {
                    if (!goupile_db.Run(R"(UPDATE rec_entries SET version = ?, json = ?
                                           WHERE store = ? AND id = ?)",
                                        fragments[fragments.len - 1].version, json, handle.table, handle.id))
                        return false;
                }

                // Save record fragments (and variables)
                for (Size i = 0; i < fragments.len; i++) {
                    const ScriptFragment &frag = fragments[i];

                    // XXX: Silently skipping already stored fragments for now
                    if (frag.version <= version) {
                        LogError("Ignored conflicting fragment %1 for %2", frag.version, handle.id);
                        conflict = true;
                        continue;
                    }

                    int64_t anchor;
                    if (!goupile_db.Run(R"(INSERT INTO rec_fragments (store, id, version, page,
                                                                      username, mtime, complete, json)
                                           VALUES (?, ?, ?, ?, ?, ?, 0, ?))",
                                        handle.table, handle.id, frag.version,
                                        frag.page ? sq_Binding(frag.page) : sq_Binding(), session->username,
                                        frag.mtime, frag.json))
                        return false;
                    anchor = sqlite3_last_insert_rowid(goupile_db);

                    sq_Statement stmt;
                    if (!goupile_db.Prepare(R"(INSERT INTO rec_columns (key, store, page, variable,
                                                                        type, prop, before, after, anchor)
                                               VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                                               ON CONFLICT(key)
                                                    DO UPDATE SET before = excluded.before,
                                                                  after = excluded.after,
                                                                  anchor = excluded.anchor)", &stmt))
                        return false;
                    sqlite3_bind_text(stmt, 2, handle.table, -1, SQLITE_STATIC);
                    sqlite3_bind_int64(stmt, 9, anchor);

                    for (Size j = 0; j < frag.columns.len; j++) {
                        const ScriptFragment::Column &col = frag.columns[j];
                        const char *before = j ? frag.columns[j - 1].key : nullptr;
                        const char *after = (j + 1 < frag.columns.len) ? frag.columns[j + 1].key : nullptr;

                        stmt.Reset();
                        sqlite3_bind_text(stmt, 1, col.key, -1, SQLITE_STATIC);
                        sqlite3_bind_text(stmt, 3, frag.page, -1, SQLITE_STATIC);
                        sqlite3_bind_text(stmt, 4, col.variable, -1, SQLITE_STATIC);
                        sqlite3_bind_text(stmt, 5, col.type, -1, SQLITE_STATIC);
                        if (col.prop) {
                            sqlite3_bind_text(stmt, 6, col.prop, -1, SQLITE_STATIC);
                        } else {
                            sqlite3_bind_null(stmt, 6);
                        }
                        sqlite3_bind_text(stmt, 7, before, -1, SQLITE_STATIC);
                        sqlite3_bind_text(stmt, 8, after, -1, SQLITE_STATIC);

                        if (!stmt.Run())
                            return false;
                    }
                }

                return true;
            });
            if (!success)
                return;
        }

        if (conflict) {
            io->AttachText(409, "Done (with errors)!");
        } else {
            io->AttachText(200, "Done!");
        }
    });
}

}
