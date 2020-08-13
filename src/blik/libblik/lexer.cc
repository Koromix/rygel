// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "error.hh"
#include "lexer.hh"
#include "lexer_xid.hh"

namespace RG {

static const HashMap<Span<const char>, Token> KeywordsMap {
    {"func", {TokenKind::Func}},
    {"return", {TokenKind::Return}},
    {"let", {TokenKind::Let}},
    {"mut", {TokenKind::Mut}},
    {"begin", {TokenKind::Begin}},
    {"end", {TokenKind::End}},
    {"if", {TokenKind::If}},
    {"else", {TokenKind::Else}},
    {"while", {TokenKind::While}},
    {"for", {TokenKind::For}},
    {"in", {TokenKind::In}},
    {"break", {TokenKind::Break}},
    {"continue", {TokenKind::Continue}},
    {"do", {TokenKind::Do}},
    {"null", {TokenKind::Null}},
    {"true", {TokenKind::Bool, 0, 0, {.b = true}}},
    {"false", {TokenKind::Bool, 0, 0, {.b = false}}}
};

class Lexer {
    RG_DELETE_COPY(Lexer)

    const char *filename;
    Span<const char> code;
    Size offset;
    Size next;
    int32_t line;
    bool valid;

    HashSet<Span<const char>> strings;

    TokenizedFile *file;
    HeapArray<Token> &tokens;

public:
    Lexer(TokenizedFile *file);

    bool Tokenize(Span<const char> code, const char *filename);

private:
    inline bool Token1(TokenKind tok);
    inline bool Token2(char c, TokenKind tok);
    inline bool Token3(char c1, char c2, TokenKind tok);
    inline bool Token4(char c1, char c2, char c3, TokenKind tok);

    unsigned int ConvertHexaDigit(Size pos);

    void TokenizeFloat();

    template <typename... Args>
    void MarkError(Size offset, const char *fmt, Args... args)
    {
        if (valid) {
            ReportDiagnostic(DiagnosticType::Error, code, filename,
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
            MarkError(offset, "%1 byte 0x%2", prefix, FmtHex(code[offset]).Pad0(-2));
        } else {
            MarkError(offset, "%1 character '%2'", prefix, code.Take(offset, bytes));
        }
    }
};

Lexer::Lexer(TokenizedFile *file)
    : file(file), tokens(file->tokens)
{
    RG_ASSERT(file);
}

static bool TestUnicodeTable(Span<const int32_t> table, int32_t uc)
{
    RG_ASSERT(table.len > 0);
    RG_ASSERT(table.len % 2 == 0);

    auto it = std::upper_bound(table.begin(), table.end(), uc,
                               [](int32_t uc, int32_t x) { return uc < x; });
    Size idx = it - table.ptr;

    // Each pair of value in table represents a valid interval
    return idx & 0x1;
}

bool Lexer::Tokenize(Span<const char> code, const char *filename)
{
    RG_DEFER_NC(err_guard, tokens_len = tokens.len,
                           funcs_len = file->funcs.len) {
        tokens.RemoveFrom(tokens_len);
        file->funcs.RemoveFrom(funcs_len);
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
                        // We limit to IN64_MAX, but we build with -ftrapv in debug builds.
                        // Don't replace with int64_t or debug builds will crash on overflow!
                        uint64_t value = 0;
                        bool overflow = false;

                        while (++next < code.len) {
                            unsigned int digit = (unsigned int)(code[next] - '0');

                            if (digit < 2) {
                                overflow |= (value > ((uint64_t)INT64_MAX - digit) / 2);
                                value = (value * 2) + digit;
                            } else if (RG_UNLIKELY(digit < 10)) {
                                MarkUnexpected(next, "Invalid binary digit");
                                return false;
                            } else if (code[next] == '_') {
                                // Ignore underscores in number literals (e.g. 0b1000_0000_0001)
                            } else {
                                break;
                            }
                        }

                        if (RG_UNLIKELY(overflow)) {
                            MarkError(offset, "Number literal is too big (max = %1)", INT64_MAX);
                            return false;
                        }

                        tokens.Append({TokenKind::Integer, line, offset, {.i = (int64_t)value}});
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
                            } else if (RG_UNLIKELY(digit < 10)) {
                                MarkUnexpected(next, "Invalid octal digit");
                                return false;
                            } else if (code[next] == '_') {
                                // Ignore underscores in number literals (e.g. 0o700_777)
                            } else {
                                break;
                            }
                        }

                        if (RG_UNLIKELY(overflow)) {
                            MarkError(offset, "Number literal is too big (max = %1)", INT64_MAX);
                            return false;
                        }

                        tokens.Append({TokenKind::Integer, line, offset, {.i = (int64_t)value}});
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
                                } else if (code[next] == '_') {
                                    // Ignore underscores in number literals (e.g. 0b1000_0000_0001)
                                    continue;
                                } else {
                                    break;
                                }
                            }

                            overflow |= (value > ((uint64_t)INT64_MAX - digit) / 16);
                            value = (value * 16) + digit;
                        }

                        if (RG_UNLIKELY(overflow)) {
                            MarkError(offset, "Number literal is too big (max = %1)", INT64_MAX);
                            return false;
                        }

                        tokens.Append({TokenKind::Integer, line, offset, {.i = (int64_t)value}});
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
                    } else if (code[next] == '_') {
                        // Ignore underscores in number literals (e.g. 10_000_000)
                    } else {
                        break;
                    }

                    next++;
                }

                if (fp) {
                    next++;
                    TokenizeFloat();
                } else {
                    if (RG_UNLIKELY(overflow)) {
                        MarkError(offset, "Number literal is too big (max = %1)", INT64_MAX);
                        return false;
                    }

                    tokens.Append({TokenKind::Integer, line, offset, {.i = (int64_t)value}});
                }
            } break;

            case '"':
            case '\'': {
                str_buf.RemoveFrom(0);

                for (;;) {
                    if (RG_UNLIKELY(next >= code.len || code[next] == '\n')) {
                        MarkError(offset, "Unfinished string literal");
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
                                case 'r': { str_buf.Append('\r'); } break;
                                case 'n': { str_buf.Append('\n'); } break;
                                case 't': { str_buf.Append('\t'); } break;
                                case 'f': { str_buf.Append('\f'); } break;
                                case 'v': { str_buf.Append('\v'); } break;
                                case 'a': { str_buf.Append('\a'); } break;
                                case 'b': { str_buf.Append('\b'); } break;
                                case 'e': { str_buf.Append('\x1B'); } break;
                                case 'x': {
                                    if (RG_UNLIKELY(next > code.len - 3)) {
                                        MarkError(next + 1, "Truncated escape sequence");
                                        return false;
                                    }

                                    int c = 0;
                                    for (Size i = 0; i < 2; i++) {
                                        unsigned int digit = ConvertHexaDigit(++next);
                                        if (RG_UNLIKELY(digit >= 16)) {
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

                                    if (RG_UNLIKELY(next > code.len - consume - 1)) {
                                        MarkError(next + 1, "Truncated escape sequence (expected %1 hexadecimal digits)", consume);
                                        return false;
                                    }

                                    int32_t uc = 0;
                                    for (Size i = 0; i < consume; i++) {
                                        unsigned int digit = ConvertHexaDigit(++next);
                                        if (RG_UNLIKELY(digit >= 16)) {
                                            MarkError(next, "Invalid hexadecimal digit");
                                            return false;
                                        }
                                        uc = (uc << 4) | (int32_t)digit;
                                    }

                                    str_buf.Grow(4);
                                    Size bytes = EncodeUtf8(uc, str_buf.end());
                                    if (RG_UNLIKELY(!bytes)) {
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
                        Size bytes = DecodeUtf8(code.ptr, next, &uc);

                        if (RG_UNLIKELY(!bytes)) {
                            MarkError(next, "Invalid UTF-8 sequence");
                            return false;
                        }

                        str_buf.Append(code.Take(next, bytes));
                        next += bytes;
                    }
                }

                // Intern string
                std::pair<Span<const char> *, bool> ret = strings.TrySet(str_buf);
                if (ret.second) {
                    str_buf.Append(0);
                    ret.first->ptr = str_buf.TrimAndLeak().ptr;
                }

                tokens.Append({TokenKind::String, line, offset, {.str = ret.first->ptr}});
            } break;

            case '.': {
                if (RG_UNLIKELY(next < code.len && (IsAsciiDigit(code[next]) ||
                                                    code[next] == 'e' || code[next] == 'E'))) {
                    // Support '.dddd' float literals
                    TokenizeFloat();
                } else {
                    Token1(TokenKind::Dot);
                }
            } break;
            case ':': { Token2('=', TokenKind::Reassign) || Token1(TokenKind::Colon); } break;
            case '(': { Token1(TokenKind::LeftParenthesis); } break;
            case ')': { Token1(TokenKind::RightParenthesis); } break;
            case '+': { Token2('=', TokenKind::PlusAssign) || Token1(TokenKind::Plus); } break;
            case '-': { Token2('=', TokenKind::MinusAssign) || Token1(TokenKind::Minus); } break;
            case '*': { Token2('=', TokenKind::MultiplyAssign) || Token1(TokenKind::Multiply); } break;
            case '/': { Token2('=', TokenKind::DivideAssign) || Token1(TokenKind::Divide); } break;
            case '%': { Token2('=', TokenKind::ModuloAssign) || Token1(TokenKind::Modulo); } break;
            case '~': { Token2('=', TokenKind::XorAssign) || Token1(TokenKind::XorOrComplement); } break;
            case '&': { Token2('=', TokenKind::AndAssign) || Token2('&', TokenKind::AndAnd) || Token1(TokenKind::And); } break;
            case '|': { Token2('=', TokenKind::OrAssign) || Token2('|', TokenKind::OrOr) || Token1(TokenKind::Or); } break;
            case '!': { Token2('=', TokenKind::NotEqual) || Token1(TokenKind::Not); } break;
            case '=': { Token2('=', TokenKind::Equal) || Token1(TokenKind::Assign); } break;
            case '>': { Token4('>', '>', '=', TokenKind::RightRotateAssign) || Token3('>', '>', TokenKind::RightRotate) ||
                        Token3('>', '=', TokenKind::RightShiftAssign) || Token2('>', TokenKind::RightShift) ||
                        Token2('=', TokenKind::GreaterOrEqual) || Token1(TokenKind::Greater); } break;
            case '<': { Token4('<', '<', '=', TokenKind::LeftRotateAssign) || Token3('<', '<', TokenKind::LeftRotate) ||
                        Token3('<', '=', TokenKind::LeftShiftAssign) || Token2('<', TokenKind::LeftShift) ||
                        Token2('=', TokenKind::LessOrEqual) || Token1(TokenKind::Less); } break;
            case ',': { Token1(TokenKind::Comma); } break;
            case ';': { Token1(TokenKind::Semicolon); } break;

            default: {
                if (RG_LIKELY(IsAsciiAlpha(code[offset]) || code[offset] == '_')) {
                    // Go on!
                } else if ((uint8_t)code[offset] >= 128) {
                    int32_t uc = 0;
                    Size bytes = DecodeUtf8(code.ptr, offset, &uc);

                    if (RG_UNLIKELY(!TestUnicodeTable(UnicodeIdStartTable, uc))) {
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
                    } else if (RG_UNLIKELY((uint8_t)code[next] >= 128)) {
                        int32_t uc = 0;
                        Size bytes = DecodeUtf8(code.ptr, next, &uc);

                        if (!TestUnicodeTable(UnicodeIdContinueTable, uc)) {
                            MarkUnexpected(next, "Identifiers cannot contain");
                            return false;
                        }

                        next += bytes;
                    } else {
                        break;
                    }
                }

                Span<const char> ident = code.Take(offset, next - offset);
                const Token *keyword = KeywordsMap.Find(ident);

                if (keyword) {
                    // In order to have order-independent top-level functions, we need to parse
                    // their declarations first! Tell the parser where to look to help it.
                    if (keyword->kind == TokenKind::Func) {
                        file->funcs.Append(tokens.len);
                    }

                    tokens.Append({keyword->kind, line, offset, keyword->u});
                } else {
                    // Intern string
                    std::pair<Span<const char> *, bool> ret = strings.TrySet(ident);
                    if (ret.second) {
                        ret.first->ptr = DuplicateString(ident, &file->str_alloc).ptr;
                    }
                    tokens.Append({TokenKind::Identifier, line, offset, {.str = ret.first->ptr}});
                }
            } break;
        }
    }

    // Newlines are used to end statements. Make sure the last statement has one.
    Token1(TokenKind::EndOfLine);

    if (valid) {
        file->filename = DuplicateString(filename, &file->str_alloc).ptr;
        file->code = code;
        tokens.Trim();
        file->funcs.Trim();

        err_guard.Disable();
    }

    return valid;
}

inline bool Lexer::Token1(TokenKind kind)
{
    tokens.Append({kind, line, offset});
    return true;
}

inline bool Lexer::Token2(char c, TokenKind kind)
{
    if (next < code.len && code[next] == c) {
        tokens.Append({kind, line, offset});
        next++;

        return true;
    } else {
        return false;
    }
}

inline bool Lexer::Token3(char c1, char c2, TokenKind kind)
{
    if (next < code.len - 1 && code[next] == c1 && code[next + 1] == c2) {
        tokens.Append({kind, line, offset});
        next += 2;

        return true;
    } else {
        return false;
    }
}

inline bool Lexer::Token4(char c1, char c2, char c3, TokenKind kind)
{
    if (next < code.len - 2 && code[next] == c1 && code[next + 1] == c2 && code[next + 2] == c3) {
        tokens.Append({kind, line, offset});
        next += 3;

        return true;
    } else {
        return false;
    }
}

unsigned int Lexer::ConvertHexaDigit(Size pos)
{
    if (IsAsciiDigit(code[pos])) {
        return (unsigned int)(code[pos] - '0');
    } else if (code[pos] >= 'A' && code[pos] <= 'F') {
        return (unsigned int)(code[pos] - 'A' + 10);
    } else {
        return (unsigned int)(code[pos] - 'a' + 10);
    }
}

// Expects offset to point to the start of the literal, and next to point just behind the dot
void Lexer::TokenizeFloat()
{
    LocalArray<char, 256> buf;
    for (Size i = offset; i < next; i++) {
        if (code[i] != '_') {
            if (RG_UNLIKELY(buf.len >= RG_SIZE(buf.data) - 2)) {
                MarkError(offset, "Number literal is too long (max = %1 characters)", RG_SIZE(buf.data) - 1);
                return;
            }

            buf.Append(code[i]);
        }
    }
    while (next < code.len) {
        if (IsAsciiDigit(code[next]) || code[next] == 'e' || code[next] == 'E') {
            if (RG_UNLIKELY(buf.len >= RG_SIZE(buf.data) - 2)) {
                MarkError(offset, "Number literal is too long (max = %1 characters)", RG_SIZE(buf.data) - 1);
                return;
            }

            buf.Append(code[next]);
        } else if (code[next] == '_') {
            // Skip
        } else if (code[next] == '.') {
            MarkError(next, "Number literals cannot contain multiple '.' characters");
            return;
        } else {
            break;
        }

        next++;
    }
    buf.Append(0);

    errno = 0;

    // XXX: We need to ditch libc here eventually, and use something fast and well defined across
    // platforms. Or maybe std::from_chars()? Not sure it solves the cross-platform issue...
    // And my first try went wrong with MinGW-w64 (gcc 10.1.0), with a vomit of template errors :/
    char *end = nullptr;
    double d = strtod(buf.data, &end);

    if (RG_UNLIKELY(errno == ERANGE)) {
        MarkError(offset, "Float literal exceeds supported range");
        return;
    }

    tokens.Append({TokenKind::Float, line, offset, {.d = d}});
}

bool Tokenize(Span<const char> code, const char *filename, TokenizedFile *out_file)
{
    Lexer lexer(out_file);
    return lexer.Tokenize(code, filename);
}

}
