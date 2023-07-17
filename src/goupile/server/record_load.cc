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
#include "record.hh"
#include "user.hh"
#include "src/core/libwrap/json.hh"

namespace RG {

struct RecordFilter {
    const char *single_tid = nullptr;

    int64_t audit_anchor = -1;
    bool allow_deleted = false;
    bool use_claims = false;
    int64_t start_t = -1;

    bool read_data = false;
};

struct RecordInfo {
    int64_t t = -1;
    const char *tid = nullptr;

    int64_t e = -1;
    const char *eid = nullptr;
    bool deleted = false;
    int64_t anchor = -1;
    int64_t ctime = -1;
    int64_t mtime = -1;
    const char *store = nullptr;
    int64_t sequence = -1;
    Span<const char> tags = {};

    Span<const char> data = {};
    Span<const char> meta = {};
};

class RecordWalker {
    sq_Statement stmt;

    bool data = false;

    bool step = false;
    RecordInfo cursor;

public:
    // Make sure tags are safe and can't lead to SQL injection before calling this function
    bool Prepare(InstanceHolder *instance, int64_t userid, const RecordFilter &filter);

    bool Next();
    bool NextInThread();

    const RecordInfo *GetCursor() const { return &cursor; }
    bool IsValid() const { return stmt.IsValid(); }

private:
    bool Step();
};

bool RecordWalker::Prepare(InstanceHolder *instance, int64_t userid, const RecordFilter &filter)
{
    LocalArray<char, 2048> sql;

    if (filter.audit_anchor < 0) {
        sql.len += Fmt(sql.TakeAvailable(),
                       R"(SELECT t.rowid AS t, t.tid,
                                 e.rowid AS e, e.eid, e.deleted, e.anchor, e.ctime, e.mtime, e.store, e.sequence, e.tags AS tags,
                                 IIF(?4 = 1, e.data, NULL) AS data,
                                 IIF(?4 = 1, e.meta, NULL) AS meta
                          FROM rec_threads t
                          INNER JOIN rec_entries e ON (e.tid = t.tid)
                          WHERE 1+1)").len;

        if (filter.single_tid) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.tid = ?1").len;
        }
        if (!filter.allow_deleted) {
            sql.len += Fmt(sql.TakeAvailable(), " AND e.deleted = 0").len;
        }
        if (filter.use_claims) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.tid IN (SELECT tid FROM ins_claims WHERE userid = ?2)").len;
        }
        if (filter.start_t >= 0) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.rowid >= ?5").len;
        }

        sql.len += Fmt(sql.TakeAvailable(), " ORDER BY t.rowid, e.store").len;
    } else {
        RG_ASSERT(!filter.use_claims);
        RG_ASSERT(filter.allow_deleted);

        sql.len += Fmt(sql.TakeAvailable(),
                       R"(WITH RECURSIVE rec (idx, eid, anchor, mtime, data, meta, tags) AS (
                              SELECT 1, eid, anchor, mtime, data, meta, tags
                                  FROM rec_fragments
                                  WHERE (tid = ?1 OR ?1 IS NULL) AND
                                        anchor <= ?3 AND previous IS NULL AND
                                        data IS NOT NULL
                              UNION ALL
                              SELECT rec.idx + 1, f.eid, f.anchor, f.mtime,
                                  IIF(?4 = 1, json_patch(rec.data, f.data), NULL) AS data,
                                  IIF(?4 = 1, json_patch(rec.meta, f.meta), NULL) AS meta,
                                  f.tags
                                  FROM rec_fragments f, rec
                                  WHERE f.anchor <= ?3 AND f.previous = rec.anchor AND
                                                           f.data IS NOT NULL
                              ORDER BY anchor
                          )
                          SELECT t.rowid AS t, t.tid,
                                 e.rowid AS e, e.eid, e.deleted, rec.anchor, e.ctime, rec.mtime,
                                 e.store, e.sequence, rec.tags, rec.data, rec.meta
                              FROM rec
                              INNER JOIN rec_entries e ON (e.eid = rec.eid)
                              INNER JOIN rec_threads t ON (t.tid = e.tid)
                              WHERE 1+1)").len;

        if (filter.start_t >= 0) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.rowid >= ?5").len;
        }

        sql.len += Fmt(sql.TakeAvailable(), " ORDER BY t.rowid, e.store, rec.idx DESC").len;
    }

    if (!instance->db->Prepare(sql.data, &stmt))
        return false;

    sqlite3_bind_text(stmt, 1, filter.single_tid, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, userid);
    sqlite3_bind_int64(stmt, 3, filter.audit_anchor);
    sqlite3_bind_int(stmt, 4, 0 + filter.read_data);
    sqlite3_bind_int64(stmt, 5, filter.start_t);

    data = filter.read_data;
    step = true;
    cursor = {};

    return true;
}

bool RecordWalker::Next()
{
    if (!Step())
        return false;

    step = true;
    return true;
}

bool RecordWalker::NextInThread()
{
    int64_t t = cursor.t;

    if (!Step())
        return false;
    if (cursor.t != t)
        return false;

    step = true;
    return true;
}

bool RecordWalker::Step()
{
    if (!step)
        return true;

again:
    if (!stmt.Step())
        return false;

    int64_t t = sqlite3_column_int64(stmt, 0);
    int64_t e = sqlite3_column_int64(stmt, 2);

    // This can happen with the recursive CTE is used for historical data
    if (e == cursor.e)
        goto again;

    cursor.t = t;
    cursor.tid = (const char *)sqlite3_column_text(stmt, 1);

    cursor.e = e;
    cursor.eid = (const char *)sqlite3_column_text(stmt, 3);
    cursor.deleted = !!sqlite3_column_int(stmt, 4);
    cursor.anchor = sqlite3_column_int64(stmt, 5);
    cursor.ctime = sqlite3_column_int64(stmt, 6);
    cursor.mtime = sqlite3_column_int64(stmt, 7);
    cursor.store = (const char *)sqlite3_column_text(stmt, 8);
    cursor.sequence = sqlite3_column_int64(stmt, 9);
    cursor.tags = MakeSpan((const char *)sqlite3_column_text(stmt, 10), sqlite3_column_bytes(stmt, 10));

    if (data) {
        cursor.data = MakeSpan((const char*)sqlite3_column_text(stmt, 11), sqlite3_column_bytes(stmt, 11));
        cursor.meta = MakeSpan((const char*)sqlite3_column_text(stmt, 12), sqlite3_column_bytes(stmt, 12));
    }

    return true;
}

static void JsonRawOrNull(Span<const char> str, json_Writer *json)
{
    if (str.ptr) {
        json->Raw(str);
    } else {
        json->Null();
    }
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

    // Export data
    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    RecordWalker walker;
    {
        RecordFilter filter = {};

        filter.audit_anchor = anchor;
        filter.allow_deleted = stamp->HasPermission(UserPermission::DataAudit);
        filter.use_claims = !stamp->HasPermission(UserPermission::DataLoad);

        if (!walker.Prepare(instance, session->userid, filter))
            return;
    }

    json.StartArray();
    while (walker.Next()) {
        const RecordInfo *cursor = walker.GetCursor();

        json.StartObject();

        json.Key("tid"); json.String(cursor->tid);
        json.Key("saved"); json.Bool(true);

        json.Key("entries"); json.StartObject();
        do {
            json.Key(cursor->store); json.StartObject();

            json.Key("store"); json.String(cursor->store);
            json.Key("eid"); json.String(cursor->eid);
            if (stamp->HasPermission(UserPermission::DataAudit)) {
                json.Key("deleted"); json.Bool(cursor->deleted);
            } else {
                RG_ASSERT(!cursor->deleted);
            }
            json.Key("anchor"); json.Int64(cursor->anchor);
            json.Key("ctime"); json.Int64(cursor->ctime);
            json.Key("mtime"); json.Int64(cursor->mtime);
            json.Key("sequence"); json.Int64(cursor->sequence);
            json.Key("tags"); JsonRawOrNull(cursor->tags, &json);

            json.EndObject();
        } while (walker.NextInThread());
        json.EndObject();

        json.EndObject();
    }
    if (!walker.IsValid())
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

    RecordWalker walker;
    {
        RecordFilter filter = {};

        filter.single_tid = tid;
        filter.audit_anchor = anchor;
        filter.allow_deleted = stamp->HasPermission(UserPermission::DataAudit);
        filter.use_claims = !stamp->HasPermission(UserPermission::DataLoad);
        filter.read_data = true;

        if (!walker.Prepare(instance, session->userid, filter))
            return;
    }

    if (!walker.Next()) {
        if (walker.IsValid()) {
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
        const RecordInfo *cursor = walker.GetCursor();

        json.Key("tid"); json.String(cursor->tid);
        json.Key("saved"); json.Bool(true);

        json.Key("entries"); json.StartObject();
        do {
            json.Key(cursor->store); json.StartObject();

            json.Key("store"); json.String(cursor->store);
            json.Key("eid"); json.String(cursor->eid);
            if (stamp->HasPermission(UserPermission::DataAudit)) {
                json.Key("deleted"); json.Bool(cursor->deleted);
            } else {
                RG_ASSERT(!cursor->deleted);
            }
            json.Key("anchor"); json.Int64(cursor->anchor);
            json.Key("ctime"); json.Int64(cursor->ctime);
            json.Key("mtime"); json.Int64(cursor->mtime);
            json.Key("sequence"); json.Int64(cursor->sequence);
            json.Key("tags"); JsonRawOrNull(cursor->tags, &json);

            json.Key("data"); JsonRawOrNull(cursor->data, &json);
            json.Key("meta"); JsonRawOrNull(cursor->meta, &json);

            json.EndObject();
        } while (walker.NextInThread());
        json.EndObject();
    }
    if (!walker.IsValid())
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
    if (instance->config.sync_mode == SyncMode::Offline) {
        LogError("Records API is disabled in Offline mode");
        io->AttachError(403);
        return;
    }

    RetainPtr<const SessionInfo> session = GetNormalSession(instance, request, io);

    if (session) {
        if (!session->HasPermission(instance, UserPermission::DataExport)) {
            LogError("User is not allowed to export data");
            io->AttachError(403);
            return;
        }
    } else if (!instance->slaves.len) {
        const char *export_key = request.GetHeaderValue("X-Export-Key");

        if (!export_key) {
            LogError("User is not logged in");
            io->AttachError(401);
            return;
        }

        sq_Statement stmt;
        if (!gp_domain.db.Prepare(R"(SELECT permissions FROM dom_permissions
                                     WHERE instance = ?1 AND export_key = ?2)", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, instance->key.ptr, (int)instance->key.len, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, export_key, -1, SQLITE_STATIC);

        uint32_t permissions = stmt.Step() ? (uint32_t)sqlite3_column_int(stmt, 0) : 0;

        if (!stmt.IsValid())
            return;
        if (!(permissions & (int)UserPermission::DataExport)) {
            LogError("Export key is not valid");
            io->AttachError(403);
            return;
        }
    }

    int64_t from = 0;
    if (const char *str = request.GetQueryValue("from"); str) {
        if (!ParseInt(str, &from)) {
            io->AttachError(422);
            return;
        }
        if (from < 0) {
            LogError("From must be 0 or a positive number");
            io->AttachError(422);
            return;
        }
    }

    RecordWalker walker;
    {
        RecordFilter filter = {};

        filter.read_data = true;
        filter.start_t = from;

        if (!walker.Prepare(instance, session->userid, filter))
            return;
    }

    const RecordInfo *cursor = walker.GetCursor();

    // Export data
    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartObject();

    json.Key("threads"); json.StartArray();
    for (int i = 0; walker.Next() && i < 100; i++) {
        json.StartObject();

        json.Key("tid"); json.String(cursor->tid);
        json.Key("saved"); json.Bool(true);

        json.Key("entries"); json.StartObject();
        do {
            json.Key(cursor->store); json.StartObject();

            json.Key("store"); json.String(cursor->store);
            json.Key("eid"); json.String(cursor->eid);
            json.Key("anchor"); json.Int64(cursor->anchor);
            json.Key("ctime"); json.Int64(cursor->ctime);
            json.Key("mtime"); json.Int64(cursor->mtime);
            json.Key("sequence"); json.Int64(cursor->sequence);
            json.Key("tags"); JsonRawOrNull(cursor->tags, &json);

            json.Key("data"); JsonRawOrNull(cursor->data, &json);
            json.Key("meta"); JsonRawOrNull(cursor->meta, &json);

            json.EndObject();
        } while (walker.NextInThread());
        json.EndObject();

        json.EndObject();
    }
    if (!walker.IsValid())
        return;
    json.EndArray();

    if (cursor->t > 0) {
        json.Key("next"); json.Int64(cursor->t + 1);
    } else {
        json.Key("next"); json.Null();
    }

    json.EndObject();

    json.Finish();
}

}
