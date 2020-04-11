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

enum class Opcode {
    PushBool,
    PushInteger,
    PushDouble,
    PushString,
    Pop,

    Add,
    Substract,
    Multiply,
    Divide,
    Modulo,
    Equal,
    NotEqual,
    Greater,
    GreaterOrEqual,
    Less,
    LessOrEqual,

    AddDouble,
    SubstractDouble,
    MultiplyDouble,
    DivideDouble,
    EqualDouble,
    NotEqualDouble,
    GreaterDouble,
    GreaterOrEqualDouble,
    LessDouble,
    LessOrEqualDouble,

    LogicNot,
    LogicAnd,
    LogicOr,

    Jump,
    BranchIfTrue,
    BranchIfFalse
};

struct Instruction {
    Opcode code;
    union {
        bool b; // PushBool
        uint64_t i; // PushInteger
        double d; // PushDouble
        const char *str; // PushString
        Size jump; // Jump, BranchIfTrue, BranchIfFalse
    } u;

    Instruction() {}
    Instruction(Opcode code) : code(code) {}
    Instruction(Opcode code, bool b) : code(code) { u.b = b; }
    Instruction(Opcode code, uint64_t i) : code(code) { u.i = i; }
    Instruction(Opcode code, double d) : code(code) { u.d = d; }
    Instruction(Opcode code, const char *str) : code(code) { u.str = str; }
    Instruction(Opcode code, Size jump) : code(code) { u.jump = jump; }
};

bool Parse(Span<const Token> tokens, HeapArray<Instruction> *out_ir);

}
