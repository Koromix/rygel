// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "error.hh"
#include "lexer.hh"
#include "parser.hh"
#include "program.hh"
#include "std.hh"

namespace RG {

struct PrototypeInfo {
    Size pos;

    FunctionInfo *func;
    Size body_pos;

    RG_HASHTABLE_HANDLER(PrototypeInfo, pos);
};

struct PendingOperator {
    TokenKind kind;
    int prec;
    bool unary;

    Size pos; // For error messages
    Size branch_idx; // Used for short-circuit operators
};

struct StackSlot {
    const TypeInfo *type;
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
    HashTable<Size, PrototypeInfo> prototypes_map;
    HashMap<const void *, Size> definitions_map;
    HashSet<const void *> poisoned_set;

    Size func_jump_idx = -1;
    FunctionInfo *current_func = nullptr;
    int depth = 0;

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

    // These 3 functions return true if (and only if) all code paths have a return statement.
    // For simplicity, return statements inside loops are not considered.
    bool ParseBlock();
    bool ParseStatement();
    bool ParseDo();

    void ParseFunction();
    void ParseReturn();
    void ParseLet();
    bool ParseIf();
    void ParseWhile();
    void ParseFor();

    void ParseBreak();
    void ParseContinue();

    const TypeInfo *ParseType();

    const TypeInfo *ParseExpression();
    void ProduceOperator(const PendingOperator &op);
    bool EmitOperator1(PrimitiveType in_primitive, Opcode code, const TypeInfo *out_type);
    bool EmitOperator2(PrimitiveType in_primitive, Opcode code, const TypeInfo *out_type);
    bool ParseCall(const char *name);
    void EmitIntrinsic(const char *name, Size call_idx, Span<const FunctionInfo::Parameter> args);
    void EmitLoad(const VariableInfo &var);
    bool ParseExpressionOfType(const TypeInfo *type);

    void DiscardResult();

    void EmitPop(int64_t count);
    void EmitReturn();
    void DestroyVariables(Size count);

    bool TestOverload(const FunctionInfo &proto, Span<const FunctionInfo::Parameter> params2);

    bool ConsumeToken(TokenKind kind);
    const char *ConsumeIdentifier();
    bool MatchToken(TokenKind kind);
    bool PeekToken(TokenKind kind);

    bool SkipNewLines();

    const char *InternString(const char *str);

    inline const TypeInfo *GetBasicType(PrimitiveType primitive)
    {
        return &program->types[(int)primitive];
    }

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

    // Basic types
    for (Size i = 0; i < RG_LEN(PrimitiveTypeNames); i++) {
        TypeInfo *type = program->types.Append({PrimitiveTypeNames[i], (PrimitiveType)i});
        program->types_map.Append(type);
    }

    // Intrinsics
    AddFunction("intToFloat(Int): Float", {});
    AddFunction("floatToInt(Float): Int", {});
    AddFunction("typeOf(...): Type", {});
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

    prototypes_map.Clear();
    definitions_map.Clear();
    poisoned_set.Clear();

    // The caller may want to execute the code and then compile new code (e.g. REPL),
    // we can't ever try to reuse a jump that will not be executed!
    func_jump_idx = -1;

    src = program->sources.AppendDefault();
    src->filename = DuplicateString(file.filename, &program->str_alloc).ptr;

    // We want top-level order-independent functions
    ParsePrototypes(file.funcs);

    // Do the actual parsing!
    while (RG_LIKELY(pos < tokens.len)) {
        ParseStatement();
    }

    // Maybe it'll help catch bugs
    RG_ASSERT(!depth);
    RG_ASSERT(loop_var_offset == -1);
    RG_ASSERT(!current_func);

    ir.Append({Opcode::End});
    program->end_stack_len = var_offset;

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

    char *ptr = DuplicateString(signature, &program->str_alloc).ptr;
    func->signature = ptr;

    // Name
    {
        Size len = strcspn(ptr, "()");
        Span<const char> func_name = TrimStr(MakeSpan(ptr, len));

        func->name = DuplicateString(func_name, &program->str_alloc).ptr;
        ptr += len;
    }

    func->mode = native ? FunctionInfo::Mode::Native : FunctionInfo::Mode::Intrinsic;
    func->native = native;

    // Parameters
    RG_ASSERT(ptr[0] == '(');
    if (ptr[1] != ')') {
        do {
            Size len = strcspn(++ptr, ",)");
            RG_ASSERT(ptr[len]);

            char c = ptr[len];
            ptr[len] = 0;

            if (TestStr(ptr, "...")) {
                RG_ASSERT(c == ')');
                func->variadic = true;
            } else {
                const TypeInfo *type = program->types_map.FindValue(ptr, nullptr);
                RG_ASSERT(type);

                func->params.Append({"", type});
            }

            ptr[len] = c;
            ptr += len;
        } while ((ptr++)[0] != ')');
    } else {
        ptr += 2;
    }

    // Return type
    if (ptr[0] == ':') {
        RG_ASSERT(ptr[1] == ' ');

        func->ret_type = program->types_map.FindValue(ptr + 2, nullptr);
        RG_ASSERT(func->ret_type);
    } else {
        func->ret_type = GetBasicType(PrimitiveType::Null);
    }

    if (native) {
        // Jump over consecutively defined functions in one go
        if (func_jump_idx >= 0 && ir[func_jump_idx].u.i == ir.len - func_jump_idx) {
            ir[func_jump_idx].u.i++;
        } else {
            func_jump_idx = ir.len;
            ir.Append({Opcode::Jump, {.i = 2}});
        }

        func->inst_idx = ir.len;
        ir.Append({Opcode::Nop});
    }

    // Publish it!
    {
        FunctionInfo *head = *program->functions_map.Append(func).first;

        if (head != func) {
            RG_ASSERT(!head->variadic && !func->variadic);

            head->overload_prev->overload_next = func;
            func->overload_next = head;
            func->overload_prev = head->overload_prev;
            head->overload_prev = func;

#ifndef NDEBUG
            do {
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
    RG_ASSERT(!prototypes_map.count);
    RG_ASSERT(pos == 0);
    RG_ASSERT(!src->lines.len);

    // This is a preparse step, clean up main side effets
    RG_DEFER_C(ir_len = ir.len,
               lines_len = src->lines.len) {
        pos = 0;

        ir.RemoveFrom(ir_len);
        src->lines.RemoveFrom(lines_len);
    };

    for (Size i = 0; i < funcs.len; i++) {
        pos = funcs[i] + 1;
        show_errors = true;

        PrototypeInfo *proto = prototypes_map.SetDefault(pos);
        FunctionInfo *func = program->functions.AppendDefault();
        definitions_map.Append(func, pos);

        proto->pos = pos;
        proto->func = func;

        func->name = ConsumeIdentifier();
        func->mode = FunctionInfo::Mode::Blik;

        // Insert in functions map
        {
            std::pair<FunctionInfo **, bool> ret = program->functions_map.Append(func);
            FunctionInfo *proto0 = *ret.first;

            if (ret.second) {
                proto0->overload_prev = proto0;
                proto0->overload_next = proto0;
            } else {
                proto0->overload_prev->overload_next = func;
                func->overload_next = proto0;
                func->overload_prev = proto0->overload_prev;
                proto0->overload_prev = func;
            }
        }

        // Clean up parameter variables once we're done
        RG_DEFER_C(variables_len = program->variables.len) {
            DestroyVariables(program->variables.len - variables_len);
        };

        // Parameters
        ConsumeToken(TokenKind::LeftParenthesis);
        if (!MatchToken(TokenKind::RightParenthesis)) {
            do {
                SkipNewLines();

                VariableInfo *var = program->variables.AppendDefault();
                Size param_pos = pos;

                var->mut = MatchToken(TokenKind::Mut);
                var->name = ConsumeIdentifier();

                std::pair<VariableInfo **, bool> ret = program->variables_map.Append(var);
                if (RG_UNLIKELY(!ret.second)) {
                    const VariableInfo *prev_var = *ret.first;
                    var->shadow = prev_var;

                    // Error messages for globals are issued in ParseFunction(), because at
                    // this stage some globals (those issues from the same Parse call) may not
                    // yet exist. Better issue all errors about that later.
                    if (!prev_var->global) {
                        MarkError(param_pos, "Parameter named '%1' already exists", var->name);
                    }
                }

                ConsumeToken(TokenKind::Colon);
                var->type = ParseType();

                if (RG_LIKELY(func->params.Available())) {
                    FunctionInfo::Parameter *param = func->params.AppendDefault();
                    definitions_map.Append(param, param_pos);

                    param->name = var->name;
                    param->type = var->type;
                    param->mut = var->mut;
                } else {
                    MarkError(pos - 1, "Functions cannot have more than %1 parameters", RG_LEN(func->params.data));
                }
            } while (MatchToken(TokenKind::Comma));

            SkipNewLines();
            ConsumeToken(TokenKind::RightParenthesis);
        }

        // Return type
        if (MatchToken(TokenKind::Colon)) {
            func->ret_type = ParseType();
        } else {
            func->ret_type = GetBasicType(PrimitiveType::Null);
        }

        // Build signature (with parameter and return types)
        {
            HeapArray<char> buf(&program->str_alloc);

            Fmt(&buf, "%1(", func->name);
            for (Size i = 0; i < func->params.len; i++) {
                const FunctionInfo::Parameter &param = func->params[i];
                Fmt(&buf, "%1%2", i ? ", " : "", param.type->signature);
            }
            Fmt(&buf, ")");
            if (func->ret_type != GetBasicType(PrimitiveType::Null)) {
                Fmt(&buf, ": %1", func->ret_type->signature);
            }

            func->signature = buf.TrimAndLeak(1).ptr;
        }

        proto->body_pos = pos;

        // We don't know where it will live yet!
        func->inst_idx = -1;
        func->earliest_call_pos = RG_SIZE_MAX;
        func->earliest_call_idx = RG_SIZE_MAX;
    }
}

bool ParserImpl::ParseBlock()
{
    show_errors = true;
    depth++;

    RG_DEFER_C(prev_offset = var_offset,
               variables_len = program->variables.len) {
        depth--;

        EmitPop(var_offset - prev_offset);
        DestroyVariables(program->variables.len - variables_len);
        var_offset = prev_offset;
    };

    bool has_return = false;

    while (RG_LIKELY(pos < tokens.len) && tokens[pos].kind != TokenKind::Else &&
                                          tokens[pos].kind != TokenKind::End) {
        has_return |= ParseStatement();
    }

    return has_return;
}

bool ParserImpl::ParseStatement()
{
    bool has_return = false;

    src->lines.Append({ir.len, tokens[pos].line});

    switch (tokens[pos].kind) {
        case TokenKind::EndOfLine: { pos++; } break;

        case TokenKind::Begin: {
            pos++;

            if (RG_LIKELY(ConsumeToken(TokenKind::EndOfLine))) {
                has_return = ParseBlock();
                ConsumeToken(TokenKind::End);

                show_errors |= ConsumeToken(TokenKind::EndOfLine);
            }
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
            has_return = ParseIf();
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
            ParseExpression();
            DiscardResult();

            show_errors |= ConsumeToken(TokenKind::EndOfLine);
        } break;
    }

    return has_return;
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
        ParseExpression();
        DiscardResult();

        return false;
    }
}

void ParserImpl::ParseFunction()
{
    Size func_pos = ++pos;

    const PrototypeInfo *proto = prototypes_map.Find(func_pos);
    RG_ASSERT(proto);

    FunctionInfo *func = proto->func;

    RG_DEFER_C(prev_func = current_func,
               prev_offset = var_offset) {
        // Variables inside the function are destroyed at the end of the block.
        // This destroys the parameters.
        DestroyVariables(func->params.len);
        var_offset = prev_offset;

        current_func = prev_func;
    };

    // Do safety checks we could not do in ParsePrototypes()
    if (RG_UNLIKELY(current_func)) {
        MarkError(func_pos, "Nested functions are not supported");
        HintError(definitions_map.FindValue(current_func, -1), "Previous function was started here and is still open");
    } else if (RG_UNLIKELY(depth)) {
        MarkError(func_pos, "Functions must be defined in top-level scope");
    }
    current_func = func;

    // Create parameter variables
    {
        Size stack_offset = -2 - func->params.len;

        for (const FunctionInfo::Parameter &param: func->params) {
            VariableInfo *var = program->variables.AppendDefault();
            Size param_pos = definitions_map.FindValue(&param, -1);
            definitions_map.Append(var, param_pos);

            var->name = param.name;
            var->type = param.type;
            var->mut = param.mut;

            var->offset = stack_offset++;

            std::pair<VariableInfo **, bool> ret = program->variables_map.Append(var);
            if (RG_UNLIKELY(!ret.second)) {
                const VariableInfo *prev_var = *ret.first;
                var->shadow = prev_var;

                // We should already have issued an error message for the other case (duplicate
                // parameter name) in ParsePrototypes().
                if (prev_var->global) {
                    MarkError(param_pos, "Parameter '%1' is not allowed to hide global variable", var->name);
                    HintDefinition(prev_var, "Global variable '%1' is defined here", prev_var->name);
                } else {
                    valid = false;
                }
            }

            if (RG_UNLIKELY(poisoned_set.Find(&param))) {
                poisoned_set.Append(var);
            }
        }
    }

    // Check for incompatible function overloadings
    {
        FunctionInfo *overload = program->functions_map.FindValue(func->name, nullptr);

        while (overload != func) {
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

    // Skip prototype
    pos = proto->body_pos;

    // Jump over consecutively defined functions in one go
    if (func_jump_idx < 0 || ir[func_jump_idx].u.i < ir.len - func_jump_idx) {
        func_jump_idx = ir.len;
        ir.Append({Opcode::Jump});
    }

    func->inst_idx = ir.len;
    var_offset = 0;

    // Function body
    bool has_return = false;
    if (PeekToken(TokenKind::Do)) {
        has_return = ParseDo();
    } else if (RG_LIKELY(ConsumeToken(TokenKind::EndOfLine))) {
        has_return = ParseBlock();
        ConsumeToken(TokenKind::End);
    }

    if (!has_return) {
        if (func->ret_type == GetBasicType(PrimitiveType::Null)) {
            ir.Append({Opcode::PushNull});
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

    const TypeInfo *type;
    if (PeekToken(TokenKind::EndOfLine)) {
        type = GetBasicType(PrimitiveType::Null);
    } else {
        type = ParseExpression();
    }

    if (RG_UNLIKELY(type != current_func->ret_type)) {
        MarkError(return_pos, "Cannot return %1 value in function defined to return %2",
                  type->signature, current_func->ret_type->signature);
        return;
    }

    EmitReturn();
}

void ParserImpl::ParseLet()
{
    Size var_pos = ++pos;

    VariableInfo *var = program->variables.AppendDefault();

    var->mut = MatchToken(TokenKind::Mut);
    var_pos += var->mut;
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
        SkipNewLines();

        var->type = ParseExpression();
    } else {
        ConsumeToken(TokenKind::Colon);

        // Don't assign to var->type yet, so that ParseExpression() knows it
        // cannot use this variable.
        const TypeInfo *type = ParseType();

        if (MatchToken(TokenKind::Assign)) {
            SkipNewLines();

            Size expr_pos = pos;
            const TypeInfo *type2 = ParseExpression();

            if (RG_UNLIKELY(type2 != type)) {
                MarkError(expr_pos - 1, "Cannot assign %1 value to variable '%2' (defined as %3)",
                          type2->signature, var->name, type->signature);
            }
        } else {
            switch (type->primitive) {
                case PrimitiveType::Null: { ir.Append({Opcode::PushNull}); } break;
                case PrimitiveType::Bool: { ir.Append({Opcode::PushBool, {.b = false}}); } break;
                case PrimitiveType::Int: { ir.Append({Opcode::PushInt, {.i = 0}}); } break;
                case PrimitiveType::Float: { ir.Append({Opcode::PushFloat, {.d = 0.0}}); } break;
                case PrimitiveType::String: { ir.Append({Opcode::PushString, {.str = ""}}); } break;
                case PrimitiveType::Type: { ir.Append({Opcode::PushType, {.type = GetBasicType(PrimitiveType::Null)}}); } break;
            }
        }

        var->type = type;
    }

    var->global = !current_func;
    var->offset = var_offset++;
    var->defined_idx = ir.len;

    // Expressions involving this variable won't issue (visible) errors
    // and will be marked as invalid too.
    if (RG_UNLIKELY(!show_errors)) {
        poisoned_set.Append(var);
    }
}

bool ParserImpl::ParseIf()
{
    pos++;

    ParseExpressionOfType(GetBasicType(PrimitiveType::Bool));

    Size branch_idx = ir.len;
    ir.Append({Opcode::BranchIfFalse});

    bool has_return = true;
    bool has_else = false;

    if (PeekToken(TokenKind::Then)) {
        has_return &= ParseDo();
        ir[branch_idx].u.i = ir.len - branch_idx;
    } else if (RG_LIKELY(ConsumeToken(TokenKind::EndOfLine))) {
        has_return &= ParseBlock();

        if (MatchToken(TokenKind::Else)) {
            HeapArray<Size> jumps;

            jumps.Append(ir.len);
            ir.Append({Opcode::Jump});

            do {
                ir[branch_idx].u.i = ir.len - branch_idx;

                if (MatchToken(TokenKind::If)) {
                    ParseExpressionOfType(GetBasicType(PrimitiveType::Bool));

                    if (RG_LIKELY(ConsumeToken(TokenKind::EndOfLine))) {
                        branch_idx = ir.len;
                        ir.Append({Opcode::BranchIfFalse});

                        has_return &= ParseBlock();

                        jumps.Append(ir.len);
                        ir.Append({Opcode::Jump});
                    }
                } else if (RG_LIKELY(ConsumeToken(TokenKind::EndOfLine))) {
                    has_return &= ParseBlock();
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
    pos++;

    // Parse expression. We'll make a copy after the loop body so that the IR code looks
    // roughly like if (cond) { do { ... } while (cond) }.
    Size condition_idx = ir.len;
    Size condition_line_idx = src->lines.len;
    ParseExpressionOfType(GetBasicType(PrimitiveType::Bool));

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
    } else if (RG_LIKELY(ConsumeToken(TokenKind::EndOfLine))) {
        ParseBlock();
        ConsumeToken(TokenKind::End);
    }

    // Fix up continue jumps
    for (Size i = first_continue_idx; i < loop_continues.len; i++) {
        Size jump_idx = loop_continues[i];
        ir[jump_idx].u.i = ir.len - jump_idx;
    }

    // Copy the condition expression, and the IR/line map information
    for (Size i = condition_line_idx; i < src->lines.len &&
                                      src->lines[i].first_idx < branch_idx; i++) {
        const SourceInfo::Line &line = src->lines[i];
        src->lines.Append({ir.len + (line.first_idx - condition_idx), line.line});
    }
    ir.Append(ir.Take(condition_idx, branch_idx - condition_idx));

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

    it->mut = MatchToken(TokenKind::Mut);
    for_pos += it->mut;
    definitions_map.Append(it, pos);
    it->name = ConsumeIdentifier();

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

    bool inclusive;
    ConsumeToken(TokenKind::In);
    ParseExpressionOfType(GetBasicType(PrimitiveType::Int));
    if (MatchToken(TokenKind::DotDotDot)) {
        inclusive = false;
    } else {
        ConsumeToken(TokenKind::DotDot);
        inclusive = true;
    }
    ParseExpressionOfType(GetBasicType(PrimitiveType::Int));

    // Make sure start and end value remain on the stack
    var_offset += 3;

    // Put iterator value on the stack
    ir.Append({Opcode::LoadInt, {.i = it->offset - 2}});
    it->type = GetBasicType(PrimitiveType::Int);

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
    } else if (RG_LIKELY(ConsumeToken(TokenKind::EndOfLine))) {
        ParseBlock();
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

const TypeInfo *ParserImpl::ParseType()
{
    Size type_pos = pos;

    // Parse type expression
    {
        const TypeInfo *type = ParseExpression();

        if (RG_UNLIKELY(type != GetBasicType(PrimitiveType::Type))) {
            MarkError(type_pos, "Expected a Type expression, not %1", type->signature);
            return GetBasicType(PrimitiveType::Null);
        }
    }

    // Once we start to implement constant folding and CTFE, more complex expressions
    // should work without any change here.
    if (RG_UNLIKELY(ir[ir.len - 1].code != Opcode::PushType)) {
        MarkError(type_pos, "Complex type expression cannot be resolved statically");
        return GetBasicType(PrimitiveType::Null);
    }

    ir.len--;
    return ir.ptr[ir.len].u.type;
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
        case TokenKind::LeftRotateAssign:
        case TokenKind::RightRotateAssign:
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
        case TokenKind::RightShift:
        case TokenKind::LeftRotate:
        case TokenKind::RightRotate: { return 9; } break;
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

const TypeInfo *ParserImpl::ParseExpression()
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
                case TokenKind::Null: {
                    ir.Append({Opcode::PushNull});
                    stack.Append({GetBasicType(PrimitiveType::Null)});
                } break;
                case TokenKind::Bool: {
                    ir.Append({Opcode::PushBool, {.b = tok.u.b}});
                    stack.Append({GetBasicType(PrimitiveType::Bool)});
                } break;
                case TokenKind::Integer: {
                    ir.Append({Opcode::PushInt, {.i = tok.u.i}});
                    stack.Append({GetBasicType(PrimitiveType::Int)});
                } break;
                case TokenKind::Float: {
                    ir.Append({Opcode::PushFloat, {.d = tok.u.d}});
                    stack.Append({GetBasicType(PrimitiveType::Float)});
                } break;
                case TokenKind::String: {
                    ir.Append({Opcode::PushString, {.str = InternString(tok.u.str)}});
                    stack.Append({GetBasicType(PrimitiveType::String)});
                } break;

                case TokenKind::Identifier: {
                    if (MatchToken(TokenKind::LeftParenthesis)) {
                        if (RG_UNLIKELY(!ParseCall(tok.u.str)))
                            goto error;
                    } else {
                        const VariableInfo *var = program->variables_map.FindValue(tok.u.str, nullptr);

                        if (RG_LIKELY(var)) {
                            show_errors &= !poisoned_set.Find(var);

                            if (RG_LIKELY(var->type)) {
                                EmitLoad(*var);
                            } else {
                                MarkError(pos - 1, "Cannot use variable '%1' before it is defined", var->name);
                                goto error;
                            }
                        } else {
                            const TypeInfo *type = program->types_map.FindValue(tok.u.str, nullptr);

                            if (RG_UNLIKELY(!type)) {
                                MarkError(pos - 1, "Reference to unknown identifier '%1'", tok.u.str);
                                goto error;
                            }

                            ir.Append({Opcode::PushType, {.type = type}});
                            stack.Append({GetBasicType(PrimitiveType::Type)});
                        }
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
            if (op.prec < 0) {
                if (RG_UNLIKELY(pos == prev_offset + 1)) {
                    MarkError(pos - 1, "Unexpected token '%1', expected value or expression",
                              TokenKindNames[(int)tokens[pos - 1].kind]);
                    goto error;
                } else if (RG_UNLIKELY(expect_value || parentheses)) {
                    pos--;
                    if (RG_LIKELY(SkipNewLines())) {
                        continue;
                    } else {
                        pos++;
                        goto unexpected;
                    }
                } else {
                    pos--;
                    break;
                }
            }

            if (expect_value != op.unary) {
                if (RG_LIKELY(tok.kind == TokenKind::Plus || tok.kind == TokenKind::Minus)) {
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
                return GetBasicType(PrimitiveType::Null);
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
    return RG_LIKELY(stack.len) ? stack[stack.len - 1].type : GetBasicType(PrimitiveType::Null);

unexpected:
    pos--;
    {
        const char *expected;
        if (expect_value) {
            expected = "value or '('";
        } else if (parentheses) {
            expected = "operator or ')'";
        } else {
            expected = "operator or end of expression";
        }

        MarkError(pos, "Unexpected token '%1', expected %2", TokenKindNames[(int)tokens[pos].kind], expected);
    }
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
    return GetBasicType(PrimitiveType::Null);
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
        if (RG_UNLIKELY(!var->mut)) {
            MarkError(op.pos, "Cannot assign result to non-mutable variable '%1'", var->name);
            HintError(definitions_map.FindValue(var, -1), "Variable '%1' is defined without 'mut' qualifier", var->name);

            return;
        }
        if (RG_UNLIKELY(var->type != expr.type)) {
            MarkError(op.pos, "Cannot assign %1 value to variable '%2'",
                      expr.type->signature, var->name);
            HintError(definitions_map.FindValue(var, -1), "Variable '%1' is defined as %2",
                      var->name, var->type->signature);
            return;
        }

        switch (op.kind) {
            case TokenKind::Reassign: {
                stack[--stack.len - 1].var = nullptr;
                success = true;
            } break;

            case TokenKind::PlusAssign: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::AddInt, stack[stack.len - 2].type) ||
                          EmitOperator2(PrimitiveType::Float, Opcode::AddFloat, stack[stack.len - 2].type);
            } break;
            case TokenKind::MinusAssign: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::SubstractInt, stack[stack.len - 2].type) ||
                          EmitOperator2(PrimitiveType::Float, Opcode::SubstractFloat, stack[stack.len - 2].type);
            } break;
            case TokenKind::MultiplyAssign: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::MultiplyInt, stack[stack.len - 2].type) ||
                          EmitOperator2(PrimitiveType::Float, Opcode::MultiplyFloat, stack[stack.len - 2].type);
            } break;
            case TokenKind::DivideAssign: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::DivideInt, stack[stack.len - 2].type) ||
                          EmitOperator2(PrimitiveType::Float, Opcode::DivideFloat, stack[stack.len - 2].type);
            } break;
            case TokenKind::ModuloAssign: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::ModuloInt, stack[stack.len - 2].type);
            } break;
            case TokenKind::AndAssign: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::AndInt, stack[stack.len - 2].type) ||
                          EmitOperator2(PrimitiveType::Bool, Opcode::AndBool, stack[stack.len - 2].type);
            } break;
            case TokenKind::OrAssign: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::OrInt, stack[stack.len - 2].type) ||
                          EmitOperator2(PrimitiveType::Bool, Opcode::OrBool, stack[stack.len - 2].type);
            } break;
            case TokenKind::XorAssign: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::XorInt, stack[stack.len - 2].type) ||
                          EmitOperator2(PrimitiveType::Bool, Opcode::NotEqualBool, stack[stack.len - 2].type);
            } break;
            case TokenKind::LeftShiftAssign: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::LeftShiftInt, stack[stack.len - 2].type);
            } break;
            case TokenKind::RightShiftAssign: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::RightShiftInt, stack[stack.len - 2].type);
            } break;
            case TokenKind::LeftRotateAssign: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::LeftRotateInt, stack[stack.len - 2].type);
            } break;
            case TokenKind::RightRotateAssign: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::RightRotateInt, stack[stack.len - 2].type);
            } break;

            default: { RG_UNREACHABLE(); } break;
        }

        if (var->global && current_func) {
            switch (var->type->primitive) {
                case PrimitiveType::Null: {} break;
                case PrimitiveType::Bool: {
                    ir.Append({Opcode::StoreGlobalBool, {.i = var->offset}});
                    ir.Append({Opcode::LoadGlobalBool, {.i = var->offset}});
                } break;
                case PrimitiveType::Int: {
                    ir.Append({Opcode::StoreGlobalInt, {.i = var->offset}});
                    ir.Append({Opcode::LoadGlobalInt, {.i = var->offset}});
                } break;
                case PrimitiveType::Float: {
                    ir.Append({Opcode::StoreGlobalFloat, {.i = var->offset}});
                    ir.Append({Opcode::LoadGlobalFloat, {.i = var->offset}});
                } break;
                case PrimitiveType::String: {
                    ir.Append({Opcode::StoreGlobalString, {.i = var->offset}});
                    ir.Append({Opcode::LoadGlobalString, {.i = var->offset}});
                } break;
                case PrimitiveType::Type: {
                    ir.Append({Opcode::StoreGlobalType, {.i = var->offset}});
                    ir.Append({Opcode::LoadGlobalType, {.i = var->offset}});
                } break;
            }
        } else {
            switch (var->type->primitive) {
                case PrimitiveType::Null: {} break;
                case PrimitiveType::Bool: { ir.Append({Opcode::CopyBool, {.i = var->offset}}); } break;
                case PrimitiveType::Int: { ir.Append({Opcode::CopyInt, {.i = var->offset}}); } break;
                case PrimitiveType::Float: { ir.Append({Opcode::CopyFloat, {.i = var->offset}}); } break;
                case PrimitiveType::String: { ir.Append({Opcode::CopyString, {.i = var->offset}}); } break;
                case PrimitiveType::Type: { ir.Append({Opcode::CopyType, {.i = var->offset}}); } break;
            }
        }
    } else { // Other operators
        switch (op.kind) {
            case TokenKind::Plus: {
                if (op.unary) {
                    success = stack[stack.len - 1].type->primitive == PrimitiveType::Int ||
                              stack[stack.len - 1].type->primitive == PrimitiveType::Float;
                } else {
                    success = EmitOperator2(PrimitiveType::Int, Opcode::AddInt, stack[stack.len - 2].type) ||
                              EmitOperator2(PrimitiveType::Float, Opcode::AddFloat, stack[stack.len - 2].type);
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
                            success = EmitOperator1(PrimitiveType::Int, Opcode::NegateInt, stack[stack.len - 1].type) ||
                                      EmitOperator1(PrimitiveType::Float, Opcode::NegateFloat, stack[stack.len - 1].type);
                        }
                    }
                } else {
                    success = EmitOperator2(PrimitiveType::Int, Opcode::SubstractInt, stack[stack.len - 2].type) ||
                              EmitOperator2(PrimitiveType::Float, Opcode::SubstractFloat, stack[stack.len - 2].type);
                }
            } break;
            case TokenKind::Multiply: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::MultiplyInt, stack[stack.len - 2].type) ||
                          EmitOperator2(PrimitiveType::Float, Opcode::MultiplyFloat, stack[stack.len - 2].type);
            } break;
            case TokenKind::Divide: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::DivideInt, stack[stack.len - 2].type) ||
                          EmitOperator2(PrimitiveType::Float, Opcode::DivideFloat, stack[stack.len - 2].type);
            } break;
            case TokenKind::Modulo: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::ModuloInt, stack[stack.len - 2].type);
            } break;

            case TokenKind::Equal: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::EqualInt, GetBasicType(PrimitiveType::Bool)) ||
                          EmitOperator2(PrimitiveType::Float, Opcode::EqualFloat, GetBasicType(PrimitiveType::Bool)) ||
                          EmitOperator2(PrimitiveType::Bool, Opcode::EqualBool, GetBasicType(PrimitiveType::Bool)) ||
                          EmitOperator2(PrimitiveType::Type, Opcode::EqualType, GetBasicType(PrimitiveType::Bool));
            } break;
            case TokenKind::NotEqual: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::NotEqualInt, GetBasicType(PrimitiveType::Bool)) ||
                          EmitOperator2(PrimitiveType::Float, Opcode::NotEqualFloat, GetBasicType(PrimitiveType::Bool)) ||
                          EmitOperator2(PrimitiveType::Bool, Opcode::NotEqualBool, GetBasicType(PrimitiveType::Bool)) ||
                          EmitOperator2(PrimitiveType::Type, Opcode::NotEqualType, GetBasicType(PrimitiveType::Bool));
            } break;
            case TokenKind::Greater: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::GreaterThanInt, GetBasicType(PrimitiveType::Bool)) ||
                          EmitOperator2(PrimitiveType::Float, Opcode::GreaterThanFloat, GetBasicType(PrimitiveType::Bool));
            } break;
            case TokenKind::GreaterOrEqual: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::GreaterOrEqualInt, GetBasicType(PrimitiveType::Bool)) ||
                          EmitOperator2(PrimitiveType::Float, Opcode::GreaterOrEqualFloat, GetBasicType(PrimitiveType::Bool));
            } break;
            case TokenKind::Less: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::LessThanInt, GetBasicType(PrimitiveType::Bool)) ||
                          EmitOperator2(PrimitiveType::Float, Opcode::LessThanFloat, GetBasicType(PrimitiveType::Bool));
            } break;
            case TokenKind::LessOrEqual: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::LessOrEqualInt, GetBasicType(PrimitiveType::Bool)) ||
                          EmitOperator2(PrimitiveType::Float, Opcode::LessOrEqualFloat, GetBasicType(PrimitiveType::Bool));
            } break;

            case TokenKind::And: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::AndInt, stack[stack.len - 2].type) ||
                          EmitOperator2(PrimitiveType::Bool, Opcode::AndBool, stack[stack.len - 2].type);
            } break;
            case TokenKind::Or: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::OrInt, stack[stack.len - 2].type) ||
                          EmitOperator2(PrimitiveType::Bool, Opcode::OrBool, stack[stack.len - 2].type);
            } break;
            case TokenKind::Xor: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::XorInt, stack[stack.len - 2].type) ||
                          EmitOperator2(PrimitiveType::Bool, Opcode::NotEqualBool, stack[stack.len - 2].type);
            } break;
            case TokenKind::Complement: {
                success = EmitOperator1(PrimitiveType::Int, Opcode::ComplementInt, stack[stack.len - 1].type) ||
                          EmitOperator1(PrimitiveType::Bool, Opcode::NotBool, stack[stack.len - 1].type);
            } break;
            case TokenKind::LeftShift: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::LeftShiftInt, stack[stack.len - 2].type);
            } break;
            case TokenKind::RightShift: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::RightShiftInt, stack[stack.len - 2].type);
            } break;
            case TokenKind::LeftRotate: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::LeftRotateInt, stack[stack.len - 2].type);
            } break;
            case TokenKind::RightRotate: {
                success = EmitOperator2(PrimitiveType::Int, Opcode::RightRotateInt, stack[stack.len - 2].type);
            } break;

            case TokenKind::Not: {
                success = EmitOperator1(PrimitiveType::Bool, Opcode::NotBool, stack[stack.len - 1].type);
            } break;
            case TokenKind::AndAnd: {
                success = EmitOperator2(PrimitiveType::Bool, Opcode::AndBool, stack[stack.len - 2].type);

                RG_ASSERT(op.branch_idx && ir[op.branch_idx].code == Opcode::SkipIfFalse);
                ir[op.branch_idx].u.i = ir.len - op.branch_idx;
            } break;
            case TokenKind::OrOr: {
                success = EmitOperator2(PrimitiveType::Bool, Opcode::OrBool, stack[stack.len - 2].type);

                RG_ASSERT(op.branch_idx && ir[op.branch_idx].code == Opcode::SkipIfTrue);
                ir[op.branch_idx].u.i = ir.len - op.branch_idx;
            } break;

            default: { RG_UNREACHABLE(); } break;
        }
    }

    if (RG_UNLIKELY(!success)) {
        if (op.unary) {
            MarkError(op.pos, "Cannot use '%1' operator on %2 value",
                      TokenKindNames[(int)op.kind], stack[stack.len - 1].type->signature);
        } else if (stack[stack.len - 2].type == stack[stack.len - 1].type) {
            MarkError(op.pos, "Cannot use '%1' operator on %2 values",
                      TokenKindNames[(int)op.kind], stack[stack.len - 2].type->signature);
        } else {
            MarkError(op.pos, "Cannot use '%1' operator on %2 and %3 values",
                      TokenKindNames[(int)op.kind], stack[stack.len - 2].type->signature,
                      stack[stack.len - 1].type->signature);
        }
    }
}

bool ParserImpl::EmitOperator1(PrimitiveType in_primitive, Opcode code, const TypeInfo *out_type)
{
    const TypeInfo *type = stack[stack.len - 1].type;

    if (type->primitive == in_primitive) {
        ir.Append({code});
        stack[stack.len - 1] = {out_type};

        return true;
    } else {
        return false;
    }
}

bool ParserImpl::EmitOperator2(PrimitiveType in_primitive, Opcode code, const TypeInfo *out_type)
{
    const TypeInfo *type1 = stack[stack.len - 2].type;
    const TypeInfo *type2 = stack[stack.len - 1].type;

    if (type1->primitive == in_primitive && type2->primitive == in_primitive && type1 == type2) {
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

        switch (var.type->primitive) {
            case PrimitiveType::Null: { ir.Append({Opcode::PushNull}); } break;
            case PrimitiveType::Bool: { ir.Append({Opcode::LoadGlobalBool, {.i = var.offset}}); } break;
            case PrimitiveType::Int: { ir.Append({Opcode::LoadGlobalInt, {.i = var.offset}}); } break;
            case PrimitiveType::Float: { ir.Append({Opcode::LoadGlobalFloat, {.i = var.offset}});} break;
            case PrimitiveType::String: { ir.Append({Opcode::LoadGlobalString, {.i = var.offset}}); } break;
            case PrimitiveType::Type: { ir.Append({Opcode::LoadGlobalType, {.i = var.offset}}); } break;
        }
    } else {
        switch (var.type->primitive) {
            case PrimitiveType::Null: { ir.Append({Opcode::PushNull}); } break;
            case PrimitiveType::Bool: { ir.Append({Opcode::LoadBool, {.i = var.offset}}); } break;
            case PrimitiveType::Int: { ir.Append({Opcode::LoadInt, {.i = var.offset}}); } break;
            case PrimitiveType::Float: { ir.Append({Opcode::LoadFloat, {.i = var.offset}});} break;
            case PrimitiveType::String: { ir.Append({Opcode::LoadString, {.i = var.offset}}); } break;
            case PrimitiveType::Type: { ir.Append({Opcode::LoadType, {.i = var.offset}}); } break;
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
    Size call_idx = ir.len;

    FunctionInfo *func0 = program->functions_map.FindValue(name, nullptr);
    if (RG_UNLIKELY(!func0)) {
        MarkError(call_pos, "Function '%1' does not exist", name);
        return false;
    }

    // Parse arguments
    if (!MatchToken(TokenKind::RightParenthesis)) {
        do {
            SkipNewLines();

            if (RG_UNLIKELY(!args.Available())) {
                MarkError(pos, "Functions cannot take more than %1 arguments", RG_LEN(args.data));
                return false;
            }

            const TypeInfo *type = ParseExpression();

            args.Append({nullptr, type});
            if (func0->variadic && args.len > func0->params.len) {
                ir.Append({Opcode::PushType, {.type = type}});
            }
        } while (MatchToken(TokenKind::Comma));

        SkipNewLines();
        ConsumeToken(TokenKind::RightParenthesis);
    }
    if (func0->variadic) {
        ir.Append({Opcode::PushInt, {.i = args.len - func0->params.len}});
    }

    // Find appropriate overload. Variadic functions cannot be overloaded but it
    // does not hurt to use the same logic to check argument types.
    FunctionInfo *func = func0;
    while (!TestOverload(*func, args)) {
        func = func->overload_next;

        if (RG_UNLIKELY(func == func0)) {
            LocalArray<char, 1024> buf = {};
            for (Size i = 0; i < args.len; i++) {
                buf.len += Fmt(buf.TakeAvailable(), "%1%2", i ? ", " : "", args[i].type->signature).len;
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

    // Emit intrinsic or call
    switch (func->mode) {
        case FunctionInfo::Mode::Intrinsic: {
            EmitIntrinsic(name, call_idx, args);
        } break;
        case FunctionInfo::Mode::Native: {
            ir.Append({Opcode::CallNative, {.func = func}});
        } break;
        case FunctionInfo::Mode::Blik: {
            if (func->inst_idx < 0) {
                if (current_func && current_func != func) {
                    func->earliest_call_pos = std::min(func->earliest_call_pos, current_func->earliest_call_pos);
                    func->earliest_call_idx = std::min(func->earliest_call_idx, current_func->earliest_call_idx);
                } else {
                    func->earliest_call_pos = std::min(func->earliest_call_pos, call_pos);
                    func->earliest_call_idx = std::min(func->earliest_call_idx, ir.len);
                }
            }

            ir.Append({Opcode::Call, {.func = func}});
        } break;
    }
    stack.Append({func->ret_type});

    return true;
}

void ParserImpl::EmitIntrinsic(const char *name, Size call_idx, Span<const FunctionInfo::Parameter> args)
{
    if (TestStr(name, "intToFloat")) {
        ir.Append({Opcode::IntToFloat});
    } else if (TestStr(name, "floatToInt")) {
        ir.Append({Opcode::FloatToInt});
    } else if (TestStr(name, "typeOf")) {
        // XXX: We can change the signature from typeOf(...) to typeOf(Any) after Any
        // is implemented, and remove this check.
        if (RG_UNLIKELY(args.len != 1)) {
            LogError("Intrinsic function typeOf() takes one argument");
            return;
        }

        // typeOf() does not execute anything!
        ir.RemoveFrom(call_idx);
        ir.Append({Opcode::PushType, {.type = args[0].type}});
    } else {
        RG_UNREACHABLE();
    }
}

bool ParserImpl::ParseExpressionOfType(const TypeInfo *type)
{
    Size expr_pos = pos;

    const TypeInfo *type2 = ParseExpression();
    if (RG_UNLIKELY(type2 != type)) {
        MarkError(expr_pos, "Expected expression result type to be %1, not %2",
                  type->signature, type2->signature);
        return false;
    }

    return true;
}

void ParserImpl::DiscardResult()
{
    if (ir.len >= 1) {
        switch (ir[ir.len - 1].code) {
            case Opcode::LoadBool:
            case Opcode::LoadInt:
            case Opcode::LoadFloat:
            case Opcode::LoadString:
            case Opcode::LoadType:
            case Opcode::LoadGlobalBool:
            case Opcode::LoadGlobalInt:
            case Opcode::LoadGlobalFloat:
            case Opcode::LoadGlobalString:
            case Opcode::LoadGlobalType: { ir.len--; } break;

            case Opcode::CopyBool: { ir[ir.len - 1].code = Opcode::StoreBool; } break;
            case Opcode::CopyInt: { ir[ir.len - 1].code = Opcode::StoreInt; } break;
            case Opcode::CopyFloat: { ir[ir.len - 1].code = Opcode::StoreFloat; } break;
            case Opcode::CopyString: { ir[ir.len - 1].code = Opcode::StoreString; } break;
            case Opcode::CopyType: { ir[ir.len - 1].code = Opcode::StoreType; } break;

            default: { EmitPop(1); } break;
        }
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
                      ir[ir.len - 1].u.func == current_func) {
        ir.len--;

        Size stack_offset = -2;
        for (Size i = current_func->params.len - 1; i >= 0; i--) {
            const FunctionInfo::Parameter &param = current_func->params[i];

            switch (param.type->primitive) {
                case PrimitiveType::Null: { stack_offset--; } break;
                case PrimitiveType::Bool: { ir.Append({Opcode::StoreBool, {.i = --stack_offset}}); } break;
                case PrimitiveType::Int: { ir.Append({Opcode::StoreInt, {.i = --stack_offset}}); } break;
                case PrimitiveType::Float: { ir.Append({Opcode::StoreFloat, {.i = --stack_offset}}); } break;
                case PrimitiveType::String: { ir.Append({Opcode::StoreString, {.i = --stack_offset}}); } break;
                case PrimitiveType::Type: { ir.Append({Opcode::StoreType, {.i = --stack_offset}}); } break;
            }
        }

        EmitPop(var_offset);
        ir.Append({Opcode::Jump, {.i = current_func->inst_idx - ir.len}});

        current_func->tre = true;
    } else {
        if (var_offset > 0) {
            Size pop = var_offset - 1;

            switch (current_func->ret_type->primitive) {
                case PrimitiveType::Null: { pop++; } break;
                case PrimitiveType::Bool: { ir.Append({Opcode::StoreBool, {.i = 0}}); } break;
                case PrimitiveType::Int: { ir.Append({Opcode::StoreInt, {.i = 0}}); } break;
                case PrimitiveType::Float: { ir.Append({Opcode::StoreFloat, {.i = 0}}); } break;
                case PrimitiveType::String: { ir.Append({Opcode::StoreString, {.i = 0}}); } break;
                case PrimitiveType::Type: { ir.Append({Opcode::StoreType, {.i = 0}}); } break;
            }

            EmitPop(pop);
        }

        ir.Append({Opcode::Return, {.i = current_func->params.len}});
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

        poisoned_set.Remove(&var);
    }

    program->variables.RemoveLast(count);
}

bool ParserImpl::TestOverload(const FunctionInfo &proto, Span<const FunctionInfo::Parameter> params)
{
    if (proto.variadic) {
        if (proto.params.len > params.len)
            return false;
    } else {
        if (proto.params.len != params.len)
            return false;
    }

    for (Size i = 0; i < proto.params.len; i++) {
        if (proto.params[i].type != params[i].type)
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

bool ParserImpl::SkipNewLines()
{
    if (MatchToken(TokenKind::EndOfLine)) {
        while (MatchToken(TokenKind::EndOfLine));

        if (RG_LIKELY(pos < tokens.len)) {
            src->lines.Append({ir.len, tokens[pos].line});
        }

        return true;
    } else {
        return false;
    }
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
    ImportAll(&parser);
    return parser.Parse(file);
}

}
