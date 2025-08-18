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
#include "src/core/http/http.hh"
#include "web.hh"
#include "mail.hh"
#include "repository.hh"
#include "user.hh"
#include "../../lib/librekkord.hh"

namespace RG {

static bool CheckURL(const char *url)
{
    rk_Config config;

    if (!rk_DecodeURL(url, &config))
        return false;

    bool valid = false;

    switch (config.type) {
        case rk_DiskType::Local: {} break;
        case rk_DiskType::S3: { valid = config.s3.remote.host; } break;
        case rk_DiskType::SFTP: { valid = config.ssh.host; } break;
    }

    if (!valid) {
        LogError("Unsupported URL '%1'", url);
        return false;
    }

    return true;
}

static bool UpdateSnapshots(int64_t id, int64_t now,
                            Span<const rk_SnapshotInfo> snapshots, Span<const rk_ChannelInfo> channels)
{
    bool success = db.Transaction([&]() {
        if (!db.Run(R"(UPDATE failures SET resolved = 1,
                                           sent = NULL
                       WHERE repository = ?1 AND
                             resolved = 0)", id))
            return false;

        for (const rk_SnapshotInfo &snapshot: snapshots) {
            char oid[128];
            Fmt(oid, "%1", snapshot.oid);

            if (!db.Run(R"(INSERT INTO snapshots (repository, oid, channel, timestamp, size, stored, added)
                           VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)
                           ON CONFLICT DO UPDATE SET channel = excluded.channel,
                                                     timestamp = excluded.timestamp,
                                                     size = excluded.size,
                                                     stored = excluded.stored,
                                                     added = excluded.added)",
                        id, oid, snapshot.channel, snapshot.time,
                        snapshot.size, snapshot.stored, snapshot.added))
                return false;
        }

        for (const rk_ChannelInfo &channel: channels) {
            char oid[128];
            Fmt(oid, "%1", channel.oid);

            if (!db.Run(R"(INSERT INTO channels (repository, name, oid, timestamp, size, count, ignore)
                           VALUES (?1, ?2, ?3, ?4, ?5, ?6, 0)
                           ON CONFLICT DO UPDATE SET oid = excluded.oid,
                                                     timestamp = excluded.timestamp,
                                                     size = excluded.size,
                                                     count = excluded.count)",
                        id, channel.name, oid, channel.time, channel.size, channel.count))
                return false;

            if (now - channel.time >= config.stale_delay) {
                if (!db.Run(R"(INSERT INTO stales (repository, channel, timestamp, resolved)
                               VALUES (?1, ?2, ?3, 0)
                               ON CONFLICT DO UPDATE SET timestamp = excluded.timestamp,
                                                         resolved = 0)",
                            id, channel.name, channel.time))
                    return false;
            } else {
                if (!db.Run(R"(UPDATE stales SET resolved = 1,
                                                 sent = NULL
                               WHERE repository = ?1 AND
                                     channel = ?2 AND
                                     resolved = 0)",
                            id, channel.name))
                    return false;
            }
        }

        if (!db.Run("UPDATE repositories SET checked = ?2, failed = NULL, errors = 0 WHERE id = ?1", id, now))
            return false;

        return true;
    });
    if (!success)
        return false;

    return true;
}

void HandleRepositoryList(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT id, name, url, checked, failed, errors
                       FROM repositories
                       WHERE owner = ?1)", &stmt, session->userid))
        return;

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartArray();
    while (stmt.Step()) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        const char *url = (const char *)sqlite3_column_text(stmt, 2);
        int64_t checked = sqlite3_column_int64(stmt, 3);
        const char *failed = (const char *)sqlite3_column_text(stmt, 4);
        int errors = sqlite3_column_int(stmt, 5);

        json.StartObject();

        json.Key("id"); json.Int64(id);
        json.Key("name"); json.String(name);
        json.Key("url"); json.String(url);
        if (checked) {
            json.Key("checked"); json.Int64(checked);
        } else {
            json.Key("checked"); json.Null();
        }
        if (failed) {
            json.Key("failed"); json.String(failed);
        } else {
            json.Key("failed"); json.Null();
        }
        json.Key("errors"); json.Int(errors);

        json.EndObject();
    }
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish();
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
    if (!db.Prepare(R"(SELECT name, url, checked, failed, errors
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

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartObject();

    // Main information
    {
        const char *name = (const char *)sqlite3_column_text(stmt, 0);
        const char *url = (const char *)sqlite3_column_text(stmt, 1);
        int64_t checked = sqlite3_column_int64(stmt, 2);
        const char *failed = (const char *)sqlite3_column_text(stmt, 3);
        int errors = sqlite3_column_int(stmt, 4);

        json.Key("id"); json.Int64(id);
        json.Key("name"); json.String(name);
        json.Key("url"); json.String(url);
        if (checked) {
            json.Key("checked"); json.Int64(checked);
        } else {
            json.Key("checked"); json.Null();
        }
        if (failed) {
            json.Key("failed"); json.String(failed);
        } else {
            json.Key("failed"); json.Null();
        }
        json.Key("errors"); json.Int(errors);
    }

    // Channels
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(SELECT name, oid, timestamp, size, count, ignore
                           FROM channels
                           WHERE repository = ?1)",
                        &stmt, id))
            return;

        json.Key("channels"); json.StartArray();
        while (stmt.Step()) {
            const char *name = (const char *)sqlite3_column_text(stmt, 0);
            const char *oid = (const char *)sqlite3_column_text(stmt, 1);
            int64_t time = sqlite3_column_int64(stmt, 2);
            int64_t size = sqlite3_column_int64(stmt, 3);
            int64_t count = sqlite3_column_int64(stmt, 4);
            bool ignore = sqlite3_column_int(stmt, 5);

            json.StartObject();
            json.Key("name"); json.String(name);
            json.Key("oid"); json.String(oid);
            json.Key("time"); json.Int64(time);
            json.Key("size"); json.Int64(size);
            json.Key("count"); json.Int64(count);
            json.Key("ignore"); json.Bool(ignore);
            json.EndObject();
        }
        if (!stmt.IsValid())
            return;
        json.EndArray();
    }

    json.EndObject();

    json.Finish();
}

void HandleRepositorySave(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    // Parse input data
    int64_t id = -1;
    const char *name = nullptr;
    const char *url = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(4), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "id") {
                parser.SkipNull() || parser.ParseInt(&id);
            } else if (key == "name") {
                parser.ParseString(&name);
            } else if (key == "url") {
                parser.ParseString(&url);
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

        if (!name || !name[0]) {
            LogError("Missing or invalid 'name' parameter");
            valid = false;
        }

        if (url) {
            valid = CheckURL(url);
        } else {
            LogError("Missing 'url' value");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    // Create or update repository
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(INSERT INTO repositories (id, owner, name, url, checked, failed, errors)
                           VALUES (?1, ?2, ?3, ?4, 0, NULL, 0)
                           ON CONFLICT DO UPDATE SET id = IF(owner = excluded.owner, id, NULL),
                                                     name = excluded.name,
                                                     url = excluded.url,
                                                     checked = excluded.checked
                           RETURNING id)",
                        &stmt, id >= 0 ? sq_Binding(id) : sq_Binding(),
                        session->userid, name, url))
            return;
        if (!stmt.GetSingleValue(&id))
            return;
    }

    const char *json = Fmt(io->Allocator(), "{\"id\": %1}", id).ptr;
    io->SendText(200, json, "application/json");
}

void HandleRepositoryDelete(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    // Parse input data
    int64_t id = -1;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "id") {
                parser.ParseInt(&id);
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
    if (id < 0) {
        LogError("Missing or invalid 'id' parameter");
        io->SendError(422);
        return;
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

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartArray();

    while (stmt.Step()) {
        const char *oid = (const char *)sqlite3_column_text(stmt, 0);
        int64_t time = sqlite3_column_int64(stmt, 1);
        int64_t size = sqlite3_column_int64(stmt, 2);
        int64_t stored = sqlite3_column_int64(stmt, 3);
        int64_t added = sqlite3_column_int64(stmt, 4);

        json.StartObject();
        json.Key("oid"); json.String(oid);
        json.Key("time"); json.Int64(time);
        json.Key("size"); json.Int64(size);
        json.Key("stored"); json.Int64(stored);
        if (added) {
            json.Key("added"); json.Int64(added);
        } else {
            json.Key("added"); json.Null();
        }
        json.EndObject();
    }
    if (!stmt.IsValid())
        return;

    json.EndArray();

    json.Finish();
}

}
