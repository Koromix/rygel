// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "../server/config.hh"
#include "../server/domain.hh"
#include "../server/goupile.hh"
#include "../server/instance.hh"
#include "../server/user.hh"
#include "records.hh"
#include "lib/native/wrap/json.hh"
#if defined(_WIN32)
    #include <io.h>
#else
    #include <unistd.h>
#endif

namespace K {

void HandleLegacyLoad(http_IO *io, InstanceHolder *instance)
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

    int64_t anchor;
    if (const char *str = request.GetQueryValue("anchor"); str) {
        if (!ParseInt(str, &anchor)) {
            io->SendError(422);
            return;
        }
    } else {
        LogError("Missing 'userid' parameter");
        io->SendError(422);
        return;
    }

    sq_Statement stmt;
    {
        LocalArray<char, 1024> sql;

        sql.len += Fmt(sql.TakeAvailable(), R"(SELECT e.rowid, e.ulid, e.form, e.sequence, e.hid, e.anchor,
                                                      e.parent_ulid, e.parent_version, f.anchor,
                                                      f.version, f.type, f.username, f.mtime, f.fs, f.page,
                                                      f.json, f.tags FROM rec_entries e
                                               LEFT JOIN rec_fragments f ON (f.ulid = e.ulid)
                                               WHERE e.anchor >= ?1)").len;
        if (!stamp->HasPermission(UserPermission::DataRead)) {
            sql.len += Fmt(sql.TakeAvailable(), " AND e.root_ulid IN (SELECT ulid FROM ins_claims WHERE userid = ?2)").len;
        }
        sql.len += Fmt(sql.TakeAvailable(), " ORDER BY e.rowid, f.anchor").len;

        if (!instance->db->Prepare(sql.data, &stmt))
            return;

        sqlite3_bind_int64(stmt, 1, anchor);
        sqlite3_bind_int64(stmt, 2, -session->userid);
    }

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();

        if (stmt.Step()) {
            do {
                int64_t rowid = sqlite3_column_int64(stmt, 0);

                json->StartObject();

                json->Key("ulid"); json->String((const char *)sqlite3_column_text(stmt, 1));
                json->Key("form"); json->String((const char *)sqlite3_column_text(stmt, 2));
                json->Key("sequence"); json->Int64(sqlite3_column_int64(stmt, 3));
                json->Key("hid"); switch (sqlite3_column_type(stmt, 4)) {
                    case SQLITE_NULL: { json->Null(); } break;
                    case SQLITE_INTEGER: { json->Int64(sqlite3_column_int64(stmt, 4)); } break;
                    default: { json->String((const char *)sqlite3_column_text(stmt, 4)); } break;
                }
                json->Key("anchor"); json->Int64(sqlite3_column_int64(stmt, 5));
                if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
                    json->Key("parent"); json->StartObject();
                    json->Key("ulid"); json->String((const char *)sqlite3_column_text(stmt, 6));
                    json->Key("version"); json->Int64(sqlite3_column_int64(stmt, 7));
                    json->EndObject();
                } else {
                    json->Key("parent"); json->Null();
                }

                json->Key("fragments"); json->StartArray();
                if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
                    do {
                        json->StartObject();

                        const char *type = (const char *)sqlite3_column_text(stmt, 10);

                        json->Key("anchor"); json->Int64(sqlite3_column_int64(stmt, 8));
                        json->Key("version"); json->Int64(sqlite3_column_int64(stmt, 9));
                        json->Key("type"); json->String(type);
                        json->Key("username"); json->String((const char *)sqlite3_column_text(stmt, 11));
                        json->Key("mtime"); json->String((const char *)sqlite3_column_text(stmt, 12));
                        json->Key("fs"); json->Int64(sqlite3_column_int64(stmt, 13));
                        if (TestStr(type, "save")) {
                            json->Key("page"); json->String((const char *)sqlite3_column_text(stmt, 14));
                            json->Key("values"); json->Raw((const char *)sqlite3_column_text(stmt, 15));
                            json->Key("tags"); json->Raw((const char *)sqlite3_column_text(stmt, 16));
                        }

                        json->EndObject();
                    } while (stmt.Step() && sqlite3_column_int64(stmt, 0) == rowid);
                } else {
                    stmt.Step();
                }
                json->EndArray();

                json->EndObject();
            } while (stmt.IsRow());
        }
        if (!stmt.IsValid())
            return;

        json->EndArray();
    });
}

struct SaveRecord {
    struct Fragment {
        const char *type = nullptr;
        const char *mtime = nullptr;
        int64_t fs = -1;
        const char *page = nullptr;
        Span<const char> json = {};
        Span<const char> tags = {};
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

void HandleLegacySave(http_IO *io, InstanceHolder *instance)
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

    if (!session->userid) {
        K_ASSERT(session->type == SessionType::Auto);

        session = MigrateGuestSession(io, instance, nullptr);
        if (!session)
            return;
        stamp = session->GetStamp(instance);
        if (!stamp)
            return;

        K_ASSERT(session->userid < 0);
    }

    HeapArray<SaveRecord> records;

    // Parse records from JSON
    {
        bool success = http_ParseJson(io, Mebibytes(256), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseArray(); json->InArray(); ) {
                SaveRecord *record = records.AppendDefault();

                for (json->ParseObject(); json->InObject(); ) {
                    Span<const char> key = json->ParseKey();

                    if (key == "form") {
                        json->ParseString(&record->form);
                    } else if (key == "ulid") {
                        json->ParseString(&record->ulid);
                    } else if (key == "hid") {
                        switch (json->PeekToken()) {
                            case json_TokenType::Null: {
                                json->ParseNull();
                                record->hid = nullptr;
                            } break;
                            case json_TokenType::Number: {
                                int64_t value;
                                json->ParseInt(&value);
                                record->hid = Fmt(io->Allocator(), "%1", value).ptr;
                            } break;
                            default: {
                                json->ParseString(&record->hid);
                            } break;
                        }
                    } else if (key == "parent") {
                        if (json->PeekToken() == json_TokenType::Null) {
                            json->ParseNull();
                            record->parent.ulid = nullptr;
                            record->parent.version = -1;
                        } else {
                            for (json->ParseObject(); json->InObject(); ) {
                                Span<const char> key = json->ParseKey();

                                if (key == "ulid") {
                                    json->ParseString(&record->parent.ulid);
                                } else if (key == "version") {
                                    json->ParseInt(&record->parent.version);
                                } else {
                                    json->UnexpectedKey(key);
                                    valid = false;
                                }
                            }

                            if (!record->parent.ulid || record->parent.version < 0) [[unlikely]] {
                                LogError("Missing or invalid parent ULID or version");
                                valid = false;
                            }
                        }
                    } else if (key == "fragments") {
                        for (json->ParseArray(); json->InArray(); ) {
                            SaveRecord::Fragment *fragment = record->fragments.AppendDefault();

                            for (json->ParseObject(); json->InObject(); ) {
                                Span<const char> key = json->ParseKey();

                                if (key == "type") {
                                    json->ParseString(&fragment->type);
                                } else if (key == "mtime") {
                                    json->ParseString(&fragment->mtime);
                                } else if (key == "fs") {
                                    json->ParseInt(&fragment->fs);
                                } else if (key == "page") {
                                    if (json->PeekToken() == json_TokenType::Null) {
                                        json->ParseNull();
                                        fragment->page = nullptr;
                                    } else {
                                        json->ParseString(&fragment->page);
                                    }
                                } else if (key == "json") {
                                    json->ParseString(&fragment->json);
                                } else if (key == "tags") {
                                    json->ParseString(&fragment->tags);
                                } else {
                                    json->UnexpectedKey(key);
                                    valid = false;
                                }
                            }
                        }
                    } else {
                        json->UnexpectedKey(key);
                        valid = false;
                    }
                }

                record->deleted = record->fragments.len &&
                                  TestStr(record->fragments[record->fragments.len - 1].type, "delete");
            }
            valid &= json->IsValid();

            if (valid) {
                for (const SaveRecord &record: records) {
                    if (!record.form || !record.ulid) [[unlikely]] {
                        LogError("Missing form or ULID in record object");
                        valid = false;
                    }

                    for (const SaveRecord::Fragment &fragment: record.fragments) {
                        if (!fragment.mtime || fragment.fs < 0) [[unlikely]] {
                            LogError("Missing type, mtime or FS in fragment object");
                            valid = false;
                        }

                        if (!fragment.type) {
                            LogError("Missing fragment type");
                            valid = false;
                        } else if (TestStr(fragment.type, "save")) {
                            if (!fragment.page || !fragment.json.IsValid() || !fragment.tags.IsValid()) {
                                LogError("Fragment 'save' is missing page or JSON data");
                                valid = false;
                            }
                        } else if (TestStr(fragment.type, "delete")) {
                            // Valid
                        } else {
                            LogError("Invalid fragment type '%1'", fragment.type);
                            valid = false;
                        }
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

    // Save to database
    bool success = instance->db->Transaction([&]() {
        for (const SaveRecord &record: records) {
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
            if (!stamp->HasPermission(UserPermission::DataRead)) {
                sq_Statement stmt;
                if (!instance->db->Prepare(R"(SELECT e.rowid, c.rowid FROM rec_entries e
                                              LEFT JOIN ins_claims c ON (c.userid = ?1 AND c.ulid = e.ulid)
                                              WHERE e.ulid = ?2)", &stmt))
                    return false;
                sqlite3_bind_int64(stmt, 1, -session->userid);
                sqlite3_bind_text(stmt, 2, root_ulid, -1, SQLITE_STATIC);

                if (stmt.Step()) {
                    bool can_save = (sqlite3_column_type(stmt, 1) == SQLITE_INTEGER);

                    if (!can_save) {
                        LogError("You are not allowed to alter this record", record.ulid, record.form);
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
            int64_t anchor = -1;
            if (record.fragments.len) {
                for (Size i = 0; i < record.fragments.len; i++) {
                    const SaveRecord::Fragment &fragment = record.fragments[i];

                    sq_Statement stmt;
                    if (!instance->db->Prepare(R"(INSERT INTO rec_fragments (ulid, version, type, userid, username,
                                                                             mtime, fs, page, json, tags)
                                                  VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)
                                                  ON CONFLICT DO NOTHING
                                                  RETURNING anchor)",
                                          &stmt, record.ulid, i + 1, fragment.type, session->userid, session->username,
                                          fragment.mtime, fragment.fs, fragment.page, fragment.json, fragment.tags.ptr))
                        return false;

                    if (stmt.Step()) {
                        anchor = sqlite3_column_int64(stmt, 0);
                    } else {
                        if (!stmt.IsValid())
                            return false;

                        LogDebug("Ignoring conflicting fragment %1", i);
                    }
                }
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
            }
            if (anchor < 0)
                continue;

            // Insert or update record entry
            int64_t sequence;
            int64_t rowid;
            {
                sq_Statement stmt;
                if (!instance->db->Prepare(R"(INSERT INTO rec_entries (ulid, sequence, hid, form, parent_ulid,
                                                                       parent_version, root_ulid, anchor, deleted)
                                              VALUES (?1, -1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)
                                              ON CONFLICT (ulid) DO UPDATE SET hid = IFNULL(excluded.hid, hid),
                                                                               anchor = excluded.anchor,
                                                                               deleted = excluded.deleted
                                              RETURNING sequence, rowid)", &stmt))
                    return false;
                sqlite3_bind_text(stmt, 1, record.ulid, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, record.hid, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, record.form, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 4, record.parent.ulid, -1, SQLITE_STATIC);
                if (record.parent.version >= 0) {
                    sqlite3_bind_int64(stmt, 5, record.parent.version);
                } else {
                    sqlite3_bind_null(stmt, 5);
                }
                sqlite3_bind_text(stmt, 6, root_ulid, -1, SQLITE_STATIC);
                sqlite3_bind_int64(stmt, 7, anchor);
                sqlite3_bind_int(stmt, 8, 0 + record.deleted);

                if (!stmt.Step()) {
                    K_ASSERT(!stmt.IsValid());
                    return false;
                }

                sequence = sqlite3_column_int64(stmt, 0);
                rowid = sqlite3_column_int64(stmt, 1);
            }

            // Update sequence counter
            if (sequence < 0) {
                int64_t counter;
                {
                    sq_Statement stmt;
                    if (!instance->db->Prepare(R"(INSERT INTO seq_counters (type, key, counter)
                                                  VALUES ('record', ?1, 1)
                                                  ON CONFLICT (type, key) DO UPDATE SET counter = counter + 1
                                                  RETURNING counter)", &stmt, record.form))
                        return false;
                    if (!stmt.GetSingleValue(&counter))
                        return false;
                }

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

        return true;
    });
    if (!success)
        return;

    io->SendText(200, "Done!");
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

        K_HASHTABLE_HANDLER(Row, ulid);
    };

    struct Column {
        const char *name;

        Column *prev;
        Column *next;
        const char *prev_name;

        Type type;
        HeapArray<const char *> values;
        bool valued;

        K_HASHTABLE_HANDLER(Column, name);
    };

    struct Table {
        const char *name;
        bool root;

        BucketArray<Row> rows;
        HashTable<const char *, Row *> rows_map;

        BucketArray<Column> columns;
        HashTable<const char *, Column *> columns_map;
        HeapArray<const Column *> ordered_columns;

        Column *first_column;
        Column *last_column;
        const char *prev_name;

        K_HASHTABLE_HANDLER(Table, name);
    };

    const char *instance_key;
    const char *project;
    const char *center;
    int64_t schema;
    int64_t mtime;

    json_Parser *json;

    BucketArray<Table> tables;
    HashTable<const char *, Table *> tables_map;

    BlockAllocator str_alloc;

public:
    RecordExporter(InstanceHolder *instance);

    bool Parse(const char *root_ulid, const char *ulid, const char *hid,
               const char *form, const char *mtime, Span<const char> data);
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
    project = DuplicateString(master->settings.name, &str_alloc).ptr;
    if (master != instance) {
        center = DuplicateString(instance->settings.name, &str_alloc).ptr;
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
                           const char *mtime, Span<const char> data)
{
    StreamReader reader(MakeSpan((const uint8_t *)data.ptr, data.len), "<json>");
    json_Parser json(&reader, &str_alloc);

    this->json = &json;

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
            if (it->valued) {
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

        for (Size i = 0; i < table.rows.count; i++) {
            stmt.Reset();

            sqlite3_bind_text(stmt, 1, table.rows[i].root_ulid, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, table.rows[i].ulid, -1, SQLITE_STATIC);
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

    for (json->ParseObject(); json->InObject(); ) {
        Span<const char> key = json->ParseKey();

        switch (json->PeekToken()) {
            case json_TokenType::Null: {
                json->ParseNull();

                Column *col = GetColumn(table, prefix, key.ptr, nullptr);
                col->values[row->idx] = nullptr;
            } break;
            case json_TokenType::Bool: {
                bool value = false;
                json->ParseBool(&value);

                Column *col = GetColumn(table, prefix, key.ptr, nullptr);
                col->type = (Type)std::max((int)col->type, (int)Type::Integer);
                col->values[row->idx] = value ? "1" : "0";
                col->valued = true;
            } break;
            case json_TokenType::Number: {
                if (json->IsNumberFloat()) {
                    double value = 0.0;
                    json->ParseDouble(&value);

                    Column *col = GetColumn(table, prefix, key.ptr, nullptr);
                    col->type = (Type)std::max((int)col->type, (int)Type::Double);
                    col->values[row->idx] = Fmt(&str_alloc, "%1", value).ptr;
                    col->valued = true;
                } else {
                    int64_t value = 0;
                    json->ParseInt(&value);

                    Column *col = GetColumn(table, prefix, key.ptr, nullptr);
                    col->type = (Type)std::max((int)col->type, (int)Type::Integer);
                    col->values[row->idx] = Fmt(&str_alloc, "%1", value).ptr;
                    col->valued = true;
                }
            } break;
            case json_TokenType::String: {
                const char *str = nullptr;
                json->ParseString(&str);

                Column *col = GetColumn(table, prefix, key.ptr, nullptr);
                col->type = (Type)std::max((int)col->type, (int)Type::String);
                col->values[row->idx] = DuplicateString(str, &str_alloc).ptr;
                col->valued = true;
            } break;

            case json_TokenType::StartArray: {
                for (json->ParseArray(); json->InArray(); ) {
                    switch (json->PeekToken()) {
                        case json_TokenType::Null: {
                            json->ParseNull();

                            Column *col = GetColumn(table, prefix, key.ptr, "null");
                            col->type = (Type)std::max((int)col->type, (int)Type::Integer);
                            col->values[row->idx] = "1";
                            col->valued = true;
                        } break;
                        case json_TokenType::Bool: {
                            bool value = false;
                            json->ParseBool(&value);

                            Column *col = GetColumn(table, prefix, key.ptr, value ? "1" : "0");
                            col->type = (Type)std::max((int)col->type, (int)Type::Integer);
                            col->values[row->idx] = "1";
                            col->valued = true;
                        } break;
                        case json_TokenType::Number: {
                            if (json->IsNumberFloat()) {
                                double value = 0.0;
                                json->ParseDouble(&value);

                                char buf[64];
                                Fmt(buf, "%1", value);

                                Column *col = GetColumn(table, prefix, key.ptr, buf);
                                col->type = (Type)std::max((int)col->type, (int)Type::Integer);
                                col->values[row->idx] = "1";
                                col->valued = true;
                            } else {
                                int64_t value = 0;
                                json->ParseInt(&value);

                                char buf[64];
                                Fmt(buf, "%1", value);

                                Column *col = GetColumn(table, prefix, key.ptr, buf);
                                col->type = (Type)std::max((int)col->type, (int)Type::Integer);
                                col->values[row->idx] = "1";
                                col->valued = true;
                            }
                        } break;
                        case json_TokenType::String: {
                            const char *str = nullptr;
                            json->ParseString(&str);

                            Column *col = GetColumn(table, prefix, key.ptr, str);
                            col->type = (Type)std::max((int)col->type, (int)Type::String);
                            col->values[row->idx] = "1";
                            col->valued = true;
                        } break;

                        default: { json->Skip(); } break;
                    }
                }
            } break;

            case json_TokenType::StartObject: {
                if (depth >= 16) [[unlikely]] {
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
                if (json->IsValid()) {
                    LogError("Unexpected JSON token type for '%1'", key);
                }
                return false;
            } break;
        }
    }
    if (!json->IsValid())
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

        if (table->columns.count > 1) {
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
    col->values.AppendDefault(table->rows.count - col->values.len);

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
        row->idx = table->rows.count - 1;
        CopyString(mtime, row->ctime);

        table->rows_map.Set(row);

        for (Column &col: table->columns) {
            col.values.AppendDefault(table->rows.count - col.values.len);
        }
    }

    CopyString(mtime, row->mtime);

    return row;
}

void HandleLegacyExport(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();

    if (!instance->settings.data_remote) {
        LogError("Records API is disabled in Offline mode");
        io->SendError(403);
        return;
    }

    RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);

    if (!session || !session->HasPermission(instance, UserPermission::ExportCreate)) {
        const InstanceHolder *master = instance->master;
        const char *export_key = !instance->slaves.len ? request.GetHeaderValue("X-Export-Key") : nullptr;

        if (!export_key) {
            if (!session) {
                LogError("User is not logged in");
                io->SendError(401);
            } else {
                K_ASSERT(!session->HasPermission(instance, UserPermission::ExportCreate));

                LogError("User is not allowed to export data");
                io->SendError(403);
            }
            return;
        }

        sq_Statement stmt;
        if (!gp_db.Prepare(R"(SELECT permissions FROM dom_permissions
                              WHERE instance = ?1 AND export_key = ?2)", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, master->key.ptr, (int)master->key.len, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, export_key, -1, SQLITE_STATIC);

        uint32_t permissions = stmt.Step() ? (uint32_t)sqlite3_column_int(stmt, 0) : 0;

        if (!stmt.IsValid())
            return;
        if (!(permissions & (int)UserPermission::ExportCreate)) {
            LogError("Export key is not valid");
            io->SendError(403);
            return;
        }
    }

    sq_Statement stmt;
    if (!instance->db->Prepare(R"(SELECT e.root_ulid, e.ulid, e.hid, lower(e.form), f.type, f.mtime, f.json FROM rec_entries e
                                  INNER JOIN rec_entries r ON (r.ulid = e.root_ulid)
                                  INNER JOIN rec_fragments f ON (f.ulid = e.ulid)
                                  WHERE r.deleted = 0
                                  ORDER BY f.anchor)", &stmt))
        return;

    const char *export_filename = CreateUniqueFile(gp_config.tmp_directory, "", ".tmp", io->Allocator());
    K_DEFER { UnlinkFile(export_filename); };

    RecordExporter exporter(instance);

    // Export can take a long time, don't timeout because request looks idle
    io->ExtendTimeout(120000);

    while (stmt.Step()) {
        const char *root_ulid = (const char *)sqlite3_column_text(stmt, 0);
        const char *ulid = (const char *)sqlite3_column_text(stmt, 1);
        const char *hid = (const char *)sqlite3_column_text(stmt, 2);
        const char *form = (const char *)sqlite3_column_text(stmt, 3);
        const char *type = (const char *)sqlite3_column_text(stmt, 4);
        const char *mtime = (const char *)sqlite3_column_text(stmt, 5);

        if (TestStr(type, "save")) {
            Span<const char> data = MakeSpan((const char *)sqlite3_column_blob(stmt, 6),
                                             sqlite3_column_bytes(stmt, 6));
            if (!exporter.Parse(root_ulid, ulid, hid, form, mtime, data))
                return;
        }
    }
    if (!stmt.IsValid())
        return;

    if (!exporter.Export(export_filename))
        return;

    // Ask browser to download
    {
        int64_t time = GetUnixTime();
        const char *disposition = Fmt(io->Allocator(), "attachment; filename=\"%1_%2.db\"",
                                                       instance->key, FmtTimeISO(DecomposeTimeLocal(time))).ptr;
        io->AddHeader("Content-Disposition", disposition);
    }

    io->SendFile(200, export_filename);
}

}
