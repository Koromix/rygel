// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

#include <napi.h>

namespace K {

struct InstanceData;
struct FunctionInfo;
struct TypeInfo;
struct CallData;
struct TrampolineInfo;

struct InstructionData {
    void *op;

    int32_t a = 0;
    int16_t b1 = 0;
    int16_t b2 = 0;
    const TypeInfo *type = nullptr;
};

struct ExecutionPlan {
    HeapArray<InstructionData> sync;
    HeapArray<InstructionData> async;
    HeapArray<InstructionData> relay;

    Size stk_size = 0;
};

enum class Opcode {
    #define PRIMITIVE(Name) Push ## Name,
    #include "primitives.inc"
    PushAggregateReg,
    PushAggregateSplit,
    PushAggregateMem,
    PushPair,

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

bool PreparePlan(InstanceData *instance, FunctionInfo *func);

static inline void *Code2Op(Opcode code) { return (void *)code; }

void FillAsyncPlan(Span<const InstructionData> sync, HeapArray<InstructionData> *out_async);
int AnalyseFlat(const TypeInfo *type, FunctionRef<void(const TypeInfo *type, int offset, int count)> func);

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

PRESERVE_NONE typedef napi_value ForwardFunc(CallData *call, uint8_t *base, void *native, const InstructionData *inst);
PRESERVE_NONE typedef int RelayFunc(CallData *call, TrampolineInfo *trampoline, uint8_t *base, const InstructionData *inst);

static K_FORCE_INLINE napi_value RunForward(CallData *call, uint8_t *base, void *native, const InstructionData *inst)
{
    return ((ForwardFunc *)inst->op)(call, base, native, inst);
}

static K_FORCE_INLINE int RunRelay(CallData *call, TrampolineInfo *trampoline, uint8_t *base, const InstructionData *inst)
{
    return ((RelayFunc *)inst->op)(call, trampoline, base, inst);

}

#else

napi_value RunForward(CallData *call, uint8_t *base, void *native, const InstructionData *inst);
int RunRelay(CallData *call, TrampolineInfo *trampoline, uint8_t *base, const InstructionData *inst);

#endif

// ABI-specific

void AnalyseFunction(InstanceData *instance, const FunctionInfo *func, ExecutionPlan *out_plan, const char **out_decorated = nullptr);

}
