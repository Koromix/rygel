// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "config.hh"
#include "domain.hh"
#include "goupile.hh"
#include "instance.hh"
#include "message.hh"
#include "user.hh"
#include "lib/native/http/http.hh"
#include "lib/native/password/otp.hh"
#include "lib/native/password/password.hh"
#include "lib/native/wrap/qrcode.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace K {

static_assert(PasswordHashBytes == crypto_pwhash_STRBYTES);

static const int BanThreshold = 6;
static const int64_t BanTime = 1800 * 1000;
static const int64_t TotpPeriod = 30000;

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

    K_HASHTABLE_HANDLER(EventInfo, key);
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

            if (instance->demo) {
                stamp->authorized = true;
                stamp->permissions = (int)UserPermission::BuildCode |
                                     (int)UserPermission::BuildPublish |
                                     (int)UserPermission::DataRead |
                                     (int)UserPermission::DataSave |
                                     (int)UserPermission::DataDelete |
                                     (int)UserPermission::ExportCreate |
                                     (int)UserPermission::ExportDownload;
            } else if (userid > 0) {
                uint32_t permissions;
                {
                    sq_Statement stmt;
                    if (!gp_db.Prepare(R"(SELECT permissions FROM dom_permissions
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
                    if (!gp_db.Prepare(R"(SELECT permissions FROM dom_permissions
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
            } else if (instance->settings.allow_guests) {
                stamp->authorized = true;
                stamp->permissions = (int)UserPermission::DataSave;
                stamp->single = true;
            }
        }
    }

    return stamp->authorized ? stamp : nullptr;
}

void SessionInfo::InvalidateStamps()
{
    if (is_admin && !is_root) {
        sq_Statement stmt;
        if (!gp_db.Prepare(R"(SELECT IIF(a.permissions IS NOT NULL, 1, 0) AS admin
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

void SessionInfo::AuthorizeInstance(const InstanceHolder *instance, uint32_t permissions,
                                    bool single, const char *lock)
{
    std::lock_guard<std::shared_mutex> lock_excl(mutex);

    SessionStamp *stamp = stamps.AppendDefault();

    stamp->unique = instance->unique;
    stamp->authorized = true;
    stamp->permissions = permissions;
    stamp->single = single;
    stamp->lock = lock ? DuplicateString(lock, &stamps_alloc).ptr : nullptr;

    stamps_map.Set(stamp);
}

void ExportProfile(const SessionInfo *session, const InstanceHolder *instance, json_Writer *json)
{
    char buf[512];

    json->StartObject();
    if (session) {
        json->Key("userid"); json->Int64(session->userid);
        json->Key("username"); json->String(session->username);
        json->Key("online"); json->Bool(true);

        // Atomic load
        SessionConfirm confirm = session->confirm;

        if (session->change_password) {
            json->Key("authorized"); json->Bool(false);
            json->Key("confirm"); json->String("password");
        } else if (confirm != SessionConfirm::None) {
            json->Key("authorized"); json->Bool(false);

            switch (confirm) {
                case SessionConfirm::None: { K_UNREACHABLE(); } break;
                case SessionConfirm::Mail: { json->Key("confirm"); json->String("mail"); } break;
                case SessionConfirm::SMS: { json->Key("confirm"); json->String("sms"); } break;
                case SessionConfirm::TOTP: { json->Key("confirm"); json->String("totp"); } break;
                case SessionConfirm::QRcode: { json->Key("confirm"); json->String("qrcode"); } break;
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
                json->Key("instance"); json->String(instance->key.ptr);
                json->Key("authorized"); json->Bool(true);

                switch (session->type) {
                    case SessionType::Login: { json->Key("type"); json->String("login"); } break;
                    case SessionType::Token: { json->Key("type"); json->String("token"); } break;
                    case SessionType::Auto: { json->Key("type"); json->String("auto"); } break;
                }

                if (!instance->slaves.len) {
                    json->Key("namespaces"); json->StartObject();
                    if (instance->settings.shared_key) {
                        json->Key("records"); json->String("global");
                    } else {
                        json->Key("records"); json->Int64(session->userid);
                    }
                    json->EndObject();
                    json->Key("keys"); json->StartObject();
                    if (instance->settings.shared_key) {
                        json->Key("records"); json->String(instance->settings.shared_key);
                    } else if (session->local_key[0]) {
                        json->Key("records"); json->String(session->local_key);
                    }
                    if (session->type == SessionType::Login) {
                        json->Key("lock"); json->String(instance->settings.lock_key);
                    }
                    json->EndObject();
                }

                if (master->slaves.len) {
                    json->Key("instances"); json->StartArray();
                    for (const InstanceHolder *slave: master->slaves) {
                        if (session->GetStamp(slave)) {
                            json->StartObject();
                            json->Key("key"); json->String(slave->key.ptr);
                            json->Key("title"); json->String(slave->title);
                            json->Key("name"); json->String(slave->settings.name);
                            json->Key("url"); json->String(Fmt(buf, "/%1/", slave->key).ptr);
                            json->EndObject();
                        }
                    }
                    json->EndArray();
                }

                json->Key("permissions"); json->StartObject();
                for (Size i = 0; i < K_LEN(UserPermissionNames); i++) {
                    Span<const char> key = json_ConvertToJsonName(UserPermissionNames[i], buf);
                    json->Key(key.ptr, (size_t)key.len); json->Bool(stamp->permissions & (1 << i));
                }
                json->EndObject();

                json->Key("single"); json->Bool(stamp->single);
                if (stamp->lock) {
                    json->Key("lock"); json->Raw(stamp->lock);
                }

                if (stamp->HasPermission(UserPermission::BuildCode)) {
                    const SessionStamp *stamp = session->GetStamp(master);
                    json->Key("develop"); json->Bool(stamp && stamp->develop.load(std::memory_order_relaxed));
                } else {
                    K_ASSERT(!stamp->develop.load());
                }

                json->Key("root"); json->Bool(session->is_root);
            } else {
                json->Key("authorized"); json->Bool(false);
            }
        } else {
            bool authorized = (session->is_admin && session->admin_until > GetMonotonicTime());

            json->Key("authorized"); json->Bool(authorized);
            json->Key("root"); json->Bool(session->is_root);
        }
    }
    json->EndObject();
}

Span<const char> ExportProfile(const SessionInfo *session, const InstanceHolder *instance, Allocator *alloc)
{
    HeapArray<char> buf(alloc);
    StreamWriter st(&buf, "<profile>");
    json_Writer json(&st);

    ExportProfile(session, instance, &json);
    K_ASSERT(st.Close());

    buf.Grow(1);
    buf.ptr[buf.len] = 0;

    return buf.TrimAndLeak(1);
}

static void SendProfile(http_IO *io, const SessionInfo *session, const InstanceHolder *instance)
{
    http_SendJson(io, 200, [&](json_Writer *json) {
        ExportProfile(session, instance, json);
    });
}

static RetainPtr<SessionInfo> CreateUserSession(SessionType type, int64_t userid,
                                                const char *username, const char *local_key)
{
    Size username_bytes = strlen(username) + 1;
    Size session_bytes = K_SIZE(SessionInfo) + username_bytes;

    SessionInfo *session = (SessionInfo *)AllocateRaw(nullptr, session_bytes, (int)AllocFlag::Zero);

    new (session) SessionInfo;
    RetainPtr<SessionInfo> ptr(session, [](SessionInfo *session) {
        session->~SessionInfo();
        ReleaseRaw(nullptr, session, -1);
    });

    session->type = type;
    session->userid = userid;
    if (local_key) {
        CopyString(local_key, session->local_key);
    }
    CopyString(username, MakeSpan((char *)session->username, username_bytes));

    return ptr;
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

    if (!session && instance) {
        if (instance->demo) {
            int64_t userid = instance->unique + 1;

            session = CreateUserSession(SessionType::Auto, userid, instance->key.ptr, nullptr);
            sessions.Open(io, session);
        } else if (instance->settings.allow_guests) {
            // Create local key
            char local_key[45];
            {
                uint8_t buf[32];
                FillRandomSafe(buf);
                sodium_bin2base64(local_key, K_SIZE(local_key), buf, K_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
            }

            session = CreateUserSession(SessionType::Auto, 0, "Guest", local_key);
        }
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

bool CheckPasswordComplexity(const char *password, const char *username, PasswordComplexity treshold)
{
    unsigned int flags = 0;

    switch (treshold) {
        case PasswordComplexity::Easy: { flags = UINT_MAX & ~(int)pwd_CheckFlag::Classes & ~(int)pwd_CheckFlag::Score; } break;
        case PasswordComplexity::Moderate: { flags = UINT_MAX & ~(int)pwd_CheckFlag::Score; } break;
        case PasswordComplexity::Hard: { flags = UINT_MAX; } break;
    }
    K_ASSERT(flags);

    Span<const char *> blacklist = MakeSpan(&username, username ? 1 : 0);
    return pwd_CheckPassword(password, blacklist, flags);
}

static bool CheckPasswordComplexity(const SessionInfo &session, const char* password)
{
    DomainHolder *domain = RefDomain();
    K_DEFER { UnrefDomain(domain); };

    PasswordComplexity treshold;

    if (session.is_root) {
        treshold = domain->settings.root_password;
    } else if (session.is_admin) {
        treshold = domain->settings.admin_password;
    } else {
        treshold = domain->settings.user_password;
    }

    return CheckPasswordComplexity(password, session.username, treshold);
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

void HandleSessionLogin(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();

    const char *username = nullptr;
    Span<const char> password = {};
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "username") {
                    json->ParseString(&username);
                } else if (key == "password") {
                    json->ParseString(&password);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!username || !password.ptr) {
                    LogError("Missing 'username' or 'password' parameter");
                    valid = false;
                }
                if (password.len > pwd_MaxLength) {
                    LogError("Excessive password length");
                    valid = false;
                }
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
    }

    // We use this to extend/fix the response delay in case of error
    int64_t now = GetMonotonicTime();

    sq_Statement stmt;
    if (instance) {
        InstanceHolder *master = instance->master;

        if (!instance->slaves.len) {
            if (!gp_db.Prepare(R"(SELECT u.userid, u.password_hash, u.change_password,
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

            if (!gp_db.Prepare(R"(SELECT u.userid, u.password_hash, u.change_password,
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
        if (!gp_db.Prepare(R"(SELECT u.userid, u.password_hash, u.change_password,
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

            if (!gp_db.Run(R"(INSERT INTO adm_events (time, address, type, username)
                              VALUES (?1, ?2, ?3, ?4))",
                           time, request.client_addr, "login", username))
                return;

            RetainPtr<SessionInfo> session;
            if (!confirm) {
                session = CreateUserSession(SessionType::Login, userid, username, local_key);
            } else if (TestStr(confirm, "TOTP")) {
                if (secret) {
                    if (strlen(secret) >= K_SIZE(SessionInfo::secret)) {
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

                if (!change_password && (root || admin)) {
                    change_password = !CheckPasswordComplexity(*session, password.ptr);
                }
                session->change_password = change_password;

                sessions.Open(io, session);
                SendProfile(io, session.GetRaw(), instance);
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
                                                const char *username, const char *email, const char *sms,
                                                bool single, const char *lock)
{
    K_ASSERT(!email || !sms);

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
            sodium_bin2base64((char *)local_key, 45, buf, K_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
        }

        if (!instance->db->Prepare(R"(INSERT INTO ins_users (key, local_key)
                                      VALUES (?1, ?2)
                                      ON CONFLICT DO NOTHING
                                      RETURNING userid, local_key)",
                                   &stmt, key, local_key))
            return nullptr;

        if (!stmt.Step()) {
            K_ASSERT(!stmt.IsValid());
            return nullptr;
        }
    }

    userid = sqlite3_column_int64(stmt, 0);
    local_key = (const char *)sqlite3_column_text(stmt, 1);

    K_ASSERT(userid > 0);
    userid = -userid;

    RetainPtr<SessionInfo> session;

    if (email) {
        char code[9];
        {
            static_assert(K_SIZE(code) <= K_SIZE(SessionInfo::secret));

            uint32_t rnd = randombytes_uniform(100000000); // 8 digits
            Fmt(code, "%1", FmtInt(rnd, 8));
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

        SendMail(email, content);
    } else if (sms) {
        if (gp_config.sms.provider == sms_Provider::None) [[unlikely]] {
            LogError("This domain is not configured to send SMS messages");
            return nullptr;
        }

        char code[9];
        {
            static_assert(K_SIZE(code) <= K_SIZE(SessionInfo::secret));

            uint32_t rnd = randombytes_uniform(1000000); // 6 digits
            Fmt(code, "%1", FmtInt(rnd, 6));
        }

        session = CreateUserSession(type, userid, username, local_key);
        session->confirm = SessionConfirm::SMS;
        CopyString(code, session->secret);

        const char *message = Fmt(&temp_alloc, "Code: %1", code).ptr;

        SendSMS(sms, message);
    } else {
        session = CreateUserSession(type, userid, username, local_key);
    }

    uint32_t permissions = (int)UserPermission::DataSave;
    session->AuthorizeInstance(instance, permissions, single, lock);

    return session;
}

void HandleSessionToken(http_IO *io, InstanceHolder *instance)
{
    const http_RequestInfo &request = io->Request();

    if (!instance->settings.token_key) {
        LogError("This instance does not use tokens");
        io->SendError(403);
        return;
    }

    Span<const char> token = {};
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "token") {
                    json->ParseString(&token);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!token.len) {
                    LogError("Missing 'token' parameter");
                    valid = false;
                }
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
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
                                 instance->settings.token_pkey, instance->settings.token_skey) != 0) {
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
    bool many = instance->legacy;
    const char *lock = nullptr;
    {
        StreamReader st(json, "<token>");
        json_Parser json(&st, io->Allocator());

        for (json.ParseObject(); json.InObject(); ) {
            Span<const char> key = json.ParseKey();

            if (key == "key" || key == "id") {
                json.ParseString(&id);
            } else if (key == "email") {
                json.ParseString(&email);
            } else if (key == "sms") {
                json.ParseString(&sms);
            } else if (key == "username") {
                json.ParseString(&username);
            } else if (key == "claims") {
                for (json.ParseArray(); json.InArray(); ) {
                    const char *claim = json.ParseString().ptr;
                    claims.Append(claim);
                }
            } else if (!instance->legacy && key == "many") {
                json.ParseBool(&many);
            } else if (instance->legacy && key == "lock") {
                if (instance->legacy) {
                    json.ParseString(&lock);
                } else {
                    json.PassThrough(&lock);
                }
            } else if (json.IsValid()) {
                json.UnexpectedKey(key);
                io->SendError(422);
                return;
            }
        }
        if (!json.IsValid()) {
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
        if (email && !IsMailValid(email)) {
            LogError("Empty or invalid email address");
            valid = false;
        }
        if (sms && !IsPhoneValid(sms)) {
            LogError("Empty or invalid SMS phone number");
            valid = false;
        }
        for (const char *claim: claims) {
            if (!claim || !claim[0]) {
                LogError("Missing or invalid claim");
                valid = false;
            }
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

    RetainPtr<SessionInfo> session = CreateAutoSession(instance, SessionType::Token, id, username,
                                                       email, sms, !many, lock);
    if (!session)
        return;

    if (claims.len) {
        bool success = instance->db->Transaction([&]() {
            K_ASSERT(session->userid < 0);

            for (const char *claim: claims) {
                if (!instance->legacy) {
                    if (!instance->db->Run(R"(INSERT INTO ins_claims (userid, tid) VALUES (?1, ?2)
                                              ON CONFLICT DO NOTHING)", -session->userid, claim))
                        return false;
                } else {
                    if (!instance->db->Run(R"(INSERT INTO ins_claims (userid, ulid) VALUES (?1, ?2)
                                              ON CONFLICT DO NOTHING)", -session->userid, claim))
                        return false;
                }
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

static bool CheckTotp(http_IO *io, const SessionInfo &session, InstanceHolder *instance, const char *code)
{
    int64_t time = GetUnixTime();
    int64_t counter = time / TotpPeriod;
    int64_t min = counter - 1;
    int64_t max = counter + 1;

    if (pwd_CheckHotp(session.secret, pwd_HotpAlgorithm::SHA1, min, max, 6, code)) {
        K_ASSERT(session.userid > 0 || instance);

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
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "code") {
                    json->ParseString(&code);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!code) {
                    LogError("Missing 'code' parameter");
                    valid = false;
                }
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
    }

    if (CountEvents(request.client_addr, session->username) >= BanThreshold) {
        LogError("You are blocked for %1 minutes after excessive login failures", (BanTime + 59000) / 60000);
        io->SendError(403);
        return;
    }

    // Immediate confirmation looks weird
    WaitDelay(800);

    switch (session->confirm) {
        case SessionConfirm::None: { K_UNREACHABLE(); } break;

        case SessionConfirm::Mail:
        case SessionConfirm::SMS: {
            if (TestStr(code, session->secret)) {
                session->confirm = SessionConfirm::None;
                sodium_memzero(session->secret, K_SIZE(session->secret));

                SendProfile(io, session.GetRaw(), instance);
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
                    if (!gp_db.Run("UPDATE dom_users SET secret = ?2 WHERE userid = ?1",
                                   session->userid, session->secret))
                        return;
                }

                session->confirm = SessionConfirm::None;
                sodium_memzero(session->secret, K_SIZE(session->secret));

                lock_excl.unlock();
                SendProfile(io, session.GetRaw(), instance);
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
    SendProfile(io, session.GetRaw(), instance);
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
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "old_password") {
                    json->SkipNull() || json->ParseString(&old_password);
                } else if (key == "new_password") {
                    json->ParseString(&new_password);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!old_password && !session->change_password) {
                    LogError("Missing 'old_password' parameter");
                    valid = false;
                }
                if (!new_password) {
                    LogError("Missing 'new_password' parameter");
                    valid = false;
                }
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
    }

    // Complex enough?
    if (!CheckPasswordComplexity(*session, new_password)) {
        io->SendError(422);
        return;
    }

    // Authenticate with old password
    {
        K_ASSERT(old_password || session->change_password);

        // We use this to extend/fix the response delay in case of error
        int64_t now = GetMonotonicTime();

        sq_Statement stmt;
        if (!gp_db.Prepare("SELECT password_hash FROM dom_users WHERE userid = ?1", &stmt))
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

    bool success = gp_db.Transaction([&]() {
        int64_t time = GetUnixTime();

        if (!gp_db.Run(R"(INSERT INTO adm_events (time, address, type, username)
                          VALUES (?1, ?2, ?3, ?4))",
                       time, request.client_addr, "change_password", session->username))
            return false;
        if (!gp_db.Run("UPDATE dom_users SET password_hash = ?2, change_password = 0 WHERE userid = ?1",
                       session->userid, new_hash))
            return false;

        return true;
    });
    if (!success)
        return;

    if (session->change_password) {
        session->change_password = false;

        lock_excl.unlock();
        SendProfile(io, session.GetRaw(), instance);
    } else {
        io->SendText(200, "{}", "application/json");
    }
}

// This does not make any persistent change and it needs to return an image
// so it is a GET even though it performs an action (change the secret).
void HandleChangeQRcode(http_IO *io, const char *title)
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

    const char *url = pwd_GenerateHotpUrl(title, session->username, title,
                                          pwd_HotpAlgorithm::SHA1, session->secret, 6, io->Allocator());
    if (!url)
        return;

    Span<const uint8_t> png;
    {
        HeapArray<uint8_t> buf(io->Allocator());

        StreamWriter st(&buf, "<png>");
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
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "password") {
                    json->ParseString(&password);
                } else if (key == "code") {
                    json->ParseString(&code);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!password) {
                    LogError("Missing 'password' parameter");
                    valid = false;
                }
                if (!code) {
                    LogError("Missing 'code' parameter");
                    valid = false;
                }
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
            return;
        }
    }

    // We use this to extend/fix the response delay in case of error
    int64_t now = GetMonotonicTime();

    // Authenticate with password
    {
        sq_Statement stmt;
        if (!gp_db.Prepare("SELECT password_hash FROM dom_users WHERE userid = ?1", &stmt))
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

    bool success = gp_db.Transaction([&]() {
        int64_t time = GetUnixTime();

        if (!gp_db.Run(R"(INSERT INTO adm_events (time, address, type, username)
                          VALUES (?1, ?2, ?3, ?4))",
                       time, request.client_addr, "change_totp", session->username))
            return false;
        if (!gp_db.Run("UPDATE dom_users SET confirm = 'TOTP', secret = ?2 WHERE userid = ?1",
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
    if (!stamp || session->userid <= 0) {
        LogError("This account does not have a profile");
        io->SendError(403);
        return;
    }

    bool develop = stamp->develop;

    // Read changes
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "develop") {
                    json->SkipNull() || json->ParseBool(&develop);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            return valid;
        });

        if (!success) {
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
    SessionStamp *stamp = session ? session->GetStamp(instance) : nullptr;

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!stamp || !(stamp->permissions & ((int)UserPermission::ExportCreate | (int)UserPermission::ExportDownload))) {
        LogError("User is not allowed to export data");
        io->SendError(403);
        return;
    }

    Span<char> key_buf = AllocateSpan<char>(io->Allocator(), 45);

    // Generate export key
    {
        uint8_t buf[32];
        FillRandomSafe(buf);
        sodium_bin2base64(key_buf.ptr, (size_t)key_buf.len, buf, K_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
    }

    if (!gp_db.Run(R"(UPDATE dom_permissions SET export_key = ?3
                      WHERE userid = ?1 AND instance = ?2)",
                   session->userid, instance->master->key, key_buf.ptr))
        return;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->String(key_buf.ptr);
    });
}

int64_t CreateInstanceUser(InstanceHolder *instance, const char *username)
{
    K_ASSERT(username);

    // Create random local key
    char local_key[45];
    {
        uint8_t buf[32];
        FillRandomSafe(buf);
        sodium_bin2base64((char *)local_key, 45, buf, K_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
    }

    int64_t userid;
    bool success = instance->db->Transaction([&]() {
        sq_Statement stmt;
        if (!instance->db->Prepare(R"(INSERT INTO ins_users (key, local_key)
                                      VALUES (?1, ?2)
                                      ON CONFLICT (key) DO UPDATE SET key = key
                                      RETURNING userid)",
                                   &stmt, username, local_key))
            return false;
        if (!stmt.GetSingleValue(&userid))
            return false;

        return true;
    });
    if (!success)
        return 0;

    K_ASSERT(userid > 0);
    userid = -userid;

    return userid;
}

RetainPtr<const SessionInfo> MigrateGuestSession(http_IO *io, InstanceHolder *instance, const char *username)
{
    // Create random username (if needed)
    if (!username) {
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
        sodium_bin2base64((char *)local_key, 45, buf, K_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
    }

    int64_t userid = CreateInstanceUser(instance, username);
    bool success = instance->db->Transaction([&]() {
        sq_Statement stmt;
        if (!instance->db->Prepare(R"(INSERT INTO ins_users (key, local_key)
                                      VALUES (?1, ?2)
                                      ON CONFLICT (key) DO UPDATE SET key = key
                                      RETURNING userid)",
                                   &stmt, username, local_key))
            return false;
        if (!stmt.GetSingleValue(&userid))
            return false;

        return true;
    });
    if (!success)
        return nullptr;

    K_ASSERT(userid > 0);
    userid = -userid;

    RetainPtr<SessionInfo> session = CreateUserSession(SessionType::Auto, userid, username, local_key);

    uint32_t permissions = (int)UserPermission::DataSave;
    session->AuthorizeInstance(instance, permissions, true);

    sessions.Open(io, session);

    return session;
}

}
