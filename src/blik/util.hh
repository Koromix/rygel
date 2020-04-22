// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"

namespace RG {

template <typename... Args>
void ReportError(Span<const char> code, const char *filename, int line, Size offset,
                 const char *fmt, Args... args)
{
    // We point the user to error location with '^^^', if the token is a single
    // character (e.g. operator) we want the second caret to be centered on it.
    // There is a small trap: we can't do that if the character before is a tabulation,
    // see below for tab handling.
    offset -= (offset > 0 && code[offset - 1] == ' ' &&
               offset + 1 < code.len && IsAsciiWhite(code[offset + 1]));

    // Extract code line
    Span<const char> extract = MakeSpan(code.ptr + offset, 0);
    while (extract.ptr > code.ptr && extract.ptr[-1] != '\n') {
        extract.ptr--;
        extract.len++;
    }
    while (extract.end() < code.end() && !strchr("\r\n", extract.ptr[extract.len])) {
        extract.len++;
    }

    int column = (int)(offset - (extract.ptr - code.ptr)) + 1;

    // Because we accept tabulation users, including the crazy ones who may put tabulations
    // after other characters, we can't just repeat ' ' (column - 1) times to align the
    // visual indicator. Instead, we create an alignment string containing spaces (for all
    // characters but tab) and tabulations.
    char align[1024];
    int align_more;
    {
        int align_len = std::min((int)RG_SIZE(align) - 1, column - 1);
        for (Size i = 0; i < align_len; i++) {
            align[i] = (extract[i] == '\t') ? '\t' : ' ';
        }
        align[align_len] = 0;

        // Tabulations and very long lines... we'll try out best, but you really need to stop...
        align_more = column - align_len - 1;
    }

    if (EnableAnsiOutput()) {
        Print(stderr, "\x1B[91m%1(%2:%3):\x1B[0m \x1B[1m", filename, line, column);
        PrintLn(stderr, fmt, args...);
        PrintLn(stderr, "%1 |\x1B[0m  %2", FmtArg(line).Pad(-7), extract);
        PrintLn(stderr, "        |  %1%2\x1B[95m^^^\x1B[0m", align, FmtArg(' ').Repeat(align_more));
    } else {
        Print(stderr, "%1(%2:%3): ", filename, line, column);
        PrintLn(stderr, fmt, args...);
        PrintLn(stderr, "%1 |  %2", FmtArg(line).Pad(-7), extract);
        PrintLn(stderr, "        |  %1%2^^^", align, FmtArg(' ').Repeat(align_more));
    }
}

}
