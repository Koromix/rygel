// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "lexer.hh"

namespace RG {

class Lexer {
    bool valid = true;

    Span<const char> code;
    Size offset;
    Size next;
    int32_t line;

    TokenSet set;

public:
    bool Tokenize(Span<const char> code, const char *filename);
    void Finish(TokenSet *out_set);

private:
    bool Token1(TokenKind tok);
    bool Token2(char c, TokenKind tok);

    template <typename... Args>
    void MarkError(const char *fmt, Args... args)
    {
        LogError(fmt, args...);
        valid = false;
    }
};

bool Lexer::Tokenize(Span<const char> code, const char *filename)
{
    RG_ASSERT(valid);

    this->code = code;
    line = 1;

    PushLogFilter([&](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
        if (valid) {
            char msg_buf[4096];
            Fmt(msg_buf, "%1(%2): %3", filename, line, msg);

            func(level, ctx, msg_buf);
        }
    });
    RG_DEFER { PopLogFilter(); };

    for (offset = 0, next = 1; offset < code.len; offset = next++) {
        switch (code[offset]) {
            case ' ':
            case '\t':
            case '\r': {} break;

            case '\n': {
                Token1(TokenKind::NewLine);
                line++;
            } break;

            case '#': {
                while (next < code.len && code[next] != '\n') {
                    next++;
                }
            } break;

            case '0': {
                if (next < code.len && IsAsciiAlpha(code[next])) {
                    if (code[next] == 'b') {
                        int64_t value = 0;
                        bool overflow = false;

                        while (++next < code.len) {
                            unsigned int digit = (unsigned int)(code[next] - '0');

                            if (digit < 2) {
                                overflow |= (value > (INT64_MAX - digit) / 2);
                                value = (value * 2) + digit;
                            } else if (RG_UNLIKELY(digit < 10)) {
                                MarkError("Invalid binary digit '%1'", code[next]);
                                return false;
                            } else {
                                break;
                            }
                        }

                        if (RG_UNLIKELY(overflow)) {
                            MarkError("Number literal is too large (max = %1)", INT64_MAX);
                            return false;
                        }

                        set.tokens.Append({TokenKind::Integer, line, {.i = value}});
                        continue;
                    } else if (code[next] == 'o') {
                        int64_t value = 0;
                        bool overflow = false;

                        while (++next < code.len) {
                            unsigned int digit = (unsigned int)(code[next] - '0');

                            if (digit < 8) {
                                overflow |= (value > (INT64_MAX - digit) / 8);
                                value = (value * 8) + digit;
                            } else if (RG_UNLIKELY(digit < 10)) {
                                MarkError("Invalid octal digit '%1'", code[next]);
                                return false;
                            } else {
                                break;
                            }
                        }

                        if (RG_UNLIKELY(overflow)) {
                            MarkError("Number literal is too large (max = %1)", INT64_MAX);
                            return false;
                        }

                        set.tokens.Append({TokenKind::Integer, line, {.i = value}});
                        continue;
                    } else if (code[next] == 'x') {
                        int64_t value = 0;
                        bool overflow = false;

                        while (++next < code.len) {
                            if (IsAsciiDigit(code[next])) {
                                unsigned int digit = (unsigned int)(code[next] - '0');

                                overflow |= (value > (INT64_MAX - digit) / 16);
                                value = (value * 16) + (code[next] - '0');
                            } else if (code[next] >= 'A' && code[next] <= 'F') {
                                unsigned int digit = (unsigned int)(code[next] - 'A' + 10);

                                overflow |= (value > (INT64_MAX - digit) / 16);
                                value = (value * 16) + (code[next] - 'A' + 10);
                            } else if (code[next] >= 'a' && code[next] <= 'f') {
                                unsigned int digit = (unsigned int)(code[next] - 'a' + 10);

                                overflow |= (value > (INT64_MAX - digit) / 16);
                                value = (value * 16) + (code[next] - 'a' + 10);
                            } else if (RG_UNLIKELY(IsAsciiAlpha(code[next]))) {
                                MarkError("Invalid hexadecimal digit '%1'", code[next]);
                                return false;
                            } else {
                                break;
                            }
                        }

                        if (RG_UNLIKELY(overflow)) {
                            MarkError("Number literal is too large (max = %1)", INT64_MAX);
                            return false;
                        }

                        set.tokens.Append({TokenKind::Integer, line, {.i = value}});
                        continue;
                    } else {
                        MarkError("Invalid literal base character '%1'", code[next]);
                        return false;
                    }
                }
            } RG_FALLTHROUGH;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9': {
                int64_t value = code[offset] - '0';
                bool overflow = false;
                bool dot = false;

                while (next < code.len) {
                    unsigned int digit = (unsigned int)(code[next] - '0');

                    if (digit < 10) {
                        overflow |= (value > (INT64_MAX - digit) / 10);
                        value = (value * 10) + (code[next] - '0');
                    } else if (code[next] == '.') {
                        dot = (next + 1 < code.len && IsAsciiDigit(code[next + 1]));
                        break;
                    } else {
                        break;
                    }

                    next++;
                }

                if (dot) {
                    errno = 0;

                    char *end;
                    double d = strtod(code.ptr + offset, &end);
                    next = end - code.ptr;

                    if (RG_UNLIKELY(errno == ERANGE)) {
                        MarkError("Double value exceeds supported range");
                        return false;
                    }

                    set.tokens.Append({TokenKind::Double, line, {.d = d}});
                } else {
                    if (RG_UNLIKELY(overflow)) {
                        MarkError("Number literal is too large (max = %1)", INT64_MAX);
                        return false;
                    }

                    set.tokens.Append({TokenKind::Integer, line, {.i = value}});
                }
            } break;

            case '"':
            case '\'': {
                HeapArray<char> str(&set.str_alloc);

                for (;;) {
                    if (RG_UNLIKELY(next >= code.len || code[next] == '\n')) {
                        MarkError("Unfinished string literal");
                        return false;
                    }
                    if (code[next] == code[offset]) {
                        next++;
                        break;
                    }

                    if (code[next] == '\\') {
                        if (++next < code.len) {
                            switch (code[next]) {
                                case 'r': { str.Append('\r'); } break;
                                case 'n': { str.Append('\n'); } break;
                                case 't': { str.Append('\t'); } break;
                                case 'f': { str.Append('\f'); } break;
                                case 'v': { str.Append('\v'); } break;
                                case 'a': { str.Append('\a'); } break;
                                case 'b': { str.Append('\b'); } break;
                                case '\\': { str.Append('\\'); } break;
                                case '"':  { str.Append('"'); } break;
                                case '\'':  { str.Append('\''); } break;
                                case '0':  { str.Append(0); } break;

                                default: {
                                    MarkError("Unsupported escape sequence '\\%1'", code[next]);
                                    return false;
                                } break;
                            }
                        }
                    } else {
                        str.Append(code[next]);
                    }

                    next++;
                }
                str.Append(0);

                set.tokens.Append({TokenKind::String, line, {.str = str.TrimAndLeak().ptr}});
            } break;

            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
            case 'g':
            case 'h':
            case 'i':
            case 'j':
            case 'k':
            case 'l':
            case 'm':
            case 'n':
            case 'o':
            case 'p':
            case 'q':
            case 'r':
            case 's':
            case 't':
            case 'u':
            case 'v':
            case 'w':
            case 'x':
            case 'y':
            case 'z':
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
            case 'G':
            case 'H':
            case 'I':
            case 'J':
            case 'K':
            case 'L':
            case 'M':
            case 'N':
            case 'O':
            case 'P':
            case 'Q':
            case 'R':
            case 'S':
            case 'T':
            case 'U':
            case 'V':
            case 'W':
            case 'X':
            case 'Y':
            case 'Z':
            case '_': {
                while (next < code.len && (IsAsciiAlphaOrDigit(code[next]) || code[next] == '_')) {
                    next++;
                }

                Span<const char> ident = code.Take(offset, next - offset);

                if (ident == "func") {
                    // In order to have order-independent top-level functions, we need to parse
                    // their declarations first! Tell the parser where to look to help it.
                    set.funcs.Append(set.tokens.len);
                    Token1(TokenKind::Func);
                } else if (ident == "return") {
                    Token1(TokenKind::Return);
                } else if (ident == "let") {
                    Token1(TokenKind::Let);
                } else if (ident == "do") {
                    Token1(TokenKind::Do);
                } else if (ident == "end") {
                    Token1(TokenKind::End);
                } else if (ident == "if") {
                    Token1(TokenKind::If);
                } else if (ident == "else") {
                    Token1(TokenKind::Else);
                } else if (ident == "while") {
                    Token1(TokenKind::While);
                } else if (ident == "for") {
                    Token1(TokenKind::For);
                } else if (ident == "in") {
                    Token1(TokenKind::In);
                } else if (ident == "print") {
                    Token1(TokenKind::Print);
                } else if (ident == "null") {
                    Token1(TokenKind::Null);
                } else if (ident == "true") {
                    set.tokens.Append({TokenKind::Bool, line, {.b = true}});
                } else if (ident == "false") {
                    set.tokens.Append({TokenKind::Bool, line, {.b = false}});
                } else {
                    const char *str = DuplicateString(ident, &set.str_alloc).ptr;
                    set.tokens.Append({TokenKind::Identifier, line, {.str = str}});
                }
            } break;

            case '+': { Token1(TokenKind::Plus); } break;
            case '-': { Token1(TokenKind::Minus); } break;
            case '*': { Token1(TokenKind::Multiply); } break;
            case '/': { Token1(TokenKind::Divide); } break;
            case '%': { Token1(TokenKind::Modulo); } break;
            case '^': { Token1(TokenKind::Xor); } break;
            case '~': { Token1(TokenKind::Not); } break;
            case '.': {
                if (RG_UNLIKELY(!Token2('.', TokenKind::DotDot))) {
                    MarkError("Unexpected character '.'");
                    return false;
                }
            } break;
            case ':': { Token2('=', TokenKind::Reassign) || Token1(TokenKind::Colon); } break;
            case '(': { Token1(TokenKind::LeftParenthesis); } break;
            case ')': { Token1(TokenKind::RightParenthesis); } break;
            case ',': { Token1(TokenKind::Comma); } break;

            case '=': { Token1(TokenKind::Equal); } break;
            case '!': { Token2('=', TokenKind::NotEqual) || Token1(TokenKind::LogicNot); } break;
            case '&': { Token2('&', TokenKind::LogicAnd) || Token1(TokenKind::And); } break;
            case '|': { Token2('|', TokenKind::LogicOr) || Token1(TokenKind::Or); } break;
            case '>': { Token2('>', TokenKind::RightShift) || Token2('=', TokenKind::GreaterOrEqual) || Token1(TokenKind::Greater); } break;
            case '<': { Token2('<', TokenKind::LeftShift) || Token2('=', TokenKind::LessOrEqual) || Token1(TokenKind::Less); } break;

            default: {
                MarkError("Unexpected character '%1'", code[offset]);
                return false;
            } break;
        }
    }

    // Newlines are used to end statements. Make sure the last statement has one.
    Token1(TokenKind::NewLine);

    return valid;
}

void Lexer::Finish(TokenSet *out_set)
{
    RG_ASSERT(!out_set->tokens.len);
    SwapMemory(&set, out_set, RG_SIZE(set));
}

bool Lexer::Token1(TokenKind tok)
{
    set.tokens.Append({tok, line});
    return true;
}

bool Lexer::Token2(char c, TokenKind tok)
{
    if (next < code.len && code[next] == c) {
        set.tokens.Append({tok, line});
        next++;

        return true;
    } else {
        return false;
    }
}

bool Tokenize(Span<const char> code, const char *filename, TokenSet *out_set)
{
    Lexer lexer;
    if (!lexer.Tokenize(code, filename))
        return false;

    lexer.Finish(out_set);
    return true;
}

}
