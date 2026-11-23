// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "ffi.hh"
#include "call.hh"
#include "interp.hh"
#include "type.hh"
#include "util.hh"
#if defined(_WIN32)
    #include "win32.hh"
#endif

#include <napi.h>

namespace K {

struct RetGG {
    uint64_t r0;
    uint64_t r1;
};
struct RetGD {
    uint64_t r0;
    double d0;
};
struct RetDG {
    double d0;
    uint64_t r0;
};
struct RetDD {
    double d0;
    double d1;
};

extern "C" {
    // Each ABI backend uses a different subset of CallXX assembly functions.
    // Use weak symbols to provide a default implementation for unused ones and avoid linker errors.

#if defined(_MSC_VER) && defined(_WIN64)
    void CallUnused(const void *, uint8_t *, uint8_t **) { K_UNREACHABLE(); }

    #define WEAK_CALL(Ret, Name) \
        Ret Name(const void *, uint8_t *, uint8_t **); \
        _Pragma(K_STRINGIFY(comment(linker, K_STRINGIFY(/alternatename:Name=CallUnused))));
#elif defined(_MSC_VER)
    void CallUnused(const void *, uint8_t *, uint8_t **) { K_UNREACHABLE(); }

    #define WEAK_CALL(Ret, Name) \
        Ret Name(const void *, uint8_t *, uint8_t **); \
        _Pragma(K_STRINGIFY(comment(linker, K_STRINGIFY(/alternatename:K_CONCAT(_, Name)=_CallUnused))))
#else
    #define WEAK_CALL(Ret, Name) __attribute__((weak)) Ret Name(const void *, uint8_t *, uint8_t **) { K_UNREACHABLE(); }
#endif

    WEAK_CALL(uint64_t, CallG)
    WEAK_CALL(float, CallF)
    WEAK_CALL(double, CallD)
    WEAK_CALL(RetGG, CallGG)
    WEAK_CALL(RetDD, CallDD)
    WEAK_CALL(RetGD, CallGD)
    WEAK_CALL(RetDG, CallDG)
    WEAK_CALL(void, CallHfa)
    WEAK_CALL(uint64_t, CallGX)
    WEAK_CALL(float, CallFX)
    WEAK_CALL(double, CallDX)
    WEAK_CALL(RetGG, CallGGX)
    WEAK_CALL(RetDD, CallDDX)
    WEAK_CALL(RetGD, CallGDX)
    WEAK_CALL(RetDG, CallDGX)
    WEAK_CALL(void, CallHfaX)

#undef WEAK_CALL
}

static inline Opcode Op2Code(void *o) { return (Opcode)(uintptr_t)o; }

static uint32_t GetTypeIndex(InstanceData *instance, const TypeInfo *type)
{
    uint32_t idx = type->type_idx;

    if (!idx--) {
        idx = instance->types.FindIndex(type);
        K_ASSERT(idx < (1u << 29));

        type->type_idx = idx + 1;
    }

    K_ASSERT(&instance->types[idx] == type);

    return idx;
}

void *Code2Op(Opcode code) { return (void *)code; }

uint32_t PackType(InstanceData *instance, const ParameterInfo &param)
{
    return PackType(instance, param.type, param.directions);
}

uint32_t PackType(InstanceData *instance, const TypeInfo *type, int directions)
{
    K_ASSERT(directions >= 0 && directions < 4);

    uint32_t packed = GetTypeIndex(instance, type);

    packed |= directions << 29;
    packed |= type->dispose ? (1u << 31) : 0;

    return packed;
}

static K_FORCE_INLINE const TypeInfo *GetType(const InstanceData *instance, uint32_t packed)
{
    uint32_t idx = packed & ((1u << 29) - 1);
    return &instance->types[idx];
}

static K_FORCE_INLINE int GetTypeDirections(uint32_t packed) { return (packed >> 29) & 3; }
static K_FORCE_INLINE bool IsTypeDisposable(uint32_t packed) {  return packed & (1u << 31); }

#if defined(MUST_TAIL)

#define RUN_TAIL
#define RUN_ATTRIBUTES
#define RUN_SUFFIX Normal
#include "loop.cc"

#if defined(EXTENDED_CALL_THRESHOLD)
    #define RUN_TAIL
    #define RUN_ATTRIBUTES __attribute__((preserve_none))
    #define RUN_SUFFIX Extended
    #include "loop.cc"
#endif

#else

#define RUN_ATTRIBUTES
#define RUN_SUFFIX Normal
#include "loop.cc"

#endif

bool PreparePlan(InstanceData *instance, FunctionInfo *func)
{
    K_ASSERT(!func->plan.sync.len);
    K_ASSERT(!func->plan.async.len);
    K_ASSERT(!func->plan.relay.len);

    if (!func->lib && func->convention != CallConvention::Cdecl &&
                      func->convention != CallConvention::Stdcall) {
        ThrowError<Napi::Error>(instance->env, "Only Cdecl and Stdcall callbacks are supported");
        return false;
    }

    AnalyseFunction(instance, func, &func->plan, &func->decorated_name);

    K_ASSERT(func->plan.sync.len);
    K_ASSERT(!func->plan.relay.len);
    K_ASSERT(!func->plan.async.len);
    K_ASSERT(func->plan.stk_size);

    // Use optimized instructions (when possible)
    for (OpData &op: func->plan.sync) {
        K_ASSERT(Op2Code(op.o) != Opcode::PushStringFast);
        K_ASSERT(Op2Code(op.o) != Opcode::PushPointerFast);

        if (Op2Code(op.o) == Opcode::PushString && GetTypeDirections(op.b) == 1) {
            op.o = Code2Op(Opcode::PushStringFast);
        }
        if (Op2Code(op.o) == Opcode::PushPointer && GetType(instance, op.b)->ref.conversion == BufferConversion::None) {
            op.o = Code2Op(Opcode::PushPointerFast);
        }
    }

    if (!func->variadic) {
        func->plan.relay = func->plan.sync;

        for (OpData &op: func->plan.sync) {
            switch (Op2Code(op.o)) {
                case Opcode::PushVoid:
                case Opcode::PushBool:
                case Opcode::PushInt8:
                case Opcode::PushUInt8:
                case Opcode::PushInt16:
                case Opcode::PushInt16S:
                case Opcode::PushUInt16:
                case Opcode::PushUInt16S:
                case Opcode::PushInt32:
                case Opcode::PushInt32S:
                case Opcode::PushUInt32:
                case Opcode::PushUInt32S:
                case Opcode::PushInt64:
                case Opcode::PushInt64S:
                case Opcode::PushUInt64:
                case Opcode::PushUInt64S:
                case Opcode::PushString:
                case Opcode::PushString16:
                case Opcode::PushString32:
                case Opcode::PushPointer:
                case Opcode::PushRecord:
                case Opcode::PushUnion:
                case Opcode::PushArray:
                case Opcode::PushFloat32:
                case Opcode::PushFloat64:
                case Opcode::PushCallback:
                case Opcode::PushPrototype:
                case Opcode::PushAggregateReg:
                case Opcode::PushAggregatePair:
                case Opcode::PushAggregateSplit:
                case Opcode::PushAggregateMem:
                case Opcode::PushStringFast:
                case Opcode::PushPointerFast: { func->plan.async.Append(op); } break;

                case Opcode::ConfigRelay: {} break;

                case Opcode::RunVoid:
                case Opcode::RunBool:
                case Opcode::RunInt8:
                case Opcode::RunUInt8:
                case Opcode::RunInt16:
                case Opcode::RunInt16S:
                case Opcode::RunUInt16:
                case Opcode::RunUInt16S:
                case Opcode::RunInt32:
                case Opcode::RunInt32S:
                case Opcode::RunUInt32:
                case Opcode::RunUInt32S:
                case Opcode::RunInt64:
                case Opcode::RunInt64S:
                case Opcode::RunUInt64:
                case Opcode::RunUInt64S:
                case Opcode::RunString:
                case Opcode::RunString16:
                case Opcode::RunString32:
                case Opcode::RunPointer:
                case Opcode::RunCallback: {
                    int delta = (int)Opcode::ReturnVoid - (int)Opcode::RunVoid;
                    Opcode ret = (Opcode)((intptr_t)op.o + delta);

                    func->plan.async.Append({ .o = Code2Op(Opcode::Yield) });
                    func->plan.async.Append({ .o = Code2Op(Opcode::CallG) });
                    func->plan.async.Append({ .o = Code2Op(ret), .b = op.b });
                } break;
                case Opcode::RunRecord:
                case Opcode::RunUnion:
                case Opcode::RunArray: { K_UNREACHABLE(); } break;
                case Opcode::RunFloat32: {
                    func->plan.async.Append({ .o = Code2Op(Opcode::Yield) });
                    func->plan.async.Append({ .o = Code2Op(Opcode::CallF) });
                    func->plan.async.Append({ .o = Code2Op(Opcode::ReturnFloat32), .b = op.b });
                } break;
                case Opcode::RunFloat64: {
                    func->plan.async.Append({ .o = Code2Op(Opcode::Yield) });
                    func->plan.async.Append({ .o = Code2Op(Opcode::CallD) });
                    func->plan.async.Append({ .o = Code2Op(Opcode::ReturnFloat64), .b = op.b });
                } break;
                case Opcode::RunPrototype: { K_UNREACHABLE(); } break;
                case Opcode::RunAggregateG:
                case Opcode::RunAggregateF:
                case Opcode::RunAggregateD:
                case Opcode::RunAggregateGG:
                case Opcode::RunAggregateDD:
                case Opcode::RunAggregateGD:
                case Opcode::RunAggregateDG:
                case Opcode::RunAggregateHfa: {
                    int delta = (int)Opcode::CallG - (int)Opcode::RunAggregateG;
                    Opcode call = (Opcode)((intptr_t)op.o + delta);

                    func->plan.async.Append({ .o = Code2Op(Opcode::Yield) });
                    func->plan.async.Append({ .o = Code2Op(call) });
                    func->plan.async.Append({ .o = Code2Op(Opcode::ReturnAggregateReg), .b = op.b });
                } break;
                case Opcode::RunAggregateMem: {
                    const TypeInfo *type = GetType(instance, op.b);

                    func->plan.async.Append({ .o = Code2Op(Opcode::Yield) });
                    func->plan.async.Append({ .o = Code2Op(Opcode::CallMem), .a1l = op.a1l, .b = (uint32_t)type->size });
                    func->plan.async.Append({ .o = Code2Op(Opcode::ReturnAggregateMem), .b = op.b });
                } break;
                case Opcode::RunVoidX:
                case Opcode::RunBoolX:
                case Opcode::RunInt8X:
                case Opcode::RunUInt8X:
                case Opcode::RunInt16X:
                case Opcode::RunInt16SX:
                case Opcode::RunUInt16X:
                case Opcode::RunUInt16SX:
                case Opcode::RunInt32X:
                case Opcode::RunInt32SX:
                case Opcode::RunUInt32X:
                case Opcode::RunUInt32SX:
                case Opcode::RunInt64X:
                case Opcode::RunInt64SX:
                case Opcode::RunUInt64X:
                case Opcode::RunUInt64SX:
                case Opcode::RunStringX:
                case Opcode::RunString16X:
                case Opcode::RunString32X:
                case Opcode::RunPointerX:
                case Opcode::RunCallbackX: {
                    int delta = (int)Opcode::ReturnVoid - (int)Opcode::RunVoidX;
                    Opcode ret = (Opcode)((intptr_t)op.o + delta);

                    func->plan.async.Append({ .o = Code2Op(Opcode::Yield) });
                    func->plan.async.Append({ .o = Code2Op(Opcode::CallGX) });
                    func->plan.async.Append({ .o = Code2Op(ret), .b = op.b });
                } break;
                case Opcode::RunRecordX:
                case Opcode::RunUnionX:
                case Opcode::RunArrayX: { K_UNREACHABLE(); } break;
                case Opcode::RunFloat32X: {
                    func->plan.async.Append({ .o = Code2Op(Opcode::Yield) });
                    func->plan.async.Append({ .o = Code2Op(Opcode::CallFX) });
                    func->plan.async.Append({ .o = Code2Op(Opcode::ReturnFloat32), .b = op.b });
                } break;
                case Opcode::RunFloat64X: {
                    func->plan.async.Append({ .o = Code2Op(Opcode::Yield) });
                    func->plan.async.Append({ .o = Code2Op(Opcode::CallDX) });
                    func->plan.async.Append({ .o = Code2Op(Opcode::ReturnFloat64), .b = op.b });
                } break;
                case Opcode::RunPrototypeX: { K_UNREACHABLE(); } break;
                case Opcode::RunAggregateGX:
                case Opcode::RunAggregateFX:
                case Opcode::RunAggregateDX:
                case Opcode::RunAggregateGGX:
                case Opcode::RunAggregateDDX:
                case Opcode::RunAggregateGDX:
                case Opcode::RunAggregateDGX:
                case Opcode::RunAggregateHfaX: {
                    int delta = (int)Opcode::CallGX - (int)Opcode::RunAggregateGX;
                    Opcode call = (Opcode)((intptr_t)op.o + delta);

                    func->plan.async.Append({ .o = Code2Op(Opcode::Yield) });
                    func->plan.async.Append({ .o = Code2Op(call) });
                    func->plan.async.Append({ .o = Code2Op(Opcode::ReturnAggregateReg), .b = op.b });
                } break;
                case Opcode::RunAggregateMemX: {
                    const TypeInfo *type = GetType(instance, op.b);

                    func->plan.async.Append({ .o = Code2Op(Opcode::Yield) });
                    func->plan.async.Append({ .o = Code2Op(Opcode::CallMemX), .a1l = op.a1l, .b = (uint32_t)type->size });
                    func->plan.async.Append({ .o = Code2Op(Opcode::ReturnAggregateMem), .b = op.b });
                } break;

                case Opcode::Int8ToInt8:
                case Opcode::UInt8ToUInt8:
                case Opcode::Int16ToInt16:
                case Opcode::UInt16ToUInt16:
                case Opcode::Int32ToInt32:
                case Opcode::UInt32ToUInt32:
                case Opcode::Int64ToInt64:
                case Opcode::UInt64ToUInt64:
                case Opcode::Float32ToFloat32:
                case Opcode::Float64ToFloat64:
                case Opcode::StringToInt32:
                case Opcode::Int32ToString:
                case Opcode::PointerToInt32:
                case Opcode::Int32ToPointer: { K_UNREACHABLE(); } break;

                case Opcode::Yield: { K_UNREACHABLE(); } break;

                case Opcode::CallG:
                case Opcode::CallF:
                case Opcode::CallD:
                case Opcode::CallGG:
                case Opcode::CallDD:
                case Opcode::CallGD:
                case Opcode::CallDG:
                case Opcode::CallHfa:
                case Opcode::CallMem:
                case Opcode::CallGX:
                case Opcode::CallFX:
                case Opcode::CallDX:
                case Opcode::CallGGX:
                case Opcode::CallDDX:
                case Opcode::CallGDX:
                case Opcode::CallDGX:
                case Opcode::CallHfaX:
                case Opcode::CallMemX: { K_UNREACHABLE(); } break;

                case Opcode::ReturnVoid:
                case Opcode::ReturnBool:
                case Opcode::ReturnInt8:
                case Opcode::ReturnUInt8:
                case Opcode::ReturnInt16:
                case Opcode::ReturnInt16S:
                case Opcode::ReturnUInt16:
                case Opcode::ReturnUInt16S:
                case Opcode::ReturnInt32:
                case Opcode::ReturnInt32S:
                case Opcode::ReturnUInt32:
                case Opcode::ReturnUInt32S:
                case Opcode::ReturnInt64:
                case Opcode::ReturnInt64S:
                case Opcode::ReturnUInt64:
                case Opcode::ReturnUInt64S:
                case Opcode::ReturnString:
                case Opcode::ReturnString16:
                case Opcode::ReturnString32:
                case Opcode::ReturnPointer:
                case Opcode::ReturnCallback:
                case Opcode::ReturnRecord:
                case Opcode::ReturnUnion:
                case Opcode::ReturnArray:
                case Opcode::ReturnFloat32:
                case Opcode::ReturnFloat64:
                case Opcode::ReturnPrototype:
                case Opcode::ReturnAggregateReg:
                case Opcode::ReturnAggregateMem: { K_UNREACHABLE(); } break;
            }
        }

#if defined(MUST_TAIL)
        for (OpData &op: func->plan.async) {
            op.o = (void *)ForwardDispatchNormal[(uintptr_t)op.o];
        }
        for (OpData &op: func->plan.relay) {
            op.o = (void *)RelayDispatchNormal[(uintptr_t)op.o];
        }
#endif
    }

    // Use fused push-return instructions where relevant
    if (func->plan.sync.len >= 2) {
        OpData &a = func->plan.sync[func->plan.sync.len - 2];
        OpData &b = func->plan.sync[func->plan.sync.len - 1];

#define FUSE(A, B, C) \
            do { \
                if (Op2Code(a.o) == Opcode::A && Op2Code(b.o) == Opcode::B) { \
                    a.o = Code2Op(Opcode::C); \
                    func->plan.sync.len--; \
                } \
            } while (false)

        FUSE(PushInt8, RunInt8, Int8ToInt8);
        FUSE(PushUInt8, RunUInt8, UInt8ToUInt8);
        FUSE(PushInt16, RunInt16, Int16ToInt16);
        FUSE(PushUInt16, RunUInt16, UInt16ToUInt16);
        FUSE(PushInt32, RunInt32, Int32ToInt32);
        FUSE(PushUInt32, RunUInt32, UInt32ToUInt32);
        FUSE(PushInt64, RunInt64, Int64ToInt64);
        FUSE(PushUInt64, RunUInt64, UInt64ToUInt64);
        FUSE(PushFloat32, RunFloat32, Float32ToFloat32);
        FUSE(PushFloat64, RunFloat64, Float64ToFloat64);
        FUSE(PushInt32, RunString, Int32ToString);
        FUSE(PushStringFast, RunInt32, StringToInt32);
        FUSE(PushInt32, RunPointer, Int32ToPointer);
        FUSE(PushPointerFast, RunInt32, PointerToInt32);

#undef FUSE
    }

#if defined(MUST_TAIL) && defined(EXTENDED_CALL_THRESHOLD)
    if (func->parameters.len >= EXTENDED_CALL_THRESHOLD) {
        for (OpData &op: func->plan.sync) {
            op.o = (void *)ForwardDispatchExtended[(uintptr_t)op.o];
        }
    } else {
        for (OpData &op: func->plan.sync) {
            op.o = (void *)ForwardDispatchNormal[(uintptr_t)op.o];
        }
    }
#elif defined(MUST_TAIL)
    for (OpData &op: func->plan.sync) {
        op.o = (void *)ForwardDispatchNormal[(uintptr_t)op.o];
    }
#endif

    if (func->plan.sync.len <= K_LEN(func->small_sync)) {
        MemCpy(func->small_sync, func->plan.sync.ptr, func->plan.sync.len * K_SIZE(OpData));
    } else {
        MemSet(func->small_sync, 0, K_SIZE(func->small_sync));
    }

    return true;
}

}
