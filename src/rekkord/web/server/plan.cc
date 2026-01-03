// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "lib/native/http/http.hh"
#include "lib/native/password/password.hh"
#include "../../lib/librekkord.hh"
#include "rokkerd.hh"
#include "plan.hh"
#include "user.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace K {

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
    if (!db.Prepare(R"(SELECT p.id, p.repository, p.name, p.key, p.scan,
                              COUNT(i.id) AS items
                       FROM plans p
                       LEFT JOIN items i ON (i.plan = p.id)
                       WHERE p.owner = ?1
                       GROUP BY p.id)", &stmt, session->userid))
        return;

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartArray();

        while (stmt.Step()) {
            int64_t id = sqlite3_column_int64(stmt, 0);
            int64_t repository = sqlite3_column_int64(stmt, 1);
            const char *name = (const char *)sqlite3_column_text(stmt, 2);
            const char *key = (const char *)sqlite3_column_text(stmt, 3);
            int scan = sqlite3_column_int(stmt, 4);
            int items = sqlite3_column_int(stmt, 5);

            json->StartObject();

            json->Key("id"); json->Int64(id);
            if (repository > 0) {
                json->Key("repository"); json->Int64(repository);
            } else {
                json->Key("repository"); json->Null();
            }
            json->Key("name"); json->String(name);
            json->Key("key"); json->String(key);
            if (scan) {
                json->Key("scan"); json->Int(scan);
            } else {
                json->Key("scan"); json->Null();
            }
            json->Key("items"); json->Int(items);

            json->EndObject();
        }
        if (!stmt.IsValid())
            return;

        json->EndArray();
    });
}

static bool DumpItems(json_Writer *json, int64_t id, bool details)
{
    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT i.id, i.channel, i.days, i.clock,
                              r.timestamp, r.oid, r.error, p.path
                       FROM items i
                       LEFT JOIN reports r ON (r.id = i.report)
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
            const char *oid = (const char *)sqlite3_column_text(stmt, 5);
            const char *error = (const char *)sqlite3_column_text(stmt, 6);

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
            if (details) {
                if (oid) {
                    json->Key("oid"); json->String(oid);
                } else {
                    json->Key("oid"); json->Null();
                }
                if (error) {
                    json->Key("error"); json->String(error);
                } else {
                    json->Key("error"); json->Null();
                }
            } else {
                json->Key("success"); json->Bool(oid);
            }

            json->Key("paths"); json->StartArray();
            if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
                do {
                    const char *path = (const char *)sqlite3_column_text(stmt, 7);
                    json->String(path);
                } while (stmt.Step() && sqlite3_column_int64(stmt, 0) == id);
            } else {
                stmt.Step();
            }
            json->EndArray();

            json->EndObject();
        } while (stmt.IsRow());
    }
    if (!stmt.IsValid())
        return false;

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
    if (!db.Prepare(R"(SELECT repository, name, key, scan
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

    http_SendJson(io, 200, [&](json_Writer *json) {
        int64_t repository = sqlite3_column_int64(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        const char *key = (const char *)sqlite3_column_text(stmt, 2);
        int scan = sqlite3_column_int(stmt, 3);

        json->StartObject();

        json->Key("id"); json->Int64(id);
        if (repository > 0) {
            json->Key("repository"); json->Int64(repository);
        } else {
            json->Key("repository"); json->Null();
        }
        json->Key("name"); json->String(name);
        json->Key("key"); json->String(key);
        if (scan) {
            json->Key("scan"); json->Int(scan);
        } else {
            json->Key("scan"); json->Null();
        }

        json->Key("items");
        if (!DumpItems(json, id, true))
            return;

        json->EndObject();
    });
}

void HandlePlanSave(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    int64_t id = -1;
    const char *name = nullptr;
    int64_t repository = -1;
    int scan = -1;
    HeapArray<PlanItem> items;
    {
        bool success = http_ParseJson(io, Kibibytes(4), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "id") {
                    json->SkipNull() || json->ParseInt(&id);
                } else if (key == "name") {
                    json->ParseString(&name);
                } else if (key == "repository") {
                    json->SkipNull() || json->ParseInt(&repository);
                } else if (key == "scan") {
                    json->SkipNull() || json->ParseInt(&scan);
                } else if (key == "items") {
                    for (json->ParseArray(); json->InArray(); ) {
                        PlanItem item = {};

                        for (json->ParseObject(); json->InObject(); ) {
                            Span<const char> key = json->ParseKey();

                            if (key == "channel") {
                                json->ParseString(&item.channel);
                            } else if (key == "clock") {
                                json->ParseInt(&item.clock);
                            } else if (key == "days") {
                                json->ParseInt(&item.days);
                            } else if (key == "paths") {
                                for (json->ParseArray(); json->InArray(); ) {
                                    const char *path = nullptr;
                                    json->ParseString(&path);

                                    item.paths.Append(path);
                                }
                            } else {
                                json->UnexpectedKey(key);
                                valid = false;
                            }
                        }

                        items.Append(item);
                    }
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!name || !name[0]) {
                    LogError("Missing or invalid 'name' parameter");
                    valid = false;
                }
                if (scan >= 2400) {
                    LogError("Invalid 'scan' parameter");
                    valid = false;
                }

                for (const PlanItem &item: items) {
                    if (!item.channel || !item.channel[0]) {
                        LogError("Missing or invalid 'channel' parameter");
                        valid = false;
                    }
                    if (item.days < 0 || item.days >= 128) {
                        LogError("Missing or invalid 'days' parameter");
                        valid = false;
                    }
                    if (item.clock < 0 || item.clock >= 2400) {
                        LogError("Missing or invalid 'clock' parameter");
                        valid = false;
                    }

                    for (const char *path: item.paths) {
                        if (!path || !path[0]) {
                            LogError("Missing or invalid item path");
                            valid = false;
                        }
                    }

                    if (item.days && !item.paths.len) {
                        LogError("Missing item paths");
                        valid = false;
                    }
                }
            }

            return valid;
        });

        if (!success) {
            io->SendError(422);
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
        if (!db.Prepare(R"(INSERT INTO plans (id, owner, name, key, hash, scan)
                           VALUES (?1, ?2, ?3, ?4, ?5, ?6)
                           ON CONFLICT DO UPDATE SET id = IF(owner = excluded.owner, id, NULL),
                                                     name = excluded.name,
                                                     scan = excluded.scan
                           RETURNING id)",
                        &stmt, id >= 0 ? sq_Binding(id) : sq_Binding(), session->userid,
                        name, key, hash, scan >= 0 ? sq_Binding(scan) : sq_Binding()))
            return false;
        if (!stmt.Step()) {
            K_ASSERT(!stmt.IsValid());
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

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        json->Key("id"); json->Int64(id);
        if (key[0]) {
            json->Key("key"); json->String(key);
            json->Key("secret"); json->String(secret);
        }

        json->EndObject();
    });
}

void HandlePlanDelete(http_IO *io)
{
    RetainPtr<const SessionInfo> session = GetNormalSession(io);

    if (!session) {
        LogError("User is not logged in");
        io->SendError(401);
        return;
    }

    int64_t id = -1;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "id") {
                    json->ParseInt(&id);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (id < 0) {
                    LogError("Missing or invalid 'id' parameter");
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

    int64_t id = -1;
    {
        bool success = http_ParseJson(io, Kibibytes(1), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "id") {
                    json->ParseInt(&id);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (id < 0) {
                    LogError("Missing or invalid 'id' parameter");
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

    // Make sure plan exists
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT id FROM plans WHERE owner = ?1 AND id = ?2", &stmt, session->userid, id))
            return;

        if (!stmt.Step()) {
            if (stmt.IsValid()) {
                LogError("Unknown plan ID %1", id);
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

    http_SendJson(io, 200, [&](json_Writer *json) {
        json->StartObject();

        json->Key("key"); json->String(key);
        json->Key("secret"); json->String(secret);

        json->EndObject();
    });
}

void HandlePlanFetch(http_IO *io)
{
    int64_t plan = ValidateApiKey(io);
    if (plan < 0)
        return;

    http_SendJson(io, 200, [&](json_Writer *json) {
        DumpItems(json, plan, false);
    });
}

}
