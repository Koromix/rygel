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

#include "src/core/libcc/libcc.hh"
#include "domain.hh"
#include "goupile.hh"
#include "message.hh"
#include "src/core/libnet/libnet.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

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

bool SendMail(const char *to, const smtp_MailContent &content)
{
    return smtp.Send(to, content);
}

bool SendSMS(const char *to, const char *message)
{
    return sms.Send(to, message);
}

void HandleSendMail(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    if (!gp_domain.config.smtp.url) {
        LogError("This instance is not configured to send mails");
        io->AttachError(403);
        return;
    }

    RetainPtr<const SessionInfo> session = GetNormalSession(instance, request, io);

    if (!session) {
        LogError("User is not logged in");
        io->AttachError(401);
        return;
    }
    if (instance) {
        if (!session->HasPermission(instance, UserPermission::MiscMail)) {
            LogError("User is not allowed to send messages");
            io->AttachError(403);
            return;
        }
    } else if (!session->IsAdmin()) {
        if (session->admin_until) {
            LogError("Admin user needs to confirm identity");
            io->AttachError(401);
        } else {
            LogError("Non-admin users are not allowed to send mails");
            io->AttachError(403);
        }
        return;
    }

    io->RunAsync([=]() {
        const char *to = nullptr;
        smtp_MailContent content;
        {
            StreamReader st;
            if (!io->OpenForRead(Kibibytes(256), &st))
                return;
            json_Parser parser(&st, &io->allocator);

            parser.ParseObject();
            while (parser.InObject()) {
                Span<const char> key = {};
                parser.ParseKey(&key);

                if (key == "to") {
                    parser.ParseString(&to);
                } else if (key == "subject") {
                    parser.ParseString(&content.subject);
                } else if (key == "text") {
                    parser.ParseString(&content.text);
                } else if (key == "html") {
                    parser.ParseString(&content.html);
                } else if (parser.IsValid()) {
                    LogError("Unexpected key '%1'", key);
                    io->AttachError(422);
                    return;
                }
            }
            if (!parser.IsValid()) {
                io->AttachError(422);
                return;
            }
        }

        // Check for missing of invalid values
        {
            bool valid = true;

            if (!to || !strchr(to, '@')) {
                LogError("Missing or invalid 'to' parameter");
                valid = false;
            }
            if (!content.subject && !content.text && !content.html) {
                LogError("Missing 'subject', 'text' and 'html' parameters");
                valid = false;
            }

            if (!valid) {
                io->AttachError(422);
                return;
            }
        }

        if (!SendMail(to, content))
            return;

        io->AttachText(200, "{}", "application/json");
    });
}

void HandleSendSMS(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    if (gp_domain.config.sms.provider == sms_Provider::None) {
        LogError("This instance is not configured to send SMS messages");
        io->AttachError(403);
        return;
    }

    RetainPtr<const SessionInfo> session = GetNormalSession(instance, request, io);

    if (!session) {
        LogError("User is not logged in");
        io->AttachError(401);
        return;
    }
    if (instance) {
        if (!session->HasPermission(instance, UserPermission::MiscTexto)) {
            LogError("User is not allowed to send messages");
            io->AttachError(403);
            return;
        }
    } else if (!session->IsAdmin()) {
        if (session->admin_until) {
            LogError("Admin user needs to confirm identity");
            io->AttachError(401);
        } else {
            LogError("Non-admin users are not allowed to send mails");
            io->AttachError(403);
        }
        return;
    }

    io->RunAsync([=]() {
        const char *to = nullptr;
        const char *message = nullptr;
        {
            StreamReader st;
            if (!io->OpenForRead(Kibibytes(1), &st))
                return;
            json_Parser parser(&st, &io->allocator);

            parser.ParseObject();
            while (parser.InObject()) {
                Span<const char> key = {};
                parser.ParseKey(&key);

                if (key == "to") {
                    parser.ParseString(&to);
                } else if (key == "message") {
                    parser.ParseString(&message);
                } else if (parser.IsValid()) {
                    LogError("Unexpected key '%1'", key);
                    io->AttachError(422);
                    return;
                }
            }
            if (!parser.IsValid()) {
                io->AttachError(422);
                return;
            }
        }

        // Check missing values
        {
            bool valid = true;

            if (!to || !to[0]) {
                LogError("Missing or empty 'to' parameter");
                valid = false;
            }
            if (!message) {
                LogError("Missing 'message' parameter");
                valid = false;
            }

            if (!valid) {
                io->AttachError(422);
                return;
            }
        }

        if (!SendSMS(to, message))
            return;

        io->AttachText(200, "{}", "application/json");
    });
}

void HandleSendTokenize(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(instance, request, io);

    if (!session) {
        LogError("User is not logged in");
        io->AttachError(401);
        return;
    }
    if (!session->HasPermission(instance, UserPermission::MiscMail) &&
            !session->HasPermission(instance, UserPermission::MiscTexto)) {
        LogError("User is not allowed to send messages");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        Span<uint8_t> msg;
        {
            msg = AllocateSpan<uint8_t>(&io->allocator, Kibibytes(8));

            StreamReader reader;
            if (!io->OpenForRead(msg.len, &reader))
                return;
            msg.len = reader.Read(msg);
            if (msg.len < 0)
                return;
        }

        // Encode token
        Span<uint8_t> cypher;
        {
            cypher = AllocateSpan<uint8_t>(&io->allocator, msg.len + crypto_box_SEALBYTES);

            if (crypto_box_seal((uint8_t *)cypher.ptr, msg.ptr, msg.len, instance->config.token_pkey) != 0) {
                LogError("Failed to seal token");
                io->AttachError(403);
                return;
            }
        }

        // Encode Base64
        Span<char> token;
        {
            token = AllocateSpan<char>(&io->allocator, cypher.len * 2 + 1);

            sodium_bin2hex(token.ptr, (size_t)token.len, cypher.ptr, (size_t)cypher.len);

            token.len = (Size)strlen(token.ptr);
        }

        io->AttachText(200, token);
    });
}

}
