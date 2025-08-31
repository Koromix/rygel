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

#pragma once

#include "src/core/base/base.hh"
#include "program.hh"

namespace K {

enum class bk_DiagnosticType {
    Error,
    Hint
};

template <typename... Args>
void bk_ReportDiagnostic(bk_DiagnosticType type, Span<const char> code, const char *filename,
                         int line, Size offset, const char *fmt, Args... args)
{
    // Find entire code line and compute column from offset
    int column = 0;
    Span<const char> extract = MakeSpan(code.ptr + offset, 0);
    while (extract.ptr > code.ptr && extract.ptr[-1] != '\n') {
        extract.ptr--;
        extract.len++;

        // Ignore UTF-8 trailing bytes to count code points. Not perfect (we want
        // to count graphemes), but close enough for now.
        column += ((extract.ptr[0] & 0xC0) != 0x80);
    }
    while (extract.end() < code.end() && !strchr("\r\n", extract.ptr[extract.len])) {
        extract.len++;
    }

    // Because we accept tabulation users, including the crazy ones who may put tabulations
    // after other characters, we can't just repeat ' ' (column - 1) times to align the
    // visual indicator. Instead, we create an alignment string containing spaces (for all
    // characters but tab) and tabulations.
    char align[1024];
    int align_more;
    {
        int align_len = std::min((int)K_SIZE(align) - 1, column);
        for (Size i = 0; i < align_len; i++) {
            align[i] = (extract[i] == '\t') ? '\t' : ' ';
        }
        align[align_len] = 0;

        // Tabulations and very long lines... if you can read this comment: just stop.
        align_more = column - align_len;
    }

    // Yeah I may have gone overboard a bit... but it looks nice :)
    Span<const char> comment = {};
    for (Size i = 0; i < extract.len; i++) {
        if (extract[i] == '"' || extract[i] == '\'') {
            char quote = extract[i];

            for (i++; i < extract.len; i++) {
                if (extract[i] == '\\') {
                    i++;
                } else if (extract[i] == quote) {
                    break;
                }
            }
        } else if (extract[i] == '#') {
            comment = extract.Take(i, extract.len - i);
            extract.len = i;
        }
    }

    switch (type) {
        case bk_DiagnosticType::Error:{
            char ctx[512];
            Fmt(ctx, "%1(%2:%3): ", filename, line, column + 1);

            LocalArray<char, 2048> msg_buf;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), StdErr->IsVt100(), "%!..+").len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), StdErr->IsVt100(), fmt, args...).len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), StdErr->IsVt100(), "\n%1 |%!0  %2%!D..%3%!0", FmtArg(line).Pad(-7), extract, comment).len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), StdErr->IsVt100(), "\n        |  %1%2%!M..^%!0", align, FmtArg(' ').Repeat(align_more)).len;

            Log(LogLevel::Error, ctx, "%1", msg_buf.data);
        } break;

        case bk_DiagnosticType::Hint: {
            char ctx[512];
            Fmt(ctx, "    %1(%2:%3): ", filename, line, column + 1);

            LocalArray<char, 2048> msg_buf;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), StdErr->IsVt100(), "%!..+").len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), StdErr->IsVt100(), fmt, args...).len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), StdErr->IsVt100(), "\n    %1 |%!0  %2%!D..%3%!0", FmtArg(line).Pad(-7), extract, comment).len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), StdErr->IsVt100(), "\n            |  %1%2%!D..^%!0", align, FmtArg(' ').Repeat(align_more)).len;

            Log(LogLevel::Info, ctx, "%1", msg_buf.data);
        } break;
    }
}


template <typename... Args>
void bk_ReportDiagnostic(bk_DiagnosticType type, const char *fmt, Args... args)
{
    switch (type) {
        case bk_DiagnosticType::Error: {
            LocalArray<char, 2048> msg_buf;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), StdErr->IsVt100(), "%!..+").len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), StdErr->IsVt100(), fmt, args...).len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), StdErr->IsVt100(), "%!0").len;

            Log(LogLevel::Error, "Error: ", "%1", msg_buf.data);
        } break;

        case bk_DiagnosticType::Hint: {
            LocalArray<char, 2048> msg_buf;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), StdErr->IsVt100(), "%!..+").len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), StdErr->IsVt100(), fmt, args...).len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), StdErr->IsVt100(), "%!0").len;

            Log(LogLevel::Info, "    Hint: ", "%1", msg_buf.data);
        } break;
    }
}

template <typename... Args>
void bk_ReportRuntimeError(const bk_Program &program, Span<const bk_CallFrame> frames,
                           const char *fmt, Args... args)
{
    LogInfo("Something wrong has happened, execution has stopped");
    LogInfo();

    if (frames.len) {
        LogInfo("Dumping stack trace:");

        for (Size i = 0; i < frames.len; i++) {
            const bk_CallFrame &frame = frames[frames.len - i - 1];

            const char *prototype = frame.func ? frame.func->prototype : "<outside function>";
            bool tre = frame.func && frame.func->tre;

            int32_t line;
            const char *filename = program.LocateInstruction(frame.func, frame.pc, &line);

            if (filename) {
                LogInfo(" %!M.+%1%!0 %!..+%2%3%!0 %!D..%4 (%5)%!0",
                        i ? "   " : ">>>", FmtArg(prototype).Pad(36), tre ? "[+]" : "   ", filename, line);
            } else {
                LogInfo(" %!M.+%1%!0 %!..+%2%3%!0 %!D..<native function>%!0",
                        i ? "   " : ">>>", FmtArg(prototype).Pad(36), tre ? "[+]" : "   ");
            }
        }

        LogInfo();
    }

    Log(LogLevel::Error, "Error: ", fmt, args...);
}

}
