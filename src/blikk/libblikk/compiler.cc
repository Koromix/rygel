// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "compiler.hh"
#include "error.hh"
#include "lexer.hh"
#include "program.hh"
#include "vm.hh"

namespace RG {

struct ForwardInfo {
    const char *name;

    bk_TokenKind kind;
    Size pos;
    Size skip;
    bk_VariableInfo *var;

    ForwardInfo *next;

    RG_HASHTABLE_HANDLER(ForwardInfo, name);
};

struct LoopContext {
    Size offset;
    Size break_addr;
    Size continue_addr;
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

    bk_VariableInfo *var = nullptr;
    bool lea = false;
    Size indirect_addr = 0;
    Size indirect_imbalance = 0;
};

enum class ExpressionFlag {
    StopOperator = 1 << 0
};

static Size LevenshteinDistance(Span<const char> str1, Span<const char> str2);

static ForwardInfo fake_fwd = {};

#define IR (*ir)

class bk_Parser {
    RG_DELETE_COPY(bk_Parser)

    bk_Program *program;

    // All these members are relevant to the current parse only, and get resetted each time
    const bk_TokenizedFile *file;
    bk_CompileReport *out_report; // Can be NULL
    Span<const bk_Token> tokens;
    Size pos;
    Size prev_main_len;
    bool valid;
    bool show_errors;
    bool show_hints;

    // Transient mappings
    BucketArray<ForwardInfo> forwards;
    HashTable<const char *, ForwardInfo *> forwards_map;
    HashMap<Size, ForwardInfo *> skip_map;
    HashMap<const void *, Size> definitions_map;
    HashSet<const void *> poisoned_set;

    // Global or function context
    HeapArray<bk_Instruction> *ir;
    bk_SourceMap *src;
    Size *offset_ptr;
    int depth = 0;
    int recursion = 0;
    bk_FunctionInfo *current_func = nullptr;
    LoopContext *loop = nullptr;

    Size main_offset = 0;
    HashSet<const char *> strings;

    // Only used (and valid) while parsing expression
    HeapArray<StackSlot> stack;
    bk_VirtualMachine folder;

public:
    bk_Parser(bk_Program *program);

    bool Parse(const bk_TokenizedFile &file, bk_CompileReport *out_report);

    void AddFunction(const char *prototype, unsigned int flags, std::function<bk_NativeFunction> native);
    bk_VariableInfo *AddGlobal(const char *name, const bk_TypeInfo *type,
                               Span<const bk_PrimitiveValue> values, bool module);
    void AddOpaque(const char *name);

private:
    void Preparse(Span<const Size> positions);

    // These 3 functions return true if (and only if) all code paths have a return statement.
    // For simplicity, return statements inside loops are not considered.
    bool ParseBlock(bool end_with_else);
    bool ParseStatement();
    bool ParseDo();

    void ParseFunction(ForwardInfo *fwd, bool record);
    void ParseEnum(ForwardInfo *fwd);
    void ParseReturn();
    void ParseLet();
    bool ParseIf();
    void ParseWhile();
    void ParseFor();

    void ParseBreak();
    void ParseContinue();

    StackSlot ParseExpression(unsigned int flags = 0, const bk_TypeInfo *hint = nullptr);
    bool ParseExpression(const bk_TypeInfo *type);
    void ProduceOperator(const PendingOperator &op);
    bool EmitOperator1(bk_PrimitiveKind in_primitive, bk_Opcode code, const bk_TypeInfo *out_type);
    bool EmitOperator2(bk_PrimitiveKind in_primitive, bk_Opcode code, const bk_TypeInfo *out_type);
    bk_VariableInfo *FindVariable(const char *name);
    const bk_FunctionTypeInfo *ParseFunctionType();
    const bk_ArrayTypeInfo *ParseArrayType();
    void ParseArraySubscript();
    void ParseRecordDot();
    void ParseEnumDot();
    bool ParseCall(const bk_FunctionTypeInfo *func_type, const bk_FunctionInfo *func, bool overload);
    void EmitIntrinsic(const char *name, Size call_pos, Size call_addr, Span<const bk_TypeInfo *const> args);
    void EmitLoad(bk_VariableInfo *var);

    const bk_TypeInfo *ParseType();

    void FoldInstruction(Size count, const bk_TypeInfo *out_type);
    void DiscardResult(Size discard);
    bool CopyBigConstant(Size size);

    void EmitPop(int64_t count);
    void EmitReturn(Size size);

    inline void Emit(bk_Opcode code) { IR.Append({ code, {}, {} }); }
    inline void Emit(bk_Opcode code, bk_PrimitiveValue u2) { IR.Append({ code, {}, u2 }); }
    inline void Emit(bk_Opcode code, bool b) { IR.Append({ code, {}, { .b = b } }); }
    inline void Emit(bk_Opcode code, int i) { IR.Append({ code, {}, { .i = i } }); }
    inline void Emit(bk_Opcode code, int64_t i) { IR.Append({ code, {}, { .i = i } }); }
    inline void Emit(bk_Opcode code, double d) { IR.Append({ code, {}, { .d = d } }); }
    inline void Emit(bk_Opcode code, const char *str) { IR.Append({ code, {}, { .str = str } }); }
    inline void Emit(bk_Opcode code, const bk_TypeInfo *type) { IR.Append({ code, {}, { .type = type } }); }
    inline void Emit(bk_Opcode code, const bk_FunctionInfo *func) { IR.Append({ code, {}, { .func = func } }); }
    inline void Emit(bk_Opcode code, void *opaque) { IR.Append({ code, {}, { .opaque = opaque } }); }
    inline void Emit(bk_Opcode code, decltype(bk_Instruction::u1) u1, bk_PrimitiveValue u2) { IR.Append({ code, u1, u2 }); }
    inline void Emit(bk_Opcode code, decltype(bk_Instruction::u1) u1, bool b) { IR.Append({ code, u1, { .b = b } }); }
    inline void Emit(bk_Opcode code, decltype(bk_Instruction::u1) u1, int i) { IR.Append({ code, u1, { .i = i } }); }
    inline void Emit(bk_Opcode code, decltype(bk_Instruction::u1) u1, int64_t i) { IR.Append({ code, u1, { .i = i } }); }
    inline void Emit(bk_Opcode code, decltype(bk_Instruction::u1) u1, double d) { IR.Append({ code, u1, { .d = d } }); }
    inline void Emit(bk_Opcode code, decltype(bk_Instruction::u1) u1, const char *str) { IR.Append({ code, u1, { .str = str } }); }
    inline void Emit(bk_Opcode code, decltype(bk_Instruction::u1) u1, const bk_TypeInfo *type) { IR.Append({ code, u1, { .type = type } }); }
    inline void Emit(bk_Opcode code, decltype(bk_Instruction::u1) u1, const bk_FunctionInfo *func) { IR.Append({ code, u1, { .func = func } }); }
    inline void Emit(bk_Opcode code, decltype(bk_Instruction::u1) u1, void *opaque) { IR.Append({ code, u1, { .opaque = opaque } }); }

    bk_VariableInfo *CreateGlobal(const char *name, const bk_TypeInfo *type,
                                  Span<const bk_PrimitiveValue> values, bool module);
    bool MapVariable(bk_VariableInfo *var, Size defn_pos);
    const char *GetVariableKind(const bk_VariableInfo *var, bool capitalize);
    void DestroyVariables(Size first_idx);
    template<typename T>
    void DestroyTypes(BucketArray<T> *types, Size first_idx);

    void FixJumps(Size jump_addr, Size target_addr);
    void TrimInstructions(Size trim_addr);

    bool TestOverload(const bk_FunctionTypeInfo &func_type, Span<const bk_TypeInfo *const> params);

    bool ConsumeToken(bk_TokenKind kind);
    const char *ConsumeIdentifier();
    bool MatchToken(bk_TokenKind kind);
    bool PeekToken(bk_TokenKind kind);

    bool EndStatement();
    bool SkipNewLines();

    const char *InternString(const char *str);
    template<typename T>
    bk_TypeInfo *InsertType(const T &type_buf, BucketArray<T> *out_types);

    bool RecurseInc();
    void RecurseDec();

    void FlagError()
    {
        valid = false;
        show_hints = show_errors;
        show_errors = false;

        if (current_func) {
            current_func->valid = false;
        }

        if (out_report && valid) {
            out_report->depth = depth;
        }
    }

    template <typename... Args>
    void MarkError(Size pos, const char *fmt, Args... args)
    {
        RG_ASSERT(pos >= 0);

        if (show_errors) {
            Size offset = (pos < tokens.len) ? tokens[pos].offset : file->code.len;
            int line = tokens[std::min(pos, tokens.len - 1)].line;

            if (offset <= file->code.len) {
                bk_ReportDiagnostic(bk_DiagnosticType::Error, file->code, file->filename, line, offset, fmt, args...);
            } else {
                bk_ReportDiagnostic(bk_DiagnosticType::Error, fmt, args...);
            }
        }

        FlagError();
    }

    template <typename... Args>
    void Hint(Size pos, const char *fmt, Args... args)
    {
        if (show_hints) {
            if (pos >= 0) {
                Size offset = (pos < tokens.len) ? tokens[pos].offset : file->code.len;
                int line = tokens[std::min(pos, tokens.len - 1)].line;

                if (offset <= file->code.len) {
                    bk_ReportDiagnostic(bk_DiagnosticType::Hint, file->code, file->filename, line, offset, fmt, args...);
                } else {
                    bk_ReportDiagnostic(bk_DiagnosticType::Hint, fmt, args...);
                }
            } else {
                bk_ReportDiagnostic(bk_DiagnosticType::Hint, fmt, args...);
            }
        }
    }

    template <typename... Args>
    void HintDefinition(Size defn_pos, const char *fmt, Args... args)
    {
        if (defn_pos >= 0) {
            Hint(defn_pos, fmt, args...);
        }
    }

    template <typename... Args>
    void HintDefinition(const void *defn, const char *fmt, Args... args)
    {
        Size defn_pos = definitions_map.FindValue(defn, -1);
        HintDefinition(defn_pos, fmt, args...);
    }

    template <typename T>
    void HintSuggestions(const char *name, const T &symbols)
    {
        Size threshold = strlen(name) / 2;
        bool warn_case = false;

        for (const auto &it: symbols) {
            Size dist = LevenshteinDistance(name, it.name);

            if (dist <= threshold) {
                Hint(definitions_map.FindValue(&it, -1), "Suggestion: %1", it.name);
                warn_case |= !dist;
            }
        }

        if (warn_case) {
            Hint(-1, "Identifiers are case-sensitive (e.g. foo and FOO are different)");
        }
    }
};

// Copied almost as-is (but made case insensitive within ASCII range) from Wikipedia:
// https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance
static Size LevenshteinDistance(Span<const char> str1, Span<const char> str2)
{
    if (str1.len > str2.len)
        return LevenshteinDistance(str2, str1);

    HeapArray<Size> distances;
    distances.AppendDefault(str1.len + 1);

    for (Size i = 0; i <= str1.len; ++i) {
        distances[i] = i;
    }

    for (Size j = 1; j <= str2.len; ++j) {
        Size prev_diagonal = distances[0]++;
        Size prev_diagonal_save;

        for (Size i = 1; i <= str1.len; ++i) {
            prev_diagonal_save = distances[i];

            char c1 = LowerAscii(str1[i - 1]);
            char c2 = LowerAscii(str2[i - 1]);

            if (c1 == c2) {
                distances[i] = prev_diagonal;
            } else {
                distances[i] = std::min(std::min(distances[i - 1], distances[i]), prev_diagonal) + 1;
            }

            prev_diagonal = prev_diagonal_save;
        }
    }

    return distances[str1.len];
}

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

void bk_Compiler::AddFunction(const char *prototype, unsigned int flags, std::function<bk_NativeFunction> native)
{
    RG_ASSERT(native);
    parser->AddFunction(prototype, flags, native);
}

void bk_Compiler::AddGlobal(const char *name, const bk_TypeInfo *type, Span<const bk_PrimitiveValue> values)
{
    parser->AddGlobal(name, type, values, false);
}

void bk_Compiler::AddOpaque(const char *name)
{
    parser->AddOpaque(name);
}

bk_Parser::bk_Parser(bk_Program *program)
    : program(program), folder(program, (int)bk_RunFlag::HideErrors)
{
    ir = &program->main;
    offset_ptr = &main_offset;

    RG_ASSERT(program);
    RG_ASSERT(!IR.len);

    // Base types
    for (const bk_TypeInfo &type: bk_BaseTypes) {
        AddGlobal(type.signature, bk_TypeType, { { .type = &type } }, true);
        program->types_map.Set(&type);
    }

    // Not needed because true and flase are keywords, but adding them as symbols
    // makes them visible when try to help the user with a typo.
    AddGlobal("true", bk_BoolType, {{ .b = true }}, false);
    AddGlobal("false", bk_BoolType, {{ .b = false }}, false);

    // Special values
    AddGlobal("Version", bk_StringType, { { .str = FelixVersion } }, false);
    AddGlobal("NaN", bk_FloatType, { { .d = (double)NAN } }, false);
    AddGlobal("Inf", bk_FloatType, { { .d = (double)INFINITY } }, false);

    // Intrinsics
    AddFunction("toFloat(Int): Float", (int)bk_FunctionFlag::Pure, {});
    AddFunction("toFloat(Float): Float", (int)bk_FunctionFlag::Pure, {});
    AddFunction("toInt(Int): Int", (int)bk_FunctionFlag::Pure, {});
    AddFunction("toInt(Float): Int", (int)bk_FunctionFlag::Pure, {});
    AddFunction("typeOf(...): Type", (int)bk_FunctionFlag::Pure, {});
    AddFunction("iif(Bool, ...)", (int)bk_FunctionFlag::Pure, {});
}

bool bk_Parser::Parse(const bk_TokenizedFile &file, bk_CompileReport *out_report)
{
    prev_main_len = program->main.len;

    // Restore previous state if something goes wrong
    RG_DEFER_NC(err_guard, globals_len = program->globals.len,
                           sources_len = program->sources.len,
                           prev_main_offset = main_offset,
                           variables_count = program->variables.count,
                           functions_count = program->functions.count,
                           ro_len = program->ro.len,
                           function_types_count = program->function_types.count,
                           array_types_count = program->array_types.count,
                           record_types_count = program->record_types.count,
                           enum_types_count = program->enum_types.count,
                           bare_types_count = program->bare_types.count) {
        program->main.RemoveFrom(prev_main_len);
        program->globals.RemoveFrom(globals_len);
        program->sources.RemoveFrom(sources_len);

        main_offset = prev_main_offset;
        DestroyVariables(variables_count);

        for (Size i = functions_count; i < program->functions.count; i++) {
            bk_FunctionInfo *func = &program->functions[i];
            bk_FunctionInfo **it = program->functions_map.Find(func->name);

            if (it) {
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
        }
        program->functions.RemoveFrom(functions_count);

        program->ro.RemoveFrom(ro_len);

        DestroyTypes(&program->function_types, function_types_count);
        DestroyTypes(&program->array_types, array_types_count);
        DestroyTypes(&program->record_types, record_types_count);
        DestroyTypes(&program->enum_types, enum_types_count);
        DestroyTypes(&program->bare_types, bare_types_count);
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

    forwards.Clear();
    forwards_map.Clear();
    skip_map.Clear();
    definitions_map.Clear();
    poisoned_set.Clear();

    src = program->sources.AppendDefault();
    src->filename = DuplicateString(file.filename, &program->str_alloc).ptr;
    RG_ASSERT(ir == &program->main);

    // Protect IR from before this parse step
    Emit(bk_Opcode::Nop);

    // Preparse (top-level order-independence)
    Preparse(file.prototypes);

    // Do the actual parsing!
    src->lines.Append({ IR.len, 0 });
    while (pos < tokens.len) [[likely]] {
        ParseStatement();
    }

    // Maybe it'll help catch bugs
    RG_ASSERT(!depth);
    RG_ASSERT(!loop);
    RG_ASSERT(!current_func);

    if (valid) {
        Emit(bk_Opcode::End, main_offset);
        IR.Trim();

        err_guard.Disable();
    }

    return valid;
}

// This is not exposed to user scripts, and the validation of prototype is very light,
// with a few debug-only asserts. Bad function names (even invalid UTF-8 sequences)
// will go right through. Don't pass in garbage!
void bk_Parser::AddFunction(const char *prototype, unsigned int flags, std::function<bk_NativeFunction> native)
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
        MemCpy(buf.ptr + offset, "func ", 5);
        signature = InternString(buf.ptr + offset);

        ptr += offset;
    }

    func->prototype = InternString(prototype);
    func->mode = native ? bk_FunctionInfo::Mode::Native : bk_FunctionInfo::Mode::Intrinsic;
    func->native = native;
    func->valid = true;
    func->impure = !(flags & (int)bk_FunctionFlag::Pure);
    func->side_effects = !(flags & ((int)bk_FunctionFlag::Pure | (int)bk_FunctionFlag::NoSideEffect));

    // Reuse or create function type
    {
        const bk_TypeInfo *type = program->types_map.FindValue(signature, nullptr);

        if (type) {
            func->type = type->AsFunctionType();

            for (const bk_TypeInfo *type2: func->type->params) {
                func->params.Append({"", type2, false});
            }
        } else {
            bk_FunctionTypeInfo *func_type = program->function_types.AppendDefault();

            func_type->primitive = bk_PrimitiveKind::Function;
            func_type->signature = signature;
            func_type->size = 1;

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
                        const bk_TypeInfo *type2 = program->types_map.FindValue(ptr, nullptr);
                        RG_ASSERT(type2);

                        func->params.Append({"", type2, false});
                        func_type->params.Append(type2);
                        func_type->params_size += type2->size;
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
        bk_FunctionInfo *head = *program->functions_map.TrySet(func);

        if (head != func) {
            RG_ASSERT(!head->type->variadic && !func->type->variadic);

            head->overload_prev->overload_next = func;
            func->overload_next = head;
            func->overload_prev = head->overload_prev;
            head->overload_prev = func;

#if defined(RG_DEBUG)
            do {
                RG_ASSERT(!TestOverload(*head->type, func->type->params));
                head = head->overload_next;
            } while (head != func);
#endif
        } else {
            func->overload_prev = func;
            func->overload_next = func;

            AddGlobal(func->name, func->type, { { .func = func } }, true);
        }
    }
}

bk_VariableInfo *bk_Parser::AddGlobal(const char *name, const bk_TypeInfo *type,
                                      Span<const bk_PrimitiveValue> values, bool module)
{
    bk_VariableInfo *var = CreateGlobal(name, type, values, module);
    MapVariable(var, -1);

    return var;
}

void bk_Parser::AddOpaque(const char *name)
{
    bk_TypeInfo type_buf = {};

    type_buf.signature = InternString(name);
    type_buf.primitive = bk_PrimitiveKind::Opaque;
    type_buf.init0 = true;
    type_buf.size = 1;

    const bk_TypeInfo *type = InsertType(type_buf, &program->bare_types);

    bk_VariableInfo *var = CreateGlobal(type_buf.signature, bk_TypeType, { { .type = type } }, true);
    MapVariable(var, -1);
}

void bk_Parser::Preparse(Span<const Size> positions)
{
    RG_ASSERT(!forwards.count);

    for (Size i = positions.len - 1; i >= 0; i--) {
        Size fwd_pos = positions[i];
        Size id_pos = fwd_pos + 1;

        if (id_pos < tokens.len && tokens[id_pos].kind == bk_TokenKind::Identifier) {
            ForwardInfo *fwd = forwards.AppendDefault();

            fwd->name = InternString(tokens[id_pos].u.str);
            fwd->kind = tokens[fwd_pos].kind;
            fwd->pos = fwd_pos;
            fwd->skip = -1;

            bool inserted;
            ForwardInfo **ptr = forwards_map.TrySet(fwd, &inserted);

            if (inserted) {
                fwd->var = CreateGlobal(fwd->name, bk_NullType, {{}}, true);
            } else {
                ForwardInfo *prev = *ptr;

                *ptr = fwd;
                fwd->next = prev;

                fwd->var = prev->var;
            }

            skip_map.Set(fwd_pos, fwd);
        }
    }
}

template<typename T>
bk_TypeInfo *bk_Parser::InsertType(const T &type_buf, BucketArray<T> *out_types)
{
    bool inserted;
    const bk_TypeInfo **ptr = program->types_map.TrySetDefault(type_buf.signature, &inserted);

    bk_TypeInfo *type;
    if (inserted) {
        type = out_types->Append(type_buf);
        *ptr = type;
    } else {
        type = (bk_TypeInfo *)*ptr;
    }

    return type;
}

bool bk_Parser::ParseBlock(bool end_with_else)
{
    show_errors = true;
    depth++;

    bool recurse = RecurseInc();
    RG_DEFER_C(prev_offset = *offset_ptr,
               variables_count = program->variables.count) {
        RecurseDec();
        depth--;

        EmitPop(*offset_ptr - prev_offset);
        DestroyVariables(variables_count);
        *offset_ptr = prev_offset;
    };

    bool has_return = false;
    bool issued_unreachable = false;

    while (pos < tokens.len) [[likely]] {
        if (tokens[pos].kind == bk_TokenKind::End)
            break;
        if (end_with_else && tokens[pos].kind == bk_TokenKind::Else)
            break;

        if (has_return && !issued_unreachable) [[unlikely]] {
            MarkError(pos, "Unreachable statement");
            Hint(-1, "Code after return statement can never run");

            issued_unreachable = true;
        }

        if (recurse) [[likely]] {
            has_return |= ParseStatement();
        } else {
            if (!has_return) {
                MarkError(pos, "Excessive parsing depth (compiler limit)");
                Hint(-1, "Simplify surrounding code");
            }

            pos++;
            has_return = true;
        }
    }

    return has_return;
}

bool bk_Parser::ParseStatement()
{
    bool has_return = false;

    src->lines.Append({ IR.len, tokens[pos].line });
    show_errors = true;

    switch (tokens[pos].kind) {
        case bk_TokenKind::EndOfLine: {
            pos++;
            src->lines.len--;
        } break;
        case bk_TokenKind::Semicolon: { pos++; } break;

        case bk_TokenKind::Begin: {
            pos++;

            if (EndStatement()) [[likely]] {
                has_return = ParseBlock(false);
                ConsumeToken(bk_TokenKind::End);

                EndStatement();
            }
        } break;
        case bk_TokenKind::Func: {
            if (pos + 1 < tokens.len && tokens[pos + 1].kind == bk_TokenKind::Identifier) {
                ForwardInfo *fwd = skip_map.FindValue(pos, &fake_fwd);
                ParseFunction(fwd, false);
            } else {
                StackSlot slot = ParseExpression();
                DiscardResult(slot.type->size);
            }

            EndStatement();
        } break;
        case bk_TokenKind::Record: {
            ForwardInfo *fwd = skip_map.FindValue(pos, &fake_fwd);

            ParseFunction(fwd, true);
            EndStatement();
        } break;
        case bk_TokenKind::Enum: {
            ForwardInfo *fwd = skip_map.FindValue(pos, &fake_fwd);

            ParseEnum(fwd);
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
        case bk_TokenKind::Pass: {
            pos++;
            EndStatement();
        } break;

        default: {
            StackSlot slot = ParseExpression();
            DiscardResult(slot.type->size);

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
    } else if (PeekToken(bk_TokenKind::Pass)) {
        pos++;
        return false;
    } else {
        StackSlot slot = ParseExpression();
        DiscardResult(slot.type->size);

        return false;
    }
}

void bk_Parser::ParseFunction(ForwardInfo *fwd, bool record)
{
    Size func_pos = ++pos;

    if (current_func) [[unlikely]] {
        if (record) {
            MarkError(func_pos, "Record types cannot be defined inside functions");
            Hint(definitions_map.FindValue(current_func, -1), "Function was started here and is still open");
        } else {
            MarkError(func_pos, "Nested functions are not supported");
            Hint(definitions_map.FindValue(current_func, -1), "Previous function was started here and is still open");
        }
    } else if (depth) [[unlikely]] {
        MarkError(func_pos, "%1 must be defined in top-level scope", record ? "Records" : "Functions");
    }

    if (fwd != &fake_fwd && fwd->skip >= 0) {
        pos = fwd->skip;
        return;
    }

    bk_FunctionInfo *func = program->functions.AppendDefault();
    definitions_map.Set(func, pos);

    func->name = ConsumeIdentifier();
    func->mode = record ? bk_FunctionInfo::Mode::Record : bk_FunctionInfo::Mode::Blikk;

    HeapArray<char> signature_buf;
    HeapArray<char> prototype_buf;
    bk_FunctionTypeInfo type_buf = {};
    signature_buf.Append("func (");
    Fmt(&prototype_buf, "%1(", func->name);
    type_buf.primitive = bk_PrimitiveKind::Function;
    type_buf.size = 1;

    // Parameters
    ConsumeToken(bk_TokenKind::LeftParenthesis);
    if (!MatchToken(bk_TokenKind::RightParenthesis)) {
        for (;;) {
            SkipNewLines();

            bk_FunctionInfo::Parameter param = {};
            Size param_pos = pos;

            param.mut = !record && MatchToken(bk_TokenKind::Mut);
            param.name = ConsumeIdentifier();

            ConsumeToken(bk_TokenKind::Colon);
            param.type = ParseType();

            if (func->params.Available()) [[likely]] {
                bk_FunctionInfo::Parameter *ptr = func->params.Append(param);
                definitions_map.Set(ptr, param_pos);

                type_buf.params.Append(param.type);
                type_buf.params_size += param.type->size;
            } else {
                MarkError(pos - 1, "Functions cannot have more than %1 parameters", RG_LEN(type_buf.params.data));
            }

            signature_buf.Append(param.type->signature);
            Fmt(&prototype_buf, "%1: %2", param.name, param.type->signature);

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
    if (record) {
        bk_RecordTypeInfo *record_type = program->record_types.AppendDefault();

        record_type->signature = func->name;
        record_type->primitive = bk_PrimitiveKind::Record;
        record_type->init0 = true;
        record_type->func = func;

        for (const bk_FunctionInfo::Parameter &param: func->params) {
            bk_RecordTypeInfo::Member *member = record_type->members.AppendDefault();

            member->name = param.name;
            member->type = param.type;
            member->offset = record_type->size;

            record_type->init0 &= param.type->init0;
            record_type->size += param.type->size;

            // Evaluate each time, so that overflow is not a problem
            if (record_type->size > UINT16_MAX) [[unlikely]] {
                MarkError(func_pos, "Record size is too big");
            }

            Size param_pos = definitions_map.FindValue(&param, -1);
            definitions_map.Set(member, param_pos);
        }

        bool inserted;
        program->types_map.TrySet(record_type, &inserted);

        if (!inserted) [[unlikely]] {
            MarkError(func_pos, "Duplicate type name '%1'", record_type->signature);
        }

        type_buf.ret_type = record_type;

        Fmt(&signature_buf, ": %1", record_type->signature);
        Fmt(&prototype_buf, ": %1", record_type->signature);

        // Reuse or add function type
        type_buf.signature = InternString(signature_buf.ptr);
        func->type = InsertType(type_buf, &program->function_types)->AsFunctionType();
        func->prototype = InternString(prototype_buf.ptr);
    } else if (MatchToken(bk_TokenKind::Colon)) {
        type_buf.ret_type = ParseType();

        if (type_buf.ret_type != bk_NullType) {
            Fmt(&signature_buf, ": %1", type_buf.ret_type->signature);
            Fmt(&prototype_buf, ": %1", type_buf.ret_type->signature);
        } else {
            signature_buf.Grow(1); signature_buf.ptr[signature_buf.len] = 0;
            prototype_buf.Grow(1); prototype_buf.ptr[prototype_buf.len] = 0;
        }

        // Reuse or add function type
        type_buf.signature = InternString(signature_buf.ptr);
        func->type = InsertType(type_buf, &program->function_types)->AsFunctionType();
        func->prototype = InternString(prototype_buf.ptr);
    } else {
        // type_buf.ret_type = nullptr;

        signature_buf.Grow(1); signature_buf.ptr[signature_buf.len] = 0;
        prototype_buf.Grow(1); prototype_buf.ptr[prototype_buf.len] = 0;

        type_buf.signature = signature_buf.ptr;
        func->type = &type_buf;
        func->prototype = prototype_buf.ptr;
    }

    // Publish function
    {
        bool inserted;
        bk_FunctionInfo *overload = *program->functions_map.TrySet(func, &inserted);

        if (inserted || record) {
            func->overload_prev = func;
            func->overload_next = func;
        } else if (!record) {
            overload->overload_prev->overload_next = func;
            func->overload_next = overload;
            func->overload_prev = overload->overload_prev;
            overload->overload_prev = func;

            while (overload != func) {
                if (TestOverload(*overload->type, func->type->params)) {
                    if (overload->type->ret_type == func->type->ret_type || !func->type->ret_type) {
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
    }

    // Publish symbol
    bk_VariableInfo *var = fwd->var ? fwd->var : CreateGlobal(func->name, bk_NullType, {{}}, true);

    if (record) {
        var->type = bk_TypeType;
        var->ir->ptr[var->ir_addr - 1].u2.type = type_buf.ret_type;
        var->ir->ptr[var->ir_addr - 1].u1.primitive = bk_PrimitiveKind::Type;

        MapVariable(var, func_pos);
    } else if (func->type != &type_buf && func->overload_next == func) {
        var->type = func->type;
        var->ir->ptr[var->ir_addr - 1].u2.func = func;
        var->ir->ptr[var->ir_addr - 1].u1.primitive = bk_PrimitiveKind::Function;

        MapVariable(var, func_pos);
    }

    // Expressions involving this prototype (function or record) won't issue (visible) errors
    if (!show_errors) [[unlikely]] {
        poisoned_set.Set(var);
    }

    func->valid = true;
    func->impure = false;
    func->side_effects = false;

    if (!record) {
        Size func_offset = 0;

        RG_DEFER_C(prev_func = current_func,
                   prev_variables = program->variables.count,
                   prev_offset = offset_ptr,
                   prev_src = src,
                   prev_ir = ir) {
            // Variables inside the function are destroyed at the end of the block.
            // This destroys the parameters.
            DestroyVariables(prev_variables);
            offset_ptr = prev_offset;
            current_func = prev_func;

            src = prev_src;
            ir = prev_ir;
        };
        offset_ptr = &func_offset;
        current_func = func;

        func->src.filename = src->filename;
        func->src.lines.Append(bk_SourceMap::Line { 0, pos < tokens.len ? tokens[pos].line : 0 });
        src = &func->src;
        ir = &func->ir;

        // Create parameter variables
        for (const bk_FunctionInfo::Parameter &param: func->params) {
            bk_VariableInfo *var = program->variables.AppendDefault();
            Size param_pos = definitions_map.FindValue(&param, -1);

            var->name = param.name;
            var->type = param.type;
            var->mut = param.mut;
            var->local = true;
            var->ir = &func->ir;

            var->offset = func_offset;
            func_offset += param.type->size;

            MapVariable(var, param_pos);

            if (poisoned_set.Find(&param)) [[unlikely]] {
                poisoned_set.Set(var);
            }
        }

        // Most code assumes at least one instruction exists
        Emit(bk_Opcode::Nop);

        // Parse function body
        bool has_return = false;
        if (PeekToken(bk_TokenKind::Do)) {
            has_return = ParseDo();
        } else if (EndStatement()) [[likely]] {
            has_return = ParseBlock(false);
            ConsumeToken(bk_TokenKind::End);
        }

        // Deal with inferred return type
        if (func->type == &type_buf) {
            if (!type_buf.ret_type) {
                type_buf.ret_type = bk_NullType;
            } else if (type_buf.ret_type != bk_NullType) {
                Fmt(&signature_buf, ": %1", type_buf.ret_type->signature);
                Fmt(&prototype_buf, ": %1", type_buf.ret_type->signature);
            }

            type_buf.signature = InternString(signature_buf.ptr);
            func->type = InsertType(type_buf, &program->function_types)->AsFunctionType();
            func->prototype = InternString(prototype_buf.ptr);

            if (func->overload_next == func) {
                var->type = func->type;
                var->ir->ptr[var->ir_addr - 1].u2.func = func;
                var->ir->ptr[var->ir_addr - 1].u1.primitive = bk_PrimitiveKind::Function;

                MapVariable(var, func_pos);
            }
        }

        if (!has_return) {
            if (func->type->ret_type == bk_NullType) {
                EmitReturn(0);
            } else {
                MarkError(func_pos, "Some code paths do not return a value in function '%1'", func->name);
            }
        }
    } else {
        HashMap<const char *, const bk_FunctionInfo::Parameter *> names;

        for (const bk_FunctionInfo::Parameter &param: func->params) {
            bool inserted;
            const bk_FunctionInfo::Parameter **ptr = names.TrySet(param.name, &param, &inserted);

            if (!inserted) [[unlikely]] {
                Size param_pos = definitions_map.FindValue(&param, -1);
                Size previous_pos = definitions_map.FindValue(*ptr, -1);

                MarkError(param_pos, "Duplicate member name '%1'", param.name);
                Hint(previous_pos, "Previous member was declared here");
            }
        }
    }

    fwd->skip = pos;
    skip_map.Set(func_pos - 1, fwd);

    // Prevent CTFE of invalid functions
    func->impure |= !func->valid;
}

void bk_Parser::ParseEnum(ForwardInfo *fwd)
{
    Size enum_pos = ++pos;

    if (current_func) [[unlikely]] {
        MarkError(pos, "Enum types cannot be defined inside functions");
        Hint(definitions_map.FindValue(current_func, -1), "Function was started here and is still open");
    } else if (depth) [[unlikely]] {
        MarkError(pos, "Enums must be defined in top-level scope");
    }

    if (fwd != &fake_fwd && fwd->skip >= 0) {
        pos = fwd->skip;
        return;
    }

    bk_EnumTypeInfo *enum_type = program->enum_types.AppendDefault();

    enum_type->signature = ConsumeIdentifier();
    enum_type->primitive = bk_PrimitiveKind::Enum;
    enum_type->init0 = true;
    enum_type->size = 1;

    ConsumeToken(bk_TokenKind::LeftParenthesis);
    if (!MatchToken(bk_TokenKind::RightParenthesis)) [[likely]] {
        do {
            SkipNewLines();

            bk_EnumTypeInfo::Label *label = enum_type->labels.AppendDefault();

            label->name = ConsumeIdentifier();
            label->value = enum_type->labels.len - 1;

            bool inserted;
            enum_type->labels_map.TrySet(label, &inserted);

            if (!inserted) [[unlikely]] {
                MarkError(pos - 1, "Label '%1' is already used", label->name);
            }
        } while (MatchToken(bk_TokenKind::Comma));

        SkipNewLines();
        ConsumeToken(bk_TokenKind::RightParenthesis);
    } else {
        MarkError(pos - 1, "Empty enums are not allowed");
    }

    // Publish enum
    program->types_map.Set(enum_type);

    // Publish symbol
    {
        bk_VariableInfo *var = fwd->var ? fwd->var : CreateGlobal(enum_type->signature, bk_NullType, {{}}, true);

        var->type = bk_TypeType;
        var->ir->ptr[var->ir_addr - 1].u2.type = enum_type;
        var->ir->ptr[var->ir_addr - 1].u1.primitive = bk_PrimitiveKind::Type;

        MapVariable(var, enum_pos);

        // Expressions involving this prototype (function or record) won't issue (visible) errors
        if (!show_errors) [[unlikely]] {
            poisoned_set.Set(var);
        }
    }

    fwd->skip = pos;
    skip_map.Set(enum_pos - 1, fwd);
}

void bk_Parser::ParseReturn()
{
    Size return_pos = ++pos;

    if (!current_func) [[unlikely]] {
        MarkError(pos - 1, "Return statement cannot be used outside function");
        return;
    }

    StackSlot slot;
    if (PeekToken(bk_TokenKind::EndOfLine) || PeekToken(bk_TokenKind::Semicolon)) {
        slot = { bk_NullType };
    } else {
        slot = ParseExpression();
    }

    if (slot.type != current_func->type->ret_type) [[unlikely]] {
        if (!current_func->type->ret_type) [[likely]] {
            bk_FunctionTypeInfo *type = (bk_FunctionTypeInfo *)current_func->type;
            type->ret_type = slot.type;
        } else {
            MarkError(return_pos, "Cannot return '%1' value in function defined to return '%2'",
                      slot.type->signature, current_func->type->ret_type->signature);
            return;
        }
    }

    EmitReturn(slot.type->size);
}

void bk_Parser::ParseLet()
{
    Size var_pos = ++pos;

    bk_VariableInfo *var = program->variables.AppendDefault();

    var->mut = MatchToken(bk_TokenKind::Mut);
    var_pos += var->mut;
    var->name = ConsumeIdentifier();
    var->local = !!current_func;

    Size prev_addr = IR.len;

    StackSlot slot;
    if (MatchToken(bk_TokenKind::Equal)) {
        SkipNewLines();
        slot = ParseExpression();
    } else {
        ConsumeToken(bk_TokenKind::Colon);

        // Don't assign to var->type yet, so that ParseExpression() knows it
        // cannot use this variable.
        const bk_TypeInfo *type = ParseType();

        if (MatchToken(bk_TokenKind::Equal)) {
            SkipNewLines();

            Size expr_pos = pos;
            slot = ParseExpression(0, type);

            if (slot.type != type) [[unlikely]] {
                MarkError(expr_pos - 1, "Cannot assign '%1' value to variable '%2' (defined as '%3')",
                          slot.type->signature, var->name, type->signature);
            }
        } else {
            if (!type->init0) [[unlikely]] {
                 MarkError(var_pos, "Variable '%1' (defined as '%2') must be explicitly initialized",
                           var->name, type->signature);
            }

            Emit(bk_Opcode::Reserve, type->size);
            slot = { type };
        }
    }

    if (!var->mut) {
        if (slot.var && !slot.var->mut && !slot.indirect_addr) {
            const char *name = var->name;

            // We're about to alias var to slot.var... we need to drop the load instructions
            TrimInstructions(prev_addr);

            *var = *slot.var;
            var->name = name;
            var->module = false;

            MapVariable(var, var_pos);
            return;
        }

        if (slot.type->size == 1) {
            if (IR[IR.len - 1].code == bk_Opcode::Push ||
                    IR[IR.len - 1].code == bk_Opcode::Reserve) {
                program->globals.Append(IR[IR.len - 1]);
                TrimInstructions(IR.len - 1);

                var->constant = true;
            }
        } else if (slot.type->size) {
            if (IR[IR.len - 1].code == bk_Opcode::Reserve &&
                    IR[IR.len - 1].u2.i == slot.type->size) {
                program->globals.Append(IR[IR.len - 1]);
                TrimInstructions(IR.len - 1);

                var->constant = true;
            } else {
                var->constant = CopyBigConstant(slot.type->size);
            }
        } else {
            var->constant = true;
        }
    }

    var->type = slot.type;
    var->ir = var->constant ? &program->globals : ir;
    var->ir_addr = var->ir->len;
    var->offset = var->constant ? -1 : *offset_ptr;
    *offset_ptr += var->constant ? 0 : slot.type->size;

    MapVariable(var, var_pos);

    // Expressions involving this variable won't issue (visible) errors
    // and will be marked as invalid too.
    if (!show_errors) [[unlikely]] {
        poisoned_set.Set(var);
    }
}

bool bk_Parser::ParseIf()
{
    pos++;

    ParseExpression(bk_BoolType);

    bool fold = (IR[IR.len - 1].code == bk_Opcode::Push);
    bool fold_test = fold && IR[IR.len - 1].u2.b;
    bool fold_skip = fold && fold_test;
    TrimInstructions(IR.len - fold);

    Size branch_addr = IR.len;
    if (!fold) {
        Emit(bk_Opcode::BranchIfFalse);
    }

    bool has_return = true;
    bool is_exhaustive = false;

    if (PeekToken(bk_TokenKind::Do)) {
        has_return &= ParseDo();

        if (fold) {
            if (fold_test) {
                is_exhaustive = true;
            } else {
                TrimInstructions(branch_addr);
            }
        } else {
            IR[branch_addr].u2.i = IR.len - branch_addr;
        }
    } else if (EndStatement()) [[likely]] {
        has_return &= ParseBlock(true);

        if (MatchToken(bk_TokenKind::Else)) {
            Size jump_addr;
            if (fold && !fold_test) {
                TrimInstructions(branch_addr);
                jump_addr = -1;
            } else if (!fold) {
                jump_addr = IR.len;
                Emit(bk_Opcode::Jump, -1);
            } else {
                jump_addr = -1;
            }

            do {
                if (!fold) {
                    IR[branch_addr].u2.i = IR.len - branch_addr;
                }

                if (MatchToken(bk_TokenKind::If)) {
                    Size test_addr = IR.len;
                    ParseExpression(bk_BoolType);

                    fold = fold_skip || (IR[IR.len - 1].code == bk_Opcode::Push);
                    fold_test = fold && !fold_skip && IR[IR.len - 1].u2.b;
                    TrimInstructions(fold ? test_addr : IR.len);

                    if (EndStatement()) [[likely]] {
                        branch_addr = IR.len;
                        if (!fold) {
                            Emit(bk_Opcode::BranchIfFalse);
                        }

                        bool block_return = ParseBlock(true);

                        if (fold) {
                            if (fold_test) {
                                has_return = block_return;
                                is_exhaustive = true;
                            } else {
                                TrimInstructions(branch_addr);
                            }
                        } else {
                            has_return &= block_return;

                            Emit(bk_Opcode::Jump, jump_addr);
                            jump_addr = IR.len - 1;
                        }
                        fold_skip |= fold && fold_test;
                    }
                } else if (EndStatement()) [[likely]] {
                    Size else_addr = IR.len;
                    bool block_return = ParseBlock(false);

                    if (fold && !fold_skip) {
                        has_return = block_return;
                    } else if (!fold) {
                        has_return &= block_return;
                    }
                    is_exhaustive = true;

                    TrimInstructions(fold_skip ? else_addr : IR.len);

                    break;
                }
            } while (MatchToken(bk_TokenKind::Else));

            FixJumps(jump_addr, IR.len);
        } else {
            if (fold) {
                if (fold_test) {
                    is_exhaustive = true;
                } else {
                    TrimInstructions(branch_addr);
                }
            } else {
                IR[branch_addr].u2.i = IR.len - branch_addr;
            }
        }

        ConsumeToken(bk_TokenKind::End);
    }

    return has_return && is_exhaustive;
}

void bk_Parser::ParseWhile()
{
    pos++;

    // Parse expression. We'll make a copy after the loop body so that the IR code looks
    // roughly like if (cond) { do { ... } while (cond) }.
    Size condition_addr = IR.len;
    Size condition_line_idx = src->lines.len;
    ParseExpression(bk_BoolType);

    bool fold = (IR[IR.len - 1].code == bk_Opcode::Push);
    bool fold_test = fold && IR[IR.len - 1].u2.b;
    TrimInstructions(IR.len - fold);

    Size branch_addr = IR.len;
    if (!fold) {
        Emit(bk_Opcode::BranchIfFalse);
    }

    // Break and continue need to apply to while loop blocks
    RG_DEFER_C(prev_loop = loop) { loop = prev_loop; };
    LoopContext ctx = { *offset_ptr, -1, -1 };
    loop = &ctx;

    // Parse body
    if (PeekToken(bk_TokenKind::Do)) {
        ParseDo();
    } else if (EndStatement()) [[likely]] {
        ParseBlock(false);
        ConsumeToken(bk_TokenKind::End);
    }

    // Append loop outro
    if (fold) {
        if (fold_test) {
            FixJumps(ctx.continue_addr, branch_addr);
            Emit(bk_Opcode::Jump, branch_addr - IR.len);
            FixJumps(ctx.break_addr, IR.len);
        } else {
            TrimInstructions(branch_addr);
        }
    } else {
        FixJumps(ctx.continue_addr, IR.len);

        // Copy the condition expression, and the IR/line map information
        for (Size i = condition_line_idx; i < src->lines.len &&
                                          src->lines[i].addr < branch_addr; i++) {
            const bk_SourceMap::Line &line = src->lines[i];
            src->lines.Append({ IR.len + (line.addr - condition_addr), line.line });
        }
        IR.Grow(branch_addr - condition_addr);
        IR.Append(IR.Take(condition_addr, branch_addr - condition_addr));

        Emit(bk_Opcode::BranchIfTrue, branch_addr - IR.len + 1);
        IR[branch_addr].u2.i = IR.len - branch_addr;

        FixJumps(ctx.break_addr, IR.len);
    }
}

void bk_Parser::ParseFor()
{
    Size for_pos = ++pos;

    bk_VariableInfo *it = program->variables.AppendDefault();

    it->mut = MatchToken(bk_TokenKind::Mut);
    for_pos += it->mut;
    it->name = ConsumeIdentifier();
    it->local = !!current_func;
    it->ir = ir;

    MapVariable(it, for_pos);

    ConsumeToken(bk_TokenKind::In);
    ParseExpression(bk_IntType);
    ConsumeToken(bk_TokenKind::Colon);
    ParseExpression(bk_IntType);

    // Make sure start and end value remain on the stack
    it->offset = *offset_ptr + 2;
    *offset_ptr += 3;

    // Put iterator value on the stack
    Emit(bk_Opcode::LoadLocal, it->offset - 2);
    it->type = bk_IntType;

    Size body_addr = IR.len;

    Emit(bk_Opcode::LoadLocal, it->offset);
    Emit(bk_Opcode::LoadLocal, it->offset - 1);
    Emit(bk_Opcode::LessThanInt);
    Emit(bk_Opcode::BranchIfFalse);

    // Break and continue need to apply to while loop blocks
    RG_DEFER_C(prev_loop = loop) { loop = prev_loop; };
    LoopContext ctx = { *offset_ptr, -1, -1 };
    loop = &ctx;

    // Parse body
    if (PeekToken(bk_TokenKind::Do)) {
        ParseDo();
    } else if (EndStatement()) [[likely]] {
        ParseBlock(false);
        ConsumeToken(bk_TokenKind::End);
    }

    // Loop outro
    if (IR.len > body_addr + 4) {
        FixJumps(ctx.continue_addr, IR.len);

        Emit(bk_Opcode::Push, { .primitive = bk_PrimitiveKind::Integer }, 1);
        Emit(bk_Opcode::AddInt);
        Emit(bk_Opcode::Jump, body_addr - IR.len);
        IR[body_addr + 3].u2.i = IR.len - (body_addr + 3);

        FixJumps(ctx.break_addr, IR.len);
        EmitPop(3);
    } else {
        TrimInstructions(body_addr - 1);
        DiscardResult(2);
    }

    // Destroy iterator and range values
    DestroyVariables(program->variables.count - 1);
    *offset_ptr -= 3;
}

void bk_Parser::ParseBreak()
{
    Size break_pos = ++pos;

    if (!loop) [[unlikely]] {
        MarkError(break_pos - 1, "Break statement outside of loop");
        return;
    }

    EmitPop(*offset_ptr - loop->offset);

    Emit(bk_Opcode::Jump, loop->break_addr);
    loop->break_addr = IR.len - 1;
}

void bk_Parser::ParseContinue()
{
    Size continue_pos = ++pos;

    if (!loop) [[unlikely]] {
        MarkError(continue_pos - 1, "Continue statement outside of loop");
        return;
    }

    EmitPop(*offset_ptr - loop->offset);

    Emit(bk_Opcode::Jump, loop->continue_addr);
    loop->continue_addr = IR.len - 1;
}

static int GetOperatorPrecedence(bk_TokenKind kind, bool expect_unary)
{
    if (expect_unary) {
        switch (kind) {
            case bk_TokenKind::XorOrComplement:
            case bk_TokenKind::Plus:
            case bk_TokenKind::Minus: return 13;
            case bk_TokenKind::Not: return 4;

            default: return -1;
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
            case bk_TokenKind::XorAssign: return 0;

            case bk_TokenKind::OrOr: return 2;
            case bk_TokenKind::AndAnd: return 3;
            case bk_TokenKind::Equal:
            case bk_TokenKind::NotEqual: return 5;
            case bk_TokenKind::Greater:
            case bk_TokenKind::GreaterOrEqual:
            case bk_TokenKind::Less:
            case bk_TokenKind::LessOrEqual: return 6;
            case bk_TokenKind::Or: return 7;
            case bk_TokenKind::XorOrComplement: return 8;
            case bk_TokenKind::And: return 9;
            case bk_TokenKind::LeftShift:
            case bk_TokenKind::RightShift:
            case bk_TokenKind::LeftRotate:
            case bk_TokenKind::RightRotate: return 10;
            case bk_TokenKind::Plus:
            case bk_TokenKind::Minus: return 11;
            case bk_TokenKind::Multiply:
            case bk_TokenKind::Divide:
            case bk_TokenKind::Modulo: return 12;

            default: return -1;
        }
    }
}

StackSlot bk_Parser::ParseExpression(unsigned int flags, const bk_TypeInfo *hint)
{
    Size start_stack_len = stack.len;
    RG_DEFER { stack.RemoveFrom(start_stack_len); };

    // Safety dummy
    stack.Append({ bk_NullType });

    LocalArray<PendingOperator, 128> operators;
    bool expect_value = true;
    Size parentheses = 0;

    // Used to detect "empty" expressions
    Size prev_offset = pos;

    bool recurse = RecurseInc();
    RG_DEFER { RecurseDec(); };

    if (!recurse) [[unlikely]] {
        MarkError(pos, "Excessive parsing depth (compiler limit)");
        Hint(-1, "Simplify surrounding code");

        goto error;
    }

    while (pos < tokens.len) [[likely]] {
        const bk_Token &tok = tokens[pos++];

        switch (tok.kind) {
            case bk_TokenKind::LeftParenthesis: {
                if (!expect_value) [[unlikely]] {
                    if (stack[stack.len - 1].type->primitive == bk_PrimitiveKind::Function) {
                        const bk_FunctionTypeInfo *func_type = stack[stack.len - 1].type->AsFunctionType();

                        if (!ParseCall(func_type, nullptr, false))
                            goto error;
                    } else {
                        goto unexpected;
                    }
                } else {
                    PendingOperator op = {};

                    op.kind = tok.kind;
                    operators.Append(op);

                    parentheses++;
                }
            } break;
            case bk_TokenKind::RightParenthesis: {
                if (expect_value) [[unlikely]]
                    goto unexpected;
                expect_value = false;

                if (!parentheses) {
                    if (pos == prev_offset + 1) [[unlikely]] {
                        MarkError(pos - 1, "Unexpected token ')', expected value or expression");
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
                if (!expect_value) [[unlikely]]
                    goto unexpected;
                expect_value = false;

                stack.Append({ bk_NullType });
            } break;
            case bk_TokenKind::Boolean: {
                if (!expect_value) [[unlikely]]
                    goto unexpected;
                expect_value = false;

                Emit(bk_Opcode::Push, { .primitive = bk_PrimitiveKind::Boolean }, tok.u.b);
                stack.Append({ bk_BoolType });
            } break;
            case bk_TokenKind::Integer: {
                if (!expect_value) [[unlikely]]
                    goto unexpected;
                expect_value = false;

                Emit(bk_Opcode::Push, { .primitive = bk_PrimitiveKind::Integer }, tok.u.i);
                stack.Append({ bk_IntType });
            } break;
            case bk_TokenKind::Float: {
                if (!expect_value) [[unlikely]]
                    goto unexpected;
                expect_value = false;

                Emit(bk_Opcode::Push, { .primitive = bk_PrimitiveKind::Float }, tok.u.d);
                stack.Append({ bk_FloatType });
            } break;
            case bk_TokenKind::String: {
                if (!expect_value) [[unlikely]]
                    goto unexpected;
                expect_value = false;

                const char *str = InternString(tok.u.str);
                str = str[0] ? str : nullptr;

                Emit(bk_Opcode::Push, { .primitive = bk_PrimitiveKind::String }, str);
                stack.Append({ bk_StringType });
            } break;

            case bk_TokenKind::Func: {
                if (!expect_value) [[unlikely]]
                    goto unexpected;
                expect_value = false;

                const bk_TypeInfo *type = ParseFunctionType();

                Emit(bk_Opcode::Push, { .primitive = bk_PrimitiveKind::Type }, type);
                stack.Append({ bk_TypeType });
            } break;

            case bk_TokenKind::LeftBracket: {
                if (expect_value) {
                    expect_value = false;

                    const bk_TypeInfo *type = ParseArrayType();

                    Emit(bk_Opcode::Push, { .primitive = bk_PrimitiveKind::Type }, type);
                    stack.Append({ bk_TypeType });
                } else if (stack[stack.len - 1].type->primitive == bk_PrimitiveKind::Array) {
                    ParseArraySubscript();
                } else {
                    MarkError(pos - 1, "Cannot subset non-array expression");
                    goto error;
                }
            } break;

            case bk_TokenKind::Dot: {
                if (!expect_value) {
                    bk_PrimitiveKind primitive = stack[stack.len - 1].type->primitive;

                    if (primitive == bk_PrimitiveKind::Record) {
                        ParseRecordDot();
                        break;
                    } else if (primitive == bk_PrimitiveKind::Type &&
                               IR[IR.len - 1].code == bk_Opcode::Push &&
                               IR[IR.len - 1].u2.type->primitive == bk_PrimitiveKind::Enum) {
                        ParseEnumDot();
                    } else {
                        MarkError(pos - 1, "Cannot use dot operator on value of type '%1'", stack[stack.len - 1].type->signature);
                        goto error;
                    }
                } else {
                    goto unexpected;
                }
            } break;

            case bk_TokenKind::Identifier: {
                if (!expect_value) [[unlikely]]
                    goto unexpected;
                expect_value = false;

                const char *name = tok.u.str;
                Size var_pos = pos - 1;
                bool call = MatchToken(bk_TokenKind::LeftParenthesis);

                bk_VariableInfo *var = FindVariable(name);

                if (!var) [[unlikely]] {
                    MarkError(var_pos, "Reference to unknown identifier '%1'", name);
                    HintSuggestions(name, program->variables);

                    goto error;
                }

                EmitLoad(var);
                show_errors &= !poisoned_set.Find(var);

                if (call) {
                    bk_PrimitiveKind primitive = var->type->primitive;

                    if (primitive == bk_PrimitiveKind::Function) {
                        if (IR[IR.len - 1].code == bk_Opcode::Push) {
                            RG_ASSERT(IR[IR.len - 1].u1.primitive == bk_PrimitiveKind::Function);

                            bk_FunctionInfo *func = (bk_FunctionInfo *)IR[IR.len - 1].u2.func;
                            bool overload = var->module;

                            TrimInstructions(IR.len - 1);
                            stack.len--;

                            if (!ParseCall(var->type->AsFunctionType(), func, overload))
                                goto error;
                        } else {
                            if (!ParseCall(var->type->AsFunctionType(), nullptr, false))
                                goto error;
                        }
                    } else if (primitive == bk_PrimitiveKind::Type) {
                        if (IR[IR.len - 1].code == bk_Opcode::Push) {
                            RG_ASSERT(IR[IR.len - 1].u1.primitive == bk_PrimitiveKind::Type);

                            const bk_TypeInfo *type = IR[IR.len - 1].u2.type;

                            if (type->primitive == bk_PrimitiveKind::Record) [[likely]] {
                                const bk_RecordTypeInfo *record_type = type->AsRecordType();
                                bk_FunctionInfo *func = (bk_FunctionInfo *)record_type->func;

                                TrimInstructions(IR.len - 1);
                                stack.len--;

                                if (!ParseCall(func->type, func, false))
                                    goto error;
                            } else {
                                MarkError(var_pos, "Variable '%1' is not a function and cannot be called", var->name);
                                goto error;
                            }
                        } else {
                            MarkError(var_pos, "Record constructors can only be called directly");
                            goto error;
                        }
                    } else {
                        MarkError(var_pos, "Variable '%1' is not a function and cannot be called", var->name);
                        goto error;
                    }
                } else if (!call && var->module && var->type->primitive == bk_PrimitiveKind::Function) {
                    RG_ASSERT(IR[IR.len - 1].code == bk_Opcode::Push &&
                              IR[IR.len - 1].u1.primitive == bk_PrimitiveKind::Function);

                    bk_FunctionInfo *func = (bk_FunctionInfo *)IR[IR.len - 1].u2.func;

                    if (func->overload_next != func) {
                        bool ambiguous = true;

                        if (hint) {
                            bk_FunctionInfo *it = func;
                            do {
                                if (it->type == hint) {
                                    IR[IR.len - 1].u2.func = it;
                                    stack[stack.len - 1] = { it->type };

                                    ambiguous = false;
                                    break;
                                }

                                it = it->overload_next;
                            } while (it != func);
                        }

                        if (ambiguous) {
                            MarkError(var_pos, "Ambiguous reference to overloaded function '%1'", var->name);

                            // Show all candidate functions with same name
                            const bk_FunctionInfo *it = func;
                            do {
                                Hint(definitions_map.FindValue(it, -1), "Candidate '%1'", it->prototype);
                                it = it->overload_next;
                            } while (it != func);

                            goto error;
                        }
                    } else if (func->mode == bk_FunctionInfo::Mode::Intrinsic) [[unlikely]] {
                        MarkError(var_pos, "Intrinsic functions can only be called directly");
                        goto error;
                    }
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
                    if (pos == prev_offset + 1) [[unlikely]] {
                        MarkError(pos - 1, "Unexpected token '%1', expected value or expression",
                                  bk_TokenKindNames[(int)tokens[pos - 1].kind]);
                        goto error;
                    } else if (expect_value || parentheses) [[unlikely]] {
                        pos--;
                        if (SkipNewLines()) [[likely]] {
                            continue;
                        } else {
                            pos++;
                            goto unexpected;
                        }
                    } else {
                        pos--;
                        goto end;
                    }
                }

                if (flags & (int)ExpressionFlag::StopOperator) {
                    pos--;
                    goto end;
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
                    // stack slots, because it will be needed when we emit the store instruction
                    // and will be removed then.
                    Size trim = std::min(stack[stack.len - 1].type->size, (Size)2);
                    TrimInstructions(IR.len - trim);
                } else if (tok.kind == bk_TokenKind::AndAnd) {
                    op.branch_addr = IR.len;
                    Emit(bk_Opcode::SkipIfFalse);
                } else if (tok.kind == bk_TokenKind::OrOr) {
                    op.branch_addr = IR.len;
                    Emit(bk_Opcode::SkipIfTrue);
                }

                if (!operators.Available()) [[unlikely]] {
                    MarkError(pos - 1, "Too many operators on the stack (compiler limitation)");
                    goto error;
                }
                operators.Append(op);
            } break;
        }

        if (stack.len >= 64) [[unlikely]] {
            MarkError(pos, "Excessive complexity while parsing expression (compiler limit)");
            Hint(-1, "Simplify expression");
            goto error;
        }
    }

end:
    if (expect_value || parentheses) [[unlikely]] {
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
    while (pos < tokens.len) [[likely]] {
        if (tokens[pos].kind == bk_TokenKind::Do)
            break;
        if (tokens[pos].kind == bk_TokenKind::EndOfLine)
            break;
        if (tokens[pos].kind == bk_TokenKind::Semicolon)
            break;

        parentheses += (tokens[pos].kind == bk_TokenKind::LeftParenthesis);
        if (tokens[pos].kind == bk_TokenKind::RightParenthesis && parentheses-- < 0)
            break;

        pos++;
    };

    return { bk_NullType };
}

bool bk_Parser::ParseExpression(const bk_TypeInfo *expected_type)
{
    Size expr_pos = pos;

    const bk_TypeInfo *type = ParseExpression().type;

    if (type != expected_type) [[unlikely]] {
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

        if (!dest.var) [[unlikely]] {
            MarkError(op.pos, "Cannot assign result to temporary value; left operand should be a variable");
            return;
        }
        if (!dest.var->mut) [[unlikely]] {
            MarkError(op.pos, "Cannot assign result to non-mutable variable '%1'", dest.var->name);
            Hint(definitions_map.FindValue(dest.var, -1), "Variable '%1' is defined without 'mut' qualifier", dest.var->name);

            return;
        }
        if (dest.type != expr.type) [[unlikely]] {
            if (!dest.indirect_addr) {
                MarkError(op.pos, "Cannot assign '%1' value to variable '%2'", expr.type->signature, dest.var->name);
            } else {
                MarkError(op.pos, "Cannot assign '%1' value here, expected '%2'", expr.type->signature, dest.type->signature);
            }
            Hint(definitions_map.FindValue(dest.var, -1), "Variable '%1' is defined as '%2'",
                      dest.var->name, dest.var->type->signature);
            return;
        }

        switch (op.kind) {
            case bk_TokenKind::Reassign: {
                stack[--stack.len - 1].var = nullptr;
                success = true;
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

        if (current_func) {
            current_func->side_effects |= !dest.var->local;
        }

        if (dest.indirect_addr) {
            // In order for StoreIndirectK to work, the variable address must remain on the stack.
            // To do so, replace LoadIndirect (which removes them) with LoadIndirectK.
            if (op.kind != bk_TokenKind::Reassign) {
                RG_ASSERT(IR[dest.indirect_addr].code == bk_Opcode::LoadIndirect);
                IR[dest.indirect_addr].code = bk_Opcode::LoadIndirectK;
            }

            Emit(bk_Opcode::StoreIndirectK, dest.type->size);
        } else if (dest.type->size == 1) {
            bk_Opcode code = dest.var->local ? bk_Opcode::StoreLocalK : bk_Opcode::StoreK;
            Emit(code, dest.var->offset);
        } else if (dest.type->size) {
            bk_Opcode code = dest.var->local ? bk_Opcode::LeaLocal : bk_Opcode::Lea;
            Emit(code, dest.var->offset);
            Emit(bk_Opcode::StoreRevK, dest.type->size);
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
                    bk_Instruction *inst = &IR[IR.len - 1];

                    if (inst->code == bk_Opcode::NegateInt || inst->code == bk_Opcode::NegateFloat) {
                        TrimInstructions(IR.len - 1);
                        success = true;
                    } else {
                        success = EmitOperator1(bk_PrimitiveKind::Integer, bk_Opcode::NegateInt, stack[stack.len - 1].type) ||
                                  EmitOperator1(bk_PrimitiveKind::Float, bk_Opcode::NegateFloat, stack[stack.len - 1].type);
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
                          EmitOperator2(bk_PrimitiveKind::Function, bk_Opcode::EqualFunc, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Enum, bk_Opcode::EqualEnum, bk_BoolType);
            } break;
            case bk_TokenKind::NotEqual: {
                success = EmitOperator2(bk_PrimitiveKind::Integer, bk_Opcode::NotEqualInt, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Float, bk_Opcode::NotEqualFloat, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Boolean, bk_Opcode::NotEqualBool, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::String, bk_Opcode::NotEqualString, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Type, bk_Opcode::NotEqualType, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Function, bk_Opcode::NotEqualFunc, bk_BoolType) ||
                          EmitOperator2(bk_PrimitiveKind::Enum, bk_Opcode::NotEqualEnum, bk_BoolType);
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
                RG_ASSERT(op.branch_addr && IR[op.branch_addr].code == bk_Opcode::SkipIfFalse);
                IR[op.branch_addr].u2.i = IR.len - op.branch_addr + 1;

                success = EmitOperator2(bk_PrimitiveKind::Boolean, bk_Opcode::AndBool, stack[stack.len - 2].type);
            } break;
            case bk_TokenKind::OrOr: {
                RG_ASSERT(op.branch_addr && IR[op.branch_addr].code == bk_Opcode::SkipIfTrue);
                IR[op.branch_addr].u2.i = IR.len - op.branch_addr + 1;

                success = EmitOperator2(bk_PrimitiveKind::Boolean, bk_Opcode::OrBool, stack[stack.len - 2].type);
            } break;

            default: { RG_UNREACHABLE(); } break;
        }
    }

    if (!success) [[unlikely]] {
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
        Emit(code);
        FoldInstruction(1, out_type);

        stack[stack.len - 1] = { out_type };

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
        Emit(code);
        FoldInstruction(2, out_type);

        stack[--stack.len - 1] = { out_type };

        return true;
    } else {
        return false;
    }
}

bk_VariableInfo *bk_Parser::FindVariable(const char *name)
{
    bk_VariableInfo *var = program->variables_map.FindValue(name, nullptr);

    if (!var) {
        ForwardInfo **ptr = forwards_map.Find(name);

        if (!ptr) [[unlikely]]
            return nullptr;

        ForwardInfo *fwd0 = *ptr;
        ForwardInfo *fwd = fwd0;

        // Make sure we don't come back here by accident
        forwards_map.Remove(ptr);

        RG_DEFER_C(prev_ir = ir,
                   prev_src = src,
                   prev_func = current_func,
                   prev_depth = depth,
                   prev_offset = offset_ptr) {
            ir = prev_ir;
            src = prev_src;
            current_func = prev_func;
            depth = prev_depth;
            offset_ptr = prev_offset;
        };
        src = &program->sources[program->sources.len - 1];
        ir = &program->main;
        current_func = nullptr;
        depth = 0;
        offset_ptr = &main_offset;

        do {
            RG_DEFER_C(prev_pos = pos,
                       prev_errors = show_errors,
                       prev_loop = loop) {
                pos = prev_pos;
                show_errors = prev_errors;
                loop = prev_loop;
            };
            pos = fwd->pos;
            show_errors = true;
            loop = nullptr;

            switch (fwd->kind) {
                case bk_TokenKind::Func: { ParseFunction(fwd, false); } break;
                case bk_TokenKind::Record: { ParseFunction(fwd, true); } break;
                case bk_TokenKind::Enum: { ParseEnum(fwd); } break;

                default: { RG_UNREACHABLE(); } break;
            }

            fwd = fwd->next;
        } while (fwd);

        var = (fwd0 && fwd0->var->type != bk_NullType) ? fwd0->var : nullptr;
    }

    return var;
}

const bk_FunctionTypeInfo *bk_Parser::ParseFunctionType()
{
    bk_FunctionTypeInfo type_buf = {};
    HeapArray<char> signature_buf;

    type_buf.primitive = bk_PrimitiveKind::Function;
    type_buf.size = 1;
    signature_buf.Append("func (");

    // Parameters
    ConsumeToken(bk_TokenKind::LeftParenthesis);
    if (!MatchToken(bk_TokenKind::RightParenthesis)) {
        for (;;) {
            SkipNewLines();

            const bk_TypeInfo *type = ParseType();

            if (type_buf.params.Available()) [[likely]] {
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
        type_buf.ret_type = ParseType();

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
    bool multi = false;

    type_buf.primitive = bk_PrimitiveKind::Array;

    // Parse array length
    {
        const bk_TypeInfo *type = ParseExpression().type;

        if (MatchToken(bk_TokenKind::Comma)) {
            multi = true;
        } else {
            ConsumeToken(bk_TokenKind::RightBracket);
            multi = false;
        }

        if (type == bk_IntType) [[likely]] {
            // Once we start to implement constant folding and CTFE, more complex expressions
            // should work without any change here.
            if (IR[IR.len - 1].code == bk_Opcode::Push) [[likely]] {
                type_buf.len = IR[IR.len - 1].u2.i;
                TrimInstructions(IR.len - 1);
            } else {
                MarkError(def_pos, "Complex 'Int' expression cannot be resolved statically");
                type_buf.len = 0;
            }
        } else {
            MarkError(def_pos, "Expected an 'Int' expression, not '%1'", type->signature);
            type_buf.len = 0;
        }
    }

    // Unit type
    if (multi) {
        bool recurse = RecurseInc();
        RG_DEFER { RecurseDec(); };

        if (recurse) [[likely]] {
            type_buf.unit_type = ParseArrayType();
        } else {
            MarkError(pos, "Excessive parsing depth (compiler limit)");
            Hint(-1, "Simplify surrounding code");

            type_buf.unit_type = bk_NullType;
        }
    } else {
        type_buf.unit_type = ParseType();
    }
    type_buf.init0 = type_buf.unit_type->init0;
    type_buf.size = type_buf.len * type_buf.unit_type->size;

    // Safety checks
    if (type_buf.len < 0) [[unlikely]] {
        MarkError(def_pos, "Negative array size is not valid");
    }
    if (type_buf.len > UINT16_MAX ||
            type_buf.unit_type->size > UINT16_MAX ||
            type_buf.size > UINT16_MAX) [[unlikely]] {
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
    if (!stack[stack.len - 1].indirect_addr) {
        if (!stack[stack.len - 1].lea) {
            // If an array gets loaded from a variable, its address is already on the stack
            // because of EmitLoad. But if it is a temporary value, we need to do it now.

            Emit(bk_Opcode::LeaRel, -stack[stack.len - 1].type->size);
            stack[stack.len - 1].indirect_addr = IR.len;
        } else {
            stack[stack.len - 1].indirect_addr = IR.len - 1;
        }
    }

    do {
        const bk_ArrayTypeInfo *array_type = stack[stack.len - 1].type->AsArrayType();
        const bk_TypeInfo *unit_type = array_type->unit_type;

        // Kill the load instructions, we need to adjust the index
        TrimInstructions(stack[stack.len - 1].indirect_addr);

        Size idx_pos = pos;

        // Parse index expression
        {
            const bk_TypeInfo *type = ParseExpression().type;

            if (type != bk_IntType) [[unlikely]] {
                MarkError(idx_pos, "Expected an 'Int' expression, not '%1'", type->signature);
            }
        }

        // Compute array index
        if (IR[IR.len - 1].code == bk_Opcode::Push) {
            int64_t idx = IR[IR.len - 1].u2.i;
            int64_t offset = idx * unit_type->size;

            if (show_errors) {
                RG_ASSERT(IR[IR.len - 1].u1.primitive == bk_PrimitiveKind::Integer);

                if (idx < 0 || idx >= array_type->len) {
                    MarkError(idx_pos, "Index is out of range: %1 (array length %2)", idx, array_type->len);
                }
            }

            if (IR[IR.len - 2].code == bk_Opcode::Lea ||
                    IR[IR.len - 2].code == bk_Opcode::LeaLocal ||
                    IR[IR.len - 2].code == bk_Opcode::LeaRel) {
                TrimInstructions(IR.len - 1);
                IR[IR.len - 1].u2.i += offset;
            } else {
                IR[IR.len - 1].u2.i = offset;
            }
        } else {
            Emit(bk_Opcode::CheckIndex, array_type->len);
            if (unit_type->size != 1) {
                Emit(bk_Opcode::Push, { .primitive = bk_PrimitiveKind::Integer }, unit_type->size);
                Emit(bk_Opcode::MultiplyInt);
            }
            Emit(bk_Opcode::AddInt);
        }

        // Load value
        stack[stack.len - 1].indirect_addr = IR.len;
        Emit(bk_Opcode::LoadIndirect, unit_type->size);

        // Clean up temporary value (if any)
        if (!stack[stack.len - 1].lea) {
            Emit(bk_Opcode::LeaRel, -unit_type->size - array_type->size);
            Emit(bk_Opcode::StoreRev, unit_type->size);

            stack[stack.len - 1].indirect_imbalance += array_type->size - unit_type->size;
            EmitPop(stack[stack.len - 1].indirect_imbalance);
        }

        stack[stack.len - 1].type = unit_type;
    } while (stack[stack.len - 1].type->primitive == bk_PrimitiveKind::Array &&
             MatchToken(bk_TokenKind::Comma));

    ConsumeToken(bk_TokenKind::RightBracket);
}

void bk_Parser::ParseRecordDot()
{
    Size member_pos = pos;

    const bk_RecordTypeInfo *record_type = stack[stack.len - 1].type->AsRecordType();

    if (!stack[stack.len - 1].indirect_addr) {
        if (!stack[stack.len - 1].lea) {
            // If a record gets loaded from a variable, its address is already on the stack
            // because of EmitLoad. But if it is a temporary value, we need to do it now.

            Emit(bk_Opcode::LeaRel, -record_type->size);
            stack[stack.len - 1].indirect_addr = IR.len;
        } else {
            stack[stack.len - 1].indirect_addr = IR.len - 1;
        }
    }

    // Kill the load instructions, we need to adjust the index
    TrimInstructions(stack[stack.len - 1].indirect_addr);

    const char *name = ConsumeIdentifier();
    const bk_RecordTypeInfo::Member *member =
        std::find_if(record_type->members.begin(), record_type->members.end(),
                     [&](const bk_RecordTypeInfo::Member &member) { return TestStr(member.name, name); });

    if (member == record_type->members.end()) [[unlikely]] {
        MarkError(member_pos, "Record '%1' does not contain member called '%2'", record_type->signature, name);
        HintSuggestions(name, record_type->members);

        return;
    }

    // Resolve member
    if (member->offset) {
        if (IR[IR.len - 1].code == bk_Opcode::Lea ||
                IR[IR.len - 1].code == bk_Opcode::LeaLocal ||
                IR[IR.len - 1].code == bk_Opcode::LeaRel) {
            IR[IR.len - 1].u2.i += member->offset;
        } else {
            Emit(bk_Opcode::Push, { .primitive = bk_PrimitiveKind::Integer }, member->offset);
            Emit(bk_Opcode::AddInt);
        }
    }

    // Load value
    stack[stack.len - 1].indirect_addr = IR.len;
    Emit(bk_Opcode::LoadIndirect, member->type->size);

    // Clean up temporary value (if any)
    if (!stack[stack.len - 1].lea) {
        Emit(bk_Opcode::LeaRel, -member->type->size - record_type->size);
        Emit(bk_Opcode::StoreRev, member->type->size);

        stack[stack.len - 1].indirect_imbalance += record_type->size - member->type->size;
        EmitPop(stack[stack.len - 1].indirect_imbalance);
    }

    stack[stack.len - 1].type = member->type;
}

void bk_Parser::ParseEnumDot()
{
    Size label_pos = pos;

    RG_ASSERT(IR[IR.len - 1].code == bk_Opcode::Push &&
              IR[IR.len - 1].u1.primitive == bk_PrimitiveKind::Type);
    const bk_EnumTypeInfo *enum_type = IR.ptr[--IR.len].u2.type->AsEnumType();

    const char *name = ConsumeIdentifier();
    const bk_EnumTypeInfo::Label *label = enum_type->labels_map.FindValue(name, nullptr);

    if (!label) [[unlikely]] {
        MarkError(label_pos, "Enum '%1' does not contain label called '%2'", enum_type->signature, name);
        HintSuggestions(name, enum_type->labels);

        return;
    }

    Emit(bk_Opcode::Push, { .primitive = bk_PrimitiveKind::Enum }, label->value);

    stack[stack.len - 1] = { enum_type };
}

// Don't try to call from outside ParseExpression()!
bool bk_Parser::ParseCall(const bk_FunctionTypeInfo *func_type, const bk_FunctionInfo *func, bool overload)
{
    LocalArray<const bk_TypeInfo *, RG_LEN(bk_FunctionTypeInfo::params.data)> args;

    Size call_pos = pos - 1;
    Size call_addr = IR.len;
    bool variadic = func_type->variadic && (!func || func->mode != bk_FunctionInfo::Mode::Intrinsic);

    // Parse arguments
    Size args_size = 0;
    if (!MatchToken(bk_TokenKind::RightParenthesis)) {
        do {
            SkipNewLines();

            if (!args.Available()) [[unlikely]] {
                MarkError(pos, "Functions cannot take more than %1 arguments", RG_LEN(args.data));
                return false;
            }

            if (variadic && args.len >= func_type->params.len) {
                Size type_addr = IR.len;
                Emit(bk_Opcode::Push, { .primitive = bk_PrimitiveKind::Type }, bk_NullType);

                const bk_TypeInfo *type = ParseExpression().type;
                args.Append(type);
                args_size += 1 + type->size;

                IR[type_addr].u2.type = type;
            } else {
                const bk_TypeInfo *type = ParseExpression().type;
                args.Append(type);
                args_size += type->size;
            }
        } while (MatchToken(bk_TokenKind::Comma));

        SkipNewLines();
        ConsumeToken(bk_TokenKind::RightParenthesis);
    }
    if (variadic) {
        Emit(bk_Opcode::Push, { .primitive = bk_PrimitiveKind::Integer }, args_size - func_type->params.len);
        args_size++;
    }

    // Find appropriate overload. Variadic functions cannot be overloaded but it
    // does not hurt to use the same logic to check argument types.
    if (func && overload) {
        const bk_FunctionInfo *func0 = func;

        while (!TestOverload(*func->type, args)) {
            func = func->overload_next;

            if (func == func0) [[unlikely]] {
                LocalArray<char, 1024> buf;
                for (Size i = 0; i < args.len; i++) {
                    buf.len += Fmt(buf.TakeAvailable(), "%1%2", i ? ", " : "", args[i]->signature).len;
                }

                MarkError(call_pos, "Cannot call '%1' with (%2) arguments", func->name, buf);

                // Show all candidate functions with same name
                const bk_FunctionInfo *it = func0;
                do {
                    Hint(definitions_map.FindValue(it, -1), "Candidate '%1'", it->prototype);
                    it = it->overload_next;
                } while (it != func0);

                return false;
            }
        }

        func_type = func->type;
    } else if (!TestOverload(*func_type, args)) {
        LocalArray<char, 1024> buf;
        for (Size i = 0; i < args.len; i++) {
            buf.len += Fmt(buf.TakeAvailable(), "%1%2", i ? ", " : "", args[i]->signature).len;
        }

        MarkError(call_pos, "Cannot call function typed '%1' with (%2) arguments", func_type->signature, buf);
        return false;
    }

    if (current_func) {
        current_func->impure |= !func || func->impure;
        current_func->side_effects |= !func || func->side_effects;
    }

    // Emit intrinsic or call
    if (!func) {
        Size offset = 1 + args_size;
        Emit(bk_Opcode::CallIndirect, -offset);

        stack[stack.len - 1] = { func_type->ret_type };
    } else if (func->mode == bk_FunctionInfo::Mode::Intrinsic) {
        EmitIntrinsic(func->name, call_pos, call_addr, args);
    } else if (func->mode == bk_FunctionInfo::Mode::Record) {
        stack.Append({ func_type->ret_type });
    } else {
        Emit(bk_Opcode::Call, func);

        if (valid && !func->impure) {
            FoldInstruction(args_size, func_type->ret_type);
        }
        show_errors &= func->valid;

        stack.Append({ func_type->ret_type });
    }

    return true;
}

void bk_Parser::EmitIntrinsic(const char *name, Size call_pos, Size call_addr, Span<const bk_TypeInfo *const> args)
{
    if (TestStr(name, "toFloat")) {
        if (args[0] == bk_IntType) {
            Emit(bk_Opcode::IntToFloat);
            FoldInstruction(1, bk_FloatType);
        }

        stack.Append({ bk_FloatType });
    } else if (TestStr(name, "toInt")) {
        if (args[0] == bk_FloatType) {
            Emit(bk_Opcode::FloatToInt);
            FoldInstruction(1, bk_IntType);
        }

        stack.Append({ bk_IntType });
    } else if (TestStr(name, "typeOf")) {
        // XXX: We can change the signature from typeOf(...) to typeOf(Any) after Any
        // is implemented, and remove this check.
        if (args.len != 1) [[unlikely]] {
            MarkError(call_pos, "Intrinsic function typeOf() takes one argument");
            return;
        }

        // typeOf() does not execute anything!
        TrimInstructions(call_addr);
        Emit(bk_Opcode::Push, { .primitive = bk_PrimitiveKind::Type }, args[0]);

        stack.Append({ bk_TypeType });
    } else if (TestStr(name, "iif")) {
        if (args.len != 3) [[unlikely]] {
            MarkError(call_pos, "Intrinsic function iif() takes three arguments");
            return;
        }
        if (args[1] != args[2]) [[unlikely]] {
            MarkError(call_pos, "Type mismatch between arguments 2 and 3");
            return;
        }

        Emit(bk_Opcode::InlineIf, args[1]->size);
        FoldInstruction(1 + args[1]->size * 2, args[1]);

        stack.Append({ args[1] });
    } else {
        RG_UNREACHABLE();
    }
}

void bk_Parser::EmitLoad(bk_VariableInfo *var)
{
    if (!var->type->size) {
        stack.Append({ var->type, var, false });
    } else if (var->constant) {
        bk_Instruction inst = (*var->ir)[var->ir_addr - 1];
        IR.Append(inst);

        stack.Append({ var->type, var, false });
    } else if (var->type->IsComposite()) {
        RG_ASSERT(var->offset >= 0);

        bk_Opcode code = var->local ? bk_Opcode::LeaLocal : bk_Opcode::Lea;
        Emit(code, var->offset);
        Emit(bk_Opcode::LoadIndirect, var->type->size);

        stack.Append({ var->type, var, true });
    } else {
        RG_ASSERT(var->offset >= 0);

        bk_Opcode code = var->local ? bk_Opcode::LoadLocal : bk_Opcode::Load;
        Emit(code, var->offset);

        stack.Append({ var->type, var, false });
    }

    if (current_func) {
        current_func->impure |= var->mut && !var->local;
    }
}

const bk_TypeInfo *bk_Parser::ParseType()
{
    Size type_pos = pos;

    // Parse type expression
    {
        const bk_TypeInfo *type = ParseExpression((int)ExpressionFlag::StopOperator).type;

        if (type != bk_TypeType) [[unlikely]] {
            MarkError(type_pos, "Expected a 'Type' expression, not '%1'", type->signature);
            return bk_NullType;
        }
    }

    if (IR[IR.len - 1].code != bk_Opcode::Push) [[unlikely]] {
        MarkError(type_pos, "Complex 'Type' expression cannot be resolved statically");
        return bk_NullType;
    }

    const bk_TypeInfo *type = IR[IR.len - 1].u2.type;
    TrimInstructions(IR.len - 1);

    return type;
}

void bk_Parser::FoldInstruction(Size count, const bk_TypeInfo *out_type)
{
    Size addr = IR.len - 1;

    // Make sure only constant data instructions are in use and skip them
    {
        Size remain = count;

        while (remain) {
            addr--;

            bk_Opcode code = IR[addr].code;

            if (code == bk_Opcode::SkipIfTrue ||
                    code == bk_Opcode::SkipIfFalse) {
                // Go on
            } else if (code == bk_Opcode::Push) {
                if (!--remain)
                    break;
            } else if (code == bk_Opcode::Fetch) {
                if (IR[addr].u1.i > remain)
                    return;
                remain -= IR[addr].u1.i;

                if (!remain)
                    break;
            } else {
                return;
            }

            if (addr <= 1) [[unlikely]]
                return;
        }
    }

    Emit(bk_Opcode::End, out_type->size);

    folder.frames.RemoveFrom(1);
    folder.frames[0].func = current_func;
    folder.frames[0].pc = addr;
    folder.stack.RemoveFrom(0);

    bool folded = folder.Run();

    if (folded) {
        TrimInstructions(addr);

        if (out_type->size == 1) {
            bk_PrimitiveValue value = folder.stack[folder.stack.len - 1];
            bk_PrimitiveKind primitive = out_type->primitive;

            Emit(bk_Opcode::Push, { .primitive = primitive }, value);
        } else if (out_type->size) {
            Size ptr = program->ro.len;

            program->ro.Append(folder.stack);

            Emit(bk_Opcode::Fetch, { .i = (int32_t)folder.stack.len }, ptr);
        }
    } else {
        IR.len--;
    }
}

void bk_Parser::DiscardResult(Size size)
{
    while (size > 0) {
        switch (IR[IR.len - 1].code) {
            case bk_Opcode::Push:
            case bk_Opcode::Lea:
            case bk_Opcode::LeaLocal:
            case bk_Opcode::LeaRel:
            case bk_Opcode::Load:
            case bk_Opcode::LoadLocal: {
                TrimInstructions(IR.len - 1);
                size--;
            } break;

            case bk_Opcode::Reserve: {
                Size operand = IR[IR.len - 1].u2.i;

                if (size >= operand) {
                    TrimInstructions(IR.len - 1);
                    size -= operand;
                } else {
                    EmitPop(size);
                    return;
                }
            } break;
            case bk_Opcode::Fetch: {
                Size operand = IR[IR.len - 1].u1.i;

                if (size >= operand) {
                    TrimInstructions(IR.len - 1);
                    size -= operand;
                } else {
                    EmitPop(size);
                    return;
                }
            } break;

            case bk_Opcode::StoreK:
            case bk_Opcode::StoreLocalK: {
                IR[IR.len - 1].code = (bk_Opcode)((int)IR[IR.len - 1].code - 1);
                size--;
            } break;

            case bk_Opcode::StoreIndirectK:
            case bk_Opcode::StoreRevK: {
                if (size >= IR[IR.len - 1].u2.i) {
                    IR[IR.len - 1].code = (bk_Opcode)((int)IR[IR.len - 1].code - 1);
                    size -= IR[IR.len - 1].u2.i;
                } else {
                    EmitPop(size);
                    return;
                }
            } break;

            case bk_Opcode::Call: {
                const bk_FunctionInfo *func = IR[IR.len - 1].u2.func;
                const bk_FunctionTypeInfo *func_type = func->type;

                if (!func->side_effects && !func_type->variadic && size >= func_type->ret_type->size) {
                    TrimInstructions(IR.len - 1);
                    size += func_type->params_size - func_type->ret_type->size;
                } else {
                    EmitPop(size);
                    return;
                }
            } break;

            default: {
                EmitPop(size);
                return;
            } break;
        }
    }
}

bool bk_Parser::CopyBigConstant(Size size)
{
    RG_ASSERT(size > 1);
    RG_ASSERT(size <= INT32_MAX);

    program->ro.Grow(size);

    Size addr = IR.len;

    for (Size offset = size; offset > 0;) {
        addr--;

        switch (IR[addr].code) {
            case bk_Opcode::Push: {
                offset--;
                program->ro.ptr[program->ro.len + offset].i = IR[addr].u2.i;
            } break;
            case bk_Opcode::Reserve: {
                if (IR[addr].u2.i > offset)
                    return false;

                offset -= IR[addr].u2.i;
                MemSet(program->ro.end() + offset, 0, IR[addr].u2.i * RG_SIZE(bk_PrimitiveValue));
            } break;
            case bk_Opcode::Fetch: {
                if (IR[addr].u1.i > offset)
                    return false;

                offset -= IR[addr].u1.i;
                MemCpy(program->ro.end() + offset, program->ro.ptr + IR[addr].u2.i, IR[addr].u1.i * RG_SIZE(bk_PrimitiveValue));
            } break;

            default: return false;
        }
    }

    TrimInstructions(addr);
    program->globals.Append({ bk_Opcode::Fetch, { .i = (int32_t)size }, { .i = program->ro.len } });
    program->ro.len += size;

    return true;
}

void bk_Parser::EmitPop(int64_t count)
{
    RG_ASSERT(count >= 0 || !valid);

    if (count) {
        Emit(bk_Opcode::Pop, count);
    }
}

void bk_Parser::EmitReturn(Size size)
{
    RG_ASSERT(current_func);

    // We support tail recursion elimination (TRE)
    if (IR[IR.len - 1].code == bk_Opcode::Call && IR[IR.len - 1].u2.func == current_func) {
        IR.len--;

        if (current_func->type->params_size == 1) {
            Emit(bk_Opcode::StoreLocal, 0);
        } else if (current_func->type->params_size > 1) {
            Emit(bk_Opcode::LeaLocal, 0);
            Emit(bk_Opcode::StoreRev, current_func->type->params_size);
        }
        EmitPop(*offset_ptr - current_func->type->params_size);
        Emit(bk_Opcode::Jump, -IR.len);

        current_func->tre = true;
    } else {
        Emit(bk_Opcode::Return, size);
    }
}

bk_VariableInfo *bk_Parser::CreateGlobal(const char *name, const bk_TypeInfo *type,
                                         Span<const bk_PrimitiveValue> values, bool module)
{
    RG_ASSERT(values.len <= INT32_MAX);

    bk_VariableInfo *var = program->variables.AppendDefault();

    var->name = InternString(name);
    var->type = type;
    var->mut = false;
    var->module = module;
    var->constant = true;
    var->ir = &program->globals;
    var->offset = -1;

    if (values.len > 1) {
        Size ptr = program->ro.len;

        program->ro.Append(values);

        Emit(bk_Opcode::Fetch, { .i = (int32_t)values.len }, ptr);
    } else if (values.len == 1) {
        program->globals.Append({ bk_Opcode::Push, { .primitive = type->primitive }, values[0] });
    }
    var->ir_addr = program->globals.len;

    return var;
}

bool bk_Parser::MapVariable(bk_VariableInfo *var, Size var_pos)
{
    bk_VariableInfo **ptr;
    bk_VariableInfo *it;
    {
        bool inserted;
        ptr = program->variables_map.TrySetDefault(var->name, &inserted);
        it = inserted ? nullptr : *ptr;
    }

    definitions_map.Set(var, var_pos);

    while (it && (int)it->local > (int)var->local) {
        RG_ASSERT(it != var);

        ptr = (bk_VariableInfo **)&it->shadow;
        it = (bk_VariableInfo *)it->shadow;
    }

    *ptr = var;
    var->shadow = it;

    bool duplicate = it && (var->local ? var->ir == it->ir : !it->local);

    if (duplicate) [[unlikely]] {
        Size it_pos = definitions_map.FindValue(it, -1);

        if (var_pos < it_pos) {
            std::swap(var_pos, it_pos);
            std::swap(var, it);
        }

        if (current_func && !current_func->ir.len) {
            MarkError(var_pos, "Parameter '%1' already exists", var->name);
            HintDefinition(it_pos, "Previous parameter '%1' is defined here", it->name);
        } else {
            MarkError(var_pos, "%1 '%2' cannot hide previous %3", GetVariableKind(var, true), var->name, GetVariableKind(it, false));
            HintDefinition(it_pos, "Previous %1 '%2' is defined here", GetVariableKind(it, false), it->name);
        }
    }

    return !duplicate;
}

const char *bk_Parser::GetVariableKind(const bk_VariableInfo *var, bool capitalize)
{
    if (var->module && var->type->primitive == bk_PrimitiveKind::Function) {
        return capitalize ? "Function" : "function";
    } else if (var->module && var->type->primitive == bk_PrimitiveKind::Type) {
        return capitalize ? "Type" : "type";
    } else {
        return capitalize ? "Variable" : "variable";
    }
}

void bk_Parser::DestroyVariables(Size first_idx)
{
    for (Size i = program->variables.count - 1; i >= first_idx; i--) {
        const bk_VariableInfo &var = program->variables[i];
        bk_VariableInfo **ptr = program->variables_map.Find(var.name);

        if (ptr) [[likely]] {
            if (*ptr == &var && !var.shadow) {
                program->variables_map.Remove(ptr);
            } else {
                 while (*ptr && *ptr != &var) {
                    ptr = (bk_VariableInfo **)&(*ptr)->shadow;
                }

                *ptr = (bk_VariableInfo *)var.shadow;
            }
        }

        poisoned_set.Remove(&var);
    }

    program->variables.RemoveFrom(first_idx);
}

template <typename T>
void bk_Parser::DestroyTypes(BucketArray<T> *types, Size first_idx)
{
    for (Size i = types->count - 1; i >= first_idx; i--) {
        const bk_TypeInfo &type = (*types)[i];
        const bk_TypeInfo **ptr = program->types_map.Find(type.signature);

        if (ptr && *ptr == &type) {
            program->types_map.Remove(ptr);
        }
    }
}

void bk_Parser::FixJumps(Size jump_addr, Size target_addr)
{
    while (jump_addr >= 0) {
        Size next_addr = IR[jump_addr].u2.i;
        IR[jump_addr].u2.i = target_addr - jump_addr;
        jump_addr = next_addr;
    }
}

void bk_Parser::TrimInstructions(Size trim_addr)
{
    Size min_addr = current_func ? 0 : prev_main_len;

    // Don't trim previously compiled code
    if (trim_addr < min_addr) [[unlikely]] {
        RG_ASSERT(!valid);
        return;
    }

    // Remove potential jump sources
    if (loop) {
        while (loop->break_addr >= trim_addr) {
            loop->break_addr = IR[loop->break_addr].u2.i;
        }
        while (loop->continue_addr >= trim_addr) {
            loop->continue_addr = IR[loop->continue_addr].u2.i;
        }
    }

    // Adjust IR-line map
    if (src->lines.len > 0 && src->lines[src->lines.len - 1].addr > trim_addr) {
        bk_SourceMap::Line line = src->lines[src->lines.len - 1];
        line.addr = trim_addr;

        do {
            src->lines.len--;
        } while (src->lines.len > 0 && src->lines[src->lines.len - 1].addr >= trim_addr);

        src->lines.Append(line);
    }

    IR.RemoveFrom(trim_addr);
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
    if (pos >= tokens.len) [[unlikely]] {
        if (valid) {
            if (out_report) {
                out_report->unexpected_eof = true;
            }
            MarkError(pos, "Unexpected end of file, expected '%1'", bk_TokenKindNames[(int)kind]);
        }

        return false;
    }

    if (tokens[pos].kind != kind) [[unlikely]] {
        MarkError(pos, "Unexpected token '%1', expected '%2'",
                  bk_TokenKindNames[(int)tokens[pos].kind], bk_TokenKindNames[(int)kind]);
        return false;
    }

    pos++;
    return true;
}

const char *bk_Parser::ConsumeIdentifier()
{
    if (ConsumeToken(bk_TokenKind::Identifier)) [[likely]] {
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
    if (pos >= tokens.len) [[unlikely]] {
        if (valid) {
            if (out_report) {
                out_report->unexpected_eof = true;
            }
            MarkError(pos, "Unexpected end of file, expected end of statement");
        }

        return false;
    }

    if (tokens[pos].kind != bk_TokenKind::EndOfLine &&
            tokens[pos].kind != bk_TokenKind::Semicolon) [[unlikely]] {
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

        if (pos < tokens.len) [[likely]] {
            src->lines.Append({ IR.len, tokens[pos].line });
        }

        return true;
    } else {
        return false;
    }
}

const char *bk_Parser::InternString(const char *str)
{
    bool inserted;
    const char **ptr = strings.TrySet(str, &inserted);

    if (inserted) {
        *ptr = DuplicateString(str, &program->str_alloc).ptr;
    }

    return *ptr;
}

bool bk_Parser::RecurseInc()
{
    bool accept = ++recursion < 64;
    return accept;
}

void bk_Parser::RecurseDec()
{
    recursion--;
}

#undef IR

}
