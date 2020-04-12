// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "lexer.hh"
#include "parser.hh"

namespace RG {

enum class Type {
    Bool,
    Integer,
    Double,
    String
};
static const char *const TypeNames[] = {
    "Bool",
    "Integer",
    "Double",
    "String"
};

class Parser {
    struct PendingOperator {
        TokenType type;
        Size branch_idx;
    };

    HeapArray<Instruction> ir;
    HeapArray<Type> types;

public:
    bool ParseExpression(Span<const Token> tokens);

    bool ProduceOperator(const PendingOperator &op);
    bool EmitOperator1(Type in_type, Opcode code, Type out_type);
    bool EmitOperator2(Type in_type, Opcode code, Type out_type);

    void Finish(HeapArray<Instruction> *out_ir);
};

static int GetOperatorPrecedence(TokenType type)
{
    switch (type) {
        case TokenType::LogicOr: { return 2; } break;
        case TokenType::LogicAnd: { return 3; } break;
        case TokenType::Equal: { return 4; } break;
        case TokenType::NotEqual: { return 4; } break;
        case TokenType::Greater: { return 5; } break;
        case TokenType::GreaterOrEqual: { return 5; } break;
        case TokenType::Less: { return 5; } break;
        case TokenType::LessOrEqual: { return 5; } break;
        case TokenType::Or: { return 6; } break;
        case TokenType::Xor: { return 7; } break;
        case TokenType::And: { return 8; } break;
        case TokenType::LeftShift: { return 9; } break;
        case TokenType::RightShift: { return 9; } break;
        case TokenType::Plus: { return 10; } break;
        case TokenType::Minus: { return 10; } break;
        case TokenType::Multiply: { return 11; } break;
        case TokenType::Divide: { return 11; } break;
        case TokenType::Modulo: { return 11; } break;
        case TokenType::Not: { return 12; } break;
        case TokenType::LogicNot: { return 12; } break;

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

static bool IsOperand(TokenType type)
{
    return type == TokenType::Bool || type == TokenType::Integer || type == TokenType::Double ||
           type == TokenType::String || type == TokenType::Identifier;
}

bool Parser::ParseExpression(Span<const Token> tokens)
{
    LocalArray<PendingOperator, 128> stack;

    bool expect_op = false;
    for (Size i = 0, j = 1; i < tokens.len; i++, j++) {
        const Token &tok = tokens[i];

        if (tok.type == TokenType::LeftParenthesis) {
            if (RG_UNLIKELY(expect_op))
                goto expected_op;

            stack.Append({tok.type});
        } else if (tok.type == TokenType::RightParenthesis) {
            if (RG_UNLIKELY(!expect_op))
                goto expected_value;
            expect_op = true;

            for (;;) {
                if (RG_UNLIKELY(!stack.len)) {
                    LogError("Too many closing parentheses");
                    return false;
                }

                const PendingOperator &op = stack.data[stack.len - 1];

                if (op.type == TokenType::LeftParenthesis) {
                    stack.len--;
                    break;
                }

                if (RG_UNLIKELY(!ProduceOperator(op)))
                    return false;
                stack.len--;
            }
        } else if (IsOperand(tok.type)) {
            if (RG_UNLIKELY(expect_op))
                goto expected_op;
            expect_op = true;

            switch (tok.type) {
                case TokenType::Bool: {
                    ir.Append(Instruction(Opcode::PushBool, tok.u.b));
                    types.Append(Type::Bool);
                } break;
                case TokenType::Integer: {
                    ir.Append(Instruction(Opcode::PushInteger, tok.u.i));
                    types.Append(Type::Integer);
                } break;
                case TokenType::Double: {
                    ir.Append(Instruction(Opcode::PushDouble, tok.u.d));
                    types.Append(Type::Double);
                } break;
                case TokenType::String: {
                    ir.Append(Instruction(Opcode::PushString, tok.u.str));
                    types.Append(Type::String);
                } break;
                case TokenType::Identifier: { RG_ASSERT(false); } break;

                default: { RG_ASSERT(false); } break;
            }
        } else {
            int prec = GetOperatorPrecedence(tok.type);

            if (RG_UNLIKELY(prec < 0))
                goto expected_value;
            if (RG_UNLIKELY(!expect_op)) {
                if (tok.type == TokenType::Plus) {
                    if (RG_UNLIKELY(j >= tokens.len))
                        goto expected_value;
                    if (RG_UNLIKELY(!IsOperand(tokens[j].type)))
                        goto expected_value;

                    continue;
                } else if (tok.type == TokenType::Minus) {
                    if (RG_UNLIKELY(j >= tokens.len))
                        goto expected_value;
                    if (RG_UNLIKELY(!IsOperand(tokens[j].type)))
                        goto expected_value;

                    ir.Append(Instruction(Opcode::PushInteger, 0ll));
                    types.Append(Type::Integer);

                    prec = 12;
                } else if (RG_UNLIKELY(!IsUnaryOperator(tok.type))) {
                    goto expected_value;
                }
            }
            expect_op = false;

            while (stack.len) {
                const PendingOperator &op = stack[stack.len - 1];
                int op_prec = GetOperatorPrecedence(op.type) - IsUnaryOperator(op.type);

                if (prec > op_prec)
                    break;

                if (RG_UNLIKELY(!ProduceOperator(op)))
                    return false;
                stack.len--;
            }

            if (RG_UNLIKELY(!stack.Available())) {
                LogError("Too many operators on the stack");
                return false;
            }

            // Short-circuit operators need a short-circuit branch
            if (tok.type == TokenType::LogicAnd) {
                stack.Append({tok.type, ir.len});
                ir.Append(Instruction(Opcode::BranchIfFalse));
            } else if (tok.type == TokenType::LogicOr) {
                stack.Append({tok.type, ir.len});
                ir.Append(Instruction(Opcode::BranchIfTrue));
            } else {
                stack.Append({tok.type});
            }
        }
    }

    if (RG_UNLIKELY(!expect_op))
        goto expected_value;

    for (Size i = stack.len - 1; i >= 0; i--) {
        const PendingOperator &op = stack[i];

        if (RG_UNLIKELY(op.type == TokenType::LeftParenthesis)) {
            LogError("Missing closing parenthesis");
            return false;
        }

        if (RG_UNLIKELY(!ProduceOperator(op)))
            return false;
    }

    return true;

expected_op:
    LogError("Unexpected token, expected operator or ')'");
    return false;
expected_value:
    LogError("Unexpected token, expected value or '('");
    return false;
}

bool Parser::ProduceOperator(const PendingOperator &op)
{
    bool success;

    switch (op.type) {
        case TokenType::Plus: {
            success = EmitOperator2(Type::Integer, Opcode::Add, Type::Integer) ||
                      EmitOperator2(Type::Double, Opcode::AddDouble, Type::Double);
        } break;
        case TokenType::Minus: {
            success = EmitOperator2(Type::Integer, Opcode::Substract, Type::Integer) ||
                      EmitOperator2(Type::Double, Opcode::SubstractDouble, Type::Double);
        } break;
        case TokenType::Multiply: {
            success = EmitOperator2(Type::Integer, Opcode::Multiply, Type::Integer) ||
                      EmitOperator2(Type::Double, Opcode::MultiplyDouble, Type::Double);
        } break;
        case TokenType::Divide: {
            success = EmitOperator2(Type::Integer, Opcode::Divide, Type::Integer) ||
                      EmitOperator2(Type::Double, Opcode::DivideDouble, Type::Double);
        } break;
        case TokenType::Modulo: {
            success = EmitOperator2(Type::Integer, Opcode::Modulo, Type::Integer);
        } break;

        case TokenType::Equal: {
            success = EmitOperator2(Type::Integer, Opcode::Equal, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::EqualDouble, Type::Bool) ||
                      EmitOperator2(Type::Bool, Opcode::EqualBool, Type::Bool);
        } break;
        case TokenType::NotEqual: {
            success = EmitOperator2(Type::Integer, Opcode::NotEqual, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::NotEqualDouble, Type::Bool) ||
                      EmitOperator2(Type::Bool, Opcode::NotEqualBool, Type::Bool);
        } break;
        case TokenType::Greater: {
            success = EmitOperator2(Type::Integer, Opcode::Greater, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::GreaterDouble, Type::Bool);
        } break;
        case TokenType::GreaterOrEqual: {
            success = EmitOperator2(Type::Integer, Opcode::GreaterOrEqual, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::GreaterOrEqualDouble, Type::Bool);
        } break;
        case TokenType::Less: {
            success = EmitOperator2(Type::Integer, Opcode::Less, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::LessDouble, Type::Bool);
        } break;
        case TokenType::LessOrEqual: {
            success = EmitOperator2(Type::Integer, Opcode::LessOrEqual, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::LessOrEqualDouble, Type::Bool);
        } break;

        case TokenType::And: {
            success = EmitOperator2(Type::Integer, Opcode::And, Type::Integer) ||
                      EmitOperator2(Type::Bool, Opcode::LogicAnd, Type::Bool);
        } break;
        case TokenType::Or: {
            success = EmitOperator2(Type::Integer, Opcode::Or, Type::Integer) ||
                      EmitOperator2(Type::Bool, Opcode::LogicOr, Type::Bool);
        } break;
        case TokenType::Xor: {
            success = EmitOperator2(Type::Integer, Opcode::Xor, Type::Integer) ||
                      EmitOperator2(Type::Bool, Opcode::LogicXor, Type::Bool);
        } break;
        case TokenType::Not: {
            success = EmitOperator1(Type::Integer, Opcode::Not, Type::Integer) ||
                      EmitOperator1(Type::Bool, Opcode::LogicNot, Type::Bool);
        } break;
        case TokenType::LeftShift: {
            success = EmitOperator2(Type::Integer, Opcode::LeftShift, Type::Integer);
        } break;
        case TokenType::RightShift: {
            success = EmitOperator2(Type::Integer, Opcode::RightShift, Type::Integer);
        } break;

        case TokenType::LogicNot: {
            success = EmitOperator1(Type::Bool, Opcode::LogicNot, Type::Bool);
        } break;
        case TokenType::LogicAnd: {
            success = EmitOperator2(Type::Bool, Opcode::LogicAnd, Type::Bool);

            RG_ASSERT(op.branch_idx && ir[op.branch_idx].code == Opcode::BranchIfFalse);
            ir[op.branch_idx].u.i = ir.len;
        } break;
        case TokenType::LogicOr: {
            success = EmitOperator2(Type::Bool, Opcode::LogicOr, Type::Bool);

            RG_ASSERT(op.branch_idx && ir[op.branch_idx].code == Opcode::BranchIfTrue);
            ir[op.branch_idx].u.i = ir.len;
        } break;

        default: { RG_ASSERT(false); } break;
    }

    if (RG_UNLIKELY(!success)) {
        if (IsUnaryOperator(op.type)) {
            LogError("Cannot use '%1' operator on %2 value",
                     TokenTypeNames[(int)op.type], TypeNames[(int)types[types.len - 1]]);
        } else if (types[types.len - 2] == types[types.len - 1]) {
            LogError("Cannot use '%1' operator on %2 values",
                     TokenTypeNames[(int)op.type], TypeNames[(int)types[types.len - 2]]);
        } else {
            LogError("Cannot use '%1' operator on %2 and %3 values",
                     TokenTypeNames[(int)op.type], TypeNames[(int)types[types.len - 2]], TypeNames[(int)types[types.len - 1]]);
        }

        return false;
    }

    return true;
}

bool Parser::EmitOperator1(Type in_type, Opcode code, Type out_type)
{
    Type type = types[types.len - 1];

    if (type == in_type) {
        ir.Append(Instruction(code));
        types[types.len - 1] = out_type;

        return true;
    } else {
        return false;
    }
}

bool Parser::EmitOperator2(Type in_type, Opcode code, Type out_type)
{
    Type type1 = types[types.len - 2];
    Type type2 = types[types.len - 1];

    if (type1 == in_type && type2 == in_type) {
        ir.Append(Instruction(code));
        types[--types.len - 1] = out_type;

        return true;
    } else {
        return false;
    }
}

void Parser::Finish(HeapArray<Instruction> *out_ir)
{
    RG_ASSERT(!out_ir->len);
    SwapMemory(&ir, out_ir, RG_SIZE(ir));
}

bool Parse(Span<const Token> tokens, HeapArray<Instruction> *out_ir)
{
    Parser parser;

    if (!parser.ParseExpression(tokens))
        return false;

    parser.Finish(out_ir);
    return true;
}

}
