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
                LogError("User is not allowed to get historical data");
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
    if (anchor < 0) {
        LocalArray<char, 1024> sql;

        sql.len += Fmt(sql.TakeAvailable(), R"(SELECT e.rowid, e.eid, e.ctime, e.mtime, e.store, e.sequence, e.data
                                               FROM rec_threads t
                                               INNER JOIN rec_entries e ON (e.tid = t.tid)
                                               WHERE t.tid = ?1 AND t.deleted = 0)").len;
        if (!stamp->HasPermission(UserPermission::DataLoad)) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.tid IN (SELECT tid FROM ins_claims WHERE userid = ?2)").len;
        }
        sql.len += Fmt(sql.TakeAvailable(), " ORDER BY e.store").len;

        if (!instance->db->Prepare(sql.data, &stmt))
            return;
        sqlite3_bind_text(stmt, 1, tid, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, -session->userid);
    } else {
        RG_ASSERT(stamp->HasPermission(UserPermission::DataLoad));

        if (!instance->db->Prepare(R"(WITH RECURSIVE rec (idx, eid, anchor, mtime, data) AS (
                                          SELECT 1, eid, anchor, mtime, data
                                              FROM rec_fragments
                                              WHERE tid = ?1 AND
                                                    anchor <= ?2 AND previous IS NULL
                                          UNION ALL
                                          SELECT rec.idx + 1, f.eid, f.anchor, f.mtime, json_patch(rec.data, f.data) AS data
                                              FROM rec_fragments f, rec
                                              WHERE f.anchor <= ?2 AND f.previous = rec.anchor
                                          ORDER BY anchor
                                      )
                                      SELECT e.rowid, e.eid, e.ctime, rec.mtime, e.store, e.sequence, rec.data
                                          FROM rec
                                          INNER JOIN rec_entries e ON (e.eid = rec.eid)
                                          ORDER BY e.store, rec.idx DESC)", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, tid, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, anchor);
    }

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
        int64_t prev_rowid = -1;

        do {
            int64_t rowid = sqlite3_column_int64(stmt, 0);
            const char *store = (const char *)sqlite3_column_text(stmt, 4);

            // This can happen with the recursive CTE is used for historical data
            if (rowid == prev_rowid)
                continue;
            prev_rowid = rowid;

            json.Key(store); json.StartObject();
                json.Key("eid"); json.String((const char *)sqlite3_column_text(stmt, 1));
                json.Key("ctime"); json.Int64(sqlite3_column_int64(stmt, 2));
                json.Key("mtime"); json.Int64(sqlite3_column_int64(stmt, 3));
                json.Key("sequence"); json.Int64(sqlite3_column_int64(stmt, 5));
                json.Key("data"); json.Raw((const char *)sqlite3_column_text(stmt, 6));
            json.EndObject();
        } while (stmt.Step());
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
            json.Key("anchor"); json.String((const char *)sqlite3_column_text(stmt, 0));
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
