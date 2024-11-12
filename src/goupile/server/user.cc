// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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
#include "domain.hh"
#include "goupile.hh"
#include "instance.hh"
#include "message.hh"
#include "user.hh"
#include "src/core/http/http.hh"
#include "src/core/password/otp.hh"
#include "src/core/password/password.hh"
#include "src/core/wrap/qrcode.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static_assert(PasswordHashBytes == crypto_pwhash_STRBYTES);

static const int BanThreshold = 6;
static const int64_t BanTime = 1800 * 1000;
static const int64_t TotpPeriod = 30000;
static const int MaxChallengeDelay = 5000 * 1000;

struct EventInfo {
    struct Key {
        const char *where;
        const char *who;

        bool operator==(const Key &other) const { return TestStr(where, other.where) && TestStr(who, other.who); }
        bool operator!=(const Key &other) const { return !(*this == other); }

        uint64_t Hash() const
        {
            uint64_t hash = HashTraits<const char *>::Hash(where) ^
                            HashTraits<const char *>::Hash(who);
            return hash;
        }
    };

    Key key;
    int64_t until; // Monotonic

    int count;
    int64_t prev_time; // Unix time
    int64_t time; // Unix time

    RG_HASHTABLE_HANDLER(EventInfo, key);
};

static http_SessionManager<SessionInfo> sessions;

static std::shared_mutex events_mutex;
static BucketArray<EventInfo> events;
static HashTable<EventInfo::Key, EventInfo *> events_map;

bool SessionInfo::IsAdmin() const
{
    if (!is_admin)
        return false;

    if (change_password)
        return false;
    if (confirm != SessionConfirm::None)
        return false;

    return true;
}

bool SessionInfo::IsRoot() const
{
    return is_root && IsAdmin();
}

bool SessionInfo::HasPermission(const InstanceHolder *instance, UserPermission perm) const
{
    const SessionStamp *stamp = GetStamp(instance);
    return stamp && stamp->HasPermission(perm);
}

SessionStamp *SessionInfo::GetStamp(const InstanceHolder *instance) const
{
    if (change_password)
        return nullptr;
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
                uint32_t permissions;
                {
                    sq_Statement stmt;
                    if (!gp_domain.db.Prepare(R"(SELECT permissions FROM dom_permissions
                                                 WHERE userid = ?1 AND instance = ?2)", &stmt))
                        return nullptr;
                    sqlite3_bind_int64(stmt, 1, userid);
                    sqlite3_bind_text(stmt, 2, instance->key.ptr, (int)instance->key.len, SQLITE_STATIC);
                    if (!stmt.Step())
                        return nullptr;

                    permissions = (uint32_t)sqlite3_column_int(stmt, 0);
                }

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
    if (is_admin && !is_root) {
        sq_Statement stmt;
        if (!gp_domain.db.Prepare(R"(SELECT IIF(a.permissions IS NOT NULL, 1, 0) AS admin
                                     FROM dom_users u
                                     INNER JOIN dom_permissions a ON (a.userid = u.userid AND
                                                                      a.permissions & ?2)
                                     WHERE u.userid = ?1)", &stmt))
            return;
        sqlite3_bind_int64(stmt, 1, userid);
        sqlite3_bind_int(stmt, 2, (int)UserPermission::BuildAdmin);

        if (!stmt.Step()) {
            is_admin = false;
            admin_until = 0;
        }
    }

    std::lock_guard<std::shared_mutex> lock_excl(mutex);
    stamps_map.Clear();

    // We can't clear the array or the allocator because the stamps may
    // be in use so they will waste memory until the session ends.
}

void SessionInfo::AuthorizeInstance(const InstanceHolder *instance, uint32_t permissions, const char *lock)
{
    std::lock_guard<std::shared_mutex> lock_excl(mutex);

    SessionStamp *stamp = stamps.AppendDefault();

    stamp->unique = instance->unique;
    stamp->authorized = true;
    stamp->permissions = permissions;
    stamp->lock = lock ? DuplicateString(lock, &stamps_alloc).ptr : nullptr;

    stamps_map.Set(stamp);
}

static void WriteProfileJson(http_IO *io, const SessionInfo *session, const InstanceHolder *instance)
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

        if (session->change_password) {
            json.Key("authorized"); json.Bool(false);
            json.Key("confirm"); json.String("password");
        } else if (confirm != SessionConfirm::None) {
            json.Key("authorized"); json.Bool(false);

            switch (confirm) {
                case SessionConfirm::None: { RG_UNREACHABLE(); } break;
                case SessionConfirm::Mail: { json.Key("confirm"); json.String("mail"); } break;
                case SessionConfirm::SMS: { json.Key("confirm"); json.String("sms"); } break;
                case SessionConfirm::TOTP: { json.Key("confirm"); json.String("totp"); } break;
                case SessionConfirm::QRcode: { json.Key("confirm"); json.String("qrcode"); } break;
            }
        } else if (instance) {
            const InstanceHolder *master = instance->master;
            const SessionStamp *stamp = session->GetStamp(instance);

            if (!stamp) {
                for (const InstanceHolder *slave: instance->slaves) {
                    stamp = session->GetStamp(slave);

                    if (stamp) {
                        instance = slave;
                        break;
                    }
                }
            }

            if (stamp) {
                json.Key("instance"); json.String(instance->key.ptr);
                json.Key("authorized"); json.Bool(true);

                switch (session->type) {
                    case SessionType::Login: { json.Key("type"); json.String("login"); } break;
                    case SessionType::Token: { json.Key("type"); json.String("token"); } break;
                    case SessionType::Key: {
                        RG_ASSERT(instance->config.auto_key);
                        json.Key("type"); json.String("key");
                    } break;
                    case SessionType::Auto: { json.Key("type"); json.String("auto"); } break;
                }

                if (!instance->slaves.len) {
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
                    if (session->type == SessionType::Login) {
                        json.Key("lock"); json.String(instance->config.lock_key);
                    }
                    json.EndObject();
                }

                if (master->slaves.len) {
                    json.Key("instances"); json.StartArray();
                    for (const InstanceHolder *slave: master->slaves) {
                        if (session->GetStamp(slave)) {
                            json.StartObject();
                            json.Key("key"); json.String(slave->key.ptr);
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
                    Span<const char> key = json_ConvertToJsonName(UserPermissionNames[i], buf);
                    json.Key(key.ptr, (size_t)key.len); json.Bool(stamp->permissions & (1 << i));
                }
                json.EndObject();

                if (stamp->lock) {
                    json.Key("lock"); json.Raw(stamp->lock);
                }

                if (stamp->HasPermission(UserPermission::BuildCode)) {
                    const SessionStamp *stamp = session->GetStamp(master);
                    json.Key("develop"); json.Bool(stamp && stamp->develop.load(std::memory_order_relaxed));
                } else {
                    RG_ASSERT(!stamp->develop.load());
                }

                json.Key("root"); json.Bool(session->is_root);
            } else {
                json.Key("authorized"); json.Bool(false);
            }
        } else {
            bool authorized = (session->is_admin && session->admin_until > GetMonotonicTime());

            json.Key("authorized"); json.Bool(authorized);
            json.Key("root"); json.Bool(session->is_root);
        }
    }
    json.EndObject();

    json.Finish();
}

static RetainPtr<SessionInfo> CreateUserSession(SessionType type, int64_t userid,
                                                const char *username, const char *local_key)
{
    Size username_bytes = strlen(username) + 1;
    Size session_bytes = RG_SIZE(SessionInfo) + username_bytes;

    SessionInfo *session = (SessionInfo *)AllocateRaw(nullptr, session_bytes, (int)AllocFlag::Zero);

    new (session) SessionInfo;
    RetainPtr<SessionInfo> ptr(session, [](SessionInfo *session) {
        session->~SessionInfo();
        ReleaseRaw(nullptr, session, -1);
    });

    session->type = type;
    session->userid = userid;
    CopyString(local_key, session->local_key);
    CopyString(username, MakeSpan((char *)session->username, username_bytes));

    return ptr;
}

RetainPtr<SessionInfo> LoginUserAuto(http_IO *io, int64_t userid)
{
    RG_ASSERT(userid > 0);

    sq_Statement stmt;
    if (!gp_domain.db.Prepare(R"(SELECT username, local_key, change_password
                                 FROM dom_users
                                 WHERE userid = ?1)", &stmt))
        return nullptr;
    sqlite3_bind_int64(stmt, 1, userid);

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("User ID %1 does not exist");
            io->SendError(404);
        }
        return nullptr;
    }

    const char *username = (const char *)sqlite3_column_text(stmt, 0);
    const char *local_key = (const char *)sqlite3_column_text(stmt, 1);
    bool change_password = sqlite3_column_int(stmt, 2);

    RetainPtr<SessionInfo> session = CreateUserSession(SessionType::Login, userid, username, local_key);
    if (!session.IsValid())
        return nullptr;
    session->change_password = change_password;

    sessions.Open(io, session);

    return session;
}

void InvalidateUserStamps(int64_t userid)
{
    // Deal with real sessions
    sessions.ApplyAll([&](SessionInfo *session) {
        if (session->userid == userid) {
            session->InvalidateStamps();
        }
    });
}

RetainPtr<const SessionInfo> GetNormalSession(http_IO *io, InstanceHolder *instance)
{
    RetainPtr<SessionInfo> session = sessions.Find(io);

    if (!session && instance && instance->config.allow_guests) {
        // Create local key
        char local_key[45];
        {
            uint8_t buf[32];
            FillRandomSafe(buf);
            sodium_bin2base64(local_key, RG_SIZE(local_key), buf, RG_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
        }

        session = CreateUserSession(SessionType::Auto, 0, "Guest", local_key);

        uint32_t permissions = (int)UserPermission::DataNew | (int)UserPermission::DataEdit;
        session->AuthorizeInstance(instance, permissions);

        // sessions.Open(request, io, session);
    }

    return session;
}

RetainPtr<const SessionInfo> GetAdminSession(http_IO *io, InstanceHolder *instance)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);

    if (!session)
        return nullptr;
    if (!session->is_admin)
        return nullptr;
    if (session->admin_until <= GetMonotonicTime())
        return nullptr;

    return session;
}

void PruneSessions()
{
    // Prune sessions
    sessions.Prune();

    // Prune events
    {
        std::lock_guard<std::shared_mutex> lock_excl(events_mutex);

        int64_t now = GetMonotonicTime();

        Size expired = 0;
        for (const EventInfo &event: events) {
            if (event.until > now)
                break;

            EventInfo **ptr = events_map.Find(event.key);
            if (*ptr == &event) {
                events_map.Remove(ptr);
            }
            expired++;
        }
        events.RemoveFirst(expired);

        events.Trim();
        events_map.Trim();
    }
}

bool HashPassword(Span<const char> password, char out_hash[PasswordHashBytes])
{
    if (crypto_pwhash_str(out_hash, password.ptr, password.len,
                          crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        LogError("Failed to hash password");
        return false;
    }

    return true;
}

static const EventInfo *RegisterEvent(const char *where, const char *who, int64_t time = GetUnixTime())
{
    std::lock_guard<std::shared_mutex> lock_excl(events_mutex);

    EventInfo::Key key = { where, who };
    EventInfo *event = events_map.FindValue(key, nullptr);

    if (!event || event->until < GetMonotonicTime()) {
        Allocator *alloc;
        event = events.AppendDefault(&alloc);

        event->key.where = DuplicateString(where, alloc).ptr;
        event->key.who = DuplicateString(who, alloc).ptr;
        event->until = GetMonotonicTime() + BanTime;

        events_map.Set(event);
    }

    event->count++;
    event->prev_time = event->time;
    event->time = time;

    return event;
}

static int CountEvents(const char *where, const char *who)
{
    std::shared_lock<std::shared_mutex> lock_shr(events_mutex);

    EventInfo::Key key = { where, who };
    const EventInfo *event = events_map.FindValue(key, nullptr);

    // We don't need to use precise timing, and a ban can last a bit
    // more than BanTime (until pruning clears the ban).
    return event ? event->count : 0;
}

static bool CheckPasswordComplexity(const SessionInfo &session, Span<const char> password)
{
    PasswordComplexity treshold;
    unsigned int flags = 0;

    if (session.is_root) {
        treshold = gp_domain.config.root_password;
    } else if (session.is_admin) {
        treshold = gp_domain.config.admin_password;
    } else {
        treshold = gp_domain.config.user_password;
    }

    switch (treshold) {
        case PasswordComplexity::Easy: { flags = UINT_MAX & ~(int)pwd_CheckFlag::Classes & ~(int)pwd_CheckFlag::Score; } break;
        case PasswordComplexity::Moderate: { flags = UINT_MAX & ~(int)pwd_CheckFlag::Score; } break;
        case PasswordComplexity::Hard: { flags = UINT_MAX; } break;
    }
    RG_ASSERT(flags);

    return pwd_CheckPassword(password, session.username, flags);
}

void HandleSessionLogin(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();

    const char *username = nullptr;
    Span<const char> password = {};
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "username") {
                parser.ParseString(&username);
            } else if (key == "password") {
                parser.ParseString(&password);
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

    // Check for missing values
    if (!username || !password.ptr) {
        LogError("Missing 'username' or 'password' parameter");
        io->SendError(422);
        return;
    }
    if (password.len > pwd_MaxLength) {
        LogError("Excessive password length");
        io->SendError(422);
        return;
    }

    // We use this to extend/fix the response delay in case of error
    int64_t now = GetMonotonicTime();

    sq_Statement stmt;
    if (instance) {
        InstanceHolder *master = instance->master;

        if (!instance->slaves.len) {
            if (!gp_domain.db.Prepare(R"(SELECT u.userid, u.password_hash, u.change_password,
                                                u.root, IIF(a.permissions IS NOT NULL, 1, 0) AS admin,
                                                u.local_key, u.confirm, u.secret, p.permissions
                                         FROM dom_users u
                                         INNER JOIN dom_permissions p ON (p.userid = u.userid)
                                         INNER JOIN dom_instances i ON (i.instance = p.instance)
                                         LEFT JOIN dom_permissions a ON (a.userid = u.userid AND
                                                                         a.permissions & ?3)
                                         WHERE u.username = ?1 AND i.instance = ?2 AND
                                               p.permissions > 0)", &stmt))
                return;
            sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, instance->key.ptr, (int)instance->key.len, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, (int)UserPermission::BuildAdmin);

            stmt.Run();
        }

        if (!stmt.IsRow() && master->slaves.len) {
            instance = master;

            if (!gp_domain.db.Prepare(R"(SELECT u.userid, u.password_hash, u.change_password,
                                                u.root, IIF(a.permissions IS NOT NULL, 1, 0) AS admin,
                                                u.local_key, u.confirm, u.secret
                                         FROM dom_users u
                                         INNER JOIN dom_permissions p ON (p.userid = u.userid)
                                         INNER JOIN dom_instances i ON (i.instance = p.instance)
                                         LEFT JOIN dom_permissions a ON (a.userid = u.userid AND
                                                                         a.permissions & ?3)
                                         WHERE u.username = ?1 AND i.master = ?2 AND
                                               p.permissions > 0)", &stmt))
                return;
            sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, master->key.ptr, (int)master->key.len, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, (int)UserPermission::BuildAdmin);

            stmt.Run();
        }
    } else {
        if (!gp_domain.db.Prepare(R"(SELECT u.userid, u.password_hash, u.change_password,
                                            u.root, IIF(a.permissions IS NOT NULL, 1, 0) AS admin,
                                            u.local_key, u.confirm, u.secret
                                     FROM dom_users u
                                     LEFT JOIN dom_permissions a ON (a.userid = u.userid AND
                                                                     a.permissions & ?2)
                                     WHERE u.username = ?1 AND (u.root = 1 OR
                                                                a.permissions IS NOT NULL))", &stmt))
            return;
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, (int)UserPermission::BuildAdmin);

        stmt.Run();
    }

    if (stmt.IsRow()) {
        int64_t userid = sqlite3_column_int64(stmt, 0);
        const char *password_hash = (const char *)sqlite3_column_text(stmt, 1);
        bool change_password = (sqlite3_column_int(stmt, 2) == 1);
        bool root = (sqlite3_column_int(stmt, 3) == 1);
        bool admin = root || (sqlite3_column_int(stmt, 4) == 1);
        const char *local_key = (const char *)sqlite3_column_text(stmt, 5);
        const char *confirm = (const char *)sqlite3_column_text(stmt, 6);
        const char *secret = (const char *)sqlite3_column_text(stmt, 7);

        if (CountEvents(request.client_addr, username) >= BanThreshold) {
            LogError("You are blocked for %1 minutes after excessive login failures", (BanTime + 59000) / 60000);
            io->SendError(403);
            return;
        }

        if (password_hash && crypto_pwhash_str_verify(password_hash, password.ptr, (size_t)password.len) == 0) {
            int64_t time = GetUnixTime();

            if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username)
                                     VALUES (?1, ?2, ?3, ?4))",
                                  time, request.client_addr, "login", username))
                return;

            RetainPtr<SessionInfo> session;
            if (!confirm) {
                session = CreateUserSession(SessionType::Login, userid, username, local_key);
            } else if (TestStr(confirm, "TOTP")) {
                if (secret) {
                    if (strlen(secret) >= RG_SIZE(SessionInfo::secret)) {
                        // Should never happen, but let's be careful
                        LogError("Session secret is too big");
                        return;
                    }

                    session = CreateUserSession(SessionType::Login, userid, username, local_key);
                    session->confirm = SessionConfirm::TOTP;
                    CopyString(secret, session->secret);
                } else {
                    session = CreateUserSession(SessionType::Login, userid, username, local_key);
                    session->confirm = SessionConfirm::QRcode;
                }
            } else {
                LogError("Invalid confirmation method '%1'", confirm);
                return;
            }

            if (session) [[likely]] {
                session->is_root = root;
                session->is_admin = admin;

                if (!instance && (root || admin)) {
                    session->admin_until = GetMonotonicTime() + 1200 * 1000;
                }

                session->change_password = change_password;

                sessions.Open(io, session);
                WriteProfileJson(io, session.GetRaw(), instance);
            }

            return;
        } else {
            RegisterEvent(request.client_addr, username);
        }
    }

    if (stmt.IsValid()) {
        // Enforce constant delay if authentification fails
        int64_t safety_delay = std::max(2000 - GetMonotonicTime() + now, (int64_t)0);
        WaitDelay(safety_delay);

        LogError("Invalid username or password");
        io->SendError(403);
    }
}

static RetainPtr<SessionInfo> CreateAutoSession(InstanceHolder *instance, SessionType type, const char *key,
                                                const char *username, const char *email, const char *sms, const char *lock)
{
    RG_ASSERT(!email || !sms);

    BlockAllocator temp_alloc;

    int64_t userid = 0;
    const char *local_key = nullptr;

    sq_Statement stmt;
    if (!instance->db->Prepare("SELECT userid, local_key FROM ins_users WHERE key = ?1", &stmt))
        return nullptr;
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);

    if (!stmt.Step()) {
        if (!stmt.IsValid())
            return nullptr;
        stmt.Finalize();

        local_key = (const char *)AllocateRaw(&temp_alloc, 128);

        // Create random local key
        {
            uint8_t buf[32];
            FillRandomSafe(buf);
            sodium_bin2base64((char *)local_key, 45, buf, RG_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
        }

        if (!instance->db->Prepare(R"(INSERT INTO ins_users (key, local_key)
                                      VALUES (?1, ?2)
                                      ON CONFLICT DO NOTHING
                                      RETURNING userid, local_key)",
                                   &stmt, key, local_key))
            return nullptr;

        if (!stmt.Step()) {
            RG_ASSERT(!stmt.IsValid());
            return nullptr;
        }
    }

    userid = sqlite3_column_int64(stmt, 0);
    local_key = (const char *)sqlite3_column_text(stmt, 1);

    RG_ASSERT(userid > 0);
    userid = -userid;

    RetainPtr<SessionInfo> session;

    if (email) {
        if (!gp_domain.config.smtp.url) [[unlikely]] {
            LogError("This instance is not configured to send mails");
            return nullptr;
        }

        char code[9];
        {
            static_assert(RG_SIZE(code) <= RG_SIZE(SessionInfo::secret));

            uint32_t rnd = randombytes_uniform(100000000); // 8 digits
            Fmt(code, "%1", FmtArg(rnd).Pad0(-8));
        }

        session = CreateUserSession(type, userid, username, local_key);
        session->confirm = SessionConfirm::Mail;
        CopyString(code, session->secret);

        smtp_MailContent content;
        content.subject = Fmt(&temp_alloc, "Vérification %1", instance->title).ptr;
        content.text = Fmt(&temp_alloc, "Code: %1", code).ptr;
        content.html = Fmt(&temp_alloc, R"(
            <div style="text-align: center;">
                <p style="font-size: 1.3em;">Code de vérification</p>
                <p style="font-size: 3em; font-weight: bold;">%1</p>
            </div>
        )", code).ptr;

        if (!SendMail(email, content))
            return nullptr;
    } else if (sms) {
        if (gp_domain.config.sms.provider == sms_Provider::None) [[unlikely]] {
            LogError("This instance is not configured to send SMS messages");
            return nullptr;
        }

        char code[9];
        {
            static_assert(RG_SIZE(code) <= RG_SIZE(SessionInfo::secret));

            uint32_t rnd = randombytes_uniform(1000000); // 6 digits
            Fmt(code, "%1", FmtArg(rnd).Pad0(-6));
        }

        session = CreateUserSession(type, userid, username, local_key);
        session->confirm = SessionConfirm::SMS;
        CopyString(code, session->secret);

        const char *message = Fmt(&temp_alloc, "Code: %1", code).ptr;

        if (!SendSMS(sms, message))
            return nullptr;
    } else {
        session = CreateUserSession(type, userid, username, local_key);
    }

    uint32_t permissions = (int)UserPermission::DataNew | (int)UserPermission::DataEdit;
    session->AuthorizeInstance(instance, permissions, lock);

    return session;
}

void HandleSessionToken(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();

    if (!instance->config.token_key) {
        LogError("This instance does not use tokens");
        io->SendError(403);
        return;
    }

    Span<const char> token = {};
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "token") {
                parser.ParseString(&token);
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

    // Check for missing values
    if (!token.ptr) {
        LogError("Missing 'token' parameter");
        io->SendError(422);
        return;
    }

    // Decode Base64
    Span<uint8_t> cypher;
    {
        cypher = AllocateSpan<uint8_t>(io->Allocator(), token.len / 2 + 1);

        size_t cypher_len;
        if (sodium_hex2bin(cypher.ptr, (size_t)cypher.len, token.ptr, (size_t)token.len,
                           nullptr, &cypher_len, nullptr) != 0) {
            LogError("Failed to unseal token");
            io->SendError(403);
            return;
        }
        if (cypher_len < crypto_box_SEALBYTES) {
            LogError("Failed to unseal token");
            io->SendError(403);
            return;
        }

        cypher.len = (Size)cypher_len;
    }

    // Decode token
    Span<uint8_t> json;
    {
        json = AllocateSpan<uint8_t>(io->Allocator(), cypher.len - crypto_box_SEALBYTES);

        if (crypto_box_seal_open((uint8_t *)json.ptr, cypher.ptr, cypher.len,
                                 instance->config.token_pkey, instance->config.token_skey) != 0) {
            LogError("Failed to unseal token");
            io->SendError(403);
            return;
        }
    }

    // Parse JSON
    const char *email = nullptr;
    const char *sms = nullptr;
    const char *id = nullptr;
    const char *username = nullptr;
    HeapArray<const char *> claims;
    const char *lock = nullptr;
    {
        StreamReader st(json);
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            const char *key = "";
            parser.ParseKey(&key);

            if (TestStr(key, "key") || TestStr(key, "id")) {
                parser.ParseString(&id);
            } else if (TestStr(key, "email")) {
                parser.ParseString(&email);
            } else if (TestStr(key, "sms")) {
                parser.ParseString(&sms);
            } else if (TestStr(key, "username")) {
                parser.ParseString(&username);
            } else if (TestStr(key, "claims")) {
                parser.ParseArray();
                while (parser.InArray()) {
                    const char *claim = "";
                    parser.ParseString(&claim);
                    claims.Append(claim);
                }
            } else if (TestStr(key, "lock")) {
                if (instance->legacy) {
                    parser.ParseString(&lock);
                } else {
                    parser.PassThrough(&lock);
                }
            } else if (parser.IsValid()) {
                LogError("Unknown key '%1' in token JSON", key);
                io->SendError(422);
                return;
            }
        }
        if (!parser.IsValid()) {
            io->SendError(422);
            return;
        }
    }

    // Check token values
    {
        bool valid = true;

        if (!id || !id[0]) {
            LogError("Missing or empty key");
            valid = false;
        }
        if (email && !email[0]) {
            LogError("Empty email address");
            valid = false;
        }
        if (sms && !sms[0]) {
            LogError("Empty SMS phone number");
            valid = false;
        }

        if (!username || !username[0]) {
            username = id;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    if (email || sms) {
        // Avoid confirmation event (spam for mails, and SMS are costly)
        RegisterEvent(request.client_addr, id);
    }

    if (CountEvents(request.client_addr, id) >= BanThreshold) {
        LogError("You are blocked for %1 minutes after excessive login failures", (BanTime + 59000) / 60000);
        io->SendError(403);
        return;
    }

    RetainPtr<SessionInfo> session = CreateAutoSession(instance, SessionType::Token, id, username, email, sms, lock);
    if (!session)
        return;

    if (claims.len) {
        bool success = instance->db->Transaction([&]() {
            RG_ASSERT(session->userid < 0);

            for (const char *claim: claims) {
                if (!instance->db->Run(R"(INSERT INTO ins_claims (userid, ulid) VALUES (?1, ?2)
                                          ON CONFLICT DO NOTHING)", -session->userid, claim))
                    return false;
            }

            return true;
        });
        if (!success) {
            // The FOREIGN KEY check is deferred so the error happens on COMMIT
            LogError("Token contains invalid claims");
            io->SendError(422);
            return;
        }
    }

    sessions.Open(io, session);

    io->SendText(200, "{}", "application/json");
}

void HandleSessionKey(http_IO *io, InstanceHolder *instance)
{
    const char *session_key = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "key") {
                parser.ParseString(&session_key);
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

    // Check for missing values
    if (!session_key) {
        LogError("Missing 'key' parameter");
        io->SendError(422);
        return;
    }

    RetainPtr<SessionInfo> session = CreateAutoSession(instance, SessionType::Key, session_key, session_key, nullptr, nullptr, nullptr);
    if (!session)
        return;
    sessions.Open(io, session);

    io->SendText(200, "{}", "application/json");
}

static bool CheckTotp(http_IO *io, const SessionInfo &session, InstanceHolder *instance, const char *code)
{
    int64_t time = GetUnixTime();
    int64_t counter = time / TotpPeriod;
    int64_t min = counter - 1;
    int64_t max = counter + 1;

    if (pwd_CheckHotp(session.secret, pwd_HotpAlgorithm::SHA1, min, max, 6, code)) {
        RG_ASSERT(session.userid > 0 || instance);

        const char *where = (session.userid > 0) ? "" : instance->key.ptr;
        const EventInfo *event = RegisterEvent(where, session.username, time);

        bool replay = (event->prev_time / TotpPeriod >= min) &&
                      pwd_CheckHotp(session.secret, pwd_HotpAlgorithm::SHA1, min, event->prev_time / TotpPeriod, 6, code);

        if (replay) {
            LogError("Please wait for the next code");
            io->SendError(403);
            return false;
        }

        return true;
    } else {
        LogError("Code is incorrect");
        io->SendError(403);
        return false;
    }
}

void HandleSessionConfirm(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<SessionInfo> session = sessions.Find(io);

    if (!session) {
        LogError("Session is closed");
        io->SendError(403);
        return;
    }

    std::unique_lock<std::shared_mutex> lock_excl(session->mutex);

    if (session->confirm == SessionConfirm::None) {
        LogError("Session does not need confirmation");
        io->SendError(403);
        return;
    }

    const char *code = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "code") {
                parser.ParseString(&code);
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

    // Check for missing values
    if (!code) {
        LogError("Missing 'code' parameter");
        io->SendError(422);
        return;
    }

    if (CountEvents(request.client_addr, session->username) >= BanThreshold) {
        LogError("You are blocked for %1 minutes after excessive login failures", (BanTime + 59000) / 60000);
        io->SendError(403);
        return;
    }

    // Immediate confirmation looks weird
    WaitDelay(800);

    switch (session->confirm) {
        case SessionConfirm::None: { RG_UNREACHABLE(); } break;

        case SessionConfirm::Mail:
        case SessionConfirm::SMS: {
            if (TestStr(code, session->secret)) {
                session->confirm = SessionConfirm::None;
                sodium_memzero(session->secret, RG_SIZE(session->secret));

                WriteProfileJson(io, session.GetRaw(), instance);
            } else {
                const EventInfo *event = RegisterEvent(request.client_addr, session->username);

                if (event->count >= BanThreshold) {
                    sessions.Close(io);
                    LogError("Code is incorrect; you are now blocked for %1 minutes", (BanTime + 59000) / 60000);
                    io->SendError(403);
                }
            }
        } break;

        case SessionConfirm::TOTP:
        case SessionConfirm::QRcode: {
            if (CheckTotp(io, *session, instance, code)) {
                if (session->confirm == SessionConfirm::QRcode) {
                    if (!gp_domain.db.Run("UPDATE dom_users SET secret = ?2 WHERE userid = ?1",
                                          session->userid, session->secret))
                        return;
                }

                session->confirm = SessionConfirm::None;
                sodium_memzero(session->secret, RG_SIZE(session->secret));

                lock_excl.unlock();
                WriteProfileJson(io, session.GetRaw(), instance);
            } else {
                const EventInfo *event = RegisterEvent(request.client_addr, session->username);

                if (event->count >= BanThreshold) {
                    sessions.Close(io);
                    LogError("Code is incorrect; you are now blocked for %1 minutes", (BanTime + 59000) / 60000);
                    io->SendError(403);
                }
            }
        } break;
    }
}

void HandleSessionLogout(http_IO *io)
{
    sessions.Close(io);
    io->SendText(200, "{}", "application/json");
}

void HandleSessionProfile(http_IO *io, InstanceHolder *instance)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);
    WriteProfileJson(io, session.GetRaw(), instance);
}

void HandleChangePassword(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<SessionInfo> session = sessions.Find(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    std::unique_lock<std::shared_mutex> lock_excl(session->mutex);

    if (session->type != SessionType::Login) {
        LogError("This account does not use passwords");
        io->SendError(403);
        return;
    }
    if (session->confirm != SessionConfirm::None && !session->change_password) {
        LogError("You must be fully logged in before you do that");
        io->SendError(403);
        return;
    }

    const char *old_password = nullptr;
    const char *new_password = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "old_password") {
                parser.SkipNull() || parser.ParseString(&old_password);
            } else if (key == "new_password") {
                parser.ParseString(&new_password);
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

    // Check missing values
    {
        bool valid = true;

        if (!old_password && !session->change_password) {
            LogError("Missing 'old_password' parameter");
            valid = false;
        }
        if (!new_password) {
            LogError("Missing 'new_password' parameter");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }
    RG_ASSERT(old_password || session->change_password);

    // Complex enough?
    if (!CheckPasswordComplexity(*session, new_password)) {
        io->SendError(422);
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
                io->SendError(404);
            }
            return;
        }

        const char *password_hash = (const char *)sqlite3_column_text(stmt, 0);

        if (old_password) {
            if (!password_hash || crypto_pwhash_str_verify(password_hash, old_password, strlen(old_password)) < 0) {
                // Enforce constant delay if authentification fails
                int64_t safety_delay = std::max(2000 - GetMonotonicTime() + now, (int64_t)0);
                WaitDelay(safety_delay);

                LogError("Invalid password");
                io->SendError(403);
                return;
            }

            if (TestStr(new_password, old_password)) {
                LogError("You cannot reuse the same password");
                io->SendError(422);
                return;
            }
        } else {
            if (password_hash && crypto_pwhash_str_verify(password_hash, new_password, strlen(new_password)) == 0) {
                LogError("You cannot reuse the same password");
                io->SendError(422);
                return;
            }
        }
    }

    // Hash password
    char new_hash[PasswordHashBytes];
    if (!HashPassword(new_password, new_hash))
        return;

    bool success = gp_domain.db.Transaction([&]() {
        int64_t time = GetUnixTime();

        if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username)
                                 VALUES (?1, ?2, ?3, ?4))",
                              time, request.client_addr, "change_password", session->username))
            return false;
        if (!gp_domain.db.Run("UPDATE dom_users SET password_hash = ?2, change_password = 0 WHERE userid = ?1",
                              session->userid, new_hash))
            return false;

        return true;
    });
    if (!success)
        return;

    if (session->change_password) {
        session->change_password = false;

        lock_excl.unlock();
        WriteProfileJson(io, session.GetRaw(), instance);
    } else {
        io->SendText(200, "{}", "application/json");
    }
}

// This does not make any persistent change and it needs to return an image
// so it is a GET even though it performs an action (change the secret).
void HandleChangeQRcode(http_IO *io)
{
    RetainPtr<SessionInfo> session = sessions.Find(io);

    if (!session) {
        LogError("Session is closed");
        io->SendError(403);
        return;
    }

    std::lock_guard<std::shared_mutex> lock_excl(session->mutex);

    if (session->type != SessionType::Login) {
        LogError("This account does not use passwords");
        io->SendError(403);
        return;
    }
    if (session->confirm != SessionConfirm::None && session->confirm != SessionConfirm::QRcode) {
        LogError("Cannot generate QR code in this situation");
        io->SendError(403);
        return;
    }

    pwd_GenerateSecret(session->secret);

    const char *url = pwd_GenerateHotpUrl(gp_domain.config.title, session->username, gp_domain.config.title,
                                          pwd_HotpAlgorithm::SHA1, session->secret, 6, io->Allocator());
    if (!url)
        return;

    Span<const uint8_t> png;
    {
        HeapArray<uint8_t> buf(io->Allocator());

        StreamWriter st(&buf);
        if (!qr_EncodeTextToPng(url, 0, &st))
            return;
        if (!st.Close())
            return;

        png = buf.Leak();
    }

    io->AddHeader("X-TOTP-SecretKey", session->secret);
    io->AddCachingHeaders(0, nullptr);

    io->SendAsset(200, png, "image/png");
}

void HandleChangeTOTP(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<SessionInfo> session = sessions.Find(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    std::lock_guard<std::shared_mutex> lock_excl(session->mutex);

    if (session->type != SessionType::Login) {
        LogError("This account does not use passwords");
        io->SendError(403);
        return;
    }
    if (session->confirm != SessionConfirm::None) {
        LogError("You must be fully logged in before you do that");
        io->SendError(403);
        return;
    }

    const char *password = nullptr;
    const char *code = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "password") {
                parser.ParseString(&password);
            } else if (key == "code") {
                parser.ParseString(&code);
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

    // Check for missing values
    {
        bool valid = true;

        if (!password) {
            LogError("Missing 'password' parameter");
            valid = false;
        }
        if (!code) {
            LogError("Missing 'code' parameter");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    // We use this to extend/fix the response delay in case of error
    int64_t now = GetMonotonicTime();

    // Authenticate with password
    {
        sq_Statement stmt;
        if (!gp_domain.db.Prepare(R"(SELECT password_hash FROM dom_users
                                     WHERE userid = ?1)", &stmt))
            return;
        sqlite3_bind_int64(stmt, 1, session->userid);

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("User does not exist");
                io->SendError(404);
            }
            return;
        }

        const char *password_hash = (const char *)sqlite3_column_text(stmt, 0);

        if (!password_hash || crypto_pwhash_str_verify(password_hash, password, strlen(password)) < 0) {
            // Enforce constant delay if authentification fails
            int64_t safety_delay = std::max(2000 - GetMonotonicTime() + now, (int64_t)0);
            WaitDelay(safety_delay);

            LogError("Invalid password");
            io->SendError(403);
            return;
        }
    }

    // Check user knows secret
    if (!CheckTotp(io, *session, nullptr, code))
        return;

    bool success = gp_domain.db.Transaction([&]() {
        int64_t time = GetUnixTime();

        if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username)
                                 VALUES (?1, ?2, ?3, ?4))",
                              time, request.client_addr, "change_totp", session->username))
            return false;
        if (!gp_domain.db.Run("UPDATE dom_users SET confirm = 'TOTP', secret = ?2 WHERE userid = ?1",
                              session->userid, session->secret))
            return false;

        return true;
    });
    if (!success)
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleChangeMode(http_IO *io, InstanceHolder *instance)
{
    if (instance->master != instance) {
        LogError("Cannot change mode through slave instance");
        io->SendError(403);
        return;
    }

    RetainPtr<const SessionInfo> session = sessions.Find(io);
    SessionStamp *stamp = session ? session->GetStamp(instance) : nullptr;

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (session->type != SessionType::Login) {
        LogError("This account does not have a profile");
        io->SendError(403);
        return;
    }

    bool develop = stamp->develop;

    // Read changes
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "develop") {
                parser.SkipNull() || parser.ParseBool(&develop);
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

    // Check permissions
    if (develop && !stamp->HasPermission(UserPermission::BuildCode)) {
        LogError("User is not allowed to code");
        io->SendError(403);
        return;
    }

    stamp->develop = develop;

    io->SendText(200, "{}", "application/json");
}

void HandleChangeExportKey(http_IO *io, InstanceHolder *instance)
{
    RetainPtr<const SessionInfo> session = sessions.Find(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->HasPermission(instance, UserPermission::DataExport)) {
        LogError("User is not allowed to export data");
        io->SendError(403);
        return;
    }

    Span<char> key_buf = AllocateSpan<char>(io->Allocator(), 45);

    // Generate export key
    {
        uint8_t buf[32];
        FillRandomSafe(buf);
        sodium_bin2base64(key_buf.ptr, (size_t)key_buf.len, buf, RG_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
    }

    if (!gp_domain.db.Run(R"(UPDATE dom_permissions SET export_key = ?3
                             WHERE userid = ?1 AND instance = ?2)",
                          session->userid, instance->master->key, key_buf.ptr))
        return;

    // Export data
    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.String(key_buf.ptr);
    json.Finish();
}

RetainPtr<const SessionInfo> MigrateGuestSession(http_IO *io, InstanceHolder *instance, const char *username)
{
    // Create random username (if needed)
    if (!username || !username[0]) {
        Span<char> key = AllocateSpan<char>(io->Allocator(), 11);

        for (Size i = 0; i < 7; i++) {
            key[i] = (char)('a' + randombytes_uniform('z' - 'a' + 1));
        }
        for (Size i = 7; i < 10; i++) {
            key[i] = (char)('0' + randombytes_uniform('9' - '0' + 1));
        }
        key[10] = 0;

        username = key.ptr;
    }

    // Create random local key
    char local_key[45];
    {
        uint8_t buf[32];
        FillRandomSafe(buf);
        sodium_bin2base64((char *)local_key, 45, buf, RG_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
    }

    int64_t userid;
    bool success = instance->db->Transaction([&]() {
        sq_Statement stmt;
        if (!instance->db->Prepare(R"(INSERT INTO ins_users (key, local_key)
                                      VALUES (?1, ?2)
                                      RETURNING userid)",
                                   &stmt, username, local_key))
            return false;
        if (!stmt.GetSingleValue(&userid))
            return false;

        return true;
    });
    if (!success)
        return nullptr;

    RG_ASSERT(userid > 0);
    userid = -userid;

    RetainPtr<SessionInfo> session = CreateUserSession(SessionType::Auto, userid, username, local_key);

    uint32_t permissions = (int)UserPermission::DataNew | (int)UserPermission::DataEdit;
    session->AuthorizeInstance(instance, permissions);

    sessions.Open(io, session);

    return session;
}

void HandleAuthChallenge(http_IO *io, InstanceHolder *instance)
{
    int64_t now = GetUnixTime();

    LocalArray<uint8_t, 1024> cypher;
    char base64[2048];
    static_assert(RG_SIZE(cypher) >= crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES);

    randombytes_buf(cypher.data, crypto_secretbox_NONCEBYTES);
    crypto_secretbox_easy(cypher.data + crypto_secretbox_NONCEBYTES, (const uint8_t *)&now, RG_SIZE(now), cypher.data, instance->challenge_key);
    cypher.len = crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + RG_SIZE(now);

    sodium_bin2base64(base64, RG_SIZE(base64), cypher.data, cypher.len, sodium_base64_VARIANT_ORIGINAL);

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartObject();
    json.Key("challenge"); json.String(base64);
    json.EndObject();

    json.Finish();
}

void HandleAuthRegister(http_IO *io, InstanceHolder *instance)
{
    RetainPtr<const SessionInfo> session = sessions.Find(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    int64_t now = GetUnixTime();

    const char *challenge_key = nullptr;
    const char *credential_id = nullptr;
    const char *public_key = nullptr;
    int algorithm = 0;
    const char *attestation = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "challenge") {
                parser.ParseString(&challenge_key);
            } else if (key == "credential_id") {
                parser.ParseString(&credential_id);
            } else if (key == "public_key") {
                parser.ParseString(&public_key);
            } else if (key == "algorithm") {
                parser.ParseInt(&algorithm);
            } else if (key == "attestation") {
                parser.ParseString(&attestation);
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

    // Check for missing values
    if (!challenge_key) {
        LogError("Missing 'challenge' parameter");
        io->SendError(422);
        return;
    }
    if (!credential_id) {
        LogError("Missing 'credential_id' parameter");
        io->SendError(422);
        return;
    }
    if (!public_key) {
        LogError("Missing 'public_key' parameter");
        io->SendError(422);
        return;
    }
    if (!algorithm) {
        LogError("Missing 'algorithm' parameter");
        io->SendError(422);
        return;
    }
    if (!attestation) {
        LogError("Missing 'attestation' parameter");
        io->SendError(422);
        return;
    }

    // Check challenge key
    {
        LocalArray<uint8_t, 1024> cypher;

        size_t bin_len;
        if (sodium_base642bin(cypher.data, RG_SIZE(cypher.data), challenge_key, strlen(challenge_key),
                              nullptr, &bin_len, nullptr, sodium_base64_VARIANT_ORIGINAL) < 0) {
            LogError("Failed to decode base64 challenge");
            io->SendError(422);
            return;
        }
        cypher.len = (Size)bin_len;

        if (cypher.len != crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + RG_SIZE(int64_t)) {
            LogError("Malformed challenge key");
            io->SendError(422);
            return;
        }

        int64_t time = -1;
        if (crypto_secretbox_open_easy((uint8_t *)&time, cypher.data + crypto_secretbox_NONCEBYTES,
                                                         cypher.len - crypto_secretbox_NONCEBYTES,
                                       cypher.data, instance->challenge_key) < 0) {
            LogError("Invalid challenge key");
            io->SendError(403);
            return;
        }

        if (time < now - MaxChallengeDelay || time > now) {
            LogError("Outdated challenge key");
            io->SendError(403);
            return;
        }
    }

    // Update WebAuthn credentials
    if (!instance->db->Run(R"(INSERT INTO ins_devices (credential_id, public_key, algorithm, userid)
                              VALUES (?1, ?2, ?3, ?4))",
                           credential_id, public_key, algorithm, session->userid))
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleAuthAssert(http_IO *io, InstanceHolder *instance)
{
    int64_t now = GetUnixTime();

    const char *challenge_key = nullptr;
    const char *credential_id = nullptr;
    const char *signature = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "challenge") {
                parser.ParseString(&challenge_key);
            } else if (key == "credential_id") {
                parser.ParseString(&credential_id);
            } else if (key == "signature") {
                parser.ParseString(&signature);
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

    // Check for missing values
    if (!challenge_key) {
        LogError("Missing 'challenge' parameter");
        io->SendError(422);
        return;
    }
    if (!credential_id) {
        LogError("Missing 'credential_id' parameter");
        io->SendError(422);
        return;
    }
    if (!signature) {
        LogError("Missing 'public_key' parameter");
        io->SendError(422);
        return;
    }

    // Check challenge key
    {
        LocalArray<uint8_t, 1024> cypher;

        size_t bin_len;
        if (sodium_base642bin(cypher.data, RG_SIZE(cypher.data), challenge_key, strlen(challenge_key),
                              nullptr, &bin_len, nullptr, sodium_base64_VARIANT_ORIGINAL) < 0) {
            LogError("Failed to decode base64 challenge");
            io->SendError(422);
            return;
        }
        cypher.len = (Size)bin_len;

        if (cypher.len != crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + RG_SIZE(int64_t)) {
            LogError("Malformed challenge key");
            io->SendError(422);
            return;
        }

        int64_t time = -1;
        if (crypto_secretbox_open_easy((uint8_t *)&time, cypher.data + crypto_secretbox_NONCEBYTES,
                                                         cypher.len - crypto_secretbox_NONCEBYTES,
                                       cypher.data, instance->challenge_key) < 0) {
            LogError("Invalid challenge key");
            io->SendError(403);
            return;
        }

        if (time < now - MaxChallengeDelay || time > now) {
            LogError("Outdated challenge key");
            io->SendError(403);
            return;
        }
    }

    // Create session
    RetainPtr<SessionInfo> session;
    {
        sq_Statement stmt;
        if (!instance->db->Prepare("SELECT userid FROM ins_devices WHERE credential_id = ?1", &stmt, credential_id))
            return;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Invalid credentials");
                io->SendError(403);
            }
            return;
        }

        int64_t userid = sqlite3_column_int64(stmt, 0);

        if (userid > 0) {
            session = LoginUserAuto(io, userid);
            if (!session)
                return;
        } else {
            stmt.Finalize();
            if (!instance->db->Prepare("SELECT key, local_key FROM ins_users WHERE userid = ?1", &stmt, -userid))
                return;

            if (!stmt.Step()) {
                if (stmt.IsValid()) {
                    LogError("Invalid credentials");
                    io->SendError(403);
                }
                return;
            }

            const char *username = (const char *)sqlite3_column_text(stmt, 0);
            const char *local_key = (const char *)sqlite3_column_text(stmt, 1);

            session = CreateUserSession(SessionType::Token, userid, username, local_key);

            uint32_t permissions = (int)UserPermission::DataNew | (int)UserPermission::DataEdit;
            session->AuthorizeInstance(instance, permissions);

            sessions.Open(io, session);
        }
    }

    WriteProfileJson(io, session.GetRaw(), instance);
}

}
