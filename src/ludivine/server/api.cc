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
#include "src/core/wrap/qrcode.hh"
#include "ludivine.hh"

namespace RG {

static smtp_Sender smtp;

static const smtp_MailContent NewUser = {
    "Connexion à {{ TITLE }} !",

    R"(Bienvenue !

Nous vous remercions de votre intérêt pour les recherches de {{ TITLE }}.

Attention, ce mail est important ! Il est l’unique moyen de connexion à votre espace personnel durant toute la durée des études.

Conservez-le précieusement, ou même mieux, enregistrez la pièce jointe sur votre ordinateur/téléphone/tablette. Celle-ci contient les informations nécessaires pour récupérer votre compte si vous perdez ce mail.

Nous vous invitons à utiliser le lien suivant afin de commencer votre aventure {{ TITLE }} :

{{ URL }}

Nous utilisons un système de chiffrement end-to-end pour assurer la sécurité et l’anonymat de vos données. Nous ne serons donc pas en mesure de vous renvoyer un nouveau lien de connexion en cas de perte de celui-ci.

Nous vous sommes très reconnaissants de votre implication dans la recherche sur les psychotraumatismes.

L’équipe de {{ TITLE }}
{{ CONTACT }})",

    R"(<html lang="fr"><body>
<p>Bienvenue !</p>
<p>Nous vous remercions de votre intérêt pour les recherches de {{ TITLE }}.</p>
<p><b>Attention, ce <b>mail est important</b> ! Il est l’unique moyen de connexion à votre espace personnel durant toute la durée des études.</b></p>
<p><b>Conservez-le précieusement, ou même mieux, enregistrez la pièce jointe sur votre ordinateur/téléphone/tablette. Celle-ci contient les informations nécessaires pour récupérer votre compte si vous perdez ce mail.</b></p>
<p>Nous vous invitons à cliquer sur le lien suivant afin de commencer votre aventure {{ TITLE }}.</p>
<div align="center"><br>
    <a style="padding: 0.7em 2em; background: #2d8261; border-radius: 30px;
              font-weight: bold; text-decoration: none !important; color: white; text-transform: uppercase; text-wrap: nowrap;" href="{{ URL }}">Connexion à {{ TITLE }}</a>
<br><br></div>
<p>Vous pouvez également utiliser ce QRcode pour vous connecter à l'aide de votre smartphone si vous le souhaitez :</p>
<div align="center"><br>
    <img src="cid:qrcode.png" alt="">
<br><br></div>
<p>Nous utilisons un système de chiffrement end-to-end pour assurer la sécurité et l’anonymat de vos données. Nous ne serons donc <b>pas en mesure de vous renvoyer un nouveau lien de connexion</b> en cas de perte de celui-ci.</p>
<p>Nous vous sommes très reconnaissants de votre implication dans la recherche sur les psychotraumatismes.</p>
<p><i>L’équipe de {{ TITLE }}</i><br>
<a href="mailto:{{ CONTACT }}">{{ CONTACT }}</a></p>
</body></html>)",

    {}
};

static const smtp_MailContent ExistingUser = {
    "Nouvelle connexion à {{ TITLE }}",
    R"(Bonjour,

Un compte {{ TITLE }} existe déjà avec cette adresse email.

Pour vous reconnecter, nous vous invitons à utiliser le lien de connexion initial reçu par mail lors de la création de votre compte. Vous l'avez peut-être même enregistré sur votre ordinateur/téléphone/tablette.

Pour rappel, nous utilisons un système de chiffrement complexe pour assurer la sécurité et l'anonymat de vos données. Nous ne sommes donc pas en mesure de vous renvoyer un nouveau lien de connexion en cas de perte de celui-ci.

Si vous rencontrez un problème, vous pouvez contacter l'équipe de {{ TITLE }} : {{ CONTACT }}

Nous vous sommes très reconnaissants de votre implication dans la recherche sur les psychotraumatismes.

L'équipe de {{ TITLE }}
{{ CONTACT }})",
    R"(<html lang="fr"><body>
<p>Bonjour,</p>
<p>Un compte {{ TITLE }} existe déjà avec cette adresse email.</p>
<p><b>Pour vous reconnecter, nous vous invitons à utiliser le lien de connexion initial reçu par mail lors de la création de votre compte. Vous l’avez peut-être même enregistré sur votre ordinateur/téléphone/tablette.</b></p>
<p>Pour rappel, nous utilisons un système de chiffrement complexe pour assurer la sécurité et l’anonymat de vos données. Nous ne sommes donc pas en mesure de vous renvoyer un nouveau lien de connexion en cas de perte de celui-ci.
<p>Si vous rencontrez un problème, vous pouvez contacter l’équipe de {{ TITLE }} : <a href="mailto:{{ CONTACT }}">{{ CONTACT }}</a></p>
<p>Nous vous sommes très reconnaissants de votre implication dans la recherche sur les psychotraumatismes.</p>
<p><i>L’équipe de {{ TITLE }}</i><br>
<a href="mailto:{{ CONTACT }}">{{ CONTACT }}</a></p>
</body></html>)",
    {}
};

struct EventInfo {
    LocalDate date = {};
    bool partial = false;
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

// Enforces lower-case UUIDs
static bool IsUUIDValid(Span<const char> uuid)
{
    const auto test_char = [](char c) { return IsAsciiDigit(c) || (c >= 'a' && c <= 'f'); };

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
        } else if (key == "CONTACT") {
            writer->Write(config.contact);
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

static bool SendNewMail(const char *to, const char *uid, Span<const uint8_t> tkey, int registration, Allocator *alloc)
{
    smtp_MailContent content;

    // Format magic link
    FmtArg fmt = FmtSpan(tkey, FmtType::BigHex, "").Pad0(-2);
    const char *url = Fmt(alloc, "%1/session#uid=%2&tk=%3&r=%4", config.url, uid, fmt, registration).ptr;

    content.subject = PatchText(NewUser.subject, to, url, alloc).ptr;
    content.html = PatchText(NewUser.html, to, url, alloc).ptr;
    content.text = PatchText(NewUser.text, to, url, alloc).ptr;

    Span<const uint8_t> png;
    {
        HeapArray<uint8_t> buf(alloc);

        StreamWriter st(&buf);
        if (!qr_EncodeTextToPng(url, 0, &st))
            return false;
        st.Close();

        png = buf.Leak();
    }

    const char *filename = Fmt(alloc, "Recuperation Session %1.txt", config.title).ptr;
    const char *careful = "Gardez ce fichier en sécurité, ne le divulgez pas ou vos données pourraient être compromises !";
    Span<char> attachment = Fmt(alloc, "Récupération de la connexion à %1\n\n===\n%2\n%3/%4\n===\n\n%5", config.title, uid, fmt, registration, careful);

    smtp_AttachedFile files[] = {
        { .mimetype = "image/png", .id = "qrcode.png", .inlined = true, .data = png },
        { .mimetype = "text/plain", .name = filename, .data = attachment.As<const uint8_t>() }
    };

    content.files = files;

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

void HandleRegister(http_IO *io)
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

    // Try to create user
    uint8_t tkey[32];
    {
        uint8_t uid[16];

        FillRandomSafe(uid);
        FillRandomSafe(tkey);

        sq_Statement stmt;
        if (!db.Run(R"(INSERT INTO users (uid, mail, registration)
                       VALUES (?1, ?2, 1)
                       ON CONFLICT (mail) DO UPDATE SET registration = registration + IIF(token IS NULL, 1, 0))",
                    (Span<const uint8_t>)uid, mail))
            return;
    }

    // Retrieve user information
    const char *uid = nullptr;
    int registration = 0;
    {
        sq_Statement stmt;

        if (!db.Prepare(R"(SELECT IIF(token IS NULL, 1, 0), uuid_str(uid), registration
                           FROM users
                           WHERE mail = ?1)",
                        &stmt, mail))
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
            if (!SendExistingMail(mail, io->Allocator()))
                return;

            io->SendText(200, "{}", "application/json");
            return;
        }

        uid = DuplicateString(uid, io->Allocator()).ptr;
    }

    if (!SendNewMail(mail, uid, tkey, registration, io->Allocator()))
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleLogin(http_IO *io)
{
    // Parse input data
    const char *uid = nullptr;
    const char *token = nullptr;
    const char *vid = nullptr;
    const char *rid = nullptr;
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
            } else if (key == "vid") {
                parser.ParseString(&vid);
            } else if (key == "rid") {
                parser.ParseString(&rid);
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

        if (!uid || !IsUUIDValid(uid)) {
            LogError("Missing or invalid UID");
            valid = false;
        }
        if (!token) {
            LogError("Missing or invalid initial token");
            valid = false;
        }
        if (!vid || !IsUUIDValid(vid)) {
            LogError("Missing or invalid initial VID");
            valid = false;
        }
        if (!rid || !IsUUIDValid(rid)) {
            LogError("Missing or invalid initial RID");
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
            LogError("Please use most recent login mail");
            io->SendError(409);
            return;
        }

        if (exists) {
            token = (const char *)sqlite3_column_text(stmt, 1);
            token = DuplicateString(token, io->Allocator()).ptr;
        } else {
            bool success = db.Transaction([&]() {
                if (!db.Run("UPDATE users SET token = ?2 WHERE id = ?1", id, token))
                    return false;

                if (!db.Run("INSERT INTO vaults (vid, generation) VALUES (uuid_blob(?1), 0)", vid))
                    return false;
                if (!db.Run("INSERT INTO participants (rid) VALUES (uuid_blob(?1))", rid))
                    return false;

                return true;
            });
            if (!success)
                return;
        }
    }

    io->SendText(200, token, "application/json");
}

static void AddGenerationHeaders(http_IO *io, int64_t generation, int64_t previous)
{
    char buf[64];

    Fmt(buf, "%1", generation);
    io->AddHeader("X-Vault-Generation", buf);

    Fmt(buf, "%1", previous);
    io->AddHeader("X-Vault-Previous", buf);
}

void HandleDownload(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    // Get and check vault ID
    const char *vid;
    {
        vid = request.GetHeaderValue("X-Vault-Id");

        if (!vid || !IsUUIDValid(vid)) {
            LogError("Missing or invalid VID");

            io->SendError(422);
            return;
        }
    }

    // Get vault generation
    int64_t generation;
    int64_t previous;
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT generation, previous FROM vaults WHERE vid = uuid_blob(?1)", &stmt, vid))
            return;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Unknown vault VID");
                io->SendError(404);
            }
            return;
        }

        generation = sqlite3_column_int64(stmt, 0);
        previous = sqlite3_column_int64(stmt, 1);

        if (!generation) {
            io->SendError(204);
            return;
        }
    }

    const char *filename = Fmt(io->Allocator(), "%1%/%2%/%3.bin", config.vault_directory, vid, generation).ptr;

    AddGenerationHeaders(io, generation, previous);

    // Send it!
    int fd = OpenFile(filename, (int)OpenFlag::Read);
    if (fd < 0)
        return;
    io->SendFile(200, fd);
}

void HandleUpload(http_IO *io)
{
    const http_RequestInfo &request = io->Request();

    const char *vid = nullptr;
    int64_t previous = -1;
    {
        vid = request.GetHeaderValue("X-Vault-Id");;

        if (const char *str = request.GetHeaderValue("X-Vault-Generation"); str) {
            ParseInt(str, &previous);
        }
    }

    // Check missing or invalid values
    {
        bool valid = true;

        if (!vid || !IsUUIDValid(vid)) {
            LogError("Missing or invalid VID");
            valid = false;
        }
        if (previous < 0) {
            LogError("Missing or invalid generation header");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    const char *directory = Fmt(io->Allocator(), "%1%/%2", config.vault_directory, vid).ptr;

    int64_t vault;
    int64_t generation;
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT id, generation FROM vaults WHERE vid = uuid_blob(?1)", &stmt, vid))
            return;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Unknown vault VID");
                io->SendError(404);
            }
            return;
        }

        vault = sqlite3_column_int64(stmt, 0);
        generation = sqlite3_column_int64(stmt, 1) + 1;
    }

    int fd = -1;
    RG_DEFER { CloseDescriptor(fd); };

    // Open new vault generation file
    for (;;) {
        if (!MakeDirectory(directory, false))
            return;

        const char *filename = Fmt(io->Allocator(), "%1%/%2.bin", directory, generation).ptr;
        OpenResult ret = OpenFile(filename, (int)OpenFlag::Write | (int)OpenFlag::Exclusive, (int)OpenResult::FileExists, &fd);

        if (ret == OpenResult::Success)
            break;
        if (ret != OpenResult::FileExists)
            return;

        generation++;
    }

    // Upload new file
    int64_t size;
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

        size = writer.GetRawWritten();

        if (!writer.Close())
            return;
    }

    // Update generation
    {
        bool success = db.Transaction([&]() {
            if (!db.Run(R"(INSERT INTO generations (vault, generation, previous, size)
                           VALUES (?1, ?2, ?3, ?4))",
                        vault, generation, previous, size))
                return false;
            if (!db.Run(R"(UPDATE vaults SET generation = ?3, previous = ?4
                           WHERE vid = uuid_blob(?1) AND generation = ?2)",
                        vid, generation - 1, generation, previous))
                return false;

            return true;
        });
        if (!success)
            return;
    }

    AddGenerationHeaders(io, generation, previous);

    io->SendText(200, "{}", "application/json");
}

static bool IsTitleValid(Span<const char> title)
{
    const auto test_char = [](char c) { return strchr("<>&", c) || (uint8_t)c < 32; };

    if (!title.len)
        return false;
    if (std::any_of(title.begin(), title.end(), test_char))
        return false;

    return true;
}

void HandleRemind(http_IO *io)
{
    // Parse input data
    const char *uid = nullptr;
    int64_t study = -1;
    const char *title = nullptr;
    LocalDate start = {};
    HeapArray<EventInfo> events;
    int offset = 0;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(4), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "uid") {
                parser.ParseString(&uid);
            } else if (key == "study") {
                parser.ParseInt(&study);
            } else if (key == "title") {
                parser.ParseString(&title);
            } else if (key == "start") {
                const char *str = nullptr;
                parser.ParseString(&str);

                if (str) {
                    start = {};
                    ParseDate(str, &start);
                } else {
                    start = {};
                }
            } else if (key == "events") {
                parser.ParseArray();
                while (parser.InArray()) {
                    EventInfo evt = {};

                    parser.ParseObject();
                    while (parser.InObject()) {
                        Span<const char> key = {};
                        parser.ParseKey(&key);

                        if (key == "date") {
                            const char *str = nullptr;
                            parser.ParseString(&str);

                            if (str) {
                                ParseDate(str, &evt.date);
                            }
                        } else if (key == "partial") {
                            parser.ParseBool(&evt.partial);
                        } else if (parser.IsValid()) {
                            LogError("Unexpected key '%1'", key);
                            io->SendError(422);
                            return;
                        }
                    }

                    events.Append(evt);
                }
            } else if (key == "offset") {
                parser.ParseInt(&offset);
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

        if (!uid || !IsUUIDValid(uid)) {
            LogError("Missing or invalid UID");
            valid = false;
        }
        if (study < 0) {
            LogError("Missing or invalid study");
            valid = false;
        }
        if (!title || !IsTitleValid(title)) {
            LogError("Missing or invalid title");
            valid = false;
        }
        if (!start.IsValid()) {
            LogError("Missing or invalid start");
            valid = false;
        }
        if (std::any_of(events.begin(), events.end(), [](const EventInfo &evt) { return !evt.date.IsValid(); })) {
            LogError("Missing or invalid events");
            valid = false;
        }
        if (offset < -780 || offset >= 960) {
            LogError("Missing or invalid time offset");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    // Make sure user exists
    int64_t user = 0;
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT id FROM users WHERE uid = uuid_blob(?1)", &stmt, uid))
            return;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Unknown user UID");
                io->SendError(404);
            }
            return;
        }

        user = sqlite3_column_int64(stmt, 0);
    }

    // Update study events
    bool success = db.Transaction([&]() {
        if (!db.Run("DELETE FROM events WHERE user = ?1 AND study = ?2", user, study))
            return false;

        for (const EventInfo &evt: events) {
            char dates[2][32] = {};

            Fmt(dates[0], "%1", start);
            Fmt(dates[1], "%1", evt.date);

            if (!db.Run(R"(INSERT INTO events (user, study, title, start, date, offset, partial)
                           VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7))",
                        user, study, title, dates[0], dates[1], offset, 0 + evt.partial))
                return false;
        }

        return true;
    });
    if (!success)
        return;

    io->SendText(200, "{}", "application/json");
}

void HandlePublish(http_IO *io)
{
    // Parse input data
    const char *rid = nullptr;
    int64_t study = -1;
    const char *test = nullptr;
    Span<const char> values = {};
    {
        StreamReader st;
        if (!io->OpenForRead(Mebibytes(1), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "rid") {
                parser.ParseString(&rid);
            } else if (key == "study") {
                parser.ParseInt(&study);
            } else if (key == "key") {
                parser.ParseString(&test);
            } else if (key == "values") {
                parser.PassThrough(&values);
            } else if (parser.IsValid()) {
                LogError("Unexpected key '%1'", key);
                io->SendError(422);
                return;
            }
        }
    }

    // Check missing or invalid values
    {
        bool valid = true;

        if (!rid || !IsUUIDValid(rid)) {
            LogError("Missing or invalid RID");
            valid = false;
        }
        if (study < 0) {
            LogError("Missing or invalid study");
            valid = false;
        }
        if (!test || test[0] != '/') {
            LogError("Missing or invalid key");
            valid = false;
        }
        if (!values.len || values[0] != '{') {
            LogError("Missing or invalid values");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    // Make sure participant exists
    int64_t participant = 0;
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT id FROM participants WHERE rid = uuid_blob(?1)", &stmt, rid))
            return;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Unknown participant RID");
                io->SendError(404);
            }
            return;
        }

        participant = sqlite3_column_int64(stmt, 0);
    }

    if (!db.Run(R"(INSERT INTO tests (participant, study, key, json)
                   VALUES (?1, ?2, ?3, ?4)
                   ON CONFLICT DO UPDATE SET json = excluded.json)",
                participant, study, test, values))
        return;

    io->SendText(200, "{}", "application/json");
}

}
