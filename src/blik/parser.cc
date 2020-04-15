// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "lexer.hh"
#include "parser.hh"

namespace RG {

struct PendingOperator {
    TokenKind kind;
    int prec;
    bool unary;

    Size branch_idx; // Used for short-circuit operators
};

struct ExpressionValue {
    Type type;
    const VariableInfo *var;
};

class Parser {
    bool valid = true;

    Span<const Token> tokens;
    Size offset;

    // Reuse for performance
    HeapArray<ExpressionValue> values;
    HeapArray<Size> jumps;
    HeapArray<Instruction> buf;

    Program program;

public:
    bool Parse(Span<const Token> tokens, const char *filename);
    void Finish(Program *out_program);

private:
    void ParseIf();
    void ParseWhile();
    void ParseBlock();
    void ParsePrint();
    Type ParseExpression();
    void ParseDeclaration();

    void ProduceOperator(const PendingOperator &op);
    bool EmitOperator1(Type in_type, Opcode code, Type out_type);
    bool EmitOperator2(Type in_type, Opcode code, Type out_type);

    void EmitPop(int64_t count);

    bool ConsumeToken(TokenKind kind);
    bool MatchToken(TokenKind kind);

    void DestroyVariables(Size start_idx);

    template <typename... Args>
    void MarkError(const char *fmt, Args... args)
    {
        LogError(fmt, args...);
        valid = false;
    }
};

bool Parser::Parse(Span<const Token> tokens, const char *filename)
{
    RG_DEFER_NC(out_guard, ir_len = program.ir.len,
                           variables_len = program.variables.len) {
        program.ir.RemoveFrom(ir_len);
        DestroyVariables(variables_len);
    };

    valid = true;

    this->tokens = tokens;
    offset = 0;

    PushLogFilter([&](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
        if (valid) {
            int32_t line = tokens[std::min(offset, tokens.len - 1)].line;

            char msg_buf[4096];
            Fmt(msg_buf, "%1(%2): %3", filename, line, msg);

            func(level, ctx, msg_buf);
        }
    });
    RG_DEFER { PopLogFilter(); };

    // Do the actual parsing!
    ParseBlock();
    if (RG_UNLIKELY(offset < tokens.len)) {
        MarkError("Unexpected token '%1' without matching block", TokenKindNames[(int)tokens[offset].kind]);
        return false;
    }

    if (valid) {
        out_guard.Disable();
    }
    return valid;
}

void Parser::ParseBlock()
{
    while (valid && offset < tokens.len) {
        switch (tokens[offset++].kind) {
            case TokenKind::End:
            case TokenKind::Else: {
                offset--;
                return;
            } break;

            case TokenKind::NewLine: {} break;

            case TokenKind::Let: {
                ParseDeclaration();
                ConsumeToken(TokenKind::NewLine);
            } break;

            case TokenKind::Do: {
                Size stack_len = program.variables.len;

                ConsumeToken(TokenKind::NewLine);
                ParseBlock();
                ConsumeToken(TokenKind::End);

                EmitPop(program.variables.len - stack_len);
                DestroyVariables(stack_len);
            } break;

            case TokenKind::If: {
                ParseIf();
                ConsumeToken(TokenKind::NewLine);
            } break;

            case TokenKind::While: {
                ParseWhile();
                ConsumeToken(TokenKind::NewLine);
            } break;

            // This will be removed once we get functions, but in the mean time
            // I need to output things somehow!
            case TokenKind::Print: {
                ParsePrint();
                ConsumeToken(TokenKind::NewLine);
            } break;

            default: {
                offset--;

                ParseExpression();
                EmitPop(1);

                ConsumeToken(TokenKind::NewLine);
            } break;
        }
    }
}

void Parser::ParseIf()
{
    jumps.RemoveFrom(0);

    if (RG_UNLIKELY(ParseExpression() != Type::Bool)) {
        MarkError("Cannot use non-Bool expression as condition");
        return;
    }
    ConsumeToken(TokenKind::NewLine);

    Size branch_idx = program.ir.len;
    program.ir.Append({Opcode::BranchIfFalse});

    // Deal with the mandatory block first
    {
        Size stack_len = program.variables.len;

        ParseBlock();

        EmitPop(program.variables.len - stack_len);
        DestroyVariables(stack_len);

        jumps.Append(program.ir.len);
        program.ir.Append({Opcode::Jump});
    }

    while (MatchToken(TokenKind::Else)) {
        program.ir[branch_idx].u.i = program.ir.len - branch_idx;

        if (MatchToken(TokenKind::If)) {
            if (RG_UNLIKELY(ParseExpression() != Type::Bool)) {
                MarkError("Cannot use non-Bool expression as condition");
                return;
            }
            ConsumeToken(TokenKind::NewLine);

            Size stack_len = program.variables.len;

            branch_idx = program.ir.len;
            program.ir.Append({Opcode::BranchIfFalse});

            ParseBlock();

            EmitPop(program.variables.len - stack_len);
            DestroyVariables(stack_len);

            jumps.Append(program.ir.len);
            program.ir.Append({Opcode::Jump});
        } else {
            ConsumeToken(TokenKind::NewLine);

            Size stack_len = program.variables.len;

            ParseBlock();

            EmitPop(program.variables.len - stack_len);
            DestroyVariables(stack_len);

            break;
        }
    }

    ConsumeToken(TokenKind::End);

    program.ir[branch_idx].u.i = program.ir.len;
    for (Size jump_idx: jumps) {
        program.ir[jump_idx].u.i = program.ir.len - jump_idx;
    }
}

void Parser::ParseWhile()
{
    buf.RemoveFrom(0);
    {
        Size test_idx = program.ir.len;

        if (RG_UNLIKELY(ParseExpression() != Type::Bool)) {
            MarkError("Cannot use non-Bool expression as condition");
            return;
        }
        ConsumeToken(TokenKind::NewLine);

        buf.Append(program.ir.Take(test_idx, program.ir.len - test_idx));
        program.ir.len -= buf.len;
    }

    Size jump_idx = program.ir.len;
    program.ir.Append({Opcode::Jump});

    Size block_idx = program.ir.len;

    ParseBlock();
    ConsumeToken(TokenKind::End);

    program.ir[jump_idx].u.i = program.ir.len - jump_idx;
    program.ir.Append(buf);
    program.ir.Append({Opcode::BranchIfTrue, {.i = block_idx - program.ir.len}});
}

void Parser::ParseDeclaration()
{
    // Code below assumes identifier exists for the variable name
    if (RG_UNLIKELY(!ConsumeToken(TokenKind::Identifier)))
        return;

    VariableInfo var = {};

    var.name = tokens[offset - 1].u.str;
    if (MatchToken(TokenKind::Equal)) {
        var.type = ParseExpression();
    } else if (MatchToken(TokenKind::Colon)) {
        if (RG_UNLIKELY(!ConsumeToken(TokenKind::Identifier)))
            return;

        const char *type_name = tokens[offset - 1].u.str;

        if (RG_UNLIKELY(!OptionToEnum(TypeNames, type_name, &var.type))) {
            MarkError("Type '%1' is not valid", type_name);
            return;
        }

        if (MatchToken(TokenKind::Equal)) {
            Type type2 = ParseExpression();

            if (RG_UNLIKELY(type2 != var.type)) {
                MarkError("Cannot assign %1 value to %2 variable",
                          TypeNames[(int)type2], TypeNames[(int)var.type]);
                return;
            }
        } else {
            switch (var.type) {
                case Type::Bool: { program.ir.Append({Opcode::PushBool, {.b = false}}); } break;
                case Type::Integer: { program.ir.Append({Opcode::PushInt, {.i = 0}}); } break;
                case Type::Double: { program.ir.Append({Opcode::PushDouble, {.d = 0.0}}); } break;
                case Type::String: { program.ir.Append({Opcode::PushString, {.str = ""}}); } break;
            }
            values.Append({var.type});
        }
    } else {
        MarkError("Unexpected token '%1', expected '=' or ':'");
        return;
    }
    var.offset = program.variables.len;

    if (!program.variables_map.Append(var).second) {
        MarkError("Variable '%1' already exists", var.name);
        return;
    }
    program.variables.Append(var);
}

void Parser::ParsePrint()
{
    Type type = ParseExpression();
    program.ir.Append({Opcode::Print, {.type = type}});

    while (MatchToken(TokenKind::Comma)) {
        Type type = ParseExpression();
        program.ir.Append({Opcode::Print, {.type = type}});
    }
}

static int GetOperatorPrecedence(TokenKind kind)
{
    switch (kind) {
        case TokenKind::Reassign: { return 0; } break;
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

Type Parser::ParseExpression()
{
    values.RemoveFrom(0);

    LocalArray<PendingOperator, 128> operators;
    bool expect_op = false;
    Size parentheses = 0;

    // Used to detect "empty" expressions
    Size prev_offset = offset;

    for (; offset < tokens.len; offset++) {
        const Token &tok = tokens[offset];

        if (tok.kind == TokenKind::LeftParenthesis) {
            if (RG_UNLIKELY(expect_op))
                goto unexpected_token;

            operators.Append({tok.kind});
            parentheses++;
        } else if (tok.kind == TokenKind::RightParenthesis) {
            if (RG_UNLIKELY(!expect_op))
                goto unexpected_token;
            expect_op = true;

            for (;;) {
                if (RG_UNLIKELY(!operators.len)) {
                    MarkError("Too many closing parentheses");
                    return {};
                }

                const PendingOperator &op = operators.data[operators.len - 1];

                if (op.kind == TokenKind::LeftParenthesis) {
                    operators.len--;
                    parentheses--;
                    break;
                }

                ProduceOperator(op);
                operators.len--;
            }
        } else if (tok.kind == TokenKind::Bool || tok.kind == TokenKind::Integer ||
                   tok.kind == TokenKind::Double || tok.kind == TokenKind::String ||
                   tok.kind == TokenKind::Identifier) {
            if (RG_UNLIKELY(expect_op))
                goto unexpected_token;
            expect_op = true;

            switch (tok.kind) {
                case TokenKind::Bool: {
                    program.ir.Append({Opcode::PushBool, {.b = tok.u.b}});
                    values.Append({Type::Bool});
                } break;
                case TokenKind::Integer: {
                    if (operators.len && operators[operators.len - 1].kind == TokenKind::Minus &&
                                         operators[operators.len - 1].unary) {
                        operators.RemoveLast(1);

                        program.ir.Append({Opcode::PushInt, {.i = -tok.u.i}});
                        values.Append({Type::Integer});
                    } else {
                        program.ir.Append({Opcode::PushInt, {.i = tok.u.i}});
                        values.Append({Type::Integer});
                    }
                } break;
                case TokenKind::Double: {
                    if (operators.len && operators[operators.len - 1].kind == TokenKind::Minus &&
                                         operators[operators.len - 1].unary) {
                        operators.RemoveLast(1);

                        program.ir.Append({Opcode::PushDouble, {.d = -tok.u.d}});
                        values.Append({Type::Integer});
                    } else {
                        program.ir.Append({Opcode::PushDouble, {.d = tok.u.d}});
                        values.Append({Type::Double});
                    }
                } break;
                case TokenKind::String: {
                    program.ir.Append({Opcode::PushString, {.str = tok.u.str}});
                    values.Append({Type::String});
                } break;

                case TokenKind::Identifier: {
                    const VariableInfo *var = program.variables_map.Find(tok.u.str);

                    if (RG_UNLIKELY(!var)) {
                        MarkError("Variable '%1' is not defined", tok.u.str);
                        return {};
                    }

                    switch (var->type) {
                        case Type::Bool: { program.ir.Append({Opcode::LoadBool, {.i = var->offset}}); } break;
                        case Type::Integer: { program.ir.Append({Opcode::LoadInt, {.i = var->offset}}); } break;
                        case Type::Double: { program.ir.Append({Opcode::LoadDouble, {.i = var->offset}});} break;
                        case Type::String: { program.ir.Append({Opcode::LoadString, {.i = var->offset}}); } break;
                    }
                    values.Append({var->type, var});
                } break;

                default: { RG_ASSERT(false); } break;
            }
        } else {
            PendingOperator op = {};

            op.kind = tok.kind;
            op.prec = GetOperatorPrecedence(tok.kind);
            op.unary = (tok.kind == TokenKind::Not || tok.kind == TokenKind::LogicNot);

            if (RG_UNLIKELY(op.prec < 0)) {
                if (offset == prev_offset) {
                    if (offset >= tokens.len) {
                        MarkError("Unexpected end of file, expected expression");
                    } else {
                        MarkError("Unexpected token '%1', expected expression", TokenKindNames[(int)tokens[offset].kind]);
                    }

                    return {};
                } else if (!expect_op && tok.kind == TokenKind::NewLine) {
                    // Expression is split across multiple lines
                    continue;
                } else if (parentheses || !expect_op) {
                    goto unexpected_token;
                } else {
                    break;
                }
            }
            if (RG_UNLIKELY(expect_op == op.unary)) {
                if (tok.kind == TokenKind::Plus) {
                    continue;
                } else if (tok.kind == TokenKind::Minus) {
                    op.prec = 12;
                    op.unary = true;
                } else {
                    goto unexpected_token;
                }
            }
            expect_op = false;

            while (operators.len) {
                const PendingOperator &op2 = operators[operators.len - 1];
                bool right_associative = (op2.unary || op2.kind == TokenKind::Reassign);

                if (op2.kind == TokenKind::LeftParenthesis)
                    break;
                if (op2.prec - right_associative < op.prec)
                    break;

                ProduceOperator(op2);
                operators.len--;
            }

            if (tok.kind == TokenKind::Reassign) {
                program.ir.RemoveLast(1); // Remove load instruction
            } else if (tok.kind == TokenKind::LogicAnd) {
                op.branch_idx = program.ir.len;
                program.ir.Append({Opcode::SkipIfFalse});
            } else if (tok.kind == TokenKind::LogicOr) {
                op.branch_idx = program.ir.len;
                program.ir.Append({Opcode::SkipIfTrue});
            }

            if (RG_UNLIKELY(!operators.Available())) {
                MarkError("Too many operators on the stack");
                return {};
            }
            operators.Append(op);
        }
    }

    if (RG_UNLIKELY(!expect_op)) {
        MarkError("Unexpected end of expression, expected value or '('");
        return {};
    }
    if (RG_UNLIKELY(parentheses)) {
        MarkError("Missing closing parenthesis");
        return {};
    }

    for (Size i = operators.len - 1; i >= 0; i--) {
        const PendingOperator &op = operators[i];
        ProduceOperator(op);
    }

    if (RG_UNLIKELY(!valid))
        return {};

    RG_ASSERT(values.len == 1);
    return values[0].type;

unexpected_token:
    MarkError("Unexpected token '%1', expected %2", TokenKindNames[(int)tokens[offset].kind],
              expect_op ? "operator or ')'" : "value or '('");
    return {};
}

void Parser::ProduceOperator(const PendingOperator &op)
{
    bool success;

    switch (op.kind) {
        case TokenKind::Reassign: {
            const ExpressionValue &value1 = values[values.len - 2];
            const ExpressionValue &value2 = values[values.len - 1];

            if (RG_UNLIKELY(!value1.var)) {
                MarkError("Cannot assign expression to rvalue");
                return;
            }
            if (RG_UNLIKELY(value1.type != value2.type)) {
                MarkError("Cannot assign %1 value to %2 variable",
                          TypeNames[(int)value2.type], TypeNames[(int)value1.type]);
                return;
            }

            switch (value1.type) {
                case Type::Bool: { program.ir.Append({Opcode::StoreBool, {.i = value1.var->offset}}); } break;
                case Type::Integer: { program.ir.Append({Opcode::StoreInt, {.i = value1.var->offset}}); } break;
                case Type::Double: { program.ir.Append({Opcode::StoreDouble, {.i = value1.var->offset}}); } break;
                case Type::String: { program.ir.Append({Opcode::StoreString, {.i = value1.var->offset}}); } break;
            }
            values.len--;

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

            RG_ASSERT(op.branch_idx && program.ir[op.branch_idx].code == Opcode::SkipIfFalse);
            program.ir[op.branch_idx].u.i = program.ir.len - op.branch_idx;
        } break;
        case TokenKind::LogicOr: {
            success = EmitOperator2(Type::Bool, Opcode::OrBool, Type::Bool);

            RG_ASSERT(op.branch_idx && program.ir[op.branch_idx].code == Opcode::SkipIfTrue);
            program.ir[op.branch_idx].u.i = program.ir.len - op.branch_idx;
        } break;

        default: { RG_ASSERT(false); } break;
    }

    if (RG_UNLIKELY(!success)) {
        if (op.unary) {
            MarkError("Cannot use '%1' operator on %2 value",
                      TokenKindNames[(int)op.kind], TypeNames[(int)values[values.len - 1].type]);
        } else if (values[values.len - 2].type == values[values.len - 1].type) {
            MarkError("Cannot use '%1' operator on %2 values",
                      TokenKindNames[(int)op.kind], TypeNames[(int)values[values.len - 2].type]);
        } else {
            MarkError("Cannot use '%1' operator on %2 and %3 values",
                      TokenKindNames[(int)op.kind], TypeNames[(int)values[values.len - 2].type],
                      TypeNames[(int)values[values.len - 1].type]);
        }
    }
}

bool Parser::EmitOperator1(Type in_type, Opcode code, Type out_type)
{
    Type type = values[values.len - 1].type;

    if (type == in_type) {
        program.ir.Append({code});
        values[values.len - 1] = {out_type};

        return true;
    } else {
        return false;
    }
}

bool Parser::EmitOperator2(Type in_type, Opcode code, Type out_type)
{
    Type type1 = values[values.len - 2].type;
    Type type2 = values[values.len - 1].type;

    if (type1 == in_type && type2 == in_type) {
        program.ir.Append({code});
        values[--values.len - 1] = {out_type};

        return true;
    } else {
        return false;
    }
}

void Parser::EmitPop(int64_t count)
{
    RG_ASSERT(count >= 0);

    if (program.ir.len && program.ir[program.ir.len - 1].code == Opcode::Pop) {
        program.ir[program.ir.len - 1].u.i += count;
    } else if (count) {
        program.ir.Append({Opcode::Pop, {.i = count}});
    }
}

void Parser::Finish(Program *out_program)
{
    RG_ASSERT(!out_program->ir.len);

    program.ir.Append({Opcode::Exit});
    SwapMemory(&program, out_program, RG_SIZE(program));
}

bool Parser::ConsumeToken(TokenKind kind)
{
    if (RG_UNLIKELY(offset >= tokens.len)) {
        MarkError("Unexpected end of file, expected '%1'", TokenKindNames[(int)kind]);
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

bool Parser::MatchToken(TokenKind kind)
{
    bool match = offset < tokens.len && tokens[offset].kind == kind;
    offset += match;
    return match;
}

void Parser::DestroyVariables(Size start_idx)
{
    for (Size i = start_idx; i < program.variables.len; i++) {
        program.variables_map.Remove(program.variables[i].name);
    }

    program.variables.RemoveFrom(start_idx);
}

bool Parse(Span<const Token> tokens, const char *filename, Program *out_program)
{
    Parser parser;
    if (!parser.Parse(tokens, filename))
        return false;

    parser.Finish(out_program);
    return true;
}

}
