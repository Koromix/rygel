// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "compiler.hh"
#include "error.hh"
#include "lexer.hh"
#include "program.hh"

namespace RG {

struct PrototypeInfo {
    Size pos;

    bk_FunctionInfo *func;
    Size body_pos;

    RG_HASHTABLE_HANDLER(PrototypeInfo, pos);
};

struct PendingOperator {
    bk_TokenKind kind;
    int prec;
    bool unary;

    Size pos; // For error messages
    Size branch_addr; // Used for short-circuit operators
};

struct StackSlot {
    const bk_TypeInfo *type;
    const bk_VariableInfo *var;
};

class bk_Parser {
    RG_DELETE_COPY(bk_Parser)

    // All these members are relevant to the current parse only, and get resetted each time
    const bk_TokenizedFile *file;
    bk_CompileReport *out_report; // Can be NULL
    Span<const bk_Token> tokens;
    Size pos;
    Size prev_ir_len;
    bool valid;
    bool show_errors;
    bool show_hints;
    bk_SourceInfo *src;

    // Transient mappings
    HashTable<Size, PrototypeInfo> prototypes_map;
    HashMap<const void *, Size> definitions_map;
    HashSet<const void *> poisoned_set;

    Size func_jump_addr = -1;
    bk_FunctionInfo *current_func = nullptr;
    int depth = 0;

    Size var_offset = 0;

    Size loop_offset = -1;
    Size loop_break_addr = -1;
    Size loop_continue_addr = -1;

    HashSet<const char *> strings;

    // Only used (and valid) while parsing expression
    HeapArray<StackSlot> stack;

    bk_Program *program;
    HeapArray<bk_Instruction> &ir;

public:
    bk_Parser(bk_Program *program);

    bool Parse(const bk_TokenizedFile &file, bk_CompileReport *out_report);

    void AddFunction(const char *signature, std::function<bk_NativeFunction> native);
    void AddGlobal(const char *name, const bk_TypeInfo *type, bk_Value value, bool mut);

    inline const bk_TypeInfo *GetBasicType(bk_PrimitiveType primitive)
    {
        return &program->types[(int)primitive];
    }

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

    const bk_TypeInfo *ParseType();

    StackSlot ParseExpression(bool tolerate_assign);
    void ProduceOperator(const PendingOperator &op);
    bool EmitOperator1(bk_PrimitiveType in_primitive, bk_Opcode code, const bk_TypeInfo *out_type);
    bool EmitOperator2(bk_PrimitiveType in_primitive, bk_Opcode code, const bk_TypeInfo *out_type);
    bool ParseCall(const char *name);
    void EmitIntrinsic(const char *name, Size call_addr, Span<const bk_FunctionInfo::Parameter> args);
    void EmitLoad(const bk_VariableInfo &var);
    bool ParseExpressionOfType(const bk_TypeInfo *type);

    void DiscardResult();

    void EmitPop(int64_t count);
    void EmitReturn();
    void DestroyVariables(Size count);

    void FixJumps(Size jump_addr, Size target_addr);
    void TrimInstructions(Size count);

    bool TestOverload(const bk_FunctionInfo &proto, Span<const bk_FunctionInfo::Parameter> params2);

    bool ConsumeToken(bk_TokenKind kind);
    const char *ConsumeIdentifier();
    bool MatchToken(bk_TokenKind kind);
    bool PeekToken(bk_TokenKind kind);

    bool EndStatement();
    bool SkipNewLines();

    const char *InternString(const char *str);

    template <typename... Args>
    void MarkError(Size pos, const char *fmt, Args... args)
    {
        RG_ASSERT(pos >= 0);

        if (show_errors) {
            Size offset = (pos < tokens.len) ? tokens[pos].offset : file->code.len;
            int line = tokens[std::min(pos, tokens.len - 1)].line;

            bk_ReportDiagnostic(bk_DiagnosticType::Error, file->code, file->filename, line, offset, fmt, args...);

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

                bk_ReportDiagnostic(bk_DiagnosticType::ErrorHint, file->code, file->filename, line, offset, fmt, args...);
            } else {
                bk_ReportDiagnostic(bk_DiagnosticType::ErrorHint, fmt, args...);
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

bk_Compiler::bk_Compiler(bk_Program *program)
{
    parser = new bk_Parser(program);
}

bk_Compiler::~bk_Compiler()
{
    delete parser;
}

bool bk_Compiler::Compile(const bk_TokenizedFile &file, bk_CompileReport *out_report)
{
    return parser->Parse(file, out_report);
}

bool bk_Compiler::Compile(Span<const char> code, const char *filename, bk_CompileReport *out_report)
{
    bk_TokenizedFile file;
    if (!bk_Tokenize(code, filename, &file))
        return false;

    return parser->Parse(file, out_report);
}

void bk_Compiler::AddFunction(const char *signature, std::function<bk_NativeFunction> native)
{
    RG_ASSERT(native);
    parser->AddFunction(signature, native);
}

void bk_Compiler::AddGlobal(const char *name, const bk_TypeInfo *type, bk_Value value, bool mut)
{
    parser->AddGlobal(name, type, value, mut);
}

void bk_Compiler::AddGlobal(const char *name, bk_PrimitiveType primitive, bk_Value value, bool mut)
{
    const bk_TypeInfo *type = parser->GetBasicType(primitive);
    parser->AddGlobal(name, type, value, mut);
}

const bk_TypeInfo *bk_Compiler::GetBasicType(bk_PrimitiveType primitive)
{
    return parser->GetBasicType(primitive);
}

bk_Parser::bk_Parser(bk_Program *program)
    : program(program), ir(program->ir)
{
    RG_ASSERT(program);
    RG_ASSERT(!ir.len);

    // Basic types
    for (Size i = 0; i < RG_LEN(bk_PrimitiveTypeNames); i++) {
        bk_TypeInfo *type = program->types.Append({bk_PrimitiveTypeNames[i], (bk_PrimitiveType)i});
        program->types_map.Set(type);
    }

    // Special values
    AddGlobal("NaN", GetBasicType(bk_PrimitiveType::Float), bk_Value {.d = (double)NAN}, false);
    AddGlobal("Inf", GetBasicType(bk_PrimitiveType::Float), bk_Value {.d = (double)INFINITY}, false);

    // Intrinsics
    AddFunction("Float(Int): Float", {});
    AddFunction("Float(Float): Float", {});
    AddFunction("Int(Int): Int", {});
    AddFunction("Int(Float): Int", {});
    AddFunction("typeOf(...): Type", {});
}

bool bk_Parser::Parse(const bk_TokenizedFile &file, bk_CompileReport *out_report)
{
    prev_ir_len = ir.len;

    // Restore previous state if something goes wrong
    RG_DEFER_NC(err_guard, sources_len = program->sources.len,
                           prev_var_offset = var_offset,
                           variables_len = program->variables.len,
                           functions_len = program->functions.len) {
        ir.RemoveFrom(prev_ir_len);
        program->sources.RemoveFrom(sources_len);

        var_offset = prev_var_offset;
        DestroyVariables(program->variables.len - variables_len);

        if (functions_len) {
            for (Size i = functions_len; i < program->functions.len; i++) {
                bk_FunctionInfo *func = &program->functions[i];
                bk_FunctionInfo **it = program->functions_map.Find(func->name);

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
    func_jump_addr = -1;

    src = program->sources.AppendDefault();
    src->filename = DuplicateString(file.filename, &program->str_alloc).ptr;

    // We want top-level order-independent functions
    ParsePrototypes(file.funcs);

    // Do the actual parsing!
    src->lines.Append({ir.len, 0});
    while (RG_LIKELY(pos < tokens.len)) {
        ParseStatement();
    }

    // Maybe it'll help catch bugs
    RG_ASSERT(!depth);
    RG_ASSERT(loop_offset == -1);
    RG_ASSERT(!current_func);

    if (valid) {
        ir.Append({bk_Opcode::End, {.i = var_offset}});
        ir.Trim();

        err_guard.Disable();
    }

    return valid;
}

// This is not exposed to user scripts, and the validation of signature is very light,
// with a few debug-only asserts. Bad function names (even invalid UTF-8 sequences)
// will go right through. Don't pass in garbage!
void bk_Parser::AddFunction(const char *signature, std::function<bk_NativeFunction> native)
{
    bk_FunctionInfo *func = program->functions.AppendDefault();

    char *ptr = DuplicateString(signature, &program->str_alloc).ptr;
    func->signature = ptr;

    // Name
    {
        Size len = strcspn(ptr, "()");
        Span<const char> func_name = TrimStr(MakeSpan(ptr, len));

        func->name = DuplicateString(func_name, &program->str_alloc).ptr;
        ptr += len;
    }

    func->mode = native ? bk_FunctionInfo::Mode::Native : bk_FunctionInfo::Mode::Intrinsic;
    func->native = native;
    func->addr = -1;

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
                const bk_TypeInfo *type = program->types_map.FindValue(ptr, nullptr);
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
        func->ret_type = GetBasicType(bk_PrimitiveType::Null);
    }

    // Publish it!
    {
        bk_FunctionInfo *head = *program->functions_map.TrySet(func).first;

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

void bk_Parser::AddGlobal(const char *name, const bk_TypeInfo *type, bk_Value value, bool mut)
{
    bk_VariableInfo *var = program->variables.AppendDefault();

    var->name = InternString(name);
    var->type = type;
    var->mut = mut;

    switch (type->primitive) {
        case bk_PrimitiveType::Null: { ir.Append({bk_Opcode::PushNull}); } break;
        case bk_PrimitiveType::Bool: { ir.Append({bk_Opcode::PushBool, {.b = value.b}}); } break;
        case bk_PrimitiveType::Int: { ir.Append({bk_Opcode::PushInt, {.i = value.i}}); } break;
        case bk_PrimitiveType::Float: { ir.Append({bk_Opcode::PushFloat, {.d = value.d}}); } break;
        case bk_PrimitiveType::Type: { ir.Append({bk_Opcode::PushType, {.type = value.type}}); } break;
    }

    var->global = true;
    var->offset = var_offset++;
    var->ready_addr = ir.len;

    bool success = program->variables_map.TrySet(var).second;
    RG_ASSERT(success);
}

void bk_Parser::ParsePrototypes(Span<const Size> funcs)
{
    RG_ASSERT(!prototypes_map.count);
    RG_ASSERT(pos == 0);

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
        bk_FunctionInfo *func = program->functions.AppendDefault();
        definitions_map.Set(func, pos);

        proto->pos = pos;
        proto->func = func;

        func->name = ConsumeIdentifier();
        func->mode = bk_FunctionInfo::Mode::Blik;

        // Insert in functions map
        {
            std::pair<bk_FunctionInfo **, bool> ret = program->functions_map.TrySet(func);
            bk_FunctionInfo *proto0 = *ret.first;

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
        ConsumeToken(bk_TokenKind::LeftParenthesis);
        if (!MatchToken(bk_TokenKind::RightParenthesis)) {
            do {
                SkipNewLines();

                bk_VariableInfo *var = program->variables.AppendDefault();
                Size param_pos = pos;

                var->mut = MatchToken(bk_TokenKind::Mut);
                var->name = ConsumeIdentifier();

                std::pair<bk_VariableInfo **, bool> ret = program->variables_map.TrySet(var);
                if (RG_UNLIKELY(!ret.second)) {
                    const bk_VariableInfo *prev_var = *ret.first;
                    var->shadow = prev_var;

                    // Error messages for globals are issued in ParseFunction(), because at
                    // this stage some globals (those issues from the same Parse call) may not
                    // yet exist. Better issue all errors about that later.
                    if (!prev_var->global) {
                        MarkError(param_pos, "Parameter named '%1' already exists", var->name);
                    }
                }

                ConsumeToken(bk_TokenKind::Colon);
                var->type = ParseType();

                if (RG_LIKELY(func->params.Available())) {
                    bk_FunctionInfo::Parameter *param = func->params.AppendDefault();
                    definitions_map.Set(param, param_pos);

                    param->name = var->name;
                    param->type = var->type;
                    param->mut = var->mut;
                } else {
                    MarkError(pos - 1, "Functions cannot have more than %1 parameters", RG_LEN(func->params.data));
                }
            } while (MatchToken(bk_TokenKind::Comma));

            SkipNewLines();
            ConsumeToken(bk_TokenKind::RightParenthesis);
        }

        // Return type
        if (MatchToken(bk_TokenKind::Colon)) {
            func->ret_type = ParseType();
        } else {
            func->ret_type = GetBasicType(bk_PrimitiveType::Null);
        }

        // Build signature (with parameter and return types)
        {
            HeapArray<char> buf(&program->str_alloc);

            Fmt(&buf, "%1(", func->name);
            for (Size i = 0; i < func->params.len; i++) {
                const bk_FunctionInfo::Parameter &param = func->params[i];
                Fmt(&buf, "%1%2", i ? ", " : "", param.type->signature);
            }
            Fmt(&buf, ")");
            if (func->ret_type != GetBasicType(bk_PrimitiveType::Null)) {
                Fmt(&buf, ": %1", func->ret_type->signature);
            }

            func->signature = buf.TrimAndLeak(1).ptr;
        }

        proto->body_pos = pos;

        // We don't know where it will live yet!
        func->addr = -1;
        func->earliest_call_pos = RG_SIZE_MAX;
        func->earliest_call_addr = RG_SIZE_MAX;
    }
}

bool bk_Parser::ParseBlock()
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

    while (RG_LIKELY(pos < tokens.len) && tokens[pos].kind != bk_TokenKind::Else &&
                                          tokens[pos].kind != bk_TokenKind::End) {
        has_return |= ParseStatement();
    }

    return has_return;
}

bool bk_Parser::ParseStatement()
{
    bool has_return = false;

    src->lines.Append({ir.len, tokens[pos].line});
    show_errors = true;

    switch (tokens[pos].kind) {
        case bk_TokenKind::EndOfLine: {
            pos++;
            src->lines.len--;
        } break;
        case bk_TokenKind::Semicolon: { pos++; } break;

        case bk_TokenKind::Begin: {
            pos++;

            if (RG_LIKELY(EndStatement())) {
                has_return = ParseBlock();
                ConsumeToken(bk_TokenKind::End);

                EndStatement();
            }
        } break;
        case bk_TokenKind::Func: {
            ParseFunction();
            EndStatement();
        } break;
        case bk_TokenKind::Return: {
            ParseReturn();
            has_return = true;
            EndStatement();
        } break;
        case bk_TokenKind::Let: {
            ParseLet();
            EndStatement();
        } break;
        case bk_TokenKind::If: {
            has_return = ParseIf();
            EndStatement();
        } break;
        case bk_TokenKind::While: {
            ParseWhile();
            EndStatement();
        } break;
        case bk_TokenKind::For: {
            ParseFor();
            EndStatement();
        } break;
        case bk_TokenKind::Break: {
            ParseBreak();
            EndStatement();
        } break;
        case bk_TokenKind::Continue: {
            ParseContinue();
            EndStatement();
        } break;

        default: {
            ParseExpression(true);
            DiscardResult();

            EndStatement();
        } break;
    }

    return has_return;
}

bool bk_Parser::ParseDo()
{
    pos++;

    if (PeekToken(bk_TokenKind::Return)) {
        ParseReturn();
        return true;
    } else if (PeekToken(bk_TokenKind::Break)) {
        ParseBreak();
        return false;
    } else if (PeekToken(bk_TokenKind::Continue)) {
        ParseContinue();
        return false;
    } else {
        ParseExpression(true);
        DiscardResult();

        return false;
    }
}

void bk_Parser::ParseFunction()
{
    Size func_pos = ++pos;

    const PrototypeInfo *proto = prototypes_map.Find(func_pos);
    RG_ASSERT(proto);

    bk_FunctionInfo *func = proto->func;

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

    // Skip prototype
    var_offset = 0;
    pos = proto->body_pos;

    // Create parameter variables
    for (const bk_FunctionInfo::Parameter &param: func->params) {
        bk_VariableInfo *var = program->variables.AppendDefault();
        Size param_pos = definitions_map.FindValue(&param, -1);
        definitions_map.Set(var, param_pos);

        var->name = param.name;
        var->type = param.type;
        var->mut = param.mut;

        var->offset = var_offset++;

        std::pair<bk_VariableInfo **, bool> ret = program->variables_map.TrySet(var);
        if (RG_UNLIKELY(!ret.second)) {
            const bk_VariableInfo *prev_var = *ret.first;
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
            poisoned_set.Set(var);
        }
    }

    // Check for incompatible function overloadings
    {
        bk_FunctionInfo *overload = program->functions_map.FindValue(func->name, nullptr);

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

    // Jump over consecutively defined functions in one go
    if (func_jump_addr < 0 || ir[func_jump_addr].u.i < ir.len - func_jump_addr) {
        func_jump_addr = ir.len;
        ir.Append({bk_Opcode::Jump});
    }

    func->addr = ir.len;

    // Function body
    bool has_return = false;
    if (PeekToken(bk_TokenKind::Do)) {
        has_return = ParseDo();
    } else if (RG_LIKELY(EndStatement())) {
        has_return = ParseBlock();
        ConsumeToken(bk_TokenKind::End);
    }

    if (!has_return) {
        if (func->ret_type == GetBasicType(bk_PrimitiveType::Null)) {
            ir.Append({bk_Opcode::PushNull});
            EmitReturn();
        } else {
            MarkError(func_pos, "Some code paths do not return a value in function '%1'", func->name);
        }
    }

    ir[func_jump_addr].u.i = ir.len - func_jump_addr;
}

void bk_Parser::ParseReturn()
{
    Size return_pos = ++pos;

    if (RG_UNLIKELY(!current_func)) {
        MarkError(pos - 1, "Return statement cannot be used outside function");
        return;
    }

    const bk_TypeInfo *type;
    if (PeekToken(bk_TokenKind::EndOfLine) || PeekToken(bk_TokenKind::Semicolon)) {
        ir.Append({bk_Opcode::PushNull});
        type = GetBasicType(bk_PrimitiveType::Null);
    } else {
        type = ParseExpression(true).type;
    }

    if (RG_UNLIKELY(type != current_func->ret_type)) {
        MarkError(return_pos, "Cannot return %1 value in function defined to return %2",
                  type->signature, current_func->ret_type->signature);
        return;
    }

    EmitReturn();
}

void bk_Parser::ParseLet()
{
    Size var_pos = ++pos;

    bk_VariableInfo *var = program->variables.AppendDefault();

    var->mut = MatchToken(bk_TokenKind::Mut);
    var_pos += var->mut;
    definitions_map.Set(var, pos);
    var->name = ConsumeIdentifier();

    std::pair<bk_VariableInfo **, bool> ret = program->variables_map.TrySet(var);
    if (RG_UNLIKELY(!ret.second)) {
        const bk_VariableInfo *prev_var = *ret.first;
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

    StackSlot slot;
    if (MatchToken(bk_TokenKind::Assign)) {
        SkipNewLines();
        slot = ParseExpression(true);
    } else {
        ConsumeToken(bk_TokenKind::Colon);

        // Don't assign to var->type yet, so that ParseExpression() knows it
        // cannot use this variable.
        const bk_TypeInfo *type = ParseType();

        if (MatchToken(bk_TokenKind::Assign)) {
            SkipNewLines();

            Size expr_pos = pos;
            slot = ParseExpression(true);

            if (RG_UNLIKELY(slot.type != type)) {
                MarkError(expr_pos - 1, "Cannot assign %1 value to variable '%2' (defined as %3)",
                          slot.type->signature, var->name, type->signature);
            }
        } else {
            slot = {type};

            switch (type->primitive) {
                case bk_PrimitiveType::Null: { ir.Append({bk_Opcode::PushNull}); } break;
                case bk_PrimitiveType::Bool: { ir.Append({bk_Opcode::PushBool, {.b = false}}); } break;
                case bk_PrimitiveType::Int: { ir.Append({bk_Opcode::PushInt, {.i = 0}}); } break;
                case bk_PrimitiveType::Float: { ir.Append({bk_Opcode::PushFloat, {.d = 0.0}}); } break;
                case bk_PrimitiveType::Type: { ir.Append({bk_Opcode::PushType, {.type = GetBasicType(bk_PrimitiveType::Null)}}); } break;
            }
        }
    }

    var->type = slot.type;
    if (slot.var && !slot.var->mut && !var->mut) {
        // We're about to alias var to slot.var... we need to drop the load instruction.
        // Is it enough, and is it safe?
        TrimInstructions(1);

        var->global = slot.var->global;
        var->offset = slot.var->offset;
    } else {
        var->global = !current_func;
        var->offset = var_offset++;
    }
    var->ready_addr = ir.len;

    // Expressions involving this variable won't issue (visible) errors
    // and will be marked as invalid too.
    if (RG_UNLIKELY(!show_errors)) {
        poisoned_set.Set(var);
    }
}

bool bk_Parser::ParseIf()
{
    pos++;

    ParseExpressionOfType(GetBasicType(bk_PrimitiveType::Bool));

    Size branch_addr = ir.len;
    ir.Append({bk_Opcode::BranchIfFalse});

    bool has_return = true;
    bool has_else = false;

    if (PeekToken(bk_TokenKind::Do)) {
        has_return &= ParseDo();
        ir[branch_addr].u.i = ir.len - branch_addr;
    } else if (RG_LIKELY(EndStatement())) {
        has_return &= ParseBlock();

        if (MatchToken(bk_TokenKind::Else)) {
            Size jump_addr = ir.len;
            ir.Append({bk_Opcode::Jump, {.i = -1}});

            do {
                ir[branch_addr].u.i = ir.len - branch_addr;

                if (MatchToken(bk_TokenKind::If)) {
                    ParseExpressionOfType(GetBasicType(bk_PrimitiveType::Bool));

                    if (RG_LIKELY(EndStatement())) {
                        branch_addr = ir.len;
                        ir.Append({bk_Opcode::BranchIfFalse});

                        has_return &= ParseBlock();

                        ir.Append({bk_Opcode::Jump, {.i = jump_addr}});
                        jump_addr = ir.len - 1;
                    }
                } else if (RG_LIKELY(EndStatement())) {
                    has_return &= ParseBlock();
                    has_else = true;

                    break;
                }
            } while (MatchToken(bk_TokenKind::Else));

            FixJumps(jump_addr, ir.len);
        } else {
            ir[branch_addr].u.i = ir.len - branch_addr;
        }

        ConsumeToken(bk_TokenKind::End);
    }

    return has_return && has_else;
}

void bk_Parser::ParseWhile()
{
    pos++;

    // Parse expression. We'll make a copy after the loop body so that the IR code looks
    // roughly like if (cond) { do { ... } while (cond) }.
    Size condition_addr = ir.len;
    Size condition_line_idx = src->lines.len;
    ParseExpressionOfType(GetBasicType(bk_PrimitiveType::Bool));

    Size branch_addr = ir.len;
    ir.Append({bk_Opcode::BranchIfFalse});

    // Break and continue need to apply to while loop blocks
    RG_DEFER_C(prev_offset = loop_offset,
               prev_break_addr = loop_break_addr,
               prev_continue_addr = loop_continue_addr) {
        loop_offset = prev_offset;
        loop_break_addr = prev_break_addr;
        loop_continue_addr = prev_continue_addr;
    };
    loop_offset = var_offset;
    loop_break_addr = -1;
    loop_continue_addr = -1;

    // Parse body
    if (PeekToken(bk_TokenKind::Do)) {
        ParseDo();
    } else if (RG_LIKELY(EndStatement())) {
        ParseBlock();
        ConsumeToken(bk_TokenKind::End);
    }

    FixJumps(loop_continue_addr, ir.len);

    // Copy the condition expression, and the IR/line map information
    for (Size i = condition_line_idx; i < src->lines.len &&
                                      src->lines[i].addr < branch_addr; i++) {
        const bk_SourceInfo::Line &line = src->lines[i];
        src->lines.Append({ir.len + (line.addr - condition_addr), line.line});
    }
    ir.Grow(branch_addr - condition_addr);
    ir.Append(ir.Take(condition_addr, branch_addr - condition_addr));

    ir.Append({bk_Opcode::BranchIfTrue, {.i = branch_addr - ir.len + 1}});
    ir[branch_addr].u.i = ir.len - branch_addr;

    FixJumps(loop_break_addr, ir.len);
}

void bk_Parser::ParseFor()
{
    Size for_pos = ++pos;

    bk_VariableInfo *it = program->variables.AppendDefault();

    it->mut = MatchToken(bk_TokenKind::Mut);
    for_pos += it->mut;
    definitions_map.Set(it, pos);
    it->name = ConsumeIdentifier();

    it->offset = var_offset + 2;

    std::pair<bk_VariableInfo **, bool> ret = program->variables_map.TrySet(it);
    if (RG_UNLIKELY(!ret.second)) {
        const bk_VariableInfo *prev_var = *ret.first;
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

    ConsumeToken(bk_TokenKind::In);
    ParseExpressionOfType(GetBasicType(bk_PrimitiveType::Int));
    ConsumeToken(bk_TokenKind::Colon);
    ParseExpressionOfType(GetBasicType(bk_PrimitiveType::Int));

    // Make sure start and end value remain on the stack
    var_offset += 3;

    // Put iterator value on the stack
    ir.Append({bk_Opcode::LoadLocalInt, {.i = it->offset - 2}});
    it->type = GetBasicType(bk_PrimitiveType::Int);

    Size body_addr = ir.len;

    ir.Append({bk_Opcode::LoadLocalInt, {.i = it->offset}});
    ir.Append({bk_Opcode::LoadLocalInt, {.i = it->offset - 1}});
    ir.Append({bk_Opcode::LessThanInt});
    ir.Append({bk_Opcode::BranchIfFalse});

    // Break and continue need to apply to for loop blocks
    RG_DEFER_C(prev_offset = loop_offset,
               prev_break_addr = loop_break_addr,
               prev_continue_addr = loop_continue_addr) {
        loop_offset = prev_offset;
        loop_break_addr = prev_break_addr;
        loop_continue_addr = prev_continue_addr;
    };
    loop_offset = var_offset;
    loop_break_addr = -1;
    loop_continue_addr = -1;

    // Parse body
    if (PeekToken(bk_TokenKind::Do)) {
        ParseDo();
    } else if (RG_LIKELY(EndStatement())) {
        ParseBlock();
        ConsumeToken(bk_TokenKind::End);
    }

    FixJumps(loop_continue_addr, ir.len);

    ir.Append({bk_Opcode::PushInt, {.i = 1}});
    ir.Append({bk_Opcode::AddInt});
    ir.Append({bk_Opcode::Jump, {.i = body_addr - ir.len}});
    ir[body_addr + 3].u.i = ir.len - (body_addr + 3);

    FixJumps(loop_break_addr, ir.len);

    // Destroy iterator and range values
    EmitPop(3);
    DestroyVariables(1);
    var_offset -= 3;
}

void bk_Parser::ParseBreak()
{
    Size break_pos = pos++;

    if (loop_offset < 0) {
        MarkError(break_pos, "Break statement outside of loop");
        return;
    }

    EmitPop(var_offset - loop_offset);

    ir.Append({bk_Opcode::Jump, {.i = loop_break_addr}});
    loop_break_addr = ir.len - 1;
}

void bk_Parser::ParseContinue()
{
    Size continue_pos = pos++;

    if (loop_offset < 0) {
        MarkError(continue_pos, "Continue statement outside of loop");
        return;
    }

    EmitPop(var_offset - loop_offset);

    ir.Append({bk_Opcode::Jump, {.i = loop_continue_addr}});
    loop_continue_addr = ir.len - 1;
}

const bk_TypeInfo *bk_Parser::ParseType()
{
    Size type_pos = pos;

    // Parse type expression
    {
        const bk_TypeInfo *type = ParseExpression(false).type;

        if (RG_UNLIKELY(type != GetBasicType(bk_PrimitiveType::Type))) {
            MarkError(type_pos, "Expected a Type expression, not %1", type->signature);
            return GetBasicType(bk_PrimitiveType::Null);
        }
    }

    // Once we start to implement constant folding and CTFE, more complex expressions
    // should work without any change here.
    if (RG_UNLIKELY(ir[ir.len - 1].code != bk_Opcode::PushType)) {
        MarkError(type_pos, "Complex type expression cannot be resolved statically");
        return GetBasicType(bk_PrimitiveType::Null);
    }

    const bk_TypeInfo *type = ir[ir.len - 1].u.type;
    TrimInstructions(1);

    return type;
}

static int GetOperatorPrecedence(bk_TokenKind kind, bool expect_unary)
{
    if (expect_unary) {
        switch (kind) {
            case bk_TokenKind::XorOrComplement:
            case bk_TokenKind::Plus:
            case bk_TokenKind::Minus:
            case bk_TokenKind::Not: { return 12; } break;

            default: { return -1; } break;
        }
    } else {
        switch (kind) {
            case bk_TokenKind::Reassign:
            case bk_TokenKind::PlusAssign:
            case bk_TokenKind::MinusAssign:
            case bk_TokenKind::MultiplyAssign:
            case bk_TokenKind::DivideAssign:
            case bk_TokenKind::ModuloAssign:
            case bk_TokenKind::LeftShiftAssign:
            case bk_TokenKind::RightShiftAssign:
            case bk_TokenKind::LeftRotateAssign:
            case bk_TokenKind::RightRotateAssign:
            case bk_TokenKind::AndAssign:
            case bk_TokenKind::OrAssign:
            case bk_TokenKind::XorAssign: { return 0; } break;

            case bk_TokenKind::OrOr: { return 2; } break;
            case bk_TokenKind::AndAnd: { return 3; } break;
            case bk_TokenKind::Equal:
            case bk_TokenKind::NotEqual: { return 4; } break;
            case bk_TokenKind::Greater:
            case bk_TokenKind::GreaterOrEqual:
            case bk_TokenKind::Less:
            case bk_TokenKind::LessOrEqual: { return 5; } break;
            case bk_TokenKind::Or: { return 6; } break;
            case bk_TokenKind::XorOrComplement: { return 7; } break;
            case bk_TokenKind::And: { return 8; } break;
            case bk_TokenKind::LeftShift:
            case bk_TokenKind::RightShift:
            case bk_TokenKind::LeftRotate:
            case bk_TokenKind::RightRotate: { return 9; } break;
            case bk_TokenKind::Plus:
            case bk_TokenKind::Minus: { return 10; } break;
            case bk_TokenKind::Multiply:
            case bk_TokenKind::Divide:
            case bk_TokenKind::Modulo: { return 11; } break;

            default: { return -1; } break;
        }
    }
}

StackSlot bk_Parser::ParseExpression(bool tolerate_assign)
{
    Size start_values_len = stack.len;
    RG_DEFER { stack.RemoveFrom(start_values_len); };

    LocalArray<PendingOperator, 128> operators;
    bool expect_value = true;
    Size parentheses = 0;

    // Used to detect "empty" expressions
    Size prev_offset = pos;

    while (RG_LIKELY(pos < tokens.len)) {
        const bk_Token &tok = tokens[pos++];

        if (tok.kind == bk_TokenKind::LeftParenthesis) {
            if (RG_UNLIKELY(!expect_value))
                goto unexpected;

            operators.Append({tok.kind});
            parentheses++;
        } else if (parentheses && tok.kind == bk_TokenKind::RightParenthesis) {
            if (RG_UNLIKELY(expect_value))
                goto unexpected;
            expect_value = false;

            for (;;) {
                const PendingOperator &op = operators.data[operators.len - 1];

                if (op.kind == bk_TokenKind::LeftParenthesis) {
                    operators.len--;
                    parentheses--;
                    break;
                }

                ProduceOperator(op);
                operators.len--;
            }
        } else if (tok.kind == bk_TokenKind::Null || tok.kind == bk_TokenKind::Bool ||
                   tok.kind == bk_TokenKind::Integer || tok.kind == bk_TokenKind::Float ||
                   tok.kind == bk_TokenKind::Identifier) {
            if (RG_UNLIKELY(!expect_value))
                goto unexpected;
            expect_value = false;

            switch (tok.kind) {
                case bk_TokenKind::Null: {
                    ir.Append({bk_Opcode::PushNull});
                    stack.Append({GetBasicType(bk_PrimitiveType::Null)});
                } break;
                case bk_TokenKind::Bool: {
                    ir.Append({bk_Opcode::PushBool, {.b = tok.u.b}});
                    stack.Append({GetBasicType(bk_PrimitiveType::Bool)});
                } break;
                case bk_TokenKind::Integer: {
                    ir.Append({bk_Opcode::PushInt, {.i = tok.u.i}});
                    stack.Append({GetBasicType(bk_PrimitiveType::Int)});
                } break;
                case bk_TokenKind::Float: {
                    ir.Append({bk_Opcode::PushFloat, {.d = tok.u.d}});
                    stack.Append({GetBasicType(bk_PrimitiveType::Float)});
                } break;
                case bk_TokenKind::String: { RG_UNREACHABLE(); } break;

                case bk_TokenKind::Identifier: {
                    if (MatchToken(bk_TokenKind::LeftParenthesis)) {
                        if (RG_UNLIKELY(!ParseCall(tok.u.str)))
                            goto error;
                    } else {
                        const bk_VariableInfo *var = program->variables_map.FindValue(tok.u.str, nullptr);

                        if (RG_LIKELY(var)) {
                            show_errors &= !poisoned_set.Find(var);

                            if (RG_LIKELY(var->type)) {
                                EmitLoad(*var);
                            } else {
                                MarkError(pos - 1, "Cannot use variable '%1' before it is defined", var->name);
                                goto error;
                            }
                        } else {
                            const bk_TypeInfo *type = program->types_map.FindValue(tok.u.str, nullptr);

                            if (RG_UNLIKELY(!type)) {
                                MarkError(pos - 1, "Reference to unknown identifier '%1'", tok.u.str);
                                goto error;
                            }

                            ir.Append({bk_Opcode::PushType, {.type = type}});
                            stack.Append({GetBasicType(bk_PrimitiveType::Type)});
                        }
                    }
                } break;

                default: { RG_UNREACHABLE(); } break;
            }
        } else {
            PendingOperator op = {};

            op.kind = tok.kind;
            op.prec = GetOperatorPrecedence(tok.kind, expect_value);
            op.unary = expect_value;
            op.pos = pos - 1;

            // Not an operator? There's a few cases to deal with, including a perfectly
            // valid one: end of expression!
            if (op.prec < 0) {
                if (RG_UNLIKELY(pos == prev_offset + 1)) {
                    MarkError(pos - 1, "Unexpected token '%1', expected value or expression",
                              bk_TokenKindNames[(int)tokens[pos - 1].kind]);
                    goto error;
                } else if (RG_UNLIKELY(expect_value || parentheses)) {
                    pos--;
                    if (RG_LIKELY(SkipNewLines())) {
                        continue;
                    } else {
                        pos++;
                        goto unexpected;
                    }
                } else if (RG_UNLIKELY(tolerate_assign && tok.kind == bk_TokenKind::Assign)) {
                    MarkError(pos - 1, "Unexpected token '=', did you mean '==' or ':='?");

                    // Pretend the user meant '==' to recover
                    op.kind = bk_TokenKind::Equal;
                    op.prec = GetOperatorPrecedence(bk_TokenKind::Equal, expect_value);
                } else {
                    pos--;
                    break;
                }
            }

            if (expect_value != op.unary)
                goto unexpected;
            expect_value = true;

            while (operators.len) {
                const PendingOperator &op2 = operators[operators.len - 1];
                bool right_associative = (op2.unary || op2.kind == bk_TokenKind::Reassign);

                if (op2.kind == bk_TokenKind::LeftParenthesis)
                    break;
                if (op2.prec - right_associative < op.prec)
                    break;

                ProduceOperator(op2);
                operators.len--;
            }

            if (tok.kind == bk_TokenKind::Reassign) {
                // Remove useless load instruction. We don't remove the variable from
                // stack slots,  because it will be needed when we emit the store instruction
                // and will be removed then.
                TrimInstructions(1);
            } else if (tok.kind == bk_TokenKind::AndAnd) {
                op.branch_addr = ir.len;
                ir.Append({bk_Opcode::SkipIfFalse});
            } else if (tok.kind == bk_TokenKind::OrOr) {
                op.branch_addr = ir.len;
                ir.Append({bk_Opcode::SkipIfTrue});
            }

            if (RG_UNLIKELY(!operators.Available())) {
                MarkError(pos - 1, "Too many operators on the stack (compiler limitation)");
                goto error;
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
    if (RG_LIKELY(stack.len)) {
        return stack[stack.len - 1];
    } else {
        return {GetBasicType(bk_PrimitiveType::Null)};
    }

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

        MarkError(pos, "Unexpected token '%1', expected %2", bk_TokenKindNames[(int)tokens[pos].kind], expected);
    }
error:
    // The goal of this loop is to skip expression until we get to "do" (which is used
    // for single-line constructs) or end of line (which starts a block in some cases,
    // e.g. if expressions). This way, the parent can differenciate single-line constructs
    // and block constructs, and prevent generation of garbage errors (such as "functions
    // must be defined in top-level scope") caused by undetected block and/or do statement.
    while (pos < tokens.len && tokens[pos].kind != bk_TokenKind::Do &&
                               tokens[pos].kind != bk_TokenKind::EndOfLine &&
                               tokens[pos].kind != bk_TokenKind::Semicolon) {
        pos++;
    };
    return {GetBasicType(bk_PrimitiveType::Null)};
}

void bk_Parser::ProduceOperator(const PendingOperator &op)
{
    bool success = false;

    if (op.prec == 0) { // Assignement operators
        RG_ASSERT(!op.unary);

        const bk_VariableInfo *var = stack[stack.len - 2].var;
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
            case bk_TokenKind::Reassign: {
                stack[--stack.len - 1].var = nullptr;
                success = true;
            } break;

            case bk_TokenKind::PlusAssign: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::AddInt, stack[stack.len - 2].type) ||
                          EmitOperator2(bk_PrimitiveType::Float, bk_Opcode::AddFloat, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::MinusAssign: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::SubstractInt, stack[stack.len - 2].type) ||
                          EmitOperator2(bk_PrimitiveType::Float, bk_Opcode::SubstractFloat, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::MultiplyAssign: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::MultiplyInt, stack[stack.len - 2].type) ||
                          EmitOperator2(bk_PrimitiveType::Float, bk_Opcode::MultiplyFloat, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::DivideAssign: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::DivideInt, stack[stack.len - 2].type) ||
                          EmitOperator2(bk_PrimitiveType::Float, bk_Opcode::DivideFloat, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::ModuloAssign: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::ModuloInt, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::AndAssign: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::AndInt, stack[stack.len - 2].type) ||
                          EmitOperator2(bk_PrimitiveType::Bool, bk_Opcode::AndBool, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::OrAssign: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::OrInt, stack[stack.len - 2].type) ||
                          EmitOperator2(bk_PrimitiveType::Bool, bk_Opcode::OrBool, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::XorAssign: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::XorInt, stack[stack.len - 2].type) ||
                          EmitOperator2(bk_PrimitiveType::Bool, bk_Opcode::NotEqualBool, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::LeftShiftAssign: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::LeftShiftInt, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::RightShiftAssign: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::RightShiftInt, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::LeftRotateAssign: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::LeftRotateInt, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::RightRotateAssign: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::RightRotateInt, stack[stack.len - 2].type);
            } break;

            default: { RG_UNREACHABLE(); } break;
        }

        if (var->global) {
            switch (var->type->primitive) {
                case bk_PrimitiveType::Null: {} break;
                case bk_PrimitiveType::Bool: { ir.Append({bk_Opcode::CopyBool, {.i = var->offset}}); } break;
                case bk_PrimitiveType::Int: { ir.Append({bk_Opcode::CopyInt, {.i = var->offset}}); } break;
                case bk_PrimitiveType::Float: { ir.Append({bk_Opcode::CopyFloat, {.i = var->offset}}); } break;
                case bk_PrimitiveType::Type: { ir.Append({bk_Opcode::CopyType, {.i = var->offset}}); } break;
            }
        } else {
            switch (var->type->primitive) {
                case bk_PrimitiveType::Null: {} break;
                case bk_PrimitiveType::Bool: { ir.Append({bk_Opcode::CopyLocalBool, {.i = var->offset}}); } break;
                case bk_PrimitiveType::Int: { ir.Append({bk_Opcode::CopyLocalInt, {.i = var->offset}}); } break;
                case bk_PrimitiveType::Float: { ir.Append({bk_Opcode::CopyLocalFloat, {.i = var->offset}}); } break;
                case bk_PrimitiveType::Type: { ir.Append({bk_Opcode::CopyLocalType, {.i = var->offset}}); } break;
            }
        }
    } else { // Other operators
        switch (op.kind) {
            case bk_TokenKind::Plus: {
                if (op.unary) {
                    success = stack[stack.len - 1].type->primitive == bk_PrimitiveType::Int ||
                              stack[stack.len - 1].type->primitive == bk_PrimitiveType::Float;
                } else {
                    success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::AddInt, stack[stack.len - 2].type) ||
                              EmitOperator2(bk_PrimitiveType::Float, bk_Opcode::AddFloat, stack[stack.len - 2].type);
                }
            } break;
            case bk_TokenKind::Minus: {
                if (op.unary) {
                    bk_Instruction *inst = &ir[ir.len - 1];

                    switch (inst->code) {
                        case bk_Opcode::PushInt: {
                            // In theory, this could overflow trying to negate INT64_MIN.. but we
                            // we can't have INT64_MIN, because numeric literal tokens are always
                            // positive. inst->u.i will flip between positive and negative values
                            // if we encounter successive '-' unary operators (e.g. -----64).
                            inst->u.i = -inst->u.i;
                            success = true;
                        } break;
                        case bk_Opcode::PushFloat: {
                            inst->u.d = -inst->u.d;
                            success = true;
                        } break;
                        case bk_Opcode::NegateInt:
                        case bk_Opcode::NegateFloat: {
                            TrimInstructions(1);
                            success = true;
                        } break;

                        default: {
                            success = EmitOperator1(bk_PrimitiveType::Int, bk_Opcode::NegateInt, stack[stack.len - 1].type) ||
                                      EmitOperator1(bk_PrimitiveType::Float, bk_Opcode::NegateFloat, stack[stack.len - 1].type);
                        }
                    }
                } else {
                    success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::SubstractInt, stack[stack.len - 2].type) ||
                              EmitOperator2(bk_PrimitiveType::Float, bk_Opcode::SubstractFloat, stack[stack.len - 2].type);
                }
            } break;
            case bk_TokenKind::Multiply: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::MultiplyInt, stack[stack.len - 2].type) ||
                          EmitOperator2(bk_PrimitiveType::Float, bk_Opcode::MultiplyFloat, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::Divide: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::DivideInt, stack[stack.len - 2].type) ||
                          EmitOperator2(bk_PrimitiveType::Float, bk_Opcode::DivideFloat, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::Modulo: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::ModuloInt, stack[stack.len - 2].type);
            } break;

            case bk_TokenKind::Equal: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::EqualInt, GetBasicType(bk_PrimitiveType::Bool)) ||
                          EmitOperator2(bk_PrimitiveType::Float, bk_Opcode::EqualFloat, GetBasicType(bk_PrimitiveType::Bool)) ||
                          EmitOperator2(bk_PrimitiveType::Bool, bk_Opcode::EqualBool, GetBasicType(bk_PrimitiveType::Bool)) ||
                          EmitOperator2(bk_PrimitiveType::Type, bk_Opcode::EqualType, GetBasicType(bk_PrimitiveType::Bool));
            } break;
            case bk_TokenKind::NotEqual: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::NotEqualInt, GetBasicType(bk_PrimitiveType::Bool)) ||
                          EmitOperator2(bk_PrimitiveType::Float, bk_Opcode::NotEqualFloat, GetBasicType(bk_PrimitiveType::Bool)) ||
                          EmitOperator2(bk_PrimitiveType::Bool, bk_Opcode::NotEqualBool, GetBasicType(bk_PrimitiveType::Bool)) ||
                          EmitOperator2(bk_PrimitiveType::Type, bk_Opcode::NotEqualType, GetBasicType(bk_PrimitiveType::Bool));
            } break;
            case bk_TokenKind::Greater: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::GreaterThanInt, GetBasicType(bk_PrimitiveType::Bool)) ||
                          EmitOperator2(bk_PrimitiveType::Float, bk_Opcode::GreaterThanFloat, GetBasicType(bk_PrimitiveType::Bool));
            } break;
            case bk_TokenKind::GreaterOrEqual: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::GreaterOrEqualInt, GetBasicType(bk_PrimitiveType::Bool)) ||
                          EmitOperator2(bk_PrimitiveType::Float, bk_Opcode::GreaterOrEqualFloat, GetBasicType(bk_PrimitiveType::Bool));
            } break;
            case bk_TokenKind::Less: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::LessThanInt, GetBasicType(bk_PrimitiveType::Bool)) ||
                          EmitOperator2(bk_PrimitiveType::Float, bk_Opcode::LessThanFloat, GetBasicType(bk_PrimitiveType::Bool));
            } break;
            case bk_TokenKind::LessOrEqual: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::LessOrEqualInt, GetBasicType(bk_PrimitiveType::Bool)) ||
                          EmitOperator2(bk_PrimitiveType::Float, bk_Opcode::LessOrEqualFloat, GetBasicType(bk_PrimitiveType::Bool));
            } break;

            case bk_TokenKind::And: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::AndInt, stack[stack.len - 2].type) ||
                          EmitOperator2(bk_PrimitiveType::Bool, bk_Opcode::AndBool, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::Or: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::OrInt, stack[stack.len - 2].type) ||
                          EmitOperator2(bk_PrimitiveType::Bool, bk_Opcode::OrBool, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::XorOrComplement: {
                if (op.unary) {
                    success = EmitOperator1(bk_PrimitiveType::Int, bk_Opcode::ComplementInt, stack[stack.len - 1].type) ||
                              EmitOperator1(bk_PrimitiveType::Bool, bk_Opcode::NotBool, stack[stack.len - 1].type);
                } else {
                    success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::XorInt, stack[stack.len - 1].type) ||
                              EmitOperator2(bk_PrimitiveType::Bool, bk_Opcode::NotEqualBool, stack[stack.len - 1].type);
                }
            } break;
            case bk_TokenKind::LeftShift: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::LeftShiftInt, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::RightShift: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::RightShiftInt, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::LeftRotate: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::LeftRotateInt, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::RightRotate: {
                success = EmitOperator2(bk_PrimitiveType::Int, bk_Opcode::RightRotateInt, stack[stack.len - 2].type);
            } break;

            case bk_TokenKind::Not: {
                success = EmitOperator1(bk_PrimitiveType::Bool, bk_Opcode::NotBool, stack[stack.len - 1].type);
            } break;
            case bk_TokenKind::AndAnd: {
                success = EmitOperator2(bk_PrimitiveType::Bool, bk_Opcode::AndBool, stack[stack.len - 2].type);

                RG_ASSERT(op.branch_addr && ir[op.branch_addr].code == bk_Opcode::SkipIfFalse);
                ir[op.branch_addr].u.i = ir.len - op.branch_addr;
            } break;
            case bk_TokenKind::OrOr: {
                success = EmitOperator2(bk_PrimitiveType::Bool, bk_Opcode::OrBool, stack[stack.len - 2].type);

                RG_ASSERT(op.branch_addr && ir[op.branch_addr].code == bk_Opcode::SkipIfTrue);
                ir[op.branch_addr].u.i = ir.len - op.branch_addr;
            } break;

            default: { RG_UNREACHABLE(); } break;
        }
    }

    if (RG_UNLIKELY(!success)) {
        if (op.unary) {
            MarkError(op.pos, "Cannot use '%1' operator on %2 value",
                      bk_TokenKindNames[(int)op.kind], stack[stack.len - 1].type->signature);
        } else if (stack[stack.len - 2].type == stack[stack.len - 1].type) {
            MarkError(op.pos, "Cannot use '%1' operator on %2 values",
                      bk_TokenKindNames[(int)op.kind], stack[stack.len - 2].type->signature);
        } else {
            MarkError(op.pos, "Cannot use '%1' operator on %2 and %3 values",
                      bk_TokenKindNames[(int)op.kind], stack[stack.len - 2].type->signature,
                      stack[stack.len - 1].type->signature);
        }
    }
}

bool bk_Parser::EmitOperator1(bk_PrimitiveType in_primitive, bk_Opcode code, const bk_TypeInfo *out_type)
{
    const bk_TypeInfo *type = stack[stack.len - 1].type;

    if (type->primitive == in_primitive) {
        ir.Append({code});
        stack[stack.len - 1] = {out_type};

        return true;
    } else {
        return false;
    }
}

bool bk_Parser::EmitOperator2(bk_PrimitiveType in_primitive, bk_Opcode code, const bk_TypeInfo *out_type)
{
    const bk_TypeInfo *type1 = stack[stack.len - 2].type;
    const bk_TypeInfo *type2 = stack[stack.len - 1].type;

    if (type1->primitive == in_primitive && type2->primitive == in_primitive && type1 == type2) {
        ir.Append({code});
        stack[--stack.len - 1] = {out_type};

        return true;
    } else {
        return false;
    }
}

void bk_Parser::EmitLoad(const bk_VariableInfo &var)
{
    if (var.global) {
        if (RG_UNLIKELY(current_func && current_func->earliest_call_addr < var.ready_addr)) {
            MarkError(definitions_map.FindValue(current_func, -1), "Function '%1' may be called before variable '%2' exists",
                      current_func->name, var.name);
            HintError(current_func->earliest_call_pos, "Function call happens here (it could be indirect)");
            HintDefinition(&var, "Variable '%1' is defined here", var.name);
        }

        switch (var.type->primitive) {
            case bk_PrimitiveType::Null: { ir.Append({bk_Opcode::PushNull}); } break;
            case bk_PrimitiveType::Bool: { ir.Append({bk_Opcode::LoadBool, {.i = var.offset}}); } break;
            case bk_PrimitiveType::Int: { ir.Append({bk_Opcode::LoadInt, {.i = var.offset}}); } break;
            case bk_PrimitiveType::Float: { ir.Append({bk_Opcode::LoadFloat, {.i = var.offset}});} break;
            case bk_PrimitiveType::Type: { ir.Append({bk_Opcode::LoadType, {.i = var.offset}}); } break;
        }
    } else {
        switch (var.type->primitive) {
            case bk_PrimitiveType::Null: { ir.Append({bk_Opcode::PushNull}); } break;
            case bk_PrimitiveType::Bool: { ir.Append({bk_Opcode::LoadLocalBool, {.i = var.offset}}); } break;
            case bk_PrimitiveType::Int: { ir.Append({bk_Opcode::LoadLocalInt, {.i = var.offset}}); } break;
            case bk_PrimitiveType::Float: { ir.Append({bk_Opcode::LoadLocalFloat, {.i = var.offset}});} break;
            case bk_PrimitiveType::Type: { ir.Append({bk_Opcode::LoadLocalType, {.i = var.offset}}); } break;
        }
    }

    stack.Append({var.type, &var});
}

// Don't try to call from outside ParseExpression()!
bool bk_Parser::ParseCall(const char *name)
{
    // We only need to store types, but TestOverload() wants bk_FunctionInfo::Parameter.
    LocalArray<bk_FunctionInfo::Parameter, RG_LEN(bk_FunctionInfo::params.data)> args;

    Size call_pos = pos - 2;
    Size call_addr = ir.len;

    bk_FunctionInfo *func0 = program->functions_map.FindValue(name, nullptr);
    if (RG_UNLIKELY(!func0)) {
        MarkError(call_pos, "Function '%1' does not exist", name);
        return false;
    }

    // Parse arguments
    if (!MatchToken(bk_TokenKind::RightParenthesis)) {
        do {
            SkipNewLines();

            if (RG_UNLIKELY(!args.Available())) {
                MarkError(pos, "Functions cannot take more than %1 arguments", RG_LEN(args.data));
                return false;
            }

            const bk_TypeInfo *type = ParseExpression(true).type;

            args.Append({nullptr, type});
            if (func0->variadic && args.len > func0->params.len) {
                ir.Append({bk_Opcode::PushType, {.type = type}});
            }
        } while (MatchToken(bk_TokenKind::Comma));

        SkipNewLines();
        ConsumeToken(bk_TokenKind::RightParenthesis);
    }
    if (func0->variadic) {
        ir.Append({bk_Opcode::PushInt, {.i = args.len - func0->params.len}});
    }

    // Find appropriate overload. Variadic functions cannot be overloaded but it
    // does not hurt to use the same logic to check argument types.
    bk_FunctionInfo *func = func0;
    while (!TestOverload(*func, args)) {
        func = func->overload_next;

        if (RG_UNLIKELY(func == func0)) {
            LocalArray<char, 1024> buf = {};
            for (Size i = 0; i < args.len; i++) {
                buf.len += Fmt(buf.TakeAvailable(), "%1%2", i ? ", " : "", args[i].type->signature).len;
            }

            MarkError(call_pos, "Cannot call '%1' with (%2) arguments", func0->name, buf);

            // Show all candidate functions with same name
            const bk_FunctionInfo *it = func0;
            do {
                HintError(definitions_map.FindValue(it, -1), "Candidate '%1'", it->signature);
                it = it->overload_next;
            } while (it != func0);

            return false;
        }
    }

    // Emit intrinsic or call
    switch (func->mode) {
        case bk_FunctionInfo::Mode::Intrinsic: {
            EmitIntrinsic(name, call_addr, args);
        } break;
        case bk_FunctionInfo::Mode::Native: {
            ir.Append({bk_Opcode::CallNative, {.func = func}});
        } break;
        case bk_FunctionInfo::Mode::Blik: {
            if (func->addr < 0) {
                if (current_func && current_func != func) {
                    func->earliest_call_pos = std::min(func->earliest_call_pos, current_func->earliest_call_pos);
                    func->earliest_call_addr = std::min(func->earliest_call_addr, current_func->earliest_call_addr);
                } else {
                    func->earliest_call_pos = std::min(func->earliest_call_pos, call_pos);
                    func->earliest_call_addr = std::min(func->earliest_call_addr, ir.len);
                }
            }

            ir.Append({bk_Opcode::Call, {.func = func}});
        } break;
    }
    stack.Append({func->ret_type});

    return true;
}

void bk_Parser::EmitIntrinsic(const char *name, Size call_addr, Span<const bk_FunctionInfo::Parameter> args)
{
    if (TestStr(name, "Float")) {
        if (args[0].type->primitive == bk_PrimitiveType::Int) {
            ir.Append({bk_Opcode::IntToFloat});
        }
    } else if (TestStr(name, "Int")) {
        if (args[0].type->primitive == bk_PrimitiveType::Float) {
            ir.Append({bk_Opcode::FloatToInt});
        }
    } else if (TestStr(name, "typeOf")) {
        // XXX: We can change the signature from typeOf(...) to typeOf(Any) after Any
        // is implemented, and remove this check.
        if (RG_UNLIKELY(args.len != 1)) {
            LogError("Intrinsic function typeOf() takes one argument");
            return;
        }

        // typeOf() does not execute anything!
        TrimInstructions(ir.len - call_addr);

        ir.Append({bk_Opcode::PushType, {.type = args[0].type}});
    } else {
        RG_UNREACHABLE();
    }
}

bool bk_Parser::ParseExpressionOfType(const bk_TypeInfo *type)
{
    Size expr_pos = pos;

    const bk_TypeInfo *type2 = ParseExpression(true).type;
    if (RG_UNLIKELY(type2 != type)) {
        MarkError(expr_pos, "Expected expression result type to be %1, not %2",
                  type->signature, type2->signature);
        return false;
    }

    return true;
}

void bk_Parser::DiscardResult()
{
    switch (ir[ir.len - 1].code) {
        case bk_Opcode::PushNull:
        case bk_Opcode::PushBool:
        case bk_Opcode::PushInt:
        case bk_Opcode::PushFloat:
        case bk_Opcode::PushType:
        case bk_Opcode::LoadBool:
        case bk_Opcode::LoadInt:
        case bk_Opcode::LoadFloat:
        case bk_Opcode::LoadType:
        case bk_Opcode::LoadLocalBool:
        case bk_Opcode::LoadLocalInt:
        case bk_Opcode::LoadLocalFloat:
        case bk_Opcode::LoadLocalType: { TrimInstructions(1); } break;

        case bk_Opcode::CopyBool: { ir[ir.len - 1].code = bk_Opcode::StoreBool; } break;
        case bk_Opcode::CopyInt: { ir[ir.len - 1].code = bk_Opcode::StoreInt; } break;
        case bk_Opcode::CopyFloat: { ir[ir.len - 1].code = bk_Opcode::StoreFloat; } break;
        case bk_Opcode::CopyType: { ir[ir.len - 1].code = bk_Opcode::StoreType; } break;
        case bk_Opcode::CopyLocalBool: { ir[ir.len - 1].code = bk_Opcode::StoreLocalBool; } break;
        case bk_Opcode::CopyLocalInt: { ir[ir.len - 1].code = bk_Opcode::StoreLocalInt; } break;
        case bk_Opcode::CopyLocalFloat: { ir[ir.len - 1].code = bk_Opcode::StoreLocalFloat; } break;
        case bk_Opcode::CopyLocalType: { ir[ir.len - 1].code = bk_Opcode::StoreLocalType; } break;

        default: { EmitPop(1); } break;
    }
}

void bk_Parser::EmitPop(int64_t count)
{
    RG_ASSERT(count >= 0);

    if (count) {
        ir.Append({bk_Opcode::Pop, {.i = count}});
    }
}

void bk_Parser::EmitReturn()
{
    RG_ASSERT(current_func);

    // We support tail recursion elimination (TRE)
    if (ir[ir.len - 1].code == bk_Opcode::Call && ir[ir.len - 1].u.func == current_func) {
        ir.len--;

        for (Size i = current_func->params.len - 1; i >= 0; i--) {
            const bk_FunctionInfo::Parameter &param = current_func->params[i];

            switch (param.type->primitive) {
                case bk_PrimitiveType::Null: {} break;
                case bk_PrimitiveType::Bool: { ir.Append({bk_Opcode::StoreLocalBool, {.i = i}}); } break;
                case bk_PrimitiveType::Int: { ir.Append({bk_Opcode::StoreLocalInt, {.i = i}}); } break;
                case bk_PrimitiveType::Float: { ir.Append({bk_Opcode::StoreLocalFloat, {.i = i}}); } break;
                case bk_PrimitiveType::Type: { ir.Append({bk_Opcode::StoreLocalType, {.i = i}}); } break;
            }
        }

        EmitPop(var_offset - current_func->params.len);
        ir.Append({bk_Opcode::Jump, {.i = current_func->addr - ir.len}});

        current_func->tre = true;
    } else {
        ir.Append({bk_Opcode::Return});
    }
}

void bk_Parser::DestroyVariables(Size count)
{
    Size first_idx = program->variables.len - count;

    for (Size i = program->variables.len - 1; i >= first_idx; i--) {
        const bk_VariableInfo &var = program->variables[i];
        bk_VariableInfo **ptr = program->variables_map.Find(var.name);

        if (var.shadow) {
            *ptr = (bk_VariableInfo *)var.shadow;
        } else {
            program->variables_map.Remove(ptr);
        }

        poisoned_set.Remove(&var);
    }

    program->variables.RemoveLast(count);
}

void bk_Parser::FixJumps(Size jump_addr, Size target_addr)
{
    while (jump_addr >= 0) {
        Size next_addr = ir[jump_addr].u.i;
        ir[jump_addr].u.i = target_addr - jump_addr;
        jump_addr = next_addr;
    }
}

void bk_Parser::TrimInstructions(Size count)
{
    if (RG_LIKELY(ir.len - prev_ir_len >= count)) {
        ir.RemoveLast(count);

        if (src->lines.len > 0 && src->lines[src->lines.len - 1].addr > ir.len) {
            bk_SourceInfo::Line line = src->lines[src->lines.len - 1];
            line.addr = ir.len;

            do {
                src->lines.len--;
            } while (src->lines.len > 0 && src->lines[src->lines.len - 1].addr >= ir.len);

            src->lines.Append(line);
        }
    }
}

bool bk_Parser::TestOverload(const bk_FunctionInfo &proto, Span<const bk_FunctionInfo::Parameter> params)
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

bool bk_Parser::ConsumeToken(bk_TokenKind kind)
{
    if (RG_UNLIKELY(pos >= tokens.len)) {
        if (out_report && valid) {
            out_report->unexpected_eof = true;
        }
        MarkError(pos, "Unexpected end of file, expected '%1'", bk_TokenKindNames[(int)kind]);

        return false;
    }

    Size prev_pos = pos++;

    if (RG_UNLIKELY(tokens[prev_pos].kind != kind)) {
        MarkError(prev_pos, "Unexpected token '%1', expected '%2'",
                  bk_TokenKindNames[(int)tokens[prev_pos].kind], bk_TokenKindNames[(int)kind]);
        return false;
    }

    return true;
}

const char *bk_Parser::ConsumeIdentifier()
{
    if (RG_LIKELY(ConsumeToken(bk_TokenKind::Identifier))) {
        return InternString(tokens[pos - 1].u.str);
    } else {
        return "";
    }
}

bool bk_Parser::MatchToken(bk_TokenKind kind)
{
    bool match = pos < tokens.len && tokens[pos].kind == kind;
    pos += match;

    return match;
}

bool bk_Parser::PeekToken(bk_TokenKind kind)
{
    bool match = pos < tokens.len && tokens[pos].kind == kind;
    return match;
}

bool bk_Parser::EndStatement()
{
    if (RG_UNLIKELY(pos >= tokens.len)) {
        MarkError(pos, "Unexpected end of file, expected end of statement");
        return false;
    }

    if (RG_UNLIKELY(tokens[pos].kind != bk_TokenKind::EndOfLine &&
                    tokens[pos].kind != bk_TokenKind::Semicolon)) {
        MarkError(pos, "Unexpected token '%1', expected end of statement",
                  bk_TokenKindNames[(int)tokens[pos].kind]);

        // Find next statement to recover (for error report)
        do {
            pos++;
        } while (pos < tokens.len && tokens[pos].kind != bk_TokenKind::EndOfLine &&
                                     tokens[pos].kind != bk_TokenKind::Semicolon);

        return false;
    }

    pos++;
    return true;
}

bool bk_Parser::SkipNewLines()
{
    if (MatchToken(bk_TokenKind::EndOfLine)) {
        while (MatchToken(bk_TokenKind::EndOfLine));

        if (RG_LIKELY(pos < tokens.len)) {
            src->lines.Append({ir.len, tokens[pos].line});
        }

        return true;
    } else {
        return false;
    }
}

const char *bk_Parser::InternString(const char *str)
{
    std::pair<const char **, bool> ret = strings.TrySet(str);
    if (ret.second) {
        *ret.first = DuplicateString(str, &program->str_alloc).ptr;
    }
    return *ret.first;
}

}
