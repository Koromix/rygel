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
#include "src/core/request/smtp.hh"
#include "ludivine.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

static smtp_Sender smtp;

static const smtp_MailContent ExistingUser = {
    "Tentative de connexion à {{ TITLE }}",
    R"(Un utilisateur a tenté de se connecter sur votre compte :\n\n{{ EMAIL }})",
    R"(Un utilisateur a tenté de se connecter sur votre compte :<br><br><b>{{ EMAIL }}</b>)"
};
static const smtp_MailContent NewUser = {
    "Connexion à {{ TITLE }}",
    R"(Connexion à {{ TITLE }} :\n\n{{ APP_URL }}/#uuid={{ UUID }}&mk={{ KEY }})",
    R"(Connexion à {{ TITLE }} :<br><br><a href="{{ APP_URL }}/#uuid={{ UUID }}&mk={{ KEY }}">Lien de connexion</a>)"
};

bool InitSMTP(const smtp_Config &config)
{
    return smtp.Init(config);
}

static bool IsEmailValid(const char *email)
{
    const auto test_char = [](char c) { return strchr("<>& ", c) || (uint8_t)c < 32; };

    Span<const char> domain;
    Span<const char> prefix = SplitStr(email, '@', &domain);

    if (!prefix.len || !domain.len)
        return false;
    if (std::any_of(prefix.begin(), prefix.end(), test_char))
        return false;
    if (std::any_of(domain.begin(), domain.end(), test_char))
        return false;

    return true;
}

static Span<const char> PatchText(Span<const char> text, const char *uuid, const char *email, Span<const uint8_t> mkey, Allocator *alloc)
{
    RG_ASSERT(mkey.len == crypto_secretbox_KEYBYTES);

    Span<const char> ret = PatchFile(text, alloc, [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "TITLE") {
            writer->Write(config.title);
        } else if (key == "APP_URL") {
            writer->Write(config.app_url);
        } else if (key == "EMAIL") {
            writer->Write(email);
        } else if (key == "UUID") {
            writer->Write(uuid);
        } else if (key == "KEY") {
            const Size needed = sodium_base64_ENCODED_LEN(crypto_secretbox_KEYBYTES, sodium_base64_VARIANT_ORIGINAL_NO_PADDING);

            char base64[needed] = {};
            sodium_bin2base64(base64, RG_SIZE(base64), mkey.ptr, mkey.len, sodium_base64_VARIANT_URLSAFE_NO_PADDING);

            writer->Write(base64);
        } else {
            Print(writer, "{{%1}}", expr);
        }
    });

    return ret;
}

static bool SendMail(const char *to, const smtp_MailContent &model, const char *uuid, Span<const uint8_t> mkey, Allocator *alloc)
{
    smtp_MailContent content;

    content.subject = PatchText(model.subject, uuid, to, mkey, alloc).ptr;
    content.html = PatchText(model.html, uuid, to, mkey, alloc).ptr;
    content.text = PatchText(model.text, uuid, to, mkey, alloc).ptr;

    return smtp.Send(to, content);
}

void HandleUserRegister(http_IO *io)
{
    // Parse input data
    const char *email = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(4), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "email") {
                parser.ParseString(&email);
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

        if (!email || !IsEmailValid(email)) {
            LogError("Missing or invalid email");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    // Generate UUID and key
    uint8_t unique[16];
    uint8_t mkey[32];
    FillRandomSafe(unique);
    FillRandomSafe(mkey);
    static_assert(RG_SIZE(mkey) == crypto_secretbox_KEYBYTES);

    // Try to create user
    const char *uuid;
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(INSERT INTO users (id, email, valid)
                           VALUES (?1, ?2, 1)
                           ON CONFLICT DO NOTHING
                           RETURNING uuid_str(id))", &stmt, (Span<const uint8_t>)unique, email, 1))
            return;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                if (!SendMail(email, ExistingUser, uuid, mkey, io->Allocator()))
                    return;

                io->SendText(200, "{}", "application/json");
            }

            return;
        }

        uuid = (const char *)sqlite3_column_text(stmt, 0);
    }

    if (!SendMail(email, NewUser, uuid, mkey, io->Allocator()))
        return;

    io->SendText(200, "{}", "application/json");
}

}
