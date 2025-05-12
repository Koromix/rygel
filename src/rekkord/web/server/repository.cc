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
#include "backord.hh"
#include "repository.hh"
#include "user.hh"
#include "../../lib/librekkord.hh"

namespace RG {

enum class OpenError {
    FailedConnection,
    AuthenticationError
};

static std::unique_ptr<rk_Disk> OpenRepository(const rk_Config &repo, OpenError *out_error)
{
    RG_ASSERT(repo.Validate(true));

    std::unique_ptr<rk_Disk> disk = rk_Open(repo, false);

    if (!disk) {
        *out_error = OpenError::FailedConnection;
        return nullptr;
    }
    if (!disk->Authenticate(repo.username, repo.password)) {
        *out_error = OpenError::AuthenticationError;
        return nullptr;
    }

    return disk;
}

static bool FillConfig(const char *url, const char *user, const char *password,
                       const HashMap<const char *, const char *> variables, rk_Config *out_repo)
{
    rk_Config repo;

    if (!rk_DecodeURL(url, &repo))
        return false;

    switch (repo.type) {
        case rk_DiskType::S3:
        case rk_DiskType::SFTP: {} break;

        case rk_DiskType::Local: {
            LogError("Unsupported URL '%1'", url);
            return false;
        }
    }

    repo.username = user;
    repo.password = password;

    switch (repo.type) {
        case rk_DiskType::Local: { RG_UNREACHABLE(); } break;

        case rk_DiskType::S3: {
            repo.s3.access_id = variables.FindValue("AWS_ACCESS_KEY_ID", nullptr);
            repo.s3.access_key = variables.FindValue("AWS_SECRET_ACCESS_KEY", nullptr);
        } break;

        case rk_DiskType::SFTP: {
            repo.ssh.known_hosts = false;
            repo.ssh.password = variables.FindValue("SSH_PASSWORD", nullptr);
            repo.ssh.key = variables.FindValue("SSH_KEY", nullptr);
            repo.ssh.fingerprint = variables.FindValue("SSH_FINGERPRINT", nullptr);
        } break;
    }

    if (!repo.Validate(true))
        return false;

    std::swap(*out_repo, repo);
    return true;
}

bool CheckRepositories()
{
    BlockAllocator temp_alloc;

    int64_t now = GetUnixTime();

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT r.id, r.url, r.user, r.password, v.key, v.value
                       FROM repositories r
                       LEFT JOIN variables v
                       WHERE r.checked - IIF(r.errors = 0, ?2, ?3) <= ?1
                       ORDER BY r.id)", &stmt, now, config.update_delay, config.retry_delay))
        return false;
    if (!stmt.Run())
        return false;

    Async async;
    BucketArray<rk_Config> repositories;

    while (stmt.IsRow()) {
        rk_Config *repo = repositories.AppendDefault();

        int64_t id = sqlite3_column_int64(stmt, 0);
        const char *url = DuplicateString((const char *)sqlite3_column_text(stmt, 1), &temp_alloc).ptr;
        const char *user = DuplicateString((const char *)sqlite3_column_text(stmt, 2), &temp_alloc).ptr;
        const char *password = DuplicateString((const char *)sqlite3_column_text(stmt, 3), &temp_alloc).ptr;

        HashMap<const char *, const char *> variables;
        do {
            if (sqlite3_column_int64(stmt, 0) != id)
                break;
            if (sqlite3_column_type(stmt, 4) == SQLITE_NULL)
                break;

            const char *key = DuplicateString((const char *)sqlite3_column_text(stmt, 4), &temp_alloc).ptr;
            const char *value = DuplicateString((const char *)sqlite3_column_text(stmt, 5), &temp_alloc).ptr;

            variables.Set(key, value);
        } while (stmt.Step());

        if (!FillConfig(url, user, password, variables, repo))
            continue;

        async.Run([=]() {
            BlockAllocator temp_alloc;

            const char *last_err = nullptr;

            // Keep last error message
            PushLogFilter([&](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
                if (level == LogLevel::Error) {
                    last_err = DuplicateString(msg, &temp_alloc).ptr;
                }

                func(level, ctx, msg);
            });
            RG_DEFER { PopLogFilter(); };

            OpenError error;
            std::unique_ptr<rk_Disk> disk = OpenRepository(*repo, &error);

            if (!disk) {
                db.Run(R"(UPDATE repositories SET checked = ?2,
                                                  failed = ?3,
                                                  errors = errors + 1
                          WHERE id = ?1)", id, now, last_err);
                return false;
            }

            HeapArray<rk_ChannelInfo> channels;
            if (!rk_Channels(disk.get(), &temp_alloc, &channels)) {
                db.Run(R"(UPDATE repositories SET checked = ?2,
                                                  failed = ?3,
                                                  errors = errors + 1
                          WHERE id = ?1)", id, now, last_err);
                return false;
            }

            for (const rk_ChannelInfo &channel: channels) {
                char hash[128];
                Fmt(hash, "%1", channel.hash);

                if (!db.Run(R"(INSERT INTO channels (repository, name, hash, timestamp, size, count, ignore)
                               VALUES (?1, ?2, ?3, ?4, ?5, ?6, 0)
                               ON CONFLICT DO UPDATE SET hash = excluded.hash,
                                                         timestamp = excluded.timestamp,
                                                         size = excluded.size,
                                                         count = excluded.count)",
                            id, channel.name, hash, channel.time, channel.size, channel.count))
                    return false;
            }

            db.Run("UPDATE repositories SET checked = ?2, failed = NULL, errors = 0 WHERE id = ?1", id, now);

            return true;
        });
    }
    if (!stmt.IsValid())
        return false;

    // Some tasks may fail but it is not critical
    async.Sync();

    return true;
}

void HandleRepositoryList(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(404);
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
        io->SendError(404);
        return;
    }

    // Get and check vault ID
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
    if (!db.Prepare(R"(SELECT name, url, user, password, checked, failed, errors
                       FROM repositories
                       WHERE owner = ?1 AND id = ?2)", &stmt, session->userid, id))
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
        const char *user = (const char *)sqlite3_column_text(stmt, 2);
        const char *password = (const char *)sqlite3_column_text(stmt, 3);
        int64_t checked = sqlite3_column_int64(stmt, 4);
        const char *failed = (const char *)sqlite3_column_text(stmt, 5);
        int errors = sqlite3_column_int(stmt, 6);

        json.Key("id"); json.Int64(id);
        json.Key("name"); json.String(name);
        json.Key("url"); json.String(url);
        json.Key("user"); json.String(user);
        json.Key("password"); json.String(password);
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

    // Variables
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT key, value FROM variables WHERE repository = ?1", &stmt, id))
            return;

        json.Key("variables"); json.StartObject();
        while (stmt.Step()) {
            const char *key = (const char *)sqlite3_column_text(stmt, 0);
            const char *value = (const char *)sqlite3_column_text(stmt, 1);

            json.Key(key); json.String(value);
        }
        if (!stmt.IsValid())
            return;
        json.EndObject();
    }

    // Channels
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT name, hash, timestamp, size, count, ignore FROM channels WHERE repository = ?1", &stmt, id))
            return;

        json.Key("channels"); json.StartArray();
        while (stmt.Step()) {
            const char *name = (const char *)sqlite3_column_text(stmt, 0);
            const char *hash = (const char *)sqlite3_column_text(stmt, 1);
            int64_t time = sqlite3_column_int64(stmt, 2);
            int64_t size = sqlite3_column_int64(stmt, 3);
            int64_t count = sqlite3_column_int64(stmt, 4);
            bool ignore = sqlite3_column_int(stmt, 5);

            json.StartObject();
            json.Key("name"); json.String(name);
            json.Key("hash"); json.String(hash);
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
        io->SendError(404);
        return;
    }

    // Parse input data
    int64_t id = -1;
    const char *name = nullptr;
    const char *url = nullptr;
    const char *user = nullptr;
    const char *password = nullptr;
    HashMap<const char *, const char *> variables;
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
            } else if (key == "user") {
                parser.ParseString(&user);
            } else if (key == "password") {
                parser.ParseString(&password);
            } else if (key == "variables") {
                parser.ParseObject();
                while (parser.InObject()) {
                    Span<const char> key = {};
                    Span<const char> value = {};
                    parser.ParseKey(&key);
                    parser.ParseString(&value);

                    variables.Set(key.ptr, value.ptr);
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

    rk_Config repo;

    // Check missing or invalid values
    {
        bool valid = true;

        if (!name || !name[0]) {
            LogError("Missing or invalid 'name' parameter");
            valid = false;
        }
        if (!user || !user[0]) {
            LogError("Missing or invalid 'user' parameter");
            valid = false;
        }
        if (!password || !password[0]) {
            LogError("Missing or invalid 'password' parameter");
            valid = false;
        }

        if (url) {
            valid &= FillConfig(url, user, password, variables, &repo);
        } else {
            LogError("Missing 'url' value");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    // Make sure it works
    {
        OpenError error;
        std::unique_ptr<rk_Disk> disk = OpenRepository(repo, &error);

        if (!disk) {
            switch (error) {
                case OpenError::FailedConnection: { io->SendError(404); } break;
                case OpenError::AuthenticationError: { io->SendError(403); } break;
            }
            return;
        }
    }

    // Create or update repository
    bool success = db.Transaction([&]() {
        sq_Statement stmt;
        if (!db.Prepare(R"(INSERT INTO repositories (id, owner, name, url, user, password, checked, failed, errors)
                           VALUES (?1, ?2, ?3, ?4, ?5, ?6, 0, NULL, 0)
                           ON CONFLICT DO UPDATE SET id = IF(owner = excluded.owner, id, NULL),
                                                     name = excluded.name,
                                                     url = excluded.url,
                                                     user = excluded.user,
                                                     password = excluded.password,
                                                     checked = excluded.checked
                           RETURNING id)",
                        &stmt, id >= 0 ? sq_Binding(id) : sq_Binding(), session->userid,
                        name, url, user, password))
            return false;
        if (!stmt.GetSingleValue(&id))
            return false;

        if (!db.Run("DELETE FROM variables WHERE repository = ?1", id))
            return false;

        for (const auto &it: variables.table) {
            if (!db.Run("INSERT INTO variables (repository, key, value) VALUES (?1, ?2, ?3)",
                        id, it.key, it.value))
                return false;
        }

        return true;
    });
    if (!success)
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleRepositoryDelete(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(404);
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

}
