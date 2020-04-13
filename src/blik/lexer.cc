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

    for (Size i = 0, j = 1; i < code.len; i = j++) {
        switch (code[i]) {
            case ' ':
            case '\t':
            case '\r': {} break;

            case '\n': {
                out_set->tokens.Append(Token(TokenType::NewLine, line));
                line++;
            } break;

            case '0': {
                if (j < code.len && IsAsciiAlpha(code[j])) {
                    if (code[j] == 'b') {
                        int64_t value = 0;
                        bool overflow = false;

                        while (++j < code.len) {
                            unsigned int digit = (unsigned int)(code[j] - '0');

                            if (digit < 2) {
                                overflow |= (value > (INT64_MAX - digit) / 2);
                                value = (value * 2) + digit;
                            } else if (RG_UNLIKELY(digit < 10)) {
                                LogError("Invalid binary digit '%1'", code[j]);
                                valid = false;
                                break;
                            } else {
                                break;
                            }
                        }

                        if (RG_UNLIKELY(overflow)) {
                            LogError("Number literal is too large (max = %1)", INT64_MAX);
                            valid = false;
                        }

                        out_set->tokens.Append(Token(TokenType::Integer, line, value));
                        continue;
                    } else if (code[j] == 'o') {
                        int64_t value = 0;
                        bool overflow = false;

                        while (++j < code.len) {
                            unsigned int digit = (unsigned int)(code[j] - '0');

                            if (digit < 8) {
                                overflow |= (value > (INT64_MAX - digit) / 8);
                                value = (value * 8) + digit;
                            } else if (RG_UNLIKELY(digit < 10)) {
                                LogError("Invalid octal digit '%1'", code[j]);
                                valid = false;
                                break;
                            } else {
                                break;
                            }
                        }

                        if (RG_UNLIKELY(overflow)) {
                            LogError("Number literal is too large (max = %1)", INT64_MAX);
                            valid = false;
                        }

                        out_set->tokens.Append(Token(TokenType::Integer, line, value));
                        continue;
                    } else if (code[j] == 'x') {
                        int64_t value = 0;
                        bool overflow = false;

                        while (++j < code.len) {
                            if (IsAsciiDigit(code[j])) {
                                unsigned int digit = (unsigned int)(code[j] - '0');

                                overflow |= (value > (INT64_MAX - digit) / 16);
                                value = (value * 16) + (code[j] - '0');
                            } else if (code[j] >= 'A' && code[j] <= 'F') {
                                unsigned int digit = (unsigned int)(code[j] - 'A' + 10);

                                overflow |= (value > (INT64_MAX - digit) / 16);
                                value = (value * 16) + (code[j] - 'A' + 10);
                            } else if (code[j] >= 'a' && code[j] <= 'f') {
                                unsigned int digit = (unsigned int)(code[j] - 'a' + 10);

                                overflow |= (value > (INT64_MAX - digit) / 16);
                                value = (value * 16) + (code[j] - 'a' + 10);
                            } else if (RG_UNLIKELY(IsAsciiAlpha(code[j]))) {
                                LogError("Invalid hexadecimal digit '%1'", code[j]);
                                valid = false;
                                break;
                            } else {
                                break;
                            }
                        }

                        if (RG_UNLIKELY(overflow)) {
                            LogError("Number literal is too large (max = %1)", INT64_MAX);
                            valid = false;
                        }

                        out_set->tokens.Append(Token(TokenType::Integer, line, value));
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
                int64_t value = code[i] - '0';
                bool overflow = false;
                bool dot = false;

                while (j < code.len) {
                    unsigned int digit = (unsigned int)(code[j] - '0');

                    if (digit < 10) {
                        overflow |= (value > (INT64_MAX - digit) / 10);
                        value = (value * 10) + (code[j] - '0');
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
                        LogError("Number literal is too large (max = %1)", INT64_MAX);
                        valid = false;
                    }

                    out_set->tokens.Append(Token(TokenType::Integer, line, value));
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
                                case 'a': { str.Append('\a'); } break;
                                case 'b': { str.Append('\b'); } break;
                                case '\\': { str.Append('\\'); } break;
                                case '"':  { str.Append('"'); } break;
                                case '\'':  { str.Append('\''); } break;
                                case '0':  { str.Append(0); } break;

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
                } else if (ident == "true") {
                    out_set->tokens.Append(Token(TokenType::Bool, line, true));
                } else if (ident == "false") {
                    out_set->tokens.Append(Token(TokenType::Bool, line, false));
                } else {
                    const char *str = DuplicateString(ident, &out_set->str_alloc).ptr;
                    out_set->tokens.Append(Token(TokenType::Identifier, line, str));
                }
            } break;

            case '+': { out_set->tokens.Append(Token(TokenType::Plus, line)); } break;
            case '-': { out_set->tokens.Append(Token(TokenType::Minus, line)); } break;
            case '*': { out_set->tokens.Append(Token(TokenType::Multiply, line)); } break;
            case '/': {
                if (j < code.len && code[j] == '/') {
                    while (++j < code.len && code[j] != '\n');
                } else {
                    out_set->tokens.Append(Token(TokenType::Divide, line));
                }
            } break;
            case '%': { out_set->tokens.Append(Token(TokenType::Modulo, line)); } break;
            case '^': { out_set->tokens.Append(Token(TokenType::Xor, line)); } break;
            case '~': { out_set->tokens.Append(Token(TokenType::Not, line)); } break;
            case '(': { out_set->tokens.Append(Token(TokenType::LeftParenthesis, line)); } break;
            case ')': { out_set->tokens.Append(Token(TokenType::RightParenthesis, line)); } break;
            case '{': { out_set->tokens.Append(Token(TokenType::LeftBrace, line)); } break;
            case '}': { out_set->tokens.Append(Token(TokenType::RightBrace, line)); } break;

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

    // Newlines are used to end statements. Make sure the last statement has one.
    out_set->tokens.Append(Token(TokenType::NewLine, line));

    if (valid) {
        out_guard.Disable();
    }
    return valid;
}

}
