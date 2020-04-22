// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"

namespace RG {

static inline Size DecodeUtf8(Span<const char> str, Size offset, int32_t *out_c)
{
    RG_ASSERT(offset < str.len);

    const uint8_t *ptr = (const uint8_t *)(str.ptr + offset);
    Size available = str.len - offset;

    if (ptr[0] < 0x80) {
        *out_c = ptr[0];
        return 1;
    } else if (RG_UNLIKELY(ptr[0] - 0xC2 > 0xF4 - 0xC2)) {
        return -1;
    } else if (ptr[0] < 0xE0 &&
               RG_LIKELY(available >= 2 && (ptr[1] & 0xC0) == 0x80)) {
        *out_c = ((ptr[0] & 0x1F) << 6) | (ptr[1] & 0x3F);
        return 2;
    } else if (ptr[0] < 0xF0 &&
               RG_LIKELY(available >= 3 && (ptr[1] & 0xC0) == 0x80 &&
                                           (ptr[2] & 0xC0) == 0x80)) {
        *out_c = ((ptr[0] & 0xF) << 12) | ((ptr[1] & 0x3F) << 6) | (ptr[2] & 0x3F);
        return 3;
    } else if (RG_LIKELY(available >= 4 && (ptr[1] & 0xC0) == 0x80 &&
                                           (ptr[2] & 0xC0) == 0x80 &&
                                           (ptr[3] & 0xC0) == 0x80)) {
        *out_c = ((ptr[0] & 0x7) << 18) | ((ptr[1] & 0x3F) << 12) | ((ptr[2] & 0x3F) << 6) | (ptr[3] & 0x3F);
        return 4;
    } else {
        return -1;
    }
}

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
    int column = 1;
    Span<const char> extract = MakeSpan(code.ptr + offset, 0);
    while (extract.ptr > code.ptr && extract.ptr[-1] != '\n') {
        extract.ptr--;
        extract.len++;

        // Ignore UTF-8 trailing bytes
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
