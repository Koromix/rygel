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

// XXX: Support native calling conventions to provide seamless integration
typedef Value NativeFunction(VirtualMachine *vm, Span<const Value> args);

struct FunctionInfo {
    enum class Mode {
        Intrinsic,
        Native,
        Blik
    };

    struct Parameter {
        const char *name;
        const TypeInfo *type;
        bool mut;
    };

    const char *name;
    const char *signature;

    Mode mode;
    std::function<NativeFunction> native;

    LocalArray<Parameter, 16> params;
    bool variadic;
    const TypeInfo *ret_type;

    Size addr; // IR
    bool tre;

    // Linked list
    FunctionInfo *overload_prev;
    FunctionInfo *overload_next;

    // Used to prevent dangerous forward calls (if relevant globals are not defined yet)
    Size earliest_call_pos;
    Size earliest_call_addr;

    RG_HASHTABLE_HANDLER(FunctionInfo, name);
};

struct VariableInfo {
    const char *name;
    const TypeInfo *type;
    bool mut;

    bool global;
    Size offset; // Stack
    Size ready_addr; // IR (for globals)

    const VariableInfo *shadow;

    RG_HASHTABLE_HANDLER(VariableInfo, name);
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
                   // Jump, BranchIfTrue, BranchIfFalse, Return
        double d; // PushFloat
        const char *str; // PushString
        const TypeInfo *type; // PushType
        const FunctionInfo *func; // Call
    } u;
};

struct SourceInfo {
    struct Line {
        Size addr;
        int32_t line;
    };

    const char *filename;
    HeapArray<Line> lines;
};

struct CallFrame {
    const FunctionInfo *func; // Can be NULL
    Size pc;
    Size bp;
};

struct Program {
    HeapArray<Instruction> ir;
    HeapArray<SourceInfo> sources;

    BucketArray<TypeInfo> types;
    BucketArray<FunctionInfo> functions;
    BucketArray<VariableInfo> variables;
    HashTable<const char *, TypeInfo *> types_map;
    HashTable<const char *, FunctionInfo *> functions_map;
    HashTable<const char *, VariableInfo *> variables_map;

    Size end_stack_len;

    BlockAllocator str_alloc;

    const char *LocateInstruction(Size pc, int32_t *out_line = nullptr) const;
};

}
