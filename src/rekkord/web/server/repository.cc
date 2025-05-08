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
#include "backord.hh"
#include "repository.hh"
#include "user.hh"
#include "../../lib/librekkord.hh"

namespace RG {

void HandleRepositoryList(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(404);
        return;
    }

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT r.id, r.name, r.url, r.user, r.password, v.key, v.value
                       FROM repositories r
                       LEFT JOIN variables v
                       WHERE r.owner = ?1
                       ORDER BY r.id)", &stmt, session->userid))
        return;
    if (!stmt.Run())
        return;

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartArray();
    while (stmt.IsRow()) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        const char *url = (const char *)sqlite3_column_text(stmt, 2);
        const char *user = (const char *)sqlite3_column_text(stmt, 3);
        const char *password = (const char *)sqlite3_column_text(stmt, 4);

        json.StartObject();

        json.Key("id"); json.Int64(id);
        json.Key("name"); json.String(name);
        json.Key("url"); json.String(url);
        json.Key("user"); json.String(user);
        json.Key("password"); json.String(password);

        json.Key("variables"); json.StartObject();
        do {
            if (sqlite3_column_int64(stmt, 0) != id)
                break;
            if (sqlite3_column_type(stmt, 5) == SQLITE_NULL)
                break;

            const char *key = (const char *)sqlite3_column_text(stmt, 5);
            const char *value = (const char *)sqlite3_column_text(stmt, 6);

            json.Key(key); json.String(value);
        } while (stmt.Step());
        json.EndObject();

        json.EndObject();
    }
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish();
}

void HandleRepositorySave(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(404);
        return;
    }

    // Parse input data
    int64_t id = -1;
    const char *name = nullptr;
    const char *url = nullptr;
    const char *user = nullptr;
    const char *password = nullptr;
    HashMap<const char *, const char *> variables;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(4), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "id") {
                parser.SkipNull() || parser.ParseInt(&id);
            } else if (key == "name") {
                parser.ParseString(&name);
            } else if (key == "url") {
                parser.ParseString(&url);
            } else if (key == "user") {
                parser.ParseString(&user);
            } else if (key == "password") {
                parser.ParseString(&password);
            } else if (key == "variables") {
                parser.ParseObject();
                while (parser.InObject()) {
                    Span<const char> key = {};
                    Span<const char> value = {};
                    parser.ParseKey(&key);
                    parser.ParseString(&value);

                    variables.Set(key.ptr, value.ptr);
                }
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
        rk_Config config;

        if (!name || !name[0]) {
            LogError("Missing or invalid 'name' parameter");
            valid = false;
        }
        if (!user || !user[0]) {
            LogError("Missing or invalid 'user' parameter");
            valid = false;
        }
        if (!password || !password[0]) {
            LogError("Missing or invalid 'password' parameter");
            valid = false;
        }

        config.username = user;
        config.password = password;

        if (url) {
            if (rk_DecodeURL(url, &config)) {
                switch (config.type) {
                    case rk_DiskType::S3:
                    case rk_DiskType::SFTP: {} break;

                    case rk_DiskType::Local: {
                        LogError("Unsupported URL '%1'", url);
                        valid = false;
                    }
                }
            } else {
                valid = false;
            }
        } else {
            LogError("Missing 'url' value");
            valid = false;
        }

        switch (config.type) {
            case rk_DiskType::Local: { RG_ASSERT(!valid); } break;

            case rk_DiskType::S3: {
                config.s3.access_id = variables.FindValue("AWS_ACCESS_KEY_ID", nullptr);
                config.s3.access_key = variables.FindValue("AWS_SECRET_ACCESS_KEY", nullptr);

                valid &= config.Validate(true);
            } break;

            case rk_DiskType::SFTP: {
                config.ssh.known_hosts = false;
                config.ssh.password = variables.FindValue("SSH_PASSWORD", nullptr);
                config.ssh.key = variables.FindValue("SSH_KEY", nullptr);
                config.ssh.fingerprint = variables.FindValue("SSH_FINGERPRINT", nullptr);

                valid &= config.Validate(true);
            } break;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    // Create or update repository
    bool success = db.Transaction([&]() {
        sq_Statement stmt;
        if (!db.Prepare(R"(INSERT INTO repositories (id, owner, name, url, user, password)
                           VALUES (?1, ?2, ?3, ?4, ?5, ?6)
                           ON CONFLICT DO UPDATE SET id = IF(owner = excluded.owner, id, NULL),
                                                     name = excluded.name,
                                                     url = excluded.url
                           RETURNING id)",
                        &stmt, id >= 0 ? sq_Binding(id) : sq_Binding(), session->userid,
                        name, url, user, password))
            return false;
        if (!stmt.GetSingleValue(&id))
            return false;

        if (!db.Run("DELETE FROM variables WHERE repository = ?1", id))
            return false;

        for (const auto &it: variables.table) {
            if (!db.Run("INSERT INTO variables (repository, key, value) VALUES (?1, ?2, ?3)",
                        id, it.key, it.value))
                return false;
        }

        return true;
    });
    if (!success)
        return;

    io->SendText(200, "{}", "application/json");
}

void HandleRepositoryDelete(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(404);
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
    if (id < 0) {
        LogError("Missing or invalid 'id' parameter");
        io->SendError(422);
        return;
    }

    if (!db.Run("DELETE FROM repositories WHERE id = ?1 AND owner = ?2", id, session->userid))
        return;

    io->SendText(200, "{}", "application/json");
}

}
