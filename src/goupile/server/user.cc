// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "goupile.hh"
#include "user.hh"
#include "../../core/libwrap/sqlite.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static http_SessionManager sessions;

std::shared_ptr<const Session> GetCheckedSession(const http_RequestInfo &request, http_IO *io)
{
    std::shared_ptr<const Session> session = sessions.Find<const Session>(request, io);

    if (!session) {
        LogError("User is not connected");
        io->AttachError(403);
    }

    return session;
}

void HandleLogin(const http_RequestInfo &request, http_IO *io)
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

        SQLiteStatement stmt;
        if (!goupile_db.Prepare(R"(SELECT u.password_hash, u.admin,
                                          p.read, p.query, p.new, p.remove, p.edit, p.validate
                                   FROM users u
                                   INNER JOIN permissions p ON (p.username = u.username)
                                   WHERE u.username = ?)", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

        // We use this to extend/fix the response delay in case of error
        int64_t now = GetMonotonicTime();

        if (stmt.Next()) {
            const char *password_hash = (const char *)sqlite3_column_text(stmt, 0);

            if (crypto_pwhash_str_verify(password_hash, password, strlen(password)) == 0) {
                Session *session = (Session *)Allocator::Allocate(nullptr, RG_SIZE(Session) + strlen(username) + 1);

                session->permissions |= !!sqlite3_column_int(stmt, 1) * (int)UserPermission::Admin;
                session->permissions |= !!sqlite3_column_int(stmt, 2) * (int)UserPermission::Read;
                session->permissions |= !!sqlite3_column_int(stmt, 3) * (int)UserPermission::Query;
                session->permissions |= !!sqlite3_column_int(stmt, 4) * (int)UserPermission::New;
                session->permissions |= !!sqlite3_column_int(stmt, 5) * (int)UserPermission::Remove;
                session->permissions |= !!sqlite3_column_int(stmt, 6) * (int)UserPermission::Edit;
                session->permissions |= !!sqlite3_column_int(stmt, 7) * (int)UserPermission::Validate;
                strcpy(session->username, username);

                // XXX: Ugly allocation for refcount, we need to make our own intrusive shared pointer
                std::shared_ptr<Session> ptr =
                    std::shared_ptr<Session>(session, [](Session *session) { Allocator::Release(nullptr, session, -1); });
                sessions.Open(request, io, ptr);

                // Success!
                io->AttachText(200, "{}", "application/json");
                return;
            }
        } else if (!stmt.IsValid()) {
            LogError("SQLite Error: %1", sqlite3_errmsg(goupile_db));
            return;
        }

        // Enforce constant delay if authentification fails
        int64_t safety_delay = std::max(2000 - GetMonotonicTime() + now, (int64_t)0);
        WaitForDelay(safety_delay);

        LogError("Incorrect username or password");
        io->AttachError(403);
    });
}

void HandleLogout(const http_RequestInfo &request, http_IO *io)
{
    sessions.Close(request, io);
    io->AttachText(200, "{}", "application/json");
}

}
