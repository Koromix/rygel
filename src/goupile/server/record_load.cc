// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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
#include "domain.hh"
#include "goupile.hh"
#include "instance.hh"
#include "record.hh"
#include "user.hh"
#include "src/core/wrap/json.hh"

namespace RG {

struct RecordFilter {
    const char *single_tid = nullptr;

    int64_t audit_anchor = -1;
    bool allow_deleted = false;
    bool use_claims = false;

    int64_t start_t = -1;
    int64_t end_t = -1;

    bool read_data = false;
    bool read_meta = false;
};

struct RecordInfo {
    int64_t t = -1;
    const char *tid = nullptr;
    const char *counters = nullptr;
    const char *secrets = nullptr;
    bool locked = false;

    int64_t e = -1;
    const char *eid = nullptr;
    bool deleted = false;
    int64_t anchor = -1;
    int64_t ctime = -1;
    int64_t mtime = -1;
    const char *store = nullptr;
    Span<const char> tags = {};

    const char *summary = nullptr;
    Span<const char> data = {};
    Span<const char> meta = {};
};

class RecordWalker {
    sq_Statement stmt;

    bool read_data = false;
    bool read_meta = false;

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
                       R"(SELECT t.sequence AS t, t.tid, t.counters, t.secrets, t.locked,
                                 e.rowid AS e, e.eid, e.deleted, e.anchor, e.ctime, e.mtime,
                                 e.store, e.summary, e.tags AS tags,
                                 IIF(?6 = 1, e.data, NULL) AS data, IIF(?7 = 1, e.meta, NULL) AS meta
                          FROM rec_threads t
                          INNER JOIN rec_entries e ON (e.tid = t.tid)
                          WHERE 1=1)").len;

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
            sql.len += Fmt(sql.TakeAvailable(), " AND t.sequence >= ?4").len;
        }
        if (filter.end_t >= 0) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.sequence < ?5").len;
        }

        sql.len += Fmt(sql.TakeAvailable(), " ORDER BY t.sequence, e.store").len;
    } else {
        RG_ASSERT(!filter.use_claims);

        sql.len += Fmt(sql.TakeAvailable(),
                       R"(WITH RECURSIVE rec (idx, eid, anchor, mtime, summary, data, meta, tags) AS (
                              SELECT 1, eid, anchor, mtime, summary, data, meta, tags
                                  FROM rec_fragments
                                  WHERE (tid = ?1 OR ?1 IS NULL) AND
                                        anchor <= ?3 AND previous IS NULL
                              UNION ALL
                              SELECT rec.idx + 1, f.eid, f.anchor, f.mtime, f.summary,
                                  IIF(?6 = 1, json_patch(rec.data, f.data), NULL) AS data,
                                  IIF(?7 = 1, json_patch(rec.meta, f.meta), NULL) AS meta,
                                  f.tags
                                  FROM rec_fragments f, rec
                                  WHERE f.anchor <= ?3 AND f.previous = rec.anchor
                              ORDER BY anchor
                          )
                          SELECT t.sequence AS t, t.tid, t.counters, t.secrets, t.locked,
                                 e.rowid AS e, e.eid, IIF(rec.data IS NULL, 1, 0) AS deleted,
                                 rec.anchor, e.ctime, rec.mtime, e.store,
                                 rec.summary, rec.tags, rec.data, rec.meta
                              FROM rec
                              INNER JOIN rec_entries e ON (e.eid = rec.eid)
                              INNER JOIN rec_threads t ON (t.tid = e.tid)
                              WHERE 1+1)").len;

        if (!filter.allow_deleted) {
            sql.len += Fmt(sql.TakeAvailable(), " AND rec.data IS NOT NULL").len;
        }
        if (filter.start_t >= 0) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.sequence >= ?4").len;
        }
        if (filter.end_t >= 0) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.sequence < ?5").len;
        }

        sql.len += Fmt(sql.TakeAvailable(), " ORDER BY t.sequence, e.store, rec.idx DESC").len;
    }

    if (!instance->db->Prepare(sql.data, &stmt))
        return false;

    sqlite3_bind_text(stmt, 1, filter.single_tid, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, -userid);
    sqlite3_bind_int64(stmt, 3, filter.audit_anchor);
    sqlite3_bind_int64(stmt, 4, filter.start_t);
    sqlite3_bind_int64(stmt, 5, filter.end_t);
    sqlite3_bind_int(stmt, 6, 0 + filter.read_data);
    sqlite3_bind_int(stmt, 7, 0 + filter.read_meta);

    read_data = filter.read_data;
    read_meta = filter.read_meta;

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
    if (stmt.IsDone())
        return false;

    if (!step)
        return true;
    step = false;

again:
    if (!stmt.Step())
        return false;

    int64_t t = sqlite3_column_int64(stmt, 0);
    int64_t e = sqlite3_column_int64(stmt, 5);

    // This can happen with the recursive CTE is used for historical data
    if (e == cursor.e)
        goto again;

    cursor.t = t;
    cursor.tid = (const char *)sqlite3_column_text(stmt, 1);
    cursor.counters = (const char *)sqlite3_column_text(stmt, 2);
    cursor.secrets = (const char *)sqlite3_column_text(stmt, 3);
    cursor.locked = sqlite3_column_int(stmt, 4);

    cursor.e = e;
    cursor.eid = (const char *)sqlite3_column_text(stmt, 6);
    cursor.deleted = !!sqlite3_column_int(stmt, 7);
    cursor.anchor = sqlite3_column_int64(stmt, 8);
    cursor.ctime = sqlite3_column_int64(stmt, 9);
    cursor.mtime = sqlite3_column_int64(stmt, 10);
    cursor.store = (const char *)sqlite3_column_text(stmt, 11);
    cursor.summary = (const char *)sqlite3_column_text(stmt, 12);
    cursor.tags = MakeSpan((const char *)sqlite3_column_text(stmt, 13), sqlite3_column_bytes(stmt, 13));

    cursor.data = read_data ? MakeSpan((const char *)sqlite3_column_text(stmt, 14), sqlite3_column_bytes(stmt, 14))
                            : MakeSpan((const char *)nullptr, 0);
    cursor.meta = read_meta ? MakeSpan((const char *)sqlite3_column_text(stmt, 15), sqlite3_column_bytes(stmt, 15))
                            : MakeSpan((const char *)nullptr, 0);

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

void HandleRecordList(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();

    if (!instance->config.data_remote) {
        LogError("Records API is disabled in Offline mode");
        io->SendError(403);
        return;
    }

    RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);
    const SessionStamp *stamp = session ? session->GetStamp(instance) : nullptr;

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!stamp) {
        LogError("User is not allowed to list data");
        io->SendError(403);
        return;
    }

    int64_t anchor = -1;
    bool allow_deleted = false;
    {
        if (const char *str = request.GetQueryValue("anchor"); str) {
            if (!ParseInt(str, &anchor)) {
                io->SendError(422);
                return;
            }
            if (anchor <= 0) {
                LogError("Anchor must be a positive number");
                io->SendError(422);
                return;
            }
        }

        if (const char *str = request.GetQueryValue("deleted"); str && !ParseBool(str, &allow_deleted)) {
            io->SendError(422);
            return;
        }

        if (!stamp->HasPermission(UserPermission::DataRead) ||
                !stamp->HasPermission(UserPermission::DataAudit)) {
            if (anchor >= 0) {
                LogError("User is not allowed to access historical data");
                io->SendError(403);
                return;
            }
            if (allow_deleted) {
                LogError("User is not allowed to access deleted data");
                io->SendError(403);
                return;
            }
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
        filter.allow_deleted = allow_deleted;
        filter.use_claims = !stamp->HasPermission(UserPermission::DataRead);

        if (!walker.Prepare(instance, session->userid, filter))
            return;
    }

    json.StartArray();
    while (walker.Next()) {
        const RecordInfo *cursor = walker.GetCursor();

        json.StartObject();

        json.Key("tid"); json.String(cursor->tid);
        json.Key("sequence"); json.Int64(cursor->t);
        json.Key("saved"); json.Bool(true);
        json.Key("locked"); json.Bool(cursor->locked);

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
            if (cursor->summary) {
                json.Key("summary"); json.String(cursor->summary);
            } else {
                json.Key("summary"); json.Null();
            }
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

void HandleRecordGet(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();

    if (!instance->config.data_remote) {
        LogError("Records API is disabled in Offline mode");
        io->SendError(403);
        return;
    }

    RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);
    const SessionStamp *stamp = session ? session->GetStamp(instance) : nullptr;

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!stamp) {
        LogError("User is not allowed to load data");
        io->SendError(403);
        return;
    }

    const char *tid = nullptr;
    int64_t anchor = -1;
    bool allow_deleted = false;
    {
        tid = request.GetQueryValue("tid");
        if (!tid) {
            LogError("Missing 'tid' parameter");
            io->SendError(422);
            return;
        }

        if (const char *str = request.GetQueryValue("anchor"); str) {
            if (!ParseInt(str, &anchor)) {
                io->SendError(422);
                return;
            }
            if (anchor <= 0) {
                LogError("Anchor must be a positive number");
                io->SendError(422);
                return;
            }
        }

        if (const char *str = request.GetQueryValue("deleted"); str && !ParseBool(str, &allow_deleted)) {
            io->SendError(422);
            return;
        }

        if (!stamp->HasPermission(UserPermission::DataRead) ||
                !stamp->HasPermission(UserPermission::DataAudit)) {
            if (anchor >= 0) {
                LogError("User is not allowed to access historical data");
                io->SendError(403);
                return;
            }
            if (allow_deleted) {
                LogError("User is not allowed to access deleted data");
                io->SendError(403);
                return;
            }
        }
    }

    RecordWalker walker;
    {
        RecordFilter filter = {};

        filter.single_tid = tid;
        filter.audit_anchor = anchor;
        filter.allow_deleted = allow_deleted;
        filter.use_claims = !stamp->HasPermission(UserPermission::DataRead);
        filter.read_data = true;
        filter.read_meta = true;

        if (!walker.Prepare(instance, session->userid, filter))
            return;
    }

    if (!walker.Next()) {
        if (walker.IsValid()) {
            LogError("Thread '%1' does not exist", tid);
            io->SendError(404);
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
        json.Key("sequence"); json.Int64(cursor->t);
        json.Key("counters"); json.Raw(cursor->counters);
        json.Key("saved"); json.Bool(true);
        json.Key("locked"); json.Bool(cursor->locked);

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
            if (cursor->summary) {
                json.Key("summary"); json.String(cursor->summary);
            } else {
                json.Key("summary"); json.Null();
            }
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

void HandleRecordAudit(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();

    if (!instance->config.data_remote) {
        LogError("Records API is disabled in Offline mode");
        io->SendError(403);
        return;
    }

    RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->HasPermission(instance, UserPermission::DataAudit)) {
        LogError("User is not allowed to audit data");
        io->SendError(403);
        return;
    }

    const char *tid = request.GetQueryValue("tid");
    if (!tid) {
        LogError("Missing 'tid' parameter");
        io->SendError(422);
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
            io->SendError(404);
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

void RunExport(http_IO *io, InstanceHolder *instance, bool data, bool meta)
{
    const http_RequestInfo &request = io->Request();

    if (!instance->config.data_remote) {
        LogError("Records API is disabled in Offline mode");
        io->SendError(403);
        return;
    }

    // Check permissions
    {
        const char *export_key = !instance->slaves.len ? request.GetHeaderValue("X-Export-Key") : nullptr;

        if (export_key) {
            const InstanceHolder *master = instance->master;

            sq_Statement stmt;
            if (!gp_domain.db.Prepare(R"(SELECT permissions FROM dom_permissions
                                         WHERE instance = ?1 AND export_key = ?2)", &stmt))
                return;
            sqlite3_bind_text(stmt, 1, master->key.ptr, (int)master->key.len, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, export_key, -1, SQLITE_STATIC);

            uint32_t permissions = stmt.Step() ? (uint32_t)sqlite3_column_int(stmt, 0) : 0;

            if (!stmt.IsValid())
                return;
            if (!(permissions & (int)UserPermission::DataExport)) {
                LogError("Export key is not valid");
                io->SendError(403);
                return;
            }
        } else {
           RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);

           if (!session) {
                LogError("User is not logged in");
                io->SendError(401);
                return;
            } else if (!session->HasPermission(instance, UserPermission::DataExport)) {
                LogError("User is not allowed to export data");
                io->SendError(403);
                return;
            }
        }
    }

    int64_t from = 0;
    int64_t to = -1;
    {
        if (const char *str = request.GetQueryValue("from"); str) {
            if (!ParseInt(str, &from)) {
                io->SendError(422);
                return;
            }
            if (from < 0) {
                LogError("From must be 0 or a positive number");
                io->SendError(422);
                return;
            }
        }

        if (const char *str = request.GetQueryValue("to"); str) {
            if (!ParseInt(str, &to)) {
                io->SendError(422);
                return;
            }
            if (to <= from) {
                LogError("To must be greater than from");
                io->SendError(422);
                return;
            }
        }
    }

    RecordWalker walker;
    {
        RecordFilter filter = {};

        filter.start_t = from;
        filter.end_t = to;
        filter.read_data = data;
        filter.read_meta = meta;

        if (!walker.Prepare(instance, 0, filter))
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
        json.Key("sequence"); json.Int64(cursor->t);
        json.Key("counters"); json.Raw(cursor->counters);
        json.Key("secrets"); json.Raw(cursor->secrets);

        json.Key("entries"); json.StartObject();
        do {
            json.Key(cursor->store); json.StartObject();

            json.Key("store"); json.String(cursor->store);
            json.Key("eid"); json.String(cursor->eid);
            json.Key("anchor"); json.Int64(cursor->anchor);
            json.Key("ctime"); json.Int64(cursor->ctime);
            json.Key("mtime"); json.Int64(cursor->mtime);
            json.Key("tags"); JsonRawOrNull(cursor->tags, &json);

            if (data) {
                json.Key("data"); JsonRawOrNull(cursor->data, &json);
            }
            if (meta) {
                json.Key("meta"); JsonRawOrNull(cursor->meta, &json);
            }

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

void HandleExportData(http_IO *io, InstanceHolder *instance)
{
    RunExport(io, instance, true, false);
}

void HandleExportMeta(http_IO *io, InstanceHolder *instance)
{
    RunExport(io, instance, false, true);
}

}
