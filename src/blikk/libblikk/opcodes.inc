// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#if !defined(OPCODE)
    #error Please define OPCODE() before including opcodes.inc
#endif

OPCODE(Nop)

// Constant values
OPCODE(Push)
OPCODE(Reserve)
OPCODE(Fetch)
OPCODE(Pop)

// Load/store
OPCODE(Lea)
OPCODE(LeaLocal)
OPCODE(LeaRel)
OPCODE(Load)
OPCODE(LoadLocal)
OPCODE(LoadIndirect)
OPCODE(LoadIndirectK)
OPCODE(Store)
OPCODE(StoreK)
OPCODE(StoreLocal)
OPCODE(StoreLocalK)
OPCODE(StoreIndirect)
OPCODE(StoreIndirectK)
OPCODE(StoreRev)
OPCODE(StoreRevK)
OPCODE(CheckIndex)

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

// Other operators
OPCODE(EqualString)
OPCODE(NotEqualString)
OPCODE(EqualType)
OPCODE(NotEqualType)
OPCODE(EqualFunc)
OPCODE(NotEqualFunc)
OPCODE(EqualEnum)
OPCODE(NotEqualEnum)

// Jumps and branch
OPCODE(Jump)
OPCODE(BranchIfTrue)
OPCODE(BranchIfFalse)
OPCODE(SkipIfTrue)
OPCODE(SkipIfFalse)

// Functions
OPCODE(CallIndirect)
OPCODE(Call)
OPCODE(Return)

// Intrinsics
OPCODE(IntToFloat)
OPCODE(FloatToInt)
OPCODE(InlineIf)

OPCODE(End)

#undef OPCODE
