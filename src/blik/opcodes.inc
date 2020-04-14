// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPCODE
    #error Please define OPCODE() before including opcodes.inc
#endif

OPCODE(PushBool)
OPCODE(PushInt)
OPCODE(PushDouble)
OPCODE(PushString)
OPCODE(Pop)

OPCODE(StoreBool)
OPCODE(StoreInt)
OPCODE(StoreDouble)
OPCODE(StoreString)
OPCODE(LoadBool)
OPCODE(LoadInt)
OPCODE(LoadDouble)
OPCODE(LoadString)

OPCODE(NegateInt)
OPCODE(AddInt)
OPCODE(SubstractInt)
OPCODE(MultiplyInt)
OPCODE(DivideInt)
OPCODE(ModuloInt)
OPCODE(EqualInt)
OPCODE(NotEqualInt)
OPCODE(GreaterInt)
OPCODE(GreaterOrEqualInt)
OPCODE(LessInt)
OPCODE(LessOrEqualInt)
OPCODE(AndInt)
OPCODE(OrInt)
OPCODE(XorInt)
OPCODE(NotInt)
OPCODE(LeftShiftInt)
OPCODE(RightShiftInt)

OPCODE(NegateDouble)
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

OPCODE(EqualBool)
OPCODE(NotEqualBool)
OPCODE(NotBool)
OPCODE(AndBool)
OPCODE(OrBool)
OPCODE(XorBool)

OPCODE(Jump)
OPCODE(BranchIfTrue)
OPCODE(BranchIfFalse)
OPCODE(SkipIfTrue)
OPCODE(SkipIfFalse)

#undef OPCODE
