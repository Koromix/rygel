// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"

namespace RG {

class VirtualMachine;

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

        uint64_t payload; // CallNative, Print
    } u;
};

struct SourceInfo {
    struct LineInfo {
        int32_t line;
        Size first_idx;
    };

    const char *filename;
    HeapArray<LineInfo> lines;
};

enum class Type {
    Null,
    Bool,
    Int,
    Float,
    String
};
static const char *const TypeNames[] = {
    "Null",
    "Bool",
    "Int",
    "Float",
    "String"
};

struct VariableInfo {
    const char *name;
    Type type;
    bool global;
    bool readonly;
    bool poisoned;
    bool implicit;
    const VariableInfo *shadow;

    Size offset;

    Size defined_pos; // Token
    Size defined_idx; // IR

    RG_HASHTABLE_HANDLER(VariableInfo, name);
};

union Value {
    bool b;
    int64_t i;
    double d;
    const char *str;
};

// XXX: Support native calling conventions to provide seamless integration
typedef Value NativeFunction(VirtualMachine *vm, Span<const Value> args);

struct FunctionInfo {
    struct Parameter {
        const char *name;
        Type type;
    };

    const char *name;
    const char *signature;

    bool intrinsic;

    LocalArray<Parameter, 16> params;
    bool variadic;
    Size ret_pop;
    Type ret_type;

    Size defined_pos; // Token
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

struct Program {
    HeapArray<Instruction> ir;
    HeapArray<SourceInfo> sources;

    BucketArray<FunctionInfo> functions;
    HashTable<const char *, FunctionInfo *> functions_map;
    BucketArray<VariableInfo> variables;
    HashTable<const char *, VariableInfo *> variables_map;

    BlockAllocator str_alloc;
};

}
