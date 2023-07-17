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
#include "instance.hh"
#include "record.hh"
#include "user.hh"
#include "src/core/libwrap/json.hh"

namespace RG {

static void JsonRawOrNull(sq_Statement *stmt, int column, json_Writer *json)
{
    const char *text = (const char *)sqlite3_column_text(*stmt, column);

    if (text) {
        json->Raw(text);
    } else {
        json->Null();
    }
}

// Make sure tags are safe and can't lead to SQL injection before calling this function
static bool PrepareRecordSelect(InstanceHolder *instance, int64_t userid, const SessionStamp &stamp,
                                const char *tid, int64_t anchor, sq_Statement *out_stmt)
{
    LocalArray<char, 2048> sql;

    if (anchor < 0) {
        sql.len += Fmt(sql.TakeAvailable(),
                       R"(SELECT t.rowid AS t, t.tid,
                                 e.rowid AS e, e.eid, e.deleted, e.anchor, e.ctime, e.mtime, e.store, e.sequence,
                                 IIF(?1 IS NOT NULL, e.data, NULL) AS data,
                                 IIF(?1 IS NOT NULL, e.meta, NULL) AS meta,
                                 e.tags AS tags
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
    } else {
        RG_ASSERT(stamp.HasPermission(UserPermission::DataLoad));
        RG_ASSERT(stamp.HasPermission(UserPermission::DataAudit));

        sql.len += Fmt(sql.TakeAvailable(),
                       R"(WITH RECURSIVE rec (idx, eid, anchor, mtime, data, meta, tags) AS (
                              SELECT 1, eid, anchor, mtime, data, meta, tags
                                  FROM rec_fragments
                                  WHERE (tid = ?1 OR ?1 IS NULL) AND
                                        anchor <= ?3 AND previous IS NULL AND
                                        data IS NOT NULL
                              UNION ALL
                              SELECT rec.idx + 1, f.eid, f.anchor, f.mtime,
                                  IIF(?1 IS NOT NULL, json_patch(rec.data, f.data), NULL) AS data,
                                  IIF(?1 IS NOT NULL, json_patch(rec.meta, f.meta), NULL) AS meta,
                                  f.tags
                                  FROM rec_fragments f, rec
                                  WHERE f.anchor <= ?3 AND f.previous = rec.anchor AND
                                                           f.data IS NOT NULL
                              ORDER BY anchor
                          )
                          SELECT t.rowid AS t, t.tid,
                                 e.rowid AS e, e.eid, e.deleted, rec.anchor, e.ctime, rec.mtime,
                                 e.store, e.sequence, rec.data, rec.meta, rec.tags
                              FROM rec
                              INNER JOIN rec_entries e ON (e.eid = rec.eid)
                              INNER JOIN rec_threads t ON (t.tid = e.tid)
                              WHERE 1+1)", out_stmt).len;

        sql.len += Fmt(sql.TakeAvailable(), " ORDER BY t.rowid, e.store, rec.idx DESC").len;
    }

    return instance->db->Prepare(sql.data, out_stmt, tid, userid, anchor);
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
            json.Key("saved"); json.Bool(true);

            json.Key("entries"); json.StartObject();
            do {
                int64_t e = sqlite3_column_int64(stmt, 2);
                const char *store = (const char *)sqlite3_column_text(stmt, 8);

                // This can happen when the recursive CTE is used for historical data
                if (e == prev_e)
                    continue;
                prev_e = e;

                int64_t anchor = sqlite3_column_int64(stmt, 5);

                json.Key(store); json.StartObject();

                json.Key("store"); json.String(store);
                json.Key("eid"); json.String((const char *)sqlite3_column_text(stmt, 3));
                if (stamp->HasPermission(UserPermission::DataAudit)) {
                    json.Key("deleted"); json.Bool(sqlite3_column_int(stmt, 4));
                } else {
                    RG_ASSERT(!sqlite3_column_int(stmt, 4));
                }
                json.Key("anchor"); json.Int64(anchor);
                json.Key("ctime"); json.Int64(sqlite3_column_int64(stmt, 6));
                json.Key("mtime"); json.Int64(sqlite3_column_int64(stmt, 7));
                json.Key("sequence"); json.Int64(sqlite3_column_int64(stmt, 9));
                json.Key("tags"); JsonRawOrNull(&stmt, 12, &json);

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
        json.Key("saved"); json.Bool(true);

        json.Key("entries"); json.StartObject();
        do {
            int64_t e = sqlite3_column_int64(stmt, 2);
            const char *store = (const char *)sqlite3_column_text(stmt, 8);

            // This can happen with the recursive CTE is used for historical data
            if (e == prev_e)
                continue;
            prev_e = e;

            int64_t anchor = sqlite3_column_int64(stmt, 5);

            json.Key(store); json.StartObject();

            json.Key("store"); json.String(store);
            json.Key("eid"); json.String((const char *)sqlite3_column_text(stmt, 3));
            if (stamp->HasPermission(UserPermission::DataAudit)) {
                json.Key("deleted"); json.Bool(sqlite3_column_int(stmt, 4));
            } else {
                RG_ASSERT(!sqlite3_column_int(stmt, 4));
            }
            json.Key("anchor"); json.Int64(anchor);
            json.Key("ctime"); json.Int64(sqlite3_column_int64(stmt, 6));
            json.Key("mtime"); json.Int64(sqlite3_column_int64(stmt, 7));
            json.Key("sequence"); json.Int64(sqlite3_column_int64(stmt, 9));
            json.Key("data"); JsonRawOrNull(&stmt, 10, &json);
            json.Key("meta"); JsonRawOrNull(&stmt, 11, &json);
            json.Key("tags"); JsonRawOrNull(&stmt, 12, &json);

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
                                  ORDER BY f.anchor)", &stmt, tid))
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

void HandleRecordExport(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RG_UNREACHABLE();
}

}
