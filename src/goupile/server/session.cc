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
#include "../../core/libsecurity/libsecurity.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static const int BanTreshold = 6;
static const int64_t BanTime = 1800 * 1000;

struct FloodInfo {
    struct Key {
        const char *address;
        const char *context;

        bool operator==(const Key &other) const { return TestStr(address, other.address) && TestStr(context, other.context); }
        bool operator!=(const Key &other) const { return !(*this == other); }

        uint64_t Hash() const
        {
            uint64_t hash = HashTraits<const char *>::Hash(address) ^
                            HashTraits<const char *>::Hash(context);
            return hash;
        }
    };

    Key key;
    int64_t until_time;

    int events;
    bool banned;

    RG_HASHTABLE_HANDLER(FloodInfo, key);
};

static http_SessionManager<SessionInfo> sessions;

static std::shared_mutex floods_mutex;
static BucketArray<FloodInfo> floods;
static HashTable<FloodInfo::Key, FloodInfo *> floods_map;

bool SessionInfo::IsAdmin() const
{
    if (confirm != SessionConfirm::None)
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
    if (confirm != SessionConfirm::None)
        return nullptr;

    SessionStamp *stamp = nullptr;

    // Fast path
    {
        std::shared_lock<std::shared_mutex> lock_shr(mutex);
        stamp = stamps_map.FindValue(instance->unique, nullptr);
    }

    if (!stamp) {
        std::lock_guard<std::shared_mutex> lock_excl(mutex);
        stamp = stamps_map.FindValue(instance->unique, nullptr);

        if (!stamp) {
            stamp = stamps.AppendDefault();
            stamp->unique = instance->unique;

            stamps_map.Set(stamp);

            if (userid > 0) {
                sq_Statement stmt;
                if (!gp_domain.db.Prepare(R"(SELECT permissions FROM dom_permissions
                                             WHERE userid = ?1 AND instance = ?2)", &stmt))
                    return nullptr;
                sqlite3_bind_int64(stmt, 1, userid);
                sqlite3_bind_text(stmt, 2, instance->key.ptr, (int)instance->key.len, SQLITE_STATIC);
                if (!stmt.Step())
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
                    if (stmt.Step()) {
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
    }

    return stamp->authorized ? stamp : nullptr;
}

void SessionInfo::InvalidateStamps()
{
    std::lock_guard<std::shared_mutex> lock_excl(mutex);
    stamps_map.Clear();

    // We can't clear the array or the allocator because the stamps may
    // be in use so they will waste memory until the session ends.
}

void SessionInfo::AuthorizeInstance(const InstanceHolder *instance, uint32_t permissions)
{
    std::lock_guard<std::shared_mutex> lock_excl(mutex);

    SessionStamp *stamp = stamps.AppendDefault();

    stamp->unique = instance->unique;
    stamp->authorized = true;
    stamp->permissions = permissions;

    stamps_map.Set(stamp);
}

void InvalidateUserStamps(int64_t userid)
{
    // Deal with real sessions
    sessions.ApplyAll([&](SessionInfo *session) {
        if (session->userid == userid) {
            session->InvalidateStamps();
        }
    });

    // Deal with automatic sessions
    {
        Span<InstanceHolder *> instances = gp_domain.LockInstances();
        RG_DEFER { gp_domain.UnlockInstances(); };

        for (const InstanceHolder *instance: instances) {
            if (instance->default_init && instance->default_session->userid == userid) {
                instance->default_session->InvalidateStamps();
            }
        }
    }
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
        json.Key("online"); json.Bool(true);

        // Atomic load
        SessionConfirm confirm = session->confirm;

        if (confirm != SessionConfirm::None) {
            json.Key("authorized"); json.Bool(false);

            switch (confirm) {
                case SessionConfirm::None: { RG_UNREACHABLE(); } break;
                case SessionConfirm::SMS: { json.Key("confirm"); json.String("sms"); } break;
                case SessionConfirm::TOTP: { json.Key("confirm"); json.String("totp"); } break;
                case SessionConfirm::QRcode: { json.Key("confirm"); json.String("qrcode"); } break;
            }
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

                switch (session->type) {
                    case SessionType::Login: { json.Key("type"); json.String("login"); } break;
                    case SessionType::Token: { json.Key("type"); json.String("token"); } break;
                    case SessionType::Key: {
                        json.Key("type"); json.String("key");

                        RG_ASSERT(instance->config.auto_key);

                        json.Key("restore"); json.StartObject();
                            json.Key(instance->config.auto_key); json.String(session->username);
                        json.EndObject();
                    } break;
                    case SessionType::Auto: { json.Key("type"); json.String("auto"); } break;
                }

                json.Key("namespaces"); json.StartObject();
                if (instance->config.shared_key) {
                    json.Key("records"); json.String("global");
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
                if (instance->config.shared_key) {
                    json.Key("encrypt_usb"); json.Bool(true);
                }

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

                json.Key("admin"); json.Bool(session->admin_until != 0);
            } else {
                json.Key("authorized"); json.Bool(false);
            }
        } else {
            json.Key("authorized"); json.Bool(session->IsAdmin());
            json.Key("admin"); json.Bool(session->admin_until != 0);
        }
    }
    json.EndObject();

    json.Finish();
}

static RetainPtr<SessionInfo> CreateUserSession(SessionType type, int64_t userid,
                                                const char *username, const char *local_key,
                                                SessionConfirm confirm, const char *secret)
{
    Size username_bytes = strlen(username) + 1;
    Size session_bytes = RG_SIZE(SessionInfo) + username_bytes;

    SessionInfo *session = (SessionInfo *)Allocator::Allocate(nullptr, session_bytes, (int)Allocator::Flag::Zero);

    new (session) SessionInfo;
    RetainPtr<SessionInfo> ptr(session, [](SessionInfo *session) {
        session->~SessionInfo();
        Allocator::Release(nullptr, session, -1);
    });

    session->type = type;
    session->userid = userid;
    session->username = (char *)session + RG_SIZE(SessionInfo);
    CopyString(username, MakeSpan((char *)session->username, username_bytes));
    if (!CopyString(local_key, session->local_key)) {
        // Should never happen, but let's be careful
        LogError("User local key is too big");
        return {};
    }

    session->confirm = confirm;
    if (secret) {
        RG_ASSERT(confirm != SessionConfirm::None);

        if (!CopyString(secret, session->secret)) {
            // Should never happen, but let's be careful
            LogError("Session secret is too big");
            return {};
        }
    }

    return ptr;
}

RetainPtr<const SessionInfo> GetCheckedSession(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<SessionInfo> session = sessions.Find(request, io);

    if (!session && instance) {
        int64_t default_userid = instance->master->config.default_userid;

        if (default_userid > 0) {
            if (RG_UNLIKELY(!instance->default_init)) {
                instance->default_session = [&]() {
                    sq_Statement stmt;
                    if (!gp_domain.db.Prepare("SELECT userid, username, local_key FROM dom_users WHERE userid = ?1", &stmt))
                        return RetainPtr<SessionInfo>(nullptr);
                    sqlite3_bind_int64(stmt, 1, default_userid);

                    if (!stmt.Step()) {
                        if (stmt.IsValid()) {
                            LogError("Automatic user ID %1 does not exist", default_userid);
                        }
                        return RetainPtr<SessionInfo>(nullptr);
                    }

                    int64_t userid = sqlite3_column_int64(stmt, 0);
                    const char *username = (const char *)sqlite3_column_text(stmt, 1);
                    const char *local_key = (const char *)sqlite3_column_text(stmt, 2);

                    RetainPtr<SessionInfo> session = CreateUserSession(SessionType::Auto, userid, username, local_key,
                                                                       SessionConfirm::None, nullptr);

                    return session;
                }();
                instance->default_init = true;
            }

            session = instance->default_session;
        }
    }

    return session;
}

void PruneSessions()
{
    // Prune sessions
    sessions.Prune();

    // Prune floods
    {
        std::lock_guard<std::shared_mutex> lock_excl(floods_mutex);

        int64_t now = GetMonotonicTime();

        Size expired = 0;
        for (const FloodInfo &flood: floods) {
            if (flood.until_time > now)
                break;

            FloodInfo **ptr = floods_map.Find(flood.key);
            if (*ptr == &flood) {
                floods_map.Remove(ptr);
            }
            expired++;
        }
        floods.RemoveFirst(expired);

        floods.Trim();
        floods_map.Trim();
    }
}

bool HashPassword(Span<const char> password, char out_hash[crypto_pwhash_STRBYTES])
{
    if (crypto_pwhash_str(out_hash, password.ptr, password.len,
                          crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        LogError("Failed to hash password");
        return false;
    }

    return true;
}

static bool IsUserBanned(const char *address, const char *context)
{
    std::shared_lock<std::shared_mutex> lock_shr(floods_mutex);

    FloodInfo::Key key = {address, context};
    const FloodInfo *flood = floods_map.FindValue(key, nullptr);

    // We don't need to use precise timing, and a ban can last a bit
    // more than BanTime (until pruning clears the ban).
    return flood && flood->banned;
}

static bool RegisterFloodEvent(const char *address, const char *context)
{
    std::lock_guard<std::shared_mutex> lock_excl(floods_mutex);

    FloodInfo::Key key = {address, context};
    FloodInfo *flood = floods_map.FindValue(key, nullptr);

    if (!flood || flood->until_time < GetMonotonicTime()) {
        Allocator *alloc;
        flood = floods.AppendDefault(&alloc);
        flood->key = {DuplicateString(address, alloc).ptr, DuplicateString(context, alloc).ptr};
        flood->until_time = GetMonotonicTime() + BanTime;
        floods_map.Set(flood);
    }

    if (++flood->events >= BanTreshold) {
        flood->banned = true;
    }

    return flood->banned;
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
            if (instance->slaves.len) {
                if (!gp_domain.db.Prepare(R"(SELECT u.userid, u.password_hash, u.admin,
                                                    u.local_key, u.totp_required, u.totp_secret
                                             FROM dom_users u
                                             INNER JOIN dom_permissions p ON (p.userid = u.userid)
                                             INNER JOIN dom_instances i ON (i.instance = p.instance)
                                             WHERE u.username = ?1 AND i.master = ?2 AND
                                                   p.permissions > 0)", &stmt))
                    return;
            } else {
                if (!gp_domain.db.Prepare(R"(SELECT u.userid, u.password_hash, u.admin,
                                                    u.local_key, u.totp_required, u.totp_secret
                                             FROM dom_users u
                                             INNER JOIN dom_permissions p ON (p.userid = u.userid)
                                             INNER JOIN dom_instances i ON (i.instance = p.instance)
                                             WHERE u.username = ?1 AND i.instance = ?2 AND
                                                   p.permissions > 0)", &stmt))
                    return;
            }
            sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, instance->key.ptr, (int)instance->key.len, SQLITE_STATIC);
        } else {
            if (!gp_domain.db.Prepare(R"(SELECT userid, password_hash, admin,
                                                local_key, totp_required, totp_secret
                                         FROM dom_users
                                         WHERE username = ?1 AND admin = 1)", &stmt))
                return;
            sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        }

        if (stmt.Step()) {
            int64_t userid = sqlite3_column_int64(stmt, 0);
            const char *password_hash = (const char *)sqlite3_column_text(stmt, 1);
            bool admin = (sqlite3_column_int(stmt, 2) == 1);
            const char *local_key = (const char *)sqlite3_column_text(stmt, 3);
            bool totp_required = (sqlite3_column_int(stmt, 4) == 1);
            const char *totp_secret = (const char *)sqlite3_column_text(stmt, 5);

            if (IsUserBanned(request.client_addr, username)) {
                LogError("You are blocked for %1 minutes after excessive login failures", (BanTime + 59000) / 60000);
                io->AttachError(403);
                return;
            }

            if (crypto_pwhash_str_verify(password_hash, password, strlen(password)) == 0) {
                int64_t time = GetUnixTime();

                if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username)
                                         VALUES (?1, ?2, ?3, ?4))",
                                      time, request.client_addr, "login", username))
                    return;

                RetainPtr<SessionInfo> session;
                if (totp_required) {
                    if (totp_secret) {
                        session = CreateUserSession(SessionType::Login, userid, username, local_key,
                                                    SessionConfirm::TOTP, totp_secret);
                    } else {
                        session = CreateUserSession(SessionType::Login, userid, username, local_key,
                                                    SessionConfirm::QRcode, nullptr);
                    }
                } else {
                    session = CreateUserSession(SessionType::Login, userid, username, local_key,
                                                SessionConfirm::None, nullptr);
                }

                if (RG_LIKELY(session)) {
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
            } else {
                RegisterFloodEvent(request.client_addr, username);
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

static RetainPtr<SessionInfo> CreateAutoSession(InstanceHolder *instance, SessionType type,
                                                const char *key, const char *sms)
{
    char tmp[128];

    int64_t userid = 0;
    const char *local_key = nullptr;

    sq_Statement stmt;
    if (!instance->db->Prepare("SELECT userid, local_key FROM ins_users WHERE key = ?1", &stmt))
        return nullptr;
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);

    if (stmt.Step()) {
        userid = sqlite3_column_int64(stmt, 0);
        local_key = (const char *)sqlite3_column_text(stmt, 1);
    } else if (stmt.IsValid()) {
        stmt.Finalize();

        bool success = instance->db->Transaction([&]() {
            if (!instance->db->Prepare("SELECT userid, local_key FROM ins_users WHERE key = ?1", &stmt))
                return false;
            sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);

            if (stmt.Step()) {
                userid = sqlite3_column_int64(stmt, 0);
                local_key = (const char *)sqlite3_column_text(stmt, 1);
            } else if (stmt.IsValid()) {
                local_key = tmp;

                // Create random local key
                {
                    uint8_t buf[32];
                    randombytes_buf(&buf, RG_SIZE(buf));
                    sodium_bin2base64((char *)local_key, 45, buf, RG_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
                }

                if (!instance->db->Run("INSERT INTO ins_users (key, local_key) VALUES (?1, ?2)", key, local_key))
                    return false;

                userid = sqlite3_last_insert_rowid(*instance->db);
            } else {
                return false;
            }

            return true;
        });
        if (!success)
            return nullptr;
    } else {
        return nullptr;
    }

    RG_ASSERT(userid > 0);
    userid = -userid;

    RetainPtr<SessionInfo> session;
    if (sms) {
        if (RG_UNLIKELY(gp_domain.config.sms.provider == sms_Provider::None)) {
            LogError("This instance is not configured to send SMS messages");
            return nullptr;
        }

        char code[9];
        {
            uint32_t rnd = randombytes_uniform(1000000); // 6 digits
            Fmt(code, "%1", FmtArg(rnd).Pad0(-6));
        }

        session = CreateUserSession(type, userid, key, local_key, SessionConfirm::SMS, code);
        if (RG_UNLIKELY(!session))
            return nullptr;

        char message[128];
        Fmt(message, "Code: %1", code);

        if (!SendSMS(sms, message))
            return nullptr;
    } else {
        session = CreateUserSession(type, userid, key, local_key, SessionConfirm::None, nullptr);
        if (RG_UNLIKELY(!session))
            return nullptr;
    }

    session->AuthorizeInstance(instance, (int)UserPermission::DataSave);

    return session;
}

// Returns true if not handled or not relevant, false if an error has occured
bool HandleSessionToken(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    Span<const char> token = request.GetQueryValue("token");
    if (!token.len)
        return true;

    if (!instance->config.token_key) {
        LogError("This instance does not use tokens");
        io->AttachError(403);
        return false;
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
            return false;
        }
        if (cypher_len < crypto_box_SEALBYTES) {
            LogError("Failed to unseal token");
            io->AttachError(403);
            return false;
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
            return false;
        }
    }

    // Parse JSON
    const char *sms = nullptr;
    const char *tid = nullptr;
    {
        StreamReader st(json);
        json_Parser parser(&st, &io->allocator);

        parser.ParseObject();
        while (parser.InObject()) {
            const char *key = "";
            parser.ParseKey(&key);

            if (TestStr(key, "sms")) {
                parser.ParseString(&sms);
            } else if (TestStr(key, "id")) {
                parser.ParseString(&tid);
            } else if (parser.IsValid()) {
                LogError("Unknown key '%1' in token JSON", key);
                io->AttachError(422);
                return false;
            }
        }
        if (!parser.IsValid()) {
            io->AttachError(422);
            return false;
        }
    }

    // Check token values
    {
        bool valid = true;

        if (sms && !sms[0]) {
            LogError("Empty SMS");
            valid = false;
        }
        if (!tid || !tid[0]) {
            LogError("Missing or empty token id");
            valid = false;
        }

        if (!valid) {
            io->AttachError(422);
            return false;
        }
    }

    if (sms) {
        // Avoid confirmation flood (SMS are costly)
        RegisterFloodEvent(request.client_addr, tid);
    }

    if (IsUserBanned(request.client_addr, tid)) {
        LogError("You are blocked for %1 minutes after excessive login failures", (BanTime + 59000) / 60000);
        io->AttachError(403);
        return false;
    }

    RetainPtr<SessionInfo> session = CreateAutoSession(instance, SessionType::Token, tid, sms);
    if (!session)
        return false;
    sessions.Open(request, io, session);

    return true;
}

// Returns true if not handled or not relevant, false if an error has occured
bool HandleSessionKey(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RG_ASSERT(instance->config.auto_key);

    const char *key = request.GetQueryValue(instance->config.auto_key);
    if (!key || !key[0])
        return true;

    RetainPtr<SessionInfo> session = CreateAutoSession(instance, SessionType::Key, key, nullptr);
    if (!session)
        return false;
    sessions.Open(request, io, session);

    return true;
}

void HandleSessionConfirm(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<SessionInfo> session = sessions.Find(request, io);

    if (!session) {
        LogError("Session is closed");
        io->AttachError(403);
        return;
    }

    std::lock_guard<std::shared_mutex> lock_excl(session->mutex);

    if (session->confirm == SessionConfirm::None) {
        LogError("Session does not need confirmation");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Read POST values
        const char *code;
        {
            HashMap<const char *, const char *> values;
            if (!io->ReadPostValues(&io->allocator, &values)) {
                io->AttachError(422);
                return;
            }

            code = values.FindValue("code", nullptr);
            if (!code) {
                LogError("Missing 'code' parameter");
                io->AttachError(422);
                return;
            }
        }

        if (IsUserBanned(request.client_addr, session->username)) {
            LogError("You are blocked for %1 minutes after excessive login failures", (BanTime + 59000) / 60000);
            io->AttachError(403);
            return;
        }

        // Immediate confirmation looks weird
        WaitDelay(800);

        switch (session->confirm) {
            case SessionConfirm::None: { RG_UNREACHABLE(); } break;

            case SessionConfirm::SMS: {
                if (TestStr(code, session->secret)) {
                    session->confirm = SessionConfirm::None;
                    sodium_memzero(session->secret, RG_SIZE(session->secret));

                    WriteProfileJson(session.GetRaw(), instance, request, io);
                } else {
                    if (RegisterFloodEvent(request.client_addr, session->username)) {
                        sessions.Close(request, io);
                        LogError("Code is incorrect; you are now blocked for %1 minutes", (BanTime + 59000) / 60000);
                    } else {
                        LogError("Code is incorrect");
                    }
                    io->AttachError(403);
                }
            } break;

            case SessionConfirm::TOTP:
            case SessionConfirm::QRcode: {
                int64_t time = GetUnixTime();

                if (sec_CheckHotp(session->secret, sec_HotpAlgorithm::SHA1, time / 30000, 6, 1, code)) {
                    if (session->confirm == SessionConfirm::QRcode) {
                        if (!gp_domain.db.Run("UPDATE dom_users SET totp_secret = ?2 WHERE userid = ?1",
                                              session->userid, session->secret))
                            return;
                    }

                    session->confirm = SessionConfirm::None;
                    sodium_memzero(session->secret, RG_SIZE(session->secret));

                    WriteProfileJson(session.GetRaw(), instance, request, io);
                } else {
                    if (RegisterFloodEvent(request.client_addr, session->username)) {
                        sessions.Close(request, io);
                        LogError("Code is incorrect; you are now blocked for %1 minutes", (BanTime + 59000) / 60000);
                    } else {
                        LogError("Code is incorrect");
                    }
                    io->AttachError(403);
                }
            } break;
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

void HandleChangePassword(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<SessionInfo> session = sessions.Find(request, io);

    if (!session) {
        LogError("User is not logged in");
        io->AttachError(401);
        return;
    }

    std::lock_guard<std::shared_mutex> lock_excl(session->mutex);

    if (session->type != SessionType::Login) {
        LogError("This account does not use passwords");
        io->AttachError(403);
        return;
    }
    if (session->confirm != SessionConfirm::None) {
        LogError("You must be fully logged in before you do that");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Read POST values
        const char *old_password;
        const char *new_password;
        {
            HashMap<const char *, const char *> values;
            if (!io->ReadPostValues(&io->allocator, &values)) {
                io->AttachError(422);
                return;
            }

            bool valid = true;

            old_password = values.FindValue("old_password", nullptr);
            if (!old_password) {
                LogError("Missing 'old_password' parameter");
                valid = false;
            }

            new_password = values.FindValue("new_password", nullptr);
            if (!new_password) {
                LogError("Missing 'new_password' parameter");
                valid = false;
            }

            if (!valid) {
                io->AttachError(422);
                return;
            }
        }

        // Check password strength
        if (!sec_CheckPassword(new_password, session->username)) {
            io->AttachError(422);
            return;
        }
        if (TestStr(new_password, old_password)) {
            LogError("This is the same password");
            io->AttachError(422);
            return;
        }

        // Authenticate with old password
        {
            // We use this to extend/fix the response delay in case of error
            int64_t now = GetMonotonicTime();

            sq_Statement stmt;
            if (!gp_domain.db.Prepare(R"(SELECT password_hash FROM dom_users
                                         WHERE userid = ?1)", &stmt))
                return;
            sqlite3_bind_int64(stmt, 1, session->userid);

            if (!stmt.Step()) {
                if (stmt.IsValid()) {
                    LogError("User does not exist");
                    io->AttachError(404);
                }
                return;
            }

            const char *password_hash = (const char *)sqlite3_column_text(stmt, 0);

            if (crypto_pwhash_str_verify(password_hash, old_password, strlen(old_password)) < 0) {
                // Enforce constant delay if authentification fails
                int64_t safety_delay = std::max(2000 - GetMonotonicTime() + now, (int64_t)0);
                WaitDelay(safety_delay);

                LogError("Invalid password");
                io->AttachError(403);
                return;
            }
        }

        // Hash password
        char new_hash[crypto_pwhash_STRBYTES];
        if (!HashPassword(new_password, new_hash))
            return;

        bool success = gp_domain.db.Transaction([&]() {
            int64_t time = GetUnixTime();

            if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username)
                                     VALUES (?1, ?2, ?3, ?4))",
                                  time, request.client_addr, "change_password", session->username))
                return false;
            if (!gp_domain.db.Run("UPDATE dom_users SET password_hash = ?2 WHERE userid = ?1",
                                  session->userid, new_hash))
                return false;

            return true;
        });
        if (!success)
            return;

        io->AttachText(200, "Done!");
    });
}

// This does not make any persistent change and it needs to return an image
// so it is a GET even though it performs an action (change the secret).
void HandleChangeQRcode(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<SessionInfo> session = sessions.Find(request, io);

    if (!session) {
        LogError("Session is closed");
        io->AttachError(403);
        return;
    }

    std::lock_guard<std::shared_mutex> lock_excl(session->mutex);

    if (session->type != SessionType::Login) {
        LogError("This account does not use passwords");
        io->AttachError(403);
        return;
    }
    if (session->confirm != SessionConfirm::None && session->confirm != SessionConfirm::QRcode) {
        LogError("Cannot generate QR code in this situation");
        io->AttachError(403);
        return;
    }

    sec_GenerateSecret(session->secret);

    const char *url = sec_GenerateHotpUrl(gp_domain.config.title, session->username, gp_domain.config.title,
                                          sec_HotpAlgorithm::SHA1, session->secret, 6, &io->allocator);
    if (!url)
        return;

    Span<const uint8_t> png;
    {
        HeapArray<uint8_t> buf(&io->allocator);
        if (!sec_GenerateHotpPng(url, 0, &buf))
            return;
        png = buf.Leak();
    }

    io->AttachBinary(200, png, "image/png");
    io->AddCachingHeaders(0, nullptr);
}

void HandleChangeTOTP(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<SessionInfo> session = sessions.Find(request, io);

    if (!session) {
        LogError("User is not logged in");
        io->AttachError(401);
        return;
    }

    std::lock_guard<std::shared_mutex> lock_excl(session->mutex);

    if (session->type != SessionType::Login) {
        LogError("This account does not use passwords");
        io->AttachError(403);
        return;
    }
    if (session->confirm != SessionConfirm::None) {
        LogError("You must be fully logged in before you do that");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Read POST values
        const char *password;
        const char *code;
        {
            HashMap<const char *, const char *> values;
            if (!io->ReadPostValues(&io->allocator, &values)) {
                io->AttachError(422);
                return;
            }

            bool valid = true;

            password = values.FindValue("password", nullptr);
            if (!password) {
                LogError("Missing 'password' parameter");
                valid = false;
            }

            code = values.FindValue("code", nullptr);
            if (!code) {
                LogError("Missing 'code' parameter");
                valid = false;
            }

            if (!valid) {
                io->AttachError(422);
                return;
            }
        }

        int64_t time = GetUnixTime();

        // Authenticate with password
        {
            // We use this to extend/fix the response delay in case of error
            int64_t now = GetMonotonicTime();

            sq_Statement stmt;
            if (!gp_domain.db.Prepare(R"(SELECT password_hash FROM dom_users
                                         WHERE userid = ?1)", &stmt))
                return;
            sqlite3_bind_int64(stmt, 1, session->userid);

            if (!stmt.Step()) {
                if (stmt.IsValid()) {
                    LogError("User does not exist");
                    io->AttachError(404);
                }
                return;
            }

            const char *password_hash = (const char *)sqlite3_column_text(stmt, 0);

            if (crypto_pwhash_str_verify(password_hash, password, strlen(password)) < 0) {
                // Enforce constant delay if authentification fails
                int64_t safety_delay = std::max(2000 - GetMonotonicTime() + now, (int64_t)0);
                WaitDelay(safety_delay);

                LogError("Invalid password");
                io->AttachError(403);
                return;
            }
        }

        // Check user knows secret
        if (!sec_CheckHotp(session->secret, sec_HotpAlgorithm::SHA1, time / 30000, 6, 1, code)) {
            LogError("Code is incorrect");
            io->AttachError(403);
            return;
        }

        bool success = gp_domain.db.Transaction([&]() {
            int64_t time = GetUnixTime();

            if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username)
                                     VALUES (?1, ?2, ?3, ?4))",
                                  time, request.client_addr, "change_totp", session->username))
                return false;
            if (!gp_domain.db.Run("UPDATE dom_users SET totp_required = 1, totp_secret = ?2 WHERE userid = ?1",
                                  session->userid, session->secret))
                return false;

            return true;
        });
        if (!success)
            return;

        io->AttachText(200, "Done!");
    });
}

}
