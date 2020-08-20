// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "config.hh"
#include "goupile.hh"
#include "user.hh"
#include "../../core/libwrap/sqlite.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static http_SessionManager sessions;

static const int64_t PruneDelay = 86400000;
static const int64_t TokenLimit = 14 * 86400000;

RetainPtr<const Session> GetCheckedSession(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = sessions.Find<const Session>(request, io);
    return session;
}

static Size ConvertToJsName(const char *name, Span<char> out_buf)
{
    // This is used for static strings (e.g. permission names), and the Span<char>
    // output buffer will abort debug builds on out-of-bounds access.

    if (name[0]) {
        out_buf[0] = LowerAscii(name[0]);

        Size j = 1;
        for (Size i = 1; name[i]; i++) {
            if (name[i] >= 'A' && name[i] <= 'Z') {
                out_buf[j++] = '_';
                out_buf[j++] = LowerAscii(name[i]);
            } else {
                out_buf[j++] = name[i];
            }
        }
        out_buf[j] = 0;

        return j;
    } else {
        out_buf[0] = 0;
        return 0;
    }
}

static void WriteProfileJson(const Session *session, json_Writer *out_json)
{
    out_json->StartObject();
    if (session) {
        out_json->Key("username"); out_json->String(session->username);
        out_json->Key("permissions"); out_json->StartObject();
        for (Size i = 0; i < RG_LEN(UserPermissionNames); i++) {
            char js_name[64];
            ConvertToJsName(UserPermissionNames[i], js_name);

            out_json->Key(js_name); out_json->Bool(session->permissions & (1 << i));
        }
        out_json->EndObject();
    }
    out_json->EndObject();
}

void HandleProfile(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    http_JsonPageBuilder json(request.compression_type);
    WriteProfileJson(session.GetRaw(), &json);
    json.Finish(io);
}

static RetainPtr<Session> CreateSessionFromRow(const http_RequestInfo &request, http_IO *io, sq_Statement &stmt)
{
    const char *username = (const char *)sqlite3_column_text(stmt, 0);

    Session *session = (Session *)Allocator::Allocate(nullptr, RG_SIZE(Session) + strlen(username) + 1,
                                                      (int)Allocator::Flag::Zero);

    session->permissions |= !!sqlite3_column_int(stmt, 1) * (int)UserPermission::Develop;
    session->permissions |= !!sqlite3_column_int(stmt, 2) * (int)UserPermission::New;
    session->permissions |= !!sqlite3_column_int(stmt, 3) * (int)UserPermission::Edit;
    strcpy(session->username, username);

    RetainPtr<Session> ptr(session, [](Session *session) { Allocator::Release(nullptr, session, -1); });
    sessions.Open(request, io, ptr);

    return ptr;
}

static void PruneStaleTokens()
{
    int64_t now = GetMonotonicTime();

    static std::mutex last_pruning_mutex;
    static int64_t last_pruning = 0;
    {
        std::lock_guard<std::mutex> lock(last_pruning_mutex);
        if (now - last_pruning < PruneDelay)
            return;
        last_pruning = now;
    }

    int64_t limit = GetUnixTime() - TokenLimit;
    goupile_db.Run("DELETE FROM users_tokens WHERE login_time < ?", limit);
}

void HandleLogin(const http_RequestInfo &request, http_IO *io)
{
    PruneStaleTokens();

    io->RunAsync([=]() {
        // Read POST values
        HashMap<const char *, const char *> values;
        if (!io->ReadPostValues(&io->allocator, &values)) {
            io->AttachError(422);
            return;
        }

        const char *username = values.FindValue("username", nullptr);
        const char *password = values.FindValue("password", nullptr);
        if (!username || !password) {
            LogError("Missing parameters");
            io->AttachError(422);
            return;
        }

        // We use this to extend/fix the response delay in case of error
        int64_t now = GetMonotonicTime();

        sq_Statement stmt;
        if (!goupile_db.Prepare(R"(SELECT u.username, u.develop, u.new, u.edit, u.password_hash
                                   FROM users u
                                   WHERE u.username = ?)", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

        if (stmt.Next()) {
            const char *password_hash = (const char *)sqlite3_column_text(stmt, 4);

            if (crypto_pwhash_str_verify(password_hash, password, strlen(password)) == 0) {
                char token[65];
                {
                    uint64_t buf[4];
                    randombytes_buf(&buf, RG_SIZE(buf));
                    Fmt(token, "%1%2%3%4", FmtHex(buf[0]).Pad0(-16), FmtHex(buf[1]).Pad0(-16),
                                           FmtHex(buf[2]).Pad0(-16), FmtHex(buf[3]).Pad0(-16));
                }

                int64_t login_time = GetUnixTime();
                if (!goupile_db.Run("INSERT INTO users_tokens (token, username, login_time) VALUES (?, ?, ?)",
                                    token, username, login_time))
                    return;

                RetainPtr<Session> session = CreateSessionFromRow(request, io, stmt);

                http_JsonPageBuilder json(request.compression_type);
                json.StartObject();
                json.Key("token"); json.String(token);
                json.Key("profile"); WriteProfileJson(session.GetRaw(), &json);
                json.EndObject();
                json.Finish(io);
            }
        } else if (stmt.IsValid()) {
            // Enforce constant delay if authentification fails
            int64_t safety_delay = std::max(2000 - GetMonotonicTime() + now, (int64_t)0);
            WaitForDelay(safety_delay);

            LogError("Invalid token");
            io->AttachError(403);
        } else {
            LogError("SQLite Error: %1", sqlite3_errmsg(goupile_db));
        }
    });
}

void HandleReconnect(const http_RequestInfo &request, http_IO *io)
{
    PruneStaleTokens();

    io->RunAsync([=]() {
        // Read POST values
        HashMap<const char *, const char *> values;
        if (!io->ReadPostValues(&io->allocator, &values)) {
            io->AttachError(422);
            return;
        }

        const char *token = values.FindValue("token", nullptr);
        if (!token) {
            LogError("Missing parameters");
            io->AttachError(422);
            return;
        }

        int64_t now = GetMonotonicTime();
        int64_t limit = GetUnixTime() - TokenLimit;

        sq_Statement stmt;
        if (!goupile_db.Prepare(R"(SELECT u.username, u.develop, u.new, u.edit
                                   FROM users_tokens t
                                   INNER JOIN users u ON (u.username = t.username)
                                   WHERE t.token = ? AND t.login_time >= ?)", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, limit);

        if (stmt.Next()) {
            RetainPtr<Session> session = CreateSessionFromRow(request, io, stmt);

            http_JsonPageBuilder json(request.compression_type);
            WriteProfileJson(session.GetRaw(), &json);
            json.Finish(io);
        } else if (stmt.IsValid()) {
            // Enforce constant delay if authentification fails
            int64_t safety_delay = std::max(2000 - GetMonotonicTime() + now, (int64_t)0);
            WaitForDelay(safety_delay);

            LogError("Incorrect username or password");
            io->AttachError(403);
        } else {
            LogError("SQLite Error: %1", sqlite3_errmsg(goupile_db));
        }
    });
}

void HandleLogout(const http_RequestInfo &request, http_IO *io)
{
    sessions.Close(request, io);
    io->AttachText(200, "");
}

}
