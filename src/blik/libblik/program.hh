// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"

namespace RG {

class VirtualMachine;

// Keep ordering in sync with Push* opcodes!
enum class PrimitiveType {
    Null,
    Bool,
    Int,
    Float,
    String,
    Type
};
static const char *const PrimitiveTypeNames[] = {
    "Null",
    "Bool",
    "Int",
    "Float",
    "String",
    "Type"
};

struct TypeInfo {
    const char *signature;
    PrimitiveType primitive;

    RG_HASHTABLE_HANDLER(TypeInfo, signature);
};

union Value {
    bool b;
    int64_t i;
    double d;
    const char *str;
    const TypeInfo *type;
};

enum class Opcode {
    #define OPCODE(Code) Code,
    #include "opcodes.inc"
};
static const char *const OpcodeNames[] = {
    #define OPCODE(Code) RG_STRINGIFY(Code),
    #include "opcodes.inc"
};

struct Instruction {
    Opcode code;
    union {
        bool b; // PushBool, Exit
        int64_t i; // PushInteger, Pop,
                   // StoreBool, StoreInt, StoreFloat, StoreString,
                   // LoadBool, LoadInt, LoadFloat, LoadString,
                   // Jump, BranchIfTrue, BranchIfFalse,
                   // Call, Return, Exit
        double d; // PushFloat
        const char *str; // PushString
        const TypeInfo *type; // PushType

        uint64_t payload; // Invoke
    } u;
};

struct SourceInfo {
    struct LineInfo {
        Size first_idx;
        int32_t line;
    };

    const char *filename;
    HeapArray<LineInfo> lines;
};

// XXX: Support native calling conventions to provide seamless integration
typedef Value NativeFunction(VirtualMachine *vm, Span<const Value> args);

struct FunctionInfo {
    struct Parameter {
        const char *name;
        const TypeInfo *type;
    };

    const char *name;
    const char *signature;

    bool intrinsic;
    std::function<NativeFunction> native;

    LocalArray<Parameter, 16> params;
    bool variadic;
    const TypeInfo *ret_type;

    Size inst_idx; // IR
    bool tre;

    // Linked list
    FunctionInfo *overload_prev;
    FunctionInfo *overload_next;

    // Used to prevent dangerous forward calls (if relevant globals are not defined yet)
    Size earliest_call_pos;
    Size earliest_call_idx;

    RG_HASHTABLE_HANDLER(FunctionInfo, name);
};

struct VariableInfo {
    const char *name;
    const TypeInfo *type;
    bool mut;

    bool global;
    Size offset; // Stack
    Size defined_idx; // IR

    const VariableInfo *shadow;

    RG_HASHTABLE_HANDLER(VariableInfo, name);
};

struct Program {
    HeapArray<Instruction> ir;
    HeapArray<SourceInfo> sources;

    BucketArray<TypeInfo> types;
    HashTable<const char *, TypeInfo *> types_map;

    BucketArray<FunctionInfo> functions;
    HashTable<const char *, FunctionInfo *> functions_map;

    BucketArray<VariableInfo> variables;
    HashTable<const char *, VariableInfo *> variables_map;

    Size end_stack_len;

    BlockAllocator str_alloc;
};

}
