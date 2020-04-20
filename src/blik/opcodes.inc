// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPCODE
    #error Please define OPCODE() before including opcodes.inc
#endif

OPCODE(PushNull)
OPCODE(PushBool)
OPCODE(PushInt)
OPCODE(PushFloat)
OPCODE(PushString)
OPCODE(Pop)
OPCODE(Duplicate)

OPCODE(LoadGlobalBool)
OPCODE(LoadGlobalInt)
OPCODE(LoadGlobalFloat)
OPCODE(LoadGlobalString)
OPCODE(StoreGlobalBool)
OPCODE(StoreGlobalInt)
OPCODE(StoreGlobalFloat)
OPCODE(StoreGlobalString)

OPCODE(LoadLocalBool)
OPCODE(LoadLocalInt)
OPCODE(LoadLocalFloat)
OPCODE(LoadLocalString)
OPCODE(StoreLocalBool)
OPCODE(StoreLocalInt)
OPCODE(StoreLocalFloat)
OPCODE(StoreLocalString)

OPCODE(NegateInt)
OPCODE(AddInt)
OPCODE(SubstractInt)
OPCODE(MultiplyInt)
OPCODE(DivideInt)
OPCODE(ModuloInt)
OPCODE(EqualInt)
OPCODE(NotEqualInt)
OPCODE(GreaterThanInt)
OPCODE(GreaterOrEqualInt)
OPCODE(LessThanInt)
OPCODE(LessOrEqualInt)
OPCODE(AndInt)
OPCODE(OrInt)
OPCODE(XorInt)
OPCODE(NotInt)
OPCODE(LeftShiftInt)
OPCODE(RightShiftInt)

OPCODE(NegateFloat)
OPCODE(AddFloat)
OPCODE(SubstractFloat)
OPCODE(MultiplyFloat)
OPCODE(DivideFloat)
OPCODE(EqualFloat)
OPCODE(NotEqualFloat)
OPCODE(GreaterThanFloat)
OPCODE(GreaterOrEqualFloat)
OPCODE(LessThanFloat)
OPCODE(LessOrEqualFloat)

OPCODE(EqualBool)
OPCODE(NotEqualBool)
OPCODE(NotBool)
OPCODE(AndBool)
OPCODE(OrBool)

OPCODE(Jump)
OPCODE(BranchIfTrue)
OPCODE(BranchIfFalse)
OPCODE(SkipIfTrue)
OPCODE(SkipIfFalse)

OPCODE(Call)
OPCODE(Return)

OPCODE(Print)
OPCODE(IntToFloat)
OPCODE(FloatToInt)

OPCODE(Exit)

#undef OPCODE
