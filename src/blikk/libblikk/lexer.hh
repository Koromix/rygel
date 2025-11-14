// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

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
