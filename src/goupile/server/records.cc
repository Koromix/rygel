// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "config.hh"
#include "goupile.hh"
#include "ports.hh"
#include "../../core/libwrap/json.hh"
#include "../../core/libwrap/sqlite.hh"

namespace RG {

void HandleRecordGet(const http_RequestInfo &request, http_IO *io)
{
    Span<const char> form_name;
    Span<const char> id;
    {
        Span<const char> remain = request.url + 1;

        SplitStr(remain, '/', &remain);
        form_name = SplitStr(remain, '/', &remain);
        id = SplitStr(remain, '/', &remain);

        if (!form_name.len || remain.len) {
            LogError("URL must contain form and optional ID (and nothing more)");
            io->AttachError(422);
            return;
        }

        if (form_name == ".." || id == "..") {
            LogError("URL must not contain '..' components");
            io->AttachError(422);
            return;
        }
    }

    if (id.len) {
        sq_Statement stmt;
        if (!goupile_db.Prepare(R"(SELECT id, sequence, data
                                   FROM records
                                   WHERE form = ? AND id = ?)", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, form_name.ptr, form_name.len, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, id.ptr, id.len, SQLITE_STATIC);

        if (!stmt.Next()) {
            if (stmt.IsValid()) {
                LogError("Record does not exist");
                io->AttachError(404);
            }
            return;
        }

        // Export data
        http_JsonPageBuilder json(request.compression_type);

        json.StartObject();
        json.Key("id"); json.String((const char *)sqlite3_column_text(stmt, 0));
        json.Key("sequence"); json.Int(sqlite3_column_int(stmt, 1));
        json.Key("data"); json.Raw((const char *)sqlite3_column_text(stmt, 2));
        json.EndObject();

        json.Finish(io);
    } else {
        sq_Statement stmt;
        if (!goupile_db.Prepare(R"(SELECT id, sequence, data
                                   FROM records
                                   WHERE form = ?
                                   ORDER BY id)", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, form_name.ptr, form_name.len, SQLITE_STATIC);

        // Export data
        http_JsonPageBuilder json(request.compression_type);

        json.StartArray();
        while (stmt.Next()) {
            json.StartObject();
            json.Key("id"); json.String((const char *)sqlite3_column_text(stmt, 0));
            json.Key("sequence"); json.Int(sqlite3_column_int(stmt, 1));
            json.Key("data"); json.Raw((const char *)sqlite3_column_text(stmt, 2));
            json.EndObject();
        }
        if (!stmt.IsValid())
            return;
        json.EndArray();

        json.Finish(io);
    }
}

void HandleRecordPut(const http_RequestInfo &request, http_IO *io)
{
    Span<const char> form_name;
    Span<const char> id;
    Span<const char> page_name;
    {
        Span<const char> remain = request.url + 1;

        SplitStr(remain, '/', &remain);
        form_name = SplitStr(remain, '/', &remain);
        id = SplitStr(remain, '/', &remain);
        page_name = SplitStr(remain, '/', &remain);

        if (!form_name.len || !page_name.len || !id.len || remain.len) {
            LogError("URL must contain form, page and ID (and nothing more)");
            io->AttachError(422);
            return;
        }

        if (form_name == ".." || id == ".." || page_name == "..") {
            LogError("URL must not contain '..' components");
            io->AttachError(422);
            return;
        }
    }

    /// XXX: Check form and page actually exist!
    const char *page_filename = Fmt(&io->allocator, "%1%/pages%/%2.js",
                                    goupile_config.files_directory, page_name).ptr;

    io->RunAsync([=]() {
        // Load page code
        HeapArray<char> script;
        if (ReadFile(page_filename, goupile_config.max_file_size, &script) < 0)
            return;

        // Find appropriate port
        ScriptPort *port = LockPort();
        RG_DEFER { UnlockPort(port); };

        // Parse request body (JSON)
        ScriptHandle values;
        {
            StreamReader st;
            if (!io->OpenForRead(&st))
                return;

            if (!port->ParseValues(&st, &values))
                return;
        }

        ScriptRecord record;
        if (!port->RunRecord(script, values, &record))
            return;

        bool success = goupile_db.Transaction([&]() {
            int sequence;
            {
                sq_Statement stmt;
                if (!goupile_db.Prepare(R"(SELECT sequence
                                           FROM records_sequences
                                           WHERE form = ?)", &stmt))
                    return false;
                sqlite3_bind_text(stmt, 1, form_name.ptr, form_name.len, SQLITE_STATIC);

                if (stmt.Next()) {
                    sequence = sqlite3_column_int(stmt, 0);
                } else if (stmt.IsValid()) {
                    sequence = 1;
                } else {
                    return false;
                }
            }

            // Update sequence number
            if (!goupile_db.Run(R"(INSERT INTO records_sequences (form, sequence)
                                   VALUES (?, ?)
                                   ON CONFLICT (form) DO UPDATE SET sequence = excluded.sequence)",
                                form_name, sequence + 1))
                return false;

            // Save record
            if (!goupile_db.Run(R"(INSERT INTO records (id, form, sequence, data)
                                   VALUES (?, ?, ?, ?)
                                   ON CONFLICT (id) DO UPDATE SET form = excluded.form,
                                                                  data = json_patch(data, excluded.data))",
                                    id, form_name, sequence, record.json))
                return false;

            // Save variables
            {
                sq_Statement stmt;
                if (!goupile_db.Prepare(R"(INSERT INTO records_variables (form, key, page, before, after)
                                           VALUES (?, ?, ?, ?, ?)
                                           ON CONFLICT (form, key) DO UPDATE SET before = excluded.before,
                                                                                 after = excluded.after)", &stmt))
                    return false;
                sqlite3_bind_text(stmt, 1, form_name.ptr, form_name.len, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, page_name.ptr, page_name.len, SQLITE_STATIC);

                for (Size i = 0; i < record.variables.len; i++) {
                    const char *key = record.variables[i];
                    const char *before = i ? record.variables[i - 1] : nullptr;
                    const char *after = (i + 1 < record.variables.len) ? record.variables[i + 1] : nullptr;

                    stmt.Reset();
                    sqlite3_bind_text(stmt, 2, key, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 4, before, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 5, after, -1, SQLITE_STATIC);

                    if (!stmt.Run())
                        return false;
                }
            }

            return true;
        });
        if (!success)
            return;

        io->AttachText(200, "Done!");
    });
}

void HandleRecordDelete(const http_RequestInfo &request, http_IO *io)
{
    Span<const char> form_name;
    Span<const char> id;
    {
        Span<const char> remain = request.url + 1;

        SplitStr(remain, '/', &remain);
        form_name = SplitStr(remain, '/', &remain);
        id = SplitStr(remain, '/', &remain);

        if (!form_name.len || !id.len || remain.len) {
            LogError("URL must contain form and ID (and nothing more)");
            io->AttachError(422);
            return;
        }

        if (form_name == ".." || id == "..") {
            LogError("URL must not contain '..' components");
            io->AttachError(422);
            return;
        }
    }

    // Asking for deletion of non-existent records is teolerated
    goupile_db.Run("DELETE FROM records WHERE id = ?", id);

    io->AttachText(200, "Done!");
}

void HandleRecordVariables(const http_RequestInfo &request, http_IO *io)
{
    const char *form_name = request.GetQueryValue("form");
    if (!form_name) {
        LogError("Missing 'form' parameter'");
        io->AttachError(422);
        return;
    }

    sq_Statement stmt;
    if (!goupile_db.Prepare(R"(SELECT key, page, before, after
                               FROM records_variables
                               WHERE form = ?)", &stmt))
        return;
    sqlite3_bind_text(stmt, 1, form_name, -1, SQLITE_STATIC);

    // Export data
    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    while (stmt.Next()) {
        json.StartObject();
        json.Key("key"); json.String((const char *)sqlite3_column_text(stmt, 0));
        json.Key("page"); json.String((const char *)sqlite3_column_text(stmt, 1));
        json.Key("before"); json.String((const char *)sqlite3_column_text(stmt, 2));
        json.Key("after"); json.String((const char *)sqlite3_column_text(stmt, 3));
        json.EndObject();
    }
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish(io);
}

}
