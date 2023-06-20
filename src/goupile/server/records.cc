// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/libcc/libcc.hh"
#include "domain.hh"
#include "goupile.hh"
#include "instance.hh"
#include "user.hh"
#include "src/core/libwrap/json.hh"
#ifdef _WIN32
    #include <io.h>
#else
    #include <unistd.h>
#endif

namespace RG {

struct RecordFragment {
    int64_t fs;
    char eid[27];
    const char *store;
    int64_t mtime;
    bool has_data;
    Span<const char> data;
};

static bool PrepareRecordSelect(InstanceHolder *instance, int64_t userid, const SessionStamp &stamp,
                                const char *tid, int64_t anchor, sq_Statement *out_stmt)
{
    if (anchor < 0) {
        LocalArray<char, 2048> sql;

        sql.len += Fmt(sql.TakeAvailable(), R"(SELECT t.rowid AS t, t.tid,
                                                      e.rowid AS e, e.eid, e.deleted, e.anchor, e.ctime, e.mtime,
                                                      e.store, e.sequence, IIF(?1 IS NOT NULL, e.data, NULL) AS data
                                               FROM rec_threads t
                                               INNER JOIN rec_entries e ON (e.tid = t.tid)
                                               WHERE 1+1)").len;
        if (tid) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.tid = ?1").len;
        }
        if (!stamp.HasPermission(UserPermission::DataAudit)) {
            sql.len += Fmt(sql.TakeAvailable(), " AND e.deleted = 0").len;
        }
        if (!stamp.HasPermission(UserPermission::DataLoad)) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.tid IN (SELECT tid FROM ins_claims WHERE userid = ?2)").len;
        }
        sql.len += Fmt(sql.TakeAvailable(), " ORDER BY t.rowid, e.store").len;

        if (!instance->db->Prepare(sql.data, out_stmt))
            return false;
        sqlite3_bind_text(*out_stmt, 1, tid, -1, SQLITE_STATIC);
        sqlite3_bind_int64(*out_stmt, 2, userid);
    } else {
        RG_ASSERT(stamp.HasPermission(UserPermission::DataLoad));
        RG_ASSERT(stamp.HasPermission(UserPermission::DataAudit));

        if (!instance->db->Prepare(R"(WITH RECURSIVE rec (idx, eid, anchor, mtime, data) AS (
                                          SELECT 1, eid, anchor, mtime, data
                                              FROM rec_fragments
                                              WHERE (tid = ?1 OR ?1 IS NULL) AND
                                                    anchor <= ?2 AND previous IS NULL AND
                                                    data IS NOT NULL
                                          UNION ALL
                                          SELECT rec.idx + 1, f.eid, f.anchor, f.mtime,
                                              IIF(?1 IS NOT NULL, json_patch(rec.data, f.data), NULL) AS data
                                              FROM rec_fragments f, rec
                                              WHERE f.anchor <= ?2 AND f.previous = rec.anchor AND
                                                                       f.data IS NOT NULL
                                          ORDER BY anchor
                                      )
                                      SELECT t.rowid AS t, t.tid,
                                             e.rowid AS e, e.eid, e.deleted, rec.anchor, e.ctime, rec.mtime,
                                             e.store, e.sequence, rec.data
                                          FROM rec
                                          INNER JOIN rec_entries e ON (e.eid = rec.eid)
                                          INNER JOIN rec_threads t ON (t.tid = e.tid)
                                          ORDER BY t.rowid, e.store, rec.idx DESC)", out_stmt))
            return false;
        sqlite3_bind_text(*out_stmt, 1, tid, -1, SQLITE_STATIC);
        sqlite3_bind_int64(*out_stmt, 2, anchor);
    }

    return true;
}

void HandleRecordList(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    if (instance->config.sync_mode == SyncMode::Offline) {
        LogError("Records API is disabled in Offline mode");
        io->AttachError(403);
        return;
    }

    RetainPtr<const SessionInfo> session = GetNormalSession(instance, request, io);
    const SessionStamp *stamp = session ? session->GetStamp(instance) : nullptr;

    if (!session) {
        LogError("User is not logged in");
        io->AttachError(401);
        return;
    }
    if (!stamp || !stamp->HasPermission(UserPermission::DataLoad)) {
        LogError("User is not allowed to list data");
        io->AttachError(403);
        return;
    }

    int64_t anchor = -1;
    if (const char *str = request.GetQueryValue("anchor"); str) {
        if (!stamp->HasPermission(UserPermission::DataAudit)) {
            LogError("User is not allowed to access historical data");
            io->AttachError(403);
            return;
        }

        if (!ParseInt(str, &anchor)) {
            io->AttachError(422);
            return;
        }
        if (anchor <= 0) {
            LogError("Anchor must be a positive number");
            io->AttachError(422);
            return;
        }
    }

    sq_Statement stmt;
    if (!PrepareRecordSelect(instance, session->userid, *stamp, nullptr, anchor, &stmt))
        return;

    // Export data
    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartArray();
    if (stmt.Step()) {
        do {
            int64_t t = sqlite3_column_int64(stmt, 0);
            int64_t prev_e = -1;

            json.StartObject();

            json.Key("tid"); json.String((const char *)sqlite3_column_text(stmt, 1));

            json.Key("entries"); json.StartObject();
            do {
                int64_t e = sqlite3_column_int64(stmt, 2);
                const char *store = (const char *)sqlite3_column_text(stmt, 8);

                // This can happen when the recursive CTE is used for historical data
                if (e == prev_e)
                    continue;
                prev_e = e;

                json.Key(store); json.StartObject();
                    json.Key("eid"); json.String((const char *)sqlite3_column_text(stmt, 3));
                    if (stamp->HasPermission(UserPermission::DataAudit)) {
                        json.Key("deleted"); json.Bool(sqlite3_column_int(stmt, 4));
                    } else {
                        RG_ASSERT(!sqlite3_column_int(stmt, 4));
                    }
                    json.Key("ctime"); json.Int64(sqlite3_column_int64(stmt, 6));
                    json.Key("mtime"); json.Int64(sqlite3_column_int64(stmt, 7));
                    json.Key("sequence"); json.Int64(sqlite3_column_int64(stmt, 9));
                json.EndObject();
            } while (stmt.Step() && sqlite3_column_int64(stmt, 0) == t);
            json.EndObject();

            json.EndObject();
        } while (stmt.IsRow());
    }
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish();
}

void HandleRecordGet(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    if (instance->config.sync_mode == SyncMode::Offline) {
        LogError("Records API is disabled in Offline mode");
        io->AttachError(403);
        return;
    }

    RetainPtr<const SessionInfo> session = GetNormalSession(instance, request, io);
    const SessionStamp *stamp = session ? session->GetStamp(instance) : nullptr;

    if (!session) {
        LogError("User is not logged in");
        io->AttachError(401);
        return;
    }
    if (!stamp) {
        LogError("User is not allowed to load data");
        io->AttachError(403);
        return;
    }

    const char *tid = nullptr;
    int64_t anchor = -1;
    {
        tid = request.GetQueryValue("tid");
        if (!tid) {
            LogError("Missing 'tid' parameter");
            io->AttachError(422);
            return;
        }

        if (const char *str = request.GetQueryValue("anchor"); str) {
            if (!stamp->HasPermission(UserPermission::DataLoad) ||
                    !stamp->HasPermission(UserPermission::DataAudit)) {
                LogError("User is not allowed to access historical data");
                io->AttachError(403);
                return;
            }

            if (!ParseInt(str, &anchor)) {
                io->AttachError(422);
                return;
            }
            if (anchor <= 0) {
                LogError("Anchor must be a positive number");
                io->AttachError(422);
                return;
            }
        }
    }

    sq_Statement stmt;
    if (!PrepareRecordSelect(instance, session->userid, *stamp, tid, anchor, &stmt))
        return;

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("Thread '%1' does not exist", tid);
            io->AttachError(404);
        }
        return;
    }

    // Export data
    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartObject();
    {
        int64_t prev_e = -1;

        json.Key("tid"); json.String(tid);

        json.Key("entries"); json.StartObject();
        do {
            int64_t e = sqlite3_column_int64(stmt, 2);
            const char *store = (const char *)sqlite3_column_text(stmt, 8);

            // This can happen with the recursive CTE is used for historical data
            if (e == prev_e)
                continue;
            prev_e = e;

            json.Key(store); json.StartObject();

            json.Key("eid"); json.String((const char *)sqlite3_column_text(stmt, 3));
            if (stamp->HasPermission(UserPermission::DataAudit)) {
                json.Key("deleted"); json.Bool(sqlite3_column_int(stmt, 4));
            } else {
                RG_ASSERT(!sqlite3_column_int(stmt, 4));
            }
            json.Key("ctime"); json.Int64(sqlite3_column_int64(stmt, 6));
            json.Key("mtime"); json.Int64(sqlite3_column_int64(stmt, 7));
            json.Key("sequence"); json.Int64(sqlite3_column_int64(stmt, 9));
            json.Key("data"); json.Raw((const char *)sqlite3_column_text(stmt, 10));

            json.EndObject();
        } while (stmt.Step());
        json.EndObject();
    }
    if (!stmt.IsValid())
        return;
    json.EndObject();

    json.Finish();
}

void HandleRecordAudit(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    if (instance->config.sync_mode == SyncMode::Offline) {
        LogError("Records API is disabled in Offline mode");
        io->AttachError(403);
        return;
    }

    RetainPtr<const SessionInfo> session = GetNormalSession(instance, request, io);

    if (!session) {
        LogError("User is not logged in");
        io->AttachError(401);
        return;
    }
    if (!session->HasPermission(instance, UserPermission::DataAudit)) {
        LogError("User is not allowed to audit data");
        io->AttachError(403);
        return;
    }

    const char *tid = request.GetQueryValue("tid");
    if (!tid) {
        LogError("Missing 'tid' parameter");
        io->AttachError(422);
        return;
    }

    sq_Statement stmt;
    if (!instance->db->Prepare(R"(SELECT f.anchor, f.eid, e.store, IIF(f.data IS NOT NULL, 'save', 'delete') AS type,
                                         f.userid, f.username
                                  FROM rec_threads t
                                  INNER JOIN rec_fragments f ON (f.tid = t.tid)
                                  INNER JOIN rec_entries e ON (e.eid = f.eid)
                                  WHERE t.tid = ?1
                                  ORDER BY f.anchor)", &stmt))
        return;
    sqlite3_bind_text(stmt, 1, tid, -1, SQLITE_STATIC);

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("Thread '%1' does not exist", tid);
            io->AttachError(404);
        }
        return;
    }

    // Export data
    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartArray();
    do {
        json.StartObject();

        json.Key("anchor"); json.Int64(sqlite3_column_int64(stmt, 0));
        json.Key("eid"); json.String((const char *)sqlite3_column_text(stmt, 1));
        json.Key("store"); json.String((const char *)sqlite3_column_text(stmt, 2));
        json.Key("type"); json.String((const char *)sqlite3_column_text(stmt, 3));
        json.Key("userid"); json.Int64(sqlite3_column_int64(stmt, 4));
        json.Key("username"); json.String((const char *)sqlite3_column_text(stmt, 5));

        json.EndObject();
    } while (stmt.Step());
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish();
}

static bool CheckULID(Span<const char> str)
{
    const auto test_char = [](char c) { return IsAsciiDigit(c) || (c >= 'A' && c <= 'Z'); };

    if (str.len != 26 || !std::all_of(str.begin(), str.end(), test_char)) {
        LogError("Malformed ULID value '%1'", str);
        return false;
    }

    return true;
}

void HandleRecordSave(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    if (instance->config.sync_mode == SyncMode::Offline) {
        LogError("Records API is disabled in Offline mode");
        io->AttachError(403);
        return;
    }

    RetainPtr<const SessionInfo> session = GetNormalSession(instance, request, io);
    const SessionStamp *stamp = session ? session->GetStamp(instance) : nullptr;

    if (!session) {
        LogError("User is not logged in");
        io->AttachError(401);
        return;
    }
    if (!stamp || (!stamp->HasPermission(UserPermission::DataNew) &&
                   !stamp->HasPermission(UserPermission::DataEdit) &&
                   !stamp->HasPermission(UserPermission::DataDelete))) {
        LogError("User is not allowed to save data");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        char tid[27];
        int64_t base = -1;
        HeapArray<RecordFragment> fragments;
        bool has_deletes = false;
        {
            StreamReader st;
            if (!io->OpenForRead(Kibibytes(64), &st))
                return;
            json_Parser parser(&st, &io->allocator);

            parser.ParseObject();
            while (parser.InObject()) {
                Span<const char> key = {};
                parser.ParseKey(&key);

                if (key == "tid") {
                    Span<const char> str = nullptr;

                    if (parser.ParseString(&str)) {
                        if (!CheckULID(str)) {
                            io->AttachError(422);
                            return;
                        }

                        CopyString(str, tid);
                    }
                } else if (key == "anchor") {
                    parser.SkipNull() || parser.ParseInt(&base);
                    base = std::max(base, (int64_t)-1);
                } else if (key == "fragments") {
                    parser.ParseArray();
                    while (parser.InArray()) {
                        RecordFragment *frag = fragments.AppendDefault();
                        frag->fs = -1;

                        parser.ParseObject();
                        while (parser.InObject()) {
                            Span<const char> key = {};
                            parser.ParseKey(&key);

                            if (key == "fs") {
                                parser.ParseInt(&frag->fs);
                            } else if (key == "eid") {
                                Span<const char> str = nullptr;

                                if (parser.ParseString(&str)) {
                                    if (!CheckULID(str)) {
                                        io->AttachError(422);
                                        return;
                                    }

                                    CopyString(str, frag->eid);
                                }
                            } else if (key == "store") {
                                parser.ParseString(&frag->store);
                            } else if (key == "mtime") {
                                parser.ParseInt(&frag->mtime);
                            } else if (key == "data") {
                                switch (parser.PeekToken()) {
                                    case json_TokenType::Null: {
                                        frag->data = {};
                                        frag->has_data = true;
                                    } break;
                                    case json_TokenType::StartObject: {
                                        parser.PassThrough(&frag->data);
                                        frag->has_data = true;
                                    } break;

                                    default: {
                                        LogError("Unexpected value type for fragment data");
                                        io->AttachError(422);
                                        return;
                                    } break;
                                }
                            } else if (parser.IsValid()) {
                                LogError("Unexpected key '%1'", key);
                                io->AttachError(422);
                                return;
                            }
                        }

                        has_deletes |= !frag->has_data;
                    }
                } else if (parser.IsValid()) {
                    LogError("Unexpected key '%1'", key);
                    io->AttachError(422);
                    return;
                }
            }
            if (!parser.IsValid()) {
                io->AttachError(422);
                return;
            }
        }

        // Check missing or invalid values
        {
            bool valid = true;

            if (!tid[0]) {
                LogError("Missing or empty 'tid' value");
                valid = false;
            }

            if (!fragments.len) {
                LogError("Missing data fragments");
                valid = false;
            }
            for (const RecordFragment &frag: fragments) {
                if (frag.fs < 0 || !frag.eid[0] || !frag.store || !frag.mtime || !frag.has_data) {
                    LogError("Missing fragment values");
                    valid = false;
                    break;
                }
            }

            if (!valid) {
                io->AttachError(422);
                return;
            }
        }

        bool success = instance->db->Transaction([&]() {
            // Get existing record
            int64_t anchor;
            {
                sq_Statement stmt;
                if (!PrepareRecordSelect(instance, session->userid, *stamp, tid, -1, &stmt))
                    return false;

                if (stmt.Step()) {
                    anchor = sqlite3_column_int64(stmt, 5);
                } else if (stmt.IsValid()) {
                    anchor = -1;
                } else {
                    return false;
                }
            }

            // Check permissions
            if (base != anchor) {
                LogError("Thread version mismatch");
                io->AttachError(409);
                return false;
            }
            if (anchor < 0) {
                if (!stamp->HasPermission(UserPermission::DataNew)) {
                    LogError("You are not allowed to create new records");
                    io->AttachError(403);
                    return false;
                }
            } else {
                if (!stamp->HasPermission(UserPermission::DataEdit)) {
                    LogError("You are not allowed to edit records");
                    io->AttachError(403);
                    return false;
                }
            }
            if (has_deletes && !stamp->HasPermission(UserPermission::DataDelete)) {
                LogError("You are not allowed to delete records");
                io->AttachError(403);
                return false;
            }

            // Write new data fragments
            {
                HashMap<const char *, int64_t> anchors;

                for (const RecordFragment &frag: fragments) {
                    int64_t *anchor_ptr = anchors.Find(frag.store);

                    // Get current record entry anchor
                    if (!anchor_ptr) {
                        anchor_ptr = anchors.Set(frag.store, 0);

                        sq_Statement stmt;
                        if (!instance->db->Prepare("SELECT store, anchor FROM rec_entries WHERE eid = ?1", &stmt))
                            return false;
                        sqlite3_bind_text(stmt, 1, frag.eid, -1, SQLITE_STATIC);

                        if (stmt.Step()) {
                            const char *store = (const char *)sqlite3_column_text(stmt, 0);
                            int64_t anchor = sqlite3_column_int64(stmt, 1);

                            if (!TestStr(store, frag.store)) {
                                LogError("Record entry store mismatch");
                                io->AttachError(409);
                                return false;
                            }

                            *anchor_ptr = anchor;
                        } else if (!stmt.IsValid()) {
                            return false;
                        }
                    }

                    // Insert data fragment
                    int64_t new_anchor;
                    {
                        sq_Statement stmt;
                        if (!instance->db->Prepare(R"(INSERT INTO rec_fragments (previous, tid, eid, userid, username, mtime, fs, data)
                                                      VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8))",
                                                      &stmt, *anchor_ptr > 0 ? sq_Binding(*anchor_ptr) : sq_Binding(), tid,
                                                      frag.eid, session->userid, session->username, frag.mtime, frag.fs, frag.data))
                            return false;
                        if (!stmt.GetSingleValue(&new_anchor))
                            return false;
                    }

                    // Create or update store entry
                    int64_t e;
                    {
                        sq_Statement stmt;
                        if (!instance->db->Prepare(R"(INSERT INTO rec_entries (tid, eid, anchor, ctime, mtime,
                                                                               store, sequence, deleted, data)
                                                      VALUES (?1, ?2, ?3, ?4, ?4, ?5, -1, ?6, ?7)
                                                      ON CONFLICT DO UPDATE SET anchor = excluded.anchor,
                                                                                mtime = excluded.mtime,
                                                                                deleted = excluded.deleted,
                                                                                data = json_patch(data, excluded.data))",
                                                   &stmt, tid, frag.eid, new_anchor, frag.mtime, frag.store,
                                                   0 + !frag.data.len, frag.data))
                            return false;
                        if (!stmt.GetSingleValue(&e))
                            return false;
                    }

                    // Deal with per-store sequence number
                    if (!anchor_ptr) {
                        int64_t counter;
                        {
                            sq_Statement stmt;
                            if (!instance->db->Prepare(R"(INSERT INTO seq_counters (type, key, counter)
                                                          VALUES ('record', ?1, 1)
                                                          ON CONFLICT (type, key) DO UPDATE SET counter = counter + 1
                                                          RETURNING counter)",
                                                       &stmt, frag.store))
                                return false;
                            if (!stmt.GetSingleValue(&counter))
                                return false;
                        }

                        if (!instance->db->Run("UPDATE rec_entries SET sequence = ?2 WHERE rowid = ?1",
                                               e, counter))
                            return false;
                    }

                    // Create thread if needed
                    if (!instance->db->Run(R"(INSERT INTO rec_threads (tid, stores) VALUES (?1, json_object(?2, ?3))
                                              ON CONFLICT DO UPDATE SET stores = json_patch(stores, excluded.stores))",
                                           tid, frag.store, !frag.data.len))
                        return false;

                    *anchor_ptr = new_anchor;
                }
            }

            return true;
        });
        if (!success)
            return;

        io->AttachText(200, "{}", "application/json");
    });
}

void HandleRecordExport(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RG_UNREACHABLE();
}

}
