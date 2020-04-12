// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPCODE
    #error Please define OPCODE() before including opcodes.inc
#endif

OPCODE(PushBool)
OPCODE(PushInteger)
OPCODE(PushDouble)
OPCODE(PushString)
OPCODE(Pop)

OPCODE(Add)
OPCODE(Substract)
OPCODE(Multiply)
OPCODE(Divide)
OPCODE(Modulo)
OPCODE(Equal)
OPCODE(NotEqual)
OPCODE(Greater)
OPCODE(GreaterOrEqual)
OPCODE(Less)
OPCODE(LessOrEqual)
OPCODE(And)
OPCODE(Or)
OPCODE(Xor)
OPCODE(Not)
OPCODE(LeftShift)
OPCODE(RightShift)

OPCODE(AddDouble)
OPCODE(SubstractDouble)
OPCODE(MultiplyDouble)
OPCODE(DivideDouble)
OPCODE(EqualDouble)
OPCODE(NotEqualDouble)
OPCODE(GreaterDouble)
OPCODE(GreaterOrEqualDouble)
OPCODE(LessDouble)
OPCODE(LessOrEqualDouble)

OPCODE(LogicNot)
OPCODE(LogicAnd)
OPCODE(LogicOr)
OPCODE(LogicXor)

OPCODE(Jump)
OPCODE(BranchIfTrue)
OPCODE(BranchIfFalse)

#undef OPCODE
