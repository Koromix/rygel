// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "src/core/libcc/libcc.hh"

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

    struct bk_FunctionTypeInfo *AsFunctionType()
    {
        RG_ASSERT(primitive == bk_PrimitiveKind::Function);
        return (bk_FunctionTypeInfo *)this;
    }
    const struct bk_FunctionTypeInfo *AsFunctionType() const
    {
        RG_ASSERT(primitive == bk_PrimitiveKind::Function);
        return (const bk_FunctionTypeInfo *)this;
    }
    struct bk_ArrayTypeInfo *AsArrayType()
    {
        RG_ASSERT(primitive == bk_PrimitiveKind::Array);
        return (bk_ArrayTypeInfo *)this;
    }
    const struct bk_ArrayTypeInfo *AsArrayType() const
    {
        RG_ASSERT(primitive == bk_PrimitiveKind::Array);
        return (const bk_ArrayTypeInfo *)this;
    }
    struct bk_RecordTypeInfo *AsRecordType()
    {
        RG_ASSERT(primitive == bk_PrimitiveKind::Record);
        return (bk_RecordTypeInfo *)this;
    }
    const struct bk_RecordTypeInfo *AsRecordType() const
    {
        RG_ASSERT(primitive == bk_PrimitiveKind::Record);
        return (const bk_RecordTypeInfo *)this;
    }
    struct bk_EnumTypeInfo *AsEnumType()
    {
        RG_ASSERT(primitive == bk_PrimitiveKind::Enum);
        return (bk_EnumTypeInfo *)this;
    }
    const struct bk_EnumTypeInfo *AsEnumType() const
    {
        RG_ASSERT(primitive == bk_PrimitiveKind::Enum);
        return (const bk_EnumTypeInfo *)this;
    }

    RG_HASHTABLE_HANDLER(bk_TypeInfo, signature);
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

    LocalArray<Member, RG_LEN(bk_FunctionTypeInfo::params.data)> members;
    const bk_FunctionInfo *func;
};
struct bk_EnumTypeInfo: public bk_TypeInfo {
    struct Label {
        const char *name;
        int64_t value;

        RG_HASHTABLE_HANDLER(Label, name);
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
    #define OPCODE(Code) RG_STRINGIFY(Code),
    #include "opcodes.inc"
};

struct bk_Instruction {
    bk_Opcode code;
    bk_PrimitiveKind primitive; // Only set for Push
    bk_PrimitiveValue u;
};

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
    LocalArray<Parameter, RG_LEN(bk_FunctionTypeInfo::params.data)> params;

    Mode mode;
    std::function<bk_NativeFunction> native;

    HeapArray<bk_Instruction> ir;
    bk_SourceMap src;
    bool tre;

    bool valid;
    bool impure;
    bool side_effects;

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
    bool constant;

    Scope scope;
    Size offset; // Stack

    // Only set for globals and locals (not parameters, loop iterators, etc.)
    Size ready_addr;

    const bk_VariableInfo *shadow;

    RG_HASHTABLE_HANDLER(bk_VariableInfo, name);
};

struct bk_CallFrame {
    const bk_FunctionInfo *func; // Can be NULL
    Size pc;
    Size bp;
    bool direct;
};

struct bk_Program {
    HeapArray<bk_Instruction> ir;
    HeapArray<bk_SourceMap> sources;

    BucketArray<bk_FunctionTypeInfo> function_types;
    BucketArray<bk_ArrayTypeInfo> array_types;
    BucketArray<bk_RecordTypeInfo> record_types;
    BucketArray<bk_EnumTypeInfo> enum_types;
    BucketArray<bk_TypeInfo> bare_types;

    BucketArray<bk_FunctionInfo> functions;
    BucketArray<bk_VariableInfo> variables;

    HashTable<const char *, const bk_TypeInfo *> types_map;
    HashTable<const char *, bk_FunctionInfo *> functions_map;
    HashTable<const char *, bk_VariableInfo *> variables_map;

    BlockAllocator str_alloc;

    const char *LocateInstruction(const bk_FunctionInfo *func, Size pc, int32_t *out_line = nullptr) const;
};

}
