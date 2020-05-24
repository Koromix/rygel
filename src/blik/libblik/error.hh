// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "program.hh"

namespace RG {

enum class DiagnosticType {
    Error,
    ErrorHint
};

template <typename... Args>
void ReportDiagnostic(DiagnosticType type, Span<const char> code, const char *filename,
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
        int align_len = std::min((int)RG_SIZE(align) - 1, column);
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
        case DiagnosticType::Error: {
            char ctx_buf[512];
            Fmt(ctx_buf, "%1(%2:%3): ", filename, line, column + 1);

            LocalArray<char, 2048> msg_buf;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), "%!..+").len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), fmt, args...).len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), "\n%1 |%!0  %2%!D..%3%!0", FmtArg(line).Pad(-7), extract, comment).len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), "\n        |  %1%2%!M..^%!0", align, FmtArg(' ').Repeat(align_more)).len;

            Log(LogLevel::Error, ctx_buf, "%1", msg_buf.data);
        } break;

        case DiagnosticType::ErrorHint: {
            char ctx_buf[512];
            Fmt(ctx_buf, "    %1(%2:%3): ", filename, line, column + 1);

            LocalArray<char, 2048> msg_buf;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), "%!..+").len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), fmt, args...).len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), "\n    %1 |%!0  %2%!D..%3%!0", FmtArg(line).Pad(-7), extract, comment).len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), "\n            |  %1%2%!D..^%!0", align, FmtArg(' ').Repeat(align_more)).len;

            Log(LogLevel::Info, ctx_buf, "%1", msg_buf.data);
        } break;
    }
}


template <typename... Args>
void ReportDiagnostic(DiagnosticType type, const char *fmt, Args... args)
{
    switch (type) {
        case DiagnosticType::Error: {
            LocalArray<char, 2048> msg_buf;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), "%!..+").len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), fmt, args...).len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), "%!0").len;

            Log(LogLevel::Error, "Error: ", "%1", msg_buf.data);
        } break;

        case DiagnosticType::ErrorHint: {
            LocalArray<char, 2048> msg_buf;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), "%!..+").len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), fmt, args...).len;
            msg_buf.len += Fmt(msg_buf.TakeAvailable(), "%!0").len;

            Log(LogLevel::Info, "    Hint: ", "%1", msg_buf.data);
        } break;
    }
}

template <typename... Args>
void ReportRuntimeError(const Program &program, Span<const CallFrame> frames, const char *fmt, Args... args)
{
    if (frames.len) {
        LogInfo("Dumping stack trace:");

        for (Size i = frames.len - 1; i >= 0; i--) {
            const CallFrame &frame = frames[i];

            const char *name = frame.func ? frame.func->signature : "<outside function>";
            bool tre = frame.func && frame.func->tre;

            int32_t line;
            const char *filename = program.LocateInstruction(frame.pc, &line);

            if (filename) {
                LogInfo(" %!M.+%1%!0 %!..+%2%3%!0 %!D..%4 (%5)%!0",
                        i ? "   " : ">>>", FmtArg(name).Pad(36), tre ? "(+)" : "   ", filename, line);
            } else {
                LogInfo(" %!M.+%1%!0 %!..+%2%3%!0 %!D..<native function>%!0",
                        i ? "   " : ">>>", FmtArg(name).Pad(36), tre ? "(+)" : "   ");
            }
        }

        LogInfo();
    }

    LogError(fmt, args...);
}

}
