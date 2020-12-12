// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"

namespace RG {

struct bk_TypeInfo;
struct bk_FunctionInfo;
class bk_VirtualMachine;

enum class bk_PrimitiveKind {
    Null,
    Boolean,
    Integer,
    Float,
    String,
    Type,
    Function,
    Array
};
static const char *const bk_PrimitiveKindNames[] = {
    "Null",
    "Boolean",
    "Integer",
    "Float",
    "String",
    "Type",
    "Function",
    "Array"
};

union bk_PrimitiveValue {
    bool b;
    int64_t i;
    double d;
    const char *str;
    const bk_TypeInfo *type;
    const bk_FunctionInfo *func;
};

struct bk_TypeInfo {
    const char *signature;
    bk_PrimitiveKind primitive;

    bool PassByReference() const { return primitive == bk_PrimitiveKind::Array; }

    const struct bk_FunctionTypeInfo *AsFunctionType() const
    {
        RG_ASSERT(primitive == bk_PrimitiveKind::Function);
        return (const bk_FunctionTypeInfo *)this;
    }
    const struct bk_ArrayTypeInfo *AsArrayType() const
    {
        RG_ASSERT(primitive == bk_PrimitiveKind::Array);
        return (const bk_ArrayTypeInfo *)this;
    }

    RG_HASHTABLE_HANDLER(bk_TypeInfo, signature);
};
struct bk_FunctionTypeInfo: public bk_TypeInfo {
    LocalArray<const bk_TypeInfo *, 16> params;
    bool variadic;
    const bk_TypeInfo *ret_type;
};
struct bk_ArrayTypeInfo: public bk_TypeInfo {
    const bk_TypeInfo *unit_type;
    Size unit_size;
    Size len;
};

extern Span<const bk_TypeInfo> bk_BaseTypes;
extern const bk_TypeInfo *bk_NullType;
extern const bk_TypeInfo *bk_BoolType;
extern const bk_TypeInfo *bk_IntType;
extern const bk_TypeInfo *bk_FloatType;
extern const bk_TypeInfo *bk_StringType;
extern const bk_TypeInfo *bk_TypeType;

// XXX: Support native calling conventions to provide seamless integration
typedef bk_PrimitiveValue bk_NativeFunction(bk_VirtualMachine *vm, Span<const bk_PrimitiveValue> args);

struct bk_FunctionInfo {
    enum class Mode {
        Intrinsic,
        Native,
        blikk
    };

    struct Parameter {
        const char *name;
        const bk_TypeInfo *type;
        bool mut;
    };

    const char *name;
    const char *prototype;
    const bk_FunctionTypeInfo *type;
    LocalArray<Parameter, RG_LEN(bk_FunctionTypeInfo::params.data)> params;

    Mode mode;
    std::function<bk_NativeFunction> native;

    Size addr; // IR
    bool tre;

    // Linked list
    bk_FunctionInfo *overload_prev;
    bk_FunctionInfo *overload_next;

    // Used to prevent dangerous forward calls (if relevant globals are not defined yet)
    Size earliest_ref_pos;
    Size earliest_ref_addr;

    RG_HASHTABLE_HANDLER(bk_FunctionInfo, name);
};

struct bk_VariableInfo {
    enum class Scope {
        Module,
        Global,
        Local
    };

    const char *name;
    const bk_TypeInfo *type;
    bool mut;

    Scope scope;
    Size offset; // Stack
    Size ready_addr; // Only set for globals and locals (not parameters, loop iterators, etc.)

    const bk_VariableInfo *shadow;

    RG_HASHTABLE_HANDLER(bk_VariableInfo, name);
};

enum class bk_Opcode {
    #define OPCODE(Code) Code,
    #include "opcodes.inc"
};
static const char *const bk_OpcodeNames[] = {
    #define OPCODE(Code) RG_STRINGIFY(Code),
    #include "opcodes.inc"
};

struct bk_Instruction {
    bk_Opcode code;
    bk_PrimitiveKind primitive; // Only set for Push/Load/Store/Copy
    bk_PrimitiveValue u;
};

struct bk_SourceInfo {
    struct Line {
        Size addr;
        int32_t line;
    };

    const char *filename;
    HeapArray<Line> lines;
};

struct bk_CallFrame {
    const bk_FunctionInfo *func; // Can be NULL
    Size pc;
    Size bp;
    bool direct;
};

struct bk_Program {
    HeapArray<bk_Instruction> ir;
    HeapArray<bk_SourceInfo> sources;

    BucketArray<bk_FunctionTypeInfo> function_types;
    BucketArray<bk_ArrayTypeInfo> array_types;
    BucketArray<bk_FunctionInfo> functions;
    BucketArray<bk_VariableInfo> variables;
    HashTable<const char *, const bk_TypeInfo *> types_map;
    HashTable<const char *, bk_FunctionInfo *> functions_map;
    HashTable<const char *, bk_VariableInfo *> variables_map;

    BlockAllocator str_alloc;

    const char *LocateInstruction(Size pc, int32_t *out_line = nullptr) const;
};

}
