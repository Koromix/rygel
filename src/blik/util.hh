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

    if (LogUsesTerminalOutput()) {
        Print(stderr, "\x1B[38;5;202m%1(%2:%3):\x1B[0m \x1B[31m", filename, line, column);
        PrintLn(stderr, fmt, args...);
        Print(stderr, "\x1B[0m");

        PrintLn(stderr, "    %1", extract);
        PrintLn(stderr, "  %1\x1B[38;5;202m^^^\x1B[0m", FmtArg(' ').Repeat(column));
    } else {
        Print(stderr, "%1(%2:%3): ", filename, line, column);
        PrintLn(stderr, fmt, args...);

        PrintLn(stderr, "    %1", extract);
        PrintLn(stderr, "  %1^^^", FmtArg(' ').Repeat(column));
    }
}

}
