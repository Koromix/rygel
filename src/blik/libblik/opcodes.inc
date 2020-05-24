// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPCODE
    #error Please define OPCODE() before including opcodes.inc
#endif

OPCODE(Nop)

// Immediate values
OPCODE(PushNull)
OPCODE(PushBool)
OPCODE(PushInt)
OPCODE(PushFloat)
OPCODE(PushString)
OPCODE(PushType)
OPCODE(Pop)

// Local variables
OPCODE(LoadBool)
OPCODE(LoadInt)
OPCODE(LoadFloat)
OPCODE(LoadString)
OPCODE(LoadType)
OPCODE(StoreBool)
OPCODE(StoreInt)
OPCODE(StoreFloat)
OPCODE(StoreString)
OPCODE(StoreType)
OPCODE(CopyBool)
OPCODE(CopyInt)
OPCODE(CopyFloat)
OPCODE(CopyString)
OPCODE(CopyType)

// Global variables
OPCODE(LoadGlobalBool)
OPCODE(LoadGlobalInt)
OPCODE(LoadGlobalFloat)
OPCODE(LoadGlobalString)
OPCODE(LoadGlobalType)
OPCODE(StoreGlobalBool)
OPCODE(StoreGlobalInt)
OPCODE(StoreGlobalFloat)
OPCODE(StoreGlobalString)
OPCODE(StoreGlobalType)

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
OPCODE(LeftRotateInt)
OPCODE(RightRotateInt)

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

// Type operators
OPCODE(EqualType)
OPCODE(NotEqualType)

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

// Intrinsics
OPCODE(IntToFloat)
OPCODE(FloatToInt)

OPCODE(End)

#undef OPCODE
