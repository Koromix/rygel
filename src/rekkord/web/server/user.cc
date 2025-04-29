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
#include "src/core/http/http.hh"
#include "src/core/password/password.hh"
#include "src/core/request/smtp.hh"
#include "src/core/wrap/qrcode.hh"
#include "rekkow.hh"
#include "user.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static const int PasswordHashBytes = 128;

static const int64_t TokenDuration = 1800 * 1000;
static const int64_t InvalidTimeout = 86400 * 1000;
static const int BanThreshold = 6;
static const int64_t BanTime = 1800 * 1000;

struct SessionInfo: public RetainObject<SessionInfo> {
    int64_t userid;
    char username[];
};

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

static smtp_Sender smtp;

static const smtp_MailContent NewUser = {
    "Welcome to {{ TITLE }}",

    R"(Welcome !

Use the following link to validate your account and access {{ TITLE }}:

{{ URL }}

Once your address is confirmed, you will be invited to choose a secure password.

{{ TITLE }})",

    R"(<html lang="en"><body>
<p>Welcome !</p>
<p>Use the following link to validate your account and access {{ TITLE }}:</p>
<div align="center"><br>
    <a style="padding: 0.7em 2em; background: #2d8261; border-radius: 30px;
              font-weight: bold; text-decoration: none !important; color: white; text-transform: uppercase; text-wrap: nowrap;" href="{{ URL }}">Connexion à {{ TITLE }}</a>
<br><br></div>
<p>Once your address is confirmed, you will be invited to choose a secure password.</p>
<p><i>{{ TITLE }}</i></p>
</body></html>)",

    {}
};

static const smtp_MailContent ExistingUser = {
    "Recover {{ TITLE }} account",
    R"(Hello,

Someone tried to create an account on {{ TITLE }} with your email.

If it is not you, ignore this mail. Use the recovery page if you are trying to login but you have forgotten the password.

{{ TITLE }})",
    R"(<html lang="en"><body>
<p>Hello,</p>
<p>Someone tried to create an account on {{ TITLE }} with your email.</p>
<p>If it is not you, ignore this mail. Use the recovery page if you are trying to login but you have forgotten the password.</p>
<p><i>{{ TITLE }}</i></p>
</body></html>)",

    {}
};

static const smtp_MailContent ResetPassword = {
    "Reset {{ TITLE }} password",
    R"(Hello,

Use the following link to reset your password on {{ TITLE }}:

{{ URL }}

If you did not ask for password recovery, delete this mail. Someone may have tried to access your account without your permission.

{{ TITLE }})",
    R"(<html lang="en"><body>
<p>Hello,</p>
<p>Use the following link to reset your password on {{ TITLE }}:</p>
<div align="center"><br>
    <a style="padding: 0.7em 2em; background: #2d8261; border-radius: 30px;
              font-weight: bold; text-decoration: none !important; color: white; text-transform: uppercase; text-wrap: nowrap;" href="{{ URL }}">Connexion à {{ TITLE }}</a>
<br><br></div>
<p>If you did not ask for password recovery, delete this mail. Someone may have tried to access your account without your permission.</p>
<p><i>{{ TITLE }}</i></p>
</body></html>)",

    {}
};

static bool IsMailValid(const char *mail)
{
    const auto test_char = [](char c) { return strchr("<>& ", c) || (uint8_t)c < 32; };

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

bool InitSMTP(const smtp_Config &config)
{
    return smtp.Init(config);
}

static Span<const char> PatchText(Span<const char> text, const char *mail, const char *url, Allocator *alloc)
{
    Span<const char> ret = PatchFile(text, alloc, [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "TITLE") {
            writer->Write(config.title);
        } else if (key == "MAIL") {
            writer->Write(mail);
        } else if (key == "URL") {
            RG_ASSERT(url);
            writer->Write(url);
        } else {
            Print(writer, "{{%1}}", expr);
        }
    });

    return ret;
}

static const char *FormatUUID(const uint8_t raw[16], Allocator *alloc)
{
    const char *uuid = Fmt(alloc, "%1-%2-%3-%4-%5", 
                           FmtSpan(MakeSpan(raw + 0, 4), FmtType::BigHex, "").Pad0(-2),
                           FmtSpan(MakeSpan(raw + 4, 2), FmtType::BigHex, "").Pad0(-2),
                           FmtSpan(MakeSpan(raw + 6, 2), FmtType::BigHex, "").Pad0(-2),
                           FmtSpan(MakeSpan(raw + 8, 2), FmtType::BigHex, "").Pad0(-2),
                           FmtSpan(MakeSpan(raw + 10, 6), FmtType::BigHex, "").Pad0(-2)).ptr;
    return uuid;
}

static bool SendNewMail(const char *to, const uint8_t token[16], Allocator *alloc)
{
    smtp_MailContent content;

    const char *uuid = FormatUUID(token, alloc);
    const char *url = Fmt(alloc, "%1/confirm#token=%2", config.url, uuid).ptr;

    content.subject = PatchText(NewUser.subject, to, url, alloc).ptr;
    content.html = PatchText(NewUser.html, to, url, alloc).ptr;
    content.text = PatchText(NewUser.text, to, url, alloc).ptr;

    return smtp.Send(to, content);
}

static bool SendExistingMail(const char *to, Allocator *alloc)
{
    smtp_MailContent content;

    content.subject = PatchText(ExistingUser.subject, to, nullptr, alloc).ptr;
    content.html = PatchText(ExistingUser.html, to, nullptr, alloc).ptr;
    content.text = PatchText(ExistingUser.text, to, nullptr, alloc).ptr;

    return smtp.Send(to, content);
}

static bool SendResetMail(const char *to, const uint8_t token[16], Allocator *alloc)
{
    smtp_MailContent content;

    const char *uuid = FormatUUID(token, alloc);
    const char *url = Fmt(alloc, "%1/reset#token=%2", config.url, uuid).ptr;

    content.subject = PatchText(ResetPassword.subject, to, url, alloc).ptr;
    content.html = PatchText(ResetPassword.html, to, url, alloc).ptr;
    content.text = PatchText(ResetPassword.text, to, url, alloc).ptr;

    return smtp.Send(to, content);
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

static RetainPtr<SessionInfo> CreateUserSession(int64_t userid, const char *username)
{
    Size username_bytes = strlen(username) + 1;
    Size session_bytes = RG_SIZE(SessionInfo) + username_bytes;

    SessionInfo *session = (SessionInfo *)AllocateRaw(nullptr, session_bytes, (int)AllocFlag::Zero);

    new (session) SessionInfo;
    RetainPtr<SessionInfo> ptr(session, [](SessionInfo *session) {
        session->~SessionInfo();
        ReleaseRaw(nullptr, session, -1);
    });

    session->userid = userid;
    CopyString(username, MakeSpan((char *)session->username, username_bytes));

    return ptr;
}

static int RegisterEvent(const char *where, const char *who, int64_t time = GetUnixTime())
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

    return event->count;
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
    if (!db.Run("DELETE FROM users WHERE creation < ?1 AND password_hash IS NULL", now - InvalidTimeout))
        return false;

    return true;
}

void HandleUserRegister(http_IO *io)
{
    // Parse input data
    const char *mail = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "mail") {
                parser.ParseString(&mail);
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

    // Check missing or invalid values
    {
        bool valid = true;

        if (!mail || !IsMailValid(mail)) {
            LogError("Missing or invalid mail address");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    int64_t userid = -1;
    bool exists = false;
    uint8_t token[16];

    // Always create it to reduce timing discloure
    FillRandomSafe(token, RG_SIZE(token));

    // Try to create user
    bool success = db.Transaction([&]() {
        int64_t now = GetUnixTime();

        sq_Statement stmt;
        if (!db.Prepare(R"(INSERT INTO users (mail, username, creation) VALUES (?1, ?2, ?3)
                           ON CONFLICT DO UPDATE SET mail = excluded.mail
                           RETURNING id, password_hash)",
                        &stmt, mail, mail, GetUnixTime()))
            return false;

        if (stmt.Step()) {
            userid = sqlite3_column_int64(stmt, 0);
            exists = (sqlite3_column_type(stmt, 1) != SQLITE_NULL);
        } else {
            RG_ASSERT(!stmt.IsValid());
            return false;
        }

        if (!exists && !db.Run(R"(INSERT INTO tokens (token, timestamp, user)
                                  VALUES (?1, ?2, ?3))", sq_Binding(token), now, userid))
            return false;

        return true;
    });
    if (!success)
        return;

    if (exists) {
        if (!SendExistingMail(mail, io->Allocator()))
            return;
    } else {
        if (!SendNewMail(mail, token, io->Allocator()))
            return;
    }

    io->SendText(200, "{}", "application/json");
}

static void ExportSession(const SessionInfo *session, http_IO *io)
{
    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    if (session) {
        json.StartObject();
        json.Key("userid"); json.Int64(session->userid);
        json.Key("username"); json.String(session->username);
        json.EndObject();
    } else {
        json.Null();
    }

    json.Finish();
}

void HandleUserSession(http_IO *io)
{
    RetainPtr<SessionInfo> session = sessions.Find(io);
    ExportSession(session.GetRaw(), io);
}

void HandleUserLogin(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    // Parse input data
    const char *mail = nullptr;
    const char *password = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "mail") {
                parser.ParseString(&mail);
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

    // Check missing or invalid values
    {
        bool valid = true;

        if (!mail || !IsMailValid(mail)) {
            LogError("Missing or invalid mail address");
            valid = false;
        }
        if (!password) {
            LogError("Missing password");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    // We use this to extend/fix the response delay in case of error
    int64_t start = GetMonotonicTime();

    sq_Statement stmt;
    if (!db.Prepare("SELECT id, password_hash, username FROM users WHERE mail = ?1", &stmt, mail))
        return;
    stmt.Run();

    // Validate password if user exists
    if (stmt.IsRow() && CountEvents(request.client_addr, mail) < BanThreshold) {
        int64_t userid = sqlite3_column_int64(stmt, 0);
        const char *password_hash = (const char *)sqlite3_column_text(stmt, 1);
        const char *username = (const char *)sqlite3_column_text(stmt, 2);

        if (password_hash && crypto_pwhash_str_verify(password_hash, password, strlen(password)) == 0) {
            RetainPtr<SessionInfo> session = CreateUserSession(userid, username);
            sessions.Open(io, session);

            ExportSession(session.GetRaw(), io);
            return;
        } else {
            RegisterEvent(request.client_addr, username);
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
    const http_RequestInfo &request = io->Request();

    // Parse input data
    const char *mail = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "mail") {
                parser.ParseString(&mail);
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

    // Check missing or invalid values
    {
        bool valid = true;

        if (!mail || !IsMailValid(mail)) {
            LogError("Missing or invalid mail address");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    if (RegisterEvent(request.client_addr, mail) >= BanThreshold) {
        LogError("You are blocked for %1 minutes after excessive recoveries", (BanTime + 59000) / 60000);
        io->SendError(403);
        return;
    }

    int64_t userid = -1;
    uint8_t token[16];

    // Always create it to reduce timing discloure
    FillRandomSafe(token, RG_SIZE(token));

    // Find user
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT id FROM users WHERE mail = ?1", &stmt, mail))
            return;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("User does not exist");
                io->SendError(404);
            }

            return;
        }

        userid = sqlite3_column_int64(stmt, 0);
    }

    // Create recovery token
    if (userid > 0) {
        int64_t now = GetUnixTime();

        if (!db.Run(R"(INSERT INTO tokens (token, timestamp, user)
                       VALUES (?1, ?2, ?3))", sq_Binding(token), now, userid))
            return;

        // XXX: Use a proper task queue for mails!
        if (!SendResetMail(mail, token, io->Allocator()))
            return;
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
    // Parse input data
    const char *token = nullptr;
    const char *password = nullptr;
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
            } else if (key == "password") {
                parser.SkipNull() || parser.ParseString(&password);
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

    // Check missing or invalid values
    {
        bool valid = true;

        if (!token) {
            LogError("Missing token");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    int64_t userid = -1;
    const char *username = nullptr;

    // Validate token
    {
        int64_t now = GetUnixTime();

        sq_Statement stmt;
        if (!db.Prepare(R"(SELECT t.timestamp, t.user, u.username
                           FROM tokens t
                           INNER JOIN users u ON (u.id = t.user)
                           WHERE t.token = uuid_blob(?1))", &stmt, token))
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
            if (!db.Run("UPDATE users SET password_hash = ?2 WHERE id = ?1", userid, hash))
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
    RetainPtr<SessionInfo> session = sessions.Find(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    // Parse input data
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

    // Check missing or invalid values
    {
        bool valid = true;

        if (!old_password) {
            LogError("Missing old_password value");
            valid = false;
        }
        if (!new_password) {
            LogError("Missing new_password value");
            valid = false;
        }

        if (!valid) {
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
        if (!db.Prepare("SELECT password_hash FROM users WHERE id = ?1", &stmt))
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
        } else {
            if (password_hash && crypto_pwhash_str_verify(password_hash, new_password, strlen(new_password)) == 0) {
                LogError("You cannot reuse the same password");
                io->SendError(422);
                return;
            }
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

}
