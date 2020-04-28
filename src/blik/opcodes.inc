// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPCODE
    #error Please define OPCODE() before including opcodes.inc
#endif

// Immediate values
OPCODE(PushBool)
OPCODE(PushInt)
OPCODE(PushFloat)
OPCODE(PushString)
OPCODE(Pop)

// Local variables
OPCODE(LoadBool)
OPCODE(LoadInt)
OPCODE(LoadFloat)
OPCODE(LoadString)
OPCODE(StoreBool)
OPCODE(StoreInt)
OPCODE(StoreFloat)
OPCODE(StoreString)
OPCODE(CopyBool)
OPCODE(CopyInt)
OPCODE(CopyFloat)
OPCODE(CopyString)

// Global variables
OPCODE(LoadGlobalBool)
OPCODE(LoadGlobalInt)
OPCODE(LoadGlobalFloat)
OPCODE(LoadGlobalString)
OPCODE(StoreGlobalBool)
OPCODE(StoreGlobalInt)
OPCODE(StoreGlobalFloat)
OPCODE(StoreGlobalString)

// Integer operators
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
OPCODE(ComplementInt)
OPCODE(LeftShiftInt)
OPCODE(RightShiftInt)

// Float operators
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

// Boolean operators
OPCODE(EqualBool)
OPCODE(NotEqualBool)
OPCODE(NotBool)
OPCODE(AndBool)
OPCODE(OrBool)

// Jumps and branch
OPCODE(Jump)
OPCODE(BranchIfTrue)
OPCODE(BranchIfFalse)
OPCODE(SkipIfTrue)
OPCODE(SkipIfFalse)

// Functions
OPCODE(Call)
OPCODE(CallNative)
OPCODE(Return)
OPCODE(ReturnNull)

// Intrinsics
OPCODE(Print)
OPCODE(IntToFloat)
OPCODE(FloatToInt)
OPCODE(Exit)

#undef OPCODE
