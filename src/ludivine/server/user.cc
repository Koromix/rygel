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

namespace RG {

static smtp_Sender smtp;

static const smtp_MailContent NewUser = {
    "Connexion à {{ TITLE }} !",
    R"(Bienvenue !

Nous vous remercions de votre intérêt pour les recherches de {{ TITLE }}.

Attention, ce **mail est important** ! Il est l’**unique moyen de connexion** à votre espace personnel durant toute la durée des études.

Conservez-le **précieusement, ou même mieux, enregistrez la pièce jointe** sur votre ordinateur/téléphone/tablette. Celle-ci contient une copie du lien nécessaire à la connexion.

Nous vous invitons à utiliser le lien suivant afin de commencer votre aventure {{ TITLE }} :

{{ URL }}

Nous utilisons un système de chiffrement end-to-end pour assurer la sécurité et l’anonymat de vos données. Nous ne serons donc **pas en mesure de vous renvoyer un nouveau lien de connexion** en cas de perte de celui-ci.

Nous vous sommes très reconnaissants de votre implication dans la recherche sur les psychotraumatismes.

L’équipe de {{ TITLE }}
contact@ldv-recherche.fr)",
    R"(<html><body>
<p>Bienvenue !</p>
<p>Nous vous remercions de votre intérêt pour les recherches de {{ TITLE }}.</p>
<p><b>Attention, ce <b>mail est important</b> ! Il est l’unique moyen de connexion à votre espace personnel durant toute la durée des études.</b></p>
<p><b>Conservez-le précieusement, ou même mieux, enregistrez la pièce jointe sur votre ordinateur/téléphone/tablette. Celle-ci contient une copie du lien nécessaire à la connexion.</b></p>
<p>Nous vous invitons à cliquer sur le lien suivant afin de commencer votre aventure {{ TITLE }}.</p>
<div align="center"><br>
    <a style="padding: 0.35em 2em 0.45em 2em; background: #2d8261; border-radius: 30px;
              font-weight: bold; text-decoration: none !important; color: white;" href="{{ URL }}">Lien de connexion</a>
<br><br></div>
<p>Si vous rencontrez des problèmes pour appuyer sur le bouton, copiez-collez le lien suivant dans votre navigateur :</p>
<p>{{ URL }}</p>
<p>Nous utilisons un système de chiffrement end-to-end pour assurer la sécurité et l’anonymat de vos données. Nous ne serons donc <b>pas en mesure de vous renvoyer un nouveau lien de connexion</b> en cas de perte de celui-ci.</p>
<p>Nous vous sommes très reconnaissants de votre implication dans la recherche sur les psychotraumatismes.</p>
<p><i>L’équipe de {{ TITLE }}</i><br>
<a href="mailto:contact@ldv-recherche.fr">contact@ldv-recherche.fr</a></p>
</body></html>)"
};

static const smtp_MailContent ExistingUser = {
    "Tentative de connexion à {{ TITLE }}",
    R"(Un utilisateur a tenté de se connecter sur votre compte :\n\n{{ EMAIL }})",
    R"(Un utilisateur a tenté de se connecter sur votre compte :<br><br><b>{{ EMAIL }}</b>)"
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

static Span<const char> PatchText(Span<const char> text, const char *email, const char *url, Allocator *alloc)
{
    Span<const char> ret = PatchFile(text, alloc, [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "TITLE") {
            writer->Write(config.title);
        } else if (key == "EMAIL") {
            writer->Write(email);
        } else if (key == "URL") {
            writer->Write(url);
        } else {
            Print(writer, "{{%1}}", expr);
        }
    });

    return ret;
}

static bool SendMail(const char *to, const smtp_MailContent &model,
                     const char *uid, Span<const uint8_t> tkey, int registration, Allocator *alloc)
{
    smtp_MailContent content;

    // Format magic link
    FmtArg fmt = FmtSpan(tkey, FmtType::BigHex, "").Pad0(-2);
    const char *url = Fmt(alloc, "%1/session#uid=%2&tk=%3&r=%4", config.url, uid, fmt, registration).ptr;

    content.subject = PatchText(model.subject, to, url, alloc).ptr;
    content.html = PatchText(model.html, to, url, alloc).ptr;
    content.text = PatchText(model.text, to, url, alloc).ptr;

    return smtp.Send(to, content);
}

void HandleUserRegister(http_IO *io)
{
    // Parse input data
    const char *email = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
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

    // Try to create user
    uint8_t tkey[32];
    {
        uint8_t uid[16];

        FillRandomSafe(uid);
        FillRandomSafe(tkey);

        sq_Statement stmt;
        if (!db.Run(R"(INSERT INTO users (uid, email, registration)
                       VALUES (?1, ?2, 1)
                       ON CONFLICT (email) DO UPDATE SET registration = registration + IIF(token IS NULL, 1, 0))",
                    (Span<const uint8_t>)uid, email))
            return;
    }

    // Retrieve user information
    const char *uid = nullptr;
    int registration = 0;
    {
        sq_Statement stmt;

        if (!db.Prepare(R"(SELECT IIF(token IS NULL, 1, 0), uuid_str(uid), registration
                           FROM users
                           WHERE email = ?1)",
                        &stmt, email))
            return;

        if (!stmt.Step()) {
            if (!stmt.IsValid()) [[unlikely]] {
                LogError("Unexpected missing user (parallel delete?)");
            }
            return;
        }

        bool valid = sqlite3_column_int(stmt, 0);

        uid = (const char *)sqlite3_column_text(stmt, 1);
        registration = sqlite3_column_int(stmt, 2);

        if (!valid) {
            if (!SendMail(email, ExistingUser, uid, {}, 0, io->Allocator()))
                return;

            io->SendText(200, "{}", "application/json");
            return;
        }

        uid = DuplicateString(uid, io->Allocator()).ptr;
    }

    if (!SendMail(email, NewUser, uid, tkey, registration, io->Allocator()))
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleUserLogin(http_IO *io)
{
    // Parse input data
    const char *uid = nullptr;
    const char *token = nullptr;
    int registration = 0;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "uid") {
                parser.ParseString(&uid);
            } else if (key == "token") {
                parser.PassThrough(&token);
            } else if (key == "registration") {
                parser.ParseInt(&registration);
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

        if (!uid) {
            LogError("Missing or invalid UID");
            valid = false;
        }
        if (!token) {
            LogError("Missing or invalid initial token");
            valid = false;
        }
        if (registration <= 0) {
            LogError("Missing or invalid registration value");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    // Retrieve user token
    {
        sq_Statement stmt;

        if (!db.Prepare("SELECT id, token, registration FROM users WHERE uid = uuid_blob(?1)",
                        &stmt, uid))
            return;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Unknown user UID");
                io->SendError(404);
            }
            return;
        }

        int64_t id = sqlite3_column_int64(stmt, 0);
        bool exists = (sqlite3_column_type(stmt, 1) != SQLITE_NULL);
        int count = sqlite3_column_int(stmt, 2);

        if (registration != count) {
            LogError("Please use most recent login email");
            io->SendError(409);
            return;
        }

        if (exists) {
            token = (const char *)sqlite3_column_text(stmt, 1);
            token = DuplicateString(token, io->Allocator()).ptr;
        } else {
            if (!db.Run("UPDATE users SET token = ?2 WHERE id = ?1", id, token))
                return;
        }
    }

    io->SendText(200, token, "application/json");
}

static bool IsUUIDValid(Span<const char> uuid)
{
    const auto test_char = [](char c) { return IsAsciiDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); };

    if (uuid.len != 36)
        return false;
    if (!std::all_of(uuid.ptr + 0, uuid.ptr + 8, test_char) || uuid[8] != '-')
        return false;
    if (!std::all_of(uuid.ptr + 9, uuid.ptr + 13, test_char) || uuid[13] != '-')
        return false;
    if (!std::all_of(uuid.ptr + 14, uuid.ptr + 18, test_char) || uuid[18] != '-')
        return false;
    if (!std::all_of(uuid.ptr + 19, uuid.ptr + 23, test_char) || uuid[23] != '-')
        return false;
    if (!std::all_of(uuid.ptr + 24, uuid.ptr + 36, test_char))
        return false;

    return true;
}

void HandleUserDownload(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    // Get and check vault ID
    const char *vid;
    {
        vid = request.GetHeaderValue("X-VaultID");

        if (!vid || !IsUUIDValid(vid)) {
            LogError("Missing or invalid VID");

            io->SendError(422);
            return;
        }
    }

    const char *filename = Fmt(io->Allocator(), "%1%/%2", config.vault_directory, vid).ptr;

    // Check file access and size
    FileInfo file_info;
    {
        StatResult stat = StatFile(filename, &file_info);

        switch (stat) {
            case StatResult::Success: {} break;

            case StatResult::MissingPath: {
                io->SendError(404);
                return;
            } break;

            case StatResult::AccessDenied:
            case StatResult::OtherError: return;
        }
    }

    // Send the file
    int fd = OpenFile(filename, (int)OpenFlag::Read);
    if (fd < 0)
        return;
    io->SendFile(200, fd, file_info.size);
}

void HandleUserUpload(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    // Get and check vault ID
    const char *vid;
    {
        vid = request.GetHeaderValue("X-VaultID");

        if (!vid || !IsUUIDValid(vid)) {
            LogError("Missing or invalid VID");

            io->SendError(422);
            return;
        }
    }

    const char *filename = Fmt(io->Allocator(), "%1%/%2", config.vault_directory, vid).ptr;

    // Create temporary file
    int fd = -1;
    const char *tmp_filename = CreateUniqueFile(config.tmp_directory, nullptr, ".tmp", io->Allocator(), &fd);
    if (!tmp_filename)
        return;
    RG_DEFER {
        CloseDescriptor(fd);
        UnlinkFile(tmp_filename);
    };

    // Read file content
    {
        StreamWriter writer(fd, "<temp>");
        StreamReader reader;
        if (!io->OpenForRead(Mebibytes(8), &reader))
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

    // Commit new file
    {
        unsigned int flags = (int)RenameFlag::Overwrite | (int)RenameFlag::Sync;

        if (!RenameFile(tmp_filename, filename, flags))
            return;
    }

    io->SendText(200, "{}", "application/json");
}

}
