// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"

namespace RG {

class bk_VirtualMachine;

// Keep ordering in sync with Push* opcodes!
enum class bk_PrimitiveType {
    Null,
    Bool,
    Int,
    Float,
    Type
};
static const char *const bk_PrimitiveTypeNames[] = {
    "Null",
    "Bool",
    "Int",
    "Float",
    "Type"
};

struct bk_TypeInfo {
    const char *signature;
    bk_PrimitiveType primitive;

    RG_HASHTABLE_HANDLER(bk_TypeInfo, signature);
};

union bk_Value {
    bool b;
    int64_t i;
    double d;
    const bk_TypeInfo *type;
};

// XXX: Support native calling conventions to provide seamless integration
typedef bk_Value bk_NativeFunction(bk_VirtualMachine *vm, Span<const bk_Value> args);

struct bk_FunctionInfo {
    enum class Mode {
        Intrinsic,
        Native,
        Blik
    };

    struct Parameter {
        const char *name;
        const bk_TypeInfo *type;
        bool mut;
    };

    const char *name;
    const char *signature;

    Mode mode;
    std::function<bk_NativeFunction> native;

    LocalArray<Parameter, 16> params;
    bool variadic;
    const bk_TypeInfo *ret_type;

    Size addr; // IR
    bool tre;

    // Linked list
    bk_FunctionInfo *overload_prev;
    bk_FunctionInfo *overload_next;

    // Used to prevent dangerous forward calls (if relevant globals are not defined yet)
    Size earliest_call_pos;
    Size earliest_call_addr;

    RG_HASHTABLE_HANDLER(bk_FunctionInfo, name);
};

struct bk_VariableInfo {
    const char *name;
    const bk_TypeInfo *type;
    bool mut;

    bool global;
    Size offset; // Stack
    Size ready_addr; // IR (for globals)

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
    union {
        bool b; // PushBool, Exit
        int64_t i; // PushInteger, Pop,
                   // StoreBool, StoreInt, StoreFloat, StoreString,
                   // LoadBool, LoadInt, LoadFloat, LoadString,
                   // Jump, BranchIfTrue, BranchIfFalse, Return
        double d; // PushFloat
        const char *str; // PushString
        const bk_TypeInfo *type; // PushType
        const bk_FunctionInfo *func; // Call
    } u;
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
};

struct bk_Program {
    HeapArray<bk_Instruction> ir;
    HeapArray<bk_SourceInfo> sources;

    BucketArray<bk_TypeInfo> types;
    BucketArray<bk_FunctionInfo> functions;
    BucketArray<bk_VariableInfo> variables;
    HashTable<const char *, bk_TypeInfo *> types_map;
    HashTable<const char *, bk_FunctionInfo *> functions_map;
    HashTable<const char *, bk_VariableInfo *> variables_map;

    BlockAllocator str_alloc;

    const char *LocateInstruction(Size pc, int32_t *out_line = nullptr) const;
};

}
