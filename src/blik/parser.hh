// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"

namespace RG {

struct Token;

enum class Opcode {
    #define OPCODE(Code) Code,
    #include "opcodes.inc"
};
static const char *const OpcodeNames[] = {
    #define OPCODE(Code) RG_STRINGIFY(Code),
    #include "opcodes.inc"
};

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

struct Instruction {
    Opcode code;
    union {
        bool b; // PushBool
        int64_t i; // PushInteger, StoreBool, StoreInt, StoreDouble, StoreString,
                   // LoadBool, LoadInt, LoadDouble, LoadString,
                   // Jump, BranchIfTrue, BranchIfFalse
        double d; // PushDouble
        const char *str; // PushString
    } u;
};

bool Parse(Span<const Token> tokens, const char *filename, HeapArray<Instruction> *out_ir);

}
