// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "../../core/libcc/libcc.hh"
#include "domain.hh"
#include "goupile.hh"
#include "instance.hh"
#include "user.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static http_SessionManager<Session> sessions;

const InstanceToken *Session::GetToken(const InstanceHolder *instance) const
{
    if (instance) {
        InstanceToken *token;
        {
            std::shared_lock<std::shared_mutex> lock_shr(tokens_lock);
            token = tokens_map.Find(instance->unique);
        }

        if (!token) {
            std::lock_guard<std::shared_mutex> lock_excl(tokens_lock);

            // Actually get the token we want
            do {
                sq_Statement stmt;
                if (!gp_domain.db.Prepare(R"(SELECT permissions FROM dom_permissions
                                             WHERE userid = ?1 AND instance = ?2)", &stmt))
                    break;
                sqlite3_bind_int64(stmt, 1, userid);
                sqlite3_bind_text(stmt, 2, instance->key.ptr, (int)instance->key.len, SQLITE_STATIC);
                if (!stmt.Next())
                    break;

                uint32_t permissions = sqlite3_column_int(stmt, 0);

                token = tokens_map.SetDefault(instance->unique);
                token->permissions = permissions;
                token->title = DuplicateString(instance->config.title, &tokens_alloc).ptr;
                token->url = Fmt(&tokens_alloc, "/%1/", instance->key).ptr;
            } while (false);

            // Redirect user to appropiate slave instance
            if (instance->GetSlaveCount()) {
                do {
                    sq_Statement stmt;
                    if (!gp_domain.db.Prepare(R"(SELECT i.instance FROM dom_instances i
                                                 INNER JOIN dom_permissions p ON (p.instance = i.instance)
                                                 WHERE p.userid = ?1 AND i.master = ?2
                                                 ORDER BY i.instance)", &stmt))
                        break;
                    sqlite3_bind_int64(stmt, 1, userid);
                    sqlite3_bind_text(stmt, 2, instance->key.ptr, (int)instance->key.len, SQLITE_STATIC);
                    if (!stmt.Next())
                        break;

                    const char *redirect_key = (const char *)sqlite3_column_text(stmt, 0);

                    InstanceHolder *redirect = gp_domain.Ref(redirect_key);
                    if (!redirect)
                        break;
                    RG_DEFER { redirect->Unref(); };

                    if (!token) {
                        token = tokens_map.SetDefault(instance->unique);
                        token->permissions = 0;
                    }

                    token->title = DuplicateString(redirect->config.title, &tokens_alloc).ptr;
                    token->url = Fmt(&tokens_alloc, "/%1/", redirect->key).ptr;
                } while (false);
            }

            // User is not assigned to this project, cache this information
            if (!token) {
                token = tokens_map.SetDefault(instance->unique);
            }
        }

        return token->title ? token : nullptr;
    } else {
        return nullptr;
    }
}

static void WriteProfileJson(const Session *session, const InstanceHolder *instance,
                             const http_RequestInfo &request, http_IO *io)
{
    http_JsonPageBuilder json(request.compression_type);
    char buf[128];

    json.StartObject();
    if (session) {
        json.Key("userid"); json.Int64(session->userid);
        json.Key("username"); json.String(session->username);

        if (instance) {
            const InstanceToken *token = session->GetToken(instance);

            if (token) {
                json.Key("authorized"); json.Bool(true);

                json.Key("keys"); json.StartArray();
                    json.StartArray();
                        json.Int64(session->userid);
                        json.String(session->local_key);
                    json.EndArray();
                    json.StartArray();
                        json.String("shared");
                        json.String(instance->config.shared_key);
                    json.EndArray();
                json.EndArray();

                json.Key("instance"); json.StartObject();
                    json.Key("title"); json.String(token->title);
                    json.Key("url"); json.String(token->url);
                json.EndObject();

                json.Key("permissions"); json.StartObject();
                for (Size i = 0; i < RG_LEN(UserPermissionNames); i++) {
                    Span<const char> key = ConvertToJsonName(UserPermissionNames[i], buf);
                    json.Key(key.ptr, (size_t)key.len); json.Bool(token->permissions & (1 << i));
                }
                json.EndObject();
            } else {
                json.Key("authorized"); json.Bool(false);
            }
        } else {
            json.Key("authorized"); json.Bool(session->IsAdmin());
        }
    }
    json.EndObject();

    json.Finish(io);
}

static RetainPtr<Session> CreateUserSession(int64_t userid, const char *username, const char *local_key)
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
    if (!CopyString(local_key, session->local_key)) {
        // Should never happen, but let's be careful
        LogError("User local key is too big");
        return {};
    }

    return ptr;
}

RetainPtr<const Session> GetCheckedSession(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<Session> session = sessions.Find(request, io);

    if (gp_domain.config.demo_user && !session) {
        static RetainPtr<Session> demo_session = [&]() {
            sq_Statement stmt;
            if (!gp_domain.db.Prepare("SELECT userid, local_key FROM dom_users WHERE username = ?1", &stmt))
                return RetainPtr<Session>(nullptr);
            sqlite3_bind_text(stmt, 1, gp_domain.config.demo_user, -1, SQLITE_STATIC);

            if (!stmt.Next()) {
                if (stmt.IsValid()) {
                    LogError("Demo user '%1' does not exist", gp_domain.config.demo_user);
                }
                return RetainPtr<Session>(nullptr);
            }

            int64_t userid = sqlite3_column_int64(stmt, 0);
            const char *local_key = (const char *)sqlite3_column_text(stmt, 1);
            RetainPtr<Session> session = CreateUserSession(userid, gp_domain.config.demo_user, local_key);

            return session;
        }();

        session = demo_session;
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
            if (!gp_domain.db.Prepare(R"(SELECT u.userid, u.password_hash, u.admin, u.local_key FROM dom_users u
                                         INNER JOIN dom_permissions p ON (p.userid = u.userid)
                                         INNER JOIN dom_instances i ON (i.instance = p.instance)
                                         WHERE u.username = ?1 AND
                                               (i.instance = ?2 OR i.master = ?2) AND
                                               p.permissions > 0)", &stmt))
                return;
            sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, instance->key.ptr, (int)instance->key.len, SQLITE_STATIC);
        } else {
            if (!gp_domain.db.Prepare(R"(SELECT userid, password_hash, admin, local_key FROM dom_users
                                         WHERE username = ?1 AND admin = 1)", &stmt))
                return;
            sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        }

        if (stmt.Next()) {
            int64_t userid = sqlite3_column_int64(stmt, 0);
            const char *password_hash = (const char *)sqlite3_column_text(stmt, 1);
            bool admin = (sqlite3_column_int(stmt, 2) == 1);
            const char *local_key = (const char *)sqlite3_column_text(stmt, 3);

            if (crypto_pwhash_str_verify(password_hash, password, strlen(password)) == 0) {
                int64_t time = GetUnixTime();

                if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username)
                                         VALUES (?1, ?2, ?3, ?4))",
                                      time, request.client_addr, "login", username))
                    return;

                RetainPtr<Session> session = CreateUserSession(userid, username, local_key);

                if (RG_LIKELY(session)) {
                    if (admin && !instance) {
                        // Require regular relogin (every 20 minutes) to access admin panel
                        session->admin_until = GetMonotonicTime() + 1200 * 1000;
                    }

                    sessions.Open(request, io, session);
                    WriteProfileJson(session.GetRaw(), instance, request, io);
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
    io->AttachText(200, "Done!");
}

void HandleUserProfile(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);
    WriteProfileJson(session.GetRaw(), instance, request, io);
}

}
