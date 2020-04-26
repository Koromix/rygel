// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "error.hh"
#include "lexer.hh"
#include "lexer_xid.hh"

namespace RG {

class Lexer {
    bool valid = true;

    HashSet<Span<const char>> strings;

    const char *filename;
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
    bool Token3(char c1, char c2, TokenKind tok);

    template <typename... Args>
    void MarkError(Size offset, const char *fmt, Args... args)
    {
        if (valid) {
            ReportDiagnostic(DiagnosticType::Error, code, filename, line, offset, fmt, args...);
            valid = false;
        }
    }
};

static inline Size DecodeUtf8(Span<const char> str, Size offset, int32_t *out_c)
{
    RG_ASSERT(offset < str.len);

    const uint8_t *ptr = (const uint8_t *)(str.ptr + offset);
    Size available = str.len - offset;

    if (ptr[0] < 0x80) {
        *out_c = ptr[0];
        return 1;
    } else if (RG_UNLIKELY(ptr[0] - 0xC2 > 0xF4 - 0xC2)) {
        return -1;
    } else if (ptr[0] < 0xE0 &&
               RG_LIKELY(available >= 2 && (ptr[1] & 0xC0) == 0x80)) {
        *out_c = ((ptr[0] & 0x1F) << 6) | (ptr[1] & 0x3F);
        return 2;
    } else if (ptr[0] < 0xF0 &&
               RG_LIKELY(available >= 3 && (ptr[1] & 0xC0) == 0x80 &&
                                           (ptr[2] & 0xC0) == 0x80)) {
        *out_c = ((ptr[0] & 0xF) << 12) | ((ptr[1] & 0x3F) << 6) | (ptr[2] & 0x3F);
        return 3;
    } else if (RG_LIKELY(available >= 4 && (ptr[1] & 0xC0) == 0x80 &&
                                           (ptr[2] & 0xC0) == 0x80 &&
                                           (ptr[3] & 0xC0) == 0x80)) {
        *out_c = ((ptr[0] & 0x7) << 18) | ((ptr[1] & 0x3F) << 12) | ((ptr[2] & 0x3F) << 6) | (ptr[3] & 0x3F);
        return 4;
    } else {
        return -1;
    }
}

static bool TestUnicodeTable(Span<const int32_t> table, int32_t c)
{
    RG_ASSERT(table.len > 0);
    RG_ASSERT(table.len % 2 == 0);

    if (c >= table[0] && c <= table[table.len - 1]) {
        Size start = 0;
        Size end = table.len;

        while (end > start + 1) {
            Size idx = start + (end - start) / 2;

            if (c >= table[idx]) {
                start = idx;
            } else {
                end = idx;
            }
        }

        return (start % 2) == 0;
    } else {
        return false;
    }
}

bool Lexer::Tokenize(Span<const char> code, const char *filename)
{
    RG_ASSERT(valid);

    // Make sure we only have one EndOfLine token at the end. Without it some parser errors
    // caused by premature end of file may be not be located correctly.
    code = TrimStrRight(code);

    this->filename = filename;
    this->code = code;
    line = 1;

    for (offset = 0, next = 1; offset < code.len; offset = next++) {
        switch (code[offset]) {
            case ' ':
            case '\t':
            case '\r': {} break;

            case '\n': {
                Token1(TokenKind::EndOfLine);
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
                                MarkError(next, "Invalid binary digit '%1'", code[next]);
                                return false;
                            } else {
                                break;
                            }
                        }

                        if (RG_UNLIKELY(overflow)) {
                            MarkError(offset, "Number literal is too large (max = %1)", INT64_MAX);
                            return false;
                        }

                        set.tokens.Append({TokenKind::Integer, line, offset, {.i = value}});
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
                                MarkError(next, "Invalid octal digit '%1'", code[next]);
                                return false;
                            } else {
                                break;
                            }
                        }

                        if (RG_UNLIKELY(overflow)) {
                            MarkError(offset, "Number literal is too large (max = %1)", INT64_MAX);
                            return false;
                        }

                        set.tokens.Append({TokenKind::Integer, line, offset, {.i = value}});
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
                                MarkError(next, "Invalid hexadecimal digit '%1'", code[next]);
                                return false;
                            } else {
                                break;
                            }
                        }

                        if (RG_UNLIKELY(overflow)) {
                            MarkError(offset, "Number literal is too large (max = %1)", INT64_MAX);
                            return false;
                        }

                        set.tokens.Append({TokenKind::Integer, line, offset, {.i = value}});
                        continue;
                    } else {
                        MarkError(next, "Invalid literal base character '%1'", code[next]);
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
                        MarkError(offset, "Float value exceeds supported range");
                        return false;
                    }

                    set.tokens.Append({TokenKind::Float, line, offset, {.d = d}});
                } else {
                    if (RG_UNLIKELY(overflow)) {
                        MarkError(offset, "Number literal is too large (max = %1)", INT64_MAX);
                        return false;
                    }

                    set.tokens.Append({TokenKind::Integer, line, offset, {.i = value}});
                }
            } break;

            case '"':
            case '\'': {
                HeapArray<char> str(&set.str_alloc);

                for (;;) {
                    if (RG_UNLIKELY(next >= code.len || code[next] == '\n')) {
                        MarkError(next, "Unfinished string literal");
                        return false;
                    }
                    if (RG_UNLIKELY(code[next] == '\r')) {
                        MarkError(next, "Carriage return is not allowed in string literals, use \\r");
                        return false;
                    }

                    if (code[next] == code[offset]) {
                        next++;
                        break;
                    } else if (code[next] == '\\') {
                        if (++next < code.len) {
                            switch (code[next]) {
                                case 'r': { str.Append('\r'); } break;
                                case 'n': { str.Append('\n'); } break;
                                case 't': { str.Append('\t'); } break;
                                case 'f': { str.Append('\f'); } break;
                                case 'v': { str.Append('\v'); } break;
                                case 'a': { str.Append('\a'); } break;
                                case 'b': { str.Append('\b'); } break;
                                case 'e': { str.Append('\x1B'); } break;
                                case '\\': { str.Append('\\'); } break;
                                case '"': { str.Append('"'); } break;
                                case '\'': { str.Append('\''); } break;
                                case '0': { str.Append(0); } break;

                                default: {
                                    if (code[next] >= 32 && (uint8_t)code[next] < 128) {
                                        MarkError(next, "Unsupported escape sequence '\\%1'", code[next]);
                                    } else {
                                        MarkError(next, "Unsupported escape sequence byte '\\0x%1",
                                                  FmtHex(code[next]).Pad0(-2));
                                    }

                                    return false;
                                } break;
                            }
                        }
                    } else {
                        int32_t c;
                        if (RG_UNLIKELY(DecodeUtf8(code, next, &c) < 0)) {
                            MarkError(next, "Invalid UTF-8 sequence");
                            return false;
                        }

                        str.Append(code[next]);
                    }

                    next++;
                }

                // Intern string
                std::pair<Span<const char> *, bool> ret = strings.Append(str);
                if (ret.second) {
                    str.Append(0);
                    ret.first->ptr = str.TrimAndLeak().ptr;
                }

                set.tokens.Append({TokenKind::String, line, offset, {.str = ret.first->ptr}});
            } break;

            case '.': { Token3('.', '.', TokenKind::DotDotDot) || Token2('.', TokenKind::DotDot) || Token1(TokenKind::Dot); } break;
            case ':': { Token2('=', TokenKind::Reassign) || Token1(TokenKind::Colon); } break;
            case '(': { Token1(TokenKind::LeftParenthesis); } break;
            case ')': { Token1(TokenKind::RightParenthesis); } break;
            case '+': { Token2('=', TokenKind::PlusAssign) || Token1(TokenKind::Plus); } break;
            case '-': { Token2('=', TokenKind::MinusAssign) || Token1(TokenKind::Minus); } break;
            case '*': { Token2('=', TokenKind::MultiplyAssign) || Token1(TokenKind::Multiply); } break;
            case '/': { Token2('=', TokenKind::DivideAssign) || Token1(TokenKind::Divide); } break;
            case '%': { Token2('=', TokenKind::ModuloAssign) || Token1(TokenKind::Modulo); } break;
            case '^': { Token2('=', TokenKind::XorAssign) || Token1(TokenKind::Xor); } break;
            case '~': { Token1(TokenKind::Not); } break;
            case '=': { Token2('=', TokenKind::Equal) || Token1(TokenKind::Assign); } break;
            case '!': { Token2('=', TokenKind::NotEqual) || Token1(TokenKind::LogicNot); } break;
            case '&': { Token2('=', TokenKind::AndAssign) || Token2('&', TokenKind::LogicAnd) || Token1(TokenKind::And); } break;
            case '|': { Token2('=', TokenKind::OrAssign) || Token2('|', TokenKind::LogicOr) || Token1(TokenKind::Or); } break;
            case '>': { Token3('>', '=', TokenKind::RightShiftAssign) || Token2('>', TokenKind::RightShift) ||
                        Token2('=', TokenKind::GreaterOrEqual) || Token1(TokenKind::Greater); } break;
            case '<': { Token3('<', '=', TokenKind::LeftShiftAssign) || Token2('<', TokenKind::LeftShift) ||
                        Token2('=', TokenKind::LessOrEqual) || Token1(TokenKind::Less); } break;
            case ',': { Token1(TokenKind::Comma); } break;

            default: {
                if (RG_LIKELY(IsAsciiAlpha(code[offset]) || code[offset] == '_')) {
                    // Go on!
                } else if ((uint8_t)code[offset] >= 128) {
                    int32_t c = -1;
                    Size bytes = DecodeUtf8(code.ptr, offset, &c);

                    if (RG_UNLIKELY(!TestUnicodeTable(UnicodeIdStartTable, c))) {
                        if (bytes >= 0) {
                            MarkError(offset, "Character '%1' is not allowed at the beginning of identifiers", code.Take(offset, bytes));
                        } else {
                            MarkError(offset, "Invalid UTF-8 sequence");
                        }

                        return false;
                    }

                    next += bytes - 1;
                } else if (code[offset] >= 32) {
                    MarkError(offset, "Unexpected character '%1'", code[offset]);
                    return false;
                } else {
                    MarkError(offset, "Unexpected byte 0x%1", FmtHex(code[offset]).Pad0(-2));
                    return false;
                }

                while (next < code.len) {
                    if (IsAsciiAlphaOrDigit(code[next]) || code[next] == '_') {
                        next++;
                    } else if (RG_UNLIKELY((uint8_t)code[next] >= 128)) {
                        int32_t c = -1;
                        Size bytes = DecodeUtf8(code.ptr, next, &c);

                        if (!TestUnicodeTable(UnicodeIdContinueTable, c)) {
                            if (bytes >= 0) {
                                MarkError(next, "Character '%1' is not allowed in identifiers", code.Take(next, bytes));
                            } else {
                                MarkError(offset, "Invalid UTF-8 sequence");
                            }

                            return false;
                        }

                        next += bytes;
                    } else {
                        break;
                    }
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
                } else if (ident == "mut") {
                    Token1(TokenKind::Mut);
                } else if (ident == "begin") {
                    Token1(TokenKind::Begin);
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
                } else if (ident == "break") {
                    Token1(TokenKind::Break);
                } else if (ident == "continue") {
                    Token1(TokenKind::Continue);
                } else if (ident == "do") {
                    Token1(TokenKind::Do);
                } else if (ident == "null") {
                    Token1(TokenKind::Null);
                } else if (ident == "true") {
                    set.tokens.Append({TokenKind::Bool, line, offset, {.b = true}});
                } else if (ident == "false") {
                    set.tokens.Append({TokenKind::Bool, line, offset, {.b = false}});
                } else {
                    // Intern string
                    std::pair<Span<const char> *, bool> ret = strings.Append(ident);
                    if (ret.second) {
                        ret.first->ptr = DuplicateString(ident, &set.str_alloc).ptr;
                    }
                    set.tokens.Append({TokenKind::Identifier, line, offset, {.str = ret.first->ptr}});
                }
            } break;
        }
    }

    // Newlines are used to end statements. Make sure the last statement has one.
    Token1(TokenKind::EndOfLine);

    return valid;
}

void Lexer::Finish(TokenSet *out_set)
{
    RG_ASSERT(!out_set->tokens.len);

    set.tokens.Trim();
    set.code = code;
    SwapMemory(&set, out_set, RG_SIZE(set));
}

bool Lexer::Token1(TokenKind tok)
{
    set.tokens.Append({tok, line, offset});
    return true;
}

bool Lexer::Token2(char c, TokenKind tok)
{
    if (next < code.len && code[next] == c) {
        set.tokens.Append({tok, line, offset});
        next++;

        return true;
    } else {
        return false;
    }
}

bool Lexer::Token3(char c1, char c2, TokenKind tok)
{
    if (next + 1 < code.len && code[next] == c1 && code[next + 1] == c2) {
        set.tokens.Append({tok, line, offset});
        next += 2;

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
