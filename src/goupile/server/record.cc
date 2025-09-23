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
#include "config.hh"
#include "domain.hh"
#include "file.hh"
#include "goupile.hh"
#include "instance.hh"
#include "message.hh"
#include "record.hh"
#include "user.hh"
#include "vm.hh"
#include "src/core/request/smtp.hh"
#include "src/core/wrap/json.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace K {

struct RecordFilter {
    const char *single_tid = nullptr;

    int64_t audit_anchor = -1;
    bool allow_deleted = false;
    bool use_claims = false;

    int64_t min_sequence = -1;
    int64_t min_anchor = -1;

    bool read_data = false;
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
};

class RecordWalker {
    sq_Statement stmt;

    bool read_data = false;

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

struct DataConstraint {
    const char *key = nullptr;
    bool exists = false;
    bool unique = false;
};

struct FragmentInfo {
    int64_t fs = -1;
    const char *eid = nullptr;
    const char *store = nullptr;
    int64_t anchor = -1;
    const char *summary = nullptr;
    bool has_data = false;
    Span<const char> data = {};
    HeapArray<const char *> tags;
};

struct CounterInfo {
    const char *key = nullptr;
    int max = 0;
    bool randomize = false;
    bool secret = false;
};

struct SignupInfo {
    bool enable = false;

    const char *url = nullptr;
    const char *to = nullptr;
    const char *subject = nullptr;
    Span<const char> html = nullptr;
    Span<const char> text = nullptr;
};

struct BlobInfo {
    const char *sha256 = nullptr;
    const char *name = nullptr;
};

static bool CheckTag(Span<const char> tag)
{
    const auto test_char = [](char c) { return IsAsciiAlphaOrDigit(c) || c == '_'; };

    if (!tag.len) {
        LogError("Tag name cannot be empty");
        return false;
    }
    if (!std::all_of(tag.begin(), tag.end(), test_char)) {
        LogError("Tag names must only contain alphanumeric or '_' characters");
        return false;
    }

    return true;
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

static bool CheckKey(Span<const char> key)
{
    const auto test_char = [](char c) { return IsAsciiAlphaOrDigit(c) || c == '_'; };

    if (!key.len) {
        LogError("Empty key is not allowed");
        return false;
    }
    if (!std::all_of(key.begin(), key.end(), test_char)) {
        LogError("Invalid key characters");
        return false;
    }
    if (StartsWith(key, "__")) {
        LogError("Keys must not start with '__'");
        return false;
    }

    return true;
}

static bool CheckSha256(Span<const char> sha256)
{
    const auto test_char = [](char c) { return (c >= 'A' && c <= 'Z') || IsAsciiDigit(c); };

    if (sha256.len != 64) {
        LogError("Malformed SHA256 (incorrect length)");
        return false;
    }
    if (!std::all_of(sha256.begin(), sha256.end(), test_char)) {
        LogError("Malformed SHA256 (unexpected character)");
        return false;
    }

    return true;
}

bool RecordWalker::Prepare(InstanceHolder *instance, int64_t userid, const RecordFilter &filter)
{
    LocalArray<char, 2048> sql;

    if (filter.audit_anchor < 0) {
        sql.len += Fmt(sql.TakeAvailable(),
                       R"(SELECT t.sequence AS t, t.tid, t.counters, t.secrets, t.locked,
                                 e.rowid AS e, e.eid, e.deleted, e.anchor, e.ctime, e.mtime,
                                 e.store, e.summary, e.tags AS tags,
                                 IIF(?6 = 1, e.data, NULL) AS data
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
        if (filter.min_sequence >= 0) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.sequence >= ?4").len;
        }
        if (filter.min_anchor >= 0) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.tid IN (SELECT tid FROM rec_entries WHERE e.anchor >= ?5)").len;
        }

        sql.len += Fmt(sql.TakeAvailable(), " ORDER BY t.sequence, e.store").len;
    } else {
        K_ASSERT(!filter.single_tid);
        K_ASSERT(!filter.use_claims);

        sql.len += Fmt(sql.TakeAvailable(),
                       R"(WITH RECURSIVE rec (idx, eid, anchor, mtime, summary, tags, data) AS (
                              SELECT 1, eid, anchor, mtime, summary, tags, data
                                  FROM rec_fragments
                                  WHERE (tid = ?1 OR ?1 IS NULL) AND
                                        anchor <= ?3 AND previous IS NULL
                              UNION ALL
                              SELECT rec.idx + 1, f.eid, f.anchor, f.mtime, f.summary, f.tags,
                                  IIF(?6 = 1, json_patch(rec.data, f.data), NULL) AS data
                                  FROM rec_fragments f, rec
                                  WHERE f.anchor <= ?3 AND f.previous = rec.anchor
                              ORDER BY anchor
                          )
                          SELECT t.sequence AS t, t.tid, t.counters, t.secrets, t.locked,
                                 e.rowid AS e, e.eid, IIF(rec.data IS NULL, 1, 0) AS deleted,
                                 rec.anchor, e.ctime, rec.mtime, e.store,
                                 rec.summary, rec.tags, rec.data
                              FROM rec
                              INNER JOIN rec_entries e ON (e.eid = rec.eid)
                              INNER JOIN rec_threads t ON (t.tid = e.tid)
                              WHERE 1+1)").len;

        if (!filter.allow_deleted) {
            sql.len += Fmt(sql.TakeAvailable(), " AND rec.data IS NOT NULL").len;
        }
        if (filter.min_sequence >= 0) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.sequence >= ?4").len;
        }
        if (filter.min_anchor >= 0) {
            sql.len += Fmt(sql.TakeAvailable(), " AND t.tid IN (SELECT tid FROM rec_entries WHERE e.anchor >= ?5)").len;
        }

        sql.len += Fmt(sql.TakeAvailable(), " ORDER BY t.sequence, e.store, rec.idx DESC").len;
    }

    if (!instance->db->Prepare(sql.data, &stmt))
        return false;

    sqlite3_bind_text(stmt, 1, filter.single_tid, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, -userid);
    sqlite3_bind_int64(stmt, 3, filter.audit_anchor);
    sqlite3_bind_int64(stmt, 4, filter.min_sequence);
    sqlite3_bind_int64(stmt, 5, filter.min_anchor);
    sqlite3_bind_int(stmt, 6, 0 + filter.read_data);

    read_data = filter.read_data;

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

    if (read_data) {
        cursor.data = MakeSpan((const char *)sqlite3_column_text(stmt, 14), sqlite3_column_bytes(stmt, 14));
    } else {
        cursor.data = {};
    }

    return true;
}

void HandleRecordList(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();

    if (!instance->settings.data_remote) {
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

    RecordWalker walker;
    {
        RecordFilter filter = {};

        filter.audit_anchor = anchor;
        filter.allow_deleted = allow_deleted;
        filter.use_claims = !stamp->HasPermission(UserPermission::DataRead);

        if (!walker.Prepare(instance, session->userid, filter))
            return;
    }

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();

        while (walker.Next()) {
            const RecordInfo *cursor = walker.GetCursor();

            json->StartObject();

            json->Key("tid"); json->String(cursor->tid);
            json->Key("sequence"); json->Int64(cursor->t);
            json->Key("saved"); json->Bool(true);
            json->Key("locked"); json->Bool(cursor->locked);

            json->Key("entries"); json->StartArray();
            do {
                json->StartObject();

                json->Key("store"); json->String(cursor->store);
                json->Key("eid"); json->String(cursor->eid);
                if (stamp->HasPermission(UserPermission::DataAudit)) {
                    json->Key("deleted"); json->Bool(cursor->deleted);
                } else {
                    K_ASSERT(!cursor->deleted);
                }
                json->Key("anchor"); json->Int64(cursor->anchor);
                json->Key("ctime"); json->Int64(cursor->ctime);
                json->Key("mtime"); json->Int64(cursor->mtime);
                if (cursor->summary) {
                    json->Key("summary"); json->String(cursor->summary);
                } else {
                    json->Key("summary"); json->Null();
                }
                if (cursor->tags.ptr) {
                    json->Key("tags"); json->Raw(cursor->tags);
                } else {
                    json->Key("tags"); json->Null();
                }

                json->EndObject();
            } while (walker.NextInThread());
            json->EndArray();

            json->EndObject();
        }
        if (!walker.IsValid())
            return;

        json->EndArray();
    });
}

void HandleRecordGet(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();

    if (!instance->settings.data_remote) {
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

    http_SendJson(io, 200, [&](json_Writer *json) {
        const RecordInfo *cursor = walker.GetCursor();

        json->StartObject();

        json->Key("tid"); json->String(cursor->tid);
        json->Key("sequence"); json->Int64(cursor->t);
        json->Key("counters"); json->Raw(cursor->counters);
        json->Key("saved"); json->Bool(true);
        json->Key("locked"); json->Bool(cursor->locked);

        json->Key("entries"); json->StartArray();
        do {
            json->StartObject();

            json->Key("store"); json->String(cursor->store);
            json->Key("eid"); json->String(cursor->eid);
            if (stamp->HasPermission(UserPermission::DataAudit)) {
                json->Key("deleted"); json->Bool(cursor->deleted);
            } else {
                K_ASSERT(!cursor->deleted);
            }
            json->Key("anchor"); json->Int64(cursor->anchor);
            json->Key("ctime"); json->Int64(cursor->ctime);
            json->Key("mtime"); json->Int64(cursor->mtime);
            if (cursor->summary) {
                json->Key("summary"); json->String(cursor->summary);
            } else {
                json->Key("summary"); json->Null();
            }
            if (cursor->tags.ptr) {
                json->Key("tags"); json->Raw(cursor->tags);
            } else {
                json->Key("tags"); json->Null();
            }
            if (cursor->data.ptr) {
                json->Key("data"); json->Raw(cursor->data);
            } else {
                json->Key("data"); json->Null();
            }

            json->EndObject();
        } while (walker.NextInThread());
        json->EndArray();

        if (!walker.IsValid())
            return;

        json->EndObject();
    });
}

void HandleRecordAudit(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();

    if (!instance->settings.data_remote) {
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

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();

        do {
            json->StartObject();

            json->Key("anchor"); json->Int64(sqlite3_column_int64(stmt, 0));
            json->Key("eid"); json->String((const char *)sqlite3_column_text(stmt, 1));
            json->Key("store"); json->String((const char *)sqlite3_column_text(stmt, 2));
            json->Key("type"); json->String((const char *)sqlite3_column_text(stmt, 3));
            json->Key("userid"); json->Int64(sqlite3_column_int64(stmt, 4));
            json->Key("username"); json->String((const char *)sqlite3_column_text(stmt, 5));

            json->EndObject();
        } while (stmt.Step());
        if (!stmt.IsValid())
            return;

        json->EndArray();
    });
}

static int64_t CheckExportPermission(http_IO *io, InstanceHolder *instance, uint32_t allow)
{
    const http_RequestInfo &request = io->Request();
    const char *export_key = !instance->slaves.len ? request.GetHeaderValue("X-Export-Key") : nullptr;

    if (export_key) {
        const InstanceHolder *master = instance->master;

        sq_Statement stmt;
        if (!gp_db.Prepare(R"(SELECT p.permissions, u.userid
                              FROM dom_permissions p
                              INNER JOIN dom_users ON (u.userid = p.userid)
                              WHERE p.instance = ?1 AND p.export_key = ?2)", &stmt))
            return -1;
        sqlite3_bind_text(stmt, 1, master->key.ptr, (int)master->key.len, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, export_key, -1, SQLITE_STATIC);

        if (stmt.Step()) {
            uint32_t permissions = (uint32_t)sqlite3_column_int(stmt, 0);
            int64_t userid = sqlite3_column_int64(stmt, 1);

            if (!(permissions & allow)) {
                LogError("Missing data export or fetch permission");
                io->SendError(403);
                return -1;
            }

            return userid;
        } else {
            if (stmt.IsValid()) {
                LogError("Export key is not valid");
                io->SendError(403);
            }
            return -1;
        }
    } else {
       RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);
       const SessionStamp *stamp = session ? session->GetStamp(instance) : nullptr;

       if (!session) {
            LogError("User is not logged in");
            io->SendError(401);
            return -1;
        }
        if (!stamp || !(stamp->permissions & allow)) {
            LogError("User is not allowed to export data");
            io->SendError(403);
            return -1;
        }

        return session->userid;
    }
}

static const char *MakeExportFileName(const char *instance_key, int64_t export_id, int64_t ctime, Allocator *alloc)
{
    TimeSpec spec = DecomposeTimeUTC(ctime);
    Span<char> basename = Fmt(alloc, "%1_%2_%3.json.gz", instance_key, export_id, FmtTimeISO(spec));

    for (char &c: basename) {
        c = (c == '/') ? '@' : c;
    }

    const char *filename = Fmt(alloc, "%1%/%2", gp_config.export_directory, basename).ptr;
    return filename;
}

int64_t ExportRecords(InstanceHolder *instance, int64_t userid, const ExportSettings &settings, ExportInfo *out_info)
{
    BlockAllocator temp_alloc;

    int fd = -1;
    const char *tmp_filename = CreateUniqueFile(gp_config.export_directory, nullptr, ".tmp", &temp_alloc, &fd);
    if (!tmp_filename)
        return -1;
    K_DEFER {
        CloseDescriptor(fd);
        UnlinkFile(tmp_filename);
    };

    RecordWalker walker;
    {
        RecordFilter filter = {};

        filter.min_sequence = settings.sequence;
        filter.min_anchor = settings.anchor;
        filter.read_data = true;

        if (!walker.Prepare(instance, 0, filter))
            return -1;
    }

    const RecordInfo *cursor = walker.GetCursor();

    StreamWriter st(fd, tmp_filename, 0, CompressionType::Gzip);
    json_Writer json(&st);

    int64_t now = GetUnixTime();
    int64_t max_sequence = -1;
    int64_t max_anchor = -1;
    int64_t threads = 0;

    json.StartObject();

    json.Key("format"); json.Int(1);

    json.Key("threads"); json.StartArray();
    while (walker.Next()) {
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
            if (cursor->tags.ptr) {
                json.Key("tags"); json.Raw(cursor->tags);
            } else {
                json.Key("tags"); json.Null();
            }
            if (cursor->data.ptr) {
                json.Key("data"); json.Raw(cursor->data);
            } else {
                json.Key("data"); json.Null();
            }

            json.EndObject();

            max_sequence = std::max(max_sequence, cursor->t);
            max_anchor = std::max(max_anchor, cursor->anchor);
        } while (walker.NextInThread());
        json.EndObject();

        json.EndObject();

        threads++;
    }
    if (!walker.IsValid())
        return -1;
    json.EndArray();

    json.EndObject();

    if (!st.Close())
        return -1;

    if (!threads) {
        sq_Statement stmt;
        if (!instance->db->Prepare(R"(SELECT sequence, anchor
                                      FROM rec_exports
                                      ORDER BY export DESC)", &stmt))
            return -1;

        if (stmt.Step()) {
            max_sequence = sqlite3_column_int64(stmt, 0);
            max_anchor = sqlite3_column_int64(stmt, 0);
        } else if (stmt.IsValid()) {
            max_sequence = 0;
            max_anchor = 0;
        } else {
            return -1;
        }
    }

    int64_t export_id;
    char secret[33];

    static_assert(K_SIZE(secret) == K_SIZE(ExportInfo::secret));
    Fmt(secret, "%1", FmtRandom(32));

    bool success = instance->db->Transaction([&]() {
        // Create export metadata
        {
            sq_Statement stmt;
            if (!instance->db->Prepare(R"(INSERT INTO rec_exports (ctime, userid, sequence, anchor, threads, scheduled, secret)
                                          VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)
                                          RETURNING export)",
                                       &stmt, now, userid, max_sequence, max_anchor, threads, 0 + settings.scheduled, secret))
                return false;
            if (!stmt.GetSingleValue(&export_id))
                return false;
        }

        const char *filename = MakeExportFileName(instance->key.ptr, export_id, now, &temp_alloc);

        if (threads && RenameFile(tmp_filename, filename, 0) != RenameResult::Success)
            return false;

        return true;
    });
    if (!success)
        return -1;

    if (out_info) {
        out_info->max_sequence = max_sequence;
        out_info->max_anchor = max_anchor;
        CopyString(secret, out_info->secret);
    }

    return export_id;
}

void HandleExportCreate(http_IO *io, InstanceHolder *instance)
{
    if (!instance->settings.data_remote) {
        LogError("Records API is disabled in Offline mode");
        io->SendError(403);
        return;
    }

    int64_t userid = CheckExportPermission(io, instance, (int)UserPermission::ExportCreate);
    if (userid < 0)
        return;

    ExportSettings settings;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "sequence") {
                    json->SkipNull() || json->ParseInt(&settings.sequence);
                } else if (key == "anchor") {
                    json->SkipNull() || json->ParseInt(&settings.anchor);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
    }

    ExportInfo info;
    int64_t export_id = ExportRecords(instance, userid, settings, &info);
    if (export_id < 0)
        return;

    const char *response = Fmt(io->Allocator(), "{ \"export\": %1, \"secret\": \"%2\" }", export_id, info.secret).ptr;
    io->SendText(200, response, "application/json");
}

void HandleExportList(http_IO *io, InstanceHolder *instance)
{
    if (!instance->settings.data_remote) {
        LogError("Records API is disabled in Offline mode");
        io->SendError(403);
        return;
    }

    if (CheckExportPermission(io, instance, (int)UserPermission::ExportCreate | (int)UserPermission::ExportDownload) < 0)
        return;

    sq_Statement stmt;
    if (!instance->db->Prepare(R"(SELECT export, ctime, userid, sequence,
                                         anchor, threads, scheduled
                                  FROM rec_exports
                                  ORDER BY export)", &stmt))
        return;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();

        while (stmt.Step()) {
            int64_t export_id = sqlite3_column_int64(stmt, 0);
            int64_t ctime = sqlite3_column_int64(stmt, 1);
            int64_t userid = sqlite3_column_int64(stmt, 2);
            int64_t sequence = sqlite3_column_int64(stmt, 3);
            int64_t anchor = sqlite3_column_int64(stmt, 4);
            int64_t threads = sqlite3_column_int64(stmt, 5);
            bool scheduled = sqlite3_column_int(stmt, 6);

            json->StartObject();
            json->Key("export"); json->Int64(export_id);
            json->Key("ctime"); json->Int64(ctime);
            json->Key("userid"); json->Int64(userid);
            json->Key("sequence"); json->Int64(sequence);
            json->Key("anchor"); json->Int64(anchor);
            json->Key("threads"); json->Int64(threads);
            json->Key("scheduled"); json->Bool(scheduled);
            json->EndObject();
        }
        if (!stmt.IsValid())
            return;

        json->EndArray();
    });
}

void HandleExportDownload(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();

    if (!instance->settings.data_remote) {
        LogError("Records API is disabled in Offline mode");
        io->SendError(403);
        return;
    }

    int64_t export_id;
    if (const char *str = request.GetQueryValue("export"); str) {
        if (!ParseInt(str, &export_id)) {
            io->SendError(422);
            return;
        }
        if (export_id <= 0) {
            LogError("Export ID must be a positive number");
            io->SendError(422);
            return;
        }
    } else {
        LogError("Missing 'export' parameter");
        io->SendError(422);
        return;
    }

    // The secret value allows users without export but without fetch permission to download the export after it's made
    const char *secret = request.GetHeaderValue("X-Export-Secret");

    if (!secret && CheckExportPermission(io, instance, (int)UserPermission::ExportDownload) < 0)
        return;

    int64_t ctime;
    int64_t threads;
    {
        sq_Statement stmt;
        if (!instance->db->Prepare(R"(SELECT ctime, threads FROM rec_exports
                                      WHERE export = ?1 AND secret = IFNULL(?2, secret))",
                                   &stmt, export_id, secret))
            return;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Unknown export %1", export_id);
                io->SendError(404);
            }
            return;
        }

        ctime = sqlite3_column_int64(stmt, 0);
        threads = sqlite3_column_int64(stmt, 1);
    }

    if (!threads) {
        LogError("Cannot download empty export");
        io->SendError(404);
        return;
    }

    TimeSpec spec = DecomposeTimeLocal(ctime);
    Span<const char> name = Fmt(io->Allocator(), "%1_%2_%3", instance->key, export_id, FmtTimeISO(spec));
    const char *filename = MakeExportFileName(instance->key.ptr, export_id, ctime, io->Allocator());

    io->AddHeader("Content-Encoding", "gzip");
    io->AddHeader("X-Export-Name", name);
    io->SendFile(200, filename);
}

static const char *TagsToJson(Span<const char *const> tags, Allocator *alloc)
{
    HeapArray<char> buf(alloc);

    if (!tags.len)
        return "[]";

    Fmt(&buf, "[");
    for (const char *tag: tags) {
        K_ASSERT(CheckTag(tag));
        Fmt(&buf, "\"%1\", ", tag);
    }
    buf.len -= 2;
    Fmt(&buf, "]");

    return buf.Leak().ptr;
}

static int64_t RunCounter(const CounterInfo &counter, int64_t state, int64_t *out_state)
{
    if (counter.max) {
        K_ASSERT(counter.max >= 1 && counter.max <= 64);

        uint64_t mask = state ? (uint64_t)state : ((1u << counter.max) - 1);

        if (counter.randomize) {
            int range = PopCount(mask);

            int rnd = GetRandomInt(0, range);
            int value = -1;

            while (rnd >= 0) {
                value++;
                rnd -= !!(mask & (1u << value));
            };

            *out_state = (int64_t)(mask & ~(1u << value));
            return value + 1;
        } else {
            int64_t value = CountTrailingZeros(mask);

            *out_state = (int64_t)(mask & ~(1u << value));
            return value + 1;
        }
    } else {
        int64_t value = (int64_t)state + 1;

        *out_state = value;
        return value;
    }
}

static bool PrepareSignup(const InstanceHolder *instance, const char *username,
                          SignupInfo &info, Allocator *alloc, smtp_MailContent *out_mail)
{
    Span<char> token;
    {
        Span<const char> msg = Fmt(alloc, R"({"key": "%1"})", username);

        Span<uint8_t> cypher = AllocateSpan<uint8_t>(alloc, msg.len + crypto_box_SEALBYTES);

        // Encode token
        if (crypto_box_seal((uint8_t *)cypher.ptr, (const uint8_t *)msg.ptr, msg.len, instance->settings.token_pkey) != 0) {
            LogError("Failed to seal token");
            return false;
        }

        // Encode Base64
        token = AllocateSpan<char>(alloc, cypher.len * 2 + 1);
        sodium_bin2hex(token.ptr, (size_t)token.len, cypher.ptr, (size_t)cypher.len);
        token.len = (Size)strlen(token.ptr);
    }

    const char *url = Fmt(alloc, "%1?token=%2", info.url, token).ptr;

    Span<const uint8_t> text = PatchFile(info.text.As<const uint8_t>(), alloc,
                                         [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "LINK") {
            writer->Write(url);
        } else {
            Print(writer, "{{%1}}", expr);
        }
    });
    Span<const uint8_t> html = PatchFile(info.html.As<const uint8_t>(), alloc,
                                         [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "LINK") {
            writer->Write(url);
        } else {
            Print(writer, "{{%1}}", expr);
        }
    });

    out_mail->subject = DuplicateString(info.subject, alloc).ptr;
    out_mail->text = (const char *)text.ptr;
    out_mail->html = (const char *)html.ptr;

    return true;
}

void HandleRecordSave(http_IO *io, InstanceHolder *instance)
{
    if (!instance->settings.data_remote) {
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
    if (!stamp || !stamp->HasPermission(UserPermission::DataSave)) {
        LogError("User is not allowed to save data");
        io->SendError(403);
        return;
    }

    const char *tid = nullptr;
    FragmentInfo fragment = {};
    HeapArray<DataConstraint> constraints;
    HeapArray<CounterInfo> counters;
    HeapArray<const char *> publics;
    SignupInfo signup = {};
    HeapArray<BlobInfo> blobs;
    bool claim = true;
    {
        bool success = http_ParseJson(io, Mebibytes(8), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "tid") {
                    json->ParseString(&tid);
                } else if (key == "eid") {
                    json->ParseString(&fragment.eid);
                } else if (key == "store") {
                    json->ParseString(&fragment.store);
                } else if (key == "anchor") {
                    json->ParseInt(&fragment.anchor);
                } else if (key == "fs") {
                    json->ParseInt(&fragment.fs);
                } else if (key == "summary") {
                    json->SkipNull() || json->ParseString(&fragment.summary);
                } else if (key == "data") {
                    switch (json->PeekToken()) {
                        case json_TokenType::Null: {
                            json->ParseNull();

                            fragment.data = {};
                            fragment.has_data = true;
                        } break;
                        case json_TokenType::StartObject: {
                            json->PassThrough(&fragment.data);
                            fragment.has_data = true;
                        } break;

                        default: {
                            LogError("Unexpected value type for fragment data");
                            valid = false;
                        } break;
                    }
                } else if (key == "tags") {
                    for (json->ParseArray(); json->InArray(); ) {
                        const char *tag = json->ParseString().ptr;
                        fragment.tags.Append(tag);
                    }
                } else if (key == "constraints") {
                    for (json->ParseObject(); json->InObject(); ) {
                        DataConstraint constraint = {};

                        json->ParseKey(&constraint.key);
                        for (json->ParseObject(); json->InObject(); ) {
                            Span<const char> type = json->ParseKey();

                            if (type == "exists") {
                                json->ParseBool(&constraint.exists);
                            } else if (type == "unique") {
                                json->ParseBool(&constraint.unique);
                            } else {
                                json->UnexpectedKey(key);
                                valid = false;
                            }
                        }

                        constraints.Append(constraint);
                    }
                } else if (key == "counters") {
                    for (json->ParseObject(); json->InObject(); ) {
                        CounterInfo counter = {};

                        json->ParseKey(&counter.key);
                        for (json->ParseObject(); json->InObject(); ) {
                            Span<const char> key = json->ParseKey();

                            if (key == "key") {
                                json->ParseString(&counter.key);
                            } else if (key == "max") {
                                json->SkipNull() || json->ParseInt(&counter.max);
                            } else if (key == "randomize") {
                                json->ParseBool(&counter.randomize);
                            } else if (key == "secret") {
                                json->ParseBool(&counter.secret);
                            } else {
                                json->UnexpectedKey(key);
                                valid = false;
                            }
                        }

                        counters.Append(counter);
                    }
                } else if (key == "publics") {
                    for (json->ParseArray(); json->InArray(); ) {
                        const char *key = json->ParseString().ptr;
                        publics.Append(key);
                    }
                } else if (key == "signup") {
                    switch (json->PeekToken()) {
                        case json_TokenType::Null: {
                            json->ParseNull();
                            signup.enable = false;
                        } break;
                        case json_TokenType::StartObject: {
                            signup.enable = (session->userid >= 0);

                            for (json->ParseObject(); json->InObject(); ) {
                                Span<const char> key = json->ParseKey();

                                if (key == "url") {
                                    json->ParseString(&signup.url);
                                } else if (key == "to") {
                                    json->ParseString(&signup.to);
                                } else if (key == "subject") {
                                    json->ParseString(&signup.subject);
                                } else if (key == "html") {
                                    json->ParseString(&signup.html);
                                } else if (key == "text") {
                                    json->ParseString(&signup.text);
                                } else {
                                    json->UnexpectedKey(key);
                                    valid = false;
                                }
                            }
                        } break;

                        default: {
                            LogError("Unexpected value type for signup data");
                            valid = false;
                        } break;
                    }
                } else if (key == "blobs") {
                    for (json->ParseArray(); json->InArray(); ) {
                        BlobInfo blob = {};

                        for (json->ParseObject(); json->InObject(); ) {
                            Span<const char> key = json->ParseKey();

                            if (key == "sha256") {
                                json->ParseString(&blob.sha256);
                            } else if (key == "name") {
                                json->ParseString(&blob.name);
                            } else {
                                json->UnexpectedKey(key);
                                valid = false;
                            }
                        }

                        blobs.Append(blob);
                    }
                } else if (key == "claim") {
                    json->ParseBool(&claim);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                valid &= CheckULID(tid);

                if (fragment.fs < 0 || !CheckULID(fragment.eid) || !fragment.store || !fragment.has_data) {
                    LogError("Missing or invalid fragment fields");
                    valid = false;
                }
                for (const char *tag: fragment.tags) {
                    valid &= CheckTag(tag);
                }

                for (const DataConstraint &constraint: constraints) {
                    valid &= CheckKey(constraint.key);
                }

                for (const CounterInfo &counter: counters) {
                    valid &= CheckKey(counter.key);

                    if (counter.max < 0 || counter.max > 64) {
                        LogError("Counter maximum must be between 1 and 64");
                        valid = false;
                    }
                }

                for (const char *key: publics) {
                    valid &= CheckKey(key);
                }

                if (signup.enable) {
                    bool content = signup.text.len || signup.html.len;

                    if (!signup.url || !signup.to || !signup.subject || !content) {
                        LogError("Missing signup fields");
                        valid = false;
                    }
                }

                for (const BlobInfo &blob: blobs) {
                    valid &= CheckSha256(blob.sha256);

                    if (!blob.name || !blob.name[0] || PathIsAbsolute(blob.name) || PathContainsDotDot(blob.name)) {
                        LogError("Invalid blob filename");
                        valid = false;
                    }
                }
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
    }

    // Safety checks
    if (signup.enable && session->userid < 0) {
        LogError("Cannot sign up from this session");
        io->SendError(403);
        return;
    }

    // Create full session for guests
    if (!session->userid) {
        const char *username = signup.enable ? signup.to : nullptr;

        session = MigrateGuestSession(io, instance, username);
        if (!session)
            return;

        stamp = session->GetStamp(instance);
        if (!stamp)
            return;

        K_ASSERT(session->userid < 0);
    }

    int64_t new_anchor = -1;

    bool success = instance->db->Transaction([&]() {
        int64_t now = GetUnixTime();

        // Check for existing thread and claim
        bool new_thread;
        bool claimed;
        {
            sq_Statement stmt;
            if (!instance->db->Prepare(R"(SELECT t.locked, IIF(c.userid IS NOT NULL, 1, 0) AS claimed
                                          FROM rec_threads t
                                          LEFT JOIN ins_claims c ON (c.tid = t.tid AND c.userid = ?2)
                                          WHERE t.tid = ?1)",
                                       &stmt, tid, -session->userid))
                return false;

            if (stmt.Step()) {
                bool locked = sqlite3_column_int(stmt, 0);

                if (locked) {
                    LogError("This record is locked");
                    io->SendError(403);
                    return false;
                }

                new_thread = false;
                claimed = sqlite3_column_int(stmt, 1);
            } else if (stmt.IsValid()) {
                new_thread = true;
                claimed = false;
            } else {
                return false;
            }
        }

        // Check for single TID mode
        if (stamp->single && !claimed) {
            sq_Statement stmt;
            if (!instance->db->Prepare("SELECT rowid FROM ins_claims WHERE userid = ?1",
                                       &stmt, -session->userid))
                return false;

            if (stmt.Step()) {
                if (signup.enable) {
                    LogError("Cannot create new registration");
                } else {
                    LogError("Cannot create new thread");
                }
                io->SendError(403);
                return false;
            } else if (!stmt.IsValid()) {
                return false;
            }
        }

        // Check for existing entry and check for lock or mismatch
        int64_t prev_anchor;
        {
            sq_Statement stmt;
            if (!instance->db->Prepare("SELECT tid, store, anchor FROM rec_entries e WHERE e.eid = ?1",
                                       &stmt, fragment.eid))
                return false;

            if (stmt.Step()) {
                const char *prev_tid = (const char *)sqlite3_column_text(stmt, 0);
                const char *prev_store = (const char *)sqlite3_column_text(stmt, 1);

                if (prev_tid && !TestStr(tid, prev_tid)) {
                    LogError("Record entry thread mismatch");
                    io->SendError(409);
                    return false;
                }
                if (prev_store && !TestStr(fragment.store, prev_store)) {
                    LogError("Record entry store mismatch");
                    io->SendError(409);
                    return false;
                }

                prev_anchor = sqlite3_column_int64(stmt, 2);
            } else if (stmt.IsValid()) {
                prev_anchor = -1;
            } else {
                return false;
            }

            if (fragment.anchor != prev_anchor) {
                LogError("Record entry version mismatch");
                io->SendError(409);
                return false;
            }
        }

        // List known counters
        HashSet<const char *> prev_counters;
        {
            sq_Statement stmt;
            if (!instance->db->Prepare(R"(SELECT c.key
                                          FROM rec_threads t, json_each(t.counters) c
                                          WHERE tid = ?1)", &stmt, tid))
                return false;

            while (stmt.Step()) {
                const char *key = (const char *)sqlite3_column_text(stmt, 0);
                const char *copy = DuplicateString(key, io->Allocator()).ptr;

                prev_counters.Set(copy);
            }
        }

        // Deal with thread claim
        if (!stamp->HasPermission(UserPermission::DataRead)) {
            if (new_thread) {
                if (!instance->db->Run(R"(INSERT INTO ins_claims (userid, tid) VALUES (?1, ?2)
                                          ON CONFLICT DO NOTHING)",
                                       -session->userid, tid))
                    return false;
            } else {
                if (!claimed) {
                    LogError("You are not allowed to alter this record");
                    io->SendError(403);
                    return false;
                }
            }
        }
        if (session->userid > 0 && signup.enable) {
            int64_t userid = CreateInstanceUser(instance, signup.to);

            if (!userid)
                return false;

            if (!instance->db->Run(R"(INSERT INTO ins_claims (userid, tid) VALUES (?1, ?2)
                                      ON CONFLICT DO NOTHING)",
                                   -userid, tid))
                return false;
        }

        // Apply constraints
        if (!instance->db->Run("DELETE FROM seq_constraints WHERE eid = ?1", fragment.eid))
            return false;
        for (const DataConstraint &constraint: constraints) {
            bool success = instance->db->Run(R"(INSERT INTO seq_constraints (eid, store, key, mandatory, value)
                                                VALUES (?1, ?2, ?3, ?4, json_extract(?5, '$.' || ?3)))",
                                             fragment.eid, fragment.store, constraint.key, 0 + constraint.exists, fragment.data);

            if (!success) {
                LogError("Empty or non-unique value for '%1'", constraint.key);
                io->SendError(409);
                return false;
            }
        }

        // Insert entry fragment
        {
            sq_Statement stmt;
            if (!instance->db->Prepare(R"(INSERT INTO rec_fragments (previous, tid, eid, userid, username,
                                                                     mtime, fs, summary, data, tags)
                                          VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)
                                          RETURNING anchor)",
                                       &stmt, prev_anchor > 0 ? sq_Binding(prev_anchor) : sq_Binding(), tid,
                                       fragment.eid, session->userid, session->username, now,
                                       fragment.fs, fragment.summary, fragment.data,
                                       TagsToJson(fragment.tags, io->Allocator())))
                return false;
            if (!stmt.GetSingleValue(&new_anchor))
                return false;
        }

        // Create or update store entry
        if (prev_anchor < 0) {
            if (!instance->db->Run(R"(INSERT INTO rec_entries (tid, eid, anchor, ctime, mtime, store,
                                                               deleted, summary, data, tags)
                                      VALUES (?1, ?2, ?3, ?4, ?4, ?5, ?6, ?7, ?8, ?9))",
                                   tid, fragment.eid, new_anchor, now, fragment.store,
                                   0 + !fragment.data.len, fragment.summary, fragment.data,
                                   TagsToJson(fragment.tags, io->Allocator())))
                return false;
        } else {
            if (!instance->db->Run(R"(UPDATE rec_entries SET anchor = ?2,
                                                             mtime = ?3,
                                                             deleted = ?4,
                                                             summary = ?5,
                                                             data = json_patch(data, ?6),
                                                             tags = ?7
                                      WHERE eid = ?1)",
                                   fragment.eid, new_anchor, now, 0 + !fragment.data.len,
                                   fragment.summary, fragment.data, TagsToJson(fragment.tags, io->Allocator())))
                return false;
        }

        // Create thread if needed
        if (new_thread && !instance->db->Run(R"(INSERT INTO rec_threads (tid, counters, secrets, locked)
                                                VALUES (?1, '{}', '{}', 0)
                                                ON CONFLICT DO NOTHING)", tid))
            return false;

        // Update entry and fragment tags
        if (!instance->db->Run("DELETE FROM rec_tags WHERE eid = ?1", fragment.eid))
            return false;
        for (const char *tag: fragment.tags) {
            if (!instance->db->Run(R"(INSERT INTO rec_tags (tid, eid, name) VALUES (?1, ?2, ?3)
                                      ON CONFLICT (eid, name) DO NOTHING)",
                                   tid, fragment.eid, tag))
                return false;
        }

        // Update counters
        for (CounterInfo &counter: counters) {
            if (prev_counters.Find(counter.key))
                continue;

            int64_t state;
            {
                sq_Statement stmt;
                if (!instance->db->Prepare("SELECT state FROM seq_counters WHERE key = ?1", &stmt, counter.key))
                    return false;

                if (stmt.Step()) {
                    state = sqlite3_column_int64(stmt, 0);
                } else if (stmt.IsValid()) {
                    state = 0;
                } else {
                    return false;
                }
            }

            int value = RunCounter(counter, state, &state);

            if (!instance->db->Run(R"(UPDATE rec_threads SET counters = json_patch(counters, json_object(?2, ?3)),
                                                             secrets = json_patch(secrets, json_object(?2, ?4))
                                      WHERE tid = ?1)",
                                   tid, counter.key, !counter.secret ? sq_Binding(value) : sq_Binding(),
                                                     counter.secret ? sq_Binding(value) : sq_Binding()))
                return false;
            if (!instance->db->Run(R"(INSERT INTO seq_counters (key, state)
                                      VALUES (?1, ?2)
                                      ON CONFLICT DO UPDATE SET state = excluded.state)",
                                   counter.key, state))
                return false;
        }

        // Update publics
        if (!instance->db->Run("DELETE FROM rec_publics WHERE eid = ?1", fragment.eid))
            return false;
        for (const char *key: publics) {
            if (!instance->db->Run(R"(INSERT INTO rec_publics (tid, eid, key, path)
                                      VALUES (?1, ?2, ?3, '$.' || replace(?4, '.', '.$v.') || '.$v'))",
                                   tid, fragment.eid, key, key))
                return false;
        }

        // Insert blobs
        for (const BlobInfo &blob: blobs) {
            if (!instance->db->Run(R"(INSERT INTO rec_files (tid, eid, anchor, sha256, name)
                                      VALUES (?1, ?2, ?3, ?4, ?5))",
                                   tid, fragment.eid, new_anchor, blob.sha256, blob.name)) {
                if (sqlite3_extended_errcode(*instance->db) == SQLITE_CONSTRAINT_FOREIGNKEY) {
                    LogError("Blob '%1' does not exist", blob.sha256);
                    io->SendError(409);
                }

                return false;
            }
        }

        // Delete claim if requested (and if any)
        if (!claim && !instance->db->Run("DELETE FROM ins_claims WHERE userid = ?1 AND tid = ?2",
                                         -session->userid, tid))
            return false;

        return true;
    });
    if (!success)
        return;

    // Best effort
    if (signup.enable && session->userid < 0) {
        do {
            smtp_MailContent content;

            if (!PrepareSignup(instance, session->username, signup, io->Allocator(), &content))
                break;
            SendMail(signup.to, content);

            LogDebug("Sent signup mail to '%1'", signup.to);
        } while (false);
    }

    const char *response = Fmt(io->Allocator(), "{ \"anchor\": %1 }", new_anchor).ptr;
    io->SendText(200, response, "application/json");
}

void HandleRecordDelete(http_IO *io, InstanceHolder *instance)
{
    if (!instance->settings.data_remote) {
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
    if (!stamp || !stamp->HasPermission(UserPermission::DataDelete)) {
        LogError("User is not allowed to delete data");
        io->SendError(403);
        return;
    }

    const char *tid = nullptr;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "tid") {
                    json->ParseString(&tid);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                valid &= CheckULID(tid);
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
    }

    bool success = instance->db->Transaction([&]() {
        int64_t now = GetUnixTime();

        // Get existing thread entries
        sq_Statement stmt;
        if (!instance->db->Prepare(R"(SELECT t.locked, IIF(c.userid IS NOT NULL, 1, 0) AS claim,
                                             e.rowid, e.eid, e.anchor, e.tags
                                      FROM rec_threads t
                                      LEFT JOIN ins_claims c ON (c.userid = ?1 AND c.tid = t.tid)
                                      INNER JOIN rec_entries e ON (e.tid = t.tid)
                                      WHERE t.tid = ?2 AND e.deleted = 0)", &stmt))
            return false;
        sqlite3_bind_int64(stmt, 1, -session->userid);
        sqlite3_bind_text(stmt, 2, tid, -1, SQLITE_STATIC);

        // Check for lock and claim (if needed)
        if (stmt.Step()) {
            bool locked = sqlite3_column_int(stmt, 0);
            bool claim = sqlite3_column_int(stmt, 1);

            if (!stamp->HasPermission(UserPermission::DataRead) && !claim) {
                LogError("Record does not exist");
                io->SendError(404);
                return false;
            }

            if (locked) {
                LogError("This record is locked");
                io->SendError(403);
                return false;
            }
        } else if (stmt.IsValid()) {
            LogError("Record does not exist");
            io->SendError(404);
            return false;
        } else {
            return false;
        }

        // Delete individual entries
        do {
            int64_t e = sqlite3_column_int64(stmt, 2);
            const char *eid = (const char *)sqlite3_column_text(stmt, 3);
            int64_t prev_anchor = sqlite3_column_int64(stmt, 4);
            const char *tags = (const char *)sqlite3_column_text(stmt, 5);

            int64_t new_anchor;
            {
                sq_Statement stmt;
                if (!instance->db->Prepare(R"(INSERT INTO rec_fragments (previous, tid, eid, userid, username,
                                                                         mtime, fs, summary, data, tags)
                                              VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)
                                              RETURNING anchor)",
                                           &stmt, prev_anchor, tid,
                                           eid, session->userid, session->username, now, nullptr,
                                           nullptr, nullptr, tags))
                    return false;
                if (!stmt.GetSingleValue(&new_anchor))
                    return false;
            }

            if (!instance->db->Run("UPDATE rec_entries SET deleted = 1, anchor = ?2 WHERE rowid = ?1",
                                   e, new_anchor))
                return false;

            if (!instance->db->Run("DELETE FROM seq_constraints WHERE eid = ?1", eid))
                return false;
        } while (stmt.Step());
        if (!stmt.IsValid())
            return false;

        // Delete thread itself
        {
            int64_t negative;

            sq_Statement stmt;
            if (!instance->db->Prepare("SELECT MIN(sequence) FROM rec_threads", &stmt))
                return false;
            if (!stmt.GetSingleValue(&negative))
                return false;

            negative = std::min(negative, (int64_t)0) - 1;

            if (!instance->db->Run(R"(UPDATE rec_threads SET deleted = sequence,
                                                             sequence = ?2
                                      WHERE tid = ?1)", tid, negative))
                return false;
        }

        // Restart sequence if no thread remains
        {
            sq_Statement stmt;
            if (!instance->db->Prepare("SELECT sequence FROM rec_threads WHERE sequence >= 0", &stmt))
                return false;

            if (!stmt.Step()) {
                if (!stmt.IsValid())
                    return false;
                if (!instance->db->Run("UPDATE sqlite_sequence SET seq = 0 WHERE name = 'rec_threads'"))
                    return false;
            }
        }

        return true;
    });
    if (!success)
        return;

    io->SendText(200, "{}", "application/json");
}

static void HandleLock(http_IO *io, InstanceHolder *instance, bool lock)
{
    if (!instance->settings.data_remote) {
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
    if (!stamp || !stamp->HasPermission(UserPermission::DataSave)) {
        if (lock) {
            LogError("User is not allowed to lock records");
        } else {
            LogError("User is not allowed to unlock records");
        }
        io->SendError(403);
        return;
    }

    char tid[64] = {};
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "tid") {
                    Span<const char> str = nullptr;

                    if (json->ParseString(&str)) {
                        CopyString(str, tid);
                    }
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                valid &= CheckULID(tid);
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
    }

    bool success = instance->db->Transaction([&]() {
        sq_Statement stmt;
        if (!instance->db->Prepare("SELECT t.locked FROM rec_threads t WHERE tid = ?1",
                                   &stmt, tid))
            return false;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Thread '%1' does not exist", tid);
                io->SendError(404);
            }
            return false;
        }

        bool locked = sqlite3_column_int(stmt, 0);

        if (locked && !stamp->HasPermission(UserPermission::DataAudit)) {
            LogError("User is not allowed to unlock records");
            io->SendError(403);
            return false;
        }

        if (!instance->db->Run("UPDATE rec_threads SET locked = ?2 WHERE tid = ?1",
                               tid, lock))
            return false;

        return true;
    });
    if (!success)
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleRecordLock(http_IO *io, InstanceHolder *instance)
{
    HandleLock(io, instance, true);
}

void HandleRecordUnlock(http_IO *io, InstanceHolder *instance)
{
    HandleLock(io, instance, false);
}

void HandleRecordPublic(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();

    if (!instance->settings.data_remote) {
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
    if (!session->HasPermission(instance, UserPermission::DataRead) &&
            !session->HasPermission(instance, UserPermission::DataSave)) {
        LogError("User is not allowed to read public values");
        io->SendError(403);
        return;
    }

    const char *store = request.GetQueryValue("store");
    if (!store) {
        LogError("Missing 'store' parameter");
        io->SendError(422);
        return;
    }

    sq_Statement stmt;
    if (!instance->db->Prepare(R"(SELECT e.rowid, p.key, json_quote(json_extract(e.data, p.path))
                                  FROM rec_entries e
                                  INNER JOIN rec_publics p ON (p.eid = e.eid)
                                  WHERE e.store = ?1 AND e.deleted = 0
                                  ORDER BY e.rowid)", &stmt, store))
        return;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();

        if (stmt.Step()) {
            do {
                int64_t e = sqlite3_column_int64(stmt, 0);

                json->StartObject();

                do {
                    const char *key = (const char *)sqlite3_column_text(stmt, 1);
                    const char *value = (const char *)sqlite3_column_text(stmt, 2);

                    if (value) {
                        json->Key(key); json->Raw(value);
                    } else {
                        json->Key(key); json->Null();
                    }
                } while (stmt.Step() && sqlite3_column_int64(stmt, 0) == e);

                json->EndObject();
            } while (stmt.IsRow());
        }
        if (!stmt.IsValid())
            return;

        json->EndArray();
    });
}

void HandleBlobGet(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();
    const char *url = request.path + 1 + instance->key.len;

    if (!instance->settings.data_remote) {
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
        LogError("User is not allowed to access blobs");
        io->SendError(403);
        return;
    }

    K_ASSERT(StartsWith(url, "/blobs/"));

    Span<const char> tid;
    const char *sha256;
    bool download;
    {
        Span<const char> remain = url + 7;

        tid = SplitStr(remain, '/', &remain);

        if (!CheckULID(tid)) {
            io->SendError(422);
            return;
        }

        if (EndsWith(remain, "/download")) {
            sha256 = DuplicateString(remain.Take(0, remain.len - 9), io->Allocator()).ptr;
            download = true;
        } else {
            sha256 = remain.ptr;
            download = false;
        }
    }

    // Get filename and check permission
    sq_Statement stmt;
    if (stamp->HasPermission(UserPermission::DataRead)) {
        if (!instance->db->Prepare(R"(SELECT f.name
                                      FROM rec_files f
                                      INNER JOIN rec_entries e ON (e.eid = f.eid AND e.anchor = f.anchor)
                                      WHERE f.tid = ?1 AND f.sha256 = ?2)",
                                   &stmt, tid, sha256))
            return;
    } else {
        if (!instance->db->Prepare(R"(SELECT f.name
                                      FROM rec_files f
                                      INNER JOIN rec_entries e ON (e.eid = f.eid AND e.anchor = f.anchor)
                                      INNER JOIN ins_claims c ON (c.tid = e.tid)
                                      WHERE f.tid = ?1 AND f.sha256 = ?2 AND c.userid = ?3)",
                                   &stmt, tid, sha256, session->userid))
            return;
    }

    // Not allowed or file does not exist
    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("File '%1' does not exist", sha256);
            io->SendError(404);
        }
        return;
    }

    const char *name = (const char *)sqlite3_column_text(stmt, 0);
    int64_t max_age = 28ll * 86400000;

    ServeFile(io, instance, sha256, name, download, max_age);
}

void HandleBlobPost(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();

    if (!instance->settings.data_remote) {
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
    if (!session->HasPermission(instance, UserPermission::DataSave)) {
        LogError("User is not allowed to save blobs");
        io->SendError(403);
        return;
    }

    const char *expect = request.GetQueryValue("sha256");
    CompressionType compression_type = CompressionType::None;
    const char *sha256 = nullptr;

    if (!PutFile(io, instance, compression_type, expect, &sha256))
        return;

    const char *response = Fmt(io->Allocator(), "{ \"sha256\": \"%1\" }", sha256).ptr;
    io->SendText(200, response, "application/json");
}

}
