// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "error.hh"
#include "lexer.hh"
#include "lexer_xid.hh"

namespace RG {

class Lexer {
    RG_DELETE_COPY(Lexer)

    const char *filename;
    Span<const char> code;
    Size offset;
    Size next;
    int32_t line;
    bool valid;

    HashSet<Span<const char>> strings;

    TokenizedFile *out_file;
    HeapArray<Token> &tokens;

public:
    Lexer(TokenizedFile *out_file);

    bool Tokenize(Span<const char> code, const char *filename);

private:
    inline bool Token1(TokenKind tok);
    inline bool Token2(char c, TokenKind tok);
    inline bool Token3(char c1, char c2, TokenKind tok);

    template <typename... Args>
    void MarkError(Size offset, const char *fmt, Args... args)
    {
        if (valid) {
            ReportDiagnostic(DiagnosticType::Error, code, filename,
                             line, offset, fmt, args...);
            valid = false;
        }
    }
};

static bool TestUnicodeTable(Span<const uint32_t> table, uint32_t c)
{
    RG_ASSERT(table.len > 0);
    RG_ASSERT(table.len % 2 == 0);

    auto it = std::upper_bound(table.begin(), table.end(), c,
                               [](uint32_t c, uint32_t x) { return c < x; });
    Size idx = it - table.ptr;

    // Each pair of value in table represents a valid interval
    return idx & 0x1;
}

Lexer::Lexer(TokenizedFile *out_file)
    : out_file(out_file), tokens(out_file->tokens)
{
    RG_ASSERT(out_file);
    RG_ASSERT(!out_file->filename);
}

bool Lexer::Tokenize(Span<const char> code, const char *filename)
{
    RG_ASSERT(!tokens.len);

    RG_DEFER_N(err_guard) {
        tokens.Clear();
        out_file->funcs.Clear();
        out_file->str_alloc.ReleaseAll();
    };

    // Make sure we only have one EndOfLine token at the end. Without it some parser errors
    // caused by premature end of file may be not be located correctly.
    code = TrimStrRight(code);

    this->filename = filename;
    this->code = code;
    line = 1;

    valid = true;

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

                        tokens.Append({TokenKind::Integer, line, offset, {.i = value}});
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

                        tokens.Append({TokenKind::Integer, line, offset, {.i = value}});
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

                        tokens.Append({TokenKind::Integer, line, offset, {.i = value}});
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

                    tokens.Append({TokenKind::Float, line, offset, {.d = d}});
                } else {
                    if (RG_UNLIKELY(overflow)) {
                        MarkError(offset, "Number literal is too large (max = %1)", INT64_MAX);
                        return false;
                    }

                    tokens.Append({TokenKind::Integer, line, offset, {.i = value}});
                }
            } break;

            case '"':
            case '\'': {
                HeapArray<char> str(&out_file->str_alloc);

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

                            next++;
                        }
                    } else if ((uint8_t)code[next] < 128) {
                        str.Append(code[next]);
                        next++;
                    } else {
                        uint32_t c;
                        Size bytes = DecodeUtf8(code.ptr, next, &c);

                        if (RG_UNLIKELY(!bytes)) {
                            MarkError(next, "Invalid UTF-8 sequence");
                            return false;
                        }

                        str.Append(code.Take(next, bytes));
                        next += bytes;
                    }
                }

                // Intern string
                std::pair<Span<const char> *, bool> ret = strings.Append(str);
                if (ret.second) {
                    str.Append(0);
                    ret.first->ptr = str.TrimAndLeak().ptr;
                }

                tokens.Append({TokenKind::String, line, offset, {.str = ret.first->ptr}});
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
            case '~': { Token1(TokenKind::Complement); } break;
            case '&': { Token2('=', TokenKind::AndAssign) || Token2('&', TokenKind::AndAnd) || Token1(TokenKind::And); } break;
            case '|': { Token2('=', TokenKind::OrAssign) || Token2('|', TokenKind::OrOr) || Token1(TokenKind::Or); } break;
            case '!': { Token2('=', TokenKind::NotEqual) || Token1(TokenKind::Not); } break;
            case '=': { Token2('=', TokenKind::Equal) || Token1(TokenKind::Assign); } break;
            case '>': { Token3('>', '=', TokenKind::RightShiftAssign) || Token2('>', TokenKind::RightShift) ||
                        Token2('=', TokenKind::GreaterOrEqual) || Token1(TokenKind::Greater); } break;
            case '<': { Token3('<', '=', TokenKind::LeftShiftAssign) || Token2('<', TokenKind::LeftShift) ||
                        Token2('=', TokenKind::LessOrEqual) || Token1(TokenKind::Less); } break;
            case ',': { Token1(TokenKind::Comma); } break;

            default: {
                if (RG_LIKELY(IsAsciiAlpha(code[offset]) || code[offset] == '_')) {
                    // Go on!
                } else if ((uint8_t)code[offset] >= 128) {
                    uint32_t c = 0;
                    Size bytes = DecodeUtf8(code.ptr, offset, &c);

                    if (RG_UNLIKELY(!TestUnicodeTable(UnicodeIdStartTable, c))) {
                        if (bytes > 0) {
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
                        uint32_t c = 0;
                        Size bytes = DecodeUtf8(code.ptr, next, &c);

                        if (!TestUnicodeTable(UnicodeIdContinueTable, c)) {
                            if (bytes > 0) {
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
                    out_file->funcs.Append(tokens.len);
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
                    tokens.Append({TokenKind::Bool, line, offset, {.b = true}});
                } else if (ident == "false") {
                    tokens.Append({TokenKind::Bool, line, offset, {.b = false}});
                } else {
                    // Intern string
                    std::pair<Span<const char> *, bool> ret = strings.Append(ident);
                    if (ret.second) {
                        ret.first->ptr = DuplicateString(ident, &out_file->str_alloc).ptr;
                    }
                    tokens.Append({TokenKind::Identifier, line, offset, {.str = ret.first->ptr}});
                }
            } break;
        }
    }

    // Newlines are used to end statements. Make sure the last statement has one.
    Token1(TokenKind::EndOfLine);

    if (valid) {
        out_file->filename = DuplicateString(filename, &out_file->str_alloc).ptr;
        out_file->code = code;
        tokens.Trim();
        out_file->funcs.Trim();

        err_guard.Disable();
    }

    return valid;
}

inline bool Lexer::Token1(TokenKind tok)
{
    tokens.Append({tok, line, offset});
    return true;
}

inline bool Lexer::Token2(char c, TokenKind tok)
{
    if (next < code.len && code[next] == c) {
        tokens.Append({tok, line, offset});
        next++;

        return true;
    } else {
        return false;
    }
}

inline bool Lexer::Token3(char c1, char c2, TokenKind tok)
{
    if (next + 1 < code.len && code[next] == c1 && code[next + 1] == c2) {
        tokens.Append({tok, line, offset});
        next += 2;

        return true;
    } else {
        return false;
    }
}

bool Tokenize(Span<const char> code, const char *filename, TokenizedFile *out_file)
{
    Lexer lexer(out_file);
    return lexer.Tokenize(code, filename);
}

}
