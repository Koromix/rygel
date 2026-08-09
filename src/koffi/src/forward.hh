// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

enum class AbiOpcode {
    #define PRIMITIVE(Name) Push ## Name,
    #include "primitives.inc"
    PushAggregateReg,
    PushAggregateSplit,
    PushAggregateStack,
    PushAggregateMem,
    #define PRIMITIVE(Name) Run ## Name,
    #include "primitives.inc"
    RunAggregateG,
    RunAggregateF,
    RunAggregateD,
    RunAggregateGG,
    RunAggregateDD,
    RunAggregateGD,
    RunAggregateDG,
    RunAggregateDDDD,
    RunAggregateMem,
    #define PRIMITIVE(Name) Run ## Name ## X,
    #include "primitives.inc"
    RunAggregateGX,
    RunAggregateFX,
    RunAggregateDX,
    RunAggregateGGX,
    RunAggregateDDX,
    RunAggregateGDX,
    RunAggregateDGX,
    RunAggregateDDDDX,
    RunAggregateMemX,
    Yield,
    CallG,
    CallF,
    CallD,
    CallGG,
    CallDD,
    CallDG,
    CallGD,
    CallDDDD,
    CallMem,
    CallGX,
    CallFX,
    CallDX,
    CallGGX,
    CallDDX,
    CallDGX,
    CallGDX,
    CallDDDDX,
    CallMemX,
    #define PRIMITIVE(Name) Return ## Name,
    #include "primitives.inc"
    ReturnAggregateReg,
    ReturnAggregateMem
};

void *Code2Op(AbiOpcode code);

}
