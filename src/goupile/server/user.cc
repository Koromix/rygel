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

const Token *Session::GetToken(const InstanceHolder *instance) const
{
    if (instance) {
        Token *token;
        {
            std::shared_lock<std::shared_mutex> lock_shr(tokens_lock);
            token = tokens_map.Find(instance->unique);
        }

        if (!token) {
            do {
                sq_Statement stmt;
                if (!gp_domain.db.Prepare(R"(SELECT zone, permissions FROM dom_permissions
                                             WHERE userid = ?1 AND instance = ?2)", &stmt))
                    break;
                sqlite3_bind_int64(stmt, 1, userid);
                sqlite3_bind_text(stmt, 2, instance->key.ptr, (int)instance->key.len, SQLITE_STATIC);
                if (!stmt.Next())
                    break;

                uint32_t permissions = sqlite3_column_int(stmt, 1);
                const char *zone = (const char *)sqlite3_column_text(stmt, 0);

                std::lock_guard<std::shared_mutex> lock_excl(tokens_lock);

                token = tokens_map.SetDefault(instance->unique);
                token->zone = zone ? DuplicateString(zone, &tokens_alloc).ptr : nullptr;
                token->permissions = permissions;
            } while (false);

            // User is not assigned to this instance, cache this information
            if (!token) {
                std::lock_guard<std::shared_mutex> lock_excl(tokens_lock);
                token = tokens_map.SetDefault(instance->unique);
            }
        }

        return token->permissions ? token : nullptr;
    } else {
        return nullptr;
    }
}

static void WriteProfileJson(const Session *session, const Token *token, json_Writer *out_json)
{
    out_json->StartObject();

    if (session) {
        out_json->Key("userid"); out_json->Int64(session->userid);
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

        out_json->Key("passport"); out_json->String(session->passport);
    }

    out_json->EndObject();
}

static RetainPtr<Session> CreateUserSession(const http_RequestInfo &request, http_IO *io,
                                            int64_t userid, const char *username, const char *passport)
{
    Size username_len = strlen(username);
    Size len = RG_SIZE(Session) + username_len + 1;
    Session *session = (Session *)Allocator::Allocate(nullptr, len, (int)Allocator::Flag::Zero);

    new (session) Session;
    RetainPtr<Session> ptr(session, [](Session *session) {
        session->~Session();
        Allocator::Release(nullptr, session, -1);
    });

    session->userid = userid;
    session->username = (char *)session + RG_SIZE(Session);
    CopyString(username, MakeSpan((char *)session->username, username_len + 1));
    if (!CopyString(passport, session->passport)) {
        // Should never happen, but let's be careful
        LogError("User passport is too big");
        return {};
    }

    sessions.Open(request, io, ptr);

    return ptr;
}

static RetainPtr<Session> CreateDemoSession(const http_RequestInfo &request, http_IO *io)
{
    static std::once_flag once;
    static int64_t demo_userid = -1;
    static char demo_passport[RG_SIZE(Session::passport)];

    std::call_once(once, []() {
        sq_Statement stmt;
        if (!gp_domain.db.Prepare("SELECT userid, passport FROM dom_users WHERE username = ?1", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, gp_domain.config.demo_user, -1, SQLITE_STATIC);

        if (!stmt.Next()) {
            if (stmt.IsValid()) {
                LogError("Demo user '%1' does not exist", gp_domain.config.demo_user);
            }
            return;
        }

        int64_t userid = sqlite3_column_int64(stmt, 0);
        const char *passport = (const char *)sqlite3_column_text(stmt, 1);

        // Should never happen, but let's be careful
        if (RG_UNLIKELY(strlen(passport) >= RG_SIZE(demo_passport))) {
            LogError("Demo user passport is too big");
            return;
        }

        demo_userid = userid;
        CopyString(passport, demo_passport);
    });
    if (RG_UNLIKELY(demo_userid < 0))
        return {};

    // Make sure there is no session related Set-Cookie header left
    io->ResetResponse();

    RetainPtr<Session>session = CreateUserSession(request, io, demo_userid,
                                                  gp_domain.config.demo_user, demo_passport);
    session->demo = true;

    return session;
}

RetainPtr<const Session> GetCheckedSession(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<Session> session = sessions.Find<Session>(request, io);

    if (gp_domain.config.demo_user && !session) {
        session = CreateDemoSession(request, io);
    }

    return session;
}

void HandleUserLogin(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    io->RunAsync([=]() {
        // Read POST values
        const char *username;
        const char *password;
        {
            HashMap<const char *, const char *> values;
            if (!io->ReadPostValues(&io->allocator, &values)) {
                io->AttachError(422);
                return;
            }

            username = values.FindValue("username", nullptr);
            password = values.FindValue("password", nullptr);
            if (!username || !password) {
                LogError("Missing 'username' or 'password' parameter");
                io->AttachError(422);
                return;
            }
        }

        // We use this to extend/fix the response delay in case of error
        int64_t now = GetMonotonicTime();

        sq_Statement stmt;
        if (instance) {
            if (!gp_domain.db.Prepare(R"(SELECT u.userid, u.password_hash, u.admin, u.passport FROM dom_users u
                                         INNER JOIN dom_permissions p ON (p.userid = u.userid)
                                         WHERE u.username = ?1 AND
                                               p.instance = ?2 AND p.permissions > 0)", &stmt))
                return;
            sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, instance->key.ptr, (int)instance->key.len, SQLITE_STATIC);
        } else {
            if (!gp_domain.db.Prepare(R"(SELECT userid, password_hash, admin, passport FROM dom_users
                                         WHERE username = ?1 AND admin = 1)", &stmt))
                return;
            sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        }

        if (stmt.Next()) {
            int64_t userid = sqlite3_column_int64(stmt, 0);
            const char *password_hash = (const char *)sqlite3_column_text(stmt, 1);
            bool admin = (sqlite3_column_int(stmt, 2) == 1);
            const char *passport = (const char *)sqlite3_column_text(stmt, 3);

            if (crypto_pwhash_str_verify(password_hash, password, strlen(password)) == 0) {
                int64_t time = GetUnixTime();

                if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username)
                                         VALUES (?1, ?2, ?3, ?4))",
                                      time, request.client_addr, "login", username))
                    return;

                RetainPtr<Session> session = CreateUserSession(request, io, userid, username, passport);

                if (RG_LIKELY(session)) {
                    session->admin = admin;

                    const Token *token = session->GetToken(instance);

                    http_JsonPageBuilder json(request.compression_type);
                    WriteProfileJson(session.GetRaw(), token, &json);
                    json.Finish(io);
                }

                return;
            }
        }

        if (stmt.IsValid()) {
            // Enforce constant delay if authentification fails
            int64_t safety_delay = std::max(2000 - GetMonotonicTime() + now, (int64_t)0);
            WaitDelay(safety_delay);

            LogError("Invalid username or password");
            io->AttachError(403);
        }
    });
}

void HandleUserLogout(InstanceHolder *, const http_RequestInfo &request, http_IO *io)
{
    sessions.Close(request, io);

    if (gp_domain.config.demo_user) {
        CreateDemoSession(request, io);
    }

    io->AttachText(200, "");
}

void HandleUserProfile(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);
    const Token *token = session ? session->GetToken(instance) : nullptr;

    http_JsonPageBuilder json(request.compression_type);
    WriteProfileJson(session.GetRaw(), token, &json);
    json.Finish(io);
}

}
