// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"

namespace RG {

struct Token;

enum class Type {
    Bool,
    Integer,
    Double,
    String
};
static const char *const TypeNames[] = {
    "Bool",
    "Integer",
    "Double",
    "String"
};

struct VariableInfo {
    const char *name;
    Type type;
    Size offset;

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
        bool b; // PushBool
        int64_t i; // PushInteger, Pop,
                   // StoreBool, StoreInt, StoreDouble, StoreString,
                   // LoadBool, LoadInt, LoadDouble, LoadString,
                   // Jump, BranchIfTrue, BranchIfFalse
        double d; // PushDouble
        const char *str; // PushString
        Type type; // Print
    } u;
};

struct Program {
    HeapArray<Instruction> ir;

    HeapArray<VariableInfo> variables;
    HashTable<const char *, VariableInfo> variables_map;
};

bool Parse(Span<const Token> tokens, const char *filename, Program *out_program);

}
