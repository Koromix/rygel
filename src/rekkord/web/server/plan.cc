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
#include "web.hh"
#include "plan.hh"
#include "user.hh"
#include "src/core/password/password.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

struct PlanItem {
    const char *channel;
    int clock;
    int days;
    HeapArray<const char *> paths;
};

void HandlePlanList(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT id, repository, name, key
                       FROM plans
                       WHERE owner = ?1)", &stmt, session->userid))
        return;

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartArray();
    while (stmt.Step()) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        int64_t repository = sqlite3_column_int64(stmt, 1);
        const char *name = (const char *)sqlite3_column_text(stmt, 2);
        const char *key = (const char *)sqlite3_column_text(stmt, 3);

        json.StartObject();

        json.Key("id"); json.Int64(id);
        json.Key("repository"); json.Int64(repository);
        json.Key("name"); json.String(name);
        json.Key("key"); json.String(key);

        json.EndObject();
    }
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish();
}

static bool DumpItems(json_Writer *json, int64_t id)
{
    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT i.id, i.channel, i.days, i.clock,
                              r.timestamp, r.failed, p.path
                       FROM items i
                       LEFT JOIN runs r ON (r.id = i.run)
                       LEFT JOIN paths p ON (p.item = i.id)
                       WHERE i.plan = ?1)", &stmt, id))
        return false;

    json->StartArray();
    if (stmt.Step()) {
        do {
            int64_t id = sqlite3_column_int64(stmt, 0);
            const char *channel = (const char *)sqlite3_column_text(stmt, 1);
            int days = sqlite3_column_int(stmt, 2);
            int clock = sqlite3_column_int(stmt, 3);
            int64_t timestamp = sqlite3_column_int64(stmt, 4);
            const char *failed = (const char *)sqlite3_column_text(stmt, 5);

            json->StartObject();

            json->Key("id"); json->Int64(id);
            json->Key("channel"); json->String(channel);
            json->Key("days"); json->Int(days);
            json->Key("clock"); json->Int(clock);

            if (timestamp) {
                json->Key("timestamp"); json->Int64(timestamp);
            } else {
                json->Key("timestamp"); json->Null();
            }
            if (failed) {
                json->Key("failed"); json->String(failed);
            } else {
                json->Key("failed"); json->Null();
            }

            json->Key("paths"); json->StartArray();
            if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
                do {
                    const char *path = (const char *)sqlite3_column_text(stmt, 6);
                    json->String(path);
                } while (stmt.Step() && sqlite3_column_int64(stmt, 0) == id);
            } else {
                stmt.Step();
            }
            json->EndArray();

            json->EndObject();
        } while (stmt.IsRow());
        if (!stmt.IsValid())
            return false;
    }
    json->EndArray();

    return true;
}

void HandlePlanGet(http_IO *io)
{
    const http_RequestInfo &request = io->Request();
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    int64_t id = -1;
    {
        const char *str = request.GetQueryValue("id");

        if (!str || !ParseInt(str, &id, (int)ParseFlag::End)) {
            LogError("Missing or invalid plan ID");
            io->SendError(422);
            return;
        }
    }

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT repository, name, key
                       FROM plans
                       WHERE owner = ?1 AND id = ?2)",
                    &stmt, session->userid, id))
        return;

    if (!stmt.Step()) {
        if (stmt.IsValid()) {
            LogError("Unknown repository ID %1", id);
            io->SendError(404);
        }
        return;
    }

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    int64_t repository = sqlite3_column_int64(stmt, 0);
    const char *name = (const char *)sqlite3_column_text(stmt, 1);
    const char *key = (const char *)sqlite3_column_text(stmt, 2);

    json.StartObject();

    json.Key("id"); json.Int64(id);
    json.Key("repository"); json.Int64(repository);
    json.Key("name"); json.String(name);
    json.Key("key"); json.String(key);

    json.Key("items");
    if (!DumpItems(&json, id))
        return;

    json.EndObject();

    json.Finish();
}

void HandlePlanSave(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    // Parse input data
    int64_t id = -1;
    const char *name = nullptr;
    int64_t repository = -1;
    HeapArray<PlanItem> items;
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
            } else if (key == "repository") {
                parser.SkipNull() || parser.ParseInt(&repository);
            } else if (key == "items") {
                parser.ParseArray();
                while (parser.InArray()) {
                    PlanItem item = {};

                    parser.ParseObject();
                    while (parser.InObject()) {
                        Span<const char> key = {};
                        parser.ParseKey(&key);

                        if (key == "channel") {
                            parser.ParseString(&item.channel);
                        } else if (key == "clock") {
                            parser.ParseInt(&item.clock);
                        } else if (key == "days") {
                            parser.ParseInt(&item.days);
                        } else if (key == "paths") {
                            parser.ParseArray();
                            while (parser.InArray()) {
                                const char *path = nullptr;
                                parser.ParseString(&path);

                                item.paths.Append(path);
                            }
                        } else if (parser.IsValid()) {
                            LogError("Unexpected key '%1'", key);
                            io->SendError(422);
                            return;
                        }
                    }

                    items.Append(item);
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

        if (!name || !name[0]) {
            LogError("Missing or invalid 'name' parameter");
            valid = false;
        }
        if (id < 0) {
            if (repository < 0) {
                LogError("Missing or invalid 'repository' parameter");
                valid = false;
            }
        } else {
            repository = -1;
        }

        for (const PlanItem &item: items) {
            if (!item.channel || !item.channel[0]) {
                LogError("Missing or invalid 'channel' parameter");
                valid = false;
            }
            if (item.days < 1 || item.days >= 128) {
                LogError("Missing or invalid 'days' parameter");
                valid = false;
            }
            if (item.clock < 0 || item.clock >= 2400) {
                LogError("Missing or invalid 'clock' parameter");
                valid = false;
            }

            if (!item.paths.len) {
                LogError("Missing item paths");
                valid = false;
            }
            for (const char *path: item.paths) {
                if (!path || !path[0]) {
                    LogError("Missing or invalid item path");
                    valid = false;
                }
            }
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    // Make sure user owns repository
    if (repository >= 0) {
        sq_Statement stmt;
        if (!db.Prepare("SELECT id FROM repositories WHERE owner = ?1 AND id = ?2", &stmt, session->userid, repository))
            return;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Unknown repository ID %1", id);
                io->SendError(404);
            }
            return;
        }
    }

    // Prepare new API key if needed
    char key[33];
    char secret[33];
    char hash[crypto_pwhash_STRBYTES];
    if (id < 0) {
        unsigned int flags = (int)pwd_GenerateFlag::Uppers |
                             (int)pwd_GenerateFlag::Lowers |
                             (int)pwd_GenerateFlag::Digits;

        pwd_GeneratePassword(flags, key);
        pwd_GeneratePassword(flags, secret);

        if (crypto_pwhash_str(hash, secret, strlen(secret),
                              crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
            LogError("Failed to hash secret");
            return;
        }
    } else {
        key[0] = 0;
        secret[0] = 0;
        hash[0] = 0;
    }

    uint8_t changeset[32];
    FillRandomSafe(changeset);

    // Create or update plan
    bool success = db.Transaction([&]() {
        sq_Statement stmt;
        if (!db.Prepare(R"(INSERT INTO plans (id, owner, repository, name, key, hash)
                           VALUES (?1, ?2, ?3, ?4, ?5, ?6)
                           ON CONFLICT DO UPDATE SET id = IF(owner = excluded.owner, id, NULL),
                                                     name = excluded.name
                           RETURNING id)",
                        &stmt, id >= 0 ? sq_Binding(id) : sq_Binding(),
                        session->userid, repository, name, key, hash))
            return false;
        if (!stmt.Step()) {
            RG_ASSERT(!stmt.IsValid());
            return false;
        }

        id = sqlite3_column_int64(stmt, 0);

        for (const PlanItem &item: items) {
            int64_t parent;
            {
                sq_Statement stmt;
                if (!db.Prepare(R"(INSERT INTO items (plan, channel, days, clock, changeset)
                                   VALUES (?1, ?2, ?3, ?4, ?5)
                                   ON CONFLICT DO UPDATE SET days = excluded.days,
                                                             clock = excluded.clock,
                                                             changeset = excluded.changeset
                                   RETURNING id)",
                                &stmt, id, item.channel, item.days, item.clock, sq_Binding(changeset)))
                    return false;
                if (!stmt.GetSingleValue(&parent))
                    return false;
            }

            if (!db.Run("DELETE FROM paths WHERE item = ?1", parent))
                return false;
            for (const char *path: item.paths) {
                if (!db.Run("INSERT INTO paths (item, path) VALUES (?1, ?2)", parent, path))
                    return false;
            }
        }

        if (!db.Run(R"(DELETE FROM items
                       WHERE plan = ?1 AND changeset IS NOT ?2)",
                    id, sq_Binding(changeset)))
            return false;

        return true;
    });
    if (!success)
        return;

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartObject();
    json.Key("id"); json.Int64(id);
    if (key[0]) {
        json.Key("key"); json.String(key);
        json.Key("secret"); json.String(secret);
    }
    json.EndObject();

    json.Finish();
}

void HandlePlanDelete(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

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
    if (id < 0) {
        LogError("Missing or invalid 'id' parameter");
        io->SendError(422);
        return;
    }

    if (!db.Run("DELETE FROM plans WHERE id = ?1 AND owner = ?2", id, session->userid))
        return;

    io->SendText(200, "{}", "application/json");
}

void HandlePlanKey(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

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
    if (id < 0) {
        LogError("Missing or invalid 'id' parameter");
        io->SendError(422);
        return;
    }

    // Make sure plan exists
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT id FROM plans WHERE owner = ?1 AND id = ?2", &stmt, session->userid, id))
            return;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Unknown repository ID %1", id);
                io->SendError(404);
            }
            return;
        }
    }

    char key[33];
    char secret[33];
    char hash[crypto_pwhash_STRBYTES];
    {
        unsigned int flags = (int)pwd_GenerateFlag::Uppers |
                             (int)pwd_GenerateFlag::Lowers |
                             (int)pwd_GenerateFlag::Digits;

        pwd_GeneratePassword(flags, key);
        pwd_GeneratePassword(flags, secret);

        if (crypto_pwhash_str(hash, secret, strlen(secret),
                              crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
            LogError("Failed to hash secret");
            return;
        }
    }

    if (!db.Run(R"(UPDATE plans SET key = ?3,
                                    hash = ?4
                   WHERE id = ?1 AND owner = ?2)",
                id, session->userid, key, hash))
        return;

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    json.StartObject();
    json.Key("key"); json.String(key);
    json.Key("secret"); json.String(secret);
    json.EndObject();

    json.Finish();
}

static int64_t ValidateApiKey(http_IO *io)
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
    if (!db.Prepare("SELECT id, hash FROM plans WHERE key = ?1", &stmt, key))
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

    int64_t id = sqlite3_column_int64(stmt, 0);
    const char *hash = (const char *)sqlite3_column_text(stmt, 1);

    if (crypto_pwhash_str_verify(hash, secret.ptr, (size_t)secret.len) < 0) {
        int64_t safety = std::max(2000 - GetMonotonicTime() + start, (int64_t)0);
        WaitDelay(safety);

        LogError("Invalid API key");
        io->SendError(403);
        return -1;
    }

    return id;
}

void HandlePlanFetch(http_IO *io)
{
    int64_t id = ValidateApiKey(io);
    if (id < 0)
        return;

    http_JsonPageBuilder json;
    if (!json.Init(io))
        return;

    if (!DumpItems(&json, id))
        return;

    json.Finish();
}

void HandlePlanReport(http_IO *io)
{
    int64_t id = ValidateApiKey(io);
    if (id < 0)
        return;

    // Parse input data
    int64_t item = -1;
    int64_t timestamp = -1;
    const char *failed = nullptr;
    {
        StreamReader st;
        if (!io->OpenForRead(Kibibytes(4), &st))
            return;
        json_Parser parser(&st, io->Allocator());

        parser.ParseObject();
        while (parser.InObject()) {
            Span<const char> key = {};
            parser.ParseKey(&key);

            if (key == "item") {
                parser.ParseInt(&item);
            } else if (key == "timestamp") {
                parser.ParseInt(&timestamp);
            } else if (key == "failed") {
                parser.SkipNull() || parser.ParseString(&failed);
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

        if (item < 0) {
            LogError("Missing or invalid 'item' parameter");
            valid = false;
        }
        if (timestamp < 0) {
            LogError("Missing or invalid 'timestamp' parameter");
            valid = false;
        }

        if (!valid) {
            io->SendError(422);
            return;
        }
    }

    bool success = db.Transaction([&]() {
        if (!db.Run("INSERT INTO runs (item, timestamp, failed) VALUES (?1, ?2, ?3)", item, timestamp, failed))
            return false;

        int64_t run = sqlite3_last_insert_rowid(db);

        if (!db.Run("UPDATE items SET run = ?3 WHERE plan = ?1 AND id = ?2", id, item, run))
            return false;
        if (!sqlite3_changes(db)) {
            LogError("Unexpected plan/item ID mismatch");
            return false;
        }

        return true;
    });
    if (!success)
        return;

    io->SendText(200, "{}", "application/json");
}

}
