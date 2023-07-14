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

struct DataConstraint {
    const char *key = nullptr;
    bool exists = false;
    bool unique = false;
};

struct RecordFragment {
    int64_t fs = -1;
    char eid[27] = {};
    const char *store = nullptr;
    int64_t anchor = -1;
    int64_t mtime = -1;
    bool has_data = false;
    Span<const char> data = {};
    Span<const char> meta = {};
    HeapArray<const char *> tags;
    HeapArray<DataConstraint> constraints;
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

static const char *TagsToJson(Span<const char *const> tags, Allocator *alloc)
{
    HeapArray<char> buf(alloc);

    if (!tags.len)
        return "[]";

    Fmt(&buf, "[");
    for (const char *tag: tags) {
        RG_ASSERT(CheckTag(tag));
        Fmt(&buf, "\"%1\", ", tag);
    }
    buf.len -= 2;
    Fmt(&buf, "]");

    return buf.Leak().ptr;
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
        RecordFragment fragment = {};
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
                } else if (key == "fragment") {
                    parser.ParseObject();
                    while (parser.InObject()) {
                        Span<const char> key = {};
                        parser.ParseKey(&key);

                        if (key == "fs") {
                            parser.ParseInt(&fragment.fs);
                        } else if (key == "eid") {
                            Span<const char> str = nullptr;

                            if (parser.ParseString(&str)) {
                                if (!CheckULID(str)) {
                                    io->AttachError(422);
                                    return;
                                }

                                CopyString(str, fragment.eid);
                            }
                        } else if (key == "store") {
                            parser.ParseString(&fragment.store);
                        } else if (key == "anchor") {
                            parser.ParseInt(&fragment.anchor);
                        } else if (key == "mtime") {
                            parser.ParseInt(&fragment.mtime);
                        } else if (key == "data") {
                            switch (parser.PeekToken()) {
                                case json_TokenType::Null: {
                                    fragment.data = {};
                                    fragment.has_data = true;
                                } break;
                                case json_TokenType::StartObject: {
                                    parser.PassThrough(&fragment.data);
                                    fragment.has_data = true;
                                } break;

                                default: {
                                    LogError("Unexpected value type for fragment data");
                                    io->AttachError(422);
                                    return;
                                } break;
                            }
                        } else if (key == "meta") {
                            switch (parser.PeekToken()) {
                                case json_TokenType::Null: { fragment.meta = {}; } break;
                                case json_TokenType::StartObject: { parser.PassThrough(&fragment.meta); } break;

                                default: {
                                    LogError("Unexpected value type for fragment notes");
                                    io->AttachError(422);
                                    return;
                                } break;
                            }
                        } else if (key == "tags") {
                            parser.ParseArray();
                            while (parser.InArray()) {
                                Span<const char> tag = {};

                                if (parser.ParseString(&tag)) {
                                    if (!CheckTag(tag)) {
                                        io->AttachError(422);
                                        return;
                                    }

                                    fragment.tags.Append(tag.ptr);
                                }
                            }
                        } else if (key == "constraints") {
                            parser.ParseObject();
                            while (parser.InObject()) {
                                DataConstraint constraint = {};

                                parser.ParseKey(&constraint.key);
                                parser.ParseObject();
                                while (parser.InObject()) {
                                    Span<const char> type = {};
                                    parser.ParseKey(&type);

                                    if (type == "exists") {
                                        parser.ParseBool(&constraint.exists);
                                    } else if (type == "unique") {
                                        parser.ParseBool(&constraint.unique);
                                    } else {
                                        if (parser.IsValid()) {
                                            LogError("Unknown constraint type '%1'", type);
                                        }
                                        io->AttachError(422);
                                        return;
                                    }
                                }

                                if (!CheckKey(constraint.key)) {
                                    io->AttachError(422);
                                    return;
                                }

                                fragment.constraints.Append(constraint);
                            }
                        } else if (parser.IsValid()) {
                            LogError("Unexpected key '%1'", key);
                            io->AttachError(422);
                            return;
                        }
                    }

                    if (!fragment.has_data) {
                        has_deletes = true;

                        fragment.meta = {};
                        fragment.tags.len = 0;
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
            if (fragment.fs < 0 || !fragment.eid[0] || !fragment.store || fragment.mtime < 0 || !fragment.has_data) {
                LogError("Missing fragment fields");
                valid = false;
            }

            if (!valid) {
                io->AttachError(422);
                return;
            }
        }

        bool success = instance->db->Transaction([&]() {
            // Get existing entry and check for mismatch
            int64_t prev_anchor;
            {
                sq_Statement stmt;
                if (!instance->db->Prepare("SELECT tid, store, anchor FROM rec_entries WHERE eid = ?1",
                                           &stmt, fragment.eid))
                    return false;

                if (stmt.Step()) {
                    const char *prev_tid = (const char *)sqlite3_column_text(stmt, 0);
                    const char *prev_store = (const char *)sqlite3_column_text(stmt, 1);

                    if (prev_tid && !TestStr(tid, prev_tid)) {
                        LogError("Record entry thread mismatch");
                        io->AttachError(409);
                        return false;
                    }
                    if (prev_store && !TestStr(fragment.store, prev_store)) {
                        LogError("Record entry store mismatch");
                        io->AttachError(409);
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
                    io->AttachError(409);
                    return false;
                }
            }

            // Check permissions
            if (prev_anchor < 0) {
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

            // Apply constraints
            if (!instance->db->Run("DELETE FROM seq_constraints WHERE eid = ?1", fragment.eid))
                return false;
            for (const DataConstraint &constraint: fragment.constraints) {
                bool success = instance->db->Run(R"(INSERT INTO seq_constraints (eid, store, key, mandatory, value)
                                                    VALUES (?1, ?2, ?3, ?4, json_extract(?5, '$.' || ?3)))",
                                                 fragment.eid, fragment.store, constraint.key, 0 + constraint.exists, fragment.data);

                if (!success) {
                    LogError("Empty or non-unique value for '%1'", constraint.key);
                    io->AttachError(409);
                    return false;
                }
            }

            // Insert entry fragment
            int64_t new_anchor;
            {
                sq_Statement stmt;
                if (!instance->db->Prepare(R"(INSERT INTO rec_fragments (previous, tid, eid, userid, username,
                                                                         mtime, fs, data, meta, tags)
                                              VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)
                                              RETURNING anchor)",
                                              &stmt, prev_anchor > 0 ? sq_Binding(prev_anchor) : sq_Binding(), tid,
                                              fragment.eid, session->userid, session->username, fragment.mtime, fragment.fs,
                                              fragment.data, fragment.meta, TagsToJson(fragment.tags, &io->allocator)))
                    return false;
                if (!stmt.GetSingleValue(&new_anchor))
                    return false;
            }

            // Create or update store entry
            int64_t e;
            {
                sq_Statement stmt;
                if (!instance->db->Prepare(R"(INSERT INTO rec_entries (tid, eid, anchor, ctime, mtime,
                                                                       store, sequence, deleted, data, meta, tags)
                                              VALUES (?1, ?2, ?3, ?4, ?4, ?5, -1, ?6, ?7, ?8, ?9)
                                              ON CONFLICT DO UPDATE SET anchor = excluded.anchor,
                                                                        mtime = excluded.mtime,
                                                                        deleted = excluded.deleted,
                                                                        data = json_patch(data, excluded.data),
                                                                        meta = excluded.meta,
                                                                        tags = excluded.tags
                                              RETURNING rowid)",
                                           &stmt, tid, fragment.eid, new_anchor, fragment.mtime, fragment.store,
                                           0 + !fragment.data.len, fragment.data, fragment.meta,
                                           TagsToJson(fragment.tags, &io->allocator)))
                    return false;
                if (!stmt.GetSingleValue(&e))
                    return false;
            }

            // Deal with per-store sequence number
            if (prev_anchor < 0) {
                int64_t counter;
                {
                    sq_Statement stmt;
                    if (!instance->db->Prepare(R"(INSERT INTO seq_counters (type, key, counter)
                                                  VALUES ('record', ?1, 1)
                                                  ON CONFLICT (type, key) DO UPDATE SET counter = counter + 1
                                                  RETURNING counter)",
                                               &stmt, fragment.store))
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
                                   tid, fragment.store, !fragment.data.len))
                return false;

            // Update entry and fragment tags
            if (!instance->db->Run("DELETE FROM rec_tags WHERE eid = ?1", fragment.eid))
                return false;
            for (const char *tag: fragment.tags) {
                if (!instance->db->Run(R"(INSERT INTO rec_tags (tid, eid, name) VALUES (?1, ?2, ?3)
                                          ON CONFLICT (eid, name) DO NOTHING)", tid, fragment.eid, tag))
                    return false;
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
