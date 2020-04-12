// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"

namespace RG {

enum class TokenType {
    #define TOKEN(Type, Value) Type = (Value),
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

    Token() {}
    Token(TokenType type, int32_t line) : type(type), line(line) {}
    Token(TokenType type, int32_t line, bool b) : type(type), line(line) { u.b = b; }
    Token(TokenType type, int32_t line, int64_t i) : type(type), line(line) { u.i = i; }
    Token(TokenType type, int32_t line, double d) : type(type), line(line) { u.d = d; }
    Token(TokenType type, int32_t line, const char *str) : type(type), line(line) { u.str = str; }
};

struct TokenSet {
    HeapArray<Token> tokens;
    BlockAllocator str_alloc;
};

bool Tokenize(Span<const char> code, const char *filename, TokenSet *out_set);

}
