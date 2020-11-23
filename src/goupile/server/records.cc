// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "goupile.hh"
#include "instance.hh"
#include "js.hh"
#include "user.hh"
#include "../../core/libwrap/json.hh"

namespace RG {

static void ExportRecord(sq_Statement *stmt, json_Writer *json)
{
    int64_t rowid = sqlite3_column_int64(*stmt, 0);

    json->StartObject();

    json->Key("table"); json->String((const char *)sqlite3_column_text(*stmt, 1));
    json->Key("id"); json->String((const char *)sqlite3_column_text(*stmt, 2));
    json->Key("sequence"); json->Int(sqlite3_column_int(*stmt, 3));
    if (sqlite3_column_type(*stmt, 4) != SQLITE_NULL) {
        json->Key("zone"); json->String((const char *)sqlite3_column_text(*stmt, 4));
    } else {
        json->Key("zone"); json->Null();
    }

    json->Key("fragments"); json->StartArray();
    do {
        json->StartObject();

        json->Key("mtime"); json->String((const char *)sqlite3_column_text(*stmt, 5));
        json->Key("username"); json->String((const char *)sqlite3_column_text(*stmt, 6));
        json->Key("version"); json->Int64(sqlite3_column_int64(*stmt, 7));
        if (sqlite3_column_type(*stmt, 8) != SQLITE_NULL) {
            json->Key("page"); json->String((const char *)sqlite3_column_text(*stmt, 8));
            json->Key("complete"); json->Bool(sqlite3_column_int(*stmt, 9));
            json->Key("values"); json->Raw((const char *)sqlite3_column_text(*stmt, 10));
        } else {
            json->Key("page"); json->Null();
            json->Key("complete"); json->Bool(false);
            json->Key("values"); json->Raw("{}");
        }
        json->Key("anchor"); json->Int64(sqlite3_column_int64(*stmt, 11));

        json->EndObject();
    } while (stmt->Next() && sqlite3_column_int64(*stmt, 0) == rowid);
    json->EndArray();

    json->EndObject();
}

void HandleRecordLoad(InstanceData *instance, const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);
    const Token *token = session ? session->GetToken(instance) : nullptr;

    if (!token) {
        LogError("User is not allowed to view data");
        io->AttachError(403);
        return;
    }

    const char *table = request.GetQueryValue("table");
    const char *id = request.GetQueryValue("id");
    int64_t anchor = -1;
    {
        const char *anchor_str = request.GetQueryValue("anchor");
        if (anchor_str && !ParseInt(anchor_str, &anchor)) {
            io->AttachError(422);
            return;
        }
    }

    sq_Statement stmt;
    {
        LocalArray<char, 1024> sql;
        int bind_idx = 1;

        sql.len += Fmt(sql.TakeAvailable(), R"(SELECT r.rowid, r.store, r.id, r.sequence, r.zone, f.mtime, f.username,
                                                      f.version, f.page, f.complete, f.json, f.anchor FROM rec_entries r
                                               INNER JOIN rec_fragments f ON (f.store = r.store AND f.id = r.id)
                                               WHERE 1 = 1)").len;
        if (token->zone) {
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
        sql.len += Fmt(sql.TakeAvailable(), " ORDER BY r.rowid, f.anchor;").len;

        if (!instance->db.Prepare(sql.data, &stmt))
            return;

        if (token->zone) {
            sqlite3_bind_text(stmt, bind_idx++, token->zone, -1, SQLITE_STATIC);
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

void HandleRecordColumns(InstanceData *instance, const http_RequestInfo &request, http_IO *io)
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
        if (anchor_str && !ParseInt(anchor_str, &anchor)) {
            io->AttachError(422);
            return;
        }
    }

    sq_Statement stmt;
    if (table) {
        if (!instance->db.Prepare(R"(SELECT store, page, variable, type, prop, before, after FROM rec_columns
                                     WHERE store = ? AND anchor >= ?)", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, table, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, anchor);
    } else {
        if (!instance->db.Prepare(R"(SELECT store, page, variable, type, prop, before, after FROM rec_columns
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

void HandleRecordSync(InstanceData *instance, const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);
    const Token *token = session ? session->GetToken(instance) : nullptr;

    // XXX: Check new/edit permissions correctly
    if (!token || !token->HasPermission(UserPermission::Edit)) {
        LogError("User is not allowed to sync data");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Find appropriate port
        ScriptPort *port = LockScriptPort();
        RG_DEFER { port->Unlock(); };

        port->Setup(instance, *session, *token);

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

        bool incomplete = false;

        for (const ScriptRecord &handle: handles) {
            // Get existing record data
            sq_Statement stmt;
            int version;
            Span<const char> json;
            {
                if (!instance->db.Prepare(R"(SELECT zone, version, json FROM rec_entries
                                             WHERE store = ? AND id = ?)", &stmt))
                    return;
                sqlite3_bind_text(stmt, 1, handle.table, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, handle.id, -1, SQLITE_STATIC);

                if (stmt.Next()) {
                    if (token->zone && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                        const char *zone = (const char *)sqlite3_column_text(stmt, 0);
                        if (!TestStr(token->zone, zone)) {
                            LogError("Zone mismatch for %1", handle.id);
                            incomplete = true;
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
                incomplete = true;
                continue;
            }

            sq_TransactionResult ret = instance->db.Transaction([&]() {
                // Get sequence number
                int sequence;
                {
                    sq_Statement stmt;
                    if (!instance->db.Prepare(R"(SELECT sequence FROM rec_sequences
                                                 WHERE store = ?)", &stmt))
                        return sq_TransactionResult::Error;
                    sqlite3_bind_text(stmt, 1, handle.table, -1, SQLITE_STATIC);

                    if (stmt.Next()) {
                        sequence = sqlite3_column_int(stmt, 0);
                    } else if (stmt.IsValid()) {
                        sequence = 1;
                    } else {
                        return sq_TransactionResult::Error;
                    }
                }

                // Insert new entry
                if (!instance->db.Run(R"(INSERT INTO rec_entries (store, id, zone, sequence, version, json)
                                         VALUES (?, ?, ?, ?, ?, ?)
                                         ON CONFLICT DO NOTHING)",
                                    handle.table, handle.id, handle.zone ? sq_Binding(handle.zone) : sq_Binding(),
                                    sequence, fragments[fragments.len - 1].version, json))
                    return sq_TransactionResult::Error;

                // Update sequence number of existing entry depending on result
                if (sqlite3_changes(instance->db)) {
                    if (!instance->db.Run(R"(INSERT INTO rec_sequences (store, sequence)
                                             VALUES (?, ?)
                                             ON CONFLICT(store)
                                                 DO UPDATE SET sequence = excluded.sequence)",
                                        handle.table, sequence + 1))
                        return sq_TransactionResult::Error;
                } else {
                    if (!instance->db.Run(R"(UPDATE rec_entries SET version = ?, json = ?
                                             WHERE store = ? AND id = ?)",
                                        fragments[fragments.len - 1].version, json, handle.table, handle.id))
                        return sq_TransactionResult::Error;
                }

                // Save record fragments (and variables)
                for (Size i = 0; i < fragments.len; i++) {
                    const ScriptFragment &frag = fragments[i];

                    // XXX: Silently skipping already stored fragments for now
                    if (frag.version <= version) {
                        LogError("Ignored conflicting fragment %1 for %2", frag.version, handle.id);
                        incomplete = true;
                        continue;
                    }
                    if (frag.complete && !token->HasPermission(UserPermission::Validate)) {
                        LogError("User is not allowed to validate records");
                        incomplete = true;
                        return sq_TransactionResult::Rollback;
                    }

                    int64_t anchor;
                    if (!instance->db.Run(R"(INSERT INTO rec_fragments (store, id, version, page,
                                                                      username, mtime, complete, json)
                                             VALUES (?, ?, ?, ?, ?, ?, ?, ?))",
                                        handle.table, handle.id, frag.version,
                                        frag.page ? sq_Binding(frag.page) : sq_Binding(), session->username,
                                        frag.mtime, 0 + frag.complete, frag.json))
                        return sq_TransactionResult::Error;
                    anchor = sqlite3_last_insert_rowid(instance->db);

                    sq_Statement stmt;
                    if (!instance->db.Prepare(R"(INSERT INTO rec_columns (key, store, page, variable,
                                                                        type, prop, before, after, anchor)
                                                 VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                                                 ON CONFLICT(key)
                                                     DO UPDATE SET before = excluded.before,
                                                                   after = excluded.after,
                                                                   anchor = excluded.anchor)", &stmt))
                        return sq_TransactionResult::Error;
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
                            return sq_TransactionResult::Error;
                    }
                }

                return sq_TransactionResult::Commit;
            });

            if (ret == sq_TransactionResult::Error)
                return;
        }

        if (incomplete) {
            io->AttachText(409, "Done (with errors)!");
        } else {
            io->AttachText(200, "Done!");
        }
    });
}

}
