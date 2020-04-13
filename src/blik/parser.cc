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
        int prec;
        bool unary;

        Size branch_idx; // Used for short-circuit operators
    };

    Span<const Token> tokens;
    Size offset = 0;
    bool valid = true;

    HeapArray<Instruction> ir;
    HeapArray<Type> types;

public:
    Parser(Span<const Token> tokens) : tokens(tokens) {}

    bool IsValid() const { return valid; }
    bool IsDone() const { return !valid || offset >= tokens.len; }

    void ParseAll();
    void ParseExpression();

    void ProduceOperator(const PendingOperator &op);
    bool EmitOperator1(Type in_type, Opcode code, Type out_type);
    bool EmitOperator2(Type in_type, Opcode code, Type out_type);

    bool Finish(HeapArray<Instruction> *out_ir);

private:
    void ConsumeToken(TokenType type);

    template <typename... Args>
    void MarkError(const char *fmt, Args... args)
    {
        if (valid) {
            LogError(fmt, args...);
            valid = false;
        }
    }
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

void Parser::ParseAll()
{
    while (!IsDone()) {
        switch (tokens[offset].type) {
            case TokenType::NewLine: { offset++; } break;

            default: {
                ParseExpression();
                ConsumeToken(TokenType::NewLine);

                ir.Append(Instruction(Opcode::Pop));
            } break;
        }
    }
}

void Parser::ParseExpression()
{
    LocalArray<PendingOperator, 128> operators;
    bool expect_op = false;

    for (; offset < tokens.len; offset++) {
        const Token &tok = tokens[offset];

        if (tok.type == TokenType::LeftParenthesis) {
            if (RG_UNLIKELY(expect_op))
                goto expected_op;

            operators.Append({tok.type});
        } else if (tok.type == TokenType::RightParenthesis) {
            if (RG_UNLIKELY(!expect_op))
                goto expected_value;
            expect_op = true;

            for (;;) {
                if (RG_UNLIKELY(!operators.len)) {
                    MarkError("Too many closing parentheses");
                    return;
                }

                const PendingOperator &op = operators.data[operators.len - 1];

                if (op.type == TokenType::LeftParenthesis) {
                    operators.len--;
                    break;
                }

                ProduceOperator(op);
                operators.len--;
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
                    if (operators.len && operators[operators.len - 1].type == TokenType::Minus &&
                                         operators[operators.len - 1].unary) {
                        operators.RemoveLast(1);

                        ir.Append(Instruction(Opcode::PushInt, -tok.u.i));
                        types.Append(Type::Integer);
                    } else {
                        ir.Append(Instruction(Opcode::PushInt, tok.u.i));
                        types.Append(Type::Integer);
                    }
                } break;
                case TokenType::Double: {
                    if (operators.len && operators[operators.len - 1].type == TokenType::Minus &&
                                         operators[operators.len - 1].unary) {
                        operators.RemoveLast(1);

                        ir.Append(Instruction(Opcode::PushDouble, -tok.u.d));
                        types.Append(Type::Integer);
                    } else {
                        ir.Append(Instruction(Opcode::PushDouble, tok.u.d));
                        types.Append(Type::Double);
                    }
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
            bool unary = IsUnaryOperator(tok.type);

            if (RG_UNLIKELY(prec < 0)) {
                if (!expect_op && tok.type == TokenType::NewLine) {
                    // Expression is split across multiple lines
                    continue;
                } else {
                    break;
                }
            }
            if (RG_UNLIKELY(expect_op == unary)) {
                if (tok.type == TokenType::Plus) {
                    continue;
                } else if (tok.type == TokenType::Minus) {
                    prec = 12;
                    unary = true;
                } else if (expect_op) {
                    goto expected_op;
                } else {
                    goto expected_value;
                }
            }
            expect_op = false;

            while (operators.len) {
                const PendingOperator &op = operators[operators.len - 1];

                if (prec > op.prec - op.unary)
                    break;

                ProduceOperator(op);
                operators.len--;
            }

            if (RG_UNLIKELY(!operators.Available())) {
                MarkError("Too many operators on the stack");
                return;
            }

            // Short-circuit operators need a short-circuit branch
            if (tok.type == TokenType::LogicAnd) {
                operators.Append({tok.type, prec, unary, ir.len});
                ir.Append(Instruction(Opcode::BranchIfFalse));
            } else if (tok.type == TokenType::LogicOr) {
                operators.Append({tok.type, prec, unary, ir.len});
                ir.Append(Instruction(Opcode::BranchIfTrue));
            } else {
                operators.Append({tok.type, prec, unary});
            }
        }
    }

    if (RG_UNLIKELY(!expect_op)) {
        MarkError("Unexpected end, expected value or '('");
        return;
    }

    for (Size i = operators.len - 1; i >= 0; i--) {
        const PendingOperator &op = operators[i];

        if (RG_UNLIKELY(op.type == TokenType::LeftParenthesis)) {
            MarkError("Missing closing parenthesis");
            return;
        }

        ProduceOperator(op);
    }

    return;

expected_op:
    MarkError("Unexpected token '%1', expected operator or ')'", TokenTypeNames[(int)tokens[offset].type]);
    return;
expected_value:
    MarkError("Unexpected token '%1', expected value or '('", TokenTypeNames[(int)tokens[offset].type]);
    return;
}

void Parser::ProduceOperator(const PendingOperator &op)
{
    bool success;

    switch (op.type) {
        case TokenType::Plus: {
            success = EmitOperator2(Type::Integer, Opcode::AddInt, Type::Integer) ||
                      EmitOperator2(Type::Double, Opcode::AddDouble, Type::Double);
        } break;
        case TokenType::Minus: {
            if (op.unary) {
                success = EmitOperator1(Type::Integer, Opcode::NegateInt, Type::Integer) ||
                          EmitOperator1(Type::Double, Opcode::NegateDouble, Type::Double);
            } else {
                success = EmitOperator2(Type::Integer, Opcode::SubstractInt, Type::Integer) ||
                          EmitOperator2(Type::Double, Opcode::SubstractDouble, Type::Double);
            }
        } break;
        case TokenType::Multiply: {
            success = EmitOperator2(Type::Integer, Opcode::MultiplyInt, Type::Integer) ||
                      EmitOperator2(Type::Double, Opcode::MultiplyDouble, Type::Double);
        } break;
        case TokenType::Divide: {
            success = EmitOperator2(Type::Integer, Opcode::DivideInt, Type::Integer) ||
                      EmitOperator2(Type::Double, Opcode::DivideDouble, Type::Double);
        } break;
        case TokenType::Modulo: {
            success = EmitOperator2(Type::Integer, Opcode::ModuloInt, Type::Integer);
        } break;

        case TokenType::Equal: {
            success = EmitOperator2(Type::Integer, Opcode::EqualInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::EqualDouble, Type::Bool) ||
                      EmitOperator2(Type::Bool, Opcode::EqualBool, Type::Bool);
        } break;
        case TokenType::NotEqual: {
            success = EmitOperator2(Type::Integer, Opcode::NotEqualInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::NotEqualDouble, Type::Bool) ||
                      EmitOperator2(Type::Bool, Opcode::NotEqualBool, Type::Bool);
        } break;
        case TokenType::Greater: {
            success = EmitOperator2(Type::Integer, Opcode::GreaterInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::GreaterDouble, Type::Bool);
        } break;
        case TokenType::GreaterOrEqual: {
            success = EmitOperator2(Type::Integer, Opcode::GreaterOrEqualInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::GreaterOrEqualDouble, Type::Bool);
        } break;
        case TokenType::Less: {
            success = EmitOperator2(Type::Integer, Opcode::LessInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::LessDouble, Type::Bool);
        } break;
        case TokenType::LessOrEqual: {
            success = EmitOperator2(Type::Integer, Opcode::LessOrEqualInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::LessOrEqualDouble, Type::Bool);
        } break;

        case TokenType::And: {
            success = EmitOperator2(Type::Integer, Opcode::AndInt, Type::Integer) ||
                      EmitOperator2(Type::Bool, Opcode::AndBool, Type::Bool);
        } break;
        case TokenType::Or: {
            success = EmitOperator2(Type::Integer, Opcode::OrInt, Type::Integer) ||
                      EmitOperator2(Type::Bool, Opcode::OrBool, Type::Bool);
        } break;
        case TokenType::Xor: {
            success = EmitOperator2(Type::Integer, Opcode::XorInt, Type::Integer) ||
                      EmitOperator2(Type::Bool, Opcode::XorBool, Type::Bool);
        } break;
        case TokenType::Not: {
            success = EmitOperator1(Type::Integer, Opcode::NotInt, Type::Integer) ||
                      EmitOperator1(Type::Bool, Opcode::NotBool, Type::Bool);
        } break;
        case TokenType::LeftShift: {
            success = EmitOperator2(Type::Integer, Opcode::LeftShiftInt, Type::Integer);
        } break;
        case TokenType::RightShift: {
            success = EmitOperator2(Type::Integer, Opcode::RightShiftInt, Type::Integer);
        } break;

        case TokenType::LogicNot: {
            success = EmitOperator1(Type::Bool, Opcode::NotBool, Type::Bool);
        } break;
        case TokenType::LogicAnd: {
            success = EmitOperator2(Type::Bool, Opcode::AndBool, Type::Bool);

            RG_ASSERT(op.branch_idx && ir[op.branch_idx].code == Opcode::BranchIfFalse);
            ir[op.branch_idx].u.i = ir.len;
        } break;
        case TokenType::LogicOr: {
            success = EmitOperator2(Type::Bool, Opcode::OrBool, Type::Bool);

            RG_ASSERT(op.branch_idx && ir[op.branch_idx].code == Opcode::BranchIfTrue);
            ir[op.branch_idx].u.i = ir.len;
        } break;

        default: { RG_ASSERT(false); } break;
    }

    if (RG_UNLIKELY(!success)) {
        if (IsUnaryOperator(op.type)) {
            MarkError("Cannot use '%1' operator on %2 value",
                      TokenTypeNames[(int)op.type], TypeNames[(int)types[types.len - 1]]);
        } else if (types[types.len - 2] == types[types.len - 1]) {
            MarkError("Cannot use '%1' operator on %2 values",
                      TokenTypeNames[(int)op.type], TypeNames[(int)types[types.len - 2]]);
        } else {
            MarkError("Cannot use '%1' operator on %2 and %3 values",
                      TokenTypeNames[(int)op.type], TypeNames[(int)types[types.len - 2]], TypeNames[(int)types[types.len - 1]]);
        }
    }
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

bool Parser::Finish(HeapArray<Instruction> *out_ir)
{
    RG_ASSERT(!out_ir->len);

    if (!valid)
        return false;

    SwapMemory(&ir, out_ir, RG_SIZE(ir));
    return true;
}

void Parser::ConsumeToken(TokenType type)
{
    if (RG_UNLIKELY(offset >= tokens.len)) {
        MarkError("Unexpected end, expected '%1'", TokenTypeNames[(int)type]);
        return;
    }
    if (RG_UNLIKELY(tokens[offset].type != type)) {
        MarkError("Unexpected token '%1', expected '%2'",
                  TokenTypeNames[(int)tokens[offset].type], TokenTypeNames[(int)type]);
        return;
    }

    offset++;
}

bool Parse(Span<const Token> tokens, HeapArray<Instruction> *out_ir)
{
    Parser parser(tokens);
    parser.ParseAll();
    return parser.Finish(out_ir);
}

}
