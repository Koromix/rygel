// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "lexer.hh"

namespace RG {

bool Tokenize(Span<const char> code, const char *filename, TokenSet *out_set)
{
    RG_DEFER_NC(out_guard, len = out_set->tokens.len) { out_set->tokens.RemoveFrom(len); };

    bool valid = true;
    int32_t line = 1;

    PushLogFilter([=](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
        char msg_buf[4096];
        Fmt(msg_buf, "%1(%2): %3", filename, line, msg);

        func(level, ctx, msg_buf);
    });
    RG_DEFER { PopLogFilter(); };

    for (Size i = 0, j; i < code.len; i = j) {
        line += (code[i] == '\n');
        j = i + 1;

        switch (code[i]) {
            case ' ':
            case '\t':
            case '\r':
            case '\n': {
                line += (code[i] == '\n');
            } break;

            case '0': {
                if (j < code.len && IsAsciiAlpha(code[j])) {
                    if (code[j] == 'b') {
                        uint64_t u = 0;
                        bool overflow = false;

                        while (++j < code.len) {
                            unsigned int digit = (unsigned int)(code[j] - '0');

                            if (digit < 2) {
                                overflow |= (u > (UINT64_MAX - digit) / 2);
                                u = (u * 2) + digit;
                            } else if (RG_UNLIKELY(digit < 10)) {
                                LogError("Invalid binary digit '%1'", code[j]);
                                valid = false;
                                break;
                            } else {
                                break;
                            }
                        }

                        if (RG_UNLIKELY(overflow)) {
                            LogError("Number literal is too large (max = %1)", UINT64_MAX);
                            valid = false;
                        }

                        out_set->tokens.Append(Token(TokenType::Integer, line, u));
                        continue;
                    } else if (code[j] == 'o') {
                        uint64_t u = 0;
                        bool overflow = false;

                        while (++j < code.len) {
                            unsigned int digit = (unsigned int)(code[j] - '0');

                            if (digit < 8) {
                                overflow |= (u > (UINT64_MAX - digit) / 8);
                                u = (u * 8) + digit;
                            } else if (RG_UNLIKELY(digit < 10)) {
                                LogError("Invalid octal digit '%1'", code[j]);
                                valid = false;
                                break;
                            } else {
                                break;
                            }
                        }

                        if (RG_UNLIKELY(overflow)) {
                            LogError("Number literal is too large (max = %1)", UINT64_MAX);
                            valid = false;
                        }

                        out_set->tokens.Append(Token(TokenType::Integer, line, u));
                        continue;
                    } else if (code[j] == 'x') {
                        uint64_t u = 0;
                        bool overflow = false;

                        while (++j < code.len) {
                            if (IsAsciiDigit(code[j])) {
                                unsigned int digit = (unsigned int)(code[j] - '0');

                                overflow |= (u > (UINT64_MAX - digit) / 16);
                                u = (u * 16) + (code[j] - '0');
                            } else if (code[j] >= 'A' && code[j] <= 'F') {
                                unsigned int digit = (unsigned int)(code[j] - 'A' + 10);

                                overflow |= (u > (UINT64_MAX - digit) / 16);
                                u = (u * 16) + (code[j] - 'A' + 10);
                            } else if (code[j] >= 'a' && code[j] <= 'f') {
                                unsigned int digit = (unsigned int)(code[j] - 'a' + 10);

                                overflow |= (u > (UINT64_MAX - digit) / 16);
                                u = (u * 16) + (code[j] - 'a' + 10);
                            } else if (RG_UNLIKELY(IsAsciiAlpha(code[j]))) {
                                LogError("Invalid hexadecimal digit '%1'", code[j]);
                                valid = false;
                                break;
                            } else {
                                break;
                            }
                        }

                        if (RG_UNLIKELY(overflow)) {
                            LogError("Number literal is too large (max = %1)", UINT64_MAX);
                            valid = false;
                        }

                        out_set->tokens.Append(Token(TokenType::Integer, line, u));
                        continue;
                    } else {
                        LogError("Invalid literal base character '%1'", code[j]);
                        valid = false;
                        break;
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
                uint64_t u = code[i] - '0';
                bool overflow = false;
                bool dot = false;

                while (j < code.len) {
                    unsigned int digit = (unsigned int)(code[j] - '0');

                    if (digit < 10) {
                        overflow |= (u > (UINT64_MAX - digit) / 10);
                        u = (u * 10) + (code[j] - '0');
                    } else if (code[j] == '.') {
                        dot = true;
                        break;
                    } else {
                        break;
                    }

                    j++;
                }

                if (dot) {
                    errno = 0;

                    char *end;
                    double d = strtod(code.ptr + i, &end);
                    j = end - code.ptr;

                    if (RG_UNLIKELY(errno == ERANGE)) {
                        LogError("Double value exceeds supported range");
                        valid = false;
                        break;
                    }

                    out_set->tokens.Append(Token(TokenType::Double, line, d));
                } else {
                    if (RG_UNLIKELY(overflow)) {
                        LogError("Number literal is too large (max = %1)", UINT64_MAX);
                        valid = false;
                    }

                    out_set->tokens.Append(Token(TokenType::Integer, line, u));
                }
            } break;

            case '"':
            case '\'': {
                HeapArray<char> str(&out_set->str_alloc);

                for (;;) {
                    if (RG_UNLIKELY(j >= code.len || code[j] == '\n')) {
                        LogError("Unfinished string literal");
                        valid = false;
                        break;
                    }
                    if (code[j] == code[i]) {
                        j++;
                        break;
                    }

                    if (code[j] == '\\') {
                        if (++j < code.len) {
                            switch (code[j]) {
                                case 'r': { str.Append('\r'); } break;
                                case 'n': { str.Append('\n'); } break;
                                case 't': { str.Append('\t'); } break;
                                case 'f': { str.Append('\f'); } break;
                                case 'v': { str.Append('\v'); } break;
                                case '\\': { str.Append('\\'); } break;
                                case '"':  { str.Append('"'); } break;
                                case '\'':  { str.Append('\''); } break;

                                default: {
                                    LogError("Unsupported escape sequence '\\%1'", code[j]);
                                    valid = false;
                                } break;
                            }
                        }
                    } else {
                        str.Append(code[j]);
                    }

                    j++;
                }
                str.Append(0);

                out_set->tokens.Append(Token(TokenType::String, line, str.TrimAndLeak().ptr));
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
                while (j < code.len && (IsAsciiAlphaOrDigit(code[j]) || code[j] == '_')) {
                    j++;
                }

                Span<const char> ident = code.Take(i, j - i);

                if (ident == "if") {
                    out_set->tokens.Append(Token(TokenType::If, line));
                } else if (ident == "else") {
                    out_set->tokens.Append(Token(TokenType::Else, line));
                } else if (ident == "while") {
                    out_set->tokens.Append(Token(TokenType::While, line));
                } else {
                    const char *str = DuplicateString(ident, &out_set->str_alloc).ptr;
                    out_set->tokens.Append(Token(TokenType::Identifier, line, str));
                }
            } break;

            case '+':
            case '-':
            case '*':
            case '/':
            case '%':
            case '^':
            case '~':
            case '(':
            case ')':
            case '{':
            case '}': {
                out_set->tokens.Append(Token((TokenType)code[i], line));
            } break;

            case '=': {
                if (j < code.len && code[j] == '=') {
                    out_set->tokens.Append(Token(TokenType::Equal, line));
                    j++;
                } else {
                    out_set->tokens.Append(Token(TokenType::Assign, line));
                }
            } break;
            case '!': {
                if (j < code.len && code[j] == '=') {
                    out_set->tokens.Append(Token(TokenType::NotEqual, line));
                    j++;
                } else {
                    out_set->tokens.Append(Token(TokenType::LogicNot, line));
                }
            } break;
            case '&': {
                if (j < code.len && code[j] == code[i]) {
                    out_set->tokens.Append(Token(TokenType::LogicAnd, line));
                    j++;
                } else {
                    out_set->tokens.Append(Token(TokenType::And, line));
                }
            } break;
            case '|': {
                if (j < code.len && code[j] == code[i]) {
                    out_set->tokens.Append(Token(TokenType::LogicOr, line));
                    j++;
                } else {
                    out_set->tokens.Append(Token(TokenType::Or, line));
                }
            } break;
            case '>': {
                if (j < code.len && code[j] == '>') {
                    out_set->tokens.Append(Token(TokenType::RightShift, line));
                    j++;
                } else if (j < code.len && code[j] == '=') {
                    out_set->tokens.Append(Token(TokenType::GreaterOrEqual, line));
                    j++;
                } else {
                    out_set->tokens.Append(Token(TokenType::Greater, line));
                }
            } break;
            case '<': {
                if (j < code.len && code[j] == '<') {
                    out_set->tokens.Append(Token(TokenType::LeftShift, line));
                    j++;
                } else if (j < code.len && code[j] == '=') {
                    out_set->tokens.Append(Token(TokenType::LessOrEqual, line));
                    j++;
                } else {
                    out_set->tokens.Append(Token(TokenType::Less, line));
                }
            } break;

            default: {
                LogError("Unexpected character '%1'", code[i]);
                valid = false;
            } break;
        }
    }

    if (valid) {
        out_guard.Disable();
    }
    return valid;
}

}
