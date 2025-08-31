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
#include "mail.hh"

namespace K {

static smtp_MailContent FailureMessage = {
    "[Error] {{ TITLE }}: {{ REPOSITORY }}",
    R"(Failed to check for {{ REPOSITORY }}:\n{{ ERROR }})",
    R"(<html lang="en"><body><p>Failed to check for <b>{{ REPOSITORY }}</b>:</p><p style="color: red;">{{ ERROR }}</p></body></html>)",
    {}
};

static smtp_MailContent FailureResolved = {
    "[Resolved] {{ TITLE }}: {{ REPOSITORY }}",
    R"(Access to {{ REPOSITORY }} is now back on track!)",
    R"(<html lang="en"><body><p>Access to <b>{{ REPOSITORY }}</b> is now back on track!</p></body></html>)",
    {}
};

static smtp_MailContent StaleMessage = {
    "[Stale] {{ TITLE }}: {{ REPOSITORY }} channel {{ CHANNEL }}",
    R"(Repository {{ REPOSITORY }} channel {{ CHANNEL }} looks stale.\n\nLast snapshot: {{ TIMESTAMP }})",
    R"(<html lang="en"><body><p>Repository <b>{{ REPOSITORY }}</b> channel <b>{{ CHANNEL }}</b> looks stale.</p><p>Last snapshot: <b>{{ TIMESTAMP }}</b></p></body></html>)",
    {}
};

static smtp_MailContent StaleResolved = {
    "[Resolved] {{ TITLE }}: {{ REPOSITORY }} channel {{ CHANNEL }}",
    R"(Repository {{ REPOSITORY }} channel {{ CHANNEL }} is now back on track!\n\nLast snapshot: {{ TIMESTAMP }})",
    R"(<html lang="en"><body><p>Repository <b>{{ REPOSITORY }}</b> channel <b>{{ CHANNEL }}</b> is now back on track!.</p><p>Last snapshot: <b>{{ TIMESTAMP }}</b></p></body></html>)",
    {}
};

static Span<const char> PatchFailure(Span<const char> text, const char *repository, const char *message, Allocator *alloc)
{
    Span<const char> ret = PatchFile(text, alloc, [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "TITLE") {
            writer->Write(config.title);
        } else if (key == "REPOSITORY") {
            writer->Write(repository);
        } else if (key == "ERROR") {
            writer->Write(message);
        } else {
            Print(writer, "{{%1}}", expr);
        }
    });

    return ret;
}

static Span<const char> PatchStale(Span<const char> text, const char *repository,
                                   const char *channel, int64_t timestamp, Allocator *alloc)
{
    Span<const char> ret = PatchFile(text, alloc, [&](Span<const char> expr, StreamWriter *writer) {
        Span<const char> key = TrimStr(expr);

        if (key == "TITLE") {
            writer->Write(config.title);
        } else if (key == "REPOSITORY") {
            writer->Write(repository);
        } else if (key == "CHANNEL") {
            writer->Write(channel);
        } else if (key == "TIMESTAMP") {
            TimeSpec spec = DecomposeTimeUTC(timestamp);
            Print(writer, "%1", FmtTimeNice(spec));
        } else {
            Print(writer, "{{%1}}", expr);
        }
    });

    return ret;
}

bool DetectAlerts()
{
    BlockAllocator temp_alloc;

    int64_t now = GetUnixTime();

    // Post error alerts
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(SELECT f.id, u.mail, r.name, f.message, f.resolved
                           FROM failures f
                           INNER JOIN repositories r ON (r.id = f.repository)
                           INNER JOIN users u ON (u.id = r.owner)
                           WHERE f.timestamp < ?1 AND
                                 (f.sent IS NULL OR f.sent < ?2))",
                        &stmt, now - config.mail_delay, now - config.repeat_delay))
            return false;

        while (stmt.Step()) {
            bool success = db.Transaction([&]() {
                int64_t id = sqlite3_column_int64(stmt, 0);
                const char *to = (const char *)sqlite3_column_text(stmt, 1);
                const char *name = (const char *)sqlite3_column_text(stmt, 2);
                const char *error = (const char *)sqlite3_column_text(stmt, 3);
                bool unsolved = !sqlite3_column_int(stmt, 4);

                smtp_MailContent content = unsolved ? FailureMessage : FailureResolved;

                content.subject = PatchFailure(content.subject, name, error, &temp_alloc).ptr;
                content.text = PatchFailure(content.text, name, error, &temp_alloc).ptr;
                content.html = PatchFailure(content.html, name, error, &temp_alloc).ptr;

                if (!PostMail(to, content))
                    return false;

                if (unsolved) {
                    if (!db.Run("UPDATE failures SET sent = ?2 WHERE id = ?1", id, now))
                        return false;
                } else {
                    if (!db.Run("DELETE FROM failures WHERE id = ?1", id))
                        return false;
                }

                return true;
            });
            if (!success)
                return false;
        }
        if (!stmt.IsValid())
            return false;
    }

    // Post stale alerts
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(SELECT s.id, u.mail, r.name, s.channel, s.timestamp, s.resolved
                           FROM stales s
                           INNER JOIN repositories r ON (r.id = s.repository)
                           INNER JOIN users u ON (u.id = r.owner)
                           WHERE s.timestamp < ?1 AND
                                 (s.sent IS NULL OR s.sent < ?2))",
                        &stmt, now - config.mail_delay, now - config.repeat_delay))
            return false;

        while (stmt.Step()) {
            bool success = db.Transaction([&]() {
                int64_t id = sqlite3_column_int64(stmt, 0);
                const char *to = (const char *)sqlite3_column_text(stmt, 1);
                const char *name = (const char *)sqlite3_column_text(stmt, 2);
                const char *channel = (const char *)sqlite3_column_text(stmt, 3);
                int64_t timestamp = sqlite3_column_int64(stmt, 4);
                bool unsolved = !sqlite3_column_int(stmt, 5);

                smtp_MailContent content = unsolved ? StaleMessage : StaleResolved;

                content.subject = PatchStale(content.subject, name, channel, timestamp, &temp_alloc).ptr;
                content.text = PatchStale(content.text, name, channel, timestamp, &temp_alloc).ptr;
                content.html = PatchStale(content.html, name, channel, timestamp, &temp_alloc).ptr;

                if (!PostMail(to, content))
                    return false;

                if (unsolved) {
                    if (!db.Run("UPDATE stales SET sent = ?2 WHERE id = ?1", id, now))
                        return false;
                } else {
                    if (!db.Run("DELETE FROM stales WHERE id = ?1", id))
                        return false;
                }

                return true;
            });
            if (!success)
                return false;
        }
        if (!stmt.IsValid())
            return false;
    }

    return true;
}

}
