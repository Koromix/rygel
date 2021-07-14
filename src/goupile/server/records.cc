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

#include "../../core/libcc/libcc.hh"
#include "domain.hh"
#include "goupile.hh"
#include "instance.hh"
#include "session.hh"
#include "../../core/libwrap/json.hh"
#ifdef _WIN32
    #include <io.h>
#else
    #include <unistd.h>
#endif

namespace RG {

void HandleRecordLoad(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
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
    if (!stamp || (!stamp->HasPermission(UserPermission::DataLoad) && !stamp->ulid)) {
        LogError("User is not allowed to load data");
        io->AttachError(403);
        return;
    }

    int64_t anchor;
    if (const char *str = request.GetQueryValue("anchor"); str) {
        if (!ParseInt(str, &anchor)) {
            io->AttachError(422);
            return;
        }
    } else {
        LogError("Missing 'userid' parameter");
        io->AttachError(422);
        return;
    }

    sq_Statement stmt;
    {
        LocalArray<char, 1024> sql;

        sql.len += Fmt(sql.TakeAvailable(), R"(SELECT e.rowid, e.ulid, e.hid, e.form, e.anchor,
                                                      e.parent_ulid, e.parent_version, f.anchor, f.version,
                                                      f.type, f.username, f.mtime, f.page, f.json FROM rec_entries e
                                               LEFT JOIN rec_fragments f ON (f.ulid = e.ulid)
                                               WHERE e.anchor >= ?1)").len;
        if (stamp->ulid) {
            sql.len += Fmt(sql.TakeAvailable(), " AND e.root_ulid = ?2").len;
        }
        sql.len += Fmt(sql.TakeAvailable(), " ORDER BY e.rowid, f.anchor").len;

        if (!instance->db->Prepare(sql.data, &stmt))
            return;

        sqlite3_bind_int64(stmt, 1, anchor);
        if (stamp->ulid) {
            sqlite3_bind_text(stmt, 2, stamp->ulid, -1, SQLITE_STATIC);
        }
    }

    // Export data
    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartArray();
    if (stmt.Step()) {
        do {
            int64_t rowid = sqlite3_column_int64(stmt, 0);

            json.StartObject();

            json.Key("ulid"); json.String((const char *)sqlite3_column_text(stmt, 1));
            json.Key("hid"); switch (sqlite3_column_type(stmt, 2)) {
                case SQLITE_NULL: { json.Null(); } break;
                case SQLITE_INTEGER: { json.Int64(sqlite3_column_int64(stmt, 2)); } break;
                default: { json.String((const char *)sqlite3_column_text(stmt, 2)); } break;
            }
            json.Key("form"); json.String((const char *)sqlite3_column_text(stmt, 3));
            json.Key("anchor"); json.Int64(sqlite3_column_int64(stmt, 4));
            if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
                json.Key("parent"); json.StartObject();
                json.Key("ulid"); json.String((const char *)sqlite3_column_text(stmt, 5));
                json.Key("version"); json.Int64(sqlite3_column_int64(stmt, 6));
                json.EndObject();
            } else {
                json.Key("parent"); json.Null();
            }

            json.Key("fragments"); json.StartArray();
            if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
                do {
                    json.StartObject();

                    const char *type = (const char *)sqlite3_column_text(stmt, 9);

                    json.Key("anchor"); json.Int64(sqlite3_column_int64(stmt, 7));
                    json.Key("version"); json.Int64(sqlite3_column_int64(stmt, 8));
                    json.Key("type"); json.String(type);
                    json.Key("username"); json.String((const char *)sqlite3_column_text(stmt, 10));
                    json.Key("mtime"); json.String((const char *)sqlite3_column_text(stmt, 11));
                    if (TestStr(type, "save")) {
                        json.Key("page"); json.String((const char *)sqlite3_column_text(stmt, 12));
                        json.Key("values"); json.Raw((const char *)sqlite3_column_text(stmt, 13));
                    }

                    json.EndObject();
                } while (stmt.Step() && sqlite3_column_int64(stmt, 0) == rowid);
            } else {
                stmt.Step();
            }
            json.EndArray();

            json.EndObject();
        } while (stmt.IsRow());
    }
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish();
}

struct SaveRecord {
    struct Fragment {
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
    HeapArray<Fragment> fragments;
};

void HandleRecordSave(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
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
    if (!stamp || (!stamp->HasPermission(UserPermission::DataSave) && !stamp->ulid)) {
        LogError("User is not allowed to save data");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        HeapArray<SaveRecord> records;

        // Parse records from JSON
        {
            StreamReader st;
            if (!io->OpenForRead(Megabytes(64), &st))
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
                        switch (parser.PeekToken()) {
                            case json_TokenType::Null: {
                                parser.ParseNull();
                                record->hid = nullptr;
                            } break;
                            case json_TokenType::Integer: {
                                int64_t value;
                                parser.ParseInt(&value);
                                record->hid = Fmt(&io->allocator, "%1", value).ptr;
                            } break;
                            default: {
                                parser.ParseString(&record->hid);
                            } break;
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
                                    io->AttachError(422);
                                    return;
                                }
                            }

                            if (RG_UNLIKELY(!record->parent.ulid || record->parent.version < 0)) {
                                LogError("Missing or invalid parent ULID or version");
                                io->AttachError(422);
                                return;
                            }
                        }
                    } else if (TestStr(key, "fragments")) {
                        parser.ParseArray();
                        while (parser.InArray()) {
                            SaveRecord::Fragment *fragment = record->fragments.AppendDefault();

                            parser.ParseObject();
                            while (parser.InObject()) {
                                const char *key = "";
                                parser.ParseKey(&key);

                                if (TestStr(key, "type")) {
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
                                    io->AttachError(422);
                                    return;
                                }
                            }

                            if (RG_UNLIKELY(!fragment->type || !fragment->mtime)) {
                                LogError("Missing type or mtime in fragment object");
                                io->AttachError(422);
                                return;
                            }
                            if (RG_UNLIKELY(!TestStr(fragment->type, "save") && !TestStr(fragment->type, "delete"))) {
                                LogError("Invalid fragment type '%1'", fragment->type);
                                io->AttachError(422);
                                return;
                            }
                            if (TestStr(fragment->type, "save") && (!fragment->page || !fragment->json.IsValid())) {
                                LogError("Fragment 'save' is missing page or JSON");
                                io->AttachError(422);
                                return;
                            }
                        }
                    } else if (parser.IsValid()) {
                        LogError("Unknown key '%1' in record object", key);
                        io->AttachError(422);
                        return;
                    }
                }

                if (RG_UNLIKELY(!record->form || !record->ulid)) {
                    LogError("Missing form or ULID in record object");
                    io->AttachError(422);
                    return;
                }
            }
            if (!parser.IsValid()) {
                io->AttachError(422);
                return;
            }
        }

        // Save to database
        bool success = instance->db->Transaction([&]() {
            for (const SaveRecord &record: records) {
                bool updated = false;

                // Retrieve root ULID
                char root_ulid[32];
                if (record.parent.ulid) {
                    sq_Statement stmt;
                    if (!instance->db->Prepare("SELECT root_ulid FROM rec_entries WHERE ulid = ?1", &stmt))
                        return false;
                    sqlite3_bind_text(stmt, 1, record.parent.ulid, -1, SQLITE_STATIC);

                    if (!stmt.Step()) {
                        if (stmt.IsValid()) {
                            LogError("Parent record '%1' does not exist", record.parent.ulid);
                        }
                        return false;
                    }

                    CopyString((const char *)sqlite3_column_text(stmt, 0), root_ulid);
                } else {
                    CopyString(record.ulid, root_ulid);
                }

                // Reject restricted users
                if (stamp->ulid && !TestStr(root_ulid, stamp->ulid)) {
                    LogError("You are not allowed to alter this record");
                    return false;
                }

                // Save record fragments
                int64_t anchor;
                if (record.fragments.len) {
                    for (Size i = 0; i < record.fragments.len; i++) {
                        const SaveRecord::Fragment &fragment = record.fragments[i];

                        if (!instance->db->Run(R"(INSERT INTO rec_fragments (ulid, version, type, userid, username,
                                                                             mtime, page, json)
                                                  VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)
                                                  ON CONFLICT DO NOTHING)",
                                              record.ulid, i + 1, fragment.type, session->userid, session->username,
                                              fragment.mtime, fragment.page, fragment.json))
                            return false;

                        if (sqlite3_changes(*instance->db)) {
                            updated = true;
                        } else {
                            LogDebug("Ignored conflicting fragment %1 for '%2'", i + 1, record.ulid);
                            continue;
                        }
                    }

                    anchor = sqlite3_last_insert_rowid(*instance->db);
                } else {
                    sq_Statement stmt;
                    if (!instance->db->Prepare("SELECT seq FROM sqlite_sequence WHERE name = 'rec_fragments'", &stmt))
                        return false;

                    if (stmt.Step()) {
                        anchor = sqlite3_column_int64(stmt, 0) + 1;
                    } else if (stmt.IsValid()) {
                        anchor = 1;
                    } else {
                        return false;
                    }

                    updated = true;
                }

                // Insert or update record entry (if needed)
                if (RG_LIKELY(updated)) {
                    if (!instance->db->Run(R"(INSERT INTO rec_entries (ulid, hid, form, parent_ulid,
                                                                       parent_version, root_ulid, anchor)
                                              VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)
                                              ON CONFLICT (ulid)
                                                  DO UPDATE SET hid = IFNULL(excluded.hid, hid),
                                                                anchor = excluded.anchor)",
                                          record.ulid, record.hid, record.form, record.parent.ulid,
                                          record.parent.version >= 0 ? sq_Binding(record.parent.version) : sq_Binding(),
                                          root_ulid, anchor))
                        return false;

                    if (sqlite3_changes(*instance->db) && !record.hid && !record.parent.ulid) {
                        int64_t rowid = sqlite3_last_insert_rowid(*instance->db);

                        sq_Statement stmt;
                        if (!instance->db->Prepare(R"(INSERT INTO rec_sequences (form, counter)
                                                      VALUES (?1, 1)
                                                      ON CONFLICT (form)
                                                          DO UPDATE SET counter = counter + 1
                                                      RETURNING counter)", &stmt))
                            return false;
                        sqlite3_bind_text(stmt, 1, record.form, -1, SQLITE_STATIC);

                        if (!stmt.Step()) {
                            RG_ASSERT(!stmt.IsValid());
                            return false;
                        }

                        int64_t counter = sqlite3_column_int64(stmt, 0);

                        if (!instance->db->Run("UPDATE rec_entries SET hid = ?2 WHERE rowid = ?1",
                                               rowid, counter))
                            return false;
                    }
                }
            }

            return true;
        });
        if (!success)
            return;

        io->AttachText(200, "Done!");
    });
}

struct ExportTable {
    enum class Type {
        Unknown,
        Integer,
        Double,
        String
    };

    struct Row {
        const char *ulid;
        const char *hid;
    };

    struct Column {
        const char *name;
        const char *escaped_name;

        Column *prev;
        Column *next;
        const char *prev_key;

        Type type;
        HeapArray<const char *> values;

        RG_HASHTABLE_HANDLER(Column, name);
    };

    const char *name;
    const char *escaped_name;

    HeapArray<Row> rows;

    BucketArray<Column> columns;
    HashTable<const char *, Column *> columns_map;
    Column *first_column;
    Column *last_column;
    HeapArray<const Column *> ordered_columns;

    RG_HASHTABLE_HANDLER(ExportTable, name);
};

static const char *EscapeSqlName(const char *name, Allocator *alloc)
{
    HeapArray<char> buf(alloc);

    buf.Append('"');
    for (Size i = 0; name[i]; i++) {
        char c = name[i];

        if (c == '"') {
            buf.Append("\"\"");
        } else {
            buf.Append(c);
        }
    }
    buf.Append('"');
    buf.Append(0);

    return buf.TrimAndLeak().ptr;
}

static ExportTable::Column *GetColumn(ExportTable *table, const char *key, const char *suffix,
                                      const char *prev_key, Allocator *alloc)
{
    const char *name;
    {
        HeapArray<char> buf(alloc);

        for (Size i = 0; key[i]; i++) {
            char c = LowerAscii(key[i]);
            buf.Append(c);
        }
        if (suffix) {
            buf.Append('.');
            for (Size i = 0; suffix[i]; i++) {
                char c = LowerAscii(key[i]);
                buf.Append(c);
            }
        }
        buf.Append(0);

        name = buf.TrimAndLeak().ptr;
    }

    ExportTable::Column *col = table->columns_map.FindValue(name, nullptr);

    if (!col) {
        col = table->columns.AppendDefault();
        col->name = name;

        table->columns_map.Set(col);

        if (table->columns.len > 1) {
            if (prev_key) {
                ExportTable::Column *it = table->columns_map.FindValue(prev_key, nullptr);

                if (it) {
                    ExportTable::Column *next = it->next;

                    while (next) {
                        if (!next->prev_key)
                            break;
                        if (!TestStr(next->prev_key, prev_key))
                            break;
                        if (CmpStr(next->name, name) > 0)
                            break;

                        it = next;
                        next = it->next;
                    }

                    if (it->next) {
                        it->next->prev = col;
                    }
                    col->next = it->next;
                    it->next = col;
                    col->prev = it;

                    table->last_column = col->next ? table->last_column : col;
                }
            }

            if (!col->prev) {
                col->prev = table->last_column;
                table->last_column->next = col;
                table->last_column = col;
            }
        } else {
            table->first_column = col;
            table->last_column = col;
        }
    }

    col->name = name;
    col->escaped_name = EscapeSqlName(name, alloc);
    col->prev_key = prev_key;
    col->values.AppendDefault(table->rows.len - col->values.len);

    return col;
}

static bool SkipObject(json_Parser *parser) {
    parser->ParseObject();

    int depth = 0;

    while (parser->InObject() || (depth-- && parser->InObject())) {
        const char *key = "";
        parser->ParseKey(&key);

        switch (parser->PeekToken()) {
            case json_TokenType::Null: { parser->ParseNull(); } break;
            case json_TokenType::Bool: {
                bool value = false;
                parser->ParseBool(&value);
            } break;
            case json_TokenType::Integer: {
                int64_t value = 0;
                parser->ParseInt(&value);
            } break;
            case json_TokenType::Double: {
                double value = 0.0;
                parser->ParseDouble(&value);
            } break;
            case json_TokenType::String: {
                const char *str = nullptr;
                parser->ParseString(&str);
            } break;

            case json_TokenType::StartObject: {
                parser->ParseObject();

                if (depth++ > 32) {
                    LogError("Excessive nesting of objects");
                    return false;
                }
            } break;

            default: {
                LogError("Unexpected JSON token type for '%1'", key);
                return false;
            } break;
        }
    }

    return true;
}

void HandleRecordExport(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
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
    if (!session->HasPermission(instance, UserPermission::DataExport)) {
        LogError("User is not allowed to export data");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        sq_Statement stmt;
        if (!instance->db->Prepare(R"(SELECT e.rowid, e.ulid, e.hid, e.form, e.anchor,
                                             e.parent_ulid, e.parent_version, f.anchor, f.version,
                                             f.type, f.username, f.mtime, f.page, f.json FROM rec_entries e
                                      INNER JOIN rec_fragments f ON (f.ulid = e.ulid)
                                      ORDER BY e.rowid, f.anchor)", &stmt))
            return;

        const char *export_filename = CreateTemporaryFile(gp_domain.config.tmp_directory, "", ".tmp", &io->allocator);
        RG_DEFER { UnlinkFile(export_filename); };

        // Prepare export file
        sq_Database db;
        if (!db.Open(export_filename, SQLITE_OPEN_READWRITE))
            return;

        BucketArray<ExportTable> tables;
        HashTable<const char *, ExportTable *> tables_map;
        HashSet<const char *> masked_columns;

        if (stmt.Step()) {
            do {
                int64_t rowid = sqlite3_column_int64(stmt, 0);
                const char *ulid = (const char *)sqlite3_column_text(stmt, 1);
                const char *hid = (const char *)sqlite3_column_text(stmt, 2);
                const char *form = (const char *)sqlite3_column_text(stmt, 3);
                bool deleted = false;

                // Create or find relevant table
                ExportTable *table = tables_map.FindValue(form, nullptr);
                if (!table) {
                    table = tables.AppendDefault();
                    table->name = DuplicateString(form, &io->allocator).ptr;
                    table->escaped_name = EscapeSqlName(form, &io->allocator);

                    tables_map.Set(table);
                }

                // Insert row metadata
                {
                    ExportTable::Row row = {};

                    row.ulid = DuplicateString(ulid, &io->allocator).ptr;
                    row.hid = DuplicateString(hid, &io->allocator).ptr;

                    table->rows.Append(row);
                }

                do {
                    const char *type = (const char *)sqlite3_column_text(stmt, 9);

                    if (TestStr(type, "save")) {
                        Span<const uint8_t> json = MakeSpan((const uint8_t *)sqlite3_column_blob(stmt, 13),
                                                            sqlite3_column_bytes(stmt, 13));
                        StreamReader reader(json, "<json>");
                        json_Parser parser(&reader, &io->allocator);

                        const char *prev_key = nullptr;

                        parser.ParseObject();
                        while (parser.InObject()) {
                            char buf[64];

                            const char *key = "";
                            parser.ParseKey(&key);

                            switch (parser.PeekToken()) {
                                case json_TokenType::Null: {
                                    parser.ParseNull();

                                    ExportTable::Column *col = GetColumn(table, key, nullptr, prev_key, &io->allocator);
                                    col->values[col->values.len - 1] = nullptr;
                                } break;
                                case json_TokenType::Bool: {
                                    bool value = false;
                                    parser.ParseBool(&value);

                                    ExportTable::Column *col = GetColumn(table, key, nullptr, prev_key, &io->allocator);
                                    col->type = (ExportTable::Type)std::max((int)col->type, (int)ExportTable::Type::Integer);
                                    col->values[col->values.len - 1] = value ? "1" : "0";
                                } break;
                                case json_TokenType::Integer: {
                                    int64_t value = 0;
                                    parser.ParseInt(&value);

                                    ExportTable::Column *col = GetColumn(table, key, nullptr, prev_key, &io->allocator);
                                    col->type = (ExportTable::Type)std::max((int)col->type, (int)ExportTable::Type::Integer);
                                    col->values[col->values.len - 1] = Fmt(&io->allocator, "%1", value).ptr;
                                } break;
                                case json_TokenType::Double: {
                                    double value = 0.0;
                                    parser.ParseDouble(&value);

                                    ExportTable::Column *col = GetColumn(table, key, nullptr, prev_key, &io->allocator);
                                    col->type = (ExportTable::Type)std::max((int)col->type, (int)ExportTable::Type::Double);
                                    col->values[col->values.len - 1] = Fmt(&io->allocator, "%1", value).ptr;
                                } break;
                                case json_TokenType::String: {
                                    const char *str = nullptr;
                                    parser.ParseString(&str);

                                    ExportTable::Column *col = GetColumn(table, key, nullptr, prev_key, &io->allocator);
                                    col->type = (ExportTable::Type)std::max((int)col->type, (int)ExportTable::Type::String);
                                    col->values[col->values.len - 1] = DuplicateString(str, &io->allocator).ptr;
                                } break;

                                case json_TokenType::StartArray: {
                                    masked_columns.Set(key);

                                    parser.ParseArray();
                                    while (parser.InArray()) {
                                        switch (parser.PeekToken()) {
                                            case json_TokenType::Null: {
                                                parser.ParseNull();

                                                ExportTable::Column *col = GetColumn(table, key, "null", prev_key, &io->allocator);
                                                col->type = (ExportTable::Type)std::max((int)col->type, (int)ExportTable::Type::Integer);
                                                col->values[col->values.len - 1] = "1";
                                            } break;
                                            case json_TokenType::Bool: {
                                                bool value = false;
                                                parser.ParseBool(&value);

                                                ExportTable::Column *col = GetColumn(table, key, value ? "1" : "0", prev_key, &io->allocator);
                                                col->type = (ExportTable::Type)std::max((int)col->type, (int)ExportTable::Type::Integer);
                                                col->values[col->values.len - 1] = "1";
                                            } break;
                                            case json_TokenType::Integer: {
                                                int64_t value = 0;
                                                parser.ParseInt(&value);

                                                const char *str = Fmt(buf, "%1", value).ptr;

                                                ExportTable::Column *col = GetColumn(table, key, str, prev_key, &io->allocator);
                                                col->type = (ExportTable::Type)std::max((int)col->type, (int)ExportTable::Type::Integer);
                                                col->values[col->values.len - 1] = "1";
                                            } break;
                                            case json_TokenType::Double: {
                                                double value = 0.0;
                                                parser.ParseDouble(&value);

                                                const char *str = Fmt(buf, "%1", value).ptr;

                                                ExportTable::Column *col = GetColumn(table, key, str, prev_key, &io->allocator);
                                                col->type = (ExportTable::Type)std::max((int)col->type, (int)ExportTable::Type::Integer);
                                                col->values[col->values.len - 1] = "1";
                                            } break;
                                            case json_TokenType::String: {
                                                const char *str = nullptr;
                                                parser.ParseString(&str);

                                                ExportTable::Column *col = GetColumn(table, key, str, prev_key, &io->allocator);
                                                col->type = (ExportTable::Type)std::max((int)col->type, (int)ExportTable::Type::String);
                                                col->values[col->values.len - 1] = "1";
                                            } break;

                                            default: {
                                                LogError("The exporter does not support arrays of objects");
                                                return;
                                            } break;
                                        }
                                    }
                                } break;

                                case json_TokenType::StartObject: {
                                    LogError("Skipping complex object '%1' in export", key);
                                    if (!SkipObject(&parser))
                                        return;
                                } break;

                                default: {
                                    if (parser.IsValid()) {
                                        LogError("Unexpected JSON token type for '%1'", key);
                                    }
                                    return;
                                } break;
                            }

                            prev_key = key;
                        }

                        deleted = !parser.IsValid();
                    } else if (TestStr(type, "delete")) {
                        deleted = true;
                    }
                } while (stmt.Step() && sqlite3_column_int64(stmt, 0) == rowid);

                if (!deleted) {
                    for (ExportTable::Column &col: table->columns) {
                        col.values.AppendDefault(table->rows.len - col.values.len);
                    }
                } else {
                    table->rows.len--;

                    for (ExportTable::Column &col: table->columns) {
                        col.values.len = std::min(table->rows.len, col.values.len);
                    }
                }
            } while (stmt.IsRow());
        }
        if (!stmt.IsValid())
            return;

        // Reorder columns
        for (ExportTable &table: tables) {
            const ExportTable::Column *it = table.first_column;

            while (it) {
                if (!masked_columns.Find(it->name)) {
                    table.ordered_columns.Append(it);
                }
                it = it->next;
            }
        }

        // Create tables
        for (const ExportTable &table: tables) {
            HeapArray<char> sql(&io->allocator);

            Fmt(&sql, "CREATE TABLE %1 (__ulid TEXT, __hid TEXT, ", table.escaped_name);
            for (const ExportTable::Column *col: table.ordered_columns) {
                switch (col->type) {
                    case ExportTable::Type::Unknown: { Fmt(&sql, "%1, ", col->escaped_name); } break;
                    case ExportTable::Type::Integer: { Fmt(&sql, "%1 INTEGER, ", col->escaped_name); } break;
                    case ExportTable::Type::Double: { Fmt(&sql, "%1 REAL, ", col->escaped_name); } break;
                    case ExportTable::Type::String: { Fmt(&sql, "%1 TEXT, ", col->escaped_name); } break;
                }
            }
            sql.len -= 2;
            Fmt(&sql, ")");

            if (!db.Run(sql.ptr))
                return;
        }

        // Import data
        for (const ExportTable &table: tables) {
            HeapArray<char> sql(&io->allocator);

            Fmt(&sql, "INSERT INTO %1 VALUES (?1, ?2, ", table.escaped_name);
            for (Size i = 0; i < table.ordered_columns.len; i++) {
                Fmt(&sql, "?%1, ", i + 3);
            }
            sql.len -= 2;
            Fmt(&sql, ")");

            sq_Statement stmt;
            if (!db.Prepare(sql.ptr, &stmt))
                return;

            for (Size i = 0; i < table.rows.len; i++) {
                stmt.Reset();

                sqlite3_bind_text(stmt, 1, table.rows[i].ulid, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, table.rows[i].hid, -1, SQLITE_STATIC);
                for (Size j = 0; j < table.ordered_columns.len; j++) {
                    const ExportTable::Column *col = table.ordered_columns[j];
                    sqlite3_bind_text(stmt, (int)j + 3, col->values[i], -1, SQLITE_STATIC);
                }

                if (!stmt.Run())
                    return;
            }
        }

        if (!db.Close())
            return;

        if (!io->AttachFile(200, export_filename))
            return;

        // Ask browser to download
        {
            int64_t time = GetUnixTime();
            const char *disposition = Fmt(&io->allocator, "attachment; filename=\"%1_%2.db\"",
                                                          instance->key, FmtTimeISO(DecomposeTime(time))).ptr;
            io->AddHeader("Content-Disposition", disposition);
        }
    });
}

}
