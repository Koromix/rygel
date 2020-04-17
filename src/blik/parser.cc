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
    HeapArray<Type> types;

    FunctionInfo *func = nullptr;
    Size func_var_offset;

    HeapArray<Instruction> *ir;
    Program program;

public:
    bool Parse(const TokenSet &set, const char *filename);
    void Finish(Program *out_program);

private:
    bool ParseBlock();
    void ParseFunction();
    void ParseReturn();
    void ParseDeclaration();
    void ParseIf();
    void ParseWhile();
    void ParsePrint();

    Type ParseExpression(bool keep_result);
    void ProduceOperator(const PendingOperator &op);
    bool EmitOperator1(Type in_type, Opcode code, Type out_type);
    bool EmitOperator2(Type in_type, Opcode code, Type out_type);

    void EmitPop(int64_t count);

    bool ConsumeToken(TokenKind kind);
    const char *ConsumeIdentifier();
    Type ConsumeType();
    bool MatchToken(TokenKind kind);

    void DestroyVariables(Size start_idx);

    template <typename... Args>
    void MarkError(const char *fmt, Args... args)
    {
        LogError(fmt, args...);
        valid = false;
    }
};

bool Parser::Parse(const TokenSet &set, const char *filename)
{
    RG_DEFER_NC(out_guard, ir_len = program.ir.len,
                           variables_len = program.variables.len) {
        program.ir.RemoveFrom(ir_len);
        DestroyVariables(variables_len);
    };

    valid = true;

    tokens = set.tokens;
    offset = 0;
    ir = &program.ir;

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

    RG_ASSERT(ir == &program.ir);

    if (valid) {
        out_guard.Disable();
    }
    return valid;
}

bool Parser::ParseBlock()
{
    Size stack_len = program.variables.len;
    bool has_return = false;

    while (valid && offset < tokens.len) {
        switch (tokens[offset++].kind) {
            case TokenKind::End:
            case TokenKind::Else: {
                offset--;
                goto done;
            } break;

            case TokenKind::NewLine: {} break;

            case TokenKind::Let: {
                ParseDeclaration();
                ConsumeToken(TokenKind::NewLine);
            } break;

            case TokenKind::Func: {
                Size jump_idx = ir->len;
                ir->Append({Opcode::Jump});

                ParseFunction();
                ConsumeToken(TokenKind::NewLine);

                (*ir)[jump_idx].u.i = ir->len - jump_idx;
            } break;

            case TokenKind::Return: {
                ParseReturn();
                ConsumeToken(TokenKind::NewLine);

                has_return = true;
            } break;

            case TokenKind::Do: {
                ConsumeToken(TokenKind::NewLine);
                has_return |= ParseBlock();
                ConsumeToken(TokenKind::End);
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

                ParseExpression(false);
                ConsumeToken(TokenKind::NewLine);
            } break;
        }
    }

done:
    EmitPop(program.variables.len - stack_len);
    DestroyVariables(stack_len);

    return has_return;
}

void Parser::ParseFunction()
{
    RG_DEFER_C(variables_len = program.variables.len) {
        func = nullptr;
        DestroyVariables(variables_len);
    };

    if (RG_UNLIKELY(func)) {
        LogError("Nested functions are not supported");
        return;
    }

    func = program.functions.AppendDefault();
    func->name = ConsumeIdentifier();

    if (RG_UNLIKELY(!program.functions_map.Append(func).second)) {
        MarkError("Function '%1' already exists", func->name);
    }

    // Parameters
    ConsumeToken(TokenKind::LeftParenthesis);
    if (!MatchToken(TokenKind::RightParenthesis)) {
        do {
            VariableInfo *var = program.variables.AppendDefault();
            var->name = ConsumeIdentifier();

            std::pair<VariableInfo **, bool> ret = program.variables_map.Append(var);
            if (RG_UNLIKELY(!ret.second)) {
               const VariableInfo *prev_var = *ret.first;

                if (prev_var->global) {
                    MarkError("Parameter '%1' is not allowed to hide global variable", var->name);
                } else {
                    MarkError("Parameter '%1' already exists", var->name);
                }

                return;
            }

            ConsumeToken(TokenKind::Colon);
            var->type = ConsumeType();
            func->params.Append(var->type);
        } while (MatchToken(TokenKind::Comma));

        // We need to know the number of parameters to compute stack offsets
        for (Size i = 1; i <= func->params.len; i++) {
            VariableInfo *var = &program.variables[program.variables.len - i];
            var->offset = -2 - i;
        }

        ConsumeToken(TokenKind::RightParenthesis);
    }

    // Return type
    ConsumeToken(TokenKind::Colon);
    func->ret = ConsumeType();
    ConsumeToken(TokenKind::NewLine);

    // Function body
    func->addr = ir->len;
    func_var_offset = program.variables.len;
    if (RG_UNLIKELY(!ParseBlock())) {
        MarkError("Function '%1' does not have a return statement", func->name);
    }
    ConsumeToken(TokenKind::End);
}

void Parser::ParseReturn()
{
    if (RG_UNLIKELY(!func)) {
        MarkError("Return statement cannot be used outside function");
        return;
    }

    Type type = ParseExpression(true);
    if (RG_UNLIKELY(type != func->ret)) {
        MarkError("Cannot return %1 value (expected %2)",
                  TypeNames[(int)type], TypeNames[(int)func->ret]);
        return;
    }

    if (program.variables.len - func_var_offset > 0) {
        switch (type) {
            case Type::Bool: { ir->Append({Opcode::StoreLocalBool, {.i = 0}}); } break;
            case Type::Integer: { ir->Append({Opcode::StoreLocalInt, {.i = 0}}); } break;
            case Type::Double: { ir->Append({Opcode::StoreLocalDouble, {.i = 0}}); } break;
            case Type::String: { ir->Append({Opcode::StoreLocalString, {.i = 0}}); } break;
        }

        EmitPop(program.variables.len - func_var_offset - 1);
    }
    ir->Append({Opcode::Return, {.i = func->params.len}});
}

void Parser::ParseDeclaration()
{
    VariableInfo *var = program.variables.AppendDefault();
    var->name = ConsumeIdentifier();

    std::pair<VariableInfo **, bool> ret = program.variables_map.Append(var);
    if (RG_UNLIKELY(!ret.second)) {
        const VariableInfo *prev_var = *ret.first;

        if (func && prev_var->global) {
            MarkError("Declaration '%1' is not allowed to hide global variable", var->name);
        } else if (func && prev_var->offset < 0) {
            MarkError("Declaration '%1' is not allowed to hide parameter", var->name);
        } else {
            MarkError("Variable '%1' already exists", var->name);
        }
    }

    if (MatchToken(TokenKind::Equal)) {
        var->type = ParseExpression(true);
    } else if (MatchToken(TokenKind::Colon)) {
        var->type = ConsumeType();

        if (MatchToken(TokenKind::Equal)) {
            Type type2 = ParseExpression(true);

            if (RG_UNLIKELY(type2 != var->type)) {
                MarkError("Cannot assign %1 value to %2 variable",
                          TypeNames[(int)type2], TypeNames[(int)var->type]);
                return;
            }
        } else {
            switch (var->type) {
                case Type::Bool: { ir->Append({Opcode::PushBool, {.b = false}}); } break;
                case Type::Integer: { ir->Append({Opcode::PushInt, {.i = 0}}); } break;
                case Type::Double: { ir->Append({Opcode::PushDouble, {.d = 0.0}}); } break;
                case Type::String: { ir->Append({Opcode::PushString, {.str = ""}}); } break;
            }
        }
    } else {
        MarkError("Unexpected token '%1', expected '=' or ':'");
        return;
    }

    if (func) {
        var->offset = program.variables.len - func_var_offset - 1;
    } else {
        var->global = true;
        var->offset = program.variables.len - 1;
    }
}

void Parser::ParseIf()
{
    jumps.RemoveFrom(0);

    if (RG_UNLIKELY(ParseExpression(true) != Type::Bool)) {
        MarkError("Cannot use non-Bool expression as condition");
        return;
    }

    Size branch_idx = ir->len;
    ir->Append({Opcode::BranchIfFalse});

    if (MatchToken(TokenKind::Do)) {
        ParseExpression(false);
        (*ir)[branch_idx].u.i = ir->len - branch_idx;
    } else {
        ConsumeToken(TokenKind::NewLine);
        ParseBlock();

        if (MatchToken(TokenKind::Else)) {
            jumps.Append(ir->len);
            ir->Append({Opcode::Jump});

            do {
                (*ir)[branch_idx].u.i = ir->len - branch_idx;

                if (MatchToken(TokenKind::If)) {
                    if (RG_UNLIKELY(ParseExpression(true) != Type::Bool)) {
                        MarkError("Cannot use non-Bool expression as condition");
                        return;
                    }
                    ConsumeToken(TokenKind::NewLine);

                    branch_idx = ir->len;
                    ir->Append({Opcode::BranchIfFalse});

                    ParseBlock();

                    jumps.Append(ir->len);
                    ir->Append({Opcode::Jump});
                } else {
                    ConsumeToken(TokenKind::NewLine);

                    ParseBlock();
                    break;
                }
            } while (MatchToken(TokenKind::Else));

            for (Size jump_idx: jumps) {
                (*ir)[jump_idx].u.i = ir->len - jump_idx;
            }
        } else {
            (*ir)[branch_idx].u.i = ir->len - branch_idx;
        }

        ConsumeToken(TokenKind::End);
    }
}

void Parser::ParseWhile()
{
    buf.RemoveFrom(0);
    {
        RG_DEFER_C(prev_ir = ir) { ir = prev_ir; };
        ir = &buf;

        if (RG_UNLIKELY(ParseExpression(true) != Type::Bool)) {
            MarkError("Cannot use non-Bool expression as condition");
            return;
        }
    }

    Size jump_idx = ir->len;
    ir->Append({Opcode::Jump});

    Size block_idx = ir->len;

    if (MatchToken(TokenKind::Do)) {
        ParseExpression(false);
    } else {
        ConsumeToken(TokenKind::NewLine);
        ParseBlock();
        ConsumeToken(TokenKind::End);
    }

    (*ir)[jump_idx].u.i = ir->len - jump_idx;
    ir->Append(buf);
    ir->Append({Opcode::BranchIfTrue, {.i = block_idx - ir->len}});
}

void Parser::ParsePrint()
{
    Type type = ParseExpression(true);
    ir->Append({Opcode::Print, {.type = type}});

    while (MatchToken(TokenKind::Comma)) {
        Type type = ParseExpression(true);
        ir->Append({Opcode::Print, {.type = type}});
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

Type Parser::ParseExpression(bool keep_result)
{
    Size start_values_len = values.len;
    RG_DEFER { values.RemoveFrom(start_values_len); };

    LocalArray<PendingOperator, 128> operators;
    bool expect_op = false;
    Size parentheses = 0;

    // Used to detect "empty" expressions
    Size prev_offset = offset;

    while (offset < tokens.len) {
        const Token &tok = tokens[offset++];

        if (tok.kind == TokenKind::LeftParenthesis) {
            if (RG_UNLIKELY(expect_op))
                goto unexpected_token;

            operators.Append({tok.kind});
            parentheses++;
        } else if (parentheses && tok.kind == TokenKind::RightParenthesis) {
            if (RG_UNLIKELY(!expect_op))
                goto unexpected_token;
            expect_op = true;

            for (;;) {
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
                    ir->Append({Opcode::PushBool, {.b = tok.u.b}});
                    values.Append({Type::Bool});
                } break;
                case TokenKind::Integer: {
                    if (operators.len && operators[operators.len - 1].kind == TokenKind::Minus &&
                                         operators[operators.len - 1].unary) {
                        operators.RemoveLast(1);

                        ir->Append({Opcode::PushInt, {.i = -tok.u.i}});
                        values.Append({Type::Integer});
                    } else {
                        ir->Append({Opcode::PushInt, {.i = tok.u.i}});
                        values.Append({Type::Integer});
                    }
                } break;
                case TokenKind::Double: {
                    if (operators.len && operators[operators.len - 1].kind == TokenKind::Minus &&
                                         operators[operators.len - 1].unary) {
                        operators.RemoveLast(1);

                        ir->Append({Opcode::PushDouble, {.d = -tok.u.d}});
                        values.Append({Type::Integer});
                    } else {
                        ir->Append({Opcode::PushDouble, {.d = tok.u.d}});
                        values.Append({Type::Double});
                    }
                } break;
                case TokenKind::String: {
                    ir->Append({Opcode::PushString, {.str = tok.u.str}});
                    values.Append({Type::String});
                } break;

                case TokenKind::Identifier: {
                    if (MatchToken(TokenKind::LeftParenthesis)) {
                        types.RemoveFrom(0);

                        const FunctionInfo *func = program.functions_map.FindValue(tok.u.str, nullptr);

                        if (RG_UNLIKELY(!func)) {
                            MarkError("Function '%1' does not exist", tok.u.str);
                            return {};
                        }

                        if (!MatchToken(TokenKind::RightParenthesis)) {
                            types.Append(ParseExpression(true));
                            while (MatchToken(TokenKind::Comma)) {
                                types.Append(ParseExpression(true));
                            }

                            ConsumeToken(TokenKind::RightParenthesis);
                        }

                        bool mismatch = false;
                        if (RG_UNLIKELY(types.len != func->params.len)) {
                            MarkError("Function '%1' expects %2 arguments, not %3", func->name,
                                      func->params.len, types.len);
                            return {};
                        }
                        for (Size i = 0; i < types.len; i++) {
                            if (RG_UNLIKELY(types[i] != func->params[i])) {
                                MarkError("Function '%1' expects %2 as %3 argument, not %4",
                                          func->name, TypeNames[(int)func->params[i]], i + 1,
                                          TypeNames[(int)types[i]]);
                                mismatch = true;
                            }
                        }
                        if (RG_UNLIKELY(mismatch))
                            return {};

                        ir->Append({Opcode::Call, {.i = func->addr - ir->len}});
                        values.Append({func->ret});
                    } else {
                        const VariableInfo *var = program.variables_map.FindValue(tok.u.str, nullptr);

                        if (RG_UNLIKELY(!var)) {
                            MarkError("Variable '%1' does not exist", tok.u.str);
                            return {};
                        }

                        if (var->global) {
                            switch (var->type) {
                                case Type::Bool: { ir->Append({Opcode::LoadGlobalBool, {.i = var->offset}}); } break;
                                case Type::Integer: { ir->Append({Opcode::LoadGlobalInt, {.i = var->offset}}); } break;
                                case Type::Double: { ir->Append({Opcode::LoadGlobalDouble, {.i = var->offset}});} break;
                                case Type::String: { ir->Append({Opcode::LoadGlobalString, {.i = var->offset}}); } break;
                            }
                        } else {
                            switch (var->type) {
                                case Type::Bool: { ir->Append({Opcode::LoadLocalBool, {.i = var->offset}}); } break;
                                case Type::Integer: { ir->Append({Opcode::LoadLocalInt, {.i = var->offset}}); } break;
                                case Type::Double: { ir->Append({Opcode::LoadLocalDouble, {.i = var->offset}});} break;
                                case Type::String: { ir->Append({Opcode::LoadLocalString, {.i = var->offset}}); } break;
                            }
                        }
                        values.Append({var->type, var});
                    }
                } break;

                default: { RG_ASSERT(false); } break;
            }
        } else {
            PendingOperator op = {};

            op.kind = tok.kind;
            op.prec = GetOperatorPrecedence(tok.kind);
            op.unary = (tok.kind == TokenKind::Not || tok.kind == TokenKind::LogicNot);

            if (RG_UNLIKELY(op.prec < 0)) {
                if (offset == prev_offset + 1) {
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
                    offset--;
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
                // Remove useless load instruction. We don't remove the variable from the
                // stack of values, because it will be needed when we emit the store instruction
                // and will be removed then.
                ir->RemoveLast(1);
            } else if (tok.kind == TokenKind::LogicAnd) {
                op.branch_idx = ir->len;
                ir->Append({Opcode::SkipIfFalse});
            } else if (tok.kind == TokenKind::LogicOr) {
                op.branch_idx = ir->len;
                ir->Append({Opcode::SkipIfTrue});
            }

            if (RG_UNLIKELY(!operators.Available())) {
                MarkError("Too many operators on the stack");
                return {};
            }
            operators.Append(op);
        }
    }

    // Discharge remaining operators
    for (Size i = operators.len - 1; i >= 0; i--) {
        const PendingOperator &op = operators[i];
        ProduceOperator(op);
    }

    if (RG_UNLIKELY(!valid))
        return {};
    if (RG_UNLIKELY(!expect_op)) {
        MarkError("Unexpected end of expression, expected value or '('");
        return {};
    }
    if (RG_UNLIKELY(parentheses)) {
        MarkError("Missing closing parenthesis");
        return {};
    }

    RG_ASSERT(values.len == start_values_len + 1);
    if (keep_result) {
        return values[0].type;
    } else {
        if (ir->len >= 2 && (*ir)[ir->len - 2].code == Opcode::Duplicate) {
            std::swap((*ir)[ir->len - 2], (*ir)[ir->len - 1]);
            ir->len--;
        } else {
            EmitPop(1);
        }

        return {};
    }

unexpected_token:
    MarkError("Unexpected token '%1', expected %2", TokenKindNames[(int)tokens[offset - 1].kind],
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

            ir->Append({Opcode::Duplicate});
            if (value1.var->global) {
                switch (value1.type) {
                    case Type::Bool: { ir->Append({Opcode::StoreGlobalBool, {.i = value1.var->offset}}); } break;
                    case Type::Integer: { ir->Append({Opcode::StoreGlobalInt, {.i = value1.var->offset}}); } break;
                    case Type::Double: { ir->Append({Opcode::StoreGlobalDouble, {.i = value1.var->offset}}); } break;
                    case Type::String: { ir->Append({Opcode::StoreGlobalString, {.i = value1.var->offset}}); } break;
                }
            } else {
                switch (value1.type) {
                    case Type::Bool: { ir->Append({Opcode::StoreLocalBool, {.i = value1.var->offset}}); } break;
                    case Type::Integer: { ir->Append({Opcode::StoreLocalInt, {.i = value1.var->offset}}); } break;
                    case Type::Double: { ir->Append({Opcode::StoreLocalDouble, {.i = value1.var->offset}}); } break;
                    case Type::String: { ir->Append({Opcode::StoreLocalString, {.i = value1.var->offset}}); } break;
                }
            }

            std::swap(values[values.len - 1], values[values.len - 2]);
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
            success = EmitOperator2(Type::Integer, Opcode::GreaterThanInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::GreaterThanDouble, Type::Bool);
        } break;
        case TokenKind::GreaterOrEqual: {
            success = EmitOperator2(Type::Integer, Opcode::GreaterOrEqualInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::GreaterOrEqualDouble, Type::Bool);
        } break;
        case TokenKind::Less: {
            success = EmitOperator2(Type::Integer, Opcode::LessThanInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::LessThanDouble, Type::Bool);
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
                      EmitOperator2(Type::Bool, Opcode::NotEqualBool, Type::Bool);
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

            RG_ASSERT(op.branch_idx && (*ir)[op.branch_idx].code == Opcode::SkipIfFalse);
            (*ir)[op.branch_idx].u.i = ir->len - op.branch_idx;
        } break;
        case TokenKind::LogicOr: {
            success = EmitOperator2(Type::Bool, Opcode::OrBool, Type::Bool);

            RG_ASSERT(op.branch_idx && (*ir)[op.branch_idx].code == Opcode::SkipIfTrue);
            (*ir)[op.branch_idx].u.i = ir->len - op.branch_idx;
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
        ir->Append({code});
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
        ir->Append({code});
        values[--values.len - 1] = {out_type};

        return true;
    } else {
        return false;
    }
}

void Parser::EmitPop(int64_t count)
{
    RG_ASSERT(count >= 0);

    if (count) {
        ir->Append({Opcode::Pop, {.i = count}});
    }
}

void Parser::Finish(Program *out_program)
{
    RG_ASSERT(!out_program->ir.len);

    ir->Append({Opcode::PushInt, {.i = 0}});
    ir->Append({Opcode::Exit});

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

const char *Parser::ConsumeIdentifier()
{
    if (RG_LIKELY(ConsumeToken(TokenKind::Identifier))) {
        return tokens[offset - 1].u.str;
    } else {
        return "";
    }
}

Type Parser::ConsumeType()
{
    const char *type_name = ConsumeIdentifier();

    Type type;
    if (RG_LIKELY(OptionToEnum(TypeNames, type_name, &type))) {
        return type;
    } else {
        MarkError("Type '%1' is not valid", type_name);
        return {};
    }
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

bool Parse(const TokenSet &set, const char *filename, Program *out_program)
{
    Parser parser;
    if (!parser.Parse(set, filename))
        return false;

    parser.Finish(out_program);
    return true;
}

}
