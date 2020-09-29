// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"

namespace RG {

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

    // Used to parse function declarations in first pass
    HeapArray<Size> funcs;

    BlockAllocator str_alloc;
};

// TokenizedFile keeps a reference to code, you must keep it around!
bool bk_Tokenize(Span<const char> code, const char *filename, bk_TokenizedFile *out_file);

}
