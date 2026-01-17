// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "lib/native/sqlite/sqlite.hh"
#include "rokkerd.hh"
#include "mail.hh"
#include "user.hh"
#include "lib/native/http/http.hh"
#include "lib/native/password/otp.hh"
#include "lib/native/password/password.hh"
#include "lib/native/sso/oidc.hh"
#include "lib/native/wrap/qrcode.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

#if defined(_WIN32)
    #include <io.h>
#endif

namespace K {

static const int PasswordHashBytes = 128;

static const int64_t TotpPeriod = 30000;

static const int64_t TokenDuration = 1800 * 1000;
static const int64_t InvalidTimeout = 86400 * 1000;
static const int BanThreshold = 6;
static const int64_t BanTime = 15 * 60000;

static const int64_t PictureCacheDelay = 3600 * 1000;
static const Size MaxPictureSize = Kibibytes(256);

static const int SsoCookieFlags =  (int)http_CookieFlag::SameSiteStrict | (int)http_CookieFlag::Secure | (int)http_CookieFlag::HttpOnly;
static const int SsoCookieMaxAge = 10 * 60000; // 10 minutes

struct EventKey {
    const char *resource;
    const char *who;

    bool operator==(const EventKey &other) const { return TestStr(resource, other.resource) && TestStr(who, other.who); }
    bool operator!=(const EventKey &other) const { return !(*this == other); }

    uint64_t Hash() const
    {
        uint64_t hash = HashTraits<const char *>::Hash(resource) ^
                        HashTraits<const char *>::Hash(who);
        return hash;
    }
};

struct EventInfo {
    EventKey key;
    int64_t until; // Monotonic

    int count;
    int64_t prev_time; // Unix time
    int64_t time; // Unix time

    K_HASHTABLE_HANDLER(EventInfo, key);
};

static http_SessionManager<SessionInfo> sessions;

static std::shared_mutex events_mutex;
static BucketArray<EventInfo> events;
static HashTable<EventKey, EventInfo *> events_map;

static bool IsMailValid(const char *mail)
{
    const auto test_char = [](char c) { return strchr("<>& ", c) || IsAsciiControl(c); };

    Span<const char> domain;
    Span<const char> prefix = SplitStr(mail, '@', &domain);

    if (!prefix.len || !domain.len)
        return false;
    if (std::any_of(prefix.begin(), prefix.end(), test_char))
        return false;
    if (std::any_of(domain.begin(), domain.end(), test_char))
        return false;

    return true;
}

static const char *FormatUUID(const uint8_t raw[16], Allocator *alloc)
{
    const char *uuid = Fmt(alloc, "%1-%2-%3-%4-%5",
                           FmtHex(MakeSpan(raw + 0, 4)),
                           FmtHex(MakeSpan(raw + 4, 2)),
                           FmtHex(MakeSpan(raw + 6, 2)),
                           FmtHex(MakeSpan(raw + 8, 2)),
                           FmtHex(MakeSpan(raw + 10, 6))).ptr;
    return uuid;
}

static bool SendNewUserMail(const char *to, const uint8_t token[16], Allocator *alloc)
{
    smtp_MailContent content;

    const char *uuid = FormatUUID(token, alloc);
    const char *url = Fmt(alloc, "%1/finalize#token=%2", config.url, uuid).ptr;

    const auto patch = [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "TITLE") {
            writer->Write(config.title);
        } else if (key == "MAIL") {
            writer->Write(to);
        } else if (key == "URL") {
            writer->Write(url);
        } else {
            Print(writer, "{{%1}}", expr);
        }
    };

    content.subject = Fmt(alloc, T("Welcome to %1"), config.title);
    content.html = PatchMail("new_user.html", alloc, patch);
    content.text = PatchMail("new_user.txt", alloc, patch);

    return PostMail(to, content);
}

static bool SendExistingUserMail(const char *to, Allocator *alloc)
{
    smtp_MailContent content;

    const auto patch = [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "TITLE") {
            writer->Write(config.title);
        } else if (key == "MAIL") {
            writer->Write(to);
        } else {
            Print(writer, "{{%1}}", expr);
        }
    };

    content.subject = Fmt(alloc, T("Recover %1 account"), config.title);
    content.html = PatchMail("existing_user.html", alloc, patch);
    content.text = PatchMail("existing_user.txt", alloc, patch);

    return PostMail(to, content);
}

static bool SendResetPasswordMail(const char *to, const uint8_t token[16], Allocator *alloc)
{
    smtp_MailContent content;

    const char *uuid = FormatUUID(token, alloc);
    const char *url = Fmt(alloc, "%1/reset#token=%2", config.url, uuid).ptr;

    const auto patch = [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "TITLE") {
            writer->Write(config.title);
        } else if (key == "MAIL") {
            writer->Write(to);
        } else if (key == "URL") {
            writer->Write(url);
        } else {
            Print(writer, "{{%1}}", expr);
        }
    };

    content.subject = Fmt(alloc, T("Reset %1 password"), config.title);
    content.html = PatchMail("reset_password.html", alloc, patch);
    content.text = PatchMail("reset_password.txt", alloc, patch);

    return PostMail(to, content);
}

static bool SendLinkIdentityMail(const char *to, const oidc_Provider &provider, const uint8_t token[16], Allocator *alloc)
{
    smtp_MailContent content;

    const char *uuid = FormatUUID(token, alloc);
    const char *url = Fmt(alloc, "%1/link#token=%2", config.url, uuid).ptr;

    const auto patch = [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "TITLE") {
            writer->Write(config.title);
        } else if (key == "MAIL") {
            writer->Write(to);
        } else if (key == "PROVIDER") {
            writer->Write(provider.title);
        } else if (key == "URL") {
            writer->Write(url);
        } else {
            Print(writer, "{{%1}}", expr);
        }
    };

    content.subject = Fmt(alloc, T("Welcome to %1"), config.title);
    content.html = PatchMail("link_identity.html", alloc, patch);
    content.text = PatchMail("link_identity.txt", alloc, patch);

    return PostMail(to, content);
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

static RetainPtr<SessionInfo> CreateUserSession(int64_t userid, bool authorize, const char *username, int picture)
{
    Size username_bytes = strlen(username) + 1;
    Size session_bytes = K_SIZE(SessionInfo) + username_bytes;

    SessionInfo *session = (SessionInfo *)AllocateRaw(nullptr, session_bytes, (int)AllocFlag::Zero);

    new (session) SessionInfo;
    RetainPtr<SessionInfo> ptr(session, [](SessionInfo *session) {
        session->~SessionInfo();
        ReleaseRaw(nullptr, session, -1);
    });

    session->userid = userid;
    session->authorized = authorize;

    CopyString(username, MakeSpan((char *)session->username, username_bytes));
    session->picture = picture;

    return ptr;
}

static const EventInfo *RegisterEvent(const EventKey &key, int64_t time)
{
    std::lock_guard<std::shared_mutex> lock_excl(events_mutex);

    EventInfo *event = events_map.FindValue(key, nullptr);

    if (!event || event->until < GetMonotonicTime()) {
        Allocator *alloc;
        event = events.AppendDefault(&alloc);

        event->key.resource = DuplicateString(key.resource, alloc).ptr;
        event->key.who = DuplicateString(key.who, alloc).ptr;
        event->until = GetMonotonicTime() + BanTime;

        events_map.Set(event);
    }

    event->count++;
    event->prev_time = event->time;
    event->time = time;

    return event;
}

static int CountEvents(const EventKey &key)
{
    std::shared_lock<std::shared_mutex> lock_shr(events_mutex);

    const EventInfo *event = events_map.FindValue(key, nullptr);

    // We don't need to use precise timing, and a ban can last a bit
    // more than BanTime (until pruning clears the ban).
    return event ? event->count : 0;
}

static bool CheckPasswordComplexity(const char *username, Span<const char> password)
{
    unsigned int flags = UINT_MAX & ~(int)pwd_CheckFlag::Score;
    return pwd_CheckPassword(password, username, flags);
}

bool PruneTokens()
{
    int64_t now = GetUnixTime();

    if (!db.Run("DELETE FROM tokens WHERE timestamp < ?1", now - TokenDuration))
        return false;
    if (!db.Run(R"(DELETE FROM users
                   WHERE creation < ?1 AND
                         password_hash IS NULL AND
                         id NOT IN (SELECT user FROM identities))", now - InvalidTimeout))
        return false;

    return true;
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

RetainPtr<SessionInfo> GetNormalSession(http_IO *io)
{
    RetainPtr<SessionInfo> session = sessions.Find(io);

    if (!session)
        return nullptr;
    if (!session->authorized.load(std::memory_order_acquire))
        return nullptr;

    return session;
}

int64_t ValidateApiKey(http_IO *io, int64_t *out_owner)
{
    const http_RequestInfo &request = io->Request();
    const char *header = request.GetHeaderValue("X-Api-Key");

    if (!header) {
        LogError("Missing API key");
        io->SendError(422);
        return -1;
    }

    // We use this to extend/fix the response delay in case of error
    int64_t start = GetMonotonicTime();

    Span<const char> secret;
    Span<const char> key = SplitStr(header, '/', &secret);

    sq_Statement stmt;
    if (!db.Prepare("SELECT id, owner, hash FROM plans WHERE key = ?1", &stmt, key))
        return -1;

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            int64_t safety = std::max(2000 - GetMonotonicTime() + start, (int64_t)0);
            WaitDelay(safety);

            LogError("Invalid API key");
            io->SendError(403);
        }
        return -1;
    }

    int64_t plan = sqlite3_column_int64(stmt, 0);
    int64_t owner = sqlite3_column_int64(stmt, 1);
    const char *hash = (const char *)sqlite3_column_text(stmt, 2);

    if (crypto_pwhash_str_verify(hash, secret.ptr, (size_t)secret.len) < 0) {
        int64_t safety = std::max(2000 - GetMonotonicTime() + start, (int64_t)0);
        WaitDelay(safety);

        LogError("Invalid API key");
        io->SendError(403);
        return -1;
    }

    if (out_owner) {
        *out_owner = owner;
    }
    return plan;
}

static void ExportSession(const SessionInfo *session, json_Writer *json)
{
    if (session) {
        json->StartObject();

        json->Key("userid"); json->Int64(session->userid);
        json->Key("username"); json->String(session->username);

        if (session->authorized.load(std::memory_order_acquire)) {
            json->Key("authorized"); json->Bool(true);
            json->Key("picture"); json->Int(session->picture.load(std::memory_order_relaxed));
        } else {
            json->Key("authorized"); json->Bool(false);
        }

        json->EndObject();
    } else {
        json->Null();
    }
}

void HandleSessionInfo(http_IO *io)
{
    RetainPtr<const SessionInfo> session = sessions.Find(io);

    http_SendJson(io, 200, [&](json_Writer *json) {
        ExportSession(session.GetRaw(), json);
    });
}

void HandleSessionPing(http_IO *io)
{
    // Do this to renew session and clear invalid session cookies
    sessions.Find(io);

    io->SendText(200, "{}", "application/json");
}

void HandleUserRegister(http_IO *io)
{
    if (!config.internal_auth) {
        LogError("Internal authentication is disabled");
        io->SendError(403);
        return;
    }

    const char *mail = nullptr;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "mail") {
                    json->ParseString(&mail);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!mail || !IsMailValid(mail)) {
                    LogError("Missing or invalid mail address");
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

    int64_t userid = -1;
    bool exists = false;
    uint8_t token[16];

    // Always create it to reduce timing discloure
    FillRandomSafe(token, K_SIZE(token));

    // Try to create user
    bool success = db.Transaction([&]() {
        int64_t now = GetUnixTime();

        sq_Statement stmt;
        if (!db.Prepare(R"(INSERT INTO users (mail, username, creation, confirmed, version)
                           VALUES (?1, ?2, ?3, 0, 1)
                           ON CONFLICT DO UPDATE SET confirmed = confirmed
                           RETURNING id, confirmed)",
                        &stmt, mail, mail, GetUnixTime()))
            return false;

        if (stmt.Step()) {
            userid = sqlite3_column_int64(stmt, 0);
            exists = sqlite3_column_int(stmt, 1);
        } else {
            K_ASSERT(!stmt.IsValid());
            return false;
        }

        if (!exists && !db.Run(R"(INSERT INTO tokens (token, type, timestamp, user)
                                  VALUES (?1, 'password', ?2, ?3))",
                               sq_Binding(token), now, userid))
            return false;

        return true;
    });
    if (!success)
        return;

    if (exists) {
        if (!SendExistingUserMail(mail, io->Allocator()))
            return;
    } else {
        if (!SendNewUserMail(mail, token, io->Allocator()))
            return;
    }

    io->SendText(200, "{}", "application/json");
}

void HandleUserLogin(http_IO *io)
{
    if (!config.internal_auth) {
        LogError("Internal authentication is disabled");
        io->SendError(403);
        return;
    }

    const http_RequestInfo &request = io->Request();

    const char *mail = nullptr;
    const char *password = nullptr;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "mail") {
                    json->ParseString(&mail);
                } else if (key == "password") {
                    json->ParseString(&password);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!mail || !IsMailValid(mail)) {
                    LogError("Missing or invalid mail address");
                    valid = false;
                }
                if (!password) {
                    LogError("Missing password");
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
    int64_t start = GetMonotonicTime();

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT id, password_hash, username, totp, version
                       FROM users
                       WHERE mail = ?1 AND confirmed = 1)", &stmt, mail))
        return;
    stmt.Run();

    // Validate password if user exists
    if (stmt.IsRow() && CountEvents({ request.client_addr, mail }) < BanThreshold) {
        int64_t userid = sqlite3_column_int64(stmt, 0);
        const char *password_hash = (const char *)sqlite3_column_text(stmt, 1);
        const char *username = (const char *)sqlite3_column_text(stmt, 2);
        bool authorize = (sqlite3_column_type(stmt, 3) == SQLITE_NULL);
        int picture = sqlite3_column_int(stmt, 4);

        if (password_hash && crypto_pwhash_str_verify(password_hash, password, strlen(password)) == 0) {
            RetainPtr<SessionInfo> session = CreateUserSession(userid, authorize, username, picture);
            sessions.Open(io, session);

            http_SendJson(io, 200, [&](json_Writer *json) {
                ExportSession(session.GetRaw(), json);
            });

            return;
        } else {
            RegisterEvent({ request.client_addr, mail }, start);
        }
    }

    // Enforce constant delay if authentification fails
    if (stmt.IsValid()) {
        int64_t safety = std::max(2000 - GetMonotonicTime() + start, (int64_t)0);
        WaitDelay(safety);

        LogError("Invalid username or password");
        io->SendError(403);
    }
}

void HandleUserRecover(http_IO *io)
{
    if (!config.internal_auth) {
        LogError("Internal authentication is disabled");
        io->SendError(403);
        return;
    }

    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    const char *mail = nullptr;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "mail") {
                    json->ParseString(&mail);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!mail || !IsMailValid(mail)) {
                    LogError("Missing or invalid mail address");
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

    int64_t userid = 0;
    uint8_t token[16];

    // Always create it to reduce timing discloure
    FillRandomSafe(token, K_SIZE(token));

    // Find user, unless it has no password and has been linked with an SSO... which would mean that
    // it was created through SSO, and we don't want users to use this API to create a password on an
    // "SSO-only" account.
    // Unless the user is logged in, in which case all is well, allow password creation.
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(SELECT u.id
                           FROM users u
                           LEFT JOIN identities i ON (i.user = u.id)
                           WHERE u.mail = ?1 AND
                                 (u.id = ?2 OR u.password_hash IS NOT NULL OR i.id IS NULL))",
                        &stmt, mail, session ? session->userid : 0))
            return;

        if (stmt.Step()) {
            userid = sqlite3_column_int64(stmt, 0);
        } else if (!stmt.IsValid()) {
            return;
        }
    }

    int64_t now = GetMonotonicTime();

    // Create recovery token
    if (userid > 0 && RegisterEvent({ request.client_addr, mail }, now)->count < BanThreshold) {
        int64_t now = GetUnixTime();

        if (!db.Run(R"(INSERT INTO tokens (token, type, timestamp, user)
                       VALUES (?1, 'password', ?2, ?3))",
                    sq_Binding(token), now, userid))
            return;

        if (!SendResetPasswordMail(mail, token, io->Allocator()))
            return;
    } else {
        LogError("Refusing to send password recovery email for '%1'", mail);
    }

    io->SendText(200, "{}", "application/json");
}

void HandleUserLogout(http_IO *io)
{
    sessions.Close(io);
    io->SendText(200, "{}", "application/json");
}

void HandleUserReset(http_IO *io)
{
    if (!config.internal_auth) {
        LogError("Internal authentication is disabled");
        io->SendError(403);
        return;
    }

    const char *token = nullptr;
    const char *password = nullptr;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "token") {
                    json->ParseString(&token);
                } else if (key == "password") {
                    json->SkipNull() || json->ParseString(&password);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!token) {
                    LogError("Missing token");
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

    int64_t userid = 0;
    const char *username = nullptr;

    // Validate token
    {
        int64_t now = GetUnixTime();

        sq_Statement stmt;
        if (!db.Prepare(R"(SELECT t.timestamp, t.user, u.username
                           FROM tokens t
                           INNER JOIN users u ON (u.id = t.user)
                           WHERE t.token = uuid_blob(?1) AND t.type = 'password')", &stmt, token))
            return;

        if (stmt.Step()) {
            int64_t timestamp = sqlite3_column_int64(stmt, 0);

            if (now - timestamp > TokenDuration)
                goto invalid;

            userid = sqlite3_column_int64(stmt, 1);
            username = DuplicateString((const char *)sqlite3_column_text(stmt, 2), io->Allocator()).ptr;
        } else if (stmt.IsValid()) {
            goto invalid;
        } else {
            return;
        }
    }

    // This API can also be used to check if a token is valid
    if (password) {
        if (!CheckPasswordComplexity(username, password)) {
            io->SendError(422);
            return;
        }

        char hash[PasswordHashBytes];
        if (!HashPassword(password, hash))
            return;

        bool success = db.Transaction([&]() {
            if (!db.Run("UPDATE users SET password_hash = ?2, confirmed = 1 WHERE id = ?1", userid, hash))
                return false;
            if (!db.Run("DELETE FROM tokens WHERE user = ?1", userid))
                return false;

            return true;
        });
        if (!success)
            return;
    }

    io->SendText(200, "{}", "application/json");
    return;

invalid:
    LogError("Invalid or expired token");
    io->SendError(404);
}

void HandleUserPassword(http_IO *io)
{
    if (!config.internal_auth) {
        LogError("Internal authentication is disabled");
        io->SendError(403);
        return;
    }

    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
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
                if (!old_password) {
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
    if (!CheckPasswordComplexity(session->username, new_password)) {
        io->SendError(422);
        return;
    }

    // Authenticate with old password
    {
        int64_t start = GetMonotonicTime();

        sq_Statement stmt;
        if (!db.Prepare("SELECT password_hash FROM users WHERE id = ?1 AND confirmed = 1", &stmt))
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

        if (!password_hash || crypto_pwhash_str_verify(password_hash, old_password, strlen(old_password)) < 0) {
            // Enforce constant delay if authentification fails
            int64_t safety = std::max(2000 - GetMonotonicTime() + start, (int64_t)0);
            WaitDelay(safety);

            LogError("Invalid password");
            io->SendError(403);
            return;
        }

        if (TestStr(new_password, old_password)) {
            LogError("You cannot reuse the same password");
            io->SendError(422);
            return;
        }
    }

    // Update password
    {
        char hash[PasswordHashBytes];
        if (!HashPassword(new_password, hash))
            return;

        if (!db.Run("UPDATE users SET password_hash = ?2 WHERE id = ?1", session->userid, hash))
            return;
    }

    io->SendText(200, "{}", "application/json");
    return;
}

void HandleUserSecurity(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    const char *mail;
    bool has_password;
    bool has_totp;
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(SELECT mail,
                                  IIF(password_hash IS NOT NULL, 1, 0) AS password,
                                  IIF(totp IS NOT NULL, 1, 0) AS totp
                           FROM users WHERE id = ?1)",
                        &stmt, session->userid))
            return;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("User does not exist");
                io->SendError(404);
            }
            return;
        }

        mail = DuplicateString((const char *)sqlite3_column_text(stmt, 0), io->Allocator()).ptr;
        has_password = sqlite3_column_int(stmt, 1);
        has_totp = sqlite3_column_int(stmt, 2);
    }

    if (!config.internal_auth) {
        has_password = false;
        has_totp = false;
    }

    sq_Statement stmt;
    if (!db.Prepare("SELECT id, issuer, allowed FROM identities WHERE user = ?1", &stmt, session->userid))
        return;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        json->Key("mail"); json->String(mail);
        json->Key("password"); json->Bool(has_password);
        json->Key("totp"); json->Bool(has_totp);

        json->Key("identities"); json->StartArray();
        while (stmt.Step()) {
            int64_t id = sqlite3_column_int64(stmt, 0);
            const char *issuer = (const char *)sqlite3_column_text(stmt, 1);
            bool allowed = sqlite3_column_int(stmt, 2);

            json->StartObject();

            json->Key("id"); json->Int64(id);
            json->Key("issuer"); json->String(issuer);
            json->Key("allowed"); json->Bool(allowed);

            json->EndObject();
        }
        if (!stmt.IsValid())
            return;
        json->EndArray();

        json->EndObject();
    });
}

void HandleSsoLogin(http_IO *io)
{
    const char *type = nullptr;
    const char *redirect = nullptr;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "provider") {
                    json->ParseString(&type);
                } else if (key == "redirect") {
                    json->ParseString(&redirect);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!type) {
                    LogError("Missing 'type' parameter");
                    valid = false;
                }
                if (!redirect) {
                    LogError("Missing 'redirect' parameter");
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

    const oidc_Provider *provider = config.oidc_map.FindValue(type, nullptr);
    if (!provider) {
        LogError("Unknown provider type '%1'", type);
        io->SendError(404);
        return;
    }

    const char *scopes = "email";
    const char *callback = Fmt(io->Allocator(), "%1/oidc", config.url).ptr;

    oidc_AuthorizationInfo auth;
    oidc_PrepareAuthorization(*provider, scopes, callback, redirect, io->Allocator(), &auth);

    // Don't set SameSite=Strict because we want the cookie to be available when the user gets redirected to the callback URL
    io->AddCookieHeader("/", "oidc", auth.cookie, SsoCookieFlags, SsoCookieMaxAge);

    Span<const char> json = Fmt(io->Allocator(), "{\"url\": \"%1\"}", FmtEscape(auth.url, '"'));
    io->SendText(200, json, "application/json");
}

void HandleSsoOidc(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    const char *cookie = request.GetCookieValue("oidc");

    if (!cookie) {
        LogError("Missing SSO safety cookie");
        io->SendError(422);
        return;
    }

    const char *code = nullptr;
    const char *state = nullptr;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "code") {
                    json->ParseString(&code);
                } else if (key == "state") {
                    json->ParseString(&state);
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
                if (!state) {
                    LogError("Missing 'state' parameter");
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

    oidc_CookieInfo info;
    if (!oidc_CheckCookie(cookie, state, io->Allocator(), &info)) {
        io->SendError(401);
        return;
    }

    const oidc_Provider *provider = config.oidc_map.FindValue(info.issuer, nullptr);
    if (!provider) [[unlikely]] {
        LogError("SSO provider '%1' is gone!", info.issuer);
        return;
    }

    oidc_TokenSet tokens;
    {
        const char *callback = Fmt(io->Allocator(), "%1/oidc", config.url).ptr;

        if (!oidc_ExchangeCode(*provider, callback, code, io->Allocator(), &tokens)) {
            io->SendError(401);
            return;
        }
    }

    oidc_IdentityInfo identity;
    if (!oidc_DecodeIdToken(*provider, tokens.id, info.nonce, io->Allocator(), &identity)) {
        io->SendError(401);
        return;
    }

    // Delete cookie with state and nonce
    io->AddCookieHeader("/", "oidc", nullptr, SsoCookieFlags, SsoCookieMaxAge);

    if (!identity.email) {
        LogError("Cannot use SSO login without mail address");
        io->SendError(403);
        return;
    }

    RetainPtr<SessionInfo> session;

    // Find matching identity and user account
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(SELECT u.id, u.username, u.version
                           FROM identities i
                           INNER JOIN users u ON (u.id = i.user)
                           WHERE i.issuer = ?1 AND i.sub = ?2 AND
                                 i.allowed = 1 AND u.confirmed = 1)",
                        &stmt, provider->issuer, identity.sub))
            return;

        if (stmt.Step()) {
            int64_t userid = sqlite3_column_int64(stmt, 0);
            const char *username = (const char *)sqlite3_column_text(stmt, 1);
            int picture = sqlite3_column_int(stmt, 2);

            session = CreateUserSession(userid, true, username, picture);
        }
        if (!stmt.IsValid())
            return;
    }

    // Create user if needed and safely link it with social identity
    if (!session) {
        int64_t now = GetUnixTime();
        bool verified = identity.email_verified;

        int64_t userid = 0;
        uint8_t token[16];
        bool created = false;
        bool allowed = false;

        // Always create it to reduce timing discloure
        FillRandomSafe(token, K_SIZE(token));

        bool success = db.Transaction([&]() {
            {
                sq_Statement stmt;
                if (!db.Prepare(R"(INSERT INTO users (mail, username, creation, confirmed, version)
                                   VALUES (?1, ?2, ?3, ?4, 1)
                                   ON CONFLICT DO UPDATE SET confirmed = confirmed
                                   RETURNING id, creation)",
                                &stmt, identity.email, identity.email, now, 0 + verified))
                    return false;

                if (!stmt.Step()) {
                    K_ASSERT(!stmt.IsValid());
                    return false;
                }

                userid = sqlite3_column_int64(stmt, 0);
                created = (sqlite3_column_int64(stmt, 1) == now);

                // Automatically allow the provider that resulted in user creation if address mail is verified
                allowed = verified && created;
            }

            // Create identity
            int64_t id;
            {
                sq_Statement stmt;
                if (!db.Prepare(R"(INSERT INTO identities (user, issuer, sub, allowed)
                                   VALUES (?1, ?2, ?3, ?4)
                                   ON CONFLICT (issuer, sub) DO UPDATE SET allowed = allowed
                                   RETURNING id)",
                                &stmt, userid, provider->issuer, identity.sub, 0 + allowed))
                    return false;

                if (!stmt.Step()) {
                    K_ASSERT(!stmt.IsValid());
                    return false;
                }

                id = sqlite3_column_int64(stmt, 0);
            }

            if (!allowed && !db.Run(R"(INSERT INTO tokens (token, type, timestamp, user, identity)
                                       VALUES (?1, 'link', ?2, ?3, ?4))",
                                    sq_Binding(token), now, userid, id))
                return false;

            return true;
        });
        if (!success)
            return;

        if (allowed) {
            session = CreateUserSession(userid, true, identity.email, 1);
        } else {
            if (!SendLinkIdentityMail(identity.email, *provider, token, io->Allocator()))
                return;

            if (created) {
                LogError("Your account has been created, but you must confirm your mail address. Consult the mail that has been sent to you to confirm it.");
            } else {
                LogError("An account with address '%1' already exists, consult the mail that has been sent to you to continue.", identity.email);
            }
            io->SendError(409);
            return;
        }
    }

    K_ASSERT(session);
    sessions.Open(io, session);

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        json->Key("allowed"); json->Bool(true);
        json->Key("redirect"); json->String(info.redirect);
        json->Key("session"); ExportSession(session.GetRaw(), json);

        json->EndObject();
    });
}

void HandleSsoLink(http_IO *io)
{
    const char *token = nullptr;
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
                if (!token) {
                    LogError("Missing token");
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

    int64_t userid = 0;
    int64_t identity = 0;
    const char *issuer = nullptr;

    // Validate token
    {
        int64_t now = GetUnixTime();

        sq_Statement stmt;
        if (!db.Prepare(R"(SELECT t.timestamp, t.user, t.identity, i.issuer
                           FROM tokens t
                           INNER JOIN users u ON (u.id = t.user)
                           INNER JOIN identities i ON (i.id = t.identity)
                           WHERE t.token = uuid_blob(?1) AND t.type = 'link')", &stmt, token))
            return;

        if (stmt.Step()) {
            int64_t timestamp = sqlite3_column_int64(stmt, 0);

            if (now - timestamp > TokenDuration)
                goto invalid;

            userid = sqlite3_column_int64(stmt, 1);
            identity = sqlite3_column_int64(stmt, 2);
            issuer = DuplicateString((const char *)sqlite3_column_text(stmt, 3), io->Allocator()).ptr;
        } else if (stmt.IsValid()) {
            goto invalid;
        } else {
            return;
        }
    }

    // Confirm link
    {
        bool success = db.Transaction([&]() {
            if (!db.Run("UPDATE users SET confirmed = 1 WHERE id = ?1", userid))
                return false;
            if (!db.Run("UPDATE identities SET allowed = 1 WHERE id = ?1", identity))
                return false;
            if (!db.Run("DELETE FROM tokens WHERE user = ?1", userid))
                return false;

            return true;
        });
        if (!success)
            return;
    }

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();
        json->Key("issuer"); json->String(issuer);
        json->EndObject();
    });
    return;

invalid:
    LogError("Invalid or expired token");
    io->SendError(404);
}

void HandleSsoUnlink(http_IO *io)
{
    RetainPtr<SessionInfo> session = sessions.Find(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    int64_t identity = -1;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "identity") {
                    json->ParseInt(&identity);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (identity < 0) {
                    LogError("Missing identity");
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

    if (!db.Run("DELETE FROM identities WHERE user = ?1 AND id = ?2", session->userid, identity))
        return;

    io->SendText(200, "{}", "application/json");
}

static bool CheckTotp(http_IO *io, int64_t userid, const char *secret, const char *code)
{
    int64_t time = GetUnixTime();
    int64_t counter = time / TotpPeriod;
    int64_t min = counter - 1;
    int64_t max = counter + 1;

    if (pwd_CheckHotp(secret, pwd_HotpAlgorithm::SHA1, min, max, 6, code)) {
        char who[64];
        Fmt(who, "%1", userid);

        const EventInfo *event = RegisterEvent({ "TOTP", who }, time);

        bool replay = (event->prev_time / TotpPeriod >= min) &&
                      pwd_CheckHotp(secret, pwd_HotpAlgorithm::SHA1, min, event->prev_time / TotpPeriod, 6, code);

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

    return true;
}

void HandleTotpConfirm(http_IO *io)
{
    if (!config.internal_auth) {
        LogError("Internal authentication is disabled");
        io->SendError(403);
        return;
    }

    RetainPtr<SessionInfo> session = sessions.Find(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (session->authorized.load(std::memory_order_acquire)) {
        LogError("Session does not need TOTP check");
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

    const char *secret;
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT totp FROM users WHERE id = ?1 AND totp IS NOT NULL", &stmt, session->userid))
            return;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Cannot find TOTP secret");
                io->SendError(404);
            }
            return;
        }

        secret = DuplicateString((const char *)sqlite3_column_text(stmt, 0), io->Allocator()).ptr;
    }

    // Immediate confirmation looks weird
    WaitDelay(800);

    if (CheckTotp(io, session->userid, secret, code)) {
        session->authorized = true;

        http_SendJson(io, 200, [&](json_Writer *json) {
            ExportSession(session.GetRaw(), json);
        });
    }
}

static uint8_t *GetTotpTokenKey32()
{
    static uint8_t key[32];
    static std::once_flag flag;

    std::call_once(flag, []() {
        FillRandomSafe(key, K_SIZE(key));
    });

    return key;
}

void HandleTotpSecret(http_IO *io)
{
    if (!config.internal_auth) {
        LogError("Internal authentication is disabled");
        io->SendError(403);
        return;
    }

    RetainPtr<SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    char secret[33];
    pwd_GenerateSecret(secret);

    const char *url = pwd_GenerateHotpUrl(config.title, session->username, config.title,
                                          pwd_HotpAlgorithm::SHA1, secret, 6, io->Allocator());
    if (!url)
        return;

    Span<const char> image;
    {
        HeapArray<uint8_t> png;

        StreamWriter st(&png, "<png>");
        if (!qr_EncodeTextToPng(url, 0, &st))
            return;
        if (!st.Close())
            return;

        Span<const char> prefix = "data:image/png;base64,";
        Size needed = prefix.len + (Size)sodium_base64_encoded_len(png.len, sodium_base64_VARIANT_ORIGINAL);
        Span<char> buf = AllocateSpan<char>(io->Allocator(), needed);

        CopyString(prefix, buf);
        sodium_bin2base64(buf.ptr + prefix.len, (size_t)(buf.len - prefix.len), png.ptr, (size_t)png.len, sodium_base64_VARIANT_ORIGINAL);

        image = buf.Take(0, buf.len - 1);
    }

    // Encrypt secret
    char token[1024];
    {
        const uint8_t *key = GetTotpTokenKey32();
        const Size secret_len = K_SIZE(secret) - 1;

        uint8_t cypher[crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + secret_len];
        FillRandomSafe(cypher, crypto_secretbox_NONCEBYTES);
        crypto_secretbox_easy(cypher + crypto_secretbox_NONCEBYTES, (const uint8_t *)secret, secret_len, cypher, key);
        static_assert(K_SIZE(token) >= K_SIZE(cypher) * 2);

        sodium_bin2base64(token, K_SIZE(token), cypher, K_SIZE(cypher), sodium_base64_VARIANT_ORIGINAL);
    }

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        json->Key("token"); json->String(token);
        json->Key("secret"); json->String(secret);
        json->Key("url"); json->String(url);
        json->Key("image"); json->String(image.ptr, image.len);

        json->EndObject();
    });
}

void HandleTotpChange(http_IO *io)
{
    if (!config.internal_auth) {
        LogError("Internal authentication is disabled");
        io->SendError(403);
        return;
    }

    RetainPtr<SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    const char *token = nullptr;
    const char *password = nullptr;
    const char *code = nullptr;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "token") {
                    json->ParseString(&token);
                } else if (key == "password") {
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
                if (!token) {
                    LogError("Missing 'token' parameter");
                    valid = false;
                }
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

    // Decrypt secret
    char secret[33];
    {
        const uint8_t *key = GetTotpTokenKey32();
        const Size secret_len = K_SIZE(secret) - 1;

        uint8_t cypher[crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + secret_len];
        {
            size_t cypher_len;

            if (sodium_base642bin(cypher, K_SIZE(cypher), token, strlen(token),
                                  nullptr, &cypher_len, nullptr, sodium_base64_VARIANT_ORIGINAL) != 0) {
                LogError("Malformed TOTP secret token");
                io->SendError(422);
                return;
            }
            if (cypher_len != K_SIZE(cypher)) {
                LogError("Malformed TOTP secret token");
                io->SendError(422);
                return;
            }
        }

        if (crypto_secretbox_open_easy((uint8_t *)secret, cypher + crypto_secretbox_NONCEBYTES,
                                       K_SIZE(cypher) - crypto_secretbox_NONCEBYTES, cypher, key) != 0) {
            LogError("Invalid TOTP secret token");
            io->SendError(422);
            return;
        }

        secret[secret_len] = 0;
    }

    // We use this to extend/fix the response delay in case of error
    int64_t now = GetMonotonicTime();

    // Authenticate with password
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(SELECT password_hash FROM users WHERE id = ?1)", &stmt))
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
    if (!CheckTotp(io, session->userid, secret, code))
        return;

    if (!db.Run("UPDATE users SET totp = ?2 WHERE id = ?1", session->userid, secret))
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleTotpDisable(http_IO *io)
{
    if (!config.internal_auth) {
        LogError("Internal authentication is disabled");
        io->SendError(403);
        return;
    }

    RetainPtr<SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    const char *password = nullptr;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "password") {
                    json->ParseString(&password);
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
        if (!db.Prepare(R"(SELECT password_hash FROM users WHERE id = ?1)", &stmt))
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

    if (!db.Run("UPDATE users SET totp = NULL WHERE id = ?1", session->userid))
        return;

    io->SendText(200, "{}", "application/json");
}

static void SendDefaultPicture(http_IO *io)
{
#if defined(FELIX_HOT_ASSETS)
    const AssetInfo *DefaultPicture = FindEmbedAsset("src/rekkord/web/assets/ui/anonymous.png");
    K_ASSERT(DefaultPicture);
#else
    static const AssetInfo *DefaultPicture = FindEmbedAsset("src/rekkord/web/assets/ui/anonymous.png");
    K_ASSERT(DefaultPicture);
#endif

    io->AddEncodingHeader(DefaultPicture->compression_type);
    io->SendBinary(200, DefaultPicture->data, "image/png");
}

void HandlePictureGet(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    int64_t userid = -1;
    bool explicit_user = false;

    if (StartsWith(request.path, "/pictures/")) {
        K_ASSERT(StartsWith(request.path, "/pictures/"));
        Span<const char> str = request.path + 10;

        if (!ParseInt(str, &userid)) {
            io->SendError(422);
            return;
        }

        explicit_user = true;
    } else {
        RetainPtr<const SessionInfo> session = GetNormalSession(io);

        if (!session) {
            LogError("User is not logged in");
            io->SendError(401);
            return;
        }

        userid = session->userid;
        explicit_user = false;
    }

    sqlite3_blob *blob;
    if (sqlite3_blob_open(db, "main", "users", "picture", userid, 0, &blob) != SQLITE_OK) {
        // Assume there's no picture!

        if (explicit_user) {
            SendDefaultPicture(io);
            return;
        } else {
            io->SendError(404);
            return;
        }
    }
    K_DEFER { sqlite3_blob_close(blob); };

    Size len = sqlite3_blob_bytes(blob);

    // Send file
    {
        io->AddHeader("Content-Type", "image/png");
        io->AddCachingHeaders(explicit_user ? PictureCacheDelay : 0);

        StreamWriter writer;
        if (!io->OpenForWrite(200, len, &writer))
            return;

        Size offset = 0;
        StreamReader reader([&](Span<uint8_t> buf) {
            Size copy_len = std::min(len - offset, buf.len);

            if (sqlite3_blob_read(blob, buf.ptr, (int)copy_len, (int)offset) != SQLITE_OK) {
                LogError("SQLite Error: %1", sqlite3_errmsg(db));
                return (Size)-1;
            }

            offset += copy_len;
            return copy_len;
        }, "<picture>");

        // Not much we can do at this stage in case of error. Client will get truncated data.
        SpliceStream(&reader, -1, &writer);
        writer.Close();
    }
}

void HandlePictureSave(http_IO *io)
{
    RetainPtr<SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    // Create temporary file
    int fd = -1;
    const char *tmp_filename = CreateUniqueFile(config.tmp_directory, nullptr, ".tmp", io->Allocator(), &fd);
    if (!tmp_filename)
        return;
    K_DEFER {
        CloseDescriptor(fd);
        UnlinkFile(tmp_filename);
    };

    // Read request body
    {
        StreamWriter writer(fd, "<temp>", 0);

        StreamReader reader;
        if (!io->OpenForRead(MaxPictureSize, &reader))
            return;

        do {
            LocalArray<uint8_t, 16384> buf;
            buf.len = reader.Read(buf.data);

            if (buf.len < 0)
                return;

            if (!writer.Write(buf))
                return;
        } while (!reader.IsEOF());

        if (!writer.Close())
            return;
    }

    // Copy to database blob
    bool success = db.Transaction([&]() {
#if defined(_WIN32)
        int64_t file_len = _lseeki64(fd, 0, SEEK_CUR);
#else
        int64_t file_len = lseek(fd, 0, SEEK_CUR);
#endif

        if (lseek(fd, 0, SEEK_SET) < 0) {
            LogError("lseek('<temp>') failed: %1", strerror(errno));
            return false;
        }

        if (!db.Run("UPDATE users SET picture = ?2, version = version + 1 WHERE id = ?1",
                    session->userid, sq_Binding::Zeroblob(file_len)))
            return false;

        sqlite3_blob *blob;
        if (sqlite3_blob_open(db, "main", "users", "picture", session->userid, 1, &blob) != SQLITE_OK) {
            LogError("SQLite Error: %1", sqlite3_errmsg(db));
            return false;
        }
        K_DEFER { sqlite3_blob_close(blob); };

        StreamReader reader(fd, "<temp>");
        int64_t read_len = 0;

        do {
            LocalArray<uint8_t, 16384> buf;
            buf.len = reader.Read(buf.data);

            if (buf.len < 0)
                return false;
            if (buf.len + read_len > file_len) {
                LogError("Temporary file size has changed (bigger)");
                return false;
            }

            if (sqlite3_blob_write(blob, buf.data, (int)buf.len, (int)read_len) != SQLITE_OK) {
                LogError("SQLite Error: %1", sqlite3_errmsg(db));
                return false;
            }

            read_len += buf.len;
        } while (!reader.IsEOF());

        if (read_len < file_len) {
            LogError("Temporary file size has changed (truncated)");
            return false;
        }

        session->picture++;

        return true;
    });
    if (!success)
        return;

    const char *response = Fmt(io->Allocator(), "{ \"picture\": %1 }", session->picture.load(std::memory_order_relaxed)).ptr;
    io->SendText(200, response, "application/json");
}

void HandlePictureDelete(http_IO *io)
{
    RetainPtr<SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    bool success = db.Transaction([&]() {
        if (!db.Run("UPDATE users SET picture = NULL, version = version + 1 WHERE id = ?1", session->userid))
            return false;

        session->picture++;

        return true;
    });
    if (!success)
        return;

    const char *response = Fmt(io->Allocator(), "{ \"picture\": %1 }", session->picture.load(std::memory_order_relaxed)).ptr;
    io->SendText(200, response, "application/json");
}

}
