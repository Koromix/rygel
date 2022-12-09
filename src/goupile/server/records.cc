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

static bool PrepareRecordSelect(InstanceHolder *instance, int64_t userid, const SessionStamp &stamp,
                                const char *tid, int64_t anchor, sq_Statement *out_stmt)
{
    if (anchor < 0) {
        LocalArray<char, 2048> sql;

        sql.len += Fmt(sql.TakeAvailable(), R"(SELECT t.rowid AS t, t.tid, t.deleted,
                                                      e.rowid AS e, e.eid, e.ctime, e.mtime, e.store, e.sequence,
                                                      IIF(?1 IS NOT NULL, e.data, NULL) AS data
                                               FROM rec_threads t
                                               INNER JOIN rec_entries e ON (e.tid = t.tid)
                                               WHERE 1+1)").len;
        if (tid) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.tid = ?1").len;
        }
        if (!stamp.HasPermission(UserPermission::DataAudit)) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.deleted = 0").len;
        }
        if (!stamp.HasPermission(UserPermission::DataLoad)) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.tid IN (SELECT tid FROM ins_claims WHERE userid = ?2)").len;
        }
        sql.len += Fmt(sql.TakeAvailable(), " ORDER BY t.rowid, e.store").len;

        if (!instance->db->Prepare(sql.data, out_stmt))
            return false;
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
                                      SELECT t.rowid AS t, t.tid, t.deleted,
                                             e.rowid AS e, e.eid, e.ctime, rec.mtime, e.store, e.sequence, rec.data
                                          FROM rec
                                          INNER JOIN rec_entries e ON (e.eid = rec.eid)
                                          INNER JOIN rec_threads t ON (t.tid = e.tid)
                                          ORDER BY t.rowid, e.store, rec.idx DESC)", out_stmt))
            return false;
    }

    if (tid) {
        sqlite3_bind_text(*out_stmt, 1, tid, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(*out_stmt, 1);
    }
    sqlite3_bind_int64(*out_stmt, 2, anchor);

    return true;
}

void HandleRecordList(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    if (instance->config.sync_mode == SyncMode::Offline) {
        LogError("Records API is disabled in Offline mode");
        io->AttachError(403);
        return;
    }

    RetainPtr<const SessionInfo> session = GetCheckedSession(instance, request, io);
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
            json.Key("deleted"); json.Bool(sqlite3_column_int(stmt, 2));

            json.Key("entries"); json.StartObject();
            do {
                int64_t e = sqlite3_column_int64(stmt, 3);
                const char *store = (const char *)sqlite3_column_text(stmt, 7);

                // This can happen when the recursive CTE is used for historical data
                if (e == prev_e)
                    continue;
                prev_e = e;

                json.Key(store); json.StartObject();
                    json.Key("eid"); json.String((const char *)sqlite3_column_text(stmt, 4));
                    json.Key("ctime"); json.Int64(sqlite3_column_int64(stmt, 5));
                    json.Key("mtime"); json.Int64(sqlite3_column_int64(stmt, 6));
                    json.Key("sequence"); json.Int64(sqlite3_column_int64(stmt, 8));
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

    RetainPtr<const SessionInfo> session = GetCheckedSession(instance, request, io);
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
        json.Key("deleted"); json.Bool(sqlite3_column_int(stmt, 2));

        json.Key("entries"); json.StartObject();
        do {
            int64_t e = sqlite3_column_int64(stmt, 3);
            const char *store = (const char *)sqlite3_column_text(stmt, 7);

            // This can happen with the recursive CTE is used for historical data
            if (e == prev_e)
                continue;
            prev_e = e;

            json.Key(store); json.StartObject();

            json.Key("eid"); json.String((const char *)sqlite3_column_text(stmt, 4));
            json.Key("ctime"); json.Int64(sqlite3_column_int64(stmt, 5));
            json.Key("mtime"); json.Int64(sqlite3_column_int64(stmt, 6));
            json.Key("sequence"); json.Int64(sqlite3_column_int64(stmt, 8));
            json.Key("data"); json.Raw((const char *)sqlite3_column_text(stmt, 9));

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

    RetainPtr<const SessionInfo> session = GetCheckedSession(instance, request, io);

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

void HandleRecordSave(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RG_UNREACHABLE();
}

void HandleRecordExport(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RG_UNREACHABLE();
}

}
