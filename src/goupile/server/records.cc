// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "goupile.hh"
#include "instance.hh"
#include "user.hh"
#include "../../core/libwrap/json.hh"

namespace RG {

static void ExportRecord(sq_Statement *stmt, json_Writer *json)
{
    int64_t rowid = sqlite3_column_int64(*stmt, 0);

    json->StartObject();

    json->Key("ulid"); json->String((const char *)sqlite3_column_text(*stmt, 1));
    if (sqlite3_column_type(*stmt, 2) != SQLITE_NULL) {
        json->Key("hid"); json->String((const char *)sqlite3_column_text(*stmt, 2));
    } else {
        json->Key("hid"); json->Null();
    }
    json->Key("form"); json->String((const char *)sqlite3_column_text(*stmt, 3));
    if (sqlite3_column_type(*stmt, 4) != SQLITE_NULL) {
        json->Key("parent"); json->StartObject();
        json->Key("ulid"); json->String((const char *)sqlite3_column_text(*stmt, 4));
        json->Key("version"); json->Int64(sqlite3_column_int64(*stmt, 5));
        json->EndObject();
    } else {
        json->Key("parent"); json->Null();
    }
    if (sqlite3_column_type(*stmt, 6) != SQLITE_NULL) {
        json->Key("zone"); json->String((const char *)sqlite3_column_text(*stmt, 6));
    } else {
        json->Key("zone"); json->Null();
    }

    json->Key("fragments"); json->StartArray();
    do {
        json->StartObject();

        const char *type = (const char *)sqlite3_column_text(*stmt, 9);

        json->Key("anchor"); json->Int64(sqlite3_column_int64(*stmt, 7));
        json->Key("version"); json->Int64(sqlite3_column_int64(*stmt, 8));
        json->Key("type"); json->String(type);
        json->Key("username"); json->String((const char *)sqlite3_column_text(*stmt, 10));
        json->Key("mtime"); json->String((const char *)sqlite3_column_text(*stmt, 11));
        if (TestStr(type, "save")) {
            json->Key("page"); json->String((const char *)sqlite3_column_text(*stmt, 12));
            json->Key("values"); json->Raw((const char *)sqlite3_column_text(*stmt, 13));
        }

        json->EndObject();
    } while (stmt->Next() && sqlite3_column_int64(*stmt, 0) == rowid);
    json->EndArray();

    json->EndObject();
}

void HandleRecordLoad(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);
    const Token *token = session ? session->GetToken(instance) : nullptr;
    if (!token) {
        LogError("User is not allowed to load data");
        io->AttachError(403);
        return;
    }

    const char *form = request.GetQueryValue("form");
    const char *ulid = request.GetQueryValue("ulid");
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

        sql.len += Fmt(sql.TakeAvailable(), R"(SELECT r.rowid, r.ulid, r.hid, r.form, r.parent_ulid, r.parent_version, r.zone,
                                                      f.anchor, f.version, f.type, f.username, f.mtime, f.page, f.json FROM rec_entries r
                                               INNER JOIN rec_fragments f ON (f.ulid = r.ulid)
                                               WHERE 1 = 1)").len;
        if (token->zone) {
            sql.len += Fmt(sql.TakeAvailable(), " AND (r.zone IS NULL OR r.zone == ?1)").len;
        }
        if (form) {
            sql.len += Fmt(sql.TakeAvailable(), " AND r.form = ?2").len;
        }
        if (ulid) {
            sql.len += Fmt(sql.TakeAvailable(), " AND r.ulid = ?3").len;
        }
        if (anchor) {
            sql.len += Fmt(sql.TakeAvailable(), " AND r.anchor >= ?4").len;
        }
        sql.len += Fmt(sql.TakeAvailable(), " ORDER BY r.rowid, f.anchor").len;

        if (!instance->db.Prepare(sql.data, &stmt))
            return;

        if (token->zone) {
            sqlite3_bind_text(stmt, 1, token->zone, -1, SQLITE_STATIC);
        }
        if (form) {
            sqlite3_bind_text(stmt, 2, form, -1, SQLITE_STATIC);
        }
        if (ulid) {
            sqlite3_bind_text(stmt, 3, ulid, -1, SQLITE_STATIC);
        }
        if (anchor) {
            sqlite3_bind_int64(stmt, 4, anchor);
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

struct SaveRecord {
    struct Fragment {
        int64_t version = -1;
        const char *type = nullptr;
        const char *mtime = nullptr;
        const char *page = nullptr;
        Span<const char> json = {};
    };

    const char *ulid = nullptr;
    const char *hid = nullptr;
    const char *form = nullptr;
    struct {
        const char *ulid = nullptr;
        int64_t version = -1;
    } parent;
    bool zoned = true;
    HeapArray<Fragment> fragments;
    int64_t version;
};

void HandleRecordSave(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
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
        HeapArray<SaveRecord> records;

        // Parse records from JSON
        {
            StreamReader st;
            if (!io->OpenForRead(Megabytes(2), &st))
                return;
            json_Parser parser(&st, &io->allocator);

            parser.ParseArray();
            while (parser.InArray()) {
                SaveRecord *record = records.AppendDefault();

                parser.ParseObject();
                while (parser.InObject()) {
                    const char *key = "";
                    parser.ParseKey(&key);

                    if (TestStr(key, "form")) {
                        parser.ParseString(&record->form);
                    } else if (TestStr(key, "ulid")) {
                        parser.ParseString(&record->ulid);
                    } else if (TestStr(key, "hid")) {
                        if (parser.PeekToken() == json_TokenType::Null) {
                            parser.ParseNull();
                            record->hid = nullptr;
                        } else {
                            parser.ParseString(&record->hid);
                        }
                    } else if (TestStr(key, "parent")) {
                        if (parser.PeekToken() == json_TokenType::Null) {
                            parser.ParseNull();
                            record->parent.ulid = nullptr;
                            record->parent.version = -1;
                        } else {
                            parser.ParseObject();
                            while (parser.InObject()) {
                                const char *key = "";
                                parser.ParseKey(&key);

                                if (TestStr(key, "ulid")) {
                                    parser.ParseString(&record->parent.ulid);
                                } else if (TestStr(key, "version")) {
                                    parser.ParseInt(&record->parent.version);
                                } else if (parser.IsValid()) {
                                    LogError("Unknown key '%1' in parent object", key);
                                    return;
                                }
                            }

                            if (RG_UNLIKELY(!record->parent.ulid || record->parent.version <= 0)) {
                                LogError("xxxx");
                                return;
                            }
                        }
                    } else if (TestStr(key, "zoned")) {
                        parser.ParseBool(&record->zoned);
                    } else if (TestStr(key, "fragments")) {
                        parser.ParseArray();
                        while (parser.InArray()) {
                            SaveRecord::Fragment *fragment = record->fragments.AppendDefault();

                            parser.ParseObject();
                            while (parser.InObject()) {
                                const char *key = "";
                                parser.ParseKey(&key);

                                if (TestStr(key, "version")) {
                                    parser.ParseInt(&fragment->version);
                                } else if (TestStr(key, "type")) {
                                    parser.ParseString(&fragment->type);
                                } else if (TestStr(key, "mtime")) {
                                    parser.ParseString(&fragment->mtime);
                                } else if (TestStr(key, "page")) {
                                    if (parser.PeekToken() == json_TokenType::Null) {
                                        parser.ParseNull();
                                        fragment->page = nullptr;
                                    } else {
                                        parser.ParseString(&fragment->page);
                                    }
                                } else if (TestStr(key, "json")) {
                                    parser.ParseString(&fragment->json);
                                } else if (parser.IsValid()) {
                                    LogError("Unknown key '%1' in fragment object", key);
                                    return;
                                }
                            }

                            if (RG_UNLIKELY(fragment->version <= 0 || !fragment->type || !fragment->mtime)) {
                                LogError("A");
                                return;
                            }
                            if (RG_UNLIKELY(!TestStr(fragment->type, "save") && !TestStr(fragment->type, "delete"))) {
                                LogError("B");
                                return;
                            }
                            if (TestStr(fragment->type, "save") && (!fragment->page || !fragment->json.IsValid())) {
                                LogError("C");
                                return;
                            }
                        }
                    } else if (parser.IsValid()) {
                        LogError("Unknown key '%1' in record object", key);
                        return;
                    }
                }

                if (RG_UNLIKELY(!record->form || !record->ulid)) {
                    LogError("C");
                    return;
                }
                if (RG_UNLIKELY(!record->fragments.len)) {
                    LogError("D");
                    return;
                }

                // XXX: CHECK ORDERING
                record->version = record->fragments[record->fragments.len - 1].version;
            }
            if (!parser.IsValid())
                return;
        }

        // Save to database
        bool success = instance->db.Transaction([&]() {
            for (const SaveRecord &record: records) {
                // Retrieve record version
                int version;
                {
                    sq_Statement stmt;
                    if (!instance->db.Prepare(R"(SELECT version, zone FROM rec_entries
                                                 WHERE ulid = ?1)", &stmt))
                        return false;
                    sqlite3_bind_text(stmt, 1, record.ulid, -1, SQLITE_STATIC);

                    if (stmt.Next()) {
                        /*if (token->zone && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                            const char *zone = (const char *)sqlite3_column_text(stmt, 0);
                            if (!TestStr(token->zone, zone)) {
                                LogError("Zone mismatch for %1", handle.id);
                                incomplete = true;
                                continue;
                            }
                        }*/

                        version = sqlite3_column_int(stmt, 0);
                    } else if (stmt.IsValid()) {
                        version = -1;
                    } else {
                        return false;
                    }
                }

                // Nothing new, skip!
                if (record.version <= version)
                    continue;

                // Save record fragments
                for (Size i = 0; i < record.fragments.len; i++) {
                    const SaveRecord::Fragment &fragment = record.fragments[i];

                    if (fragment.version <= version) {
                        LogError("Ignored conflicting fragment %1 for '%2'", fragment.version, record.ulid);
                        continue;
                    }

                    if (!instance->db.Run(R"(INSERT INTO rec_fragments (ulid, version, type, userid, username,
                                                                        mtime, page, json)
                                             VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8))",
                                          record.ulid, fragment.version, fragment.type, session->userid, session->username,
                                          fragment.mtime, fragment.page, fragment.json))
                        return false;
                }

                int64_t anchor = sqlite3_last_insert_rowid(instance->db);

                // Insert or update record entry
                if (!instance->db.Run(R"(INSERT INTO rec_entries (ulid, hid, form, parent_ulid, parent_version, version, zone, anchor)
                                         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)
                                         ON CONFLICT (ulid)
                                             DO UPDATE SET hid = excluded.hid,
                                                           version = excluded.version,
                                                           zone = excluded.zone,
                                                           anchor = excluded.anchor)",
                                      record.ulid, record.hid, record.form,
                                      record.parent.ulid, record.parent.version >= 0 ? sq_Binding(record.parent.version) : sq_Binding(),
                                      record.version, record.zoned ? nullptr : nullptr, anchor))
                    return false;
            }

            return true;
        });
        if (!success)
            return;

        io->AttachText(200, "Done!");
    });
}

}
