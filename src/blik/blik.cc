// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

    LeftParenthesis = '(',
    RightParenthesis = ')',
    LeftBrace = '{',
    RightBrace = '}',

    Integer = 256,
    Double,
    String,
    Identifier,

    LogicAnd,
    LogicOr,
    Equal,
    NotEqual,

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

static bool Tokenize(Span<const char> code, const char *filename, TokenSet *out_set)
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
                if (j < code.len && code[j] == 'b') {
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
                        LogError("Number literal is too large (max = %1)", INT64_MAX);
                        valid = false;
                    }

                    out_set->tokens.Append(Token(TokenType::Integer, line, u));
                    continue;
                } else if (j < code.len && code[j] == 'x') {
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
                        LogError("Number literal is too large (max = %1)", INT64_MAX);
                        valid = false;
                    }

                    out_set->tokens.Append(Token(TokenType::Integer, line, u));
                    continue;
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
                        LogError("Number literal is too large (max = %1)", INT64_MAX);
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

static int GetOperatorPrecedence(TokenType type, bool assoc)
{
    switch (type) {
        case TokenType::Plus: { return 10; } break;
        case TokenType::Minus: { return 10; } break;
        case TokenType::Multiply: { return 11; } break;
        case TokenType::Divide: { return 11; } break;
        case TokenType::Modulo: { return 11; } break;
        case TokenType::And: { return 6; } break;
        case TokenType::Or: { return 4; } break;
        case TokenType::Xor: { return 5; } break;
        case TokenType::Not: { return 12 - assoc; } break;
        case TokenType::LogicNot: { return 12 - assoc; } break;
        case TokenType::LogicAnd: { return 3; } break;
        case TokenType::LogicOr: { return 2; } break;
        case TokenType::Equal: { return 7; } break;
        case TokenType::NotEqual: { return 7; } break;

        default: { return -1; } break;
    }
}

static Size PostFixExpression(Span<Token> tokens)
{
    HeapArray<Token> stack;
    Size new_len = 0;

    bool expect_op = false;
    for (const Token &tok: tokens) {
        if (tok.type == TokenType::LeftParenthesis) {
            if (RG_UNLIKELY(expect_op))
                goto expected_op;

            stack.Append(tok);
        } else if (tok.type == TokenType::RightParenthesis) {
            if (RG_UNLIKELY(!expect_op))
                goto expected_value;
            expect_op = true;

            for (;;) {
                if (RG_UNLIKELY(!stack.len)) {
                    LogError("Too many closing parentheses");
                    return -1;
                }

                const Token &op = stack[stack.len - 1];

                if (op.type == TokenType::LeftParenthesis) {
                    stack.RemoveLast(1);
                    break;
                }

                tokens[new_len++] = op;
                stack.RemoveLast(1);
            }
        } else if (tok.type == TokenType::Identifier || tok.type == TokenType::Integer ||
                   tok.type == TokenType::Double || tok.type == TokenType::String) {
            if (RG_UNLIKELY(expect_op))
                goto expected_op;
            expect_op = true;

            tokens[new_len++] = tok;
        } else {
            int prec = GetOperatorPrecedence(tok.type, false);

            if (RG_UNLIKELY(prec < 0))
                goto expected_value;
            if (RG_UNLIKELY(!expect_op))
                goto expected_value;
            expect_op = false;

            while (stack.len) {
                const Token &op = stack[stack.len - 1];
                int op_prec = GetOperatorPrecedence(op.type, true);

                if (prec > op_prec)
                    break;

                tokens[new_len++] = op;
                stack.RemoveLast(1);
            }

            stack.Append(tok);
        }
    }

    if (RG_UNLIKELY(!expect_op))
        goto expected_value;

    for (Size i = stack.len - 1; i >= 0; i--) {
        const Token &op = stack[i];

        if (RG_UNLIKELY(op.type == TokenType::LeftParenthesis)) {
            LogError("Missing closing parenthesis");
            return -1;
        }

        tokens[new_len++] = op;
    }

    return new_len;

expected_op:
    LogError("Unexpected token, expected operator or ')'");
    return -1;
expected_value:
    LogError("Unexpected token, expected value or '('");
    return -1;
}

int RunBlik(int argc, char **argv)
{
    if (argc < 2) {
        PrintLn(stderr, "Usage: blik <expression> ...");
        return 1;
    }

    for (Size i = 1; i < argc; i++) {
        TokenSet token_set;
        if (!Tokenize(argv[i], "<argv>", &token_set))
            return 1;

        Size new_len = PostFixExpression(token_set.tokens);
        if (new_len < 0)
            return 1;
        token_set.tokens.RemoveFrom(new_len);

        for (const Token &tok: token_set.tokens) {
            if (tok.type == TokenType::Integer) {
                PrintLn("INTEGER %1", tok.u.i);
            } else if (tok.type == TokenType::Double) {
                PrintLn("DOUBLE %1", tok.u.d);
            } else if (tok.type == TokenType::String) {
                PrintLn("STRING '%1'", tok.u.str);
            } else if (tok.type == TokenType::Identifier) {
                PrintLn("IDENT '%1'", tok.u.str);
            } else if ((int)tok.type < 256) {
                PrintLn("TOKEN: %1", (char)tok.type);
            } else {
                PrintLn("TOKEN: %1", (int)tok.type);
            }
        }
    }

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunBlik(argc, argv); }
