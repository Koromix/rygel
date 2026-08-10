// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "ffi.hh"

#include <napi.h>

namespace K {

bool PreparePlan(Napi::Env env, InstanceData *instance, FunctionInfo *func);

enum class Opcode {
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
    CallGD,
    CallDG,
    CallDDDD,
    CallMem,
    CallGX,
    CallFX,
    CallDX,
    CallGGX,
    CallDDX,
    CallGDX,
    CallDGX,
    CallDDDDX,
    CallMemX,

    #define PRIMITIVE(Name) Return ## Name,
    #include "primitives.inc"
    ReturnAggregateReg,
    ReturnAggregateMem
};

static inline void *Code2Op(Opcode code) { return (void *)code; }

// ABI-specific
bool AnalyseFunction(Napi::Env env, InstanceData *instance, FunctionInfo *func);

}
