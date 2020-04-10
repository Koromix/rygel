// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"

namespace RG {

enum class TokenType {
    Plus = '+',
    Minus = '-',
    Multiply = '*',
    Divide = '/',
    Modulo = '%',
    Assign = '=',

    And = '&',
    Or = '|',
    Xor = '^',
    Not = '~',
    LogicNot = '!',

    Greater = '>',
    Less = '<',

    LeftParenthesis = '(',
    RightParenthesis = ')',
    LeftBrace = '{',
    RightBrace = '}',

    Integer = 256,
    Double,
    String,
    Identifier,

    LeftShift,
    RightShift,

    LogicAnd,
    LogicOr,
    Equal,
    NotEqual,
    GreaterOrEqual,
    LessOrEqual,

    If,
    Else,
    While,
};

struct Token {
    TokenType type;
    int32_t line;

    union {
        uint64_t i;
        double d;
        const char *str;
    } u;

    Token() {}
    Token(TokenType type, int32_t line) : type(type), line(line) {}
    Token(TokenType type, int32_t line, uint64_t i) : type(type), line(line) { u.i = i; }
    Token(TokenType type, int32_t line, double d) : type(type), line(line) { u.d = d; }
    Token(TokenType type, int32_t line, const char *str) : type(type), line(line) { u.str = str; }
};

struct TokenSet {
    HeapArray<Token> tokens;
    BlockAllocator str_alloc;
};

bool Tokenize(Span<const char> code, const char *filename, TokenSet *out_set);

}
