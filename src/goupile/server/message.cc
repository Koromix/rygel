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
#include "config.hh"
#include "domain.hh"
#include "goupile.hh"
#include "message.hh"
#include "src/core/http/http.hh"
#include "src/core/request/sms.hh"
#include "src/core/request/smtp.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace K {

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

bool SendMail(const char *to, const smtp_MailContent &content)
{
    K_ASSERT(IsMailValid(to));

    DomainHolder *domain = RefDomain();
    K_DEFER { UnrefDomain(domain); };

    smtp_Sender smtp;
    if (!smtp.Init(domain->settings.smtp))
        return false;
    return smtp.Send(to, content);
}

bool SendSMS(const char *to, const char *message)
{
    K_ASSERT(IsPhoneValid(to));

    sms_Sender sms;
    if (!sms.Init(gp_config.sms))
        return false;
    return sms.Send(to, message);
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
        if (!session->HasPermission(instance, UserPermission::MiscMail)) {
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
                if (!content.subject && !content.text && !content.html) {
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

    SendMail(to, content);

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
        if (!session->HasPermission(instance, UserPermission::MiscTexto)) {
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

    SendSMS(to, message);

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
    if (!session->HasPermission(instance, UserPermission::MiscMail) &&
            !session->HasPermission(instance, UserPermission::MiscTexto)) {
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
