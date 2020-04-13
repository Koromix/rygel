// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "lexer.hh"
#include "parser.hh"

namespace RG {

class Parser {
    struct PendingOperator {
        TokenKind kind;
        int prec;
        bool unary;

        Size branch_idx; // Used for short-circuit operators
    };

    struct VariableInfo {
        const char *name;
        Type type;
        Size offset;

        RG_HASHTABLE_HANDLER(VariableInfo, name);
    };

    struct ExpressionType {
        Type type;
        const VariableInfo *var;
    };

    Span<const Token> tokens;
    Size offset;
    bool valid;

    HashTable<const char *, VariableInfo> variables;

    // Reuse for performance
    HeapArray<ExpressionType> types;

    HeapArray<Instruction> ir;

public:
    bool Parse(Span<const Token> tokens, const char *filename);
    void Finish(HeapArray<Instruction> *out_ir);

private:
    void ParseExpression(Type *out_type = nullptr);

    void ProduceOperator(const PendingOperator &op);
    bool EmitOperator1(Type in_type, Opcode code, Type out_type);
    bool EmitOperator2(Type in_type, Opcode code, Type out_type);

    bool ConsumeToken(TokenKind kind);

    template <typename... Args>
    void MarkError(const char *fmt, Args... args)
    {
        if (valid) {
            LogError(fmt, args...);
            valid = false;
        }
    }
};

static int GetOperatorPrecedence(TokenKind kind)
{
    switch (kind) {
        case TokenKind::Assign: { return 0; } break;
        case TokenKind::LogicOr: { return 2; } break;
        case TokenKind::LogicAnd: { return 3; } break;
        case TokenKind::Equal: { return 4; } break;
        case TokenKind::NotEqual: { return 4; } break;
        case TokenKind::Greater: { return 5; } break;
        case TokenKind::GreaterOrEqual: { return 5; } break;
        case TokenKind::Less: { return 5; } break;
        case TokenKind::LessOrEqual: { return 5; } break;
        case TokenKind::Or: { return 6; } break;
        case TokenKind::Xor: { return 7; } break;
        case TokenKind::And: { return 8; } break;
        case TokenKind::LeftShift: { return 9; } break;
        case TokenKind::RightShift: { return 9; } break;
        case TokenKind::Plus: { return 10; } break;
        case TokenKind::Minus: { return 10; } break;
        case TokenKind::Multiply: { return 11; } break;
        case TokenKind::Divide: { return 11; } break;
        case TokenKind::Modulo: { return 11; } break;
        case TokenKind::Not: { return 12; } break;
        case TokenKind::LogicNot: { return 12; } break;

        default: { return -1; } break;
    }
}

static bool IsUnaryOperator(TokenKind kind)
{
    switch (kind) {
        case TokenKind::Not: { return true; } break;
        case TokenKind::LogicNot: { return true; } break;

        default: { return false; } break;
    }
}

static bool IsOperand(TokenKind kind)
{
    return kind == TokenKind::Bool || kind == TokenKind::Integer || kind == TokenKind::Double ||
           kind == TokenKind::String || kind == TokenKind::Identifier;
}

bool Parser::Parse(Span<const Token> tokens, const char *filename)
{
    RG_DEFER_NC(out_guard, len = ir.len) { ir.RemoveFrom(len); };

    this->tokens = tokens;
    offset = 0;
    valid = true;
    variables.Clear();

    PushLogFilter([&](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
        int32_t line = tokens[std::min(offset, tokens.len - 1)].line;

        char msg_buf[4096];
        Fmt(msg_buf, "%1(%2): %3", filename, line, msg);

        func(level, ctx, msg_buf);
    });
    RG_DEFER { PopLogFilter(); };

    while (valid && offset < tokens.len) {
        switch (tokens[offset].kind) {
            case TokenKind::NewLine: { offset++; } break;

            case TokenKind::Let: {
                offset++;

                if (ConsumeToken(TokenKind::Identifier)) {
                    VariableInfo var = {};

                    var.name = tokens[offset - 1].u.str;
                    ConsumeToken(TokenKind::Assign);
                    ParseExpression(&var.type);
                    var.offset = variables.count;

                    if (!variables.Append(var).second) {
                        MarkError("Variable '%1' already exists", var.name);
                    }
                }

                ConsumeToken(TokenKind::NewLine);
            } break;

            default: {
                ParseExpression();
                ir.Append({Opcode::Pop});

                ConsumeToken(TokenKind::NewLine);
            } break;
        }
    }

    if (valid) {
        out_guard.Disable();
    }
    return valid;
}

void Parser::ParseExpression(Type *out_type)
{
    types.RemoveFrom(0);

    LocalArray<PendingOperator, 128> operators;
    bool expect_op = false;

    for (; offset < tokens.len; offset++) {
        const Token &tok = tokens[offset];

        if (tok.kind == TokenKind::LeftParenthesis) {
            if (RG_UNLIKELY(expect_op))
                goto expected_op;

            operators.Append({tok.kind});
        } else if (tok.kind == TokenKind::RightParenthesis) {
            if (RG_UNLIKELY(!expect_op))
                goto expected_value;
            expect_op = true;

            for (;;) {
                if (RG_UNLIKELY(!operators.len)) {
                    MarkError("Too many closing parentheses");
                    return;
                }

                const PendingOperator &op = operators.data[operators.len - 1];

                if (op.kind == TokenKind::LeftParenthesis) {
                    operators.len--;
                    break;
                }

                ProduceOperator(op);
                operators.len--;
            }
        } else if (IsOperand(tok.kind)) {
            if (RG_UNLIKELY(expect_op))
                goto expected_op;
            expect_op = true;

            switch (tok.kind) {
                case TokenKind::Bool: {
                    ir.Append({Opcode::PushBool, {.b = tok.u.b}});
                    types.Append({Type::Bool});
                } break;
                case TokenKind::Integer: {
                    if (operators.len && operators[operators.len - 1].kind == TokenKind::Minus &&
                                         operators[operators.len - 1].unary) {
                        operators.RemoveLast(1);

                        ir.Append({Opcode::PushInt, {.i = -tok.u.i}});
                        types.Append({Type::Integer});
                    } else {
                        ir.Append({Opcode::PushInt, {.i = tok.u.i}});
                        types.Append({Type::Integer});
                    }
                } break;
                case TokenKind::Double: {
                    if (operators.len && operators[operators.len - 1].kind == TokenKind::Minus &&
                                         operators[operators.len - 1].unary) {
                        operators.RemoveLast(1);

                        ir.Append({Opcode::PushDouble, {.d = -tok.u.d}});
                        types.Append({Type::Integer});
                    } else {
                        ir.Append({Opcode::PushDouble, {.d = tok.u.d}});
                        types.Append({Type::Double});
                    }
                } break;
                case TokenKind::String: {
                    ir.Append({Opcode::PushString, {.str = tok.u.str}});
                    types.Append({Type::String});
                } break;

                case TokenKind::Identifier: {
                    const VariableInfo *var = variables.Find(tok.u.str);

                    if (RG_UNLIKELY(!var)) {
                        MarkError("Variable '%1' is not defined", tok.u.str);
                        return;
                    }

                    switch (var->type) {
                        case Type::Bool: { ir.Append({Opcode::LoadBool, {.i = var->offset}}); } break;
                        case Type::Integer: { ir.Append({Opcode::LoadInt, {.i = var->offset}}); } break;
                        case Type::Double: { ir.Append({Opcode::LoadDouble, {.i = var->offset}});} break;
                        case Type::String: { ir.Append({Opcode::LoadString, {.i = var->offset}}); } break;
                    }
                    types.Append({var->type, var});
                } break;

                default: { RG_ASSERT(false); } break;
            }
        } else {
            int prec = GetOperatorPrecedence(tok.kind);
            bool unary = IsUnaryOperator(tok.kind);

            if (RG_UNLIKELY(prec < 0)) {
                if (!expect_op && tok.kind == TokenKind::NewLine) {
                    // Expression is split across multiple lines
                    continue;
                } else {
                    break;
                }
            }
            if (RG_UNLIKELY(expect_op == unary)) {
                if (tok.kind == TokenKind::Plus) {
                    continue;
                } else if (tok.kind == TokenKind::Minus) {
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
            if (tok.kind == TokenKind::LogicAnd) {
                operators.Append({tok.kind, prec, unary, ir.len});
                ir.Append({Opcode::BranchIfFalse});
            } else if (tok.kind == TokenKind::LogicOr) {
                operators.Append({tok.kind, prec, unary, ir.len});
                ir.Append({Opcode::BranchIfTrue});
            } else {
                operators.Append({tok.kind, prec, unary});
            }
        }
    }

    if (RG_UNLIKELY(!expect_op)) {
        MarkError("Unexpected end, expected value or '('");
        return;
    }

    for (Size i = operators.len - 1; i >= 0; i--) {
        const PendingOperator &op = operators[i];

        if (RG_UNLIKELY(op.kind == TokenKind::LeftParenthesis)) {
            MarkError("Missing closing parenthesis");
            return;
        }

        ProduceOperator(op);
    }

    RG_ASSERT(types.len == 1);
    if (out_type) {
        *out_type = types[0].type;
    }
    return;

expected_op:
    MarkError("Unexpected token '%1', expected operator or ')'", TokenKindNames[(int)tokens[offset].kind]);
    return;
expected_value:
    MarkError("Unexpected token '%1', expected value or '('", TokenKindNames[(int)tokens[offset].kind]);
    return;
}

void Parser::ProduceOperator(const PendingOperator &op)
{
    bool success;

    switch (op.kind) {
        case TokenKind::Assign: {
            const ExpressionType &type1 = types[types.len - 2];
            const ExpressionType &type2 = types[types.len - 1];

            if (!type1.var) {
                MarkError("Cannot assign expression to rvalue");
                return;
            }
            if (type1.type != type2.type) {
                MarkError("Cannot assign %1 value to %2 variable",
                          TypeNames[(int)type2.type], TypeNames[(int)type1.type]);
                return;
            }

            switch (type1.type) {
                case Type::Bool: { ir.Append({Opcode::StoreBool, {.i = type1.var->offset}}); } break;
                case Type::Integer: { ir.Append({Opcode::StoreInt, {.i = type1.var->offset}}); } break;
                case Type::Double: { ir.Append({Opcode::StoreDouble, {.i = type1.var->offset}}); } break;
                case Type::String: { ir.Append({Opcode::StoreString, {.i = type1.var->offset}}); } break;
            }
            types.len--;

            return;
        } break;

        case TokenKind::Plus: {
            success = EmitOperator2(Type::Integer, Opcode::AddInt, Type::Integer) ||
                      EmitOperator2(Type::Double, Opcode::AddDouble, Type::Double);
        } break;
        case TokenKind::Minus: {
            if (op.unary) {
                success = EmitOperator1(Type::Integer, Opcode::NegateInt, Type::Integer) ||
                          EmitOperator1(Type::Double, Opcode::NegateDouble, Type::Double);
            } else {
                success = EmitOperator2(Type::Integer, Opcode::SubstractInt, Type::Integer) ||
                          EmitOperator2(Type::Double, Opcode::SubstractDouble, Type::Double);
            }
        } break;
        case TokenKind::Multiply: {
            success = EmitOperator2(Type::Integer, Opcode::MultiplyInt, Type::Integer) ||
                      EmitOperator2(Type::Double, Opcode::MultiplyDouble, Type::Double);
        } break;
        case TokenKind::Divide: {
            success = EmitOperator2(Type::Integer, Opcode::DivideInt, Type::Integer) ||
                      EmitOperator2(Type::Double, Opcode::DivideDouble, Type::Double);
        } break;
        case TokenKind::Modulo: {
            success = EmitOperator2(Type::Integer, Opcode::ModuloInt, Type::Integer);
        } break;

        case TokenKind::Equal: {
            success = EmitOperator2(Type::Integer, Opcode::EqualInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::EqualDouble, Type::Bool) ||
                      EmitOperator2(Type::Bool, Opcode::EqualBool, Type::Bool);
        } break;
        case TokenKind::NotEqual: {
            success = EmitOperator2(Type::Integer, Opcode::NotEqualInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::NotEqualDouble, Type::Bool) ||
                      EmitOperator2(Type::Bool, Opcode::NotEqualBool, Type::Bool);
        } break;
        case TokenKind::Greater: {
            success = EmitOperator2(Type::Integer, Opcode::GreaterInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::GreaterDouble, Type::Bool);
        } break;
        case TokenKind::GreaterOrEqual: {
            success = EmitOperator2(Type::Integer, Opcode::GreaterOrEqualInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::GreaterOrEqualDouble, Type::Bool);
        } break;
        case TokenKind::Less: {
            success = EmitOperator2(Type::Integer, Opcode::LessInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::LessDouble, Type::Bool);
        } break;
        case TokenKind::LessOrEqual: {
            success = EmitOperator2(Type::Integer, Opcode::LessOrEqualInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::LessOrEqualDouble, Type::Bool);
        } break;

        case TokenKind::And: {
            success = EmitOperator2(Type::Integer, Opcode::AndInt, Type::Integer) ||
                      EmitOperator2(Type::Bool, Opcode::AndBool, Type::Bool);
        } break;
        case TokenKind::Or: {
            success = EmitOperator2(Type::Integer, Opcode::OrInt, Type::Integer) ||
                      EmitOperator2(Type::Bool, Opcode::OrBool, Type::Bool);
        } break;
        case TokenKind::Xor: {
            success = EmitOperator2(Type::Integer, Opcode::XorInt, Type::Integer) ||
                      EmitOperator2(Type::Bool, Opcode::XorBool, Type::Bool);
        } break;
        case TokenKind::Not: {
            success = EmitOperator1(Type::Integer, Opcode::NotInt, Type::Integer) ||
                      EmitOperator1(Type::Bool, Opcode::NotBool, Type::Bool);
        } break;
        case TokenKind::LeftShift: {
            success = EmitOperator2(Type::Integer, Opcode::LeftShiftInt, Type::Integer);
        } break;
        case TokenKind::RightShift: {
            success = EmitOperator2(Type::Integer, Opcode::RightShiftInt, Type::Integer);
        } break;

        case TokenKind::LogicNot: {
            success = EmitOperator1(Type::Bool, Opcode::NotBool, Type::Bool);
        } break;
        case TokenKind::LogicAnd: {
            success = EmitOperator2(Type::Bool, Opcode::AndBool, Type::Bool);

            RG_ASSERT(op.branch_idx && ir[op.branch_idx].code == Opcode::BranchIfFalse);
            ir[op.branch_idx].u.i = ir.len;
        } break;
        case TokenKind::LogicOr: {
            success = EmitOperator2(Type::Bool, Opcode::OrBool, Type::Bool);

            RG_ASSERT(op.branch_idx && ir[op.branch_idx].code == Opcode::BranchIfTrue);
            ir[op.branch_idx].u.i = ir.len;
        } break;

        default: { RG_ASSERT(false); } break;
    }

    if (RG_UNLIKELY(!success)) {
        if (IsUnaryOperator(op.kind)) {
            MarkError("Cannot use '%1' operator on %2 value",
                      TokenKindNames[(int)op.kind], TypeNames[(int)types[types.len - 1].type]);
        } else if (types[types.len - 2].type == types[types.len - 1].type) {
            MarkError("Cannot use '%1' operator on %2 values",
                      TokenKindNames[(int)op.kind], TypeNames[(int)types[types.len - 2].type]);
        } else {
            MarkError("Cannot use '%1' operator on %2 and %3 values",
                      TokenKindNames[(int)op.kind], TypeNames[(int)types[types.len - 2].type],
                      TypeNames[(int)types[types.len - 1].type]);
        }
    }
}

bool Parser::EmitOperator1(Type in_type, Opcode code, Type out_type)
{
    Type type = types[types.len - 1].type;

    if (type == in_type) {
        ir.Append({code});
        types[types.len - 1] = {out_type};

        return true;
    } else {
        return false;
    }
}

bool Parser::EmitOperator2(Type in_type, Opcode code, Type out_type)
{
    Type type1 = types[types.len - 2].type;
    Type type2 = types[types.len - 1].type;

    if (type1 == in_type && type2 == in_type) {
        ir.Append({code});
        types[--types.len - 1] = {out_type};

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

bool Parser::ConsumeToken(TokenKind kind)
{
    if (RG_UNLIKELY(offset >= tokens.len)) {
        MarkError("Unexpected end, expected '%1'", TokenKindNames[(int)kind]);
        return false;
    }
    if (RG_UNLIKELY(tokens[offset].kind != kind)) {
        MarkError("Unexpected token '%1', expected '%2'",
                  TokenKindNames[(int)tokens[offset].kind], TokenKindNames[(int)kind]);
        return false;
    }

    offset++;
    return true;
}

bool Parse(Span<const Token> tokens, const char *filename, HeapArray<Instruction> *out_ir)
{
    Parser parser;
    if (!parser.Parse(tokens, filename))
        return false;

    parser.Finish(out_ir);
    return true;
}

}
