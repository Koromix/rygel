// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

#include <napi.h>

namespace K {

#if defined(__GNUC__) || defined(__clang__)
    #if  __has_attribute(musttail) && __has_attribute(preserve_none)
        #if defined(__x86_64__)
            #define MUST_TAIL __attribute__((musttail))
            #define PRESERVE_NONE __attribute__((preserve_none))
            #define EXTENDED_CALL_THRESHOLD 1
        #else
            #define MUST_TAIL __attribute__((musttail))
            #define PRESERVE_NONE __attribute__((preserve_none))
            #define EXTENDED_CALL_THRESHOLD 3
        #endif
    #endif

    #define NO_STACK_PROTECTOR __attribute__((no_stack_protector))
#endif

struct InstanceData;
struct FunctionInfo;
struct TypeInfo;
struct ParameterInfo;
struct CallData;
struct TrampolineInfo;
enum class CallMode;

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
    #define INSTRUCTION(Inst) Inst,
    #include "instructions.inc"
};

void *Code2Op(Opcode code);
uint32_t PackType(InstanceData *instance, const ParameterInfo &param);
uint32_t PackType(InstanceData *instance, const TypeInfo *type, int directions = 0);

bool PreparePlan(InstanceData *instance, FunctionInfo *func, CallMode mode);

#if defined(MUST_TAIL)

typedef napi_value ForwardFuncNormal(CallData *call, uint8_t *base, uintptr_t *argp, const OpData *op);
typedef napi_value ForwardFuncCompat(CallData *call, uint8_t *base, uintptr_t *argp, const OpData *op);
typedef int RelayFunc(CallData *call, uint8_t *base, napi_value *argp, const OpData *op);

static K_FORCE_INLINE napi_value RunForwardNormal(CallData *call, uint8_t *base, uintptr_t *argp, const OpData *op)
{
    return ((ForwardFuncNormal *)op->o)(call, base, argp, op);
}
static K_FORCE_INLINE napi_value RunForwardCompat(CallData *call, uint8_t *base, uintptr_t *argp, const OpData *op)
{
    return ((ForwardFuncCompat *)op->o)(call, base, argp, op);
}

static K_FORCE_INLINE int RunRelay(CallData *call, uint8_t *base, napi_value *argp, const OpData *op)
{
    return ((RelayFunc *)op->o)(call, base, argp, op);
}

#if defined(EXTENDED_CALL_THRESHOLD)

PRESERVE_NONE typedef napi_value ForwardFuncExtended(CallData *call, uint8_t *base, uintptr_t *argp, const OpData *op);

static K_FORCE_INLINE napi_value RunForwardExtended(CallData *call, uint8_t *base, uintptr_t *argp, const OpData *op)
{
    return ((ForwardFuncExtended *)op->o)(call, base, argp, op);
}

#endif

#else

napi_value RunForwardNormal(CallData *call, uint8_t *base, uintptr_t *argp, const OpData *op);
napi_value RunForwardCompat(CallData *call, uint8_t *base, uintptr_t *argp, const OpData *op);

int RunRelay(CallData *call, uint8_t *base, napi_value *argp, const OpData *op);

#endif

// ABI-specific

void AnalyseFunction(InstanceData *instance, const FunctionInfo *func, ExecutionPlan *out_plan, const char **out_decorated = nullptr);

}
