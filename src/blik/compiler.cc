// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "compiler.hh"
#include "debug.hh"
#include "lexer.hh"
#include "types.hh"

namespace RG {

struct ForwardCall {
    Size offset;
    const FunctionInfo *func;
};

struct PendingOperator {
    TokenKind kind;
    int prec;
    bool unary;

    Size pos; // For error messages
    Size branch_idx; // Used for short-circuit operators
};

struct StackSlot {
    Type type;
    const VariableInfo *var;
};

class Compiler {
    bool valid = true;
    bool valid_stmt;
    bool show_hints;

    const char *filename;
    Span<const char> code;
    Span<const Token> tokens;
    Size pos;

    BucketArray<FunctionInfo> functions;
    HashTable<const char *, FunctionInfo *> functions_map;
    HashMap<Size, FunctionInfo *> functions_by_pos; // Clear for each Parse call
    BucketArray<VariableInfo> variables;
    HashTable<const char *, VariableInfo *> variables_map;

    FunctionInfo *current_func = nullptr;
    Size depth = -1;
    Size var_offset = 0;

    Size loop_var_offset = -1;
    HeapArray<Size> loop_breaks;
    HeapArray<Size> loop_continues;

    // Only used (and valid) while parsing expression
    HeapArray<StackSlot> stack;

    HeapArray<ForwardCall> forward_calls;

    DebugInfo debug;
    bool generate_debug;

    Program program;

public:
    Compiler(bool generate_debug);

    bool Parse(const TokenSet &set, const char *filename);
    void Finish(Program *out_program, DebugInfo *out_debug = nullptr);

private:
    void ParsePrototypes(Span<const Size> funcs);

    bool ParseBlock(bool keep_variables);
    void ParseFunction();
    void ParseReturn();
    void ParseLet();
    void ParseIf();
    void ParseWhile();
    void ParseFor();
    void ParseBreak();
    void ParseContinue();

    bool ParseDo();

    Type ParseExpression(bool keep_result);
    void ProduceOperator(const PendingOperator &op);
    bool EmitOperator1(Type in_type, Opcode code, Type out_type);
    bool EmitOperator2(Type in_type, Opcode code, Type out_type);

    bool ParseCall(const char *name);
    void EmitIntrinsic(const char *name, Span<const Type> types);

    void EmitPop(int64_t count);
    void EmitInstruction(Instruction inst);

    bool TestOverload(const FunctionInfo &proto, Span<const Type> types);

    bool ConsumeToken(TokenKind kind);
    const char *ConsumeIdentifier();
    Type ConsumeType();
    bool MatchToken(TokenKind kind);
    bool PeekToken(TokenKind kind);

    void DestroyVariables(Size count);

    template <typename... Args>
    void MarkError(Size pos, const char *fmt, Args... args)
    {
        if (valid_stmt) {
            Size offset = (pos < tokens.len) ? tokens[pos].offset : code.len;
            int line = tokens[std::min(pos, tokens.len - 1)].line;

            ReportDiagnostic(DiagnosticType::Error, code, filename, line, offset, fmt, args...);

            valid = false;
            valid_stmt = false;
            show_hints = true;
        } else {
            show_hints = false;
        }
    }
    template <typename... Args>
    void HintError(Size pos, const char *fmt, Args... args)
    {
        if (show_hints) {
            Size offset = (pos < tokens.len) ? tokens[pos].offset : code.len;
            int line = tokens[std::min(pos, tokens.len - 1)].line;

            ReportDiagnostic(DiagnosticType::ErrorHint, code, filename, line, offset, fmt, args...);
        }
    }
};

Compiler::Compiler(bool generate_debug)
    : generate_debug(generate_debug)
{
    functions.Append({.name = "print", .signature = "print(...)", .variadic = true, .ret = Type::Null});
    functions.Append({.name = "printLn", .signature = "printLn(...)", .variadic = true, .ret = Type::Null});
    functions.Append({.name = "intToFloat", .signature = "intToFloat(Int): Float", .params = {{"i", Type::Int}}, .ret = Type::Float});
    functions.Append({.name = "floatToInt", .signature = "floatToInt(Float): Int", .params = {{"f", Type::Float}}, .ret = Type::Int});
    functions.Append({.name = "exit", .signature = "exit(Int)", .params = {{"code", Type::Int}}, .ret = Type::Null});

    for (FunctionInfo &intr: functions) {
        intr.intrinsic = true;
        functions_map.Append(&intr);
    }
}

bool Compiler::Parse(const TokenSet &set, const char *filename)
{
    RG_ASSERT(valid);

    this->filename = filename;
    this->code = set.code;
    tokens = set.tokens;
    pos = 0;

    valid_stmt = true;
    show_hints = false;

    if (generate_debug) {
        SourceInfo src = {};

        src.filename = DuplicateString(filename, &debug.str_alloc).ptr;
        src.first_idx = program.ir.len;
        src.line_idx = debug.lines.len;

        debug.sources.Append(src);
        debug.lines.Append(program.ir.len);
    }

    // We want top-level order-independent functions
    functions_by_pos.Clear();
    ParsePrototypes(set.funcs);

    // Do the actual parsing!
    ParseBlock(true);
    if (RG_UNLIKELY(valid && pos < tokens.len)) {
        MarkError(pos, "Unexpected token '%1' without matching block", TokenKindNames[(int)tokens[pos].kind]);
        return false;
    }

    // Maybe it'll help catch bugs
    RG_ASSERT(depth == -1);
    RG_ASSERT(loop_var_offset == -1);
    RG_ASSERT(!current_func);

    // Fix up foward calls
    for (const ForwardCall &call: forward_calls) {
        program.ir[call.offset].u.i = call.func->inst_idx;
    }
    forward_calls.Clear();

    return valid;
}

void Compiler::ParsePrototypes(Span<const Size> funcs)
{
    RG_ASSERT(!functions_by_pos.table.count);

    RG_DEFER_C(prev_debug = generate_debug,
               prev_offset = pos) {
        generate_debug = prev_debug;
        pos = prev_offset;
        valid_stmt = true;
    };

    // This is preliminary, it doesn't really count :)
    generate_debug = false;
    valid_stmt = false;

    for (Size i = 0; i < funcs.len; i++) {
        pos = funcs[i] + 1;

        FunctionInfo *proto = functions.AppendDefault();
        functions_by_pos.Append(pos, proto);

        proto->defined_pos = pos;
        proto->name = ConsumeIdentifier();

        // Insert in functions map
        {
            std::pair<FunctionInfo **, bool> ret = functions_map.Append(proto);
            FunctionInfo *proto0 = *ret.first;

            if (ret.second) {
                proto0->overload_prev = proto0;
                proto0->overload_next = proto0;
            } else {
                proto0->overload_prev->overload_next = proto;
                proto->overload_next = proto0;
                proto->overload_prev = proto0->overload_prev;
                proto0->overload_prev = proto;
            }
        }

        // Parameters
        ConsumeToken(TokenKind::LeftParenthesis);
        if (!MatchToken(TokenKind::RightParenthesis)) {
            do {
                MatchToken(TokenKind::EndOfLine);

                FunctionInfo::Parameter param = {};

                MatchToken(TokenKind::Mut);
                param.name = ConsumeIdentifier();
                ConsumeToken(TokenKind::Colon);
                param.type = ConsumeType();

                // We'll show an error in ParseFunction()
                if (RG_LIKELY(proto->params.Available())) {
                    proto->params.Append(param);
                }

                proto->ret_pop += (param.type != Type::Null);
            } while (MatchToken(TokenKind::Comma));

            MatchToken(TokenKind::EndOfLine);
            ConsumeToken(TokenKind::RightParenthesis);
        }

        // Return type
        if (MatchToken(TokenKind::Colon)) {
            proto->ret = ConsumeType();
        } else {
            proto->ret = Type::Null;
        }
        proto->ret_pop -= (proto->ret == Type::Null);

        // Build signature (with parameter and return types)
        {
            HeapArray<char> buf(&program.str_alloc);

            Fmt(&buf, "%1(", proto->name);
            for (Size i = 0; i < proto->params.len; i++) {
                const FunctionInfo::Parameter &param = proto->params[i];
                Fmt(&buf, "%1%2", i ? ", " : "", TypeNames[(int)param.type]);
            }
            Fmt(&buf, ")");
            if (proto->ret != Type::Null) {
                Fmt(&buf, ": %1", TypeNames[(int)proto->ret]);
            }

            proto->signature = buf.TrimAndLeak(1).ptr;
        }

        // We don't know where it will live yet!
        proto->inst_idx = -1;

        proto->earliest_call_pos = RG_SIZE_MAX;
        proto->earliest_call_idx = RG_SIZE_MAX;
    }
}

bool Compiler::ParseBlock(bool keep_variables)
{
    depth++;

    RG_DEFER_C(prev_offset = var_offset,
               variables_len = variables.len) {
        depth--;

        if (!keep_variables) {
            EmitPop(var_offset - prev_offset);
            DestroyVariables(variables.len - variables_len);
            var_offset = prev_offset;
        }
    };

    bool has_return = false;

    while (pos < tokens.len) {
        switch (tokens[pos].kind) {
            case TokenKind::EndOfLine: {
                if (generate_debug) {
                    debug.lines.Append(program.ir.len);
                }
                pos++;
            } break;

            case TokenKind::End:
            case TokenKind::Else: { return has_return; } break;

            case TokenKind::Begin: {
                pos++;

                valid_stmt |= ConsumeToken(TokenKind::EndOfLine);
                has_return |= ParseBlock(false);
                ConsumeToken(TokenKind::End);
                valid_stmt |= ConsumeToken(TokenKind::EndOfLine);
            } break;

            case TokenKind::Func: {
                Size jump_idx = program.ir.len;
                program.ir.Append({Opcode::Jump});

                ParseFunction();
                valid_stmt |= ConsumeToken(TokenKind::EndOfLine);

                program.ir[jump_idx].u.i = program.ir.len - jump_idx;
            } break;

            case TokenKind::Return: {
                ParseReturn();
                valid_stmt |= ConsumeToken(TokenKind::EndOfLine);
                has_return = true;
            } break;

            case TokenKind::Let: {
                ParseLet();
                valid_stmt |= ConsumeToken(TokenKind::EndOfLine);
            } break;
            case TokenKind::If: {
                ParseIf();
                valid_stmt |= ConsumeToken(TokenKind::EndOfLine);
            } break;
            case TokenKind::While: {
                ParseWhile();
                valid_stmt |= ConsumeToken(TokenKind::EndOfLine);
            } break;
            case TokenKind::For: {
                ParseFor();
                valid_stmt |= ConsumeToken(TokenKind::EndOfLine);
            } break;

            case TokenKind::Break: {
                ParseBreak();
                valid_stmt |= ConsumeToken(TokenKind::EndOfLine);
            } break;
            case TokenKind::Continue: {
                ParseContinue();
                valid_stmt |= ConsumeToken(TokenKind::EndOfLine);
            } break;

            default: {
                ParseExpression(false);
                valid_stmt |= ConsumeToken(TokenKind::EndOfLine);
            } break;
        }
    }

    return has_return;
}

void Compiler::ParseFunction()
{
    Size func_pos = ++pos;

    FunctionInfo *func = functions_by_pos.FindValue(func_pos, nullptr);
    RG_ASSERT(func);

    RG_DEFER_C(prev_func = current_func,
               prev_offset = var_offset) {
        // Variables inside the function are destroyed at the end of the block.
        // This destroys the parameters.
        DestroyVariables(func->params.len);
        var_offset = prev_offset;

        current_func = prev_func;
    };

    ConsumeIdentifier();

    if (RG_UNLIKELY(current_func)) {
        MarkError(func_pos, "Nested functions are not supported");
        HintError(current_func->defined_pos, "Current function was started here and is still open");
    } else if (RG_UNLIKELY(depth)) {
        MarkError(func_pos, "Functions must be defined in top-level scope");
    } else {
        current_func = func;
    }

    // Parameters
    HeapArray<Type> types;
    ConsumeToken(TokenKind::LeftParenthesis);
    if (!MatchToken(TokenKind::RightParenthesis)) {
        Size stack_offset = -2 - func->params.len;

        do {
            MatchToken(TokenKind::EndOfLine);

            VariableInfo *var = variables.AppendDefault();

            var->readonly = !MatchToken(TokenKind::Mut);
            var->defined_pos = pos;
            var->name = ConsumeIdentifier();

            if (stack_offset >= -2) {
                MarkError(pos - 1, "Functions cannot have more than %1 parameters", RG_LEN(func->params.data));
            }
            var->offset = stack_offset++;

            std::pair<VariableInfo **, bool> ret = variables_map.Append(var);
            if (RG_UNLIKELY(!ret.second)) {
               const VariableInfo *prev_var = *ret.first;
               var->shadow = prev_var;

                if (prev_var->global) {
                    MarkError(pos - 1, "Parameter '%1' is not allowed to hide global variable", var->name);
                    HintError(prev_var->defined_pos, "Global variable '%1' is defined here", prev_var->name);
                } else {
                    MarkError(pos - 1, "Parameter '%1' already exists", var->name);
                }
            }

            ConsumeToken(TokenKind::Colon);
            var->type = ConsumeType();
            types.Append(var->type);

            var->poisoned = !valid_stmt;
        } while (MatchToken(TokenKind::Comma));

        MatchToken(TokenKind::EndOfLine);
        ConsumeToken(TokenKind::RightParenthesis);
    }

    // Return type
    if (MatchToken(TokenKind::Colon)) {
        ConsumeType();
    }

    // Check for incompatible function overloadings
    {
        FunctionInfo *overload = functions_map.FindValue(func->name, nullptr);

        while (overload != func) {
            if (RG_UNLIKELY(overload->intrinsic)) {
                MarkError(func_pos, "Cannot replace or overload intrinsic function '%1'", func->name);
            }

            if (TestOverload(*overload, types)) {
                if (overload->ret == func->ret) {
                    MarkError(func_pos, "Function '%1' is already defined", func->signature);
                } else {
                    MarkError(func_pos, "Function '%1' only differs from previously defined '%2' by return type",
                              func->signature, overload->signature);
                }
                HintError(overload->defined_pos, "Previous definition here");
            }

            overload = overload->overload_next;
        }
    }

    func->inst_idx = program.ir.len;
    var_offset = 0;

    // Function body
    bool has_return;
    if (PeekToken(TokenKind::Do)) {
        has_return = ParseDo();
    } else {
        ConsumeToken(TokenKind::EndOfLine);
        has_return = ParseBlock(false);
        ConsumeToken(TokenKind::End);
    }

    if (!has_return) {
        if (func->ret == Type::Null) {
            program.ir.Append({Opcode::ReturnNull, {.i = func->params.len}});
        } else {
            MarkError(func_pos, "Function '%1' does not have a return statement", func->name);
        }
    }
}

void Compiler::ParseReturn()
{
    Size return_pos = ++pos;

    if (RG_UNLIKELY(!current_func)) {
        MarkError(pos - 1, "Return statement cannot be used outside function");
        return;
    }

    Type type;
    if (PeekToken(TokenKind::EndOfLine)) {
        type = Type::Null;
    } else {
        type = ParseExpression(true);
    }

    if (RG_UNLIKELY(type != current_func->ret)) {
        MarkError(return_pos, "Cannot return %1 value in function defined to return %2",
                  TypeNames[(int)type], TypeNames[(int)current_func->ret]);
        return;
    }

    if (var_offset > 0) {
        Size pop = var_offset - 1;

        switch (type) {
            case Type::Null: { pop++; } break;
            case Type::Bool: { program.ir.Append({Opcode::StoreLocalBool, {.i = 0}}); } break;
            case Type::Int: { program.ir.Append({Opcode::StoreLocalInt, {.i = 0}}); } break;
            case Type::Float: { program.ir.Append({Opcode::StoreLocalFloat, {.i = 0}}); } break;
            case Type::String: { program.ir.Append({Opcode::StoreLocalString, {.i = 0}}); } break;
        }

        EmitPop(pop);
    }
    program.ir.Append({type == Type::Null ? Opcode::ReturnNull : Opcode::Return, {.i = current_func->ret_pop}});
}

void Compiler::ParseLet()
{
    Size var_pos = ++pos;

    VariableInfo *var = variables.AppendDefault();

    var->readonly = !MatchToken(TokenKind::Mut);
    var->defined_pos = pos;
    var->name = ConsumeIdentifier();

    std::pair<VariableInfo **, bool> ret = variables_map.Append(var);
    if (RG_UNLIKELY(!ret.second)) {
        const VariableInfo *prev_var = *ret.first;
        var->shadow = prev_var;

        if (current_func && prev_var->global) {
            MarkError(var_pos, "Declaration '%1' is not allowed to hide global variable", var->name);
            HintError(prev_var->defined_pos, "Global variable '%1' is defined here", prev_var->name);
        } else if (current_func && prev_var->offset < 0) {
            MarkError(var_pos, "Declaration '%1' is not allowed to hide parameter", var->name);
            HintError(prev_var->defined_pos, "Parameter '%1' is defined here", prev_var->name);
        } else {
            MarkError(var_pos, "Variable '%1' already exists", var->name);
            HintError(prev_var->defined_pos, "Previous variable '%1' is defined here", prev_var->name);
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
                MarkError(var_pos + 3, "Cannot assign %1 value to %2 variable",
                          TypeNames[(int)type2], TypeNames[(int)var->type]);
            }
        } else {
            switch (var->type) {
                case Type::Null: {} break;
                case Type::Bool: { program.ir.Append({Opcode::PushBool, {.b = false}}); } break;
                case Type::Int: { program.ir.Append({Opcode::PushInt, {.i = 0}}); } break;
                case Type::Float: { program.ir.Append({Opcode::PushFloat, {.d = 0.0}}); } break;
                case Type::String: { program.ir.Append({Opcode::PushString, {.str = ""}}); } break;
            }
        }
    }

    var->global = !current_func;
    var->offset = var_offset;
    var->defined_idx = program.ir.len;

    // Null values don't actually exist
    var_offset += (var->type != Type::Null);

    // Expressions involving this variable won't issue (visible) errors
    // and will be marked as invalid too.
    var->poisoned = !valid_stmt;
}

void Compiler::ParseIf()
{
    Size if_pos = ++pos;

    if (RG_UNLIKELY(ParseExpression(true) != Type::Bool)) {
        MarkError(if_pos, "Cannot use non-Bool expression as condition");
        return;
    }

    Size branch_idx = program.ir.len;
    program.ir.Append({Opcode::BranchIfFalse});

    if (PeekToken(TokenKind::Do)) {
        ParseDo();
        program.ir[branch_idx].u.i = program.ir.len - branch_idx;
    } else {
        ConsumeToken(TokenKind::EndOfLine);
        ParseBlock(false);

        if (MatchToken(TokenKind::Else)) {
            HeapArray<Size> jumps;

            jumps.Append(program.ir.len);
            program.ir.Append({Opcode::Jump});

            do {
                program.ir[branch_idx].u.i = program.ir.len - branch_idx;

                if (MatchToken(TokenKind::If)) {
                    Size elseif_pos = pos;

                    if (RG_UNLIKELY(ParseExpression(true) != Type::Bool)) {
                        MarkError(elseif_pos, "Cannot use non-Bool expression as condition");
                        return;
                    }
                    ConsumeToken(TokenKind::EndOfLine);

                    branch_idx = program.ir.len;
                    program.ir.Append({Opcode::BranchIfFalse});

                    ParseBlock(false);

                    jumps.Append(program.ir.len);
                    program.ir.Append({Opcode::Jump});
                } else {
                    ConsumeToken(TokenKind::EndOfLine);

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
}

void Compiler::ParseWhile()
{
    Size while_pos = ++pos;
    Size start_idx = program.ir.len;

    // Parse expression
    Size start_fix_forward = forward_calls.len;
    Size end_fix_forward;
    if (RG_UNLIKELY(ParseExpression(true) != Type::Bool)) {
        MarkError(while_pos, "Cannot use non-Bool expression as condition");
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

    // Break and continue need to apply to while loop blocks
    Size first_break_idx = loop_breaks.len;
    Size first_continue_idx = loop_continues.len;
    RG_DEFER_C(prev_offset = loop_var_offset) {
        loop_breaks.RemoveFrom(first_break_idx);
        loop_continues.RemoveFrom(first_continue_idx);
        loop_var_offset = prev_offset;
    };
    loop_var_offset = var_offset;

    // Parse body
    if (PeekToken(TokenKind::Do)) {
        ParseDo();
    } else {
        ConsumeToken(TokenKind::EndOfLine);
        ParseBlock(false);
        ConsumeToken(TokenKind::End);
    }

    // We need to fix forward calls inside test expression because we move the instructions
    for (Size i = start_fix_forward; i < end_fix_forward; i++) {
        forward_calls[i].offset += program.ir.len - start_idx;
    }

    // Fix up continue jumps
    for (Size i = first_continue_idx; i < loop_continues.len; i++) {
        Size jump_idx = loop_continues[i];
        program.ir[jump_idx].u.i = program.ir.len - jump_idx;
    }

    // Finally write down expression IR
    program.ir[jump_idx].u.i = program.ir.len - jump_idx;
    program.ir.Append(expr);
    program.ir.Append({Opcode::BranchIfTrue, {.i = jump_idx - program.ir.len + 1}});

    // Fix up break jumps
    for (Size i = first_break_idx; i < loop_breaks.len; i++) {
        Size jump_idx = loop_breaks[i];
        program.ir[jump_idx].u.i = program.ir.len - jump_idx;
    }
}

void Compiler::ParseFor()
{
    Size for_pos = ++pos;

    VariableInfo *it = variables.AppendDefault();

    it->readonly = !MatchToken(TokenKind::Mut);
    it->defined_pos = pos;
    it->name = ConsumeIdentifier();
    it->type = Type::Int;

    it->offset = var_offset + 2;

    std::pair<VariableInfo **, bool> ret = variables_map.Append(it);
    if (RG_UNLIKELY(!ret.second)) {
        const VariableInfo *prev_var = *ret.first;
        it->shadow = prev_var;

        if (current_func && prev_var->global) {
            MarkError(for_pos, "Iterator '%1' is not allowed to hide global variable", it->name);
            HintError(prev_var->defined_pos, "Global variable '%1' is defined here", prev_var->name);
        } else {
            MarkError(for_pos, "Variable '%1' already exists", it->name);
            HintError(prev_var->defined_pos, "Previous variable '%1' is defined here", prev_var->name);
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
        MarkError(for_pos + 3, "Start value must be Int, not %1", TypeNames[(int)type1]);
        return;
    }
    if (RG_UNLIKELY(type2 != Type::Int)) {
        MarkError(for_pos + 3, "End value must be Int, not %1", TypeNames[(int)type2]);
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

    // Break and continue need to apply to for loop blocks
    Size first_break_idx = loop_breaks.len;
    Size first_continue_idx = loop_continues.len;
    RG_DEFER_C(prev_offset = loop_var_offset) {
        loop_breaks.RemoveFrom(first_break_idx);
        loop_continues.RemoveFrom(first_continue_idx);
        loop_var_offset = prev_offset;
    };
    loop_var_offset = var_offset;

    // Parse body
    if (PeekToken(TokenKind::Do)) {
        ParseDo();
    } else {
        ConsumeToken(TokenKind::EndOfLine);
        ParseBlock(false);
        ConsumeToken(TokenKind::End);
    }

    // Fix up continue jumps
    for (Size i = first_continue_idx; i < loop_continues.len; i++) {
        Size jump_idx = loop_continues[i];
        program.ir[jump_idx].u.i = program.ir.len - jump_idx;
    }

    program.ir.Append({Opcode::PushInt, {.i = 1}});
    program.ir.Append({Opcode::AddInt});
    program.ir.Append({Opcode::Jump, {.i = body_idx - program.ir.len}});
    program.ir[body_idx + 3].u.i = program.ir.len - (body_idx + 3);

    // Fix up break jumps
    for (Size i = first_break_idx; i < loop_breaks.len; i++) {
        Size jump_idx = loop_breaks[i];
        program.ir[jump_idx].u.i = program.ir.len - jump_idx;
    }

    // Destroy iterator and range values
    EmitPop(3);
    DestroyVariables(1);
    var_offset -= 3;
}

void Compiler::ParseBreak()
{
    Size break_pos = pos++;

    if (loop_var_offset < 0) {
        MarkError(break_pos, "Break statement outside of loop");
        return;
    }

    EmitPop(var_offset - loop_var_offset);

    loop_breaks.Append(program.ir.len);
    program.ir.Append({Opcode::Jump});
}

void Compiler::ParseContinue()
{
    Size continue_pos = pos++;

    if (loop_var_offset < 0) {
        MarkError(continue_pos, "Continue statement outside of loop");
        return;
    }

    EmitPop(var_offset - loop_var_offset);

    loop_continues.Append(program.ir.len);
    program.ir.Append({Opcode::Jump});
}

bool Compiler::ParseDo()
{
    pos++;

    if (PeekToken(TokenKind::Return)) {
        ParseReturn();
        return true;
    } else if (PeekToken(TokenKind::Break)) {
        ParseBreak();
        return true;
    } else if (PeekToken(TokenKind::Continue)) {
        ParseContinue();
        return true;
    } else {
        ParseExpression(false);
        return false;
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

Type Compiler::ParseExpression(bool keep_result)
{
    Size start_values_len = stack.len;
    RG_DEFER { stack.RemoveFrom(start_values_len); };

    LocalArray<PendingOperator, 128> operators;
    bool expect_op = false;
    Size parentheses = 0;

    // Used to detect "empty" expressions
    Size prev_offset = pos;

    while (pos < tokens.len) {
        const Token &tok = tokens[pos++];

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
                   tok.kind == TokenKind::Integer || tok.kind == TokenKind::Float ||
                   tok.kind == TokenKind::String || tok.kind == TokenKind::Identifier) {
            if (RG_UNLIKELY(expect_op))
                goto unexpected_token;
            expect_op = true;

            switch (tok.kind) {
                case TokenKind::Null: { stack.Append({Type::Null}); } break;
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
                case TokenKind::Float: {
                    if (operators.len && operators[operators.len - 1].kind == TokenKind::Minus &&
                                         operators[operators.len - 1].unary) {
                        operators.RemoveLast(1);

                        program.ir.Append({Opcode::PushFloat, {.d = -tok.u.d}});
                        stack.Append({Type::Float});
                    } else {
                        program.ir.Append({Opcode::PushFloat, {.d = tok.u.d}});
                        stack.Append({Type::Float});
                    }
                } break;
                case TokenKind::String: {
                    program.ir.Append({Opcode::PushString, {.str = tok.u.str}});
                    stack.Append({Type::String});
                } break;

                case TokenKind::Identifier: {
                    if (MatchToken(TokenKind::LeftParenthesis)) {
                        if (RG_UNLIKELY(!ParseCall(tok.u.str)))
                            return Type::Null;
                    } else {
                        const VariableInfo *var = variables_map.FindValue(tok.u.str, nullptr);

                        if (RG_UNLIKELY(!var)) {
                            MarkError(pos - 1, "Variable '%1' does not exist", tok.u.str);
                            return Type::Null;
                        }
                        valid_stmt &= !var->poisoned;

                        if (var->global) {
                            if (RG_UNLIKELY(current_func &&
                                            current_func->earliest_call_idx < var->defined_idx)) {
                                MarkError(current_func->defined_pos, "Function '%1' may be called before variable '%2' exists",
                                          current_func->name, var->name);
                                HintError(current_func->earliest_call_pos, "Function call happens here (it could be indirect)");
                                HintError(var->defined_pos, "Variable '%1' is defined here", var->name);

                                return Type::Null;
                            }

                            switch (var->type) {
                                case Type::Null: {} break;
                                case Type::Bool: { program.ir.Append({Opcode::LoadGlobalBool, {.i = var->offset}}); } break;
                                case Type::Int: { program.ir.Append({Opcode::LoadGlobalInt, {.i = var->offset}}); } break;
                                case Type::Float: { program.ir.Append({Opcode::LoadGlobalFloat, {.i = var->offset}});} break;
                                case Type::String: { program.ir.Append({Opcode::LoadGlobalString, {.i = var->offset}}); } break;
                            }
                        } else {
                            switch (var->type) {
                                case Type::Null: {} break;
                                case Type::Bool: { program.ir.Append({Opcode::LoadLocalBool, {.i = var->offset}}); } break;
                                case Type::Int: { program.ir.Append({Opcode::LoadLocalInt, {.i = var->offset}}); } break;
                                case Type::Float: { program.ir.Append({Opcode::LoadLocalFloat, {.i = var->offset}});} break;
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
            op.pos = pos - 1;

            if (RG_UNLIKELY(op.prec < 0)) {
                if (pos == prev_offset + 1) {
                    if (pos > tokens.len) {
                        MarkError(pos, "Unexpected end of file, expected value or expression");
                    } else {
                        MarkError(pos - 1, "Unexpected token '%1', expected value or expression",
                                  TokenKindNames[(int)tokens[pos - 1].kind]);
                    }

                    return Type::Null;
                } else if (!expect_op && tok.kind == TokenKind::EndOfLine) {
                    if (generate_debug) {
                        debug.lines.Append(program.ir.len);
                    }
                    continue;
                } else if (parentheses || !expect_op) {
                    goto unexpected_token;
                } else {
                    pos--;
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
                MarkError(pos - 1, "Too many operators on the stack (compiler limitation)");
                return Type::Null;
            }
            operators.Append(op);
        }
    }

    if (RG_UNLIKELY(!expect_op)) {
        MarkError(pos - 1, "Unexpected end of expression, expected value or '('");
        return Type::Null;
    }
    RG_ASSERT(!parentheses);

    // Discharge remaining operators
    for (Size i = operators.len - 1; i >= 0; i--) {
        const PendingOperator &op = operators[i];
        ProduceOperator(op);
    }

    if (RG_UNLIKELY(!valid_stmt))
        return Type::Null;

    RG_ASSERT(stack.len == start_values_len + 1);
    if (keep_result) {
        return stack[stack.len - 1].type;
    } else if (stack[stack.len - 1].type != Type::Null) {
        if (program.ir.len >= 2 && program.ir[program.ir.len - 2].code == Opcode::Duplicate) {
            std::swap(program.ir[program.ir.len - 2], program.ir[program.ir.len - 1]);
            program.ir.len--;
        } else {
            EmitPop(1);
        }

        return Type::Null;
    } else {
        return Type::Null;
    }

unexpected_token:
    MarkError(pos - 1, "Unexpected token '%1', expected %2", TokenKindNames[(int)tokens[pos - 1].kind],
              expect_op ? "operator or ')'" : "value or '('");
    return Type::Null;
}

void Compiler::ProduceOperator(const PendingOperator &op)
{
    bool success;

    switch (op.kind) {
        case TokenKind::Reassign: {
            const StackSlot &slot1 = stack[stack.len - 2];
            const StackSlot &slot2 = stack[stack.len - 1];

            if (RG_UNLIKELY(!slot1.var)) {
                MarkError(op.pos, "Cannot assign expression to rvalue");
                return;
            }
            if (RG_UNLIKELY(slot1.var->readonly)) {
                MarkError(op.pos, "Cannot assign expression to const variable '%1'", slot1.var->name);
                HintError(slot1.var->defined_pos, "Variable '%1' is defined here without mut qualifier", slot1.var->name);

                return;
            }
            if (RG_UNLIKELY(slot1.type != slot2.type)) {
                MarkError(op.pos, "Cannot assign %1 value to %2 variable",
                          TypeNames[(int)slot2.type], TypeNames[(int)slot1.type]);
                HintError(slot1.var->defined_pos, "Variable '%1' is defined here", slot1.var->name);
                return;
            }

            if (slot1.var->global) {
                switch (slot1.type) {
                    case Type::Null: {} break;
                    case Type::Bool: {
                        program.ir.Append({Opcode::Duplicate});
                        program.ir.Append({Opcode::StoreGlobalBool, {.i = slot1.var->offset}});
                    } break;
                    case Type::Int: {
                        program.ir.Append({Opcode::Duplicate});
                        program.ir.Append({Opcode::StoreGlobalInt, {.i = slot1.var->offset}});
                    } break;
                    case Type::Float: {
                        program.ir.Append({Opcode::Duplicate});
                        program.ir.Append({Opcode::StoreGlobalFloat, {.i = slot1.var->offset}});
                    } break;
                    case Type::String: {
                        program.ir.Append({Opcode::Duplicate});
                        program.ir.Append({Opcode::StoreGlobalString, {.i = slot1.var->offset}});
                    } break;
                }
            } else {
                switch (slot1.type) {
                    case Type::Null: {} break;
                    case Type::Bool: {
                        program.ir.Append({Opcode::Duplicate});
                        program.ir.Append({Opcode::StoreLocalBool, {.i = slot1.var->offset}});
                    } break;
                    case Type::Int: {
                        program.ir.Append({Opcode::Duplicate});
                        program.ir.Append({Opcode::StoreLocalInt, {.i = slot1.var->offset}});
                    } break;
                    case Type::Float: {
                        program.ir.Append({Opcode::Duplicate});
                        program.ir.Append({Opcode::StoreLocalFloat, {.i = slot1.var->offset}});
                    } break;
                    case Type::String: {
                        program.ir.Append({Opcode::Duplicate});
                        program.ir.Append({Opcode::StoreLocalString, {.i = slot1.var->offset}});
                    } break;
                }
            }

            std::swap(stack[stack.len - 1], stack[stack.len - 2]);
            stack.len--;

            return;
        } break;

        case TokenKind::Plus: {
            success = EmitOperator2(Type::Int, Opcode::AddInt, Type::Int) ||
                      EmitOperator2(Type::Float, Opcode::AddFloat, Type::Float);
        } break;
        case TokenKind::Minus: {
            if (op.unary) {
                success = EmitOperator1(Type::Int, Opcode::NegateInt, Type::Int) ||
                          EmitOperator1(Type::Float, Opcode::NegateFloat, Type::Float);
            } else {
                success = EmitOperator2(Type::Int, Opcode::SubstractInt, Type::Int) ||
                          EmitOperator2(Type::Float, Opcode::SubstractFloat, Type::Float);
            }
        } break;
        case TokenKind::Multiply: {
            success = EmitOperator2(Type::Int, Opcode::MultiplyInt, Type::Int) ||
                      EmitOperator2(Type::Float, Opcode::MultiplyFloat, Type::Float);
        } break;
        case TokenKind::Divide: {
            success = EmitOperator2(Type::Int, Opcode::DivideInt, Type::Int) ||
                      EmitOperator2(Type::Float, Opcode::DivideFloat, Type::Float);
        } break;
        case TokenKind::Modulo: {
            success = EmitOperator2(Type::Int, Opcode::ModuloInt, Type::Int);
        } break;

        case TokenKind::Equal: {
            success = EmitOperator2(Type::Int, Opcode::EqualInt, Type::Bool) ||
                      EmitOperator2(Type::Float, Opcode::EqualFloat, Type::Bool) ||
                      EmitOperator2(Type::Bool, Opcode::EqualBool, Type::Bool);
        } break;
        case TokenKind::NotEqual: {
            success = EmitOperator2(Type::Int, Opcode::NotEqualInt, Type::Bool) ||
                      EmitOperator2(Type::Float, Opcode::NotEqualFloat, Type::Bool) ||
                      EmitOperator2(Type::Bool, Opcode::NotEqualBool, Type::Bool);
        } break;
        case TokenKind::Greater: {
            success = EmitOperator2(Type::Int, Opcode::GreaterThanInt, Type::Bool) ||
                      EmitOperator2(Type::Float, Opcode::GreaterThanFloat, Type::Bool);
        } break;
        case TokenKind::GreaterOrEqual: {
            success = EmitOperator2(Type::Int, Opcode::GreaterOrEqualInt, Type::Bool) ||
                      EmitOperator2(Type::Float, Opcode::GreaterOrEqualFloat, Type::Bool);
        } break;
        case TokenKind::Less: {
            success = EmitOperator2(Type::Int, Opcode::LessThanInt, Type::Bool) ||
                      EmitOperator2(Type::Float, Opcode::LessThanFloat, Type::Bool);
        } break;
        case TokenKind::LessOrEqual: {
            success = EmitOperator2(Type::Int, Opcode::LessOrEqualInt, Type::Bool) ||
                      EmitOperator2(Type::Float, Opcode::LessOrEqualFloat, Type::Bool);
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
            MarkError(op.pos, "Cannot use '%1' operator on %2 value",
                      TokenKindNames[(int)op.kind], TypeNames[(int)stack[stack.len - 1].type]);
        } else if (stack[stack.len - 2].type == stack[stack.len - 1].type) {
            MarkError(op.pos, "Cannot use '%1' operator on %2 values",
                      TokenKindNames[(int)op.kind], TypeNames[(int)stack[stack.len - 2].type]);
        } else {
            MarkError(op.pos, "Cannot use '%1' operator on %2 and %3 values",
                      TokenKindNames[(int)op.kind], TypeNames[(int)stack[stack.len - 2].type],
                      TypeNames[(int)stack[stack.len - 1].type]);
        }
    }
}

bool Compiler::EmitOperator1(Type in_type, Opcode code, Type out_type)
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

bool Compiler::EmitOperator2(Type in_type, Opcode code, Type out_type)
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

// Don't try to call from outside ParseExpression()!
bool Compiler::ParseCall(const char *name)
{
    LocalArray<Type, RG_LEN(FunctionInfo::params.data)> types;

    Size call_pos = pos - 2;

    FunctionInfo *func0 = functions_map.FindValue(name, nullptr);
    if (RG_UNLIKELY(!func0)) {
        MarkError(call_pos, "Function '%1' does not exist", name);
        return false;
    }

    if (!MatchToken(TokenKind::RightParenthesis)) {
        types.Append(ParseExpression(true));
        while (MatchToken(TokenKind::Comma)) {
            if (RG_UNLIKELY(!types.Available())) {
                MarkError(pos, "Functions cannot take more than %1 arguments", RG_LEN(types.data));
                return false;
            }
            types.Append(ParseExpression(true));
        }

        ConsumeToken(TokenKind::RightParenthesis);
    }

    FunctionInfo *func = func0;
    while (!TestOverload(*func, types)) {
        func = func->overload_next;

        if (RG_UNLIKELY(func == func0)) {
            LocalArray<char, 1024> buf = {};
            for (Size i = 0; i < types.len; i++) {
                buf.len += Fmt(buf.TakeAvailable(), "%1%2", i ? ", " : "", TypeNames[(int)types[i]]).len;
            }

            MarkError(call_pos, "Cannot call '%1' with (%2) arguments", func0->name, buf);

            // Show all candidate functions with same name
            const FunctionInfo *it = func0;
            do {
                HintError(it->defined_pos, "Candidate '%1'", it->signature);
                it = it->overload_next;
            } while (it != func0);

            return false;
        }
    }

    if (func->intrinsic) {
        EmitIntrinsic(name, types);
    } else {
        if (func->inst_idx < 0) {
            forward_calls.Append({program.ir.len, func});

            if (current_func && current_func != func) {
                func->earliest_call_pos = std::min(func->earliest_call_pos, current_func->earliest_call_pos);
                func->earliest_call_idx = std::min(func->earliest_call_idx, current_func->earliest_call_idx);
            } else {
                func->earliest_call_pos = std::min(func->earliest_call_pos, call_pos);
                func->earliest_call_idx = std::min(func->earliest_call_idx, program.ir.len);
            }
        }
        program.ir.Append({Opcode::Call, {.i = func->inst_idx}});
        stack.Append({func->ret});
    }

    return true;
}

void Compiler::EmitIntrinsic(const char *name, Span<const Type> types)
{
    if (TestStr(name, "print") || TestStr(name, "printLn")) {
        RG_STATIC_ASSERT(RG_LEN(FunctionInfo::params.data) <= 18);

        bool println = TestStr(name, "printLn");

        uint64_t payload = 0;
        Size pop = 0;

        if (println) {
            program.ir.Append({Opcode::PushString, {.str = "\n"}});
            payload = (int)Type::String;
        }
        for (Size i = types.len - 1; i >= 0; i--) {
            payload = (payload << 3) | (int)types[i];
            pop += (types[i] != Type::Null);
        }

        payload = (payload << 5) | (pop + println);
        payload = (payload << 5) | (types.len + println);

        program.ir.Append({Opcode::Print, {.i = (int64_t)payload}});
        stack.Append({Type::Null});
    } else if (TestStr(name, "intToFloat")) {
        program.ir.Append({Opcode::IntToFloat});
        stack.Append({Type::Float});
    } else if (TestStr(name, "floatToInt")) {
        program.ir.Append({Opcode::FloatToInt});
        stack.Append({Type::Int});
    } else if (TestStr(name, "exit")) {
        program.ir.Append({Opcode::Exit});
        stack.Append({Type::Null});
    }
}

void Compiler::EmitPop(int64_t count)
{
    RG_ASSERT(count >= 0);

    if (count) {
        program.ir.Append({Opcode::Pop, {.i = count}});
    }
}

bool Compiler::TestOverload(const FunctionInfo &proto, Span<const Type> types)
{
    if (proto.variadic) {
        if (proto.params.len > types.len)
            return false;
    } else {
        if (proto.params.len != types.len)
            return false;
    }

    for (Size i = 0; i < proto.params.len; i++) {
        if (proto.params[i].type != types[i])
            return false;
    }

    return true;
}

void Compiler::Finish(Program *out_program, DebugInfo *out_debug)
{
    RG_ASSERT(!out_program->ir.len);
    RG_ASSERT(!!generate_debug == !!out_debug);

    program.ir.Append({Opcode::PushInt, {.i = 0}});
    program.ir.Append({Opcode::Exit, {.b = true}});

    for (const FunctionInfo &func: functions) {
        const FunctionInfo *func2 = program.functions.Append(func);
        program.functions_map.Append(func2);
    }
    for (const VariableInfo &var: variables) {
        const VariableInfo *global = program.globals.Append(var);
        program.globals_map.Append(global);
    }

    program.ir.Trim();
    SwapMemory(&program, out_program, RG_SIZE(program));
    if (out_debug) {
        SwapMemory(&debug, out_debug, RG_SIZE(debug));
    }
}

bool Compiler::ConsumeToken(TokenKind kind)
{
    if (RG_UNLIKELY(pos >= tokens.len)) {
        MarkError(pos, "Unexpected end of file, expected '%1'", TokenKindNames[(int)kind]);
        return false;
    }
    if (RG_UNLIKELY(tokens[pos].kind != kind)) {
        MarkError(pos, "Unexpected token '%1', expected '%2'",
                  TokenKindNames[(int)tokens[pos].kind], TokenKindNames[(int)kind]);
        return false;
    }

    if (generate_debug && kind == TokenKind::EndOfLine) {
        debug.lines.Append(program.ir.len);
    }

    pos++;
    return true;
}

const char *Compiler::ConsumeIdentifier()
{
    if (RG_LIKELY(ConsumeToken(TokenKind::Identifier))) {
        return tokens[pos - 1].u.str;
    } else {
        return "";
    }
}

Type Compiler::ConsumeType()
{
    const char *type_name = ConsumeIdentifier();

    Type type;
    if (RG_LIKELY(OptionToEnum(TypeNames, type_name, &type))) {
        return type;
    } else {
        MarkError(pos - 1, "Type '%1' is not valid", type_name);
        return Type::Null;
    }
}

bool Compiler::MatchToken(TokenKind kind)
{
    bool match = pos < tokens.len && tokens[pos].kind == kind;
    pos += match;

    if (generate_debug && match && kind == TokenKind::EndOfLine) {
        debug.lines.Append(program.ir.len);
    }

    return match;
}

bool Compiler::PeekToken(TokenKind kind)
{
    bool match = pos < tokens.len && tokens[pos].kind == kind;
    return match;
}

void Compiler::DestroyVariables(Size count)
{
    for (Size i = variables.len - count; i < variables.len; i++) {
        const VariableInfo &var = variables[i];
        VariableInfo **ptr = variables_map.Find(var.name);

        if (var.shadow) {
            *ptr = (VariableInfo *)var.shadow;
        } else {
            variables_map.Remove(ptr);
        }
    }

    variables.RemoveLast(count);
}

bool Compile(const TokenSet &set, const char *filename,
             Program *out_program, DebugInfo *out_debug)
{
    Compiler compiler(out_debug);
    if (!compiler.Parse(set, filename))
        return false;

    compiler.Finish(out_program, out_debug);
    return true;
}

}
