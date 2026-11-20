// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

#include <napi.h>

namespace K {

struct InstanceData;
struct FunctionInfo;
struct TypeInfo;
struct ParameterInfo;
struct CallData;
struct TrampolineInfo;

struct OpData {
    void *o;
    union {
        int32_t a;
        struct {
            union {
                int16_t a1;
                struct {
                    uint8_t a1l;
                    uint8_t a1h;
                };
            };
            int16_t a2;
        };
    };
    union {
        uint32_t b;
        struct {
            int16_t b1;
            int16_t b2;
        };
    };
};
static_assert(K_SIZE(OpData) == K_SIZE(void *) + 8);

struct ExecutionPlan {
    Size stk_size = 0;

    HeapArray<OpData> sync;
    HeapArray<OpData> relay;
    HeapArray<OpData> async;
};

enum class Opcode {
    #define PRIMITIVE(Name) Push ## Name,
    #include "primitives.inc"
    PushAggregateReg,
    PushAggregatePair,
    PushAggregateSplit,
    PushAggregateMem,

    // Optimized routines selected by interp.cc, don't pick them in ABI code
    PushStringFast,
    PushPointerFast,

    ConfigRelay,

    #define PRIMITIVE(Name) Run ## Name,
    #include "primitives.inc"
    RunAggregateG,
    RunAggregateF,
    RunAggregateD,
    RunAggregateGG,
    RunAggregateDD,
    RunAggregateGD,
    RunAggregateDG,
    RunAggregateHfa,
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
    RunAggregateHfaX,
    RunAggregateMemX,

    Yield,

    CallG,
    CallF,
    CallD,
    CallGG,
    CallDD,
    CallGD,
    CallDG,
    CallHfa,
    CallMem,
    CallGX,
    CallFX,
    CallDX,
    CallGGX,
    CallDDX,
    CallGDX,
    CallDGX,
    CallHfaX,
    CallMemX,

    #define PRIMITIVE(Name) Return ## Name,
    #include "primitives.inc"
    ReturnAggregateReg,
    ReturnAggregateMem
};

bool PreparePlan(InstanceData *instance, FunctionInfo *func);

void *Code2Op(Opcode code);
uint32_t PackType(InstanceData *instance, const ParameterInfo &param);
uint32_t PackType(InstanceData *instance, const TypeInfo *type, int directions = 0);

#if defined(__GNUC__) || defined(__clang__)
    #if defined(__x86_64__) || defined(__aarch64__)
        #if  __has_attribute(musttail) && __has_attribute(preserve_none)
            #define MUST_TAIL __attribute__((musttail))
            #define PRESERVE_NONE __attribute__((preserve_none))
            #define NO_STACK_PROTECTOR __attribute__((no_stack_protector))
        #endif
    #endif
#endif

#if defined(MUST_TAIL)

PRESERVE_NONE typedef napi_value ForwardFunc(CallData *call, uint8_t *base, napi_value *argp, const OpData *op);
PRESERVE_NONE typedef int RelayFunc(CallData *call, uint8_t *base, napi_value *argp, const OpData *op);

static K_FORCE_INLINE napi_value RunForward(CallData *call, uint8_t *base, napi_value *argp, const OpData *op)
{
    return ((ForwardFunc *)op->o)(call, base, argp, op);
}

static K_FORCE_INLINE int RunRelay(CallData *call, uint8_t *base, napi_value *argp, const OpData *op)
{
    return ((RelayFunc *)op->o)(call, base, argp, op);

}

#else

napi_value RunForward(CallData *call, uint8_t *base, napi_value *argp, const OpData *op);
int RunRelay(CallData *call, uint8_t *base, napi_value *argp, const OpData *op);

#endif

// ABI-specific

void AnalyseFunction(InstanceData *instance, const FunctionInfo *func, ExecutionPlan *out_plan, const char **out_decorated = nullptr);

}
