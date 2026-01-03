// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "rokkerd.hh"
#include "../../lib/librekkord.hh"
#include "link.hh"
#include "user.hh"

namespace K {

void HandleLinkSnapshot(http_IO *io)
{
    int64_t owner;
    int64_t plan = ValidateApiKey(io, &owner);
    if (plan < 0)
        return;

    const char *url = nullptr;
    const char *channel = nullptr;
    int64_t timestamp = -1;
    const char *oid = nullptr;
    int64_t size = -1;
    int64_t stored = -1;
    int64_t added = -1;
    const char *error = nullptr;
    {
        bool success = http_ParseJson(io, Kibibytes(4), [&](json_Parser *json) {
            bool valid = true;

            for (json->ParseObject(); json->InObject(); ) {
                Span<const char> key = json->ParseKey();

                if (key == "repository") {
                    json->ParseString(&url);
                } else if (key == "channel") {
                    json->ParseString(&channel);
                } else if (key == "timestamp") {
                    json->ParseInt(&timestamp);
                } else if (key == "oid") {
                    json->SkipNull() || json->ParseString(&oid);
                } else if (key == "size") {
                    json->ParseInt(&size);
                } else if (key == "stored") {
                    json->ParseInt(&stored);
                } else if (key == "added") {
                    json->ParseInt(&added);
                } else if (key == "error") {
                    json->ParseString(&error);
                } else {
                    json->UnexpectedKey(key);
                    valid = false;
                }
            }
            valid &= json->IsValid();

            if (valid) {
                if (!url) {
                    LogError("Missing or invalid 'repository' parameter");
                    valid = false;
                }
                if (!channel) {
                    LogError("Missing or invalid 'channel' parameter");
                    valid = false;
                }
                if (timestamp < 0) {
                    LogError("Missing or invalid 'timestamp' parameter");
                    valid = false;
                }
                if (oid) {
                    if (!rk_ParseOID(oid)) {
                        LogError("Invalid snapshot OID");
                        valid = false;
                    }
                    if (size < 0 || stored < 0 || added < 0) {
                        LogError("Missing or invalid size values");
                        valid = false;
                    }
                    if (error) {
                        LogError("Cannot specify OID and error at the same time");
                        valid = false;
                    }
                } else {
                    if (!error) {
                        LogError("Missing both OID and error message");
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

    bool success = db.Transaction([&]() {
        int64_t repository;
        {
            sq_Statement stmt;
            if (!db.Prepare(R"(INSERT INTO repositories (owner, url, checked, failed, errors)
                               VALUES (?1, ?2, 0, ?3, ?4)
                               ON CONFLICT (url) DO UPDATE SET failed = excluded.failed,
                                                               errors = errors + excluded.errors
                               RETURNING id)",
                            &stmt, owner, url, error, 0 + !!error))
                return false;

            if (!stmt.Step()) {
                K_ASSERT(!stmt.IsValid());
                return false;
            }

            repository = sqlite3_column_int64(stmt, 0);
        }

        int64_t report;
        {
            sq_Statement stmt;
            if (!db.Prepare(R"(INSERT INTO reports (plan, repository, channel, timestamp, oid, error)
                               VALUES (?1, ?2, ?3, ?4, ?5, ?6)
                               RETURNING id)",
                            &stmt, plan, repository, channel, timestamp, oid, error))
                return false;

            if (!stmt.Step()) {
                K_ASSERT(!stmt.IsValid());
                return false;
            }

            report = sqlite3_column_int64(stmt, 0);
        }

        if (oid) {
            if (!db.Run(R"(INSERT INTO snapshots (repository, oid, channel, timestamp, size, stored, added)
                           VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7))",
                        repository, oid, channel, timestamp, size, stored, added))
                return false;
            if (!db.Run(R"(INSERT INTO channels (repository, name, oid, timestamp, size, count, ignore)
                           VALUES (?1, ?2, ?3, ?4, ?5, 1, 0)
                           ON CONFLICT DO UPDATE SET oid = IIF(timestamp > excluded.timestamp, excluded.oid, oid),
                                                     timestamp = excluded.timestamp,
                                                     size = IIF(timestamp > excluded.timestamp, excluded.size, size),
                                                     count = count + 1)",
                        repository, channel, oid, timestamp, size))
                return false;
        }

        if (!db.Run(R"(INSERT INTO items (plan, channel, days, clock, report)
                       VALUES (?1, ?2, 0, 0, ?3)
                       ON CONFLICT (plan, channel) DO UPDATE SET report = excluded.report)", plan, channel, report))
            return false;
        if (!db.Run("UPDATE plans SET repository = ?2 WHERE id = ?1", plan, repository))
            return false;

        return true;
    });
    if (!success)
        return;

    io->SendText(200, "{}", "application/json");
}

}
