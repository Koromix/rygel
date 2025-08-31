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
#include "src/core/request/smtp.hh"
#include "ludivine.hh"
#include "mail.hh"

namespace K {

static const int64_t RetryDelay = 10 * 60000;
static const int64_t MaxErrors = 10;

static smtp_Sender smtp;

bool InitSMTP(const smtp_Config &config)
{
    return smtp.Init(config);
}

bool PostMail(const char *to, const smtp_MailContent &content)
{
    BlockAllocator temp_alloc;

    const char *from = smtp.GetConfig().from;
    Span<const char> mail = smtp_BuildMail(from, to, content, &temp_alloc);

    if (!db.Run(R"(INSERT INTO mails (address, mail, sent, errors)
                   VALUES (?1, ?2, 0, 0))", to, mail))
        return false;

    // Run pending tasks
    SignalWaitFor();

    return true;
}

bool SendMails()
{
    BlockAllocator temp_alloc;

    int64_t now = GetUnixTime();

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT id, address, mail, sent, errors
                       FROM mails
                       WHERE sent < ?1)", &stmt, now - RetryDelay))
        return false;

    Async async;

    while (stmt.Step()) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        const char *to = (const char *)sqlite3_column_text(stmt, 1);
        Span<const char> mail = MakeSpan((const char *)sqlite3_column_text(stmt, 2),
                                         sqlite3_column_bytes(stmt, 2));
        int64_t sent = sqlite3_column_int64(stmt, 3);
        int64_t errors = sqlite3_column_int64(stmt, 4);

        // Commit send task
        {
            sq_Statement stmt;
            if (!db.Prepare(R"(UPDATE mails SET sent = ?3
                               WHERE id = ?1 AND sent = ?2
                               RETURNING id)",
                            &stmt, id, sent, now))
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
            bool done = smtp.Send(to, mail) || (errors + 1 >= MaxErrors);

            if (done) {
                if (!db.Run("DELETE FROM mails WHERE id = ?1", id))
                    return false;
            } else {
                if (!db.Run("UPDATE mails SET errors = errors + 1 WHERE id = ?1", id))
                    return false;
            }

            return true;
        });
    }
    if (!stmt.IsValid())
        return false;

    stmt.Finalize();

    return async.Sync();
}

}
