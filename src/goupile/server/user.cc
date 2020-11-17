// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "domain.hh"
#include "goupile.hh"
#include "instance.hh"
#include "user.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static http_SessionManager sessions;

const Token *Session::GetToken(const InstanceData *instance) const
{
    Token *token;
    {
        std::shared_lock<std::shared_mutex> lock(tokens_lock);
        token = tokens_map.Find(instance);
    }

    if (!token) {
        if (!demo) {
            do {
                sq_Statement stmt;
                if (!goupile_domain.db.Prepare(R"(SELECT zone, permissions FROM dom_permissions
                                                  WHERE instance = ? AND username = ?;)", &stmt))
                    break;
                sqlite3_bind_text(stmt, 1, instance->key, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);
                if (!stmt.Next())
                    break;

                uint32_t permissions = sqlite3_column_int(stmt, 1);
                const char *zone = (const char *)sqlite3_column_text(stmt, 0);
                zone = zone ? DuplicateString(zone, &tokens_alloc).ptr : nullptr;

                std::lock_guard<std::shared_mutex> lock(tokens_lock);

                token = tokens_map.SetDefault(instance);
                token->zone = zone;
                token->permissions = permissions;
            } while (false);
        }

        // As last resort (after error or in demo session), give token without any permission
        if (!token) {
            std::lock_guard<std::shared_mutex> lock(tokens_lock);
            token = tokens_map.SetDefault(instance);
        }
    }

    return token;
}

static void WriteProfileJson(const Session *session, const Token *token, json_Writer *out_json)
{
    out_json->StartObject();

    if (session) {
        out_json->Key("username"); out_json->String(session->username);
        out_json->Key("admin"); out_json->Bool(session->admin);
        out_json->Key("demo"); out_json->Bool(session->demo);

        if (token) {
            if (token->zone) {
                out_json->Key("zone"); out_json->String(token->zone);
            } else {
                out_json->Key("zone"); out_json->Null();
            }
            out_json->Key("permissions"); out_json->StartObject();
            for (Size i = 0; i < RG_LEN(UserPermissionNames); i++) {
                char js_name[64];
                ConvertToJsonName(UserPermissionNames[i], js_name);

                out_json->Key(js_name); out_json->Bool(token->permissions & (1 << i));
            }
            out_json->EndObject();
        }
    }

    out_json->EndObject();
}

static RetainPtr<Session> CreateUserSession(const http_RequestInfo &request, http_IO *io,
                                            const char *username)
{
    Size len = RG_SIZE(Session) + strlen(username) + 1;
    Session *session = (Session *)Allocator::Allocate(nullptr, len, (int)Allocator::Flag::Zero);

    session->username = (char *)session + RG_SIZE(Session);
    strcpy((char *)session->username, username);

    RetainPtr<Session> ptr(session, [](Session *session) { Allocator::Release(nullptr, session, -1); });
    sessions.Open(request, io, ptr);

    return ptr;
}

static RetainPtr<Session> CreateDemoSession(const http_RequestInfo &request, http_IO *io)
{
    // Make sure there is no session related Set-Cookie header left
    io->ResetResponse();

    RetainPtr<Session>session = CreateUserSession(request, io, goupile_domain.config.demo_user);
    session->demo = true;

    return session;
}

RetainPtr<const Session> GetCheckedSession(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<Session> session = sessions.Find<Session>(request, io);

    if (goupile_domain.config.demo_user && !session) {
        session = CreateDemoSession(request, io);
    }

    return session;
}

void HandleUserLogin(InstanceData *instance, const http_RequestInfo &request, http_IO *io)
{
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
        if (!goupile_domain.db.Prepare(R"(SELECT password_hash, admin FROM dom_users
                                          WHERE username = ?)", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

        if (stmt.Next()) {
            const char *password_hash = (const char *)sqlite3_column_text(stmt, 0);
            bool admin = (sqlite3_column_int(stmt, 1) == 1);

            if (crypto_pwhash_str_verify(password_hash, password, strlen(password)) == 0) {
                int64_t time = GetUnixTime();

                if (!goupile_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username)
                                              VALUES (?, ?, ?, ?);)",
                                    time, request.client_addr, "login", username))
                    return;

                RetainPtr<Session> session = CreateUserSession(request, io, username);
                session->admin = admin;

                const Token *token = session->GetToken(instance);

                http_JsonPageBuilder json(request.compression_type);
                WriteProfileJson(session.GetRaw(), token, &json);
                json.Finish(io);

                return;
            }
        }

        if (stmt.IsValid()) {
            // Enforce constant delay if authentification fails
            int64_t safety_delay = std::max(2000 - GetMonotonicTime() + now, (int64_t)0);
            WaitForDelay(safety_delay);

            LogError("Invalid username or password");
            io->AttachError(403);
        } else {
            LogError("SQLite Error: %1", sqlite3_errmsg(goupile_domain.db));
        }
    });
}

void HandleUserLogout(InstanceData *, const http_RequestInfo &request, http_IO *io)
{
    sessions.Close(request, io);

    if (goupile_domain.config.demo_user) {
        CreateDemoSession(request, io);
    }

    io->AttachText(200, "");
}

void HandleUserProfile(InstanceData *instance, const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);
    const Token *token = session ? session->GetToken(instance) : nullptr;

    http_JsonPageBuilder json(request.compression_type);
    WriteProfileJson(session.GetRaw(), token, &json);
    json.Finish(io);
}

}
