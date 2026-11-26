// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "config.hh"
#include "domain.hh"
#include "goupile.hh"
#include "message.hh"
#include "lib/native/http/http.hh"
#include "lib/native/request/sms.hh"
#include "lib/native/request/smtp.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace K {

static const int RetryDelay = 1 * 60000; // 1 minute
static const int DropDelay = 20 * 60000; // 20 minutes
static const int MaxErrors = 4;

static smtp_Sender smtp;
static sms_Sender sms;

bool InitSMTP(const smtp_Config &config)
{
    return smtp.Init(config);
}

bool InitSMS(const sms_Config &config)
{
    return sms.Init(config);
}

bool IsMailValid(Span<const char> address)
{
    const auto test_char = [](char c) { return strchr("<>& ", c) || IsAsciiControl(c); };

    Span<const char> domain;
    Span<const char> prefix = SplitStr(address, '@', &domain);

    if (!prefix.len || !domain.len)
        return false;
    if (std::any_of(prefix.begin(), prefix.end(), test_char))
        return false;
    if (std::any_of(domain.begin(), domain.end(), test_char))
        return false;

    return true;
}

bool IsPhoneValid(Span<const char> number)
{
    if (number.len < 3 || number.len > 16)
        return false;

    if (number[0] != '+')
        return false;
    if (number[1] < '1' || number[1] > '9')
        return false;
    if (!std::all_of(number.begin() + 2, number.end(), IsAsciiDigit))
        return false;

    return true;
}

bool PostMail(const char *to, const smtp_MailContent &content)
{
    BlockAllocator temp_alloc;

    const char *from = smtp.GetConfig().from;
    Span<const char> mail = smtp_BuildMail(from, to, content, &temp_alloc);
    int64_t now = GetUnixTime();

    if (!gp_db.Run(R"(INSERT INTO msg_mails (address, mail, ctime, errors, timestamp)
                      VALUES (?1, ?2, ?3, 0, ?3))", to, mail, now))
        return false;

    // Run pending tasks
    PostWaitMessage();

    return true;
}

bool PostSMS(const char *to, const char *message)
{
    K_ASSERT(IsPhoneValid(to));

    sms_Sender sms;
    if (!sms.Init(gp_config.sms))
        return false;
    return sms.Send(to, message);
}

bool SendMails()
{
    BlockAllocator temp_alloc;

    int64_t now = GetUnixTime();

    sq_Statement stmt;
    if (!gp_db.Prepare(R"(SELECT id, address, mail, ctime, errors, timestamp
                          FROM msg_mails
                          WHERE timestamp <= ?1)", &stmt, now))
        return false;

    Async async;

    while (stmt.Step()) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        const char *to = (const char *)sqlite3_column_text(stmt, 1);
        Span<const char> mail = MakeSpan((const char *)sqlite3_column_text(stmt, 2),
                                         sqlite3_column_bytes(stmt, 2));
        int64_t ctime = sqlite3_column_int64(stmt, 3);
        int64_t errors = sqlite3_column_int64(stmt, 4);
        int64_t timestamp = sqlite3_column_int64(stmt, 5);

        // Commit send task
        {
            int64_t next = now + RetryDelay;

            sq_Statement stmt;
            if (!gp_db.Prepare(R"(UPDATE msg_mails SET timestamp = ?3
                                  WHERE id = ?1 AND timestamp = ?2
                                  RETURNING id)",
                            &stmt, id, timestamp, next))
                return false;

            if (!stmt.Step()) {
                if (!stmt.IsValid())
                    return false;
                continue;
            }
        }

        to = DuplicateString(to, &temp_alloc).ptr;
        mail = DuplicateString(mail, &temp_alloc);

        async.Run([=]() {
            if (!smtp.Send(to, mail)) {
                bool persist = (errors + 1) < MaxErrors;

                if (now - ctime >= DropDelay) {
                    if (!gp_db.Run("DELETE FROM msg_mails WHERE id = ?1", id))
                        return false;
                } else {
                    int64_t next = persist ? (errors + 1) : 0;

                    if (!gp_db.Run("UPDATE msg_mails SET errors = ?2 WHERE id = ?1", id, next))
                        return false;
                }

                return persist;
            }

            if (!gp_db.Run("DELETE FROM msg_mails WHERE id = ?1", id))
                return false;

            return true;
        });
    }
    if (!stmt.IsValid())
        return false;

    stmt.Finalize();

    return async.Sync();
}

bool SendSMS()
{
    BlockAllocator temp_alloc;

    int64_t now = GetUnixTime();

    sq_Statement stmt;
    if (!gp_db.Prepare(R"(SELECT id, phone, message, ctime, errors, timestamp
                          FROM msg_texts
                          WHERE timestamp <= ?1)", &stmt, now))
        return false;

    Async async;

    while (stmt.Step()) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        const char *phone = (const char *)sqlite3_column_text(stmt, 1);
        Span<const char> message = MakeSpan((const char *)sqlite3_column_text(stmt, 2),
                                            sqlite3_column_bytes(stmt, 2));
        int64_t ctime = sqlite3_column_int64(stmt, 3);
        int64_t errors = sqlite3_column_int64(stmt, 4);
        int64_t timestamp = sqlite3_column_int64(stmt, 5);

        // Commit send task
        {
            int64_t next = now + RetryDelay;

            sq_Statement stmt;
            if (!gp_db.Prepare(R"(UPDATE msg_texts SET timestamp = ?3
                                  WHERE id = ?1 AND timestamp = ?2
                                  RETURNING id)",
                            &stmt, id, timestamp, next))
                return false;

            if (!stmt.Step()) {
                if (!stmt.IsValid())
                    return false;
                continue;
            }
        }

        phone = DuplicateString(phone, &temp_alloc).ptr;
        message = DuplicateString(message, &temp_alloc);

        async.Run([=]() {
            if (!sms.Send(phone, message)) {
                bool persist = (errors + 1) < MaxErrors;

                if (now - ctime >= DropDelay) {
                    if (!gp_db.Run("DELETE FROM msg_texts WHERE id = ?1", id))
                        return false;
                } else {
                    int64_t next = persist ? (errors + 1) : 0;

                    if (!gp_db.Run("UPDATE msg_texts SET errors = ?2 WHERE id = ?1", id, next))
                        return false;
                }

                return persist;
            }

            if (!gp_db.Run("DELETE FROM msg_texts WHERE id = ?1", id))
                return false;

            return true;
        });
    }
    if (!stmt.IsValid())
        return false;

    stmt.Finalize();

    return async.Sync();
}

void HandleSendMail(http_IO *io, InstanceHolder *instance)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (instance) {
        if (!session->HasPermission(instance, UserPermission::MessageMail)) {
            LogError("User is not allowed to send messages");
            io->SendError(403);
            return;
        }
    } else if (!session->IsAdmin()) {
        if (session->admin_until) {
            LogError("Admin user needs to confirm identity");
            io->SendError(401);
        } else {
            LogError("Non-admin users are not allowed to send mails");
            io->SendError(403);
        }
        return;
    }

    const char *to = nullptr;
    smtp_MailContent content;
    {
        bool success = http_ParseJson(io, Kibibytes(256), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "to") {
                    json->ParseString(&to);
                } else if (key == "subject") {
                    json->ParseString(&content.subject);
                } else if (key == "text") {
                    json->ParseString(&content.text);
                } else if (key == "html") {
                    json->ParseString(&content.html);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!to || !strchr(to, '@')) {
                    LogError("Missing or invalid 'to' parameter");
                    valid = false;
                }
                if (!content.subject.len && !content.text.len && !content.html.len) {
                    LogError("Missing 'subject', 'text' and 'html' parameters");
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

    if (!PostMail(to, content))
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleSendSMS(http_IO *io, InstanceHolder *instance)
{
    // Check configuration
    if (gp_config.sms.provider == sms_Provider::None) {
        LogError("This domain is not configured to send SMS messages");
        io->SendError(403);
        return;
    }

    RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (instance) {
        if (!session->HasPermission(instance, UserPermission::MessageText)) {
            LogError("User is not allowed to send messages");
            io->SendError(403);
            return;
        }
    } else if (!session->IsAdmin()) {
        if (session->admin_until) {
            LogError("Admin user needs to confirm identity");
            io->SendError(401);
        } else {
            LogError("Non-admin users are not allowed to send mails");
            io->SendError(403);
        }
        return;
    }

    const char *to = nullptr;
    const char *message = nullptr;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "to") {
                    json->ParseString(&to);
                } else if (key == "message") {
                    json->ParseString(&message);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!to || !to[0]) {
                    LogError("Missing or empty 'to' parameter");
                    valid = false;
                }
                if (!message) {
                    LogError("Missing 'message' parameter");
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

    if (!PostSMS(to, message))
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleSendTokenize(http_IO *io, InstanceHolder *instance)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io, instance);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }
    if (!session->HasPermission(instance, UserPermission::MessageMail) &&
            !session->HasPermission(instance, UserPermission::MessageText)) {
        LogError("User is not allowed to send messages");
        io->SendError(403);
        return;
    }

    Span<uint8_t> msg;
    {
        msg = AllocateSpan<uint8_t>(io->Allocator(), Kibibytes(8));

        StreamReader reader;
        if (!io->OpenForRead(msg.len, &reader))
            return;
        msg.len = reader.ReadFill(msg);
        if (msg.len < 0)
            return;
    }

    // Encode token
    Span<uint8_t> cypher;
    {
        cypher = AllocateSpan<uint8_t>(io->Allocator(), msg.len + crypto_box_SEALBYTES);

        if (crypto_box_seal((uint8_t *)cypher.ptr, msg.ptr, msg.len, instance->settings.token_pkey) != 0) {
            LogError("Failed to seal token");
            io->SendError(403);
            return;
        }
    }

    // Encode Base64
    Span<char> token;
    {
        token = AllocateSpan<char>(io->Allocator(), cypher.len * 2 + 1);

        sodium_bin2hex(token.ptr, (size_t)token.len, cypher.ptr, (size_t)cypher.len);

        token.len = (Size)strlen(token.ptr);
    }

    io->SendText(200, token);
}

}
