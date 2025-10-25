// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"

namespace K {

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
    Array,
    Record,
    Enum,
    Opaque
};
static const char *const bk_PrimitiveKindNames[] = {
    "Null",
    "Boolean",
    "Integer",
    "Float",
    "String",
    "Type",
    "Function",
    "Array",
    "Record",
    "Enum",
    "Opaque"
};

union bk_PrimitiveValue {
    bool b;
    int64_t i;
    double d;
    const char *str;
    const bk_TypeInfo *type;
    const bk_FunctionInfo *func;
    void *opaque;
};

struct bk_TypeInfo {
    const char *signature;
    bk_PrimitiveKind primitive;
    bool init0;
    Size size;

    // Reference types will come later (maybe)
    bool PassByReference() const { return false; }
    bool IsComposite() const { return primitive == bk_PrimitiveKind::Array ||
                                      primitive == bk_PrimitiveKind::Record; }

    struct bk_FunctionTypeInfo *AsFunctionType()
    {
        K_ASSERT(primitive == bk_PrimitiveKind::Function);
        return (bk_FunctionTypeInfo *)this;
    }
    const struct bk_FunctionTypeInfo *AsFunctionType() const
    {
        K_ASSERT(primitive == bk_PrimitiveKind::Function);
        return (const bk_FunctionTypeInfo *)this;
    }
    struct bk_ArrayTypeInfo *AsArrayType()
    {
        K_ASSERT(primitive == bk_PrimitiveKind::Array);
        return (bk_ArrayTypeInfo *)this;
    }
    const struct bk_ArrayTypeInfo *AsArrayType() const
    {
        K_ASSERT(primitive == bk_PrimitiveKind::Array);
        return (const bk_ArrayTypeInfo *)this;
    }
    struct bk_RecordTypeInfo *AsRecordType()
    {
        K_ASSERT(primitive == bk_PrimitiveKind::Record);
        return (bk_RecordTypeInfo *)this;
    }
    const struct bk_RecordTypeInfo *AsRecordType() const
    {
        K_ASSERT(primitive == bk_PrimitiveKind::Record);
        return (const bk_RecordTypeInfo *)this;
    }
    struct bk_EnumTypeInfo *AsEnumType()
    {
        K_ASSERT(primitive == bk_PrimitiveKind::Enum);
        return (bk_EnumTypeInfo *)this;
    }
    const struct bk_EnumTypeInfo *AsEnumType() const
    {
        K_ASSERT(primitive == bk_PrimitiveKind::Enum);
        return (const bk_EnumTypeInfo *)this;
    }

    K_HASHTABLE_HANDLER(bk_TypeInfo, signature);
};
struct bk_FunctionTypeInfo: public bk_TypeInfo {
    LocalArray<const bk_TypeInfo *, 16> params;
    Size params_size;
    bool variadic;
    const bk_TypeInfo *ret_type;
};
struct bk_ArrayTypeInfo: public bk_TypeInfo {
    const bk_TypeInfo *unit_type;
    Size len;
};
struct bk_RecordTypeInfo: public bk_TypeInfo {
    struct Member {
        const char *name;
        const bk_TypeInfo *type;
        Size offset;
    };

    LocalArray<Member, K_LEN(bk_FunctionTypeInfo::params.data)> members;
    const bk_FunctionInfo *func;
};
struct bk_EnumTypeInfo: public bk_TypeInfo {
    struct Label {
        const char *name;
        int64_t value;

        K_HASHTABLE_HANDLER(Label, name);
    };

    HeapArray<Label> labels;
    HashTable<const char *, const Label *> labels_map;
};

extern Span<const bk_TypeInfo> bk_BaseTypes;
extern const bk_TypeInfo *bk_NullType;
extern const bk_TypeInfo *bk_BoolType;
extern const bk_TypeInfo *bk_IntType;
extern const bk_TypeInfo *bk_FloatType;
extern const bk_TypeInfo *bk_StringType;
extern const bk_TypeInfo *bk_TypeType;

enum class bk_Opcode {
    #define OPCODE(Code) Code,
    #include "opcodes.inc"
};
static const char *const bk_OpcodeNames[] = {
    #define OPCODE(Code) K_STRINGIFY(Code),
    #include "opcodes.inc"
};

struct bk_Instruction {
    bk_Opcode code;

    union {
        bk_PrimitiveKind primitive;
        int32_t i;
    } u1;
    bk_PrimitiveValue u2;
};
static_assert(K_SIZE(bk_Instruction) == 16);

struct bk_SourceMap {
    struct Line {
        Size addr;
        int32_t line;
    };

    const char *filename;
    HeapArray<Line> lines;
};

typedef void bk_NativeFunction(bk_VirtualMachine *vm, Span<const bk_PrimitiveValue> args, Span<bk_PrimitiveValue> ret);

enum class bk_FunctionFlag {
    Pure = 1 << 0,
    NoSideEffect = 1 << 1 // Pure implies NoSideEffect
};

struct bk_FunctionInfo {
    enum class Mode {
        Intrinsic,
        Native,
        Blikk,
        Record
    };

    struct Parameter {
        const char *name;
        const bk_TypeInfo *type;
        bool mut;
    };

    const char *name;
    const char *prototype;
    const bk_FunctionTypeInfo *type;
    LocalArray<Parameter, K_LEN(bk_FunctionTypeInfo::params.data)> params;

    Mode mode;
    std::function<bk_NativeFunction> native; // Native only
    HeapArray<bk_Instruction> ir; // Blikk only
    bk_SourceMap src; // Blikk only

    bool tre;
    bool valid;
    bool impure;
    bool side_effects;

    // Linked list
    bk_FunctionInfo *overload_prev;
    bk_FunctionInfo *overload_next;

    K_HASHTABLE_HANDLER(bk_FunctionInfo, name);
};

struct bk_VariableInfo {
    const char *name;
    const bk_TypeInfo *type;

    bool mut;
    bool module;
    bool local;
    bool constant;

    const HeapArray<bk_Instruction> *ir;
    Size ir_addr; // Only set for globals and locals (not parameters, loop iterators, etc.)
    Size offset; // Stack

    const bk_VariableInfo *shadow;

    K_HASHTABLE_HANDLER(bk_VariableInfo, name);
};

struct bk_CallFrame {
    const bk_FunctionInfo *func; // Can be NULL
    Size pc;
    Size bp;
    bool direct;
};

struct bk_Program {
    HeapArray<bk_Instruction> globals;
    HeapArray<bk_Instruction> main;
    HeapArray<bk_SourceMap> sources;

    BucketArray<bk_FunctionTypeInfo> function_types;
    BucketArray<bk_ArrayTypeInfo> array_types;
    BucketArray<bk_RecordTypeInfo> record_types;
    BucketArray<bk_EnumTypeInfo> enum_types;
    BucketArray<bk_TypeInfo> bare_types;

    BucketArray<bk_FunctionInfo> functions;
    BucketArray<bk_VariableInfo> variables;
    HeapArray<bk_PrimitiveValue> ro;

    HashTable<const char *, const bk_TypeInfo *> types_map;
    HashTable<const char *, bk_FunctionInfo *> functions_map;
    HashTable<const char *, bk_VariableInfo *> variables_map;

    BlockAllocator str_alloc;

    const char *LocateInstruction(const bk_FunctionInfo *func, Size pc, int32_t *out_line = nullptr) const;
};

}
