// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "config.hh"
#include "goupile.hh"
#include "misc.hh"
#include "user.hh"
#include "../../core/libwrap/sqlite.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static http_SessionManager sessions;

static void WriteProfileJson(const Session *session, json_Writer *out_json)
{
    out_json->StartObject();

    if (session) {
        out_json->Key("username"); out_json->String(session->username);
        if (session->zone) {
            out_json->Key("zone"); out_json->String(session->zone);
        } else {
            out_json->Key("zone"); out_json->Null();
        }
        out_json->Key("permissions"); out_json->StartObject();
        for (Size i = 0; i < RG_LEN(UserPermissionNames); i++) {
            char js_name[64];
            ConvertToJsName(UserPermissionNames[i], js_name);

            out_json->Key(js_name); out_json->Bool(session->permissions & (1 << i));
        }
        out_json->EndObject();
        out_json->Key("demo"); out_json->Bool(session->demo);
    }

    out_json->EndObject();
}

static RetainPtr<Session> CreateUserSession(const http_RequestInfo &request, http_IO *io,
                                            const char *username, sq_Statement *stmt)
{
    uint32_t permissions = (uint32_t)sqlite3_column_int64(*stmt, 0);
    const char *zone = (const char *)sqlite3_column_text(*stmt, 1);

    Size len = RG_SIZE(Session) + strlen(username) + (zone ? strlen(zone) : 0) + 2;
    Session *session = (Session *)Allocator::Allocate(nullptr, len, (int)Allocator::Flag::Zero);

    session->username = (char *)session + RG_SIZE(Session);
    strcpy((char *)session->username, username);
    if (zone) {
        session->zone = session->username + strlen(username) + 1;
        strcpy((char *)session->zone, zone);
    }
    session->permissions = (uint32_t)permissions;

    RetainPtr<Session> ptr(session, [](Session *session) { Allocator::Release(nullptr, session, -1); });
    sessions.Open(request, io, ptr);

    return ptr;
}

static RetainPtr<Session> CreateDemoSession(const http_RequestInfo &request, http_IO *io)
{
    sq_Statement stmt;
    if (!instance->db.Prepare(R"(SELECT permissions, zone FROM usr_users
                                 WHERE username = ?)", &stmt))
        return {};
    sqlite3_bind_text(stmt, 1, instance->config.demo_user, -1, SQLITE_STATIC);
    if (!stmt.Next()) {
        LogError("Demo user '%1' does not exist", instance->config.demo_user);
        return {};
    }

    // Make sure previous there is no session related Set-Cookie header left
    io->ResetResponse();

    RetainPtr<Session>session = CreateUserSession(request, io, instance->config.demo_user, &stmt);
    session->demo = true;

    return session;
}

RetainPtr<const Session> GetCheckedSession(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<Session> session = sessions.Find<Session>(request, io);

    if (instance->config.demo_user && !session) {
        session = CreateDemoSession(request, io);
    }

    return session;
}

void HandleUserLogin(const http_RequestInfo &request, http_IO *io)
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
        if (!instance->db.Prepare(R"(SELECT permissions, zone, password_hash FROM usr_users
                                     WHERE username = ?)", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

        if (stmt.Next()) {
            const char *password_hash = (const char *)sqlite3_column_text(stmt, 2);

            if (crypto_pwhash_str_verify(password_hash, password, strlen(password)) == 0) {
                int64_t time = GetUnixTime();
                const char *zone = (const char *)sqlite3_column_text(stmt, 1);

                if (!instance->db.Run(R"(INSERT INTO adm_events (time, address, type, username, zone)
                                         VALUES (?, ?, 'login', ?, ?);)",
                                    time, request.client_addr, username,
                                    zone ? sq_Binding(zone) : sq_Binding()))
                    return;

                RetainPtr<const Session> session = CreateUserSession(request, io, username, &stmt);

                http_JsonPageBuilder json(request.compression_type);
                WriteProfileJson(session.GetRaw(), &json);
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
            LogError("SQLite Error: %1", sqlite3_errmsg(instance->db));
        }
    });
}

void HandleUserLogout(const http_RequestInfo &request, http_IO *io)
{
    sessions.Close(request, io);

    if (instance->config.demo_user) {
        CreateDemoSession(request, io);
    }

    io->AttachText(200, "");
}

void HandleUserProfile(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);

    http_JsonPageBuilder json(request.compression_type);
    WriteProfileJson(session.GetRaw(), &json);
    json.Finish(io);
}

}
