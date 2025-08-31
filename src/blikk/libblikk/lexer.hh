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

namespace K {

enum class bk_TokenKind {
    #define TOKEN(Kind, Str) Kind,
    #include "tokens.inc"
};
static const char *const bk_TokenKindNames[] = {
    #define TOKEN(Kind, Str) (Str),
    #include "tokens.inc"
};

struct bk_Token {
    bk_TokenKind kind;

    int line;
    Size offset;

    union {
        bool b;
        int64_t i;
        double d;
        const char *str;
    } u;
};

struct bk_TokenizedFile {
    const char *filename = nullptr;
    Span<const char> code;

    HeapArray<bk_Token> tokens;

    // Used to parse function and record declarations in prepass
    HeapArray<Size> prototypes;

    BlockAllocator str_alloc;
};

// TokenizedFile keeps a reference to code, you must keep it around!
bool bk_Tokenize(Span<const char> code, const char *filename, bk_TokenizedFile *out_file);

}
