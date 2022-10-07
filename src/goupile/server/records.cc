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
    if (!stamp) {
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

        sql.len += Fmt(sql.TakeAvailable(), R"(SELECT e.rowid, e.ulid, e.form, e.sequence, e.hid, e.anchor,
                                                      e.parent_ulid, e.parent_version, f.anchor, f.version,
                                                      f.type, f.username, f.mtime, f.fs, f.page, f.json FROM rec_entries e
                                               LEFT JOIN rec_fragments f ON (f.ulid = e.ulid)
                                               WHERE e.anchor >= ?1)").len;
        if (!stamp->HasPermission(UserPermission::DataLoad)) {
            sql.len += Fmt(sql.TakeAvailable(), " AND e.root_ulid IN (SELECT ulid FROM ins_claims WHERE userid = ?2)").len;
        }
        sql.len += Fmt(sql.TakeAvailable(), " ORDER BY e.rowid, f.anchor").len;

        if (!instance->db->Prepare(sql.data, &stmt))
            return;

        sqlite3_bind_int64(stmt, 1, anchor);
        sqlite3_bind_int64(stmt, 2, -session->userid);
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
            json.Key("form"); json.String((const char *)sqlite3_column_text(stmt, 2));
            json.Key("sequence"); json.Int64(sqlite3_column_int64(stmt, 3));
            json.Key("hid"); switch (sqlite3_column_type(stmt, 4)) {
                case SQLITE_NULL: { json.Null(); } break;
                case SQLITE_INTEGER: { json.Int64(sqlite3_column_int64(stmt, 4)); } break;
                default: { json.String((const char *)sqlite3_column_text(stmt, 4)); } break;
            }
            json.Key("anchor"); json.Int64(sqlite3_column_int64(stmt, 5));
            if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
                json.Key("parent"); json.StartObject();
                json.Key("ulid"); json.String((const char *)sqlite3_column_text(stmt, 6));
                json.Key("version"); json.Int64(sqlite3_column_int64(stmt, 7));
                json.EndObject();
            } else {
                json.Key("parent"); json.Null();
            }

            json.Key("fragments"); json.StartArray();
            if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
                do {
                    json.StartObject();

                    const char *type = (const char *)sqlite3_column_text(stmt, 10);

                    json.Key("anchor"); json.Int64(sqlite3_column_int64(stmt, 8));
                    json.Key("version"); json.Int64(sqlite3_column_int64(stmt, 9));
                    json.Key("type"); json.String(type);
                    json.Key("username"); json.String((const char *)sqlite3_column_text(stmt, 11));
                    json.Key("mtime"); json.String((const char *)sqlite3_column_text(stmt, 12));
                    json.Key("fs"); json.Int64(sqlite3_column_int64(stmt, 13));
                    if (TestStr(type, "save")) {
                        json.Key("page"); json.String((const char *)sqlite3_column_text(stmt, 14));
                        json.Key("values"); json.Raw((const char *)sqlite3_column_text(stmt, 15));
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
        int64_t fs = -1;
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
    bool deleted = false;
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
    if (!stamp || !stamp->HasPermission(UserPermission::DataSave)) {
        LogError("User is not allowed to save data");
        io->AttachError(403);
        return;
    }

    if (!session->userid) {
        RG_ASSERT(session->type == SessionType::Auto);

        session = MigrateGuestSession(*session, instance, request, io);
        if (!session)
            return;
        stamp = session->GetStamp(instance);
        if (!stamp)
            return;

        RG_ASSERT(session->userid < 0);
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
                    Span<const char> key = {};
                    parser.ParseKey(&key);

                    if (key == "form") {
                        parser.ParseString(&record->form);
                    } else if (key == "ulid") {
                        parser.ParseString(&record->ulid);
                    } else if (key == "hid") {
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
                    } else if (key == "parent") {
                        if (parser.PeekToken() == json_TokenType::Null) {
                            parser.ParseNull();
                            record->parent.ulid = nullptr;
                            record->parent.version = -1;
                        } else {
                            parser.ParseObject();
                            while (parser.InObject()) {
                                Span<const char> key2 = {};
                                parser.ParseKey(&key2);

                                if (key2 == "ulid") {
                                    parser.ParseString(&record->parent.ulid);
                                } else if (key2 == "version") {
                                    parser.ParseInt(&record->parent.version);
                                } else if (parser.IsValid()) {
                                    LogError("Unknown key '%1' in parent object", key2);
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
                    } else if (key == "fragments") {
                        parser.ParseArray();
                        while (parser.InArray()) {
                            SaveRecord::Fragment *fragment = record->fragments.AppendDefault();

                            parser.ParseObject();
                            while (parser.InObject()) {
                                Span<const char> key2 = {};
                                parser.ParseKey(&key2);

                                if (key2 == "type") {
                                    parser.ParseString(&fragment->type);
                                } else if (key2 == "mtime") {
                                    parser.ParseString(&fragment->mtime);
                                } else if (key2 == "fs") {
                                    parser.ParseInt(&fragment->fs);
                                } else if (key2 == "page") {
                                    if (parser.PeekToken() == json_TokenType::Null) {
                                        parser.ParseNull();
                                        fragment->page = nullptr;
                                    } else {
                                        parser.ParseString(&fragment->page);
                                    }
                                } else if (key2 == "json") {
                                    parser.ParseString(&fragment->json);
                                } else if (parser.IsValid()) {
                                    LogError("Unknown key '%1' in fragment object", key2);
                                    io->AttachError(422);
                                    return;
                                }
                            }

                            if (RG_UNLIKELY(!fragment->type || !fragment->mtime || fragment->fs < 0)) {
                                LogError("Missing type, mtime or FS in fragment object");
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

                record->deleted = record->fragments.len &&
                                  TestStr(record->fragments[record->fragments.len - 1].type, "deleted");
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
                            continue;
                        } else {
                            return false;
                        }
                    }

                    CopyString((const char *)sqlite3_column_text(stmt, 0), root_ulid);
                } else {
                    CopyString(record.ulid, root_ulid);
                }

                // Reject restricted users
                if (!stamp->HasPermission(UserPermission::DataLoad)) {
                    sq_Statement stmt;
                    if (!instance->db->Prepare(R"(SELECT e.rowid, c.rowid FROM rec_entries e
                                                  LEFT JOIN ins_claims c ON (c.userid = ?1 AND c.ulid = e.ulid)
                                                  WHERE e.ulid = ?2)", &stmt))
                        return false;
                    sqlite3_bind_int64(stmt, 1, -session->userid);
                    sqlite3_bind_text(stmt, 2, root_ulid, -1, SQLITE_STATIC);

                    if (stmt.Step()) {
                        if (sqlite3_column_type(stmt, 1) != SQLITE_INTEGER) {
                            LogError("You are not allowed to alter this record");
                            return false;
                        }
                    } else if (stmt.IsValid()) {
                        if (!instance->db->Run(R"(INSERT INTO ins_claims (userid, ulid) VALUES (?1, ?2)
                                                  ON CONFLICT DO NOTHING)",
                                               -session->userid, root_ulid))
                            return false;
                    } else {
                        return false;
                    }
                }

                // Save record fragments
                int64_t anchor;
                if (record.fragments.len) {
                    for (Size i = 0; i < record.fragments.len; i++) {
                        const SaveRecord::Fragment &fragment = record.fragments[i];

                        if (!instance->db->Run(R"(INSERT INTO rec_fragments (ulid, version, type, userid,
                                                                             username, mtime, fs, page, json)
                                                  VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)
                                                  ON CONFLICT DO NOTHING)",
                                              record.ulid, i + 1, fragment.type, session->userid, session->username,
                                              fragment.mtime, fragment.fs, fragment.page, fragment.json))
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
                    if (!instance->db->Run(R"(INSERT INTO rec_entries (ulid, sequence, hid, form, parent_ulid,
                                                                       parent_version, root_ulid, anchor, deleted)
                                              VALUES (?1, -1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)
                                              ON CONFLICT (ulid) DO UPDATE SET hid = IFNULL(excluded.hid, hid),
                                                                               anchor = excluded.anchor,
                                                                               deleted = excluded.deleted)",
                                          record.ulid, record.hid, record.form, record.parent.ulid,
                                          record.parent.version >= 0 ? sq_Binding(record.parent.version) : sq_Binding(),
                                          root_ulid, anchor, record.deleted))
                        return false;

                    if (sqlite3_changes(*instance->db)) {
                        int64_t rowid = sqlite3_last_insert_rowid(*instance->db);

                        sq_Statement stmt;
                        if (!instance->db->Prepare(R"(INSERT INTO seq_counters (type, key, counter)
                                                      VALUES ('record', ?1, 1)
                                                      ON CONFLICT (type, key) DO UPDATE SET counter = counter + 1
                                                      RETURNING counter)", &stmt))
                            return false;
                        sqlite3_bind_text(stmt, 1, record.form, -1, SQLITE_STATIC);

                        if (!stmt.Step()) {
                            RG_ASSERT(!stmt.IsValid());
                            return false;
                        }

                        int64_t counter = sqlite3_column_int64(stmt, 0);

                        if (record.hid) {
                            if (!instance->db->Run("UPDATE rec_entries SET sequence = ?2 WHERE rowid = ?1",
                                                   rowid, counter))
                                return false;
                        } else {
                            if (!instance->db->Run("UPDATE rec_entries SET sequence = ?2, hid = ?2 WHERE rowid = ?1",
                                                   rowid, counter, counter))
                                return false;
                        }
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

class RecordExporter {
    enum class Type {
        Unknown,
        Integer,
        Double,
        String
    };

    struct Row {
        const char *root_ulid;
        const char *ulid;
        const char *hid;
        Size idx;

        char ctime[32];
        char mtime[32];

        RG_HASHTABLE_HANDLER(Row, ulid);
    };

    struct Column {
        const char *name;

        Column *prev;
        Column *next;
        const char *prev_name;

        Type type;
        HeapArray<const char *> values;
        bool valued;

        RG_HASHTABLE_HANDLER(Column, name);
    };

    struct Table {
        const char *name;
        bool root;

        BucketArray<Row> rows;
        HashTable<const char *, Row *> rows_map;

        BucketArray<Column> columns;
        HashTable<const char *, Column *> columns_map;
        HeapArray<const Column *> ordered_columns;
        HashSet<const char *> masked_columns;

        Column *first_column;
        Column *last_column;
        const char *prev_name;

        RG_HASHTABLE_HANDLER(Table, name);
    };

    const char *instance_key;
    const char *project;
    const char *center;
    int64_t schema;
    int64_t mtime;

    json_Parser *parser;

    BucketArray<Table> tables;
    HashTable<const char *, Table *> tables_map;

    BlockAllocator str_alloc;

public:
    RecordExporter(InstanceHolder *instance);

    bool Parse(const char *root_ulid, const char *ulid, const char *hid,
               const char *form, const char *mtime, Span<const char> json);
    bool Export(const char *filename);

private:
    bool ParseObject(const char *root_ulid, const char *form, const char *ulid, const char *hid,
                     const char *mtime, const char *prefix, int depth);

    Table *GetTable(const char *name, bool root);
    Row *GetRow(Table *table, const char *root_ulid, const char *ulid, const char *hid, const char *mtime);
    Column *GetColumn(Table *table, const char *prefix, const char *key, const char *suffix);
};

RecordExporter::RecordExporter(InstanceHolder *instance)
{
    InstanceHolder *master = instance->master;

    instance_key = DuplicateString(instance->key, &str_alloc).ptr;
    project = DuplicateString(master->config.name, &str_alloc).ptr;
    if (master != instance) {
        center = DuplicateString(instance->config.name, &str_alloc).ptr;
    } else {
        center = nullptr;
    }
    schema = master->fs_version.load();
    mtime = GetUnixTime();
}

static void EncodeSqlName(const char *name, HeapArray<char> *out_buf)
{
    out_buf->Append('"');
    for (Size i = 0; name[i]; i++) {
        char c = name[i];

        if (c == '"') {
            out_buf->Append("\"\"");
        } else {
            out_buf->Append(c);
        }
    }
    out_buf->Append('"');

    out_buf->Grow(1);
    out_buf->ptr[out_buf->len] = 0;
}

bool RecordExporter::Parse(const char *root_ulid, const char *ulid, const char *hid, const char *form, 
                           const char *mtime, Span<const char> json)
{
    StreamReader reader(MakeSpan((const uint8_t *)json.ptr, json.len), "<json>");
    json_Parser parser(&reader, &str_alloc);

    this->parser = &parser;

    if (!ParseObject(root_ulid, form, ulid, hid, mtime, nullptr, 0))
        return false;

    return true;
}

bool RecordExporter::Export(const char *filename)
{
    // Prepare export file
    sq_Database db;
    if (!db.Open(filename, SQLITE_OPEN_READWRITE))
        return false;

    // Reorder columns
    for (Table &table: tables) {
        table.ordered_columns.Clear();

        const Column *it = table.first_column;

        while (it) {
            if (it->valued && !table.masked_columns.Find(it->name)) {
                table.ordered_columns.Append(it);
            }
            it = it->next;
        }
    }

    // Project information
    {
        bool success = db.RunMany(R"(
            CREATE TABLE _goupile (
                key TEXT NOT NULL,
                value BLOB
           );
        )");
        if (!success)
            return false;


        const char *sql = "INSERT INTO _goupile (key, value) VALUES (?1, ?2)";

        if (!db.Run(sql, "instance", instance_key))
            return false;
        if (!db.Run(sql, "project", project))
            return false;
        if (!db.Run(sql, "center", center))
            return false;
        if (!db.Run(sql, "schema", schema))
            return false;
        if (!db.Run(sql, "date", mtime))
            return false;
    }

    // Create tables
    for (const Table &table: tables) {
        HeapArray<char> sql(&str_alloc);

        Fmt(&sql, "CREATE TABLE "); EncodeSqlName(table.name, &sql); Fmt(&sql, " (__ROOT TEXT, __ULID TEXT,");
        if (table.root) {
            Fmt(&sql, "__HID, ");
        }
        Fmt(&sql, "__CTIME, __MTIME, ");
        for (const Column *col: table.ordered_columns) {
            EncodeSqlName(col->name, &sql);
            switch (col->type) {
                case Type::Unknown: { Fmt(&sql, ", "); } break;
                case Type::Integer: { Fmt(&sql, " INTEGER, "); } break;
                case Type::Double: { Fmt(&sql, " REAL, "); } break;
                case Type::String: { Fmt(&sql, " TEXT, "); } break;
            }
        }
        sql.len -= 2;
        Fmt(&sql, ")");

        if (!db.Run(sql.ptr))
            return false;
    }

    // Import data
    HashSet<const void *> used_tables;
    for (const Table &table: tables) {
        HeapArray<char> sql(&str_alloc);

        Fmt(&sql, "INSERT INTO "); EncodeSqlName(table.name, &sql); Fmt(&sql, " VALUES (?1, ?2");
        if (table.root) {
            Fmt(&sql, ", ?3, ?4, ?5");
        } else {
            Fmt(&sql, ", ?3, ?4");
        }
        for (Size i = 0; i < table.ordered_columns.len; i++) {
            Fmt(&sql, ", ?%1", i + 5 + table.root);
        }
        Fmt(&sql, ")");

        sq_Statement stmt;
        if (!db.Prepare(sql.ptr, &stmt))
            return false;

        for (Size i = 0; i < table.rows.len; i++) {
            stmt.Reset();

            Span<const char> ulid = SplitStr(table.rows[i].ulid, '.');

            sqlite3_bind_text(stmt, 1, table.rows[i].root_ulid, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, ulid.ptr, (int)ulid.len, SQLITE_STATIC);
            if (table.root) {
                sqlite3_bind_text(stmt, 3, table.rows[i].hid, -1, SQLITE_STATIC);
            }
            sqlite3_bind_text(stmt, 3 + table.root, table.rows[i].ctime, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4 + table.root, table.rows[i].mtime, -1, SQLITE_STATIC);
            for (Size j = 0; j < table.ordered_columns.len; j++) {
                const Column *col = table.ordered_columns[j];
                sqlite3_bind_text(stmt, (int)j + 5 + table.root, col->values[i], -1, SQLITE_STATIC);
            }

            if (!stmt.Run())
                return false;

            used_tables.Set(&table);
        }
    }

    // Delete unused tables
    for (const Table &table: tables) {
        if (!used_tables.Find(&table)) {
            HeapArray<char> sql(&str_alloc);

            Fmt(&sql, "DROP TABLE "); EncodeSqlName(table.name, &sql);

            if (!db.Run(sql.ptr))
                return false;
        }
    }

    if (!db.Close())
        return false;

    return true;
}

bool RecordExporter::ParseObject(const char *root_ulid, const char *form, const char *ulid,
                                 const char *hid, const char *mtime, const char *prefix, int depth)
{
    bool root = TestStr(root_ulid, ulid) && !prefix;
    Table *table = GetTable(form, root);
    Row *row = GetRow(table, root_ulid, ulid, hid, mtime);

    parser->ParseObject();
    while (parser->InObject()) {
        Span<const char> key = {};
        parser->ParseKey(&key);

        switch (parser->PeekToken()) {
            case json_TokenType::Null: {
                parser->ParseNull();

                Column *col = GetColumn(table, prefix, key.ptr, nullptr);
                col->values[row->idx] = nullptr;
            } break;
            case json_TokenType::Bool: {
                bool value = false;
                parser->ParseBool(&value);

                Column *col = GetColumn(table, prefix, key.ptr, nullptr);
                col->type = (Type)std::max((int)col->type, (int)Type::Integer);
                col->values[row->idx] = value ? "1" : "0";
                col->valued = true;
            } break;
            case json_TokenType::Integer: {
                int64_t value = 0;
                parser->ParseInt(&value);

                Column *col = GetColumn(table, prefix, key.ptr, nullptr);
                col->type = (Type)std::max((int)col->type, (int)Type::Integer);
                col->values[row->idx] = Fmt(&str_alloc, "%1", value).ptr;
                col->valued = true;
            } break;
            case json_TokenType::Double: {
                double value = 0.0;
                parser->ParseDouble(&value);

                Column *col = GetColumn(table, prefix, key.ptr, nullptr);
                col->type = (Type)std::max((int)col->type, (int)Type::Double);
                col->values[row->idx] = Fmt(&str_alloc, "%1", value).ptr;
                col->valued = true;
            } break;
            case json_TokenType::String: {
                const char *str = nullptr;
                parser->ParseString(&str);

                Column *col = GetColumn(table, prefix, key.ptr, nullptr);
                col->type = (Type)std::max((int)col->type, (int)Type::String);
                col->values[row->idx] = DuplicateString(str, &str_alloc).ptr;
                col->valued = true;
            } break;

            case json_TokenType::StartArray: {
                table->masked_columns.Set(key.ptr);

                parser->ParseArray();
                while (parser->InArray()) {
                    switch (parser->PeekToken()) {
                        case json_TokenType::Null: {
                            parser->ParseNull();

                            Column *col = GetColumn(table, prefix, key.ptr, "null");
                            col->type = (Type)std::max((int)col->type, (int)Type::Integer);
                            col->values[row->idx] = "1";
                        } break;
                        case json_TokenType::Bool: {
                            bool value = false;
                            parser->ParseBool(&value);

                            Column *col = GetColumn(table, prefix, key.ptr, value ? "1" : "0");
                            col->type = (Type)std::max((int)col->type, (int)Type::Integer);
                            col->values[row->idx] = "1";
                            col->valued = true;
                        } break;
                        case json_TokenType::Integer: {
                            int64_t value = 0;
                            parser->ParseInt(&value);

                            char buf[64];
                            Fmt(buf, "%1", value);

                            Column *col = GetColumn(table, prefix, key.ptr, buf);
                            col->type = (Type)std::max((int)col->type, (int)Type::Integer);
                            col->values[row->idx] = "1";
                            col->valued = true;
                        } break;
                        case json_TokenType::Double: {
                            double value = 0.0;
                            parser->ParseDouble(&value);

                            char buf[64];
                            Fmt(buf, "%1", value);

                            Column *col = GetColumn(table, prefix, key.ptr, buf);
                            col->type = (Type)std::max((int)col->type, (int)Type::Integer);
                            col->values[row->idx] = "1";
                            col->valued = true;
                        } break;
                        case json_TokenType::String: {
                            const char *str = nullptr;
                            parser->ParseString(&str);

                            Column *col = GetColumn(table, prefix, key.ptr, str);
                            col->type = (Type)std::max((int)col->type, (int)Type::String);
                            col->values[row->idx] = "1";
                            col->valued = true;
                        } break;

                        default: { parser->Skip(); } break;
                    }
                }
            } break;

            case json_TokenType::StartObject: {
                if (RG_UNLIKELY(depth >= 16)) {
                    LogError("Excessive nesting of objects");
                    return false;
                }

                if (key.len && std::all_of(key.begin(), key.end(), IsAsciiDigit)) {
                    const char *form2 = Fmt(&str_alloc, "%1.%2", form, prefix).ptr;
                    const char *ulid2 = Fmt(&str_alloc, "%1.%2", ulid, key).ptr;

                    if (!ParseObject(root_ulid, form2, ulid2, nullptr, mtime, nullptr, depth + 1))
                        return false;
                } else if (prefix) {
                    const char *prefix2 = Fmt(&str_alloc, "%1.%2", prefix, key).ptr;

                    if (!ParseObject(root_ulid, form, ulid, nullptr, mtime, prefix2, depth + 1))
                        return false;
                } else {
                    if (!ParseObject(root_ulid, form, ulid, nullptr, mtime, key.ptr, depth + 1))
                        return false;
                }
            } break;

            default: {
                if (parser->IsValid()) {
                    LogError("Unexpected JSON token type for '%1'", key);
                }
                return false;
            } break;
        }
    }
    if (!parser->IsValid())
        return false;

    return true;
}

RecordExporter::Column *RecordExporter::GetColumn(RecordExporter::Table *table, const char *prefix,
                                                  const char *key, const char *suffix)
{
    const char *name;
    {
        HeapArray<char> buf(&str_alloc);

        if (prefix) {
            for (Size i = 0; prefix[i]; i++) {
                char c = LowerAscii(prefix[i]);
                buf.Append(c);
            }
            buf.Append('.');
        }
        for (Size i = 0; key[i]; i++) {
            char c = LowerAscii(key[i]);
            buf.Append(c);
        }
        if (suffix) {
            buf.Append('.');
            for (Size i = 0; suffix[i]; i++) {
                char c = LowerAscii(suffix[i]);
                buf.Append(c);
            }
        }
        buf.Append(0);

        name = buf.TrimAndLeak().ptr;
    }

    Column *col = table->columns_map.FindValue(name, nullptr);

    if (!col) {
        col = table->columns.AppendDefault();
        col->name = name;

        table->columns_map.Set(col);

        if (table->columns.len > 1) {
            if (table->prev_name) {
                Column *it = table->columns_map.FindValue(table->prev_name, nullptr);

                if (it) {
                    Column *next = it->next;

                    while (next) {
                        if (!next->prev_name)
                            break;
                        if (!TestStr(next->prev_name, table->prev_name))
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
    col->prev_name = table->prev_name;
    col->values.AppendDefault(table->rows.len - col->values.len);

    table->prev_name = name;

    return col;
}

RecordExporter::Table *RecordExporter::GetTable(const char *name, bool root)
{
    Table *table = tables_map.FindValue(name, nullptr);

    if (!table) {
        table = tables.AppendDefault();
        table->name = DuplicateString(name, &str_alloc).ptr;

        tables_map.Set(table);
    }

    table->root |= root;
    return table;
}

RecordExporter::Row *RecordExporter::GetRow(RecordExporter::Table *table, const char *root_ulid,
                                            const char *ulid, const char *hid, const char *mtime)
{
    Row *row = table->rows_map.FindValue(ulid, nullptr);

    if (!row) {
        row = table->rows.AppendDefault();

        row->root_ulid = DuplicateString(root_ulid, &str_alloc).ptr;
        row->ulid = DuplicateString(ulid, &str_alloc).ptr;
        row->hid = hid && hid[0] ? DuplicateString(hid, &str_alloc).ptr : nullptr;
        row->idx = table->rows.len - 1;
        CopyString(mtime, row->ctime);

        table->rows_map.Set(row);

        for (Column &col: table->columns) {
            col.values.AppendDefault(table->rows.len - col.values.len);
        }
    }

    CopyString(mtime, row->mtime);

    return row;
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
        if (!instance->db->Prepare(R"(SELECT e.root_ulid, e.ulid, e.hid, lower(e.form), f.type, f.mtime, f.json FROM rec_entries e
                                      INNER JOIN rec_entries r ON (r.ulid = e.root_ulid)
                                      INNER JOIN rec_fragments f ON (f.ulid = e.ulid)
                                      WHERE r.deleted = 0
                                      ORDER BY f.anchor)", &stmt))
            return;

        const char *export_filename = CreateUniqueFile(gp_domain.config.tmp_directory, "", ".tmp", &io->allocator);
        RG_DEFER { UnlinkFile(export_filename); };

        RecordExporter exporter(instance);

        while (stmt.Step()) {
            const char *root_ulid = (const char *)sqlite3_column_text(stmt, 0);
            const char *ulid = (const char *)sqlite3_column_text(stmt, 1);
            const char *hid = (const char *)sqlite3_column_text(stmt, 2);
            const char *form = (const char *)sqlite3_column_text(stmt, 3);
            const char *type = (const char *)sqlite3_column_text(stmt, 4);
            const char *mtime = (const char *)sqlite3_column_text(stmt, 5);

            if (TestStr(type, "save")) {
                Span<const char> json = MakeSpan((const char *)sqlite3_column_blob(stmt, 6),
                                                 sqlite3_column_bytes(stmt, 6));
                if (!exporter.Parse(root_ulid, ulid, hid, form, mtime, json))
                    return;
            }
        }
        if (!stmt.IsValid())
            return;

        if (!exporter.Export(export_filename))
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
