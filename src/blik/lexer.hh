// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"

namespace RG {

enum class TokenType {
    #define TOKEN(Type, Str) Type,
    #include "tokens.inc"
};
static const char *const TokenTypeNames[] = {
    #define TOKEN(Type, Str) (Str),
    #include "tokens.inc"
};

struct Token {
    TokenType type;
    int32_t line;

    union {
        bool b;
        int64_t i;
        double d;
        const char *str;
    } u;
};

struct TokenSet {
    HeapArray<Token> tokens;
    BlockAllocator str_alloc;
};

bool Tokenize(Span<const char> code, const char *filename, TokenSet *out_set);

}
