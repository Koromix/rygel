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
    bool ProduceOperatorArithmetic(const char *name, Opcode int_op, Opcode double_op);
    bool ProduceOperatorCompare(Opcode int_op, Opcode double_op);
    bool ProduceOperatorBitwise(const char *name, Opcode int_op, Opcode bool_op);

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

                    ir.Append(Instruction(Opcode::PushInteger, 0ull));
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
    switch (op.type) {
        case TokenType::Plus: { return ProduceOperatorArithmetic("add", Opcode::Add, Opcode::AddDouble); } break;
        case TokenType::Minus: { return ProduceOperatorArithmetic("substract", Opcode::Substract, Opcode::SubstractDouble); } break;
        case TokenType::Multiply: { return ProduceOperatorArithmetic("multiply", Opcode::Multiply, Opcode::MultiplyDouble); } break;
        case TokenType::Divide: { return ProduceOperatorArithmetic("divide", Opcode::Divide, Opcode::DivideDouble); } break;
        case TokenType::Modulo: {
            Type type1 = types[types.len - 2];
            Type type2 = types[types.len - 1];
            types.RemoveLast(2);

            if (type1 == Type::Integer && type2 == Type::Integer) {
                ir.Append(Instruction(Opcode::Modulo));
                types.Append(Type::Integer);
                return true;
            } else {
                LogError("Cannot use modulo between %1 value and %2 value", TypeNames[(int)type1], TypeNames[(int)type2]);
                return false;
            }
        } break;

        case TokenType::Equal: { return ProduceOperatorCompare(Opcode::Equal, Opcode::EqualDouble); } break;
        case TokenType::NotEqual: { return ProduceOperatorCompare(Opcode::NotEqual, Opcode::NotEqualDouble); } break;
        case TokenType::Greater: { return ProduceOperatorCompare(Opcode::Greater, Opcode::GreaterDouble); } break;
        case TokenType::GreaterOrEqual: { return ProduceOperatorCompare(Opcode::GreaterOrEqual, Opcode::GreaterOrEqualDouble); } break;
        case TokenType::Less: { return ProduceOperatorCompare(Opcode::Less, Opcode::LessDouble); } break;
        case TokenType::LessOrEqual: { return ProduceOperatorCompare(Opcode::LessOrEqual, Opcode::LessOrEqualDouble); } break;

        case TokenType::And: { return ProduceOperatorBitwise("AND", Opcode::And, Opcode::LogicAnd); } break;
        case TokenType::Or: { return ProduceOperatorBitwise("OR", Opcode::Or, Opcode::LogicOr); } break;
        case TokenType::Xor: { return ProduceOperatorBitwise("XOR", Opcode::Xor, Opcode::LogicXor); } break;
        case TokenType::Not: {
            Type type = types[types.len - 1];
            types.RemoveLast(1);

            if (type == Type::Integer) {
                ir.Append(Instruction(Opcode::Not));
                types.Append(Type::Integer);

                return true;
            } else if (type == Type::Bool) {
                ir.Append(Instruction(Opcode::LogicNot));
                types.Append(Type::Bool);

                return false;
            } else {
                LogError("Cannot use NOT operator with %1 value", TypeNames[(int)type]);
                return false;
            }
        } break;
        case TokenType::LeftShift: {
            Type type1 = types[types.len - 2];
            Type type2 = types[types.len - 1];
            types.RemoveLast(2);

            if (type1 == Type::Integer && type2 == Type::Integer) {
                ir.Append(Instruction(Opcode::LeftShift));
                types.Append(Type::Integer);

                return true;
            } else {
                LogError("Cannot use shift operator with %1 value and %2 value", TypeNames[(int)type1], TypeNames[(int)type2]);
                return false;
            }
        } break;
        case TokenType::RightShift: {
            Type type1 = types[types.len - 2];
            Type type2 = types[types.len - 1];
            types.RemoveLast(2);

            if (type1 == Type::Integer && type2 == Type::Integer) {
                ir.Append(Instruction(Opcode::RightShift));
                types.Append(Type::Integer);

                return true;
            } else {
                LogError("Cannot use shift operator with %1 value and %2 value", TypeNames[(int)type1], TypeNames[(int)type2]);
                return false;
            }
        } break;

        case TokenType::LogicNot: {
            Type type = types[types.len - 1];

            if (type == Type::Bool) {
                ir.Append(Instruction(Opcode::LogicNot));
                return true;
            } else {
                LogError("Cannot use NOT operator with %1 value", TypeNames[(int)type]);
                return false;
            }
        } break;
        case TokenType::LogicAnd: {
            Type type1 = types[types.len - 2];
            Type type2 = types[types.len - 1];
            types.RemoveLast(2);

            if (type1 == Type::Bool && type2 == Type::Bool) {
                ir.Append(Instruction(Opcode::LogicAnd));
                types.Append(Type::Bool);

                RG_ASSERT(op.branch_idx && ir[op.branch_idx].code == Opcode::BranchIfFalse);
                ir[op.branch_idx].u.jump = ir.len;

                return true;
            } else {
                LogError("Cannot use AND between %1 value and %2 value", TypeNames[(int)type1], TypeNames[(int)type2]);
                return false;
            }
        } break;
        case TokenType::LogicOr: {
            Type type1 = types[types.len - 2];
            Type type2 = types[types.len - 1];
            types.RemoveLast(2);

            if (type1 == Type::Bool && type2 == Type::Bool) {
                ir.Append(Instruction(Opcode::LogicOr));
                types.Append(Type::Bool);

                RG_ASSERT(op.branch_idx && ir[op.branch_idx].code == Opcode::BranchIfTrue);
                ir[op.branch_idx].u.jump = ir.len;

                return true;
            } else {
                LogError("Cannot use OR between %1 value and %2 value", TypeNames[(int)type1], TypeNames[(int)type2]);
                return false;
            }
        } break;

        default: { RG_ASSERT(false); } break;
    }
}

bool Parser::ProduceOperatorArithmetic(const char *name, Opcode int_op, Opcode double_op)
{
    Type type1 = types[types.len - 2];
    Type type2 = types[types.len - 1];
    types.RemoveLast(2);

    if (type1 == Type::Integer && type2 == Type::Integer) {
        ir.Append(Instruction(int_op));
        types.Append(Type::Integer);
    } else if (type1 == Type::Double && type2 == Type::Double) {
        ir.Append(Instruction(double_op));
        types.Append(Type::Double);
    } else {
        LogError("Cannot %1 %2 value and %3 value", name, TypeNames[(int)type1], TypeNames[(int)type2]);
        return false;
    }

    return true;
}

bool Parser::ProduceOperatorCompare(Opcode int_op, Opcode double_op)
{
    Type type1 = types[types.len - 2];
    Type type2 = types[types.len - 1];
    types.RemoveLast(2);

    if (type1 == Type::Integer && type2 == Type::Integer) {
        ir.Append(Instruction(int_op));
        types.Append(Type::Bool);
    } else if (type1 == Type::Double && type2 == Type::Double) {
        ir.Append(Instruction(double_op));
        types.Append(Type::Bool);
    } else {
        LogError("Cannot compare %1 value and %2 value", TypeNames[(int)type1], TypeNames[(int)type2]);
        return false;
    }

    return true;
}

bool Parser::ProduceOperatorBitwise(const char *name, Opcode int_op, Opcode bool_op)
{
    Type type1 = types[types.len - 2];
    Type type2 = types[types.len - 1];
    types.RemoveLast(2);

    if (type1 == Type::Integer && type2 == Type::Integer) {
        ir.Append(Instruction(int_op));
        types.Append(Type::Integer);
    } else if (type1 == Type::Bool && type2 == Type::Bool) {
        ir.Append(Instruction(bool_op));
        types.Append(Type::Bool);
    } else {
        LogError("Cannot %1 %2 value and %3 value", name, TypeNames[(int)type1], TypeNames[(int)type2]);
        return false;
    }

    return true;
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
