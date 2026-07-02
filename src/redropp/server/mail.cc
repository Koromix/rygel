// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "lib/native/request/smtp.hh"
#include "web.hh"
#include "mail.hh"

namespace K {

static const int RetryDelay = 1 * 60000; // 1 minute
static const int DropDelay = 20 * 60000; // 20 minutes
static const int MaxErrors = 4;

static smtp_Sender smtp;

bool InitSMTP(const smtp_Config &config)
{
    return smtp.Init(config);
}

Span<const char> PatchMail(const char *basename, Allocator *alloc, FunctionRef<void(Span<const char>, StreamWriter *)> func)
{
    const char *path = Fmt(alloc, "src/redropp/server/mails/%1/%2", GetThreadLocale(), basename).ptr;
    const AssetInfo *asset = FindEmbedAsset(path);

    if (!asset) [[unlikely]] {
        path = Fmt(alloc, "src/redropp/server/mails/en/%1", basename).ptr;
        asset = FindEmbedAsset(path);

        K_ASSERT(asset);
    }

    Span<const char> mail;
    {
        HeapArray<char> buf(alloc);

        StreamReader reader(asset->data, "<asset>", asset->compression_type);
        StreamWriter writer(&buf, "<asset>");

        bool success = PatchFile(&reader, &writer, func);
        K_ASSERT(success);

        buf.Grow(1);
        buf.ptr[buf.len] = 0;

        mail = buf.Leak();
    }

    return mail;
}

bool PostMail(const char *to, const smtp_MailContent &content)
{
    BlockAllocator temp_alloc;

    const char *from = smtp.GetConfig().from;
    Span<const char> mail = smtp_BuildMail(from, to, content, &temp_alloc);
    int64_t now = GetUnixTime();

    if (!db.Run(R"(INSERT INTO mails (address, mail, ctime, errors, timestamp)
                   VALUES (?1, ?2, ?3, 0, ?3))", to, mail, now))
        return false;

    // Run pending tasks
    PostWaitMessage();

    return true;
}

bool SendMails()
{
    BlockAllocator temp_alloc;

    int64_t now = GetUnixTime();

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT id, address, mail, ctime, errors, timestamp
                       FROM mails
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
            if (!db.Prepare(R"(UPDATE mails SET timestamp = ?3
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
                    if (!db.Run("DELETE FROM mails WHERE id = ?1", id))
                        return false;
                } else {
                    int64_t next = persist ? (errors + 1) : 0;

                    if (!db.Run("UPDATE mails SET errors = ?2 WHERE id = ?1", id, next))
                        return false;
                }

                return persist;
            }

            if (!db.Run("DELETE FROM mails WHERE id = ?1", id))
                return false;

            return true;
        });
    }
    if (!stmt.IsValid())
        return false;

    stmt.Finalize();

    return async.Sync();
}

}
