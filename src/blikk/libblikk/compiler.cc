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
    Size indirect_addr;
};

class bk_Parser {
    RG_DELETE_COPY(bk_Parser)

    // All these members are relevant to the current parse only, and get resetted each time
    const bk_TokenizedFile *file;
    bk_CompileReport *out_report; // Can be NULL
    bool preparse;
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

    void AddFunction(const char *prototype, std::function<bk_NativeFunction> native);
    bk_VariableInfo *AddGlobal(const char *name, const bk_TypeInfo *type,
                               Span<const bk_PrimitiveValue> values, bool mut, bk_VariableInfo::Scope scope);

private:
    void ParsePrototypes(Span<const Size> funcs);

    // These 3 functions return true if (and only if) all code paths have a return statement.
    // For simplicity, return statements inside loops are not considered.
    bool ParseBlock();
    bool ParseStatement();
    bool ParseDo();

    void ParseFunction(const PrototypeInfo *proto);
    void ParseReturn();
    void ParseLet();
    void PushDefaultValue(Size var_pos, const bk_VariableInfo &var, const bk_TypeInfo *type);
    bool ParseIf();
    void ParseWhile();
    void ParseFor();

    void ParseBreak();
    void ParseContinue();

    StackSlot ParseExpression(bool tolerate_assign);
    bool ParseExpression(const bk_TypeInfo *type);
    void ProduceOperator(const PendingOperator &op);
    bool EmitOperator1(bk_PrimitiveKind in_primitive, bk_Opcode code, const bk_TypeInfo *out_type);
    bool EmitOperator2(bk_PrimitiveKind in_primitive, bk_Opcode code, const bk_TypeInfo *out_type);
    const bk_FunctionTypeInfo *ParseFunctionType();
    const bk_ArrayTypeInfo *ParseArrayType();
    void ParseArraySubscript();
    bool ParseCall(const bk_FunctionTypeInfo *func_type, const bk_FunctionInfo *func, bool overload);
    void EmitIntrinsic(const char *name, Size call_pos, Size call_addr, Span<const bk_TypeInfo *const> args);
    void EmitLoad(const bk_VariableInfo &var);

    const bk_TypeInfo *ParseTypeExpression();

    void DiscardResult();

    void EmitPop(int64_t count);
    void EmitReturn();

    void DestroyVariables(Size count);

    void FixJumps(Size jump_addr, Size target_addr);
    void TrimInstructions(Size count);

    bool TestOverload(const bk_FunctionTypeInfo &func_type, Span<const bk_TypeInfo *const> params);

    bool ConsumeToken(bk_TokenKind kind);
    const char *ConsumeIdentifier();
    bool MatchToken(bk_TokenKind kind);
    bool PeekToken(bk_TokenKind kind);

    bool EndStatement();
    bool SkipNewLines();

    const char *InternString(const char *str);
    template<typename T>
    const bk_TypeInfo *InsertType(const T &type_buf, BucketArray<T> *out_types);

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

void bk_Compiler::AddFunction(const char *prototype, std::function<bk_NativeFunction> native)
{
    RG_ASSERT(native);
    parser->AddFunction(prototype, native);
}

void bk_Compiler::AddGlobal(const char *name, const bk_TypeInfo *type, Span<const bk_PrimitiveValue> values, bool mut)
{
    parser->AddGlobal(name, type, values, mut, bk_VariableInfo::Scope::Global);
}

bk_Parser::bk_Parser(bk_Program *program)
    : program(program), ir(program->ir)
{
    RG_ASSERT(program);
    RG_ASSERT(!ir.len);

    // Base types
    for (const bk_TypeInfo &type: bk_BaseTypes) {
        AddGlobal(type.signature, bk_TypeType, {{.type = &type}}, false, bk_VariableInfo::Scope::Module);
        program->types_map.Set(&type);
    }

    // Special values
    AddGlobal("Version", bk_StringType, {{.str = FelixVersion}}, false, bk_VariableInfo::Scope::Global);
    AddGlobal("NaN", bk_FloatType, {{.d = (double)NAN}}, false, bk_VariableInfo::Scope::Global);
    AddGlobal("Inf", bk_FloatType, {{.d = (double)INFINITY}}, false, bk_VariableInfo::Scope::Global);

    // Intrinsics
    AddFunction("toFloat(Int): Float", {});
    AddFunction("toFloat(Float): Float", {});
    AddFunction("toInt(Int): Int", {});
    AddFunction("toInt(Float): Int", {});
    AddFunction("typeOf(...): Type", {});
}

bool bk_Parser::Parse(const bk_TokenizedFile &file, bk_CompileReport *out_report)
{
    prev_ir_len = ir.len;

    // Restore previous state if something goes wrong
    RG_DEFER_NC(err_guard, sources_len = program->sources.len,
                           prev_var_offset = var_offset,
                           variables_len = program->variables.len,
                           functions_len = program->functions.len,
                           function_types_len = program->function_types.len) {
        ir.RemoveFrom(prev_ir_len);
        program->sources.RemoveFrom(sources_len);

        var_offset = prev_var_offset;
        DestroyVariables(program->variables.len - variables_len);

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
        program->functions.RemoveFrom(functions_len);

        for (Size i = function_types_len; i < program->function_types.len; i++) {
            const bk_TypeInfo &type = program->function_types[i];
            program->types_map.Remove(type.signature);
        }
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

    // Protect IR from before this parse step
    ir.Append({bk_Opcode::Nop});

    // Preparse (top-level order-independence)
    preparse = true;
    ParsePrototypes(file.funcs);

    // Do the actual parsing!
    preparse = false;
    src->lines.Append({ir.len, 0});
    while (RG_LIKELY(pos < tokens.len)) {
        ParseStatement();
    }

    // Maybe it'll help catch bugs
    RG_ASSERT(!depth);
    RG_ASSERT(loop_offset == -1);
    RG_ASSERT(!current_func);

    if (valid) {
        ir.Append({bk_Opcode::End, {}, {.i = var_offset}});
        ir.Trim();

        err_guard.Disable();
    }

    return valid;
}

// This is not exposed to user scripts, and the validation of prototype is very light,
// with a few debug-only asserts. Bad function names (even invalid UTF-8 sequences)
// will go right through. Don't pass in garbage!
void bk_Parser::AddFunction(const char *prototype, std::function<bk_NativeFunction> native)
{
    bk_FunctionInfo *func = program->functions.AppendDefault();

    // Reserve some space at the beginning to make sure we can replace the name with 'func '
    HeapArray<char> buf;
    char *ptr;
    Fmt(&buf, "     %1", prototype);
    ptr = buf.ptr + 5;

    // Function name and signature
    const char *signature;
    {
        Size offset = strcspn(ptr, "(");
        RG_ASSERT(offset > 0 && ptr[offset] == '(');

        ptr[offset] = 0;
        func->name = InternString(ptr);

        ptr[offset] = '(';
        memcpy(buf.ptr + offset, "func ", 5);
        signature = InternString(buf.ptr + offset);

        ptr += offset;
    }

    func->prototype = InternString(prototype);
    func->mode = native ? bk_FunctionInfo::Mode::Native : bk_FunctionInfo::Mode::Intrinsic;
    func->native = native;
    func->addr = -1;

    // Reuse or create function type
    {
        const bk_TypeInfo *type = program->types_map.FindValue(signature, nullptr);

        if (type) {
            func->type = type->AsFunctionType();

            for (const bk_TypeInfo *type: func->type->params) {
                func->params.Append({"", type});
            }
        } else {
            bk_FunctionTypeInfo *func_type = program->function_types.AppendDefault();

            func_type->primitive = bk_PrimitiveKind::Function;
            func_type->signature = signature;

            if (ptr[1] != ')') {
                do {
                    Size len = strcspn(++ptr, ",)");
                    RG_ASSERT(ptr[len]);

                    char c = ptr[len];
                    ptr[len] = 0;

                    if (TestStr(ptr, "...")) {
                        RG_ASSERT(c == ')');
                        func_type->variadic = true;
                    } else {
                        const bk_TypeInfo *type = program->types_map.FindValue(ptr, nullptr);
                        RG_ASSERT(type);

                        func->params.Append({"", type});
                        func_type->params.Append(type);
                    }

                    ptr[len] = c;
                    ptr += len;
                } while ((ptr++)[0] != ')');
            } else {
                ptr += 2;
            }
            if (ptr[0] == ':') {
                RG_ASSERT(ptr[1] == ' ');

                func_type->ret_type = program->types_map.FindValue(ptr + 2, nullptr);
                RG_ASSERT(func_type->ret_type);
            } else {
                func_type->ret_type = bk_NullType;
            }

            program->types_map.Set(func_type);
            func->type = func_type;
        }
    }

    // Publish it!
    {
        bk_FunctionInfo *head = *program->functions_map.TrySet(func).first;

        if (head != func) {
            RG_ASSERT(!head->type->variadic && !func->type->variadic);

            head->overload_prev->overload_next = func;
            func->overload_next = head;
            func->overload_prev = head->overload_prev;
            head->overload_prev = func;

#ifndef NDEBUG
            do {
                RG_ASSERT(!TestOverload(*head->type, func->type->params));
                head = head->overload_next;
            } while (head != func);
#endif
        } else {
            func->overload_prev = func;
            func->overload_next = func;

            AddGlobal(func->name, func->type, {{.func = func}}, false, bk_VariableInfo::Scope::Module);
        }
    }
}

bk_VariableInfo *bk_Parser::AddGlobal(const char *name, const bk_TypeInfo *type,
                                      Span<const bk_PrimitiveValue> values, bool mut, bk_VariableInfo::Scope scope)
{
    bk_VariableInfo *var = program->variables.AppendDefault();

    var->name = InternString(name);
    var->type = type;
    var->mut = mut;
    var->scope = scope;

    var_offset += values.len;
    var->offset = var_offset - 1;

    for (const bk_PrimitiveValue &value: values) {
        ir.Append({bk_Opcode::Push, type->primitive, value});
    }
    var->ready_addr = ir.len;

    program->variables_map.TrySet(var);
    return var;
}

void bk_Parser::ParsePrototypes(Span<const Size> funcs)
{
    RG_ASSERT(!prototypes_map.count);
    RG_ASSERT(pos == 0);

    Size prev_ir_len = ir.len;
    Size prev_lines_len = src->lines.len;

    // Reuse for performance
    HeapArray<char> signature_buf;
    HeapArray<char> prototype_buf;
    bk_FunctionTypeInfo type_buf = {};

    HeapArray<const PrototypeInfo *> prototypes;

    for (Size i = 0; i < funcs.len; i++) {
        pos = funcs[i] + 1;
        show_errors = true;

        if (!PeekToken(bk_TokenKind::Identifier))
            continue;

        PrototypeInfo *proto = prototypes_map.SetDefault(funcs[i]);
        bk_FunctionInfo *func = program->functions.AppendDefault();
        definitions_map.Set(func, pos);
        prototypes.Append(proto);

        proto->pos = funcs[i];
        proto->func = func;
        func->name = ConsumeIdentifier();
        func->mode = bk_FunctionInfo::Mode::blikk;

        // Clean up parameter variables once we're done
        RG_DEFER_C(variables_len = program->variables.len) {
            DestroyVariables(program->variables.len - variables_len);
        };

        // Reset buffers
        signature_buf.RemoveFrom(0);
        signature_buf.Append("func (");
        prototype_buf.RemoveFrom(0);
        Fmt(&prototype_buf, "%1(", func->name);
        type_buf.primitive = bk_PrimitiveKind::Function;
        type_buf.params.RemoveFrom(0);

        // Parameters
        ConsumeToken(bk_TokenKind::LeftParenthesis);
        if (!MatchToken(bk_TokenKind::RightParenthesis)) {
            for (;;) {
                SkipNewLines();

                bk_VariableInfo *var = program->variables.AppendDefault();
                Size param_pos = pos;

                var->mut = MatchToken(bk_TokenKind::Mut);
                var->name = ConsumeIdentifier();
                var->scope = bk_VariableInfo::Scope::Local;

                std::pair<bk_VariableInfo **, bool> ret = program->variables_map.TrySet(var);
                if (RG_UNLIKELY(!ret.second)) {
                    const bk_VariableInfo *prev_var = *ret.first;
                    var->shadow = prev_var;

                    // Error messages for globals are issued in ParseFunction(), because at
                    // this stage some globals (those issues from the same Parse call) may not
                    // yet exist. Better issue all errors about that later.
                    if (prev_var->scope == bk_VariableInfo::Scope::Local) {
                        MarkError(param_pos, "Parameter named '%1' already exists", var->name);
                    }
                }

                ConsumeToken(bk_TokenKind::Colon);
                var->type = ParseTypeExpression();

                if (RG_LIKELY(func->params.Available())) {
                    bk_FunctionInfo::Parameter *param = func->params.AppendDefault();
                    definitions_map.Set(param, param_pos);

                    param->name = var->name;
                    param->type = var->type;
                    param->mut = var->mut;

                    type_buf.params.Append(var->type);
                } else {
                    MarkError(pos - 1, "Functions cannot have more than %1 parameters", RG_LEN(type_buf.params.data));
                }

                signature_buf.Append(var->type->signature);
                Fmt(&prototype_buf, "%1: %2", var->name, var->type->signature);

                if (MatchToken(bk_TokenKind::Comma)) {
                    signature_buf.Append(", ");
                    prototype_buf.Append(", ");
                } else {
                    break;
                }
            }

            SkipNewLines();
            ConsumeToken(bk_TokenKind::RightParenthesis);
        }
        signature_buf.Append(')');
        prototype_buf.Append(')');

        // Return type
        if (MatchToken(bk_TokenKind::Colon)) {
            type_buf.ret_type = ParseTypeExpression();

            if (type_buf.ret_type != bk_NullType) {
                Fmt(&signature_buf, ": %1", type_buf.ret_type->signature);
                Fmt(&prototype_buf, ": %1", type_buf.ret_type->signature);
            } else {
                signature_buf.Append(0);
                prototype_buf.Append(0);
            }
        } else {
            type_buf.ret_type = bk_NullType;

            signature_buf.Append(0);
            prototype_buf.Append(0);
        }

        proto->body_pos = pos;

        // Reuse or add function type
        type_buf.signature = InternString(signature_buf.ptr);
        func->type = InsertType(type_buf, &program->function_types)->AsFunctionType();
        func->prototype = InternString(prototype_buf.ptr);

        // We don't know where it will live yet!
        func->addr = -1;
        func->earliest_ref_pos = RG_SIZE_MAX;
        func->earliest_ref_addr = RG_SIZE_MAX;

        // Publish it!
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
    }

    // This is a preparse step, clean up main side effets
    pos = 0;
    ir.RemoveFrom(prev_ir_len);
    src->lines.RemoveFrom(prev_lines_len);

    // Publish function symbols
    for (const PrototypeInfo *proto: prototypes) {
        const bk_FunctionInfo &func = *proto->func;
        const bk_VariableInfo *var = AddGlobal(func.name, func.type, {{.func = &func}}, false, bk_VariableInfo::Scope::Module);
        definitions_map.Set(var, proto->pos);
    }
}

template<typename T>
const bk_TypeInfo *bk_Parser::InsertType(const T &type_buf, BucketArray<T> *out_types)
{
    std::pair<const bk_TypeInfo **, bool> ret = program->types_map.TrySetDefault(type_buf.signature);

    const bk_TypeInfo *type;
    if (ret.second) {
        type = out_types->Append(type_buf);
        *ret.first = type;
    } else {
        type = *ret.first;
    }

    return type;
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
            const PrototypeInfo *proto = prototypes_map.Find(pos);

            if (proto) {
                ParseFunction(proto);
            } else {
                ParseExpression(true);
                DiscardResult();
            }

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

void bk_Parser::ParseFunction(const PrototypeInfo *proto)
{
    Size func_pos = ++pos;

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
        var->scope = bk_VariableInfo::Scope::Local;

        var->offset = var_offset;
        var_offset++;

        std::pair<bk_VariableInfo **, bool> ret = program->variables_map.TrySet(var);
        if (RG_UNLIKELY(!ret.second)) {
            const bk_VariableInfo *prev_var = *ret.first;
            var->shadow = prev_var;

            // We should already have issued an error message for the other case (duplicate
            // parameter name) in ParsePrototypes().
            if (prev_var->scope == bk_VariableInfo::Scope::Module && prev_var->type->primitive == bk_PrimitiveKind::Function) {
                MarkError(param_pos, "Parameter '%1' is not allowed to hide function", var->name);
                HintDefinition(prev_var, "Function '%1' is defined here", prev_var->name);
            } else if (prev_var->scope == bk_VariableInfo::Scope::Global) {
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
            if (TestOverload(*overload->type, func->type->params)) {
                if (overload->type->ret_type == func->type->ret_type) {
                    MarkError(func_pos, "Function '%1' is already defined", func->prototype);
                } else {
                    MarkError(func_pos, "Function '%1' only differs from previously defined '%2' by return type",
                              func->prototype, overload->prototype);
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
        if (func->type->ret_type == bk_NullType) {
            ir.Append({bk_Opcode::Push, bk_PrimitiveKind::Null});
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

    StackSlot slot;
    if (PeekToken(bk_TokenKind::EndOfLine) || PeekToken(bk_TokenKind::Semicolon)) {
        ir.Append({bk_Opcode::Push, bk_PrimitiveKind::Null});
        slot = {bk_NullType};
    } else {
        slot = ParseExpression(true);
    }

    if (RG_UNLIKELY(slot.type != current_func->type->ret_type)) {
        MarkError(return_pos, "Cannot return '%1' value in function defined to return '%2'",
                  slot.type->signature, current_func->type->ret_type->signature);
        return;
    }
    if (RG_UNLIKELY(slot.type->PassByReference() && slot.var &&
                    slot.var->scope == bk_VariableInfo::Scope::Local)) {
        MarkError(return_pos, "Cannot return reference to local '%1' value", slot.type->signature);
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

        if (prev_var->scope == bk_VariableInfo::Scope::Module && prev_var->type->primitive == bk_PrimitiveKind::Function) {
            MarkError(var_pos, "Declaration '%1' is not allowed to hide function", var->name);
            HintDefinition(prev_var, "Function '%1' is defined here", prev_var->name);
        } else if (current_func && prev_var->scope == bk_VariableInfo::Scope::Global) {
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
        const bk_TypeInfo *type = ParseTypeExpression();

        if (MatchToken(bk_TokenKind::Assign)) {
            SkipNewLines();

            Size expr_pos = pos;
            slot = ParseExpression(true);

            if (RG_UNLIKELY(slot.type != type)) {
                MarkError(expr_pos - 1, "Cannot assign '%1' value to variable '%2' (defined as '%3')",
                          slot.type->signature, var->name, type->signature);
            }
        } else {
            Size offset = var_offset;

            PushDefaultValue(var_pos, *var, type);
            if (type->PassByReference()) {
                bk_Opcode code = (var->scope != bk_VariableInfo::Scope::Local) ? bk_Opcode::Lea : bk_Opcode::LeaLocal;
                ir.Append({code, type->primitive, {.i = offset}});
            }

            slot = {type};
        }
    }

    var->type = slot.type;
    if (slot.var && !slot.var->mut && !var->mut && var->ready_addr > 0) {
        // We're about to alias var to slot.var... we need to drop the load instruction.
        // Is it enough, and is it safe?
        TrimInstructions(1);

        var->scope = slot.var->scope;
        var->ready_addr = slot.var->ready_addr;
        var->offset = slot.var->offset;
    } else {
        var->scope = current_func ? bk_VariableInfo::Scope::Local : bk_VariableInfo::Scope::Global;
        var->ready_addr = ir.len;

        var->offset = var_offset;
        var_offset++;
    }

    // Expressions involving this variable won't issue (visible) errors
    // and will be marked as invalid too.
    if (RG_UNLIKELY(!show_errors)) {
        poisoned_set.Set(var);
    }
}

void bk_Parser::PushDefaultValue(Size var_pos, const bk_VariableInfo &var, const bk_TypeInfo *type)
{
    switch (type->primitive) {
        case bk_PrimitiveKind::Null:
        case bk_PrimitiveKind::Boolean:
        case bk_PrimitiveKind::Integer:
        case bk_PrimitiveKind::Float: { ir.Append({bk_Opcode::Push, type->primitive, {.i = 0}}); } break;
        case bk_PrimitiveKind::String: { ir.Append({bk_Opcode::Push, type->primitive, {.str = InternString("")}}); } break;
        case bk_PrimitiveKind::Type: { ir.Append({bk_Opcode::Push, type->primitive, {.type = bk_NullType}}); } break;
        case bk_PrimitiveKind::Function: {
            MarkError(var_pos, "Variable '%1' (defined as '%2') must be explicitely initialized",
                      var.name, type->signature);
        } break;
        case bk_PrimitiveKind::Array: {
            const bk_ArrayTypeInfo *array_type = type->AsArrayType();

            for (Size i = 0; i < array_type->len; i++) {
                PushDefaultValue(var_pos, var, array_type->unit_type);
                var_offset += (array_type->unit_type->primitive != bk_PrimitiveKind::Array);
            }
        } break;
    }
}

bool bk_Parser::ParseIf()
{
    pos++;

    ParseExpression(bk_BoolType);

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
            ir.Append({bk_Opcode::Jump, {}, {.i = -1}});

            do {
                ir[branch_addr].u.i = ir.len - branch_addr;

                if (MatchToken(bk_TokenKind::If)) {
                    ParseExpression(bk_BoolType);

                    if (RG_LIKELY(EndStatement())) {
                        branch_addr = ir.len;
                        ir.Append({bk_Opcode::BranchIfFalse});

                        has_return &= ParseBlock();

                        ir.Append({bk_Opcode::Jump, {}, {.i = jump_addr}});
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
    ParseExpression(bk_BoolType);

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

    ir.Append({bk_Opcode::BranchIfTrue, {}, {.i = branch_addr - ir.len + 1}});
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
    it->scope = bk_VariableInfo::Scope::Local;

    std::pair<bk_VariableInfo **, bool> ret = program->variables_map.TrySet(it);
    if (RG_UNLIKELY(!ret.second)) {
        const bk_VariableInfo *prev_var = *ret.first;
        it->shadow = prev_var;

        if (prev_var->scope == bk_VariableInfo::Scope::Module && prev_var->type->primitive == bk_PrimitiveKind::Function) {
            MarkError(for_pos, "Iterator '%1' is not allowed to hide function", it->name);
            HintDefinition(prev_var, "Function '%1' is defined here", prev_var->name);
        } else if (current_func && prev_var->scope == bk_VariableInfo::Scope::Global) {
            MarkError(for_pos, "Iterator '%1' is not allowed to hide global variable", it->name);
            HintDefinition(prev_var, "Global variable '%1' is defined here", prev_var->name);
        } else {
            MarkError(for_pos, "Variable '%1' already exists", it->name);
            HintDefinition(prev_var, "Previous variable '%1' is defined here", prev_var->name);
        }

        return;
    }

    ConsumeToken(bk_TokenKind::In);
    ParseExpression(bk_IntType);
    ConsumeToken(bk_TokenKind::Colon);
    ParseExpression(bk_IntType);

    // Make sure start and end value remain on the stack
    it->offset = var_offset + 2;
    var_offset += 3;

    // Put iterator value on the stack
    ir.Append({bk_Opcode::LoadLocal, bk_PrimitiveKind::Integer, {.i = it->offset - 2}});
    it->type = bk_IntType;

    Size body_addr = ir.len;

    ir.Append({bk_Opcode::LoadLocal, bk_PrimitiveKind::Integer, {.i = it->offset}});
    ir.Append({bk_Opcode::LoadLocal, bk_PrimitiveKind::Integer, {.i = it->offset - 1}});
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

    ir.Append({bk_Opcode::Push, bk_PrimitiveKind::Integer, {.i = 1}});
    ir.Append({bk_Opcode::AddInt});
    ir.Append({bk_Opcode::Jump, {}, {.i = body_addr - ir.len}});
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

    ir.Append({bk_Opcode::Jump, {}, {.i = loop_break_addr}});
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

    ir.Append({bk_Opcode::Jump, {}, {.i = loop_continue_addr}});
    loop_continue_addr = ir.len - 1;
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
    Size start_stack_len = stack.len;
    RG_DEFER { stack.RemoveFrom(start_stack_len); };

    // Safety dummy
    stack.Append({bk_NullType});

    LocalArray<PendingOperator, 128> operators;
    bool expect_value = true;
    Size parentheses = 0;

    // Used to detect "empty" expressions
    Size prev_offset = pos;

    while (RG_LIKELY(pos < tokens.len)) {
        const bk_Token &tok = tokens[pos++];

        switch (tok.kind) {
            case bk_TokenKind::LeftParenthesis: {
                if (RG_UNLIKELY(!expect_value)) {
                    if (stack[stack.len - 1].type->primitive == bk_PrimitiveKind::Function) {
                        const bk_FunctionTypeInfo *func_type = stack[stack.len - 1].type->AsFunctionType();

                        if (!ParseCall(func_type, nullptr, false))
                            goto error;
                    } else {
                        goto unexpected;
                    }
                } else {
                    operators.Append({tok.kind});
                    parentheses++;
                }
            } break;
            case bk_TokenKind::RightParenthesis: {
                if (RG_UNLIKELY(expect_value))
                    goto unexpected;
                expect_value = false;

                if (!parentheses) {
                    if (RG_UNLIKELY(pos == prev_offset + 1)) {
                        MarkError(pos - 1, "Unexpected token ')', expected value or expression",
                                  bk_TokenKindNames[(int)tokens[pos - 1].kind]);
                        goto error;
                    } else {
                        pos--;
                        goto end;
                    }
                }

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
            } break;

            case bk_TokenKind::Null: {
                if (RG_UNLIKELY(!expect_value))
                    goto unexpected;
                expect_value = false;

                ir.Append({bk_Opcode::Push, bk_PrimitiveKind::Null});
                stack.Append({bk_NullType});
            } break;
            case bk_TokenKind::Boolean: {
                if (RG_UNLIKELY(!expect_value))
                    goto unexpected;
                expect_value = false;

                ir.Append({bk_Opcode::Push, bk_PrimitiveKind::Boolean, {.b = tok.u.b}});
                stack.Append({bk_BoolType});
            } break;
            case bk_TokenKind::Integer: {
                if (RG_UNLIKELY(!expect_value))
                    goto unexpected;
                expect_value = false;

                ir.Append({bk_Opcode::Push, bk_PrimitiveKind::Integer, {.i = tok.u.i}});
                stack.Append({bk_IntType});
            } break;
            case bk_TokenKind::Float: {
                if (RG_UNLIKELY(!expect_value))
                    goto unexpected;
                expect_value = false;

                ir.Append({bk_Opcode::Push, bk_PrimitiveKind::Float, {.d = tok.u.d}});
                stack.Append({bk_FloatType});
            } break;
            case bk_TokenKind::String: {
                if (RG_UNLIKELY(!expect_value))
                    goto unexpected;
                expect_value = false;

                ir.Append({bk_Opcode::Push, bk_PrimitiveKind::String, {.str = InternString(tok.u.str)}});
                stack.Append({bk_StringType});
            } break;

            case bk_TokenKind::Func: {
                if (RG_UNLIKELY(!expect_value))
                    goto unexpected;
                expect_value = false;

                const bk_TypeInfo *type = ParseFunctionType();

                ir.Append({bk_Opcode::Push, bk_PrimitiveKind::Type, {.type = type}});
                stack.Append({bk_TypeType});
            } break;

            case bk_TokenKind::LeftBracket: {
                if (expect_value) {
                    expect_value = false;

                    const bk_TypeInfo *type = ParseArrayType();

                    ir.Append({bk_Opcode::Push, bk_PrimitiveKind::Type, {.type = type}});
                    stack.Append({bk_TypeType});
                } else if (stack[stack.len - 1].type->primitive == bk_PrimitiveKind::Array) {
                    ParseArraySubscript();
                } else {
                    MarkError(pos - 1, "Cannot subset non-array expression");
                    goto error;
                }
            } break;

            case bk_TokenKind::Identifier: {
                if (RG_UNLIKELY(!expect_value))
                    goto unexpected;
                expect_value = false;

                const bk_VariableInfo *var = program->variables_map.FindValue(tok.u.str, nullptr);
                Size var_pos = pos - 1;
                bool call = MatchToken(bk_TokenKind::LeftParenthesis);

                if (RG_LIKELY(var)) {
                    show_errors &= !poisoned_set.Find(var);

                    if (RG_LIKELY(var->type)) {
                        EmitLoad(*var);

                        if (var->scope == bk_VariableInfo::Scope::Module) {
                            if (var->type->primitive == bk_PrimitiveKind::Function) {
                                RG_ASSERT(ir[ir.len - 1].code == bk_Opcode::Push &&
                                          ir[ir.len - 1].primitive == bk_PrimitiveKind::Function);

                                bk_FunctionInfo *func = (bk_FunctionInfo *)ir[ir.len - 1].u.func;

                                if (current_func && current_func != func) {
                                    func->earliest_ref_pos = std::min(func->earliest_ref_pos, current_func->earliest_ref_pos);
                                    func->earliest_ref_addr = std::min(func->earliest_ref_addr, current_func->earliest_ref_addr);
                                } else {
                                    func->earliest_ref_pos = std::min(func->earliest_ref_pos, var_pos);
                                    func->earliest_ref_addr = std::min(func->earliest_ref_addr, ir.len);
                                }

                                if (!call) {
                                    if (RG_UNLIKELY(func->overload_next != func)) {
                                        MarkError(var_pos, "Ambiguous reference to overloaded function '%1'", var->name);

                                        // Show all candidate functions with same name
                                        const bk_FunctionInfo *it = func;
                                        do {
                                            HintError(definitions_map.FindValue(it, -1), "Candidate '%1'", it->prototype);
                                            it = it->overload_next;
                                        } while (it != func);

                                        goto error;
                                    } else if (RG_UNLIKELY(func->mode == bk_FunctionInfo::Mode::Intrinsic)) {
                                        MarkError(var_pos, "Intrinsic functions can only be called directly");
                                        goto error;
                                    }
                                }
                            }
                        } else if (RG_UNLIKELY(preparse)) {
                            MarkError(var_pos, "Top-level declaration (prototype) cannot reference variable '%1'", var->name);
                            goto error;
                        }

                        if (call) {
                            if (RG_LIKELY(var->type->primitive == bk_PrimitiveKind::Function)) {
                                if (ir[ir.len - 1].code == bk_Opcode::Push) {
                                    RG_ASSERT(ir[ir.len - 1].primitive == bk_PrimitiveKind::Function);

                                    bk_FunctionInfo *func = (bk_FunctionInfo *)ir[ir.len - 1].u.func;
                                    bool overload = (var->scope == bk_VariableInfo::Scope::Module);

                                    TrimInstructions(1);
                                    stack.len--;

                                    if (!ParseCall(var->type->AsFunctionType(), func, overload))
                                        goto error;
                                } else {
                                    if (!ParseCall(var->type->AsFunctionType(), nullptr, false))
                                        goto error;
                                }
                            } else {
                                MarkError(var_pos, "Variable '%1' is not a function and cannot be called", var->name);
                                goto error;
                            }
                        }
                    } else {
                        MarkError(var_pos, "Cannot use variable '%1' before it is defined", var->name);
                        goto error;
                    }
                } else {
                    MarkError(var_pos, "Reference to unknown identifier '%1'", tok.u.str);
                    if (preparse) {
                        HintError(-1, "Top-level declaration (prototype) cannot reference variables");
                    }
                    goto error;
                }
            } break;

            default: {
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
                        goto end;
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
            } break;
        }
    }

end:
    if (RG_UNLIKELY(expect_value || parentheses)) {
        if (valid) {
            if (out_report) {
                out_report->unexpected_eof = true;
            }
            MarkError(pos - 1, "Unexpected end of file, expected value or '('");
        }

        goto error;
    }

    // Discharge remaining operators
    for (Size i = operators.len - 1; i >= 0; i--) {
        const PendingOperator &op = operators[i];
        ProduceOperator(op);
    }

    RG_ASSERT(stack.len == start_stack_len + 2 || !show_errors);
    return stack[stack.len - 1];

unexpected:
    pos--;

    // Issue error message
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
    return {bk_NullType};
}

bool bk_Parser::ParseExpression(const bk_TypeInfo *expected_type)
{
    Size expr_pos = pos;

    const bk_TypeInfo *type = ParseExpression(true).type;
    if (RG_UNLIKELY(type != expected_type)) {
        MarkError(expr_pos, "Expected expression result type to be '%1', not '%2'",
                  expected_type->signature, type->signature);
        return false;
    }

    return true;
}

void bk_Parser::ProduceOperator(const PendingOperator &op)
{
    bool success = false;

    if (op.prec == 0) { // Assignement operators
        RG_ASSERT(!op.unary);

        StackSlot dest = stack[stack.len - 2];
        const StackSlot &expr = stack[stack.len - 1];

        if (RG_UNLIKELY(!dest.var)) {
            MarkError(op.pos, "Cannot assign result to temporary value; left operand should be a variable");
            return;
        }
        if (RG_UNLIKELY(!dest.var->mut)) {
            MarkError(op.pos, "Cannot assign result to non-mutable variable '%1'", dest.var->name);
            HintError(definitions_map.FindValue(dest.var, -1), "Variable '%1' is defined without 'mut' qualifier", dest.var->name);

            return;
        }
        if (RG_UNLIKELY(dest.type != expr.type)) {
            if (!dest.indirect_addr) {
                MarkError(op.pos, "Cannot assign '%1' value to variable '%2'", expr.type->signature, dest.var->name);
            } else {
                MarkError(op.pos, "Cannot assign '%1' value here, expected '%2'", expr.type->signature, dest.type->signature);
            }
            HintError(definitions_map.FindValue(dest.var, -1), "Variable '%1' is defined as '%2'",
                      dest.var->name, dest.var->type->signature);
            return;
        }

        switch (op.kind) {
            case bk_TokenKind::Reassign: {
                if (RG_LIKELY(!dest.type->PassByReference())) {
                    stack[--stack.len - 1].var = nullptr;
                    success = true;
                } else {
                    success = false;
                }
            } break;

            case bk_TokenKind::PlusAssign: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::AddInt, dest.type) ||
                          EmitOperator2(bk_PrimitiveKind::Float, bk_Opcode::AddFloat, dest.type);
            } break;
            case bk_TokenKind::MinusAssign: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::SubstractInt, dest.type) ||
                          EmitOperator2(bk_PrimitiveKind::Float, bk_Opcode::SubstractFloat, dest.type);
            } break;
            case bk_TokenKind::MultiplyAssign: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::MultiplyInt, dest.type) ||
                          EmitOperator2(bk_PrimitiveKind::Float, bk_Opcode::MultiplyFloat, dest.type);
            } break;
            case bk_TokenKind::DivideAssign: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::DivideInt, dest.type) ||
                          EmitOperator2(bk_PrimitiveKind::Float, bk_Opcode::DivideFloat, dest.type);
            } break;
            case bk_TokenKind::ModuloAssign: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::ModuloInt, dest.type);
            } break;
            case bk_TokenKind::AndAssign: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::AndInt, dest.type) ||
                          EmitOperator2(bk_PrimitiveKind::Boolean, bk_Opcode::AndBool, dest.type);
            } break;
            case bk_TokenKind::OrAssign: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::OrInt, dest.type) ||
                          EmitOperator2(bk_PrimitiveKind::Boolean, bk_Opcode::OrBool, dest.type);
            } break;
            case bk_TokenKind::XorAssign: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::XorInt, dest.type) ||
                          EmitOperator2(bk_PrimitiveKind::Boolean, bk_Opcode::NotEqualBool, dest.type);
            } break;
            case bk_TokenKind::LeftShiftAssign: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::LeftShiftInt, dest.type);
            } break;
            case bk_TokenKind::RightShiftAssign: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::RightShiftInt, dest.type);
            } break;
            case bk_TokenKind::LeftRotateAssign: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::LeftRotateInt, dest.type);
            } break;
            case bk_TokenKind::RightRotateAssign: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::RightRotateInt, dest.type);
            } break;

            default: { RG_UNREACHABLE(); } break;
        }

        if (dest.indirect_addr) {
            // In order for CopyArray to work, the array reference and offset must remain on the stack.
            // To do so, replace LoadArray (which removes them) with LoadIndirect.
            if (op.kind != bk_TokenKind::Reassign) {
                RG_ASSERT(ir[dest.indirect_addr].code == bk_Opcode::LoadArray);
                ir[dest.indirect_addr].code = bk_Opcode::LoadIndirect;
            }

            ir.Append({bk_Opcode::CopyArray, dest.type->primitive});
        } else {
            bk_Opcode code = (dest.var->scope != bk_VariableInfo::Scope::Local) ? bk_Opcode::Copy : bk_Opcode::CopyLocal;
            ir.Append({code, dest.type->primitive, {.i = dest.var->offset}});
        }
    } else { // Other operators
        switch (op.kind) {
            case bk_TokenKind::Plus: {
                if (op.unary) {
                    success = stack[stack.len - 1].type->primitive == bk_PrimitiveKind::Integer ||
                              stack[stack.len - 1].type->primitive == bk_PrimitiveKind::Float;
                } else {
                    success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::AddInt, stack[stack.len - 2].type) ||
                              EmitOperator2(bk_PrimitiveKind::Float, bk_Opcode::AddFloat, stack[stack.len - 2].type);
                }
            } break;
            case bk_TokenKind::Minus: {
                if (op.unary) {
                    bk_Instruction *inst = &ir[ir.len - 1];

                    switch (inst->code) {
                        case bk_Opcode::Push: {
                            if (stack[stack.len - 1].type->primitive == bk_PrimitiveKind::Integer) {
                                // In theory, this could overflow trying to negate INT64_MIN.. but we
                                // we can't have INT64_MIN, because numeric literal tokens are always
                                // positive. inst->u.i will flip between positive and negative values
                                // if we encounter successive '-' unary operators (e.g. -----64).
                                inst->u.i = -inst->u.i;
                                success = true;
                            } else if (stack[stack.len - 1].type->primitive == bk_PrimitiveKind::Float) {
                                inst->u.d = -inst->u.d;
                                success = true;
                            } else {
                                success = false;
                            }
                        } break;
                        case bk_Opcode::NegateInt:
                        case bk_Opcode::NegateFloat: {
                            TrimInstructions(1);
                            success = true;
                        } break;

                        default: {
                            success = EmitOperator1(bk_PrimitiveKind::Integer, bk_Opcode::NegateInt, stack[stack.len - 1].type) ||
                                      EmitOperator1(bk_PrimitiveKind::Float, bk_Opcode::NegateFloat, stack[stack.len - 1].type);
                        }
                    }
                } else {
                    success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::SubstractInt, stack[stack.len - 2].type) ||
                              EmitOperator2(bk_PrimitiveKind::Float, bk_Opcode::SubstractFloat, stack[stack.len - 2].type);
                }
            } break;
            case bk_TokenKind::Multiply: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::MultiplyInt, stack[stack.len - 2].type) ||
                          EmitOperator2(bk_PrimitiveKind::Float, bk_Opcode::MultiplyFloat, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::Divide: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::DivideInt, stack[stack.len - 2].type) ||
                          EmitOperator2(bk_PrimitiveKind::Float, bk_Opcode::DivideFloat, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::Modulo: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::ModuloInt, stack[stack.len - 2].type);
            } break;

            case bk_TokenKind::Equal: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::EqualInt, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Float, bk_Opcode::EqualFloat, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Boolean, bk_Opcode::EqualBool, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::String, bk_Opcode::EqualString, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Type, bk_Opcode::EqualType, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Function, bk_Opcode::EqualFunc, bk_BoolType);
            } break;
            case bk_TokenKind::NotEqual: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::NotEqualInt, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Float, bk_Opcode::NotEqualFloat, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Boolean, bk_Opcode::NotEqualBool, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::String, bk_Opcode::NotEqualString, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Type, bk_Opcode::NotEqualType, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Function, bk_Opcode::NotEqualFunc, bk_BoolType);
            } break;
            case bk_TokenKind::Greater: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::GreaterThanInt, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Float, bk_Opcode::GreaterThanFloat, bk_BoolType);
            } break;
            case bk_TokenKind::GreaterOrEqual: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::GreaterOrEqualInt, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Float, bk_Opcode::GreaterOrEqualFloat, bk_BoolType);
            } break;
            case bk_TokenKind::Less: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::LessThanInt, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Float, bk_Opcode::LessThanFloat, bk_BoolType);
            } break;
            case bk_TokenKind::LessOrEqual: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::LessOrEqualInt, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Float, bk_Opcode::LessOrEqualFloat, bk_BoolType);
            } break;

            case bk_TokenKind::And: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::AndInt, stack[stack.len - 2].type) ||
                          EmitOperator2(bk_PrimitiveKind::Boolean, bk_Opcode::AndBool, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::Or: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::OrInt, stack[stack.len - 2].type) ||
                          EmitOperator2(bk_PrimitiveKind::Boolean, bk_Opcode::OrBool, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::XorOrComplement: {
                if (op.unary) {
                    success = EmitOperator1(bk_PrimitiveKind::Integer, bk_Opcode::ComplementInt, stack[stack.len - 1].type) ||
                              EmitOperator1(bk_PrimitiveKind::Boolean, bk_Opcode::NotBool, stack[stack.len - 1].type);
                } else {
                    success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::XorInt, stack[stack.len - 1].type) ||
                              EmitOperator2(bk_PrimitiveKind::Boolean, bk_Opcode::NotEqualBool, stack[stack.len - 1].type);
                }
            } break;
            case bk_TokenKind::LeftShift: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::LeftShiftInt, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::RightShift: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::RightShiftInt, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::LeftRotate: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::LeftRotateInt, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::RightRotate: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::RightRotateInt, stack[stack.len - 2].type);
            } break;

            case bk_TokenKind::Not: {
                success = EmitOperator1(bk_PrimitiveKind::Boolean, bk_Opcode::NotBool, stack[stack.len - 1].type);
            } break;
            case bk_TokenKind::AndAnd: {
                success = EmitOperator2(bk_PrimitiveKind::Boolean, bk_Opcode::AndBool, stack[stack.len - 2].type);

                RG_ASSERT(op.branch_addr && ir[op.branch_addr].code == bk_Opcode::SkipIfFalse);
                ir[op.branch_addr].u.i = ir.len - op.branch_addr;
            } break;
            case bk_TokenKind::OrOr: {
                success = EmitOperator2(bk_PrimitiveKind::Boolean, bk_Opcode::OrBool, stack[stack.len - 2].type);

                RG_ASSERT(op.branch_addr && ir[op.branch_addr].code == bk_Opcode::SkipIfTrue);
                ir[op.branch_addr].u.i = ir.len - op.branch_addr;
            } break;

            default: { RG_UNREACHABLE(); } break;
        }
    }

    if (RG_UNLIKELY(!success)) {
        if (op.unary) {
            MarkError(op.pos, "Cannot use '%1' operator on '%2' value",
                      bk_TokenKindNames[(int)op.kind], stack[stack.len - 1].type->signature);
        } else if (stack[stack.len - 2].type == stack[stack.len - 1].type) {
            MarkError(op.pos, "Cannot use '%1' operator on '%2' values",
                      bk_TokenKindNames[(int)op.kind], stack[stack.len - 2].type->signature);
        } else {
            MarkError(op.pos, "Cannot use '%1' operator on '%2' and '%3' values",
                      bk_TokenKindNames[(int)op.kind], stack[stack.len - 2].type->signature,
                      stack[stack.len - 1].type->signature);
        }
    }
}

bool bk_Parser::EmitOperator1(bk_PrimitiveKind in_primitive, bk_Opcode code, const bk_TypeInfo *out_type)
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

bool bk_Parser::EmitOperator2(bk_PrimitiveKind in_primitive, bk_Opcode code, const bk_TypeInfo *out_type)
{
    const bk_TypeInfo *type1 = stack[stack.len - 2].type;
    const bk_TypeInfo *type2 = stack[stack.len - 1].type;

    if (type1->primitive == in_primitive && type1 == type2) {
        ir.Append({code});
        stack[--stack.len - 1] = {out_type};

        return true;
    } else {
        return false;
    }
}

const bk_FunctionTypeInfo *bk_Parser::ParseFunctionType()
{
    bk_FunctionTypeInfo type_buf = {};
    HeapArray<char> signature_buf;

    type_buf.primitive = bk_PrimitiveKind::Function;
    signature_buf.Append("func (");

    // Parameters
    ConsumeToken(bk_TokenKind::LeftParenthesis);
    if (!MatchToken(bk_TokenKind::RightParenthesis)) {
        for (;;) {
            SkipNewLines();

            const bk_TypeInfo *type = ParseTypeExpression();

            if (RG_LIKELY(type_buf.params.Available())) {
                type_buf.params.Append(type);
            } else {
                MarkError(pos - 1, "Functions cannot have more than %1 parameters", RG_LEN(type_buf.params.data));
            }
            signature_buf.Append(type->signature);

            if (MatchToken(bk_TokenKind::Comma)) {
                signature_buf.Append(", ");
            } else {
                break;
            }
        }

        SkipNewLines();
        ConsumeToken(bk_TokenKind::RightParenthesis);
    }
    signature_buf.Append(')');

    // Return type
    if (MatchToken(bk_TokenKind::Colon)) {
        type_buf.ret_type = ParseTypeExpression();

        if (type_buf.ret_type != bk_NullType) {
            Fmt(&signature_buf, ": %1", type_buf.ret_type->signature);
        } else {
            signature_buf.Append(0);
        }
    } else {
        type_buf.ret_type = bk_NullType;
        signature_buf.Append(0);
    }

    // Type is complete (in theory)
    type_buf.signature = InternString(signature_buf.ptr);

    const bk_FunctionTypeInfo *func_type = InsertType(type_buf, &program->function_types)->AsFunctionType();
    return func_type;
}

const bk_ArrayTypeInfo *bk_Parser::ParseArrayType()
{
    Size def_pos = pos;

    bk_ArrayTypeInfo type_buf = {};

    type_buf.primitive = bk_PrimitiveKind::Array;

    // Parse array length
    {
        const bk_TypeInfo *type = ParseExpression(false).type;

        ConsumeToken(bk_TokenKind::RightBracket);

        if (RG_LIKELY(type == bk_IntType)) {
            // Once we start to implement constant folding and CTFE, more complex expressions
            // should work without any change here.
            if (RG_LIKELY(ir[ir.len - 1].code == bk_Opcode::Push)) {
                type_buf.len = ir[ir.len - 1].u.i;
                TrimInstructions(1);
            } else {
                MarkError(def_pos, "Complex 'Int' expression cannot be resolved statically");
                type_buf.len = 0;
            }
        } else {
            MarkError(def_pos, "Expected an 'Int' expression, not '%1'", type->signature);
            type_buf.len = 0;
        }
    }

    // Unit type and size
    {
        type_buf.unit_type = ParseTypeExpression();

        if (type_buf.unit_type->primitive == bk_PrimitiveKind::Array) {
            const bk_ArrayTypeInfo *array_type = type_buf.unit_type->AsArrayType();
            type_buf.unit_size = array_type->len * array_type->unit_size;
        } else {
            type_buf.unit_size = 1;
        }
    }

    // Safety checks
    if (RG_UNLIKELY(type_buf.len < 0)) {
        MarkError(def_pos, "Negative array size is not valid");
    }
    if (RG_UNLIKELY(type_buf.len > UINT16_MAX || type_buf.unit_size > UINT16_MAX ||
                    type_buf.len * type_buf.unit_size > UINT16_MAX)) {
        MarkError(def_pos, "Fixed array size is too big");
    }

    // Format type signature
    {
        HeapArray<char> signature_buf;
        Fmt(&signature_buf, "[%1] %2", type_buf.len, type_buf.unit_type->signature);

        type_buf.signature = InternString(signature_buf.ptr);
    }

    const bk_ArrayTypeInfo *array_type = InsertType(type_buf, &program->array_types)->AsArrayType();
    return array_type;
}

void bk_Parser::ParseArraySubscript()
{
    do {
        const bk_ArrayTypeInfo *array_type = stack[stack.len - 1].type->AsArrayType();

        if (stack[stack.len - 1].indirect_addr) {
            RG_ASSERT(stack[stack.len - 1].indirect_addr == ir.len - 1);
            TrimInstructions(1);
        }

        Size idx_pos = pos;
        const bk_TypeInfo *type = ParseExpression(false).type;
        if (RG_UNLIKELY(type != bk_IntType)) {
            MarkError(idx_pos, "Expected an 'Int' expression, not '%1'", type->signature);
        }

        // Compute array index
        ir.Append({bk_Opcode::CheckIndex, {}, {.i = array_type->len}});
        if (array_type->unit_size != 1) {
            ir.Append({bk_Opcode::Push, bk_PrimitiveKind::Integer, {.i = array_type->unit_size}});
            ir.Append({bk_Opcode::MultiplyInt});
        }
        if (stack[stack.len - 1].indirect_addr) {
            ir.Append({bk_Opcode::AddInt});
        }

        // Resolve subscript
        if (array_type->unit_type->PassByReference()) {
            ir.Append({bk_Opcode::AddInt});
        } else {
            ir.Append({bk_Opcode::LoadArray});
        }

        stack[stack.len - 1].type = array_type->unit_type;
        stack[stack.len - 1].indirect_addr = ir.len - 1;
    } while (stack[stack.len - 1].type->primitive == bk_PrimitiveKind::Array &&
             MatchToken(bk_TokenKind::Comma));

    ConsumeToken(bk_TokenKind::RightBracket);
}

// Don't try to call from outside ParseExpression()!
bool bk_Parser::ParseCall(const bk_FunctionTypeInfo *func_type, const bk_FunctionInfo *func, bool overload)
{
    LocalArray<const bk_TypeInfo *, RG_LEN(bk_FunctionTypeInfo::params.data)> args;

    Size call_pos = pos - 1;
    Size call_addr = ir.len;

    // Parse arguments
    Size args_size = 0;
    if (!MatchToken(bk_TokenKind::RightParenthesis)) {
        do {
            SkipNewLines();

            if (RG_UNLIKELY(!args.Available())) {
                MarkError(pos, "Functions cannot take more than %1 arguments", RG_LEN(args.data));
                return false;
            }

            if (func_type->variadic && args.len >= func_type->params.len) {
                Size type_addr = ir.len;
                ir.Append({bk_Opcode::Push, bk_PrimitiveKind::Type});

                const bk_TypeInfo *type = ParseExpression(true).type;
                args.Append(type);
                args_size += 2;

                ir[type_addr].u.type = type;
            } else {
                const bk_TypeInfo *type = ParseExpression(true).type;
                args.Append(type);
                args_size++;
            }
        } while (MatchToken(bk_TokenKind::Comma));

        SkipNewLines();
        ConsumeToken(bk_TokenKind::RightParenthesis);
    }
    if (func_type->variadic) {
        ir.Append({bk_Opcode::Push, bk_PrimitiveKind::Integer, {.i = args_size - func_type->params.len}});
        args_size++;
    }

    // Find appropriate overload. Variadic functions cannot be overloaded but it
    // does not hurt to use the same logic to check argument types.
    if (func && overload) {
        const bk_FunctionInfo *func0 = func;

        while (!TestOverload(*func->type, args)) {
            func = func->overload_next;

            if (RG_UNLIKELY(func == func0)) {
                LocalArray<char, 1024> buf;
                for (Size i = 0; i < args.len; i++) {
                    buf.len += Fmt(buf.TakeAvailable(), "%1%2", i ? ", " : "", args[i]->signature).len;
                }

                MarkError(call_pos, "Cannot call '%1' with (%2) arguments", func->name, buf);

                // Show all candidate functions with same name
                const bk_FunctionInfo *it = func0;
                do {
                    HintError(definitions_map.FindValue(it, -1), "Candidate '%1'", it->prototype);
                    it = it->overload_next;
                } while (it != func0);

                return false;
            }
        }
    } else if (!TestOverload(*func_type, args)) {
        LocalArray<char, 1024> buf;
        for (Size i = 0; i < args.len; i++) {
            buf.len += Fmt(buf.TakeAvailable(), "%1%2", i ? ", " : "", args[i]->signature).len;
        }

        MarkError(call_pos, "Cannot call function typed '%1' with (%2) arguments", func_type->signature, buf);
        return false;
    }

    // Emit intrinsic or call
    if (!func) {
        Size offset = 1 + args_size;
        ir.Append({bk_Opcode::Call, {}, {.i = -offset}});

        stack.len--;
    } else if (func->mode == bk_FunctionInfo::Mode::Intrinsic) {
        EmitIntrinsic(func->name, call_pos, call_addr, args);
    } else {
        ir.Append({bk_Opcode::CallDirect, {}, {.func = func}});
    }
    stack.Append({func_type->ret_type});

    return true;
}

void bk_Parser::EmitIntrinsic(const char *name, Size call_pos, Size call_addr, Span<const bk_TypeInfo *const> args)
{
    if (TestStr(name, "toFloat")) {
        if (args[0] == bk_IntType) {
            ir.Append({bk_Opcode::IntToFloat});
        }
    } else if (TestStr(name, "toInt")) {
        if (args[0] == bk_FloatType) {
            ir.Append({bk_Opcode::FloatToInt});
        }
    } else if (TestStr(name, "typeOf")) {
        // XXX: We can change the signature from typeOf(...) to typeOf(Any) after Any
        // is implemented, and remove this check.
        if (RG_UNLIKELY(args.len != 1)) {
            MarkError(call_pos, "Intrinsic function typeOf() takes one argument");
            return;
        }

        // typeOf() does not execute anything!
        TrimInstructions(ir.len - call_addr);

        ir.Append({bk_Opcode::Push, bk_PrimitiveKind::Type, {.type = args[0]}});
    } else {
        RG_UNREACHABLE();
    }
}

void bk_Parser::EmitLoad(const bk_VariableInfo &var)
{
    if (RG_UNLIKELY(var.scope == bk_VariableInfo::Scope::Global &&
                    current_func && current_func->earliest_ref_addr < var.ready_addr)) {
        MarkError(definitions_map.FindValue(current_func, -1), "Function '%1' is referenced before global variable '%2' exists",
                  current_func->name, var.name);
        HintError(current_func->earliest_ref_pos, "Function reference is here (it could be indirect)");
        HintDefinition(&var, "Variable '%1' is defined here", var.name);
    }

    if (!var.mut && var.ready_addr > 0 && ir[var.ready_addr - 1].code == bk_Opcode::Push) {
        bk_Instruction inst = ir[var.ready_addr - 1];
        ir.Append(inst);
    } else {
        bk_Opcode code = (var.scope != bk_VariableInfo::Scope::Local) ? bk_Opcode::Load : bk_Opcode::LoadLocal;
        ir.Append({code, var.type->primitive, {.i = var.offset}});
    }

    stack.Append({var.type, &var});
}

const bk_TypeInfo *bk_Parser::ParseTypeExpression()
{
    Size type_pos = pos;

    // Parse type expression
    {
        const bk_TypeInfo *type = ParseExpression(false).type;

        if (RG_UNLIKELY(type != bk_TypeType)) {
            MarkError(type_pos, "Expected a 'Type' expression, not '%1'", type->signature);
            return bk_NullType;
        }
    }

    if (RG_UNLIKELY(ir[ir.len - 1].code != bk_Opcode::Push)) {
        MarkError(type_pos, "Complex 'Type' expression cannot be resolved statically");
        return bk_NullType;
    }

    const bk_TypeInfo *type = ir[ir.len - 1].u.type;
    TrimInstructions(1);

    return type;
}

void bk_Parser::DiscardResult()
{
    switch (ir[ir.len - 1].code) {
        case bk_Opcode::Push:
        case bk_Opcode::Load:
        case bk_Opcode::LoadLocal: { TrimInstructions(1); } break;

        case bk_Opcode::Copy: { ir[ir.len - 1].code = bk_Opcode::Store; } break;
        case bk_Opcode::CopyLocal: { ir[ir.len - 1].code = bk_Opcode::StoreLocal; } break;
        case bk_Opcode::CopyArray: { ir[ir.len - 1].code = bk_Opcode::StoreArray; } break;

        default: { EmitPop(1); } break;
    }
}

void bk_Parser::EmitPop(int64_t count)
{
    RG_ASSERT(count >= 0);

    if (count) {
        ir.Append({bk_Opcode::Pop, {}, {.i = count}});
    }
}

void bk_Parser::EmitReturn()
{
    RG_ASSERT(current_func);

    // We support tail recursion elimination (TRE)
    if (ir[ir.len - 1].code == bk_Opcode::CallDirect && ir[ir.len - 1].u.func == current_func) {
        ir.len--;

        for (Size i = current_func->params.len - 1; i >= 0; i--) {
            const bk_FunctionInfo::Parameter &param = current_func->params[i];
            ir.Append({bk_Opcode::StoreLocal, param.type->primitive, {.i = i}});
        }

        EmitPop(var_offset - current_func->params.len);
        ir.Append({bk_Opcode::Jump, {}, {.i = current_func->addr - ir.len}});

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

bool bk_Parser::TestOverload(const bk_FunctionTypeInfo &func_type, Span<const bk_TypeInfo *const> params)
{
    if (func_type.variadic) {
        if (func_type.params.len > params.len)
            return false;
    } else {
        if (func_type.params.len != params.len)
            return false;
    }

    for (Size i = 0; i < func_type.params.len; i++) {
        if (func_type.params[i] != params[i])
            return false;
    }

    return true;
}

bool bk_Parser::ConsumeToken(bk_TokenKind kind)
{
    if (RG_UNLIKELY(pos >= tokens.len)) {
        if (valid) {
            if (out_report) {
                out_report->unexpected_eof = true;
            }
            MarkError(pos, "Unexpected end of file, expected '%1'", bk_TokenKindNames[(int)kind]);
        }

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
        if (valid) {
            if (out_report) {
                out_report->unexpected_eof = true;
            }
            MarkError(pos, "Unexpected end of file, expected end of statement");
        }

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
