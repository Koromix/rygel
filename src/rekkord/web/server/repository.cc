// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "lib/native/http/http.hh"
#include "../../lib/librekkord.hh"
#include "rokkerd.hh"
#include "mail.hh"
#include "repository.hh"
#include "user.hh"

namespace K {

void HandleRepositoryList(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT id, url, name, checked, failed, errors
                       FROM repositories
                       WHERE owner = ?1)", &stmt, session->userid))
        return;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();

        while (stmt.Step()) {
            int64_t id = sqlite3_column_int64(stmt, 0);
            const char *url = (const char *)sqlite3_column_text(stmt, 1);
            const char *name = (const char *)sqlite3_column_text(stmt, 2);
            int64_t checked = sqlite3_column_int64(stmt, 3);
            const char *failed = (const char *)sqlite3_column_text(stmt, 4);
            int errors = sqlite3_column_int(stmt, 5);

            json->StartObject();

            json->Key("id"); json->Int64(id);
            json->Key("url"); json->String(url);
            json->Key("name"); json->StringOrNull(name);
            if (checked) {
                json->Key("checked"); json->Int64(checked);
            } else {
                json->Key("checked"); json->Null();
            }
            if (failed) {
                json->Key("failed"); json->String(failed);
            } else {
                json->Key("failed"); json->Null();
            }
            json->Key("errors"); json->Int(errors);

            json->EndObject();
        }
        if (!stmt.IsValid())
            return;

        json->EndArray();
    });
}

void HandleRepositoryGet(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    int64_t id = -1;
    {
        const char *str = request.GetQueryValue("id");

        if (!str || !ParseInt(str, &id, (int)ParseFlag::End)) {
            LogError("Missing or invalid repository ID");
            io->SendError(422);
            return;
        }
    }

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT url, name, checked, failed, errors
                       FROM repositories
                       WHERE owner = ?1 AND id = ?2)",
                    &stmt, session->userid, id))
        return;

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("Unknown repository ID %1", id);
            io->SendError(404);
        }
        return;
    }

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        // Main information
        {
            const char *url = (const char *)sqlite3_column_text(stmt, 0);
            const char *name = (const char *)sqlite3_column_text(stmt, 1);
            int64_t checked = sqlite3_column_int64(stmt, 2);
            const char *failed = (const char *)sqlite3_column_text(stmt, 3);
            int errors = sqlite3_column_int(stmt, 4);

            json->Key("id"); json->Int64(id);
            json->Key("url"); json->String(url);
            json->Key("name"); json->StringOrNull(name);
            if (checked) {
                json->Key("checked"); json->Int64(checked);
            } else {
                json->Key("checked"); json->Null();
            }
            if (failed) {
                json->Key("failed"); json->String(failed);
            } else {
                json->Key("failed"); json->Null();
            }
            json->Key("errors"); json->Int(errors);
        }

        // Channels
        {
            sq_Statement stmt;
            if (!db.Prepare(R"(SELECT name, oid, timestamp, size, count, ignore
                               FROM channels
                               WHERE repository = ?1)",
                            &stmt, id))
                return;

            json->Key("channels"); json->StartArray();
            while (stmt.Step()) {
                const char *name = (const char *)sqlite3_column_text(stmt, 0);
                const char *oid = (const char *)sqlite3_column_text(stmt, 1);
                int64_t time = sqlite3_column_int64(stmt, 2);
                int64_t size = sqlite3_column_int64(stmt, 3);
                int64_t count = sqlite3_column_int64(stmt, 4);
                bool ignore = sqlite3_column_int(stmt, 5);

                json->StartObject();
                json->Key("name"); json->String(name);
                json->Key("oid"); json->String(oid);
                json->Key("time"); json->Int64(time);
                json->Key("size"); json->Int64(size);
                json->Key("count"); json->Int64(count);
                json->Key("ignore"); json->Bool(ignore);
                json->EndObject();
            }
            if (!stmt.IsValid())
                return;
            json->EndArray();
        }

        json->EndObject();
    });
}

void HandleRepositorySave(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    int64_t id = -1;
    const char *name = nullptr;
    {
        bool success = http_ParseJson(io, Kibibytes(4), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "id") {
                    json->ParseInt(&id);
                } else if (key == "name") {
                    json->SkipNull() || json->ParseString(&name);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (name && !name[0]) {
                    LogError("Invalid 'name' parameter");
                    valid = false;
                }
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
    }

    // Update repository
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(UPDATE repositories SET name = ?3
                           WHERE id = ?1 AND owner = ?2
                           RETURNING id)",
                        &stmt, id, session->userid, name))
            return;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Unknown repository ID %1", id);
                io->SendError(404);
            }
            return;
        }
    }

    io->SendText(200, "{}", "application/json");
}

void HandleRepositoryDelete(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    int64_t id = -1;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "id") {
                    json->ParseInt(&id);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (id < 0) {
                    LogError("Missing or invalid 'id' parameter");
                    valid = false;
                }
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
    }

    if (!db.Run("DELETE FROM repositories WHERE id = ?1 AND owner = ?2", id, session->userid))
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleRepositorySnapshots(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    int64_t id = -1;
    const char *channel = nullptr;
    {
        const char *str = request.GetQueryValue("id");

        if (!str || !ParseInt(str, &id, (int)ParseFlag::End)) {
            LogError("Missing or invalid repository ID");
            io->SendError(422);
            return;
        }

        channel = request.GetQueryValue("channel");

        if (!channel || !channel[0]) {
            LogError("Missing or invalid channel name");
            io->SendError(422);
            return;
        }
    }

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT s.oid, s.timestamp, s.size, s.stored, s.added
                       FROM snapshots s
                       INNER JOIN repositories r ON (r.id = s.repository)
                       WHERE r.owner = ?1 AND r.id = ?2 AND
                             s.channel = ?3)",
                    &stmt, session->userid, id, channel))
        return;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();

        while (stmt.Step()) {
            const char *oid = (const char *)sqlite3_column_text(stmt, 0);
            int64_t time = sqlite3_column_int64(stmt, 1);
            int64_t size = sqlite3_column_int64(stmt, 2);
            int64_t stored = sqlite3_column_int64(stmt, 3);
            int64_t added = sqlite3_column_int64(stmt, 4);

            json->StartObject();
            json->Key("oid"); json->String(oid);
            json->Key("time"); json->Int64(time);
            json->Key("size"); json->Int64(size);
            json->Key("stored"); json->Int64(stored);
            if (added) {
                json->Key("added"); json->Int64(added);
            } else {
                json->Key("added"); json->Null();
            }
            json->EndObject();
        }
        if (!stmt.IsValid())
            return;

        json->EndArray();
    });
}

}
