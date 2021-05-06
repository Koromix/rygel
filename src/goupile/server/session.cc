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
#include "messages.hh"
#include "session.hh"
#include "../../core/libnet/libnet.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static const int BanTreshold = 5;
static const int64_t BanTime = 600 * 1000;

struct BanInfo {
    int64_t userid;
    int64_t until_time;

    int tries;
    bool active;

    RG_HASHTABLE_HANDLER(BanInfo, userid);
};

static http_SessionManager<SessionInfo> sessions;

static std::shared_mutex bans_mutex;
static BucketArray<BanInfo> bans;
static HashTable<int64_t, BanInfo *> bans_map;

bool SessionInfo::IsAdmin() const
{
    if (confirm[0])
        return false;
    if (!admin_until || admin_until <= GetMonotonicTime())
        return false;

    return true;
}

bool SessionInfo::HasPermission(const InstanceHolder *instance, UserPermission perm) const
{
    const SessionStamp *stamp = GetStamp(instance);
    return stamp && stamp->HasPermission(perm);
}

const SessionStamp *SessionInfo::GetStamp(const InstanceHolder *instance) const
{
    if (confirm[0])
        return nullptr;

    SessionStamp *stamp = nullptr;

    // Fast path
    {
        std::shared_lock<std::shared_mutex> lock_shr(stamps_mutex);
        stamp = stamps_map.Find(instance->unique);
    }

    if (!stamp) {
        std::lock_guard<std::shared_mutex> lock_excl(stamps_mutex);
        stamp = stamps_map.Find(instance->unique);

        if (!stamp) {
            stamp = stamps_map.SetDefault(instance->unique);
            stamp->unique = instance->unique;

            sq_Statement stmt;
            if (!gp_domain.db.Prepare(R"(SELECT permissions FROM dom_permissions
                                         WHERE userid = ?1 AND instance = ?2)", &stmt))
                return nullptr;
            sqlite3_bind_int64(stmt, 1, userid);
            sqlite3_bind_text(stmt, 2, instance->key.ptr, (int)instance->key.len, SQLITE_STATIC);
            if (!stmt.Next())
                return nullptr;

            uint32_t permissions = (uint32_t)sqlite3_column_int(stmt, 0);

            if (instance->master != instance) {
                InstanceHolder *master = instance->master;

                sq_Statement stmt;
                if (!gp_domain.db.Prepare(R"(SELECT permissions FROM dom_permissions
                                             WHERE userid = ?1 AND instance = ?2)", &stmt))
                    return nullptr;
                sqlite3_bind_int64(stmt, 1, userid);
                sqlite3_bind_text(stmt, 2, master->key.ptr, (int)master->key.len, SQLITE_STATIC);

                permissions &= UserPermissionSlaveMask;
                if (stmt.Next()) {
                    uint32_t master_permissions = (uint32_t)sqlite3_column_int(stmt, 0);
                    permissions |= master_permissions & UserPermissionMasterMask;
                }
            } else if (instance->slaves.len) {
                permissions &= UserPermissionMasterMask;
            }

            stamp->authorized = true;
            stamp->permissions = permissions;
        }
    }

    return stamp->authorized ? stamp : nullptr;
}

void SessionInfo::InvalidateStamps()
{
    std::lock_guard<std::shared_mutex> lock_excl(stamps_mutex);
    stamps_map.Clear();
}

void InvalidateUserStamps(int64_t userid)
{
    sessions.ApplyAll([&](SessionInfo *session) {
        if (session->userid == userid) {
            session->InvalidateStamps();
        }
    });
}

static void WriteProfileJson(const SessionInfo *session, const InstanceHolder *instance,
                             const http_RequestInfo &request, http_IO *io)
{
    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;
    char buf[512];

    json.StartObject();
    if (session) {
        json.Key("userid"); json.Int64(session->userid);
        json.Key("username"); json.String(session->username);

        if (session->confirm[0]) {
            json.Key("authorized"); json.Bool(false);
            json.Key("confirm"); json.String("SMS");
        } else if (instance) {
            const InstanceHolder *master = instance->master;
            const SessionStamp *stamp = nullptr;

            if (instance->slaves.len) {
                for (const InstanceHolder *slave: instance->slaves) {
                    stamp = session->GetStamp(slave);

                    if (stamp) {
                        instance = slave;
                        break;
                    }
                }
            } else {
                stamp = session->GetStamp(instance);
            }

            if (stamp) {
                json.Key("authorized"); json.Bool(true);

                json.Key("namespaces"); json.StartObject();
                    if (instance->config.shared_key) {
                        json.Key("records"); json.String("shared");
                    } else {
                        json.Key("records"); json.Int64(session->userid);
                    }
                json.EndObject();
                json.Key("keys"); json.StartObject();
                    if (instance->config.shared_key) {
                        json.Key("records"); json.String(instance->config.shared_key);
                    } else {
                        json.Key("records"); json.String(session->local_key);
                    }
                json.EndObject();

                if (master->slaves.len) {
                    json.Key("instances"); json.StartArray();
                    for (const InstanceHolder *slave: master->slaves) {
                        if (session->GetStamp(slave)) {
                            json.StartObject();
                            json.Key("title"); json.String(slave->title);
                            json.Key("name"); json.String(slave->config.name);
                            json.Key("url"); json.String(Fmt(buf, "/%1/", slave->key).ptr);
                            json.EndObject();
                        }
                    }
                    json.EndArray();
                }

                json.Key("permissions"); json.StartObject();
                for (Size i = 0; i < RG_LEN(UserPermissionNames); i++) {
                    Span<const char> key = ConvertToJsonName(UserPermissionNames[i], buf);
                    json.Key(key.ptr, (size_t)key.len); json.Bool(stamp->permissions & (1 << i));
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

    json.Finish();
}

static RetainPtr<SessionInfo> CreateUserSession(int64_t userid, const char *username, const char *local_key)
{
    Size username_len = strlen(username);
    Size len = RG_SIZE(SessionInfo) + username_len + 1;
    SessionInfo *session = (SessionInfo *)Allocator::Allocate(nullptr, len, (int)Allocator::Flag::Zero);

    new (session) SessionInfo;
    RetainPtr<SessionInfo> ptr(session, [](SessionInfo *session) {
        session->~SessionInfo();
        Allocator::Release(nullptr, session, -1);
    });

    session->userid = userid;
    session->username = (char *)session + RG_SIZE(SessionInfo);
    CopyString(username, MakeSpan((char *)session->username, username_len + 1));
    if (!CopyString(local_key, session->local_key)) {
        // Should never happen, but let's be careful
        LogError("User local key is too big");
        return {};
    }

    return ptr;
}

RetainPtr<const SessionInfo> GetCheckedSession(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<SessionInfo> session = sessions.Find(request, io);

    if (!session && instance) {
        int64_t auto_userid = instance->master->config.auto_userid;

        if (auto_userid > 0) {
            if (RG_UNLIKELY(!instance->auto_init)) {
                instance->auto_session = [&]() {
                    sq_Statement stmt;
                    if (!gp_domain.db.Prepare("SELECT userid, username, local_key FROM dom_users WHERE userid = ?1", &stmt))
                        return RetainPtr<SessionInfo>(nullptr);
                    sqlite3_bind_int64(stmt, 1, auto_userid);

                    if (!stmt.Next()) {
                        if (stmt.IsValid()) {
                            LogError("Automatic user ID %1 does not exist", auto_userid);
                        }
                        return RetainPtr<SessionInfo>(nullptr);
                    }

                    int64_t userid = sqlite3_column_int64(stmt, 0);
                    const char *username = (const char *)sqlite3_column_text(stmt, 1);
                    const char *local_key = (const char *)sqlite3_column_text(stmt, 2);

                    RetainPtr<SessionInfo> session = CreateUserSession(userid, username, local_key);
                    return session;
                }();
                instance->auto_init = true;
            }

            session = instance->auto_session;
        }
    }

    return session;
}

void PruneSessions()
{
    // Prune sessions
    sessions.Prune();

    // Prune bans
    {
        std::lock_guard<std::shared_mutex> lock_excl(bans_mutex);

        int64_t now = GetMonotonicTime();

        Size expired = 0;
        for (const BanInfo &ban: bans) {
            if (ban.until_time > now)
                break;

            BanInfo **ptr = bans_map.Find(ban.userid);
            if (*ptr == &ban) {
                bans_map.Remove(ptr);
            }
            expired++;
        }
        bans.RemoveFirst(expired);
        bans_map.Trim();
    }
}

static bool IsUserBanned(int64_t userid)
{
    std::shared_lock<std::shared_mutex> lock_shr(bans_mutex);

    // We don't need to use precise timing, and a ban can last a bit
    // more than BanTime (until_time pruning clears the ban).
    const BanInfo *ban = bans_map.FindValue(userid, nullptr);
    return ban && ban->active;
}

static void RegisterBanEvent(int64_t userid)
{
    std::lock_guard<std::shared_mutex> lock_excl(bans_mutex);

    BanInfo *ban = bans_map.FindValue(userid, nullptr);
    if (!ban || ban->until_time < GetMonotonicTime()) {
        ban = bans.AppendDefault();
        ban->userid = userid;
        ban->until_time = GetMonotonicTime() + BanTime;
        bans_map.Set(ban);
    }

    if (ban->tries++ >= BanTreshold) {
        ban->active = true;
    }
}

void HandleSessionLogin(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
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

            if (IsUserBanned(userid)) {
                LogError("This user account has been temporarily banned (excessive logins)");
                io->AttachError(403);
                return;
            }

            if (crypto_pwhash_str_verify(password_hash, password, strlen(password)) == 0) {
                int64_t time = GetUnixTime();

                if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username)
                                         VALUES (?1, ?2, ?3, ?4))",
                                      time, request.client_addr, "login", username))
                    return;

                RetainPtr<SessionInfo> session = CreateUserSession(userid, username, local_key);

                if (RG_LIKELY(session)) {
                    // This is not a mistake; we don't want to lock people out of accounts for
                    // password failures because this provides an easy anti-user-DDoS system.
                    // Hoewever, we want to block shared user accounts ;)
                    RegisterBanEvent(userid);

                    if (admin) {
                        if (!instance) {
                            // Require regular relogin (every 20 minutes) to access admin panel
                            session->admin_until = GetMonotonicTime() + 1200 * 1000;
                        } else {
                            // Mark session as elevatable (can become admin) so the user gets
                            // identity confirmation prompts when he tries to make admin requests.
                            session->admin_until = -1;
                        }
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

void HandleSessionToken(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    if (!instance->config.enable_tokens) {
        LogError("This instance does not use tokens");
        io->AttachError(403);
        return;
    }
    if (gp_domain.config.sms.provider == sms_Provider::None) {
        LogError("This instance is not configured to send SMS messages");
        io->AttachError(503);
        return;
    }

    io->RunAsync([=]() {
        // Read POST values
        Span<const char> token;
        {
            HashMap<const char *, const char *> values;
            if (!io->ReadPostValues(&io->allocator, &values)) {
                io->AttachError(422);
                return;
            }

            token = values.FindValue("token", nullptr);
            if (!token.ptr) {
                LogError("Missing 'token' parameter");
                io->AttachError(422);
                return;
            }
        }

        // Decode Base64
        Span<uint8_t> cypher;
        {
            cypher.len = token.len / 2 + 1;
            cypher.ptr = (uint8_t *)Allocator::Allocate(&io->allocator, token.len);

            size_t cypher_len;
            if (sodium_hex2bin(cypher.ptr, cypher.len, token.ptr, (size_t)token.len,
                               nullptr, &cypher_len, nullptr) != 0) {
                LogError("Failed to unseal token");
                io->AttachError(403);
                return;
            }
            if (cypher_len < crypto_box_SEALBYTES) {
                LogError("Failed to unseal token");
                io->AttachError(403);
                return;
            }

            cypher.len = (Size)cypher_len;
        }

        // Decode token
        Span<uint8_t> json;
        {
            json.len = cypher.len - crypto_box_SEALBYTES;
            json.ptr = (uint8_t *)Allocator::Allocate(&io->allocator, json.len);

            if (crypto_box_seal_open((uint8_t *)json.ptr, cypher.ptr, cypher.len,
                                     instance->config.token_pkey, instance->config.token_skey) != 0) {
                LogError("Failed to unseal token");
                io->AttachError(403);
                return;
            }
        }

        // Parse JSON
        const char *sms = nullptr;
        int64_t userid = 0;
        const char *username = nullptr;
        {
            StreamReader st(json);
            json_Parser parser(&st, &io->allocator);

            parser.ParseObject();
            while (parser.InObject()) {
                const char *key = "";
                parser.ParseKey(&key);

                if (TestStr(key, "sms")) {
                    parser.ParseString(&sms);
                } else if (TestStr(key, "userid")) {
                    parser.ParseInt(&userid);
                } else if (TestStr(key, "username")) {
                    parser.ParseString(&username);
                } else if (parser.IsValid()) {
                    LogError("Unknown key '%1' in token JSON", key);
                    io->AttachError(422);
                    return;
                }
            }
            if (!parser.IsValid()) {
                io->AttachError(422);
                return;
            }
        }

        // Check token values
        {
            bool valid = true;

            if (!sms || !sms[0]) {
                LogError("Missing or empty SMS");
                valid = false;
            }
            if (!userid) {
                LogError("Missing or empty username");
                valid = false;
            } else if (userid > 0) {
                LogError("Only negative user ID values are allowed in tokens");
                valid = false;
            }
            if (!username || !username[0]) {
                LogError("Missing or empty username");
                valid = false;
            }

            if (!valid) {
                io->AttachError(422);
                return;
            }
        }

        if (IsUserBanned(userid)) {
            LogError("This user account has been temporarily banned (excessive logins)");
            io->AttachError(403);
            return;
        }

        // Build local key from SHA-256 hash of JSON
        char local_key[45];
        {
            RG_STATIC_ASSERT(crypto_hash_sha256_BYTES == 32);

            uint8_t sha256[32];
            crypto_hash_sha256(sha256, json.ptr, json.len);
            sodium_bin2base64(local_key, RG_SIZE(local_key), sha256, RG_SIZE(sha256), sodium_base64_VARIANT_ORIGINAL);
        }

        RetainPtr<SessionInfo> session = CreateUserSession(userid, username, local_key);

        if (RG_LIKELY(session)) {
            RegisterBanEvent(session->userid);

            uint32_t code = 100000 + randombytes_uniform(900000); // 6 digits
            Fmt(session->confirm, "%1", code);

            // Send confirmation SMS
            if (!SendSMS(sms, session->confirm))
                return;

            sessions.Open(request, io, session);
            WriteProfileJson(session.GetRaw(), nullptr, request, io);
        }
    });
}

void HandleSessionConfirm(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<SessionInfo> session = sessions.Find(request, io);

    if (!session || !session->confirm[0]) {
        LogError("There is nothing to confirm");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Read POST values
        Span<const char> code;
        {
            HashMap<const char *, const char *> values;
            if (!io->ReadPostValues(&io->allocator, &values)) {
                io->AttachError(422);
                return;
            }

            code = values.FindValue("code", nullptr);
            if (!code.ptr) {
                LogError("Missing 'code' parameter");
                io->AttachError(422);
                return;
            }
        }

        if (IsUserBanned(session->userid)) {
            LogError("This user account has been temporarily banned (excessive logins)");
            io->AttachError(403);
            return;
        }

        // Immediate confirmation looks weird
        WaitDelay(800);

        if (TestStr(code, session->confirm)) {
            session->confirm[0] = 0;
            WriteProfileJson(session.GetRaw(), instance, request, io);
        } else {
            RegisterBanEvent(session->userid);

            LogError("Code is incorrect");
            io->AttachError(403);
        }
    });
}

void HandleSessionLogout(const http_RequestInfo &request, http_IO *io)
{
    sessions.Close(request, io);
    io->AttachText(200, "Done!");
}

void HandleSessionProfile(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetCheckedSession(instance, request, io);
    WriteProfileJson(session.GetRaw(), instance, request, io);
}

}
