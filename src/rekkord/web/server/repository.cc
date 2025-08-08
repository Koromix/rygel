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

static smtp_MailContent FailureMessage = {
    "[Error] {{ TITLE }}: {{ REPOSITORY }}",
    R"(Failed to check for {{ REPOSITORY }}:\n{{ ERROR }})",
    R"(<html lang="en"><body><p>Failed to check for <b>{{ REPOSITORY }}</b>:</p><p style="color: red;">{{ ERROR }}</p></body></html>)",
    {}
};

static smtp_MailContent FailureResolved = {
    "[Resolved] {{ TITLE }}: {{ REPOSITORY }}",
    R"(Access to {{ REPOSITORY }} is now back on track!)",
    R"(<html lang="en"><body><p>Access to <b>{{ REPOSITORY }}</b> is now back on track!</p></body></html>)",
    {}
};

static smtp_MailContent StaleMessage = {
    "[Stale] {{ TITLE }}: {{ REPOSITORY }} channel {{ CHANNEL }}",
    R"(Repository {{ REPOSITORY }} channel {{ CHANNEL }} looks stale.\n\nLast snapshot: {{ TIMESTAMP }})",
    R"(<html lang="en"><body><p>Repository <b>{{ REPOSITORY }}</b> channel <b>{{ CHANNEL }}</b> looks stale.</p><p>Last snapshot: <b>{{ TIMESTAMP }}</b></p></body></html>)",
    {}
};

static smtp_MailContent StaleResolved = {
    "[Resolved] {{ TITLE }}: {{ REPOSITORY }} channel {{ CHANNEL }}",
    R"(Repository {{ REPOSITORY }} channel {{ CHANNEL }} is now back on track!\n\nLast snapshot: {{ TIMESTAMP }})",
    R"(<html lang="en"><body><p>Repository <b>{{ REPOSITORY }}</b> channel <b>{{ CHANNEL }}</b> is now back on track!.</p><p>Last snapshot: <b>{{ TIMESTAMP }}</b></p></body></html>)",
    {}
};

static bool FillConfig(const char *url, const char *user, const char *password,
                       const HashMap<const char *, const char *> variables, rk_Config *out_repo)
{
    rk_Config access;

    if (!rk_DecodeURL(url, &access))
        return false;

    switch (access.type) {
        case rk_DiskType::S3:
        case rk_DiskType::SFTP: {} break;

        case rk_DiskType::Local: {
            LogError("Unsupported URL '%1'", url);
            return false;
        }
    }

    access.username = user;
    access.password = password;

    switch (access.type) {
        case rk_DiskType::Local: { RG_UNREACHABLE(); } break;

        case rk_DiskType::S3: {
            access.s3.remote.access_id = variables.FindValue("S3_ACCESS_KEY_ID", nullptr);
            access.s3.remote.access_key = variables.FindValue("S3_SECRET_ACCESS_KEY", nullptr);
        } break;

        case rk_DiskType::SFTP: {
            access.ssh.known_hosts = false;
            access.ssh.password = variables.FindValue("SSH_PASSWORD", nullptr);
            access.ssh.key = variables.FindValue("SSH_KEY", nullptr);
            access.ssh.fingerprint = variables.FindValue("SSH_FINGERPRINT", nullptr);
        } break;
    }

    if (!access.Validate(true))
        return false;

    std::swap(*out_repo, access);
    return true;
}

static bool ListSnapshots(const rk_Config &access, Allocator *alloc,
                          HeapArray<rk_SnapshotInfo> *out_snapshots, HeapArray<rk_ChannelInfo> *out_channels)
{
    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(access);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), access, true);

    if (!repo)
        return false;

    Size prev_snapshots = out_snapshots->len;
    if (!rk_ListSnapshots(repo.get(), alloc, out_snapshots))
        return false;
    rk_ListChannels(out_snapshots->Take(prev_snapshots, out_snapshots->len - prev_snapshots), alloc, out_channels);

    return true;
}

static bool UpdateSnapshots(int64_t id, int64_t now, Span<const rk_SnapshotInfo> snapshots, Span<const rk_ChannelInfo> channels)
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

static bool CheckRepository(const rk_Config &access, int64_t id)
{
    BlockAllocator temp_alloc;

    int64_t now = GetUnixTime();

    HeapArray<rk_SnapshotInfo> snapshots;
    HeapArray<rk_ChannelInfo> channels;

    // Fetch snapshots
    {
        const char *last_err = nullptr;

        // Keep last error message
        PushLogFilter([&](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
            if (level == LogLevel::Error) {
                last_err = DuplicateString(msg, &temp_alloc).ptr;
            }

            func(level, ctx, msg);
        });
        RG_DEFER { PopLogFilter(); };

        if (!ListSnapshots(access, &temp_alloc, &snapshots, &channels)) {
            bool success = db.Transaction([&]() {
                if (!db.Run(R"(UPDATE repositories SET checked = ?2,
                                                       failed = ?3,
                                                       errors = errors + 1
                               WHERE id = ?1)", id, now, last_err))
                    return false;

                if (!db.Run(R"(INSERT INTO failures (repository, timestamp, message, resolved)
                               VALUES (?1, ?2, ?3, 0)
                               ON CONFLICT DO UPDATE SET resolved = 0)", id, now, last_err))
                    return false;

                return true;
            });
            if (!success)
                return false;

            return true;
        }
    }

    if (!UpdateSnapshots(id, now, snapshots, channels))
        return false;

    return true;
}

static Span<const char> PatchFailure(Span<const char> text, const char *repository, const char *message, Allocator *alloc)
{
    Span<const char> ret = PatchFile(text, alloc, [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "TITLE") {
            writer->Write(config.title);
        } else if (key == "REPOSITORY") {
            writer->Write(repository);
        } else if (key == "ERROR") {
            writer->Write(message);
        } else {
            Print(writer, "{{%1}}", expr);
        }
    });

    return ret;
}

static Span<const char> PatchStale(Span<const char> text, const char *repository,
                                   const char *channel, int64_t timestamp, Allocator *alloc)
{
    Span<const char> ret = PatchFile(text, alloc, [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "TITLE") {
            writer->Write(config.title);
        } else if (key == "REPOSITORY") {
            writer->Write(repository);
        } else if (key == "CHANNEL") {
            writer->Write(channel);
        } else if (key == "TIMESTAMP") {
            TimeSpec spec = DecomposeTimeUTC(timestamp);
            Print(writer, "%1", FmtTimeNice(spec));
        } else {
            Print(writer, "{{%1}}", expr);
        }
    });

    return ret;
}

bool CheckRepositories()
{
    BlockAllocator temp_alloc;

    int64_t now = GetUnixTime();

    // Check repositories
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(SELECT r.id, r.url, r.user, r.password, v.key, v.value
                           FROM repositories r
                           LEFT JOIN variables v
                           WHERE r.checked + IIF(r.errors = 0, ?2, ?3) <= ?1
                           ORDER BY r.id)",
                        &stmt, now, config.update_period, config.retry_delay))
            return false;
        if (!stmt.Run())
            return false;

        Async async;
        BucketArray<rk_Config> repositories;

        while (stmt.IsRow()) {
            rk_Config *access = repositories.AppendDefault();

            int64_t id = sqlite3_column_int64(stmt, 0);
            const char *url = DuplicateString((const char *)sqlite3_column_text(stmt, 1), &temp_alloc).ptr;
            const char *user = DuplicateString((const char *)sqlite3_column_text(stmt, 2), &temp_alloc).ptr;
            const char *password = DuplicateString((const char *)sqlite3_column_text(stmt, 3), &temp_alloc).ptr;

            LogDebug("Checking repository '%1'", url);

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

            if (!FillConfig(url, user, password, variables, access))
                continue;

            async.Run([=]() { return CheckRepository(*access, id); });
        }
        if (!stmt.IsValid())
            return false;

        stmt.Finalize();

        if (!async.Sync())
            return false;
    }

    // Post error alerts
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(SELECT f.id, u.mail, r.name, f.message, f.resolved
                           FROM failures f
                           INNER JOIN repositories r ON (r.id = f.repository)
                           INNER JOIN users u ON (u.id = r.owner)
                           WHERE f.timestamp < ?1 AND
                                 (f.sent IS NULL OR f.sent < ?2))",
                        &stmt, now - config.mail_delay, now - config.repeat_delay))
            return false;

        while (stmt.Step()) {
            bool success = db.Transaction([&]() {
                int64_t id = sqlite3_column_int64(stmt, 0);
                const char *to = (const char *)sqlite3_column_text(stmt, 1);
                const char *name = (const char *)sqlite3_column_text(stmt, 2);
                const char *error = (const char *)sqlite3_column_text(stmt, 3);
                bool unsolved = !sqlite3_column_int(stmt, 4);

                smtp_MailContent content = unsolved ? FailureMessage : FailureResolved;

                content.subject = PatchFailure(content.subject, name, error, &temp_alloc).ptr;
                content.text = PatchFailure(content.text, name, error, &temp_alloc).ptr;
                content.html = PatchFailure(content.html, name, error, &temp_alloc).ptr;

                if (!PostMail(to, content))
                    return false;

                if (unsolved) {
                    if (!db.Run("UPDATE failures SET sent = ?2 WHERE id = ?1", id, now))
                        return false;
                } else {
                    if (!db.Run("DELETE FROM failures WHERE id = ?1", id))
                        return false;
                }

                return true;
            });
            if (!success)
                return false;
        }
        if (!stmt.IsValid())
            return false;
    }

    // Post stale alerts
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(SELECT s.id, u.mail, r.name, s.channel, s.timestamp, s.resolved
                           FROM stales s
                           INNER JOIN repositories r ON (r.id = s.repository)
                           INNER JOIN users u ON (u.id = r.owner)
                           WHERE s.timestamp < ?1 AND
                                 (s.sent IS NULL OR s.sent < ?2))",
                        &stmt, now - config.mail_delay, now - config.repeat_delay))
            return false;

        while (stmt.Step()) {
            bool success = db.Transaction([&]() {
                int64_t id = sqlite3_column_int64(stmt, 0);
                const char *to = (const char *)sqlite3_column_text(stmt, 1);
                const char *name = (const char *)sqlite3_column_text(stmt, 2);
                const char *channel = (const char *)sqlite3_column_text(stmt, 3);
                int64_t timestamp = sqlite3_column_int64(stmt, 4);
                bool unsolved = !sqlite3_column_int(stmt, 5);

                smtp_MailContent content = unsolved ? StaleMessage : StaleResolved;

                content.subject = PatchStale(content.subject, name, channel, timestamp, &temp_alloc).ptr;
                content.text = PatchStale(content.text, name, channel, timestamp, &temp_alloc).ptr;
                content.html = PatchStale(content.html, name, channel, timestamp, &temp_alloc).ptr;

                if (!PostMail(to, content))
                    return false;

                if (unsolved) {
                    if (!db.Run("UPDATE stales SET sent = ?2 WHERE id = ?1", id, now))
                        return false;
                } else {
                    if (!db.Run("DELETE FROM stales WHERE id = ?1", id))
                        return false;
                }

                return true;
            });
            if (!success)
                return false;
        }
        if (!stmt.IsValid())
            return false;
    }

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
        if (!db.Prepare("SELECT name, oid, timestamp, size, count, ignore FROM channels WHERE repository = ?1", &stmt, id))
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

    rk_Config access;

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
            valid &= FillConfig(url, user, password, variables, &access);
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
        std::unique_ptr<rk_Disk> disk = rk_OpenDisk(access);
        std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), access, true);

        if (!disk) {
            io->SendError(404);
            return;
        }
        if (!repo) {
            io->SendError(403);
            return;
        }
        if (repo->GetModes() != (int)rk_AccessMode::Log) {
            LogError("Select repository user with LogOnly role and nothing else");
            io->SendError(403);
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

    const char *json = Fmt(io->Allocator(), "{\"id\": %1}", id).ptr;
    io->SendText(200, json, "application/json");
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

void HandleRepositorySnapshots(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(404);
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
