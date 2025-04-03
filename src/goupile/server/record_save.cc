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
#include "goupile.hh"
#include "domain.hh"
#include "instance.hh"
#include "record.hh"
#include "user.hh"
#include "src/core/request/smtp.hh"
#include "src/core/wrap/json.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

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
    const char *summary = nullptr;
    bool has_data = false;
    Span<const char> data = {};
    Span<const char> meta = {};
    HeapArray<const char *> tags;
    bool claim = true;
};

struct CounterInfo {
    const char *key = nullptr;
    int max = 0;
    bool randomize = false;
    bool secret = false;
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

static int64_t RunCounter(const CounterInfo &counter, int64_t state, int64_t *out_state)
{
    if (counter.max) {
        RG_ASSERT(counter.max >= 1 && counter.max <= 64);

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

void HandleRecordSave(http_IO *io, InstanceHolder *instance)
{
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
    if (!stamp || !stamp->HasPermission(UserPermission::DataSave)) {
        LogError("User is not allowed to save data");
        io->SendError(403);
        return;
    }

    char tid[27];
    RecordFragment fragment = {};
    HeapArray<DataConstraint> constraints;
    HeapArray<CounterInfo> counters;
    {
        StreamReader st;
        if (!io->OpenForRead(Mebibytes(8), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "tid") {
                Span<const char> str = nullptr;

                if (parser.ParseString(&str)) {
                    if (!CheckULID(str)) {
                        io->SendError(422);
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
                                io->SendError(422);
                                return;
                            }

                            CopyString(str, fragment.eid);
                        }
                    } else if (key == "store") {
                        parser.ParseString(&fragment.store);
                    } else if (key == "anchor") {
                        parser.ParseInt(&fragment.anchor);
                    } else if (key == "summary") {
                        parser.SkipNull() || parser.ParseString(&fragment.summary);
                    } else if (key == "data") {
                        switch (parser.PeekToken()) {
                            case json_TokenType::Null: {
                                parser.ParseNull();

                                fragment.data = {};
                                fragment.has_data = true;
                            } break;
                            case json_TokenType::StartObject: {
                                parser.PassThrough(&fragment.data);
                                fragment.has_data = true;
                            } break;

                            default: {
                                LogError("Unexpected value type for fragment data");
                                io->SendError(422);
                                return;
                            } break;
                        }
                    } else if (key == "meta") {
                        switch (parser.PeekToken()) {
                            case json_TokenType::Null: {
                                parser.ParseNull();
                                fragment.meta = {}; 
                            } break;
                            case json_TokenType::StartObject: { parser.PassThrough(&fragment.meta); } break;

                            default: {
                                LogError("Unexpected value type for fragment notes");
                                io->SendError(422);
                                return;
                            } break;
                        }
                    } else if (key == "tags") {
                        parser.ParseArray();
                        while (parser.InArray()) {
                            Span<const char> tag = {};

                            if (parser.ParseString(&tag)) {
                                if (!CheckTag(tag)) {
                                    io->SendError(422);
                                    return;
                                }

                                fragment.tags.Append(tag.ptr);
                            }
                        }
                    } else if (parser.IsValid()) {
                        LogError("Unexpected key '%1'", key);
                        io->SendError(422);
                        return;
                    }
                }
            } else if (key == "claim") {
                parser.ParseBool(&fragment.claim);
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
                            LogError("Unexpected key '%1'", key);
                            io->SendError(422);
                            return;
                        }
                    }

                    constraints.Append(constraint);
                }
            } else if (key == "counters") {
                parser.ParseObject();
                while (parser.InObject()) {
                    CounterInfo counter = {};

                    parser.ParseKey(&counter.key);
                    parser.ParseObject();
                    while (parser.InObject()) {
                        Span<const char> key = {};
                        parser.ParseKey(&key);

                        if (key == "key") {
                            parser.ParseString(&counter.key);
                        } else if (key == "max") {
                            parser.SkipNull() || parser.ParseInt(&counter.max);
                        } else if (key == "randomize") {
                            parser.ParseBool(&counter.randomize);
                        } else if (key == "secret") {
                            parser.ParseBool(&counter.secret);
                        } else {
                            LogError("Unexpected key '%1'", key);
                            io->SendError(422);
                            return;
                        }
                    }

                    counters.Append(counter);
                }
            } else if (parser.IsValid()) {
                LogError("Unexpected key '%1'", key);
                io->SendError(422);
                return;
            }
        }
        if (!parser.IsValid()) {
            io->SendError(422);
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
        if (fragment.fs < 0 || !fragment.eid[0] || !fragment.store || !fragment.has_data) {
            LogError("Missing fragment fields");
            valid = false;
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

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    // Create full session for guests
    if (!session->userid) {
        session = MigrateGuestSession(io, instance);
        if (!session)
            return;

        stamp = session->GetStamp(instance);
        if (!stamp)
            return;

        RG_ASSERT(session->userid < 0);
    }

    int64_t new_anchor = -1;

    bool success = instance->db->Transaction([&]() {
        int64_t now = GetUnixTime();

        // Get existing entry and check for lock or mismatch
        int64_t prev_anchor;
        {
            sq_Statement stmt;
            if (!instance->db->Prepare(R"(SELECT t.locked, e.tid, e.store, e.anchor
                                          FROM rec_entries e
                                          INNER JOIN rec_threads t ON (t.tid = e.tid)
                                          WHERE e.eid = ?1)",
                                       &stmt, fragment.eid))
                return false;

            if (stmt.Step()) {
                bool locked = sqlite3_column_int(stmt, 0);
                const char *prev_tid = (const char *)sqlite3_column_text(stmt, 1);
                const char *prev_store = (const char *)sqlite3_column_text(stmt, 2);

                if (locked) {
                    LogError("This record is locked");
                    io->SendError(403);
                    return false;
                }

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

                prev_anchor = sqlite3_column_int64(stmt, 3);
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

        // Check permissions
        if (!stamp->HasPermission(UserPermission::DataRead)) {
            if (prev_anchor < 0) {
                if (!instance->db->Run(R"(INSERT INTO ins_claims (userid, tid) VALUES (?1, ?2)
                                          ON CONFLICT DO NOTHING)",
                                       -session->userid, tid))
                    return false;
            } else {
                sq_Statement stmt;
                if (!instance->db->Prepare(R"(SELECT e.rowid
                                              FROM rec_entries e
                                              INNER JOIN ins_claims c ON (c.userid = ?1 AND c.tid = e.tid)
                                              WHERE e.tid = ?2)", &stmt))
                    return false;
                sqlite3_bind_int64(stmt, 1, -session->userid);
                sqlite3_bind_text(stmt, 2, tid, -1, SQLITE_STATIC);

                if (!stmt.Step()) {
                    if (stmt.IsValid()) {
                        LogError("You are not allowed to alter this record");
                        io->SendError(403);
                    }
                    return false;
                }
            }
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
                                                                     mtime, fs, summary, data, meta, tags)
                                          VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)
                                          RETURNING anchor)",
                                          &stmt, prev_anchor > 0 ? sq_Binding(prev_anchor) : sq_Binding(), tid,
                                          fragment.eid, session->userid, session->username, now,
                                          fragment.fs, fragment.summary, fragment.data,
                                          fragment.meta, TagsToJson(fragment.tags, io->Allocator())))
                return false;
            if (!stmt.GetSingleValue(&new_anchor))
                return false;
        }

        // Create or update store entry
        int64_t e;
        {
            sq_Statement stmt;
            if (!instance->db->Prepare(R"(INSERT INTO rec_entries (tid, eid, anchor, ctime, mtime, store,
                                                                   deleted, summary, data, meta, tags)
                                          VALUES (?1, ?2, ?3, ?4, ?4, ?5, ?6, ?7, ?8, ?9, ?10)
                                          ON CONFLICT DO UPDATE SET anchor = excluded.anchor,
                                                                    mtime = excluded.mtime,
                                                                    deleted = excluded.deleted,
                                                                    summary = excluded.summary,
                                                                    data = json_patch(data, excluded.data),
                                                                    meta = excluded.meta,
                                                                    tags = excluded.tags
                                          RETURNING rowid)",
                                       &stmt, tid, fragment.eid, new_anchor, now, fragment.store,
                                       0 + !fragment.data.len, fragment.summary, fragment.data,
                                       fragment.meta, TagsToJson(fragment.tags, io->Allocator())))
                return false;
            if (!stmt.GetSingleValue(&e))
                return false;
        }

        // Create thread if needed
        if (!instance->db->Run(R"(INSERT INTO rec_threads (tid, counters, secrets, locked)
                                  VALUES (?1, '{}', '{}', 0)
                                  ON CONFLICT DO NOTHING)", tid))
            return false;

        // Update entry and fragment tags
        if (!instance->db->Run("DELETE FROM rec_tags WHERE eid = ?1", fragment.eid))
            return false;
        for (const char *tag: fragment.tags) {
            if (!instance->db->Run(R"(INSERT INTO rec_tags (tid, eid, name) VALUES (?1, ?2, ?3)
                                      ON CONFLICT (eid, name) DO NOTHING)", tid, fragment.eid, tag))
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

        // Delete claim if requested (and if any)
        if (!fragment.claim) {
            if (!instance->db->Run("DELETE FROM ins_claims WHERE userid = ?1 AND tid = ?2",
                                   -session->userid, tid))
                return false;
        }

        return true;
    });
    if (!success)
        return;

    const char *json = Fmt(io->Allocator(), "{ \"anchor\": %1 }", new_anchor).ptr;
    io->SendText(200, json, "application/json");
}

void HandleRecordDelete(http_IO *io, InstanceHolder *instance)
{
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
    if (!stamp || !stamp->HasPermission(UserPermission::DataDelete)) {
        LogError("User is not allowed to delete data");
        io->SendError(403);
        return;
    }

    char tid[27];
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(64), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "tid") {
                Span<const char> str = nullptr;

                if (parser.ParseString(&str)) {
                    if (!CheckULID(str)) {
                        io->SendError(422);
                        return;
                    }

                    CopyString(str, tid);
                }
            } else if (parser.IsValid()) {
                LogError("Unexpected key '%1'", key);
                io->SendError(422);
                return;
            }
        }
        if (!parser.IsValid()) {
            io->SendError(422);
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

        if (!valid) {
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
                                                                         mtime, fs, summary, data, meta, tags)
                                              VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)
                                              RETURNING anchor)",
                                           &stmt, prev_anchor, tid,
                                           eid, session->userid, session->username, now, nullptr,
                                           nullptr, nullptr, nullptr, tags))
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

        return true;
    });
    if (!success)
        return;

    io->SendText(200, "{}", "application/json");
}

static void HandleLock(http_IO *io, InstanceHolder *instance, bool lock)
{
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
    if (!stamp || !stamp->HasPermission(UserPermission::DataSave)) {
        LogError("User is not allowed to %1 records", lock ? "lock" : "unlock");
        io->SendError(403);
        return;
    }

    char tid[27];
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(64), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "tid") {
                Span<const char> str = nullptr;

                if (parser.ParseString(&str)) {
                    if (!CheckULID(str)) {
                        io->SendError(422);
                        return;
                    }

                    CopyString(str, tid);
                }
            } else if (parser.IsValid()) {
                LogError("Unexpected key '%1'", key);
                io->SendError(422);
                return;
            }
        }
        if (!parser.IsValid()) {
            io->SendError(422);
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

        if (!valid) {
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

}
