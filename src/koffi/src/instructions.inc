// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#if !defined(INSTRUCTION)
    #error Please define INSTRUCTION() before including primitives.inc
#endif

#define PRIMITIVE(Name) INSTRUCTION(Push ## Name)
#include "primitives.inc"
INSTRUCTION(PushAggregateReg)
INSTRUCTION(PushAggregatePair)
INSTRUCTION(PushAggregateSplit)
INSTRUCTION(PushAggregateMem)

// Optimized routines selected by interp.cc, don't pick them in ABI code
INSTRUCTION(PushStringFast)
INSTRUCTION(PushPointerFast)

INSTRUCTION(ConfigRelay)

#define PRIMITIVE(Name) INSTRUCTION(Run ## Name)
#include "primitives.inc"
INSTRUCTION(RunAggregateG)
INSTRUCTION(RunAggregateF)
INSTRUCTION(RunAggregateD)
INSTRUCTION(RunAggregateGG)
INSTRUCTION(RunAggregateDD)
INSTRUCTION(RunAggregateGD)
INSTRUCTION(RunAggregateDG)
INSTRUCTION(RunAggregateHfa)
INSTRUCTION(RunAggregateMem)
#define PRIMITIVE(Name) INSTRUCTION(Run ## Name ## X)
#include "primitives.inc"
INSTRUCTION(RunAggregateGX)
INSTRUCTION(RunAggregateFX)
INSTRUCTION(RunAggregateDX)
INSTRUCTION(RunAggregateGGX)
INSTRUCTION(RunAggregateDDX)
INSTRUCTION(RunAggregateGDX)
INSTRUCTION(RunAggregateDGX)
INSTRUCTION(RunAggregateHfaX)
INSTRUCTION(RunAggregateMemX)

// Optimized 1-to-1 routines
INSTRUCTION(Int8ToInt8)
INSTRUCTION(UInt8ToUInt8)
INSTRUCTION(Int16ToInt16)
INSTRUCTION(UInt16ToUInt16)
INSTRUCTION(Int32ToInt32)
INSTRUCTION(UInt32ToUInt32)
INSTRUCTION(Int64ToInt64)
INSTRUCTION(UInt64ToUInt64)

INSTRUCTION(Yield)

INSTRUCTION(CallG)
INSTRUCTION(CallF)
INSTRUCTION(CallD)
INSTRUCTION(CallGG)
INSTRUCTION(CallDD)
INSTRUCTION(CallGD)
INSTRUCTION(CallDG)
INSTRUCTION(CallHfa)
INSTRUCTION(CallMem)
INSTRUCTION(CallGX)
INSTRUCTION(CallFX)
INSTRUCTION(CallDX)
INSTRUCTION(CallGGX)
INSTRUCTION(CallDDX)
INSTRUCTION(CallGDX)
INSTRUCTION(CallDGX)
INSTRUCTION(CallHfaX)
INSTRUCTION(CallMemX)

#define PRIMITIVE(Name) INSTRUCTION(Return ## Name)
#include "primitives.inc"
INSTRUCTION(ReturnAggregateReg)
INSTRUCTION(ReturnAggregateMem)

#undef INSTRUCTION
