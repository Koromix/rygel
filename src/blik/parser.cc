// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "lexer.hh"
#include "parser.hh"

namespace RG {

struct ForwardCall {
    Size offset;
    const FunctionInfo *func;
};

struct PendingOperator {
    TokenKind kind;
    int prec;
    bool unary;

    Size branch_idx; // Used for short-circuit operators
};

struct StackSlot {
    Type type;
    const VariableInfo *var;
};

class Parser {
    bool valid = true;

    Span<const Token> tokens;
    Size offset;

    BucketArray<FunctionInfo> functions;
    HashTable<const char *, FunctionInfo *> functions_map;
    BucketArray<VariableInfo> variables;
    HashTable<const char *, VariableInfo *> variables_map;

    Size depth = -1;
    FunctionInfo *current_func = nullptr;
    Size var_offset = 0;

    // Only used (and valid) while parsing expression
    HeapArray<StackSlot> stack;

    HeapArray<ForwardCall> forward_calls;

    Program program;

public:
    bool Parse(const TokenSet &set, const char *filename);
    void Finish(Program *out_program);

private:
    void ParsePrototypes(Span<const Size> offsets);

    bool ParseBlock(bool keep_variables);
    void ParseFunction();
    void ParseReturn();
    void ParseLet();
    void ParseIf();
    void ParseWhile();
    void ParseFor();

    void ParseExpressionOrReturn();

    Type ParseExpression(bool keep_result);
    bool ParseCall(const char *name);
    void ProduceOperator(const PendingOperator &op);
    bool EmitOperator1(Type in_type, Opcode code, Type out_type);
    bool EmitOperator2(Type in_type, Opcode code, Type out_type);

    void EmitPop(int64_t count);

    bool ConsumeToken(TokenKind kind);
    const char *ConsumeIdentifier();
    Type ConsumeType();
    bool MatchToken(TokenKind kind);

    void DestroyVariables(Size count);

    template <typename... Args>
    void MarkError(const char *fmt, Args... args)
    {
        LogError(fmt, args...);
        valid = false;
    }
};

bool Parser::Parse(const TokenSet &set, const char *filename)
{
    RG_ASSERT(valid);

    tokens = set.tokens;
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

    // We want top-level order-independent functions
    ParsePrototypes(set.funcs);
    if (!valid)
        return false;

    // Do the actual parsing!
    ParseBlock(true);
    if (RG_UNLIKELY(offset < tokens.len)) {
        MarkError("Unexpected token '%1' without matching block", TokenKindNames[(int)tokens[offset].kind]);
        return false;
    }
    RG_ASSERT(depth == -1);

    for (const ForwardCall &call: forward_calls) {
        program.ir[call.offset].u.i = call.func->addr;
    }
    forward_calls.Clear();

    return valid;
}

void Parser::ParsePrototypes(Span<const Size> offsets)
{
    RG_DEFER_C(prev_offset = offset) { offset = prev_offset; };

    for (Size i = 0; i < offsets.len; i++) {
        offset = offsets[i] + 1;

        FunctionInfo *proto = functions.AppendDefault();
        proto->name = ConsumeIdentifier();

        if (RG_UNLIKELY(!functions_map.Append(proto).second)) {
            MarkError("Function '%1' already exists", proto->name);
            return;
        }

         // Parameters
        ConsumeToken(TokenKind::LeftParenthesis);
        if (!MatchToken(TokenKind::RightParenthesis)) {
            do {
                FunctionInfo::Parameter param = {};

                param.name = ConsumeIdentifier();
                ConsumeToken(TokenKind::Colon);
                param.type = ConsumeType();

                if (RG_UNLIKELY(!proto->params.Available())) {
                    MarkError("Functions cannot have more than %1 parameters", RG_LEN(proto->params.data));
                    return;
                }
                proto->params.Append(param);
            } while (MatchToken(TokenKind::Comma));

            ConsumeToken(TokenKind::RightParenthesis);
        }

        if (MatchToken(TokenKind::Colon)) {
            proto->ret = ConsumeType();
        } else {
            proto->ret = Type::Null;
        }

        proto->addr = -1;
        proto->earliest_forward_call = RG_SIZE_MAX;
    }
}

bool Parser::ParseBlock(bool keep_variables)
{
    depth++;

    RG_DEFER_C(variables_len = variables.len) {
        depth--;

        if (!keep_variables) {
            EmitPop(variables.len - variables_len);
            DestroyVariables(variables.len - variables_len);
        }
    };

    bool has_return = false;

    while (valid && offset < tokens.len) {
        switch (tokens[offset].kind) {
            case TokenKind::NewLine: { offset++; } break;

            case TokenKind::End:
            case TokenKind::Else: { return has_return; } break;

            case TokenKind::Begin: {
                offset++;

                ConsumeToken(TokenKind::NewLine);
                has_return |= ParseBlock(false);
                ConsumeToken(TokenKind::End);
                ConsumeToken(TokenKind::NewLine);
            } break;

            case TokenKind::Func: {
                Size jump_idx = program.ir.len;
                program.ir.Append({Opcode::Jump});

                ParseFunction();

                program.ir[jump_idx].u.i = program.ir.len - jump_idx;
            } break;

            case TokenKind::Return: {
                ParseReturn();
                has_return = true;
            } break;

            case TokenKind::Let: { ParseLet(); } break;
            case TokenKind::If: { ParseIf(); } break;
            case TokenKind::While: { ParseWhile(); } break;
            case TokenKind::For: { ParseFor(); } break;

            default: {
                ParseExpression(false);
                ConsumeToken(TokenKind::NewLine);
            } break;
        }
    }

    return has_return;
}

void Parser::ParseFunction()
{
    offset++;

    RG_DEFER_C(prev_offset = var_offset) {
        // Variables inside the function are destroyed at the end of the block.
        // This destroys the parameters.
        DestroyVariables(current_func->params.len);

        current_func = nullptr;
        var_offset = prev_offset;
    };

    if (RG_UNLIKELY(current_func)) {
        MarkError("Nested functions are not supported");
        return;
    }
    if (RG_UNLIKELY(depth)) {
        MarkError("Functions must be defined in top-level scope");
        return;
    }

    current_func = functions_map.FindValue(ConsumeIdentifier(), nullptr);
    if (RG_UNLIKELY(!current_func)) {
        RG_ASSERT(!valid);
        return;
    }

    // Parameters
    ConsumeToken(TokenKind::LeftParenthesis);
    if (!MatchToken(TokenKind::RightParenthesis)) {
        do {
            VariableInfo *var = variables.AppendDefault();
            var->name = ConsumeIdentifier();

            std::pair<VariableInfo **, bool> ret = variables_map.Append(var);
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
        } while (MatchToken(TokenKind::Comma));

        // We need to know the number of parameters to compute stack offsets
        for (Size i = 1; i <= current_func->params.len; i++) {
            VariableInfo *var = &variables[variables.len - i];
            var->offset = -2 - i;
        }

        ConsumeToken(TokenKind::RightParenthesis);
    }

    // Return type
    if (MatchToken(TokenKind::Colon)) {
        ConsumeType();
    }

    current_func->addr = program.ir.len;
    var_offset = 0;

    // Function body
    if (MatchToken(TokenKind::Do)) {
        ParseExpression(true);
        program.ir.Append({Opcode::Return, {.i = current_func->params.len}});
    } else {
        ConsumeToken(TokenKind::NewLine);

        // Function body
        bool has_return = ParseBlock(false);
        if (!has_return) {
            if (current_func->ret == Type::Null) {
                program.ir.Append({Opcode::PushNull});
                program.ir.Append({Opcode::Return, {.i = current_func->params.len}});
            } else {
                MarkError("Function '%1' does not have a return statement", current_func->name);
                return;
            }
        }

        ConsumeToken(TokenKind::End);
    }

    ConsumeToken(TokenKind::NewLine);
}

void Parser::ParseReturn()
{
    offset++;

    if (RG_UNLIKELY(!current_func)) {
        MarkError("Return statement cannot be used outside function");
        return;
    }

    Type type;
    if (MatchToken(TokenKind::NewLine)) {
        offset--;

        type = Type::Null;
        program.ir.Append({Opcode::PushNull});
    } else {
        type = ParseExpression(true);
    }

    if (RG_UNLIKELY(type != current_func->ret)) {
        MarkError("Cannot return %1 value (expected %2)",
                  TypeNames[(int)type], TypeNames[(int)current_func->ret]);
        return;
    }

    if (var_offset > 0) {
        Size pop_len = var_offset - 1;

        switch (type) {
            case Type::Null: { pop_len++; } break;
            case Type::Bool: { program.ir.Append({Opcode::StoreLocalBool, {.i = 0}}); } break;
            case Type::Int: { program.ir.Append({Opcode::StoreLocalInt, {.i = 0}}); } break;
            case Type::Double: { program.ir.Append({Opcode::StoreLocalDouble, {.i = 0}}); } break;
            case Type::String: { program.ir.Append({Opcode::StoreLocalString, {.i = 0}}); } break;
        }

        EmitPop(pop_len);
    }
    program.ir.Append({Opcode::Return, {.i = current_func->params.len}});

    ConsumeToken(TokenKind::NewLine);
}

void Parser::ParseLet()
{
    offset++;

    VariableInfo *var = variables.AppendDefault();
    var->name = ConsumeIdentifier();

    std::pair<VariableInfo **, bool> ret = variables_map.Append(var);
    if (RG_UNLIKELY(!ret.second)) {
        const VariableInfo *prev_var = *ret.first;

        if (current_func && prev_var->global) {
            MarkError("Declaration '%1' is not allowed to hide global variable", var->name);
        } else if (current_func && prev_var->offset < 0) {
            MarkError("Declaration '%1' is not allowed to hide parameter", var->name);
        } else {
            MarkError("Variable '%1' already exists", var->name);
        }
    }

    if (MatchToken(TokenKind::Equal)) {
        var->type = ParseExpression(true);
    } else {
        ConsumeToken(TokenKind::Colon);
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
                case Type::Null: { program.ir.Append({Opcode::PushNull}); } break;
                case Type::Bool: { program.ir.Append({Opcode::PushBool, {.b = false}}); } break;
                case Type::Int: { program.ir.Append({Opcode::PushInt, {.i = 0}}); } break;
                case Type::Double: { program.ir.Append({Opcode::PushDouble, {.d = 0.0}}); } break;
                case Type::String: { program.ir.Append({Opcode::PushString, {.str = ""}}); } break;
            }
        }
    }

    var->global = !current_func;
    var->offset = var_offset++;
    var->defined_at = program.ir.len;

    ConsumeToken(TokenKind::NewLine);
}

void Parser::ParseIf()
{
    offset++;

    if (RG_UNLIKELY(ParseExpression(true) != Type::Bool)) {
        MarkError("Cannot use non-Bool expression as condition");
        return;
    }

    Size branch_idx = program.ir.len;
    program.ir.Append({Opcode::BranchIfFalse});

    if (MatchToken(TokenKind::Do)) {
        ParseExpressionOrReturn();
        program.ir[branch_idx].u.i = program.ir.len - branch_idx;
    } else {
        ConsumeToken(TokenKind::NewLine);
        ParseBlock(false);

        if (MatchToken(TokenKind::Else)) {
            HeapArray<Size> jumps;

            jumps.Append(program.ir.len);
            program.ir.Append({Opcode::Jump});

            do {
                program.ir[branch_idx].u.i = program.ir.len - branch_idx;

                if (MatchToken(TokenKind::If)) {
                    if (RG_UNLIKELY(ParseExpression(true) != Type::Bool)) {
                        MarkError("Cannot use non-Bool expression as condition");
                        return;
                    }
                    ConsumeToken(TokenKind::NewLine);

                    branch_idx = program.ir.len;
                    program.ir.Append({Opcode::BranchIfFalse});

                    ParseBlock(false);

                    jumps.Append(program.ir.len);
                    program.ir.Append({Opcode::Jump});
                } else {
                    ConsumeToken(TokenKind::NewLine);

                    ParseBlock(false);
                    break;
                }
            } while (MatchToken(TokenKind::Else));

            for (Size jump_idx: jumps) {
                program.ir[jump_idx].u.i = program.ir.len - jump_idx;
            }
        } else {
            program.ir[branch_idx].u.i = program.ir.len - branch_idx;
        }

        ConsumeToken(TokenKind::End);
    }

    ConsumeToken(TokenKind::NewLine);
}

void Parser::ParseWhile()
{
    offset++;

    Size start_idx = program.ir.len;

    // Parse expression
    Size start_fix_forward = forward_calls.len;
    Size end_fix_forward;
    if (RG_UNLIKELY(ParseExpression(true) != Type::Bool)) {
        MarkError("Cannot use non-Bool expression as condition");
        return;
    }
    end_fix_forward = forward_calls.len;

    // Put expression IR aside, because we want to put it after loop body
    // to avoid an extra jump after each iteration.

    HeapArray<Instruction> expr;
    expr.Append(program.ir.Take(start_idx, program.ir.len - start_idx));
    program.ir.len -= expr.len;

    Size jump_idx = program.ir.len;
    program.ir.Append({Opcode::Jump});

    if (MatchToken(TokenKind::Do)) {
        ParseExpressionOrReturn();
    } else {
        ConsumeToken(TokenKind::NewLine);
        ParseBlock(false);
        ConsumeToken(TokenKind::End);
    }

    // We need to fix forward calls inside test expression because we move the instructions
    for (Size i = start_fix_forward; i < end_fix_forward; i++) {
        forward_calls[i].offset += program.ir.len - start_idx;
    }

    // Finally write down expression IR
    program.ir[jump_idx].u.i = program.ir.len - jump_idx;
    program.ir.Append(expr);
    program.ir.Append({Opcode::BranchIfTrue, {.i = jump_idx - program.ir.len + 1}});

    ConsumeToken(TokenKind::NewLine);
}

void Parser::ParseFor()
{
    offset++;

    VariableInfo *it = variables.AppendDefault();

    it->name = ConsumeIdentifier();
    it->type = Type::Int;
    it->offset = var_offset + 2;

    std::pair<VariableInfo **, bool> ret = variables_map.Append(it);
    if (RG_UNLIKELY(!ret.second)) {
        const VariableInfo *prev_var = *ret.first;

        if (current_func && prev_var->global) {
            MarkError("Iterator '%1' is not allowed to hide global variable", it->name);
        } else {
            MarkError("Variable '%1' already exists", it->name);
        }

        return;
    }

    Type type1;
    Type type2;
    bool inclusive;
    ConsumeToken(TokenKind::In);
    type1 = ParseExpression(true);
    if (MatchToken(TokenKind::DotDotDot)) {
        inclusive = false;
    } else {
        ConsumeToken(TokenKind::DotDot);
        inclusive = true;
    }
    type2 = ParseExpression(true);

    if (RG_UNLIKELY(type1 != Type::Int)) {
        MarkError("Start value must be Int, not %1", TypeNames[(int)type1]);
        return;
    }
    if (RG_UNLIKELY(type2 != Type::Int)) {
        MarkError("End value must be Int, not %1", TypeNames[(int)type2]);
        return;
    }

    // Make sure start and end value remain on the stack
    var_offset += 3;

    // Put iterator value on the stack
    program.ir.Append({Opcode::LoadLocalInt, {.i = it->offset - 2}});

    Size body_idx = program.ir.len;

    program.ir.Append({Opcode::LoadLocalInt, {.i = it->offset}});
    program.ir.Append({Opcode::LoadLocalInt, {.i = it->offset - 1}});
    program.ir.Append({inclusive ? Opcode::LessOrEqualInt : Opcode::LessThanInt});
    program.ir.Append({Opcode::BranchIfFalse, {.i = body_idx - program.ir.len}});

    if (MatchToken(TokenKind::Do)) {
        ParseExpressionOrReturn();
    } else {
        ConsumeToken(TokenKind::NewLine);
        ParseBlock(false);
        ConsumeToken(TokenKind::End);
    }

    program.ir.Append({Opcode::PushInt, {.i = 1}});
    program.ir.Append({Opcode::AddInt});
    program.ir.Append({Opcode::Jump, {.i = body_idx - program.ir.len}});
    program.ir[body_idx + 3].u.i = program.ir.len - (body_idx + 3);

    // Destroy iterator and range values
    EmitPop(3);
    DestroyVariables(1);
    var_offset -= 2;

    ConsumeToken(TokenKind::NewLine);
}

void Parser::ParseExpressionOrReturn()
{
    if (MatchToken(TokenKind::Return)) {
        offset--;
        ParseReturn();
        offset--;
    } else {
        ParseExpression(false);
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
    Size start_values_len = stack.len;
    RG_DEFER { stack.RemoveFrom(start_values_len); };

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
        } else if (tok.kind == TokenKind::Null || tok.kind == TokenKind::Bool ||
                   tok.kind == TokenKind::Integer || tok.kind == TokenKind::Double ||
                   tok.kind == TokenKind::String || tok.kind == TokenKind::Identifier) {
            if (RG_UNLIKELY(expect_op))
                goto unexpected_token;
            expect_op = true;

            switch (tok.kind) {
                case TokenKind::Null: {
                    program.ir.Append({Opcode::PushNull});
                    stack.Append({Type::Null});
                } break;
                case TokenKind::Bool: {
                    program.ir.Append({Opcode::PushBool, {.b = tok.u.b}});
                    stack.Append({Type::Bool});
                } break;
                case TokenKind::Integer: {
                    if (operators.len && operators[operators.len - 1].kind == TokenKind::Minus &&
                                         operators[operators.len - 1].unary) {
                        operators.RemoveLast(1);

                        program.ir.Append({Opcode::PushInt, {.i = -tok.u.i}});
                        stack.Append({Type::Int});
                    } else {
                        program.ir.Append({Opcode::PushInt, {.i = tok.u.i}});
                        stack.Append({Type::Int});
                    }
                } break;
                case TokenKind::Double: {
                    if (operators.len && operators[operators.len - 1].kind == TokenKind::Minus &&
                                         operators[operators.len - 1].unary) {
                        operators.RemoveLast(1);

                        program.ir.Append({Opcode::PushDouble, {.d = -tok.u.d}});
                        stack.Append({Type::Int});
                    } else {
                        program.ir.Append({Opcode::PushDouble, {.d = tok.u.d}});
                        stack.Append({Type::Double});
                    }
                } break;
                case TokenKind::String: {
                    program.ir.Append({Opcode::PushString, {.str = tok.u.str}});
                    stack.Append({Type::String});
                } break;

                case TokenKind::Identifier: {
                    if (MatchToken(TokenKind::LeftParenthesis)) {
                        if (RG_UNLIKELY(!ParseCall(tok.u.str)))
                            return {};
                    } else {
                        const VariableInfo *var = variables_map.FindValue(tok.u.str, nullptr);

                        if (RG_UNLIKELY(!var)) {
                            MarkError("Variable '%1' does not exist", tok.u.str);
                            return {};
                        }

                        if (var->global) {
                            if (RG_UNLIKELY(current_func &&
                                            current_func->earliest_forward_call < var->defined_at)) {
                                MarkError("Function '%1' was called before variable '%2' was defined",
                                          current_func->name, var->name);
                                return {};
                            }

                            switch (var->type) {
                                case Type::Null: { program.ir.Append({Opcode::PushNull}); } break;
                                case Type::Bool: { program.ir.Append({Opcode::LoadGlobalBool, {.i = var->offset}}); } break;
                                case Type::Int: { program.ir.Append({Opcode::LoadGlobalInt, {.i = var->offset}}); } break;
                                case Type::Double: { program.ir.Append({Opcode::LoadGlobalDouble, {.i = var->offset}});} break;
                                case Type::String: { program.ir.Append({Opcode::LoadGlobalString, {.i = var->offset}}); } break;
                            }
                        } else {
                            switch (var->type) {
                                case Type::Null: { program.ir.Append({Opcode::PushNull}); } break;
                                case Type::Bool: { program.ir.Append({Opcode::LoadLocalBool, {.i = var->offset}}); } break;
                                case Type::Int: { program.ir.Append({Opcode::LoadLocalInt, {.i = var->offset}}); } break;
                                case Type::Double: { program.ir.Append({Opcode::LoadLocalDouble, {.i = var->offset}});} break;
                                case Type::String: { program.ir.Append({Opcode::LoadLocalString, {.i = var->offset}}); } break;
                            }
                        }
                        stack.Append({var->type, var});
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
                    if (offset > tokens.len) {
                        MarkError("Unexpected end of file, expected expression");
                    } else {
                        MarkError("Unexpected token '%1', expected expression",
                                  TokenKindNames[(int)tokens[offset - 1].kind]);
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
                // Remove useless load instruction. We don't remove the variable from
                // stack slots,  because it will be needed when we emit the store instruction
                // and will be removed then.
                program.ir.RemoveLast(1);
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

    RG_ASSERT(stack.len == start_values_len + 1);
    if (keep_result) {
        return stack[stack.len - 1].type;
    } else {
        if (program.ir.len >= 2 && program.ir[program.ir.len - 2].code == Opcode::Duplicate) {
            std::swap(program.ir[program.ir.len - 2], program.ir[program.ir.len - 1]);
            program.ir.len--;
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

// Don't try to call from outside ParseExpression()!
bool Parser::ParseCall(const char *name)
{
    if (TestStr(name, "print") || TestStr(name, "println")) {
        Size pop = -1;

        if (!MatchToken(TokenKind::RightParenthesis)) {
            Type type = ParseExpression(true);
            program.ir.Append({Opcode::Print, {.type = type}});
            pop++;

            while (MatchToken(TokenKind::Comma)) {
                Type type = ParseExpression(true);
                program.ir.Append({Opcode::Print, {.type = type}});
                pop++;
            }

            ConsumeToken(TokenKind::RightParenthesis);
        }

        if (TestStr(name, "println")) {
            program.ir.Append({Opcode::PushString, {.str = "\n"}});
            program.ir.Append({Opcode::Print, {.type = Type::String}});
            pop++;
        }

        if (pop < 0) {
            program.ir.Append({Opcode::PushNull});
        } else {
            EmitPop(pop);
        }

        stack.Append({Type::Null});
    } else {
        LocalArray<Type, RG_LEN(FunctionInfo::params.data)> types;

        FunctionInfo *func = functions_map.FindValue(name, nullptr);
        if (RG_UNLIKELY(!func)) {
            MarkError("Function '%1' does not exist", name);
            return false;
        }

        if (!MatchToken(TokenKind::RightParenthesis)) {
            types.Append(ParseExpression(true));
            while (MatchToken(TokenKind::Comma)) {
                if (RG_UNLIKELY(!types.Available())) {
                    MarkError("Functions cannot take more than %1 arguments", RG_LEN(types.data));
                    return false;
                }
                types.Append(ParseExpression(true));
            }

            ConsumeToken(TokenKind::RightParenthesis);
        }

        bool mismatch = false;
        if (RG_UNLIKELY(types.len != func->params.len)) {
            MarkError("Function '%1' expects %2 arguments, not %3", func->name,
                      func->params.len, types.len);
            return false;
        }
        for (Size i = 0; i < types.len; i++) {
            if (RG_UNLIKELY(types[i] != func->params[i].type)) {
                MarkError("Function '%1' expects %2 as %3 argument, not %4",
                          func->name, TypeNames[(int)func->params[i].type], i + 1,
                          TypeNames[(int)types[i]]);
                mismatch = true;
            }
        }
        if (RG_UNLIKELY(mismatch))
            return false;

        if (func->addr < 0) {
            forward_calls.Append({program.ir.len, func});
            func->earliest_forward_call = std::min(func->earliest_forward_call, program.ir.len);
        }
        program.ir.Append({Opcode::Call, {.i = func->addr}});

        stack.Append({func->ret});
    }

    return true;
}

void Parser::ProduceOperator(const PendingOperator &op)
{
    bool success;

    switch (op.kind) {
        case TokenKind::Reassign: {
            const StackSlot &slot1 = stack[stack.len - 2];
            const StackSlot &slot2 = stack[stack.len - 1];

            if (RG_UNLIKELY(!slot1.var)) {
                MarkError("Cannot assign expression to rvalue");
                return;
            }
            if (RG_UNLIKELY(slot1.type != slot2.type)) {
                MarkError("Cannot assign %1 value to %2 variable",
                          TypeNames[(int)slot2.type], TypeNames[(int)slot1.type]);
                return;
            }

            program.ir.Append({Opcode::Duplicate});
            if (slot1.var->global) {
                switch (slot1.type) {
                    case Type::Null: { EmitPop(1); } break;
                    case Type::Bool: { program.ir.Append({Opcode::StoreGlobalBool, {.i = slot1.var->offset}}); } break;
                    case Type::Int: { program.ir.Append({Opcode::StoreGlobalInt, {.i = slot1.var->offset}}); } break;
                    case Type::Double: { program.ir.Append({Opcode::StoreGlobalDouble, {.i = slot1.var->offset}}); } break;
                    case Type::String: { program.ir.Append({Opcode::StoreGlobalString, {.i = slot1.var->offset}}); } break;
                }
            } else {
                switch (slot1.type) {
                    case Type::Null: { EmitPop(1); } break;
                    case Type::Bool: { program.ir.Append({Opcode::StoreLocalBool, {.i = slot1.var->offset}}); } break;
                    case Type::Int: { program.ir.Append({Opcode::StoreLocalInt, {.i = slot1.var->offset}}); } break;
                    case Type::Double: { program.ir.Append({Opcode::StoreLocalDouble, {.i = slot1.var->offset}}); } break;
                    case Type::String: { program.ir.Append({Opcode::StoreLocalString, {.i = slot1.var->offset}}); } break;
                }
            }

            std::swap(stack[stack.len - 1], stack[stack.len - 2]);
            stack.len--;

            return;
        } break;

        case TokenKind::Plus: {
            success = EmitOperator2(Type::Int, Opcode::AddInt, Type::Int) ||
                      EmitOperator2(Type::Double, Opcode::AddDouble, Type::Double);
        } break;
        case TokenKind::Minus: {
            if (op.unary) {
                success = EmitOperator1(Type::Int, Opcode::NegateInt, Type::Int) ||
                          EmitOperator1(Type::Double, Opcode::NegateDouble, Type::Double);
            } else {
                success = EmitOperator2(Type::Int, Opcode::SubstractInt, Type::Int) ||
                          EmitOperator2(Type::Double, Opcode::SubstractDouble, Type::Double);
            }
        } break;
        case TokenKind::Multiply: {
            success = EmitOperator2(Type::Int, Opcode::MultiplyInt, Type::Int) ||
                      EmitOperator2(Type::Double, Opcode::MultiplyDouble, Type::Double);
        } break;
        case TokenKind::Divide: {
            success = EmitOperator2(Type::Int, Opcode::DivideInt, Type::Int) ||
                      EmitOperator2(Type::Double, Opcode::DivideDouble, Type::Double);
        } break;
        case TokenKind::Modulo: {
            success = EmitOperator2(Type::Int, Opcode::ModuloInt, Type::Int);
        } break;

        case TokenKind::Equal: {
            success = EmitOperator2(Type::Int, Opcode::EqualInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::EqualDouble, Type::Bool) ||
                      EmitOperator2(Type::Bool, Opcode::EqualBool, Type::Bool);
        } break;
        case TokenKind::NotEqual: {
            success = EmitOperator2(Type::Int, Opcode::NotEqualInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::NotEqualDouble, Type::Bool) ||
                      EmitOperator2(Type::Bool, Opcode::NotEqualBool, Type::Bool);
        } break;
        case TokenKind::Greater: {
            success = EmitOperator2(Type::Int, Opcode::GreaterThanInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::GreaterThanDouble, Type::Bool);
        } break;
        case TokenKind::GreaterOrEqual: {
            success = EmitOperator2(Type::Int, Opcode::GreaterOrEqualInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::GreaterOrEqualDouble, Type::Bool);
        } break;
        case TokenKind::Less: {
            success = EmitOperator2(Type::Int, Opcode::LessThanInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::LessThanDouble, Type::Bool);
        } break;
        case TokenKind::LessOrEqual: {
            success = EmitOperator2(Type::Int, Opcode::LessOrEqualInt, Type::Bool) ||
                      EmitOperator2(Type::Double, Opcode::LessOrEqualDouble, Type::Bool);
        } break;

        case TokenKind::And: {
            success = EmitOperator2(Type::Int, Opcode::AndInt, Type::Int) ||
                      EmitOperator2(Type::Bool, Opcode::AndBool, Type::Bool);
        } break;
        case TokenKind::Or: {
            success = EmitOperator2(Type::Int, Opcode::OrInt, Type::Int) ||
                      EmitOperator2(Type::Bool, Opcode::OrBool, Type::Bool);
        } break;
        case TokenKind::Xor: {
            success = EmitOperator2(Type::Int, Opcode::XorInt, Type::Int) ||
                      EmitOperator2(Type::Bool, Opcode::NotEqualBool, Type::Bool);
        } break;
        case TokenKind::Not: {
            success = EmitOperator1(Type::Int, Opcode::NotInt, Type::Int) ||
                      EmitOperator1(Type::Bool, Opcode::NotBool, Type::Bool);
        } break;
        case TokenKind::LeftShift: {
            success = EmitOperator2(Type::Int, Opcode::LeftShiftInt, Type::Int);
        } break;
        case TokenKind::RightShift: {
            success = EmitOperator2(Type::Int, Opcode::RightShiftInt, Type::Int);
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
                      TokenKindNames[(int)op.kind], TypeNames[(int)stack[stack.len - 1].type]);
        } else if (stack[stack.len - 2].type == stack[stack.len - 1].type) {
            MarkError("Cannot use '%1' operator on %2 stack",
                      TokenKindNames[(int)op.kind], TypeNames[(int)stack[stack.len - 2].type]);
        } else {
            MarkError("Cannot use '%1' operator on %2 and %3 stack",
                      TokenKindNames[(int)op.kind], TypeNames[(int)stack[stack.len - 2].type],
                      TypeNames[(int)stack[stack.len - 1].type]);
        }
    }
}

bool Parser::EmitOperator1(Type in_type, Opcode code, Type out_type)
{
    Type type = stack[stack.len - 1].type;

    if (type == in_type) {
        program.ir.Append({code});
        stack[stack.len - 1] = {out_type};

        return true;
    } else {
        return false;
    }
}

bool Parser::EmitOperator2(Type in_type, Opcode code, Type out_type)
{
    Type type1 = stack[stack.len - 2].type;
    Type type2 = stack[stack.len - 1].type;

    if (type1 == in_type && type2 == in_type) {
        program.ir.Append({code});
        stack[--stack.len - 1] = {out_type};

        return true;
    } else {
        return false;
    }
}

void Parser::EmitPop(int64_t count)
{
    RG_ASSERT(count >= 0);

    if (count) {
        program.ir.Append({Opcode::Pop, {.i = count}});
    }
}

void Parser::Finish(Program *out_program)
{
    RG_ASSERT(!out_program->ir.len);

    program.ir.Append({Opcode::PushInt, {.i = 0}});
    program.ir.Append({Opcode::Exit});

    for (const VariableInfo &var: variables) {
        VariableInfo *global = program.globals.Append(var);
        program.globals_map.Append(global);
    }

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

void Parser::DestroyVariables(Size count)
{
    for (Size i = variables.len - count; i < variables.len; i++) {
        variables_map.Remove(variables[i].name);
    }
    variables.RemoveLast(count);

    var_offset -= count;
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
