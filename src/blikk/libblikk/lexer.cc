// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "error.hh"
#include "lexer.hh"
#include "vendor/fast_float/fast_float.h"

namespace K {

static K_CONSTINIT ConstMap<64, const char *, bk_Token> KeywordsMap = {
    { "func", { bk_TokenKind::Func, 0, 0, {} } },
    { "return", { bk_TokenKind::Return, 0, 0, {} } },
    { "let", { bk_TokenKind::Let, 0, 0, {} } },
    { "mut", { bk_TokenKind::Mut, 0, 0, {} } },
    { "begin", { bk_TokenKind::Begin, 0, 0, {} } },
    { "end", { bk_TokenKind::End, 0, 0, {} } },
    { "if", { bk_TokenKind::If, 0, 0, {} } },
    { "else", { bk_TokenKind::Else, 0, 0, {} } },
    { "while", { bk_TokenKind::While, 0, 0, {} } },
    { "for", { bk_TokenKind::For, 0, 0, {} } },
    { "in", { bk_TokenKind::In, 0, 0, {} } },
    { "break", { bk_TokenKind::Break, 0, 0, {} } },
    { "continue", { bk_TokenKind::Continue, 0, 0, {} } },
    { "do", { bk_TokenKind::Do, 0, 0, {} } },
    { "record", { bk_TokenKind::Record, 0, 0, {} } },
    { "enum", { bk_TokenKind::Enum, 0, 0, {} } },
    { "pass", { bk_TokenKind::Pass, 0, 0, {} } },

    { "and", { bk_TokenKind::AndAnd, 0, 0, {} } },
    { "or", { bk_TokenKind::OrOr, 0, 0, {} } },
    { "not", { bk_TokenKind::Not, 0, 0, {} } },

    { "null", { bk_TokenKind::Null, 0, 0, {} } },
    { "true", { bk_TokenKind::Boolean, 0, 0, { .b = true } } },
    { "false", { bk_TokenKind::Boolean, 0, 0, { .b = false } } }
};

class bk_Lexer {
    K_DELETE_COPY(bk_Lexer)

    const char *filename;
    Span<const char> code;
    Size offset;
    Size next;
    int32_t line;
    bool valid;

    HashSet<Span<const char>> strings;

    bk_TokenizedFile *file;
    HeapArray<bk_Token> &tokens;

public:
    bk_Lexer(bk_TokenizedFile *file);

    bool Tokenize(Span<const char> code, const char *filename);

private:
    inline bool Token1(bk_TokenKind tok);
    inline bool Token2(char c, bk_TokenKind tok);
    inline bool Token3(char c1, char c2, bk_TokenKind tok);
    inline bool Token4(char c1, char c2, char c3, bk_TokenKind tok);

    unsigned int ConvertHexaDigit(Size pos);

    void TokenizeFloat();

    template <typename... Args>
    void MarkError(Size offset, const char *fmt, Args... args)
    {
        if (valid) {
            bk_ReportDiagnostic(bk_DiagnosticType::Error, code, filename,
                                line, offset, fmt, args...);
            valid = false;
        }
    }

    void MarkUnexpected(Size offset, const char *prefix)
    {
        // It's possible the caller has done this already, but we can afford a bit
        // of redundance here: it is an error path.
        int32_t uc;
        Size bytes = DecodeUtf8(code, offset, &uc);

        if (!bytes) {
            MarkError(offset, "Illegal UTF-8 sequence");
        } else if ((uint8_t)code[offset] < 32) {
            MarkError(offset, "%1 byte 0x%2", prefix, FmtHex(code[offset], 2));
        } else {
            MarkError(offset, "%1 character '%2'", prefix, code.Take(offset, bytes));
        }
    }
};

bk_Lexer::bk_Lexer(bk_TokenizedFile *file)
    : file(file), tokens(file->tokens)
{
    K_ASSERT(file);
}

bool bk_Lexer::Tokenize(Span<const char> code, const char *filename)
{
    K_DEFER_NC(err_guard, tokens_len = tokens.len,
                           prototypes_len = file->prototypes.len) {
        tokens.RemoveFrom(tokens_len);
        file->prototypes.RemoveFrom(prototypes_len);
    };

    // Skip UTF-8 BOM... Who invented this crap?
    if (code.len >= 3 && (uint8_t)code[0] == 0xEF &&
                         (uint8_t)code[1] == 0xBB &&
                         (uint8_t)code[2] == 0xBF) {
        code = code.Take(3, code.len - 3);
    }

    // Make sure we only have one EndOfLine token at the end. Without it some parser errors
    // caused by premature end of file may be not be located correctly.
    code = TrimStrRight(code);

    this->filename = filename;
    this->code = code;
    line = 1;

    valid = true;

    // Reuse for performance
    HeapArray<char> str_buf(&file->str_alloc);

    for (offset = 0, next = 1; offset < code.len; offset = next++) {
        switch (code[offset]) {
            case ' ':
            case '\t':
            case '\r': {} break;

            case '\n': {
                Token1(bk_TokenKind::EndOfLine);
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
                        // We limit to IN64_MAX, but we build with -ftrapv in debug builds.
                        // Don't replace with int64_t or debug builds will crash on overflow!
                        uint64_t value = 0;
                        bool overflow = false;

                        while (++next < code.len) {
                            unsigned int digit = (unsigned int)(code[next] - '0');

                            if (digit < 2) {
                                overflow |= (value > ((uint64_t)INT64_MAX - digit) / 2);
                                value = (value * 2) + digit;
                            } else if (digit < 10) [[unlikely]] {
                                MarkUnexpected(next, "Invalid binary digit");
                                return false;
                            } else {
                                break;
                            }
                        }

                        if (overflow) [[unlikely]] {
                            MarkError(offset, "Number literal is too big (max = %1)", INT64_MAX);
                            return false;
                        }

                        tokens.Append({ bk_TokenKind::Integer, line, offset, { .i = (int64_t)value } });
                        continue;
                    } else if (code[next] == 'o') {
                        // We limit to IN64_MAX, but we build with -ftrapv in debug builds.
                        // Don't replace with int64_t or debug builds will crash on overflow!
                        uint64_t value = 0;
                        bool overflow = false;

                        while (++next < code.len) {
                            unsigned int digit = (unsigned int)(code[next] - '0');

                            if (digit < 8) {
                                overflow |= (value > ((uint64_t)INT64_MAX - digit) / 8);
                                value = (value * 8) + digit;
                            } else if (digit < 10) [[unlikely]] {
                                MarkUnexpected(next, "Invalid octal digit");
                                return false;
                            } else {
                                break;
                            }
                        }

                        if (overflow) [[unlikely]] {
                            MarkError(offset, "Number literal is too big (max = %1)", INT64_MAX);
                            return false;
                        }

                        tokens.Append({ bk_TokenKind::Integer, line, offset, { .i = (int64_t)value } });
                        continue;
                    } else if (code[next] == 'x') {
                        // We limit to IN64_MAX, but we build with -ftrapv in debug builds.
                        // Don't replace with int64_t or debug builds will crash on overflow!
                        uint64_t value = 0;
                        bool overflow = false;

                        while (++next < code.len) {
                            unsigned int digit = ConvertHexaDigit(next);

                            if (digit >= 16) {
                                if (IsAsciiAlpha(code[next])) {
                                    MarkError(next, "Invalid hexadecimal digit");
                                    return false;
                                } else {
                                    break;
                                }
                            }

                            overflow |= (value > ((uint64_t)INT64_MAX - digit) / 16);
                            value = (value * 16) + digit;
                        }

                        if (overflow) [[unlikely]] {
                            MarkError(offset, "Number literal is too big (max = %1)", INT64_MAX);
                            return false;
                        }

                        tokens.Append({ bk_TokenKind::Integer, line, offset, { .i = (int64_t)value } });
                        continue;
                    } else {
                        MarkUnexpected(next, "Invalid literal base");
                        return false;
                    }
                }
            } [[fallthrough]];
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9': {
                // We limit to IN64_MAX, but we build with -ftrapv in debug builds.
                // Don't replace with int64_t or debug builds will crash on overflow!
                uint64_t value = code[offset] - '0';
                bool overflow = false;
                bool fp = false;

                while (next < code.len) {
                    unsigned int digit = (unsigned int)(code[next] - '0');

                    if (digit < 10) {
                        overflow |= (value > ((uint64_t)INT64_MAX - digit) / 10);
                        value = (value * 10) + (code[next] - '0');
                    } else if (code[next] == '.' || code[next] == 'e' || code[next] == 'E') {
                        fp = true;
                        break;
                    } else {
                        break;
                    }

                    next++;
                }

                if (fp) {
                    TokenizeFloat();
                } else {
                    if (overflow) [[unlikely]] {
                        MarkError(offset, "Number literal is too big (max = %1)", INT64_MAX);
                        return false;
                    }

                    tokens.Append({ bk_TokenKind::Integer, line, offset, { .i = (int64_t)value } });
                }
            } break;

            case '"': {
                str_buf.RemoveFrom(0);

                for (;;) {
                    if (next >= code.len || code[next] == '\n') [[unlikely]] {
                        MarkError(offset, "Unfinished string literal");
                        return false;
                    }
                    if (code[next] == '\r') [[unlikely]] {
                        MarkError(next, "Carriage return is not allowed in string literals, use \\r");
                        return false;
                    }

                    if (code[next] == '"') {
                        next++;
                        break;
                    } else if (code[next] == '\\') {
                        if (++next < code.len) {
                            switch (code[next]) {
                                case 'r': { str_buf.Append('\r'); } break;
                                case 'n': { str_buf.Append('\n'); } break;
                                case 't': { str_buf.Append('\t'); } break;
                                case 'f': { str_buf.Append('\f'); } break;
                                case 'v': { str_buf.Append('\v'); } break;
                                case 'a': { str_buf.Append('\a'); } break;
                                case 'b': { str_buf.Append('\b'); } break;
                                case 'e': { str_buf.Append('\x1B'); } break;
                                case 'x': {
                                    if (next > code.len - 3) [[unlikely]] {
                                        MarkError(next + 1, "Truncated escape sequence");
                                        return false;
                                    }

                                    int c = 0;
                                    for (Size i = 0; i < 2; i++) {
                                        unsigned int digit = ConvertHexaDigit(++next);
                                        if (digit >= 16) [[unlikely]] {
                                            MarkError(next, "Invalid hexadecimal digit");
                                            return false;
                                        }
                                        c = (c << 4) | (int)digit;
                                    }

                                    str_buf.Append((char)c);
                                } break;
                                case 'u':
                                case 'U': {
                                    Size consume = (code[next] == 'U') ? 6 : 4;

                                    if (next > code.len - consume - 1) [[unlikely]] {
                                        MarkError(next + 1, "Truncated escape sequence (expected %1 hexadecimal digits)", consume);
                                        return false;
                                    }

                                    int32_t uc = 0;
                                    for (Size i = 0; i < consume; i++) {
                                        unsigned int digit = ConvertHexaDigit(++next);
                                        if (digit >= 16) [[unlikely]] {
                                            MarkError(next, "Invalid hexadecimal digit");
                                            return false;
                                        }
                                        uc = (uc << 4) | (int32_t)digit;
                                    }

                                    str_buf.Grow(4);
                                    Size bytes = EncodeUtf8(uc, str_buf.end());
                                    if (!bytes) [[unlikely]] {
                                        MarkError(next - consume, "Invalid UTF-8 codepoint");
                                        return false;
                                    }
                                    str_buf.len += bytes;
                                } break;
                                case '\\': { str_buf.Append('\\'); } break;
                                case '"': { str_buf.Append('"'); } break;
                                case '\'': { str_buf.Append('\''); } break;
                                case '0': { str_buf.Append(0); } break;

                                default: {
                                    MarkUnexpected(next, "Unsupported escape sequence");
                                    return false;
                                } break;
                            }

                            next++;
                        }
                    } else if ((uint8_t)code[next] < 128) {
                        str_buf.Append(code[next]);
                        next++;
                    } else {
                        int32_t uc;
                        Size bytes = DecodeUtf8(code, next, &uc);

                        if (!bytes) [[unlikely]] {
                            MarkError(next, "Invalid UTF-8 sequence");
                            return false;
                        }

                        str_buf.Append(code.Take(next, bytes));
                        next += bytes;
                    }
                }

                // Intern string
                {
                    bool inserted;
                    Span<const char> *ptr = strings.TrySet(str_buf, &inserted);

                    if (inserted) {
                        str_buf.Grow(1);
                        str_buf.ptr[str_buf.len] = 0;

                        *ptr = str_buf.TrimAndLeak(1);
                    }

                    tokens.Append({ bk_TokenKind::String, line, offset, { .str = ptr->ptr } });
                }
            } break;

            case '.': { Token1(bk_TokenKind::Dot); } break;
            case ':': { Token2('=', bk_TokenKind::Reassign) || Token1(bk_TokenKind::Colon); } break;
            case '(': { Token1(bk_TokenKind::LeftParenthesis); } break;
            case ')': { Token1(bk_TokenKind::RightParenthesis); } break;
            case '[': { Token1(bk_TokenKind::LeftBracket); } break;
            case ']': { Token1(bk_TokenKind::RightBracket); } break;
            case '+': { Token2('=', bk_TokenKind::PlusAssign) || Token1(bk_TokenKind::Plus); } break;
            case '-': { Token2('=', bk_TokenKind::MinusAssign) || Token1(bk_TokenKind::Minus); } break;
            case '*': { Token2('=', bk_TokenKind::MultiplyAssign) || Token1(bk_TokenKind::Multiply); } break;
            case '/': { Token2('=', bk_TokenKind::DivideAssign) || Token1(bk_TokenKind::Divide); } break;
            case '%': { Token2('=', bk_TokenKind::ModuloAssign) || Token1(bk_TokenKind::Modulo); } break;
            case '~': { Token2('=', bk_TokenKind::XorAssign) || Token1(bk_TokenKind::XorOrComplement); } break;
            case '&': { Token2('=', bk_TokenKind::AndAssign) || Token1(bk_TokenKind::And); } break;
            case '|': { Token2('=', bk_TokenKind::OrAssign) || Token1(bk_TokenKind::Or); } break;
            case '!': {
                if (!Token2('=', bk_TokenKind::NotEqual)) {
                    MarkUnexpected(offset, "Unexpected");
                    return false;
                };
            } break;
            case '=': { Token1(bk_TokenKind::Equal); } break;
            case '>': { Token4('>', '>', '=', bk_TokenKind::RightRotateAssign) || Token3('>', '>', bk_TokenKind::RightRotate) ||
                        Token3('>', '=', bk_TokenKind::RightShiftAssign) || Token2('>', bk_TokenKind::RightShift) ||
                        Token2('=', bk_TokenKind::GreaterOrEqual) || Token1(bk_TokenKind::Greater); } break;
            case '<': { Token4('<', '<', '=', bk_TokenKind::LeftRotateAssign) || Token3('<', '<', bk_TokenKind::LeftRotate) ||
                        Token3('<', '=', bk_TokenKind::LeftShiftAssign) || Token2('<', bk_TokenKind::LeftShift) ||
                        Token2('=', bk_TokenKind::LessOrEqual) || Token1(bk_TokenKind::Less); } break;
            case ',': { Token1(bk_TokenKind::Comma); } break;
            case ';': { Token1(bk_TokenKind::Semicolon); } break;

            default: {
                if (IsAsciiAlpha(code[offset]) || code[offset] == '_') [[likely]] {
                    // Go on!
                } else if ((uint8_t)code[offset] >= 128) {
                    int32_t uc = 0;
                    Size bytes = DecodeUtf8(code, offset, &uc);

                    if (!IsXidStart(uc)) [[unlikely]] {
                        MarkUnexpected(offset, "Identifiers cannot start with");
                        return false;
                    }

                    next += bytes - 1;
                } else {
                    MarkUnexpected(offset, "Unexpected");
                    return false;
                }

                while (next < code.len) {
                    if (IsAsciiAlphaOrDigit(code[next]) || code[next] == '_') {
                        next++;
                    } else if ((uint8_t)code[next] >= 128) [[unlikely]] {
                        int32_t uc = 0;
                        Size bytes = DecodeUtf8(code, next, &uc);

                        if (!IsXidContinue(uc)) {
                            MarkUnexpected(next, "Identifiers cannot contain");
                            return false;
                        }

                        next += bytes;
                    } else {
                        break;
                    }
                }

                Span<const char> ident = code.Take(offset, next - offset);
                const bk_Token *keyword = KeywordsMap.Find(ident);

                if (keyword) {
                    // In order to have order-independent top-level records and functions, we need
                    // to parse their declarations first! Tell the parser where to look to help it.
                    if (keyword->kind == bk_TokenKind::Func ||
                            keyword->kind == bk_TokenKind::Record ||
                            keyword->kind == bk_TokenKind::Enum) {
                        file->prototypes.Append(tokens.len);
                    }

                    tokens.Append({ keyword->kind, line, offset, keyword->u });
                } else {
                    // Intern string

                    bool inserted;
                    Span<const char> *ptr = strings.TrySet(ident, &inserted);

                    if (inserted) {
                        *ptr = DuplicateString(ident, &file->str_alloc);
                    }

                    tokens.Append({ bk_TokenKind::Identifier, line, offset, { .str = ptr->ptr } });
                }
            } break;
        }
    }

    // Newlines are used to end statements. Make sure the last statement has one.
    Token1(bk_TokenKind::EndOfLine);

    if (valid) {
        file->filename = DuplicateString(filename, &file->str_alloc).ptr;
        file->code = code;
        tokens.Trim();
        file->prototypes.Trim();

        err_guard.Disable();
    }

    return valid;
}

inline bool bk_Lexer::Token1(bk_TokenKind kind)
{
    tokens.Append({ kind, line, offset, {} });
    return true;
}

inline bool bk_Lexer::Token2(char c, bk_TokenKind kind)
{
    if (next < code.len && code[next] == c) {
        tokens.Append({ kind, line, offset, {} });
        next++;

        return true;
    } else {
        return false;
    }
}

inline bool bk_Lexer::Token3(char c1, char c2, bk_TokenKind kind)
{
    if (next < code.len - 1 && code[next] == c1 && code[next + 1] == c2) {
        tokens.Append({ kind, line, offset, {} });
        next += 2;

        return true;
    } else {
        return false;
    }
}

inline bool bk_Lexer::Token4(char c1, char c2, char c3, bk_TokenKind kind)
{
    if (next < code.len - 2 && code[next] == c1 && code[next + 1] == c2 && code[next + 2] == c3) {
        tokens.Append({ kind, line, offset, {} });
        next += 3;

        return true;
    } else {
        return false;
    }
}

unsigned int bk_Lexer::ConvertHexaDigit(Size pos)
{
    if (IsAsciiDigit(code[pos])) {
        return (unsigned int)(code[pos] - '0');
    } else if (code[pos] >= 'A' && code[pos] <= 'F') {
        return (unsigned int)(code[pos] - 'A' + 10);
    } else {
        return (unsigned int)(code[pos] - 'a' + 10);
    }
}

// Expects offset to point to the start of the literal
void bk_Lexer::TokenizeFloat()
{
    double d;

    fast_float::from_chars_result ret = fast_float::from_chars(code.ptr + offset, code.end(), d);
    if (ret.ec != std::errc()) [[unlikely]] {
        MarkError(offset, "Malformed float number");
        return;
    }
    next = ret.ptr - code.ptr;

    if (code[next - 1] == '.') [[unlikely]] {
        MarkError(offset, "Malformed float number");
        return;
    }
    if (next < code.len && IsAsciiAlpha(code[next])) [[unlikely]] {
        MarkError(offset, "Malformed float number");
        return;
    }

    tokens.Append({ bk_TokenKind::Float, line, offset, { .d = d } });
}

bool bk_Tokenize(Span<const char> code, const char *filename, bk_TokenizedFile *out_file)
{
    bk_Lexer lexer(out_file);
    return lexer.Tokenize(code, filename);
}

}
