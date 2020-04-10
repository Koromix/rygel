// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "lexer.hh"
#include "parser.hh"

namespace RG {

static int GetOperatorPrecedence(TokenType type)
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
        case TokenType::Not: { return 12; } break;
        case TokenType::LogicNot: { return 12; } break;
        case TokenType::LogicAnd: { return 3; } break;
        case TokenType::LogicOr: { return 2; } break;
        case TokenType::Equal: { return 7; } break;
        case TokenType::NotEqual: { return 7; } break;

        default: { return -1; } break;
    }
}

static bool IsUnaryOperator(TokenType type)
{
    switch (type) {
        case TokenType::Not: { return true; } break;
        case TokenType::LogicNot: { return true; } break;

        default: { return false; } break;
    }
}

bool ParseExpression(Span<const Token> tokens)
{
    LocalArray<TokenType, 128> stack;

    bool expect_op = false;
    for (const Token &tok: tokens) {
        if (tok.type == TokenType::LeftParenthesis) {
            if (RG_UNLIKELY(expect_op))
                goto expected_op;

            stack.Append(tok.type);
        } else if (tok.type == TokenType::RightParenthesis) {
            if (RG_UNLIKELY(!expect_op))
                goto expected_value;
            expect_op = true;

            for (;;) {
                if (RG_UNLIKELY(!stack.len)) {
                    LogError("Too many closing parentheses");
                    return false;
                }

                TokenType op = stack[--stack.len];

                if (op == TokenType::LeftParenthesis)
                    break;

                if ((int)op < 256) {
                    LogInfo("DO %1", (char)op);
                } else {
                    LogInfo("DO %1", (int)op);
                }
            }
        } else if (tok.type == TokenType::Identifier || tok.type == TokenType::Integer ||
                   tok.type == TokenType::Double || tok.type == TokenType::String) {
            if (RG_UNLIKELY(expect_op))
                goto expected_op;
            expect_op = true;

            switch (tok.type) {
                case TokenType::Identifier: { LogInfo("PUSH VARIABLE %1", tok.u.str); } break;
                case TokenType::Integer: { LogInfo("PUSH INTEGER %1", tok.u.i); } break;
                case TokenType::Double: { LogInfo("PUSH DOUBLE %1", tok.u.d); } break;
                case TokenType::String: { LogInfo("PUSH STRING '%1'", tok.u.str); } break;

                default: { RG_ASSERT(false); } break;
            }
        } else {
            int prec = GetOperatorPrecedence(tok.type);

            if (RG_UNLIKELY(prec < 0))
                goto expected_value;
            if (RG_UNLIKELY(!expect_op && !IsUnaryOperator(tok.type)))
                goto expected_value;
            expect_op = false;

            while (stack.len) {
                TokenType op = stack[stack.len - 1];
                int op_prec = GetOperatorPrecedence(op) - IsUnaryOperator(op);

                if (prec > op_prec)
                    break;

                if ((int)op < 256) {
                    LogInfo("DO %1", (char)op);
                } else {
                    LogInfo("DO %1", (int)op);
                }
                stack.len--;
            }

            if (RG_UNLIKELY(!stack.Available())) {
                LogError("Too many operators on the stack");
                return false;
            }
            stack.Append(tok.type);
        }
    }

    if (RG_UNLIKELY(!expect_op))
        goto expected_value;

    for (Size i = stack.len - 1; i >= 0; i--) {
        TokenType op = stack[i];

        if (RG_UNLIKELY(op == TokenType::LeftParenthesis)) {
            LogError("Missing closing parenthesis");
            return false;
        }

        if ((int)op < 256) {
            LogInfo("DO %1", (char)op);
        } else {
            LogInfo("DO %1", (int)op);
        }
    }

    return true;

expected_op:
    LogError("Unexpected token, expected operator or ')'");
    return false;
expected_value:
    LogError("Unexpected token, expected value or '('");
    return false;
}

}
