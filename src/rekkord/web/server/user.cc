// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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
#include "web.hh"
#include "mail.hh"
#include "user.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

#if defined(_WIN32)
    #include <io.h>
#endif

namespace RG {

static const int PasswordHashBytes = 128;

static const int64_t TokenDuration = 1800 * 1000;
static const int64_t InvalidTimeout = 86400 * 1000;

static const int64_t PictureCacheDelay = 3600 * 1000;
static const Size MaxPictureSize = Kibibytes(256);

static http_SessionManager<SessionInfo> sessions;

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
              font-weight: bold; text-decoration: none !important; color: white; text-transform: uppercase; text-wrap: nowrap;" href="{{ URL }}">Create account</a>
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
              font-weight: bold; text-decoration: none !important; color: white; text-transform: uppercase; text-wrap: nowrap;" href="{{ URL }}">Reset password</a>
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

static bool IsNameValid(Span<const char> name)
{
    const auto test_char = [](char c) { return IsAsciiAlphaOrDigit(c) || strchr("-_.", c); };

    if (!name.len)
        return false;
    if (!std::all_of(name.begin(), name.end(), test_char))
        return false;

    return true;
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

    return PostMail(to, content);
}

static bool SendExistingMail(const char *to, Allocator *alloc)
{
    smtp_MailContent content;

    content.subject = PatchText(ExistingUser.subject, to, nullptr, alloc).ptr;
    content.html = PatchText(ExistingUser.html, to, nullptr, alloc).ptr;
    content.text = PatchText(ExistingUser.text, to, nullptr, alloc).ptr;

    return PostMail(to, content);
}

static bool SendResetMail(const char *to, const uint8_t token[16], Allocator *alloc)
{
    smtp_MailContent content;

    const char *uuid = FormatUUID(token, alloc);
    const char *url = Fmt(alloc, "%1/reset#token=%2", config.url, uuid).ptr;

    content.subject = PatchText(ResetPassword.subject, to, url, alloc).ptr;
    content.html = PatchText(ResetPassword.html, to, url, alloc).ptr;
    content.text = PatchText(ResetPassword.text, to, url, alloc).ptr;

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

static RetainPtr<SessionInfo> CreateUserSession(int64_t userid, const char *username, int picture)
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
    session->picture = picture;
    CopyString(username, MakeSpan((char *)session->username, username_bytes));

    return ptr;
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

RetainPtr<const SessionInfo> GetNormalSession(http_IO *io)
{
    RetainPtr<const SessionInfo> session = sessions.Find(io);
    return session;
}

bool ValidateApiKey(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    const char *key = request.GetHeaderValue("X-Api-Key");

    if (!key) {
        LogError("Missing API key");
        io->SendError(401);
        return false;
    }

    // We use this to extend/fix the response delay in case of error
    int64_t start = GetMonotonicTime();

    Span<const char> secret;
    Span<const char> id = SplitStr(key, '/', &secret);

    const char *hash;
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT hash FROM keys WHERE key = ?1", &stmt, id))
            return false;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Invalid API key");
                io->SendError(403);
            }
            return false;
        }

        hash = (const char *)sqlite3_column_text(stmt, 0);
    }

    if (crypto_pwhash_str_verify(hash, secret.ptr, (size_t)secret.len) < 0) {
        int64_t safety = std::max(2000 - GetMonotonicTime() + start, (int64_t)0);
        WaitDelay(safety);

        LogError("Invalid API key");
        io->SendError(403);
        return false;
    }

    return true;
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
        json.Key("picture"); json.Int(session->picture);
        json.EndObject();
    } else {
        json.Null();
    }

    json.Finish();
}

void HandleUserSession(http_IO *io)
{
    RetainPtr<const SessionInfo> session = sessions.Find(io);
    ExportSession(session.GetRaw(), io);
}

void HandleUserPing(http_IO *io)
{
    // Do this to renew session and clear invalid session cookies
    sessions.Find(io);

    io->SendText(200, "{}", "application/json");
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
        if (!db.Prepare(R"(INSERT INTO users (mail, username, creation, version)
                           VALUES (?1, ?2, ?3, 1)
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

void HandleUserLogin(http_IO *io)
{
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
    if (!db.Prepare(R"(SELECT id, password_hash, username, version
                       FROM users
                       WHERE mail = ?1)", &stmt, mail))
        return;
    stmt.Run();

    // Validate password if user exists
    if (stmt.IsRow()) {
        int64_t userid = sqlite3_column_int64(stmt, 0);
        const char *password_hash = (const char *)sqlite3_column_text(stmt, 1);
        const char *username = (const char *)sqlite3_column_text(stmt, 2);
        int picture = sqlite3_column_int(stmt, 3);

        if (password_hash && crypto_pwhash_str_verify(password_hash, password, strlen(password)) == 0) {
            RetainPtr<SessionInfo> session = CreateUserSession(userid, username, picture);
            sessions.Open(io, session);

            ExportSession(session.GetRaw(), io);
            return;
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
    RetainPtr<const SessionInfo> session = sessions.Find(io);

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

static void SendDefaultPicture(http_IO *io)
{
#if defined(FELIX_HOT_ASSETS)
    const AssetInfo *DefaultPicture = FindEmbedAsset("src/rekkord/web/assets/ui/anonymous.png");
    RG_ASSERT(DefaultPicture);
#else
    static const AssetInfo *DefaultPicture = FindEmbedAsset("src/rekkord/web/assets/ui/anonymous.png");
    RG_ASSERT(DefaultPicture);
#endif

    io->AddEncodingHeader(DefaultPicture->compression_type);
    io->SendBinary(200, DefaultPicture->data, "image.png");
}

void HandlePictureGet(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    int64_t userid = -1;
    bool explicit_user = false;

    if (StartsWith(request.path, "/pictures/")) {
        RG_ASSERT(StartsWith(request.path, "/pictures/"));
        Span<const char> str = request.path + 10;

        if (!ParseInt(str, &userid)) {
            io->SendError(422);
            return;
        }

        explicit_user = true;
    } else {
        RetainPtr<const SessionInfo> session = sessions.Find(io);

        if (!session) {
            LogError("User is not logged in");
            io->SendError(404);
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
    RG_DEFER { sqlite3_blob_close(blob); };

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
    RetainPtr<SessionInfo> session = sessions.Find(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(404);
        return;
    }

    // Create temporary file
    int fd = -1;
    const char *tmp_filename = CreateUniqueFile(config.tmp_directory, nullptr, ".tmp", io->Allocator(), &fd);
    if (!tmp_filename)
        return;
    RG_DEFER {
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
        RG_DEFER { sqlite3_blob_close(blob); };

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

    const char *response = Fmt(io->Allocator(), "{ \"picture\": %1 }", session->picture.load()).ptr;
    io->SendText(200, response, "application/json");
}

void HandlePictureDelete(http_IO *io)
{
    RetainPtr<SessionInfo> session = sessions.Find(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(404);
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

    const char *response = Fmt(io->Allocator(), "{ \"picture\": %1 }", session->picture.load()).ptr;
    io->SendText(200, response, "application/json");
}

void HandleKeyList(http_IO *io)
{
    RetainPtr<const SessionInfo> session = sessions.Find(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT id, title, key FROM keys WHERE owner = ?1)", &stmt, session->userid))
        return;

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartArray();
    while (stmt.Step()) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        const char *title = (const char *)sqlite3_column_text(stmt, 1);
        const char *key = (const char *)sqlite3_column_text(stmt, 2);

        json.StartObject();

        json.Key("id"); json.Int64(id);
        json.Key("title"); json.String(title);
        json.Key("key"); json.String(key);

        json.EndObject();
    }
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish();
}

void HandleKeyCreate(http_IO *io)
{
    RetainPtr<SessionInfo> session = sessions.Find(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    // Parse input data
    const char *title = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "title") {
                parser.ParseString(&title);
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

        if (!IsNameValid(title)) {
            LogError("Missing or invalid key title");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    char key[33];
    char secret[33];
    {
        unsigned int flags = (int)pwd_GenerateFlag::Uppers |
                             (int)pwd_GenerateFlag::Lowers |
                             (int)pwd_GenerateFlag::Digits;

        pwd_GeneratePassword(flags, key);
        pwd_GeneratePassword(flags, secret);
    }

    char hash[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hash, secret, strlen(secret),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        LogError("Failed to hash secret");
        return;
    }

    int64_t id = -1;

    // Insert new key into database
    bool success = db.Transaction([&]() {
        sq_Statement stmt;
        if (!db.Prepare(R"(INSERT INTO keys (owner, title, key, hash)
                           VALUES (?1, ?2, ?3, ?4)
                           RETURNING id)", &stmt, session->userid, title, key, hash))
            return false;
        if (!stmt.Step()) {
            RG_ASSERT(!stmt.IsValid());
            return false;
        }

        id = sqlite3_column_int64(stmt, 0);
        return true;
    });
    if (!success)
        return;

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartObject();
    json.Key("id"); json.Int64(id);
    json.Key("key"); json.String(key);
    json.Key("secret"); json.String(secret);
    json.EndObject();

    json.Finish();
}

void HandleKeyDelete(http_IO *io)
{
    RetainPtr<SessionInfo> session = sessions.Find(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    // Parse input data
    int64_t id = -1;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "id") {
                parser.ParseInt(&id);
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

        if (id <= 0) {
            LogError("Missing or invalid key ID");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    sq_Statement stmt;
    if (!db.Prepare(R"(DELETE FROM keys
                       WHERE id = ?1 AND owner = ?2
                       RETURNING id)", &stmt, id, session->userid))
        return;
    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("Unknown key ID %1", id);
            io->SendError(404);
        }
        return;
    }

    io->SendText(200, "{}", "application/json");
}

}
