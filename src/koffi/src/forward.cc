// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "ffi.hh"
#include "call.hh"
#include "forward.hh"
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
struct RetDDDD {
    double d0;
    double d1;
    double d2;
    double d3;
};

extern "C" uint64_t ForwardCallG(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" float ForwardCallF(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" double ForwardCallD(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" RetGG ForwardCallGG(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" RetDD ForwardCallDD(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" RetDG ForwardCallDG(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" RetGD ForwardCallGD(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" RetDDDD ForwardCallDDDD(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" uint64_t ForwardCallGX(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" float ForwardCallFX(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" double ForwardCallDX(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" RetGG ForwardCallGGX(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" RetDD ForwardCallDDX(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" RetDG ForwardCallDGX(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" RetGD ForwardCallGDX(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" RetDDDD ForwardCallDDDDX(const void *func, uint8_t *sp, uint8_t **out_saved_sp);

#if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

namespace {
#if defined(MUST_TAIL)
    PRESERVE_NONE typedef napi_value ForwardFunc(CallData *call, napi_value *args, uint8_t *base, const AbiInstruction *inst);

    #define OP(Code) \
        PRESERVE_NONE napi_value Handle ## Code(CallData *call, napi_value *args, uint8_t *base, const AbiInstruction *inst)
    #define NEXT() \
        do { \
            const AbiInstruction *next = inst + 1; \
            MUST_TAIL return ((ForwardFunc *)next->op)(call, args, base, next); \
        } while (false)
#else
    #define OP(Code) \
        case (int)AbiOpcode::Code:
    #define NEXT() \
        break

    napi_value RunLoop(CallData *call, napi_value *args, uint8_t *base, const AbiInstruction *inst)
    {
        for (;; ++inst) {
            switch ((intptr_t)inst->op) {
#endif

#define INTEGER(CType) \
        do { \
            CType v; \
            if (!TryNumber(call->env, args[inst->a], &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, args[inst->a])); \
                return call->env.Null(); \
            } \
             \
            *(uintptr_t *)(base + inst->b1) = (uintptr_t)v; \
        } while (false)
#define INTEGER_SWAP(CType) \
        do { \
            CType v; \
            if (!TryNumber(call->env, args[inst->a], &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, args[inst->a])); \
                return call->env.Null(); \
            } \
             \
            *(uintptr_t *)(base + inst->b1) = (uintptr_t)ReverseBytes(v); \
        } while (false)
#define INTEGER64(CType) \
        do { \
            CType v; \
            if (!TryNumber(call->env, args[inst->a], &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, args[inst->a])); \
                return call->env.Null(); \
            } \
             \
            *(uint64_t *)(base + inst->b1) = (uint64_t)v; \
        } while (false)
#define INTEGER64_SWAP(CType) \
        do { \
            CType v; \
            if (!TryNumber(call->env, args[inst->a], &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, args[inst->a])); \
                return call->env.Null(); \
            } \
             \
            *(uint64_t *)(base + inst->b1) = (uint64_t)ReverseBytes(v); \
        } while (false)

    OP(PushVoid) { K_UNREACHABLE(); return call->env.Null(); }
    OP(PushBool) {
        bool b;
        if (napi_get_value_bool(call->env, args[inst->a], &b) != napi_ok) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected boolean", GetValueType(call->instance, args[inst->a]));
            return call->env.Null();
        }

        *(uintptr_t *)(base + inst->b1) = (uintptr_t)b;

        NEXT();
    }
    OP(PushInt8) { INTEGER(int8_t); NEXT(); }
    OP(PushUInt8) { INTEGER(uint8_t); NEXT(); }
    OP(PushInt16) { INTEGER(int16_t); NEXT(); }
    OP(PushInt16S) { INTEGER_SWAP(int16_t); NEXT(); }
    OP(PushUInt16) { INTEGER(uint16_t); NEXT(); }
    OP(PushUInt16S) { INTEGER_SWAP(uint16_t); NEXT(); }
    OP(PushInt32) { INTEGER(int32_t); NEXT(); }
    OP(PushInt32S) { INTEGER_SWAP(int32_t); NEXT(); }
    OP(PushUInt32) { INTEGER(uint32_t); NEXT(); }
    OP(PushUInt32S) { INTEGER_SWAP(uint32_t); NEXT(); }
    OP(PushInt64) { INTEGER64(int64_t); NEXT(); }
    OP(PushInt64S) { INTEGER64_SWAP(int64_t); NEXT(); }
    OP(PushUInt64) { INTEGER64(int64_t); NEXT(); }
    OP(PushUInt64S) { INTEGER64_SWAP(int64_t); NEXT(); }
    OP(PushString) {
        const char *str;
        if (!call->PushString(args[inst->a], inst->b2, &str)) [[unlikely]]
            return call->env.Null();

        *(const char **)(base + inst->b1) = str;

        NEXT();
    }
    OP(PushString16) {
        const char16_t *str16;
        if (!call->PushString16(args[inst->a], inst->b2, &str16)) [[unlikely]]
            return call->env.Null();

        *(const char16_t **)(base + inst->b1) = str16;

        NEXT();
    }
    OP(PushString32) {
        const char32_t *str32;
        if (!call->PushString32(args[inst->a], inst->b2, &str32)) [[unlikely]]
            return call->env.Null();

        *(const char32_t **)(base + inst->b1) = str32;

        NEXT();
    }
    OP(PushPointer) {
        void *ptr;
        if (!call->PushPointer(args[inst->a], inst->type, inst->b2, &ptr)) [[unlikely]]
            return call->env.Null();

        *(void **)(base + inst->b1) = ptr;

        NEXT();
    }
    OP(PushRecord) { K_UNREACHABLE(); return call->env.Null(); }
    OP(PushUnion) { K_UNREACHABLE(); return call->env.Null(); }
    OP(PushArray) { K_UNREACHABLE(); return call->env.Null(); }
    OP(PushFloat32) {
        float f;
        if (!TryNumber(call->env, args[inst->a], &f)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, args[inst->a]));
            return call->env.Null();
        }

#if K_SIZE_MAX == INT64_MAX
        memset(base + inst->b1, 0xFF, 8);
#endif
        *(float *)(base + inst->b1) = f;

        NEXT();
    }
    OP(PushFloat64) {
        double d;
        if (!TryNumber(call->env, args[inst->a], &d)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, args[inst->a]));
            return call->env.Null();
        }

        *(double *)(base + inst->b1) = d;

        NEXT();
    }
    OP(PushCallback) {
        void *ptr;
        if (!call->PushCallback(args[inst->a], inst->type, &ptr)) [[unlikely]]
            return call->env.Null();

        *(void **)(base + inst->b1) = ptr;

        NEXT();
    }
    OP(PushPrototype) { K_UNREACHABLE(); return call->env.Null(); }
    OP(PushAggregateReg) {
        napi_value arg = args[inst->a];

        uint8_t *ptr = base + inst->b1;

        if (!call->PushObject(arg, inst->type, ptr)) [[unlikely]]
            return call->env.Null();

        NEXT();
    }
    OP(PushAggregateSplit) {
        napi_value arg = args[inst->a];

        uint64_t buf[2];
        if (!call->PushObject(arg, inst->type, (uint8_t *)buf)) [[unlikely]]
            return call->env.Null();

        // The second part might be useless (if object fits in one register), in
        // which case the analysis code will put the same value in both offsets to
        // make sure we don't overwrite something else. Well, if we copy the second
        // part first, that is, as we do below.
        *(uint64_t *)(base + inst->b2) = buf[1];
        *(uint64_t *)(base + inst->b1) = buf[0];

        NEXT();
    }
    OP(PushAggregateStack) {
        napi_value arg = args[inst->a];

        if (!call->PushObject(arg, inst->type, base + inst->b1)) [[unlikely]]
            return call->env.Null();

        NEXT();
    }
    OP(PushAggregateMem) {
        napi_value arg = args[inst->a];

        uint8_t *ptr = call->AllocHeap(inst->type->size);
        *(uint8_t **)(base + inst->b1) = ptr;

        if (!call->PushObject(arg, inst->type, ptr)) [[unlikely]]
            return call->env.Null();

        NEXT();
    }

#undef INTEGER64_SWAP
#undef INTEGER64
#undef INTEGER_SWAP
#undef INTEGER

#if defined(_WIN32)
    #define WRAP(Expr) \
        [&]() { \
            TEB *teb = GetTEB(); \
             \
            K_DEFER { call->instance->last_error = teb->LastErrorValue; }; \
            teb->LastErrorValue = call->instance->last_error; \
             \
            ADJUST_TEB(teb, call->mem->stack0.ptr, call->mem->stack0.end); \
             \
            call->DebugForward(); \
            return (Expr); \
        }()
#else
    #define WRAP(Expr) (call->DebugForward(), (Expr))
#endif

#define INTEGER(Suffix, CType) \
        do { \
            uint64_t ret = WRAP(ForwardCall ## Suffix(call->native, base, &call->saved_sp)); \
            return NewInt(call->env, (CType)ret); \
        } while (false)
#define INTEGER_SWAP(Suffix, CType) \
        do { \
            uint64_t ret = WRAP(ForwardCall ## Suffix(call->native, base, &call->saved_sp)); \
            return NewInt(call->env, ReverseBytes((CType)ret)); \
        } while (false)
#define DISPOSE(Ptr) \
        do { \
            if (inst->type->dispose) { \
                inst->type->dispose(call->instance, inst->type, (Ptr)); \
            } \
        } while (false)

    OP(RunVoid) {
        WRAP(ForwardCallG(call->native, base, &call->saved_sp));
        return nullptr;
    }
    OP(RunBool) {
        uint64_t ret = WRAP(ForwardCallG(call->native, base, &call->saved_sp));
        return Napi::Boolean::New(call->env, ret & 0x1);
    }
    OP(RunInt8) { INTEGER(G, int8_t); }
    OP(RunUInt8) { INTEGER(G, uint8_t); }
    OP(RunInt16) { INTEGER(G, int16_t); }
    OP(RunInt16S) { INTEGER_SWAP(G, int16_t); }
    OP(RunUInt16) { INTEGER(G, uint16_t); }
    OP(RunUInt16S) { INTEGER_SWAP(G, uint16_t); }
    OP(RunInt32) { INTEGER(G, int32_t); }
    OP(RunInt32S) { INTEGER_SWAP(G, int32_t); }
    OP(RunUInt32) { INTEGER(G, uint32_t); }
    OP(RunUInt32S) { INTEGER_SWAP(G, uint32_t); }
    OP(RunInt64) { INTEGER(G, int64_t); }
    OP(RunInt64S) { INTEGER_SWAP(G, int64_t); }
    OP(RunUInt64) { INTEGER(G, uint64_t); }
    OP(RunUInt64S) { INTEGER_SWAP(G, uint64_t); }
    OP(RunString) {
        uint64_t ret = WRAP(ForwardCallG(call->native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    OP(RunString16) {
        uint64_t ret = WRAP(ForwardCallG(call->native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char16_t *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    OP(RunString32) {
        uint64_t ret = WRAP(ForwardCallG(call->native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char32_t *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    OP(RunPointer) {
        uint64_t ret = WRAP(ForwardCallG(call->native, base, &call->saved_sp));
        napi_value value = WrapPointer(call->env, (void *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    OP(RunCallback) {
        uint64_t ret = WRAP(ForwardCallG(call->native, base, &call->saved_sp));
        return WrapPointer(call->env, (void *)ret);
    }
    OP(RunRecord) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunUnion) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunArray) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunFloat32) {
        float f = WRAP(ForwardCallF(call->native, base, &call->saved_sp));
        return NewFloat(call->env, f);
    }
    OP(RunFloat64) {
        double d = WRAP(ForwardCallD(call->native, base, &call->saved_sp));
        return NewFloat(call->env, d);
    }
    OP(RunPrototype) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunAggregateG) {
        auto ret = WRAP(ForwardCallG(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateF) {
        auto ret = WRAP(ForwardCallF(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateD) {
        auto ret = WRAP(ForwardCallD(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateGG) {
        auto ret = WRAP(ForwardCallGG(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateDD) {
        auto ret = WRAP(ForwardCallDD(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateGD) {
        auto ret = WRAP(ForwardCallGD(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateDG) {
        auto ret = WRAP(ForwardCallDG(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateDDDD) {
        auto ret = WRAP(ForwardCallDDDD(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateMem) {
        uint8_t *ptr = call->AllocHeap(inst->a);
        *(uint8_t **)(base + inst->b1) = ptr;
        WRAP(ForwardCallG(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, ptr, inst->type);
    }
    OP(RunVoidX) {
        WRAP(ForwardCallGX(call->native, base, &call->saved_sp));
        return nullptr;
    }
    OP(RunBoolX) {
        uint64_t ret = WRAP(ForwardCallGX(call->native, base, &call->saved_sp));
        return Napi::Boolean::New(call->env, ret & 0x1);
    }
    OP(RunInt8X) { INTEGER(GX, int8_t); }
    OP(RunUInt8X) { INTEGER(GX, uint8_t); }
    OP(RunInt16X) { INTEGER(GX, int16_t); }
    OP(RunInt16SX) { INTEGER_SWAP(GX, int16_t); }
    OP(RunUInt16X) { INTEGER(GX, uint16_t); }
    OP(RunUInt16SX) { INTEGER_SWAP(GX, uint16_t); }
    OP(RunInt32X) { INTEGER(GX, int32_t); }
    OP(RunInt32SX) { INTEGER_SWAP(GX, int32_t); }
    OP(RunUInt32X) { INTEGER(GX, uint32_t); }
    OP(RunUInt32SX) { INTEGER_SWAP(GX, uint32_t); }
    OP(RunInt64X) { INTEGER(GX, int64_t); }
    OP(RunInt64SX) { INTEGER_SWAP(GX, int64_t); }
    OP(RunUInt64X) { INTEGER(GX, uint64_t); }
    OP(RunUInt64SX) { INTEGER_SWAP(GX, uint64_t); }
    OP(RunStringX) {
        uint64_t ret = WRAP(ForwardCallGX(call->native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    OP(RunString16X) {
        uint64_t ret = WRAP(ForwardCallGX(call->native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char16_t *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    OP(RunString32X) {
        uint64_t ret = WRAP(ForwardCallGX(call->native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char32_t *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    OP(RunPointerX) {
        uint64_t ret = WRAP(ForwardCallGX(call->native, base, &call->saved_sp));
        napi_value value = WrapPointer(call->env, (void *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    OP(RunCallbackX) {
        uint64_t ret = WRAP(ForwardCallGX(call->native, base, &call->saved_sp));
        return WrapPointer(call->env, (void *)ret);
    }
    OP(RunRecordX) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunUnionX) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunArrayX) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunFloat32X) {
        float f = WRAP(ForwardCallFX(call->native, base, &call->saved_sp));
        return NewFloat(call->env, f);
    }
    OP(RunFloat64X) {
        double d = WRAP(ForwardCallDX(call->native, base, &call->saved_sp));
        return NewFloat(call->env, d);
    }
    OP(RunPrototypeX) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunAggregateGX) {
        auto ret = WRAP(ForwardCallGX(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateFX) {
        auto ret = WRAP(ForwardCallFX(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateDX) {
        auto ret = WRAP(ForwardCallDX(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateGGX) {
        auto ret = WRAP(ForwardCallGGX(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateDDX) {
        auto ret = WRAP(ForwardCallDDX(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateGDX) {
        auto ret = WRAP(ForwardCallGDX(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateDGX) {
        auto ret = WRAP(ForwardCallDGX(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateDDDDX) {
        auto ret = WRAP(ForwardCallDDDDX(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateMemX) {
        uint8_t *ptr = call->AllocHeap(inst->a);
        *(uint8_t **)(base + inst->b1) = ptr;
        WRAP(ForwardCallGX(call->native, base, &call->saved_sp));
        return DecodeObject(call->instance, ptr, inst->type);
    }

#undef DISPOSE
#undef INTEGER_SWAP
#undef INTEGER

#define CALL(Suffix) \
        do { \
            auto ret = WRAP(ForwardCall ## Suffix(call->native, base, &call->saved_sp)); \
            memcpy(base, &ret, K_SIZE(ret)); \
        } while (false)
#define DISPOSE() \
        do { \
            if (inst->type->dispose) { \
                void *ptr = *(void **)base; \
                inst->type->dispose(call->instance, inst->type, ptr); \
            } \
        } while (false)
#define INTEGER(CType) \
        do { \
            uint64_t ret = *(uint64_t *)base; \
            return NewInt(call->env, (CType)ret); \
        } while (false)
#define INTEGER_SWAP(CType) \
        do { \
            uint64_t ret = *(uint64_t *)base; \
            return NewInt(call->env, ReverseBytes((CType)ret)); \
        } while (false)

    OP(Yield) {
        call->async_ip = inst + 1;
        return nullptr;
    }

    OP(CallG) { CALL(G); return nullptr; }
    OP(CallF) { CALL(F); return nullptr; }
    OP(CallD) { CALL(D); return nullptr; }
    OP(CallGG) { CALL(GG); return nullptr; }
    OP(CallDD) { CALL(DD); return nullptr; }
    OP(CallDG) { CALL(DG); return nullptr; }
    OP(CallGD) { CALL(GD); return nullptr; }
    OP(CallDDDD) { CALL(DDDD); return nullptr; }
    OP(CallMem) {
        uint8_t *ptr = call->AllocHeap(inst->a);
        *(uint8_t **)(base + inst->b1) = ptr;
        CALL(G);
        *(uint8_t **)base = ptr;
        return nullptr;
    }
    OP(CallGX) { CALL(GX); return nullptr; }
    OP(CallFX) { CALL(FX); return nullptr; }
    OP(CallDX) { CALL(DX); return nullptr; }
    OP(CallGGX) { CALL(GGX); return nullptr; }
    OP(CallDDX) { CALL(DDX); return nullptr; }
    OP(CallDGX) { CALL(DGX); return nullptr; }
    OP(CallGDX) { CALL(GDX); return nullptr; }
    OP(CallDDDDX) { CALL(DDDDX); return nullptr; }
    OP(CallMemX) {
        uint8_t *ptr = call->AllocHeap(inst->a);
        *(uint8_t **)(base + inst->b1) = ptr;
        CALL(GX);
        *(uint8_t **)base = ptr;
        return nullptr;
    }

    OP(ReturnVoid) { return nullptr; }
    OP(ReturnBool) {
        uint64_t ret = *(uint64_t *)base;
        return Napi::Boolean::New(call->env, ret & 0x1);
    }
    OP(ReturnInt8) { INTEGER(int8_t); }
    OP(ReturnUInt8) { INTEGER(uint8_t); }
    OP(ReturnInt16) { INTEGER(int16_t); }
    OP(ReturnInt16S) { INTEGER_SWAP(int16_t); }
    OP(ReturnUInt16) { INTEGER(uint16_t); }
    OP(ReturnUInt16S) { INTEGER_SWAP(uint16_t); }
    OP(ReturnInt32) { INTEGER(int32_t); }
    OP(ReturnInt32S) { INTEGER_SWAP(int32_t); }
    OP(ReturnUInt32) { INTEGER(uint32_t); }
    OP(ReturnUInt32S) { INTEGER_SWAP(uint32_t); }
    OP(ReturnInt64) { INTEGER(int64_t); }
    OP(ReturnInt64S) { INTEGER_SWAP(int64_t); }
    OP(ReturnUInt64) { INTEGER(uint64_t); }
    OP(ReturnUInt64S) { INTEGER_SWAP(uint64_t); }
    OP(ReturnString) {
        uint64_t ret = *(uint64_t *)base;
        napi_value value = NewString(call->env, (const char *)ret);
        DISPOSE();
        return value;
    }
    OP(ReturnString16) {
        uint64_t ret = *(uint64_t *)base;
        napi_value value = NewString(call->env, (const char16_t *)ret);
        DISPOSE();
        return value;
    }
    OP(ReturnString32) {
        uint64_t ret = *(uint64_t *)base;
        napi_value value = NewString(call->env, (const char32_t *)ret);
        DISPOSE();
        return value;
    }
    OP(ReturnPointer) {
        uint64_t ret = *(uint64_t *)base;
        napi_value value = WrapPointer(call->env, (void *)ret);
        DISPOSE();
        return value;
    }
    OP(ReturnCallback) {
        uint64_t ret = *(uint64_t *)base;
        return WrapPointer(call->env, (void *)ret);
    }
    OP(ReturnRecord) { K_UNREACHABLE(); return call->env.Null(); }
    OP(ReturnUnion) { K_UNREACHABLE(); return call->env.Null(); }
    OP(ReturnArray) { K_UNREACHABLE(); return call->env.Null(); }
    OP(ReturnFloat32) {
        float f = *(float *)base;
        return NewFloat(call->env, f);
    }
    OP(ReturnFloat64) {
        double d = *(double *)base;
        return NewFloat(call->env, d);
    }
    OP(ReturnPrototype) { K_UNREACHABLE(); return call->env.Null(); }
    OP(ReturnAggregateReg) { return DecodeObject(call->instance, base, inst->type); }
    OP(ReturnAggregateMem) {
        uint64_t ret = *(uint64_t *)base;
        return DecodeObject(call->instance, (const uint8_t *)ret, inst->type);
    }

#undef INTEGER_SWAP
#undef INTEGER
#undef DISPOSE
#undef CALL

#if defined(MUST_TAIL)
    FORCE_INLINE napi_value RunLoop(CallData *call, napi_value *args, uint8_t *base, const AbiInstruction *inst)
    {
        return ((ForwardFunc *)inst->op)(call, args, base, inst);
    }
#else
            }
        }

        K_UNREACHABLE();
    }
#endif

#undef WRAP

#undef NEXT
#undef OP
}

#if defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif

#if defined(MUST_TAIL)

static ForwardFunc *const ForwardDispatch[256] = {
    #define PRIMITIVE(Name) HandlePush ## Name,
    #include "primitives.inc"
    HandlePushAggregateReg,
    HandlePushAggregateSplit,
    HandlePushAggregateStack,
    HandlePushAggregateMem,
    #define PRIMITIVE(Name) HandleRun ## Name,
    #include "primitives.inc"
    HandleRunAggregateG,
    HandleRunAggregateF,
    HandleRunAggregateD,
    HandleRunAggregateGG,
    HandleRunAggregateDD,
    HandleRunAggregateGD,
    HandleRunAggregateDG,
    HandleRunAggregateDDDD,
    HandleRunAggregateMem,
    #define PRIMITIVE(Name) HandleRun ## Name ## X,
    #include "primitives.inc"
    HandleRunAggregateGX,
    HandleRunAggregateFX,
    HandleRunAggregateDX,
    HandleRunAggregateGGX,
    HandleRunAggregateDDX,
    HandleRunAggregateGDX,
    HandleRunAggregateDGX,
    HandleRunAggregateDDDDX,
    HandleRunAggregateMemX,
    HandleYield,
    HandleCallG,
    HandleCallF,
    HandleCallD,
    HandleCallGG,
    HandleCallDD,
    HandleCallDG,
    HandleCallGD,
    HandleCallDDDD,
    HandleCallMem,
    HandleCallGX,
    HandleCallFX,
    HandleCallDX,
    HandleCallGGX,
    HandleCallDDX,
    HandleCallDGX,
    HandleCallGDX,
    HandleCallDDDDX,
    HandleCallMemX,
    #define PRIMITIVE(Name) HandleReturn ## Name,
    #include "primitives.inc"
    HandleReturnAggregateReg,
    HandleReturnAggregateMem
};

void *Code2Op(AbiOpcode code)
{
    return (void *)ForwardDispatch[(int)code];
}

#else

void *Code2Op(AbiOpcode code)
{
    return (void *)code;
}

#endif

napi_value CallData::Run(const FunctionInfo *func, napi_value *args)
{
    uint8_t *base = AllocStack<uint8_t>(func->stk_size);
    if (!base) [[unlikely]]
        return env.Null();

    const AbiInstruction *first = func->sync.ptr;
    return RunLoop(this, args, base, first);
}

bool CallData::PrepareAsync(const FunctionInfo *func, napi_value *args)
{
    uint8_t *base = AllocStack<uint8_t>(func->stk_size);
    if (!base) [[unlikely]]
        return env.Null();
    async_base = base;

    const AbiInstruction *first = func->async.ptr;
    return !RunLoop(this, args, base, first); // Yield returns nullptr
}

void CallData::ExecuteAsync()
{
    const AbiInstruction *next = async_ip++;
    RunLoop(this, nullptr, async_base, next);
}

napi_value CallData::EndAsync()
{
    const AbiInstruction *next = async_ip++;
    return RunLoop(this, nullptr, async_base, next);
}

#if defined(_MSC_VER)
    extern "C" uint64_t WeakForwardCallG(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" float WeakForwardCallF(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" double WeakForwardCallD(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetGG WeakForwardCallGG(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetDD WeakForwardCallDD(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetDG WeakForwardCallDG(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetGD WeakForwardCallGD(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetDDDD WeakForwardCallDDDD(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" uint64_t WeakForwardCallGX(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" float WeakForwardCallFX(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" double WeakForwardCallDX(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetGG WeakForwardCallGGX(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetDD WeakForwardCallDDX(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetDG WeakForwardCallDGX(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetGD WeakForwardCallGDX(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetDDDD WeakForwardCallDDDDX(const void *, uint8_t *, uint8_t **) { abort(); }

    #if defined(_WIN64)
        #pragma comment(linker, "/alternatename:ForwardCallG=WeakForwardCallG")
        #pragma comment(linker, "/alternatename:ForwardCallF=WeakForwardCallF")
        #pragma comment(linker, "/alternatename:ForwardCallD=WeakForwardCallD")
        #pragma comment(linker, "/alternatename:ForwardCallGG=WeakForwardCallGG")
        #pragma comment(linker, "/alternatename:ForwardCallDG=WeakForwardCallDG")
        #pragma comment(linker, "/alternatename:ForwardCallGD=WeakForwardCallGD")
        #pragma comment(linker, "/alternatename:ForwardCallDD=WeakForwardCallDD")
        #pragma comment(linker, "/alternatename:ForwardCallDDDD=WeakForwardCallDDDD")
        #pragma comment(linker, "/alternatename:ForwardCallGX=WeakForwardCallGX")
        #pragma comment(linker, "/alternatename:ForwardCallFX=WeakForwardCallFX")
        #pragma comment(linker, "/alternatename:ForwardCallDX=WeakForwardCallDX")
        #pragma comment(linker, "/alternatename:ForwardCallGGX=WeakForwardCallGGX")
        #pragma comment(linker, "/alternatename:ForwardCallDGX=WeakForwardCallDGX")
        #pragma comment(linker, "/alternatename:ForwardCallGDX=WeakForwardCallGDX")
        #pragma comment(linker, "/alternatename:ForwardCallDDX=WeakForwardCallDDX")
        #pragma comment(linker, "/alternatename:ForwardCallDDDDX=WeakForwardCallDDDDX")
    #else
        #pragma comment(linker, "/alternatename:_ForwardCallG=_WeakForwardCallG")
        #pragma comment(linker, "/alternatename:_ForwardCallF=_WeakForwardCallF")
        #pragma comment(linker, "/alternatename:_ForwardCallD=_WeakForwardCallD")
        #pragma comment(linker, "/alternatename:_ForwardCallGG=_WeakForwardCallGG")
        #pragma comment(linker, "/alternatename:_ForwardCallDD=_WeakForwardCallDD")
        #pragma comment(linker, "/alternatename:_ForwardCallDG=_WeakForwardCallDG")
        #pragma comment(linker, "/alternatename:_ForwardCallGD=_WeakForwardCallGD")
        #pragma comment(linker, "/alternatename:_ForwardCallDDDD=_WeakForwardCallDDDD")
        #pragma comment(linker, "/alternatename:_ForwardCallGX=_WeakForwardCallGX")
        #pragma comment(linker, "/alternatename:_ForwardCallFX=_WeakForwardCallFX")
        #pragma comment(linker, "/alternatename:_ForwardCallDX=_WeakForwardCallDX")
        #pragma comment(linker, "/alternatename:_ForwardCallGGX=_WeakForwardCallGGX")
        #pragma comment(linker, "/alternatename:_ForwardCallDDX=_WeakForwardCallDDX")
        #pragma comment(linker, "/alternatename:_ForwardCallDGX=_WeakForwardCallDGX")
        #pragma comment(linker, "/alternatename:_ForwardCallGDX=_WeakForwardCallGDX")
        #pragma comment(linker, "/alternatename:_ForwardCallDDDDX=_WeakForwardCallDDDDX")
    #endif
#else
    extern "C" uint64_t __attribute__((weak)) ForwardCallG(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" float __attribute__((weak)) ForwardCallF(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" double __attribute__((weak)) ForwardCallD(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetGG __attribute__((weak)) ForwardCallGG(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetDD __attribute__((weak)) ForwardCallDD(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetDG __attribute__((weak)) ForwardCallDG(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetGD __attribute__((weak)) ForwardCallGD(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetDDDD __attribute__((weak)) ForwardCallDDDD(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" uint64_t __attribute__((weak)) ForwardCallGX(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" float __attribute__((weak)) ForwardCallFX(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" double __attribute__((weak)) ForwardCallDX(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetGG __attribute__((weak)) ForwardCallGGX(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetDD __attribute__((weak)) ForwardCallDDX(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetDG __attribute__((weak)) ForwardCallDGX(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetGD __attribute__((weak)) ForwardCallGDX(const void *, uint8_t *, uint8_t **) { abort(); }
    extern "C" RetDDDD __attribute__((weak)) ForwardCallDDDDX(const void *, uint8_t *, uint8_t **) { abort(); }
#endif

}
