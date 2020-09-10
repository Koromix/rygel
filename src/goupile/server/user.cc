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

RetainPtr<const Session> GetCheckedSession(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = sessions.Find<const Session>(request, io);
    return session;
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
        if (!goupile_db.Prepare(R"(SELECT u.username, u.password_hash, u.permissions
                                   FROM usr_users u
                                   WHERE u.username = ?)", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

        if (stmt.Next()) {
            const char *password_hash = (const char *)sqlite3_column_text(stmt, 1);

            if (crypto_pwhash_str_verify(password_hash, password, strlen(password)) == 0) {
                Session *session = (Session *)Allocator::Allocate(nullptr, RG_SIZE(Session) + strlen(username) + 1,
                                                                  (int)Allocator::Flag::Zero);

                session->permissions = (uint32_t)sqlite3_column_int64(stmt, 2);
                strcpy(session->username, username);

                RetainPtr<Session> ptr(session, [](Session *session) { Allocator::Release(nullptr, session, -1); });
                sessions.Open(request, io, ptr);

                http_JsonPageBuilder json(request.compression_type);
                WriteProfileJson(session, &json);
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
            LogError("SQLite Error: %1", sqlite3_errmsg(goupile_db));
        }
    });
}

void HandleUserLogout(const http_RequestInfo &request, http_IO *io)
{
    sessions.Close(request, io);
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
