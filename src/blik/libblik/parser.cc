// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "error.hh"
#include "lexer.hh"
#include "parser.hh"
#include "program.hh"

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

class ParserImpl {
    RG_DELETE_COPY(ParserImpl)

    // All these members are relevant to the current parse only, and get resetted each time
    const TokenizedFile *file;
    ParseReport *out_report; // Can be NULL
    Span<const Token> tokens;
    Size pos;
    bool valid;
    bool show_errors;
    bool show_hints;
    SourceInfo *src;

    // Transient mappings
    HashMap<Size, FunctionInfo *> functions_by_pos;
    HashMap<const void *, Size> definitions_map;
    HashSet<const void *> poisons;

    HeapArray<ForwardCall> forward_calls;

    Size func_jump_idx = -1;
    FunctionInfo *current_func = nullptr;
    int depth = -1;

    Size var_offset = 0;

    Size loop_var_offset = -1;
    HeapArray<Size> loop_breaks;
    HeapArray<Size> loop_continues;

    HashSet<const char *> strings;

    // Only used (and valid) while parsing expression
    HeapArray<StackSlot> stack;

    Program *program;
    HeapArray<Instruction> &ir;

public:
    ParserImpl(Program *program);

    bool Parse(const TokenizedFile &file, ParseReport *out_report);

    void AddFunction(const char *signature, std::function<NativeFunction> native);

private:
    void ParsePrototypes(Span<const Size> funcs);

    bool ParseBlock(bool keep_variables);
    void ParseFunction();
    void ParseReturn();
    void ParseLet();
    bool ParseIf();
    void ParseWhile();
    void ParseFor();
    void ParseBreak();
    void ParseContinue();

    bool ParseCondition();
    bool ParseDo();

    Type ParseExpression(bool keep_result);
    void ProduceOperator(const PendingOperator &op);
    bool EmitOperator1(Type in_type, Opcode code, Type out_type);
    bool EmitOperator2(Type in_type, Opcode code, Type out_type);
    bool ParseCall(const char *name);
    void EmitIntrinsic(const char *name, Span<const FunctionInfo::Parameter> args);
    void EmitLoad(const VariableInfo &var);

    void EmitPop(int64_t count);
    void EmitReturn();
    void DestroyVariables(Size count);

    bool TestOverload(const FunctionInfo &proto, Span<const FunctionInfo::Parameter> params2);

    bool ConsumeToken(TokenKind kind);
    const char *ConsumeIdentifier();
    Type ConsumeType();

    bool MatchToken(TokenKind kind);
    bool PeekToken(TokenKind kind);

    const char *InternString(const char *str);

    template <typename... Args>
    void MarkError(Size pos, const char *fmt, Args... args)
    {
        RG_ASSERT(pos >= 0);

        if (show_errors) {
            Size offset = (pos < tokens.len) ? tokens[pos].offset : file->code.len;
            int line = tokens[std::min(pos, tokens.len - 1)].line;

            ReportDiagnostic(DiagnosticType::Error, file->code, file->filename, line, offset, fmt, args...);

            show_errors = false;
            show_hints = true;
        } else {
            show_hints = false;
        }

        if (out_report && valid) {
            out_report->depth = depth;
        }
        valid = false;
    }

    template <typename... Args>
    void HintError(Size pos, const char *fmt, Args... args)
    {
        if (show_hints) {
            if (pos >= 0) {
                Size offset = (pos < tokens.len) ? tokens[pos].offset : file->code.len;
                int line = tokens[std::min(pos, tokens.len - 1)].line;

                ReportDiagnostic(DiagnosticType::ErrorHint, file->code, file->filename, line, offset, fmt, args...);
            } else {
                ReportDiagnostic(DiagnosticType::ErrorHint, fmt, args...);
            }
        }
    }

    template <typename... Args>
    void HintDefinition(const void *defn, const char *fmt, Args... args)
    {
        Size defn_pos = definitions_map.FindValue(defn, -1);
        if (defn_pos >= 0) {
            HintError(defn_pos, fmt, args...);
        }
    }
};

Parser::Parser(Program *program)
{
    impl = new ParserImpl(program);
}

Parser::~Parser()
{
    delete impl;
}

bool Parser::Parse(const TokenizedFile &file, ParseReport *out_report)
{
    return impl->Parse(file, out_report);
}

void Parser::AddFunction(const char *signature, std::function<NativeFunction> native)
{
    RG_ASSERT(native);
    impl->AddFunction(signature, native);
}

ParserImpl::ParserImpl(Program *program)
    : program(program), ir(program->ir)
{
    RG_ASSERT(program);
    RG_ASSERT(!program->ir.len);

    // Intrinsics
    AddFunction("print(...)", {});
    AddFunction("printLn(...)", {});
    AddFunction("intToFloat(Int): Float", {});
    AddFunction("floatToInt(Float): Int", {});
}

bool ParserImpl::Parse(const TokenizedFile &file, ParseReport *out_report)
{
    // Restore previous state if something goes wrong
    RG_DEFER_NC(err_guard, ir_len = ir.len,
                           sources_len = program->sources.len,
                           prev_var_offset = var_offset,
                           variables_len = program->variables.len,
                           functions_len = program->functions.len) {
        ir.RemoveFrom(ir_len);
        program->sources.RemoveFrom(sources_len);

        var_offset = prev_var_offset;
        DestroyVariables(program->variables.len - variables_len);

        if (functions_len) {
            for (Size i = functions_len; i < program->functions.len; i++) {
                FunctionInfo *func = &program->functions[i];
                FunctionInfo **it = program->functions_map.Find(func->name);

                if (*it == func && func->overload_next == func) {
                    program->functions_map.Remove(it);
                } else {
                    if (*it == func) {
                        *it = func->overload_next;
                    }

                    func->overload_next->overload_prev = func->overload_prev;
                    func->overload_prev->overload_next = func->overload_next;
                }
            }
        } else {
            program->functions_map.Clear();
        }
        program->functions.RemoveFrom(functions_len);
    };

    this->file = &file;
    this->out_report = out_report;
    if (out_report) {
        *out_report = {};
    }
    tokens = file.tokens;
    pos = 0;

    valid = true;
    show_errors = true;
    show_hints = false;

    forward_calls.Clear();
    functions_by_pos.Clear();
    definitions_map.Clear();

    src = program->sources.AppendDefault();
    src->filename = DuplicateString(file.filename, &program->str_alloc).ptr;

    // We want top-level order-independent functions
    ParsePrototypes(file.funcs);

    // Do the actual parsing!
    ParseBlock(true);
    while (RG_UNLIKELY(pos < tokens.len)) {
        MarkError(pos, "Unexpected token '%1' without matching block", TokenKindNames[(int)tokens[pos].kind]);

        pos++;
        ParseBlock(true);
    }

    // Maybe it'll help catch bugs
    RG_ASSERT(depth == -1);
    RG_ASSERT(loop_var_offset == -1);
    RG_ASSERT(!current_func);

    // Fix up foward calls
    for (const ForwardCall &call: forward_calls) {
        ir[call.offset].u.i = call.func->inst_idx;
    }
    forward_calls.Clear();

    ir.Append({Opcode::End});
    program->end_stack_len = var_offset;

    // The caller may want to execute the code and then compile new code (e.g. REPL),
    // we can't ever try to reuse a jump that will not be executed!
    func_jump_idx = -1;

    if (valid) {
        err_guard.Disable();
    }
    return valid;
}

// This is not exposed to user scripts, and the validation of signature is very light,
// with a few debug-only asserts. Bad function names (even invalid UTF-8 sequences)
// will go right through. Don't pass in garbage!
void ParserImpl::AddFunction(const char *signature, std::function<NativeFunction> native)
{
    FunctionInfo *func = program->functions.AppendDefault();

    const char *ptr = signature;

    // Name and signature
    {
        Size len = strcspn(ptr, "()");
        Span<const char> func_name = TrimStr(MakeSpan(ptr, len));

        func->name = DuplicateString(func_name, &program->str_alloc).ptr;
        ptr += len;
    }
    func->signature = DuplicateString(signature, &program->str_alloc).ptr;

    func->intrinsic = !native;
    func->native = native;

    // Parameters
    RG_ASSERT(ptr[0] == '(');
    if (ptr[1] != ')') {
        do {
            Size len = strcspn(++ptr, ",)");
            Span<const char> type_name = TrimStr(MakeSpan(ptr, len));

            if (type_name == "...") {
                RG_ASSERT(ptr[len] == ')');
                RG_ASSERT(func->intrinsic);

                func->variadic = true;
            } else {
                Type type = Type::Null;

                bool success = OptionToEnum(TypeNames, type_name, &type);
                RG_ASSERT(success);

                func->params.Append({"", type});
                func->ret_pop += (type != Type::Null);
            }

            ptr += len;
            RG_ASSERT(ptr[0]);
        } while (ptr[0] != ')');

        ptr++;
    } else {
        ptr += 2;
    }

    // Return type
    if (ptr[0] == ':') {
        Span<const char> type_name = TrimStr(Span<const char>(ptr + 1));

        bool success = OptionToEnum(TypeNames, type_name, &func->ret_type);
        RG_ASSERT(success);
    }

    if (native) {
        RG_STATIC_ASSERT(RG_LEN(FunctionInfo::params.data) < 64);

        uint64_t payload = 0;

        payload |= (uint64_t)func->ret_pop << 57;
        payload |= (uint64_t)&func->native & 0x1FFFFFFFFFFFFFFull;

        // Jump over consecutively defined functions in one go
        if (func_jump_idx >= 0 && ir[func_jump_idx].u.i == ir.len - func_jump_idx) {
            ir[func_jump_idx].u.i++;
        } else {
            func_jump_idx = ir.len;
            ir.Append({Opcode::Jump, {.i = 2}});
        }

        func->inst_idx = ir.len;
        ir.Append({func->ret_type != Type::Null ? Opcode::Invoke : Opcode::InvokeNull, {.payload = payload}});
    }

    // Publish it!
    {
        FunctionInfo *head = *program->functions_map.Append(func).first;

        if (head != func) {
            head->overload_prev->overload_next = func;
            func->overload_next = head;
            func->overload_prev = head->overload_prev;
            head->overload_prev = func;

#ifndef NDEBUG
            do {
                RG_ASSERT(head->intrinsic == func->intrinsic);
                RG_ASSERT(!TestOverload(*head, func->params));

                head = head->overload_next;
            } while (head != func);
#endif
        } else {
            func->overload_prev = func;
            func->overload_next = func;
        }
    }
}

void ParserImpl::ParsePrototypes(Span<const Size> funcs)
{
    RG_ASSERT(!functions_by_pos.table.count);
    RG_ASSERT(pos == 0);
    RG_ASSERT(!src->lines.len);

    // This is a preparse step, don't make any noise. And make sure nobody tries to
    // call ParsePrototypes() ever again!
    RG_DEFER {
        pos = 0;
        valid = true;
        show_errors = true;
        src->lines.RemoveFrom(0);
    };
    show_errors = false;

    for (Size i = 0; i < funcs.len; i++) {
        pos = funcs[i] + 1;

        FunctionInfo *proto = program->functions.AppendDefault();
        functions_by_pos.Append(pos, proto);

        definitions_map.Append(proto, pos);
        proto->name = ConsumeIdentifier();

        // Insert in functions map
        {
            std::pair<FunctionInfo **, bool> ret = program->functions_map.Append(proto);
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
                while (MatchToken(TokenKind::EndOfLine));

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

            while (MatchToken(TokenKind::EndOfLine));
            ConsumeToken(TokenKind::RightParenthesis);
        }

        // Return type
        if (MatchToken(TokenKind::Colon)) {
            proto->ret_type = ConsumeType();
        }

        // Build signature (with parameter and return types)
        {
            HeapArray<char> buf(&program->str_alloc);

            Fmt(&buf, "%1(", proto->name);
            for (Size i = 0; i < proto->params.len; i++) {
                const FunctionInfo::Parameter &param = proto->params[i];
                Fmt(&buf, "%1%2", i ? ", " : "", TypeNames[(int)param.type]);
            }
            Fmt(&buf, ")");
            if (proto->ret_type != Type::Null) {
                Fmt(&buf, ": %1", TypeNames[(int)proto->ret_type]);
            }

            proto->signature = buf.TrimAndLeak(1).ptr;
        }

        // We don't know where it will live yet!
        proto->inst_idx = -1;

        proto->earliest_call_pos = RG_SIZE_MAX;
        proto->earliest_call_idx = RG_SIZE_MAX;
    }
}

bool ParserImpl::ParseBlock(bool keep_variables)
{
    depth++;

    RG_DEFER_C(prev_offset = var_offset,
               variables_len = program->variables.len) {
        depth--;

        if (!keep_variables) {
            EmitPop(var_offset - prev_offset);
            DestroyVariables(program->variables.len - variables_len);
            var_offset = prev_offset;
        }
    };

    bool has_return = false;

    while (RG_LIKELY(pos < tokens.len)) {
        src->lines.Append({tokens[pos].line, ir.len});

        switch (tokens[pos].kind) {
            case TokenKind::EndOfLine: {
                src->lines.len--;
                pos++;
            } break;

            case TokenKind::End:
            case TokenKind::Else: { return has_return; } break;

            case TokenKind::Begin: {
                pos++;

                show_errors |= ConsumeToken(TokenKind::EndOfLine);
                has_return |= ParseBlock(false);
                ConsumeToken(TokenKind::End);
                show_errors |= ConsumeToken(TokenKind::EndOfLine);
            } break;

            case TokenKind::Func: {
                ParseFunction();
                show_errors |= ConsumeToken(TokenKind::EndOfLine);
            } break;

            case TokenKind::Return: {
                ParseReturn();
                show_errors |= ConsumeToken(TokenKind::EndOfLine);
                has_return = true;
            } break;

            case TokenKind::Let: {
                ParseLet();
                show_errors |= ConsumeToken(TokenKind::EndOfLine);
            } break;
            case TokenKind::If: {
                has_return |= ParseIf();
                show_errors |= ConsumeToken(TokenKind::EndOfLine);
            } break;
            case TokenKind::While: {
                ParseWhile();
                show_errors |= ConsumeToken(TokenKind::EndOfLine);
            } break;
            case TokenKind::For: {
                ParseFor();
                show_errors |= ConsumeToken(TokenKind::EndOfLine);
            } break;

            case TokenKind::Break: {
                ParseBreak();
                show_errors |= ConsumeToken(TokenKind::EndOfLine);
            } break;
            case TokenKind::Continue: {
                ParseContinue();
                show_errors |= ConsumeToken(TokenKind::EndOfLine);
            } break;

            default: {
                ParseExpression(false);
                show_errors |= ConsumeToken(TokenKind::EndOfLine);
            } break;
        }
    }

    return has_return;
}

void ParserImpl::ParseFunction()
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

    ConsumeToken(TokenKind::Identifier);

    if (RG_UNLIKELY(current_func)) {
        MarkError(func_pos, "Nested functions are not supported");
        HintError(definitions_map.FindValue(current_func, -1), "Previous function was started here and is still open");
    } else if (RG_UNLIKELY(depth)) {
        MarkError(func_pos, "Functions must be defined in top-level scope");
    }
    current_func = func;

    // Parameters
    ConsumeToken(TokenKind::LeftParenthesis);
    if (!MatchToken(TokenKind::RightParenthesis)) {
        Size stack_offset = -2 - func->params.len;

        do {
            while (MatchToken(TokenKind::EndOfLine));

            VariableInfo *var = program->variables.AppendDefault();

            var->readonly = !MatchToken(TokenKind::Mut);
            definitions_map.Append(var, pos);
            var->name = ConsumeIdentifier();

            if (stack_offset >= -2) {
                MarkError(pos - 1, "Functions cannot have more than %1 parameters", RG_LEN(func->params.data));
            }
            var->offset = stack_offset++;

            std::pair<VariableInfo **, bool> ret = program->variables_map.Append(var);
            if (RG_UNLIKELY(!ret.second)) {
               const VariableInfo *prev_var = *ret.first;
               var->shadow = prev_var;

                if (prev_var->global) {
                    MarkError(pos - 1, "Parameter '%1' is not allowed to hide global variable", var->name);
                    HintDefinition(prev_var, "Global variable '%1' is defined here", prev_var->name);
                } else {
                    MarkError(pos - 1, "Parameter '%1' already exists", var->name);
                }
            }

            ConsumeToken(TokenKind::Colon);
            var->type = ConsumeType();

            if (RG_UNLIKELY(!show_errors)) {
                poisons.Append(var);
            }
        } while (MatchToken(TokenKind::Comma));

        while (MatchToken(TokenKind::EndOfLine));
        ConsumeToken(TokenKind::RightParenthesis);
    }

    // Return type
    if (MatchToken(TokenKind::Colon)) {
        ConsumeType();
    }

    // Check for incompatible function overloadings
    {
        FunctionInfo *overload = program->functions_map.FindValue(func->name, nullptr);

        while (overload != func) {
            if (RG_UNLIKELY(overload->intrinsic)) {
                MarkError(func_pos, "Cannot replace or overload intrinsic function '%1'", func->name);
            }

            if (TestOverload(*overload, func->params)) {
                if (overload->ret_type == func->ret_type) {
                    MarkError(func_pos, "Function '%1' is already defined", func->signature);
                } else {
                    MarkError(func_pos, "Function '%1' only differs from previously defined '%2' by return type",
                              func->signature, overload->signature);
                }
                HintDefinition(overload, "Previous definition is here");
            }

            overload = overload->overload_next;
        }
    }

    // Jump over consecutively defined functions in one go
    if (func_jump_idx < 0 || ir[func_jump_idx].u.i < ir.len - func_jump_idx) {
        func_jump_idx = ir.len;
        ir.Append({Opcode::Jump});
    }

    func->inst_idx = ir.len;
    var_offset = 0;

    // Function body
    bool has_return;
    if (PeekToken(TokenKind::Do)) {
        has_return = ParseDo();
    } else {
        show_errors |= ConsumeToken(TokenKind::EndOfLine);
        has_return = ParseBlock(false);
        ConsumeToken(TokenKind::End);
    }

    if (!has_return) {
        if (func->ret_type == Type::Null) {
            EmitReturn();
        } else {
            MarkError(func_pos, "Some code paths do not return a value in function '%1'", func->name);
        }
    }

    ir[func_jump_idx].u.i = ir.len - func_jump_idx;
}

void ParserImpl::ParseReturn()
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

    if (RG_UNLIKELY(type != current_func->ret_type)) {
        MarkError(return_pos, "Cannot return %1 value in function defined to return %2",
                  TypeNames[(int)type], TypeNames[(int)current_func->ret_type]);
        return;
    }

    EmitReturn();
}

void ParserImpl::ParseLet()
{
    Size var_pos = ++pos;

    VariableInfo *var = program->variables.AppendDefault();

    var->readonly = !MatchToken(TokenKind::Mut);
    definitions_map.Append(var, pos);
    var->name = ConsumeIdentifier();

    std::pair<VariableInfo **, bool> ret = program->variables_map.Append(var);
    if (RG_UNLIKELY(!ret.second)) {
        const VariableInfo *prev_var = *ret.first;
        var->shadow = prev_var;

        if (current_func && prev_var->global) {
            MarkError(var_pos, "Declaration '%1' is not allowed to hide global variable", var->name);
            HintDefinition(prev_var, "Global variable '%1' is defined here", prev_var->name);
        } else if (current_func && prev_var->offset < 0) {
            MarkError(var_pos, "Declaration '%1' is not allowed to hide parameter", var->name);
            HintDefinition(prev_var, "Parameter '%1' is defined here", prev_var->name);
        } else {
            MarkError(var_pos, "Variable '%1' already exists", var->name);
            HintDefinition(prev_var, "Previous variable '%1' is defined here", prev_var->name);
        }
    }

    if (MatchToken(TokenKind::Assign)) {
        var->type = ParseExpression(true);
    } else {
        ConsumeToken(TokenKind::Colon);
        var->type = ConsumeType();

        if (MatchToken(TokenKind::Assign)) {
            Type type2 = ParseExpression(true);

            if (RG_UNLIKELY(type2 != var->type)) {
                MarkError(var_pos + 3, "Cannot assign %1 value to variable '%2' (defined as %3)",
                          TypeNames[(int)type2], var->name, TypeNames[(int)var->type]);
            }
        } else {
            switch (var->type) {
                case Type::Null: {} break;
                case Type::Bool: { ir.Append({Opcode::PushBool, {.b = false}}); } break;
                case Type::Int: { ir.Append({Opcode::PushInt, {.i = 0}}); } break;
                case Type::Float: { ir.Append({Opcode::PushFloat, {.d = 0.0}}); } break;
                case Type::String: { ir.Append({Opcode::PushString, {.str = ""}}); } break;
            }
        }
    }

    var->global = !current_func;
    var->offset = var_offset;
    var->defined_idx = ir.len;

    // Null values don't actually exist
    var_offset += (var->type != Type::Null);

    // Expressions involving this variable won't issue (visible) errors
    // and will be marked as invalid too.
    if (RG_UNLIKELY(!show_errors)) {
        poisons.Append(var);
    }
}

bool ParserImpl::ParseIf()
{
    pos++;

    ParseCondition();

    Size branch_idx = ir.len;
    ir.Append({Opcode::BranchIfFalse});

    bool has_return = true;
    bool has_else = false;

    if (PeekToken(TokenKind::Then)) {
        has_return &= ParseDo();
        ir[branch_idx].u.i = ir.len - branch_idx;
    } else {
        show_errors |= ConsumeToken(TokenKind::EndOfLine);
        has_return &= ParseBlock(false);

        if (MatchToken(TokenKind::Else)) {
            HeapArray<Size> jumps;

            jumps.Append(ir.len);
            ir.Append({Opcode::Jump});

            do {
                ir[branch_idx].u.i = ir.len - branch_idx;

                if (MatchToken(TokenKind::If)) {
                    ParseCondition();
                    show_errors |= ConsumeToken(TokenKind::EndOfLine);

                    branch_idx = ir.len;
                    ir.Append({Opcode::BranchIfFalse});

                    has_return &= ParseBlock(false);

                    jumps.Append(ir.len);
                    ir.Append({Opcode::Jump});
                } else {
                    show_errors |= ConsumeToken(TokenKind::EndOfLine);

                    has_return &= ParseBlock(false);
                    has_else = true;

                    break;
                }
            } while (MatchToken(TokenKind::Else));

            for (Size jump_idx: jumps) {
                ir[jump_idx].u.i = ir.len - jump_idx;
            }
        } else {
            ir[branch_idx].u.i = ir.len - branch_idx;
        }

        ConsumeToken(TokenKind::End);
    }

    return has_return && has_else;
}

void ParserImpl::ParseWhile()
{
    int32_t while_line = tokens[pos].line;
    Size while_pos = ++pos;

    // Parse expression. We'll do it again at the end because we want to put a copy after
    // the loop body. The IR code will look roughly like if (cond) { do { ... } while (cond) }.
    ParseCondition();

    Size branch_idx = ir.len;
    ir.Append({Opcode::BranchIfFalse});

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
        show_errors |= ConsumeToken(TokenKind::EndOfLine);
        ParseBlock(false);
        ConsumeToken(TokenKind::End);
    }

    // Fix up continue jumps
    for (Size i = first_continue_idx; i < loop_continues.len; i++) {
        Size jump_idx = loop_continues[i];
        ir[jump_idx].u.i = ir.len - jump_idx;
    }

    // Parse the condition again. We could copy the IR instead but this implies a bunch of
    // annoying fixups (forward calls, IR/line map, etc.). It is easier and less fragile
    // this way, even if a bit slower. Don't show errors again though!
    {
        RG_DEFER_C(prev_pos = pos,
                   prev_errors = show_errors) {
            pos = prev_pos;
            show_errors = prev_errors;
        };

        pos = while_pos;
        show_errors = false;

        if (src->lines[src->lines.len - 1].line != while_line) {
            src->lines.Append({while_line, ir.len});
        }
        ParseCondition();
    }

    ir.Append({Opcode::BranchIfTrue, {.i = branch_idx - ir.len + 1}});
    ir[branch_idx].u.i = ir.len - branch_idx;

    // Fix up break jumps
    for (Size i = first_break_idx; i < loop_breaks.len; i++) {
        Size jump_idx = loop_breaks[i];
        ir[jump_idx].u.i = ir.len - jump_idx;
    }
}

void ParserImpl::ParseFor()
{
    Size for_pos = ++pos;

    VariableInfo *it = program->variables.AppendDefault();

    it->readonly = !MatchToken(TokenKind::Mut);
    definitions_map.Append(it, pos);
    it->name = ConsumeIdentifier();
    it->type = Type::Int;

    it->offset = var_offset + 2;

    std::pair<VariableInfo **, bool> ret = program->variables_map.Append(it);
    if (RG_UNLIKELY(!ret.second)) {
        const VariableInfo *prev_var = *ret.first;
        it->shadow = prev_var;

        if (current_func && prev_var->global) {
            MarkError(for_pos, "Iterator '%1' is not allowed to hide global variable", it->name);
            HintDefinition(prev_var, "Global variable '%1' is defined here", prev_var->name);
        } else {
            MarkError(for_pos, "Variable '%1' already exists", it->name);
            HintDefinition(prev_var, "Previous variable '%1' is defined here", prev_var->name);
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
    ir.Append({Opcode::LoadInt, {.i = it->offset - 2}});

    Size body_idx = ir.len;

    ir.Append({Opcode::LoadInt, {.i = it->offset}});
    ir.Append({Opcode::LoadInt, {.i = it->offset - 1}});
    ir.Append({inclusive ? Opcode::LessOrEqualInt : Opcode::LessThanInt});
    ir.Append({Opcode::BranchIfFalse});

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
        show_errors |= ConsumeToken(TokenKind::EndOfLine);
        ParseBlock(false);
        ConsumeToken(TokenKind::End);
    }

    // Fix up continue jumps
    for (Size i = first_continue_idx; i < loop_continues.len; i++) {
        Size jump_idx = loop_continues[i];
        ir[jump_idx].u.i = ir.len - jump_idx;
    }

    ir.Append({Opcode::PushInt, {.i = 1}});
    ir.Append({Opcode::AddInt});
    ir.Append({Opcode::Jump, {.i = body_idx - ir.len}});
    ir[body_idx + 3].u.i = ir.len - (body_idx + 3);

    // Fix up break jumps
    for (Size i = first_break_idx; i < loop_breaks.len; i++) {
        Size jump_idx = loop_breaks[i];
        ir[jump_idx].u.i = ir.len - jump_idx;
    }

    // Destroy iterator and range values
    EmitPop(3);
    DestroyVariables(1);
    var_offset -= 3;
}

void ParserImpl::ParseBreak()
{
    Size break_pos = pos++;

    if (loop_var_offset < 0) {
        MarkError(break_pos, "Break statement outside of loop");
        return;
    }

    EmitPop(var_offset - loop_var_offset);

    loop_breaks.Append(ir.len);
    ir.Append({Opcode::Jump});
}

void ParserImpl::ParseContinue()
{
    Size continue_pos = pos++;

    if (loop_var_offset < 0) {
        MarkError(continue_pos, "Continue statement outside of loop");
        return;
    }

    EmitPop(var_offset - loop_var_offset);

    loop_continues.Append(ir.len);
    ir.Append({Opcode::Jump});
}

bool ParserImpl::ParseCondition()
{
    Size cond_pos = pos;

    Type type = ParseExpression(true);
    if (RG_UNLIKELY(type != Type::Bool)) {
        MarkError(cond_pos, "Cannot implicitly convert %1 result to Bool condition", TypeNames[(int)type]);
        return false;
    }

    return true;
}

bool ParserImpl::ParseDo()
{
    pos++;

    if (PeekToken(TokenKind::Return)) {
        ParseReturn();
        return true;
    } else if (PeekToken(TokenKind::Break)) {
        ParseBreak();
        return false;
    } else if (PeekToken(TokenKind::Continue)) {
        ParseContinue();
        return false;
    } else {
        ParseExpression(false);
        return false;
    }
}

static int GetOperatorPrecedence(TokenKind kind)
{
    switch (kind) {
        case TokenKind::Reassign:
        case TokenKind::PlusAssign:
        case TokenKind::MinusAssign:
        case TokenKind::MultiplyAssign:
        case TokenKind::DivideAssign:
        case TokenKind::ModuloAssign:
        case TokenKind::LeftShiftAssign:
        case TokenKind::RightShiftAssign:
        case TokenKind::AndAssign:
        case TokenKind::OrAssign:
        case TokenKind::XorAssign: { return 0; } break;

        case TokenKind::OrOr: { return 2; } break;
        case TokenKind::AndAnd: { return 3; } break;
        case TokenKind::Equal:
        case TokenKind::NotEqual: { return 4; } break;
        case TokenKind::Greater:
        case TokenKind::GreaterOrEqual:
        case TokenKind::Less:
        case TokenKind::LessOrEqual: { return 5; } break;
        case TokenKind::Or: { return 6; } break;
        case TokenKind::Xor: { return 7; } break;
        case TokenKind::And: { return 8; } break;
        case TokenKind::LeftShift:
        case TokenKind::RightShift: { return 9; } break;
        // Unary '+' and '-' operators are dealt with directly in ParseExpression()
        case TokenKind::Plus:
        case TokenKind::Minus: { return 10; } break;
        case TokenKind::Multiply:
        case TokenKind::Divide:
        case TokenKind::Modulo: { return 11; } break;
        case TokenKind::Complement:
        case TokenKind::Not: { return 12; } break;

        default: { return -1; } break;
    }
}

Type ParserImpl::ParseExpression(bool keep_result)
{
    Size start_values_len = stack.len;
    RG_DEFER { stack.RemoveFrom(start_values_len); };

    LocalArray<PendingOperator, 128> operators;
    bool expect_value = true;
    Size parentheses = 0;

    // Used to detect "empty" expressions
    Size prev_offset = pos;

    while (RG_LIKELY(pos < tokens.len)) {
        const Token &tok = tokens[pos++];

        if (tok.kind == TokenKind::LeftParenthesis) {
            if (RG_UNLIKELY(!expect_value))
                goto unexpected;

            operators.Append({tok.kind});
            parentheses++;
        } else if (parentheses && tok.kind == TokenKind::RightParenthesis) {
            if (RG_UNLIKELY(expect_value))
                goto unexpected;
            expect_value = false;

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
            if (RG_UNLIKELY(!expect_value))
                goto unexpected;
            expect_value = false;

            switch (tok.kind) {
                case TokenKind::Null: { stack.Append({Type::Null}); } break;
                case TokenKind::Bool: {
                    ir.Append({Opcode::PushBool, {.b = tok.u.b}});
                    stack.Append({Type::Bool});
                } break;
                case TokenKind::Integer: {
                    ir.Append({Opcode::PushInt, {.i = tok.u.i}});
                    stack.Append({Type::Int});
                } break;
                case TokenKind::Float: {
                    ir.Append({Opcode::PushFloat, {.d = tok.u.d}});
                    stack.Append({Type::Float});
                } break;
                case TokenKind::String: {
                    ir.Append({Opcode::PushString, {.str = InternString(tok.u.str)}});
                    stack.Append({Type::String});
                } break;

                case TokenKind::Identifier: {
                    if (MatchToken(TokenKind::LeftParenthesis)) {
                        if (RG_UNLIKELY(!ParseCall(tok.u.str)))
                            goto error;
                    } else {
                        const VariableInfo *var = program->variables_map.FindValue(tok.u.str, nullptr);

                        if (RG_UNLIKELY(!var)) {
                            MarkError(pos - 1, "Variable '%1' does not exist", tok.u.str);
                            goto error;
                        }
                        show_errors &= !poisons.Find(var);

                        EmitLoad(*var);
                    }
                } break;

                default: { RG_UNREACHABLE(); } break;
            }
        } else {
            PendingOperator op = {};

            op.kind = tok.kind;
            op.prec = GetOperatorPrecedence(tok.kind);
            op.unary = (tok.kind == TokenKind::Complement || tok.kind == TokenKind::Not);
            op.pos = pos - 1;

            // Not an operator? There's a few cases to deal with, including a perfectly
            // valid one: end of expression!
            if (RG_UNLIKELY(op.prec < 0)) {
                if (pos == prev_offset + 1) {
                    MarkError(pos - 1, "Unexpected token '%1', expected value or expression",
                              TokenKindNames[(int)tokens[pos - 1].kind]);
                    goto error;
                } else if (tok.kind == TokenKind::EndOfLine && (expect_value || parentheses)) {
                    src->lines.Append({tok.line + 1, ir.len});
                    continue;
                } else if (tok.kind == TokenKind::Assign && !expect_value) {
                    MarkError(pos - 1, "Unexpected token '=', did you mean ':=' or '=='?");

                    // Pretend the user has typed '==' to avoid cascading errors
                    op.kind = TokenKind::Equal;
                    op.prec = GetOperatorPrecedence(TokenKind::Equal);
                } else if (parentheses || expect_value) {
                    goto unexpected;
                } else {
                    pos--;
                    break;
                }
            }

            if (RG_UNLIKELY(expect_value != op.unary)) {
                if (tok.kind == TokenKind::Plus || tok.kind == TokenKind::Minus) {
                    op.prec = 12;
                    op.unary = true;
                } else {
                    goto unexpected;
                }
            }
            expect_value = true;

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
                ir.RemoveLast(1);
            } else if (tok.kind == TokenKind::AndAnd) {
                op.branch_idx = ir.len;
                ir.Append({Opcode::SkipIfFalse});
            } else if (tok.kind == TokenKind::OrOr) {
                op.branch_idx = ir.len;
                ir.Append({Opcode::SkipIfTrue});
            }

            if (RG_UNLIKELY(!operators.Available())) {
                MarkError(pos - 1, "Too many operators on the stack (compiler limitation)");
                return Type::Null;
            }
            operators.Append(op);
        }
    }

    if (RG_UNLIKELY(expect_value || parentheses)) {
        if (out_report && valid) {
            out_report->unexpected_eof = true;
        }
        MarkError(pos - 1, "Unexpected end of file, expected value or '('");

        goto error;
    }

    // Discharge remaining operators
    for (Size i = operators.len - 1; i >= 0; i--) {
        const PendingOperator &op = operators[i];
        ProduceOperator(op);
    }

    RG_ASSERT(stack.len == start_values_len + 1 || !show_errors);
    if (RG_UNLIKELY(!stack.len))
        return Type::Null;

    if (keep_result) {
        return stack[stack.len - 1].type;
    } else if (stack[stack.len - 1].type != Type::Null) {
        if (RG_LIKELY(ir.len >= 1)) {
            switch (ir[ir.len - 1].code) {
                case Opcode::LoadBool:
                case Opcode::LoadInt:
                case Opcode::LoadFloat:
                case Opcode::LoadString:
                case Opcode::LoadGlobalBool:
                case Opcode::LoadGlobalInt:
                case Opcode::LoadGlobalFloat:
                case Opcode::LoadGlobalString: { ir.len--; } break;

                case Opcode::CopyBool: { ir[ir.len - 1].code = Opcode::StoreBool; } break;
                case Opcode::CopyInt: { ir[ir.len - 1].code = Opcode::StoreInt; } break;
                case Opcode::CopyFloat: { ir[ir.len - 1].code = Opcode::StoreFloat; } break;
                case Opcode::CopyString: { ir[ir.len - 1].code = Opcode::StoreString; } break;

                default: { EmitPop(1); } break;
            }
        }

        return Type::Null;
    } else {
        return Type::Null;
    }

unexpected:
    pos--;
    MarkError(pos, "Unexpected token '%1', expected %2", TokenKindNames[(int)tokens[pos].kind],
              expect_value ? "value or '('" : "operator or ')'");
error:
    // The goal of this loop is to skip expression until we get to "do" (which is used
    // for single-line constructs) or end of line (which starts a block in some cases,
    // e.g. if expressions). This way, the parent can differenciate single-line constructs
    // and block constructs, and prevent generation of garbage errors (such as "functions
    // must be defined in top-level scope") caused by undetected block and/or do statement.
    while (pos < tokens.len && tokens[pos].kind != TokenKind::Do &&
                               tokens[pos].kind != TokenKind::EndOfLine) {
        pos++;
    };
    return Type::Null;
}

void ParserImpl::ProduceOperator(const PendingOperator &op)
{
    bool success = false;

    if (op.prec == 0) { // Assignement operators
        RG_ASSERT(!op.unary);

        const VariableInfo *var = stack[stack.len - 2].var;
        const StackSlot &expr = stack[stack.len - 1];

        if (RG_UNLIKELY(!var)) {
            MarkError(op.pos, "Cannot assign result to temporary value; left operand should be a variable");
            return;
        }
        if (RG_UNLIKELY(var->readonly)) {
            MarkError(op.pos, "Cannot assign result to non-mutable variable '%1'", var->name);
            HintError(definitions_map.FindValue(var, -1), "Variable '%1' is defined without 'mut' qualifier", var->name);

            return;
        }
        if (RG_UNLIKELY(var->type != expr.type)) {
            MarkError(op.pos, "Cannot assign %1 value to variable '%2'",
                      TypeNames[(int)expr.type], var->name);
            HintError(definitions_map.FindValue(var, -1), "Variable '%1' is defined as %2",
                      var->name, TypeNames[(int)var->type]);
            return;
        }

        switch (op.kind) {
            case TokenKind::Reassign: {
                stack[--stack.len - 1].var = nullptr;
                success = true;
            } break;

            case TokenKind::PlusAssign: {
                success = EmitOperator2(Type::Int, Opcode::AddInt, Type::Int) ||
                          EmitOperator2(Type::Float, Opcode::AddFloat, Type::Float);
            } break;
            case TokenKind::MinusAssign: {
                success = EmitOperator2(Type::Int, Opcode::SubstractInt, Type::Int) ||
                          EmitOperator2(Type::Float, Opcode::SubstractFloat, Type::Float);
            } break;
            case TokenKind::MultiplyAssign: {
                success = EmitOperator2(Type::Int, Opcode::MultiplyInt, Type::Int) ||
                          EmitOperator2(Type::Float, Opcode::MultiplyFloat, Type::Float);
            } break;
            case TokenKind::DivideAssign: {
                success = EmitOperator2(Type::Int, Opcode::DivideInt, Type::Int) ||
                          EmitOperator2(Type::Float, Opcode::DivideFloat, Type::Float);
            } break;
            case TokenKind::ModuloAssign: {
                success = EmitOperator2(Type::Int, Opcode::ModuloInt, Type::Int);
            } break;
            case TokenKind::AndAssign: {
                success = EmitOperator2(Type::Int, Opcode::AndInt, Type::Int) ||
                          EmitOperator2(Type::Bool, Opcode::AndBool, Type::Bool);
            } break;
            case TokenKind::OrAssign: {
                success = EmitOperator2(Type::Int, Opcode::OrInt, Type::Int) ||
                          EmitOperator2(Type::Bool, Opcode::OrBool, Type::Bool);
            } break;
            case TokenKind::XorAssign: {
                success = EmitOperator2(Type::Int, Opcode::XorInt, Type::Int) ||
                          EmitOperator2(Type::Bool, Opcode::NotEqualBool, Type::Bool);
            } break;
            case TokenKind::LeftShiftAssign: {
                success = EmitOperator2(Type::Int, Opcode::LeftShiftInt, Type::Int);
            } break;
            case TokenKind::RightShiftAssign: {
                success = EmitOperator2(Type::Int, Opcode::RightShiftInt, Type::Int);
            } break;

            default: { RG_UNREACHABLE(); } break;
        }

        if (var->global && current_func) {
            switch (var->type) {
                case Type::Null: {} break;
                case Type::Bool: {
                    ir.Append({Opcode::StoreGlobalBool, {.i = var->offset}});
                    ir.Append({Opcode::LoadGlobalBool, {.i = var->offset}});
                } break;
                case Type::Int: {
                    ir.Append({Opcode::StoreGlobalInt, {.i = var->offset}});
                    ir.Append({Opcode::LoadGlobalInt, {.i = var->offset}});
                } break;
                case Type::Float: {
                    ir.Append({Opcode::StoreGlobalFloat, {.i = var->offset}});
                    ir.Append({Opcode::LoadGlobalFloat, {.i = var->offset}});
                } break;
                case Type::String: {
                    ir.Append({Opcode::StoreGlobalString, {.i = var->offset}});
                    ir.Append({Opcode::LoadGlobalString, {.i = var->offset}});
                } break;
            }
        } else {
            switch (var->type) {
                case Type::Null: {} break;
                case Type::Bool: { ir.Append({Opcode::CopyBool, {.i = var->offset}}); } break;
                case Type::Int: { ir.Append({Opcode::CopyInt, {.i = var->offset}}); } break;
                case Type::Float: { ir.Append({Opcode::CopyFloat, {.i = var->offset}}); } break;
                case Type::String: { ir.Append({Opcode::CopyString, {.i = var->offset}}); } break;
            }
        }
    } else { // Other operators
        switch (op.kind) {
            case TokenKind::Plus: {
                if (op.unary) {
                    success = stack[stack.len - 1].type == Type::Int ||
                              stack[stack.len - 1].type == Type::Float;
                } else {
                    success = EmitOperator2(Type::Int, Opcode::AddInt, Type::Int) ||
                              EmitOperator2(Type::Float, Opcode::AddFloat, Type::Float);
                }
            } break;
            case TokenKind::Minus: {
                if (op.unary) {
                    Instruction *inst = &ir[ir.len - 1];

                    switch (inst->code) {
                        case Opcode::PushInt: {
                            // In theory, this could overflow trying to negate INT64_MIN.. but we
                            // we can't have INT64_MIN, because numeric literal tokens are always
                            // positive. inst->u.i will flip between positive and negative values
                            // if we encounter successive '-' unary operators (e.g. -----64).
                            inst->u.i = -inst->u.i;
                            success = true;
                        } break;
                        case Opcode::PushFloat: {
                            inst->u.d = -inst->u.d;
                            success = true;
                        } break;
                        case Opcode::NegateInt:
                        case Opcode::NegateFloat: {
                            ir.len--;
                            success = true;
                        } break;

                        default: {
                            success = EmitOperator1(Type::Int, Opcode::NegateInt, Type::Int) ||
                                      EmitOperator1(Type::Float, Opcode::NegateFloat, Type::Float);
                        }
                    }
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
            case TokenKind::Complement: {
                success = EmitOperator1(Type::Int, Opcode::ComplementInt, Type::Int) ||
                          EmitOperator1(Type::Bool, Opcode::NotBool, Type::Bool);
            } break;
            case TokenKind::LeftShift: {
                success = EmitOperator2(Type::Int, Opcode::LeftShiftInt, Type::Int);
            } break;
            case TokenKind::RightShift: {
                success = EmitOperator2(Type::Int, Opcode::RightShiftInt, Type::Int);
            } break;

            case TokenKind::Not: {
                success = EmitOperator1(Type::Bool, Opcode::NotBool, Type::Bool);
            } break;
            case TokenKind::AndAnd: {
                success = EmitOperator2(Type::Bool, Opcode::AndBool, Type::Bool);

                RG_ASSERT(op.branch_idx && ir[op.branch_idx].code == Opcode::SkipIfFalse);
                ir[op.branch_idx].u.i = ir.len - op.branch_idx;
            } break;
            case TokenKind::OrOr: {
                success = EmitOperator2(Type::Bool, Opcode::OrBool, Type::Bool);

                RG_ASSERT(op.branch_idx && ir[op.branch_idx].code == Opcode::SkipIfTrue);
                ir[op.branch_idx].u.i = ir.len - op.branch_idx;
            } break;

            default: { RG_UNREACHABLE(); } break;
        }
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

bool ParserImpl::EmitOperator1(Type in_type, Opcode code, Type out_type)
{
    Type type = stack[stack.len - 1].type;

    if (type == in_type) {
        ir.Append({code});
        stack[stack.len - 1] = {out_type};

        return true;
    } else {
        return false;
    }
}

bool ParserImpl::EmitOperator2(Type in_type, Opcode code, Type out_type)
{
    Type type1 = stack[stack.len - 2].type;
    Type type2 = stack[stack.len - 1].type;

    if (type1 == in_type && type2 == in_type) {
        ir.Append({code});
        stack[--stack.len - 1] = {out_type};

        return true;
    } else {
        return false;
    }
}

void ParserImpl::EmitLoad(const VariableInfo &var)
{
    if (var.global && current_func) {
        if (RG_UNLIKELY(current_func->earliest_call_idx < var.defined_idx)) {
            MarkError(definitions_map.FindValue(current_func, -1), "Function '%1' may be called before variable '%2' exists",
                      current_func->name, var.name);
            HintError(current_func->earliest_call_pos, "Function call happens here (it could be indirect)");
            HintDefinition(&var, "Variable '%1' is defined here", var.name);
        }

        switch (var.type) {
            case Type::Null: {} break;
            case Type::Bool: { ir.Append({Opcode::LoadGlobalBool, {.i = var.offset}}); } break;
            case Type::Int: { ir.Append({Opcode::LoadGlobalInt, {.i = var.offset}}); } break;
            case Type::Float: { ir.Append({Opcode::LoadGlobalFloat, {.i = var.offset}});} break;
            case Type::String: { ir.Append({Opcode::LoadGlobalString, {.i = var.offset}}); } break;
        }
    } else {
        switch (var.type) {
            case Type::Null: {} break;
            case Type::Bool: { ir.Append({Opcode::LoadBool, {.i = var.offset}}); } break;
            case Type::Int: { ir.Append({Opcode::LoadInt, {.i = var.offset}}); } break;
            case Type::Float: { ir.Append({Opcode::LoadFloat, {.i = var.offset}});} break;
            case Type::String: { ir.Append({Opcode::LoadString, {.i = var.offset}}); } break;
        }
    }

    stack.Append({var.type, &var});
}

// Don't try to call from outside ParseExpression()!
bool ParserImpl::ParseCall(const char *name)
{
    // We only need to store types, but TestOverload() wants FunctionInfo::Parameter.
    LocalArray<FunctionInfo::Parameter, RG_LEN(FunctionInfo::params.data)> args;

    Size call_pos = pos - 2;

    FunctionInfo *func0 = program->functions_map.FindValue(name, nullptr);
    if (RG_UNLIKELY(!func0)) {
        MarkError(call_pos, "Function '%1' does not exist", name);
        return false;
    }

    if (!MatchToken(TokenKind::RightParenthesis)) {
        do {
            while (MatchToken(TokenKind::EndOfLine));

            if (RG_UNLIKELY(!args.Available())) {
                MarkError(pos, "Functions cannot take more than %1 arguments", RG_LEN(args.data));
                return false;
            }
            args.Append({nullptr, ParseExpression(true)});
        } while (MatchToken(TokenKind::Comma));

        while (MatchToken(TokenKind::EndOfLine));
        ConsumeToken(TokenKind::RightParenthesis);
    }

    FunctionInfo *func = func0;
    while (!TestOverload(*func, args)) {
        func = func->overload_next;

        if (RG_UNLIKELY(func == func0)) {
            LocalArray<char, 1024> buf = {};
            for (Size i = 0; i < args.len; i++) {
                buf.len += Fmt(buf.TakeAvailable(), "%1%2", i ? ", " : "", TypeNames[(int)args[i].type]).len;
            }

            MarkError(call_pos, "Cannot call '%1' with (%2) arguments", func0->name, buf);

            // Show all candidate functions with same name
            const FunctionInfo *it = func0;
            do {
                HintError(definitions_map.FindValue(it, -1), "Candidate '%1'", it->signature);
                it = it->overload_next;
            } while (it != func0);

            return false;
        }
    }

    if (func->intrinsic) {
        EmitIntrinsic(name, args);
    } else {
        if (func->inst_idx < 0) {
            forward_calls.Append({ir.len, func});

            if (current_func && current_func != func) {
                func->earliest_call_pos = std::min(func->earliest_call_pos, current_func->earliest_call_pos);
                func->earliest_call_idx = std::min(func->earliest_call_idx, current_func->earliest_call_idx);
            } else {
                func->earliest_call_pos = std::min(func->earliest_call_pos, call_pos);
                func->earliest_call_idx = std::min(func->earliest_call_idx, ir.len);
            }
        }

        ir.Append({Opcode::Call, {.i = func->inst_idx}});
    }
    stack.Append({func->ret_type});

    return true;
}

void ParserImpl::EmitIntrinsic(const char *name, Span<const FunctionInfo::Parameter> args)
{
    if (TestStr(name, "print") || TestStr(name, "printLn")) {
        RG_STATIC_ASSERT(RG_LEN(FunctionInfo::params.data) < 19);

        bool println = TestStr(name, "printLn");

        uint64_t payload = 0;
        int offset = 0;

        if (println) {
            ir.Append({Opcode::PushString, {.str = "\n"}});
            payload = (int)Type::String;
        }
        for (Size i = args.len - 1; i >= 0; i--) {
            payload = (payload << 3) | (int)args[i].type;
            offset += (args[i].type != Type::Null);
        }

        payload = (payload << 5) | (offset + println);
        payload = (payload << 5) | (args.len + println);

        ir.Append({Opcode::Print, {.payload = payload}});
    } else if (TestStr(name, "intToFloat")) {
        ir.Append({Opcode::IntToFloat});
    } else if (TestStr(name, "floatToInt")) {
        ir.Append({Opcode::FloatToInt});
    }
}

void ParserImpl::EmitPop(int64_t count)
{
    RG_ASSERT(count >= 0);

    if (count) {
        ir.Append({Opcode::Pop, {.i = count}});
    }
}

void ParserImpl::EmitReturn()
{
    RG_ASSERT(current_func);

    // We support tail recursion elimination (TRE)
    if (ir.len > 0 && ir[ir.len - 1].code == Opcode::Call &&
                              ir[ir.len - 1].u.i == current_func->inst_idx) {
        ir.len--;

        Size stack_offset = -2;
        for (Size i = current_func->params.len - 1; i >= 0; i--) {
            const FunctionInfo::Parameter &param = current_func->params[i];

            switch (param.type) {
                case Type::Null: {} break;
                case Type::Bool: { ir.Append({Opcode::StoreBool, {.i = --stack_offset}}); } break;
                case Type::Int: { ir.Append({Opcode::StoreInt, {.i = --stack_offset}}); } break;
                case Type::Float: { ir.Append({Opcode::StoreFloat, {.i = --stack_offset}}); } break;
                case Type::String: { ir.Append({Opcode::StoreString, {.i = --stack_offset}}); } break;
            }
        }

        EmitPop(var_offset);
        ir.Append({Opcode::Jump, {.i = current_func->inst_idx - ir.len}});

        current_func->tre = true;
    } else {
        if (var_offset > 0) {
            Size pop = var_offset - 1;

            switch (current_func->ret_type) {
                case Type::Null: { pop++; } break;
                case Type::Bool: { ir.Append({Opcode::StoreBool, {.i = 0}}); } break;
                case Type::Int: { ir.Append({Opcode::StoreInt, {.i = 0}}); } break;
                case Type::Float: { ir.Append({Opcode::StoreFloat, {.i = 0}}); } break;
                case Type::String: { ir.Append({Opcode::StoreString, {.i = 0}}); } break;
            }

            EmitPop(pop);
        }

        ir.Append({current_func->ret_type == Type::Null ? Opcode::ReturnNull : Opcode::Return, {.i = current_func->ret_pop}});
    }
}

void ParserImpl::DestroyVariables(Size count)
{
    Size first_idx = program->variables.len - count;

    for (Size i = program->variables.len - 1; i >= first_idx; i--) {
        const VariableInfo &var = program->variables[i];
        VariableInfo **ptr = program->variables_map.Find(var.name);

        if (var.shadow) {
            *ptr = (VariableInfo *)var.shadow;
        } else {
            program->variables_map.Remove(ptr);
        }

        poisons.Remove(&var);
    }

    program->variables.RemoveLast(count);
}

bool ParserImpl::TestOverload(const FunctionInfo &proto, Span<const FunctionInfo::Parameter> params2)
{
    if (proto.variadic) {
        if (proto.params.len > params2.len)
            return false;
    } else {
        if (proto.params.len != params2.len)
            return false;
    }

    for (Size i = 0; i < proto.params.len; i++) {
        if (proto.params[i].type != params2[i].type)
            return false;
    }

    return true;
}

bool ParserImpl::ConsumeToken(TokenKind kind)
{
    if (RG_UNLIKELY(pos >= tokens.len)) {
        if (out_report && valid) {
            out_report->unexpected_eof = true;
        }
        MarkError(pos, "Unexpected end of file, expected '%1'", TokenKindNames[(int)kind]);

        return false;
    }

    Size prev_pos = pos++;

    if (RG_UNLIKELY(tokens[prev_pos].kind != kind)) {
        MarkError(prev_pos, "Unexpected token '%1', expected '%2'",
                  TokenKindNames[(int)tokens[prev_pos].kind], TokenKindNames[(int)kind]);
        return false;
    }

    return true;
}

const char *ParserImpl::ConsumeIdentifier()
{
    if (RG_LIKELY(ConsumeToken(TokenKind::Identifier))) {
        return InternString(tokens[pos - 1].u.str);
    } else {
        return "";
    }
}

Type ParserImpl::ConsumeType()
{
    const char *type_name = ConsumeIdentifier();

    Type type;
    if (RG_LIKELY(OptionToEnum(TypeNames, type_name, &type))) {
        return type;
    } else {
        MarkError(pos - 1, "Type '%1' does not exist", type_name);
        return Type::Null;
    }
}

bool ParserImpl::MatchToken(TokenKind kind)
{
    bool match = pos < tokens.len && tokens[pos].kind == kind;
    pos += match;

    return match;
}

bool ParserImpl::PeekToken(TokenKind kind)
{
    bool match = pos < tokens.len && tokens[pos].kind == kind;
    return match;
}

const char *ParserImpl::InternString(const char *str)
{
    std::pair<const char **, bool> ret = strings.Append(str);
    if (ret.second) {
        *ret.first = DuplicateString(str, &program->str_alloc).ptr;
    }
    return *ret.first;
}

bool Parse(const TokenizedFile &file, Program *out_program)
{
    Parser parser(out_program);
    return parser.Parse(file);
}

}
