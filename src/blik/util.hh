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
    offset -= (offset > 0 && offset + 1 < code.len && IsAsciiWhite(code[offset + 1]));

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

    if (EnableAnsiOutput()) {
        Print(stderr, "\x1B[91m%1(%2:%3):\x1B[0m \x1B[1m", filename, line, column);
        PrintLn(stderr, fmt, args...);
        PrintLn(stderr, "%1 |\x1B[0m  %2", FmtArg(line).Pad(-7), extract);
        PrintLn(stderr, "        | %1\x1B[95m^^^\x1B[0m", FmtArg(' ').Repeat(column));
    } else {
        Print(stderr, "%1(%2:%3): ", filename, line, column);
        PrintLn(stderr, fmt, args...);
        PrintLn(stderr, "%1 |  %2", FmtArg(line).Pad(-7), extract);
        PrintLn(stderr, "        | %1^^^", FmtArg(' ').Repeat(column));
    }
}

}
