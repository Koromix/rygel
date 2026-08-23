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
struct RetDDDD {
    double d0;
    double d1;
    double d2;
    double d3;
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
    #define WEAK_CALL(Ret, Name, ...) __attribute__((weak)) Ret Name(const void *, uint8_t *, uint8_t **) { K_UNREACHABLE(); }
#endif

    WEAK_CALL(uint64_t, CallG)
    WEAK_CALL(float, CallF)
    WEAK_CALL(double, CallD)
    WEAK_CALL(RetGG, CallGG)
    WEAK_CALL(RetDD, CallDD)
    WEAK_CALL(RetGD, CallGD)
    WEAK_CALL(RetDG, CallDG)
    WEAK_CALL(RetDDDD, CallDDDD)
    WEAK_CALL(uint64_t, CallGX)
    WEAK_CALL(float, CallFX)
    WEAK_CALL(double, CallDX)
    WEAK_CALL(RetGG, CallGGX)
    WEAK_CALL(RetDD, CallDDX)
    WEAK_CALL(RetGD, CallGDX)
    WEAK_CALL(RetDG, CallDGX)
    WEAK_CALL(RetDDDD, CallDDDDX)

#undef WEAK_CALL
}

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    #pragma GCC diagnostic ignored "-Wunused-label"
#else
    #pragma warning(push)
    #pragma warning(disable:4102)
#endif

#if defined(MUST_TAIL)
    #define FWD(Code) \
        static PRESERVE_NONE napi_value Forward ## Code(CallData *call, uint8_t *base, void *native, const InstructionData *inst)
    #define NEXT() \
        do { \
            const InstructionData *next = inst + 1; \
            MUST_TAIL return ((ForwardFunc *)next->op)(call, base, native, next); \
        } while (false)
#else
    #define FWD(Code) \
        case (int)Opcode::Code:
    #define NEXT() \
        break

napi_value RunForward(CallData *call, uint8_t *base, void *native, const InstructionData *inst)
{
    for (;; ++inst) {
        switch ((intptr_t)inst->op) {
#endif

#define INTEGER(CType) \
        do { \
            napi_value arg = call->args[inst->a]; \
             \
            CType v; \
            if (!TryNumber(call->env, arg, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, call->args[inst->a])); \
                return call->env.Null(); \
            } \
             \
            *(uintptr_t *)(base + inst->b1) = (uintptr_t)v; \
        } while (false)
#define INTEGER_SWAP(CType) \
        do { \
            napi_value arg = call->args[inst->a]; \
             \
            CType v; \
            if (!TryNumber(call->env, arg, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, call->args[inst->a])); \
                return call->env.Null(); \
            } \
             \
            *(uintptr_t *)(base + inst->b1) = (uintptr_t)ReverseBytes(v); \
        } while (false)
#define INTEGER64(CType) \
        do { \
            napi_value arg = call->args[inst->a]; \
             \
            CType v; \
            if (!TryNumber(call->env, arg, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, call->args[inst->a])); \
                return call->env.Null(); \
            } \
             \
            *(uint64_t *)(base + inst->b1) = (uint64_t)v; \
        } while (false)
#define INTEGER64_SWAP(CType) \
        do { \
            napi_value arg = call->args[inst->a]; \
             \
            CType v; \
            if (!TryNumber(call->env, arg, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, call->args[inst->a])); \
                return call->env.Null(); \
            } \
             \
            *(uint64_t *)(base + inst->b1) = (uint64_t)ReverseBytes(v); \
        } while (false)

    FWD(PushVoid) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(PushBool) {
        napi_value arg = call->args[inst->a];

        bool b;
        if (napi_get_value_bool(call->env, arg, &b) != napi_ok) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected boolean", GetValueType(call->instance, call->args[inst->a]));
            return call->env.Null();
        }

        *(uintptr_t *)(base + inst->b1) = (uintptr_t)b;

        NEXT();
    }
    FWD(PushInt8) { INTEGER(int8_t); NEXT(); }
    FWD(PushUInt8) { INTEGER(uint8_t); NEXT(); }
    FWD(PushInt16) { INTEGER(int16_t); NEXT(); }
    FWD(PushInt16S) { INTEGER_SWAP(int16_t); NEXT(); }
    FWD(PushUInt16) { INTEGER(uint16_t); NEXT(); }
    FWD(PushUInt16S) { INTEGER_SWAP(uint16_t); NEXT(); }
    FWD(PushInt32) { INTEGER(int32_t); NEXT(); }
    FWD(PushInt32S) { INTEGER_SWAP(int32_t); NEXT(); }
    FWD(PushUInt32) { INTEGER(uint32_t); NEXT(); }
    FWD(PushUInt32S) { INTEGER_SWAP(uint32_t); NEXT(); }
    FWD(PushInt64) { INTEGER64(int64_t); NEXT(); }
    FWD(PushInt64S) { INTEGER64_SWAP(int64_t); NEXT(); }
    FWD(PushUInt64) { INTEGER64(int64_t); NEXT(); }
    FWD(PushUInt64S) { INTEGER64_SWAP(int64_t); NEXT(); }
    FWD(PushString) {
        napi_value arg = call->args[inst->a];

        const char *str;
        if (!call->PushString(arg, inst->b2, &str)) [[unlikely]]
            return call->env.Null();

        *(const char **)(base + inst->b1) = str;

        NEXT();
    }
    FWD(PushString16) {
        napi_value arg = call->args[inst->a];

        const char16_t *str16;
        if (!call->PushString16(arg, inst->b2, &str16)) [[unlikely]]
            return call->env.Null();

        *(const char16_t **)(base + inst->b1) = str16;

        NEXT();
    }
    FWD(PushString32) {
        napi_value arg = call->args[inst->a];

        const char32_t *str32;
        if (!call->PushString32(arg, inst->b2, &str32)) [[unlikely]]
            return call->env.Null();

        *(const char32_t **)(base + inst->b1) = str32;

        NEXT();
    }
    FWD(PushPointer) {
        napi_value arg = call->args[inst->a];

        void *ptr;
        if (!call->PushPointer(arg, inst->type, inst->b2, &ptr)) [[unlikely]]
            return call->env.Null();

        *(void **)(base + inst->b1) = ptr;

        NEXT();
    }
    FWD(PushRecord) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(PushUnion) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(PushArray) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(PushFloat32) {
        napi_value arg = call->args[inst->a];

        float f;
        if (!TryNumber(call->env, arg, &f)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, call->args[inst->a]));
            return call->env.Null();
        }

#if K_SIZE_MAX == INT64_MAX
        memset(base + inst->b1, 0xFF, 8);
#endif
        *(float *)(base + inst->b1) = f;

        NEXT();
    }
    FWD(PushFloat64) {
        napi_value arg = call->args[inst->a];

        double d;
        if (!TryNumber(call->env, arg, &d)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, call->args[inst->a]));
            return call->env.Null();
        }

        *(double *)(base + inst->b1) = d;

        NEXT();
    }
    FWD(PushCallback) {
        napi_value arg = call->args[inst->a];

        void *ptr;
        if (!call->PushCallback(arg, inst->type, &ptr)) [[unlikely]]
            return call->env.Null();

        *(void **)(base + inst->b1) = ptr;

        NEXT();
    }
    FWD(PushPrototype) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(PushAggregateReg) {
        napi_value arg = call->args[inst->a];

        uint8_t *ptr = base + inst->b1;

        if (!call->PushObject(arg, inst->type, ptr)) [[unlikely]]
            return call->env.Null();

        NEXT();
    }
    FWD(PushAggregateSplit) {
        napi_value arg = call->args[inst->a];

        uintptr_t buf[2];
        if (!call->PushObject(arg, inst->type, (uint8_t *)buf)) [[unlikely]]
            return call->env.Null();

        // The second part might be useless (if object fits in one register), in
        // which case the analysis code will put the same value in both offsets to
        // make sure we don't overwrite something else. Well, if we copy the second
        // part first, that is, as we do below.
        *(uintptr_t *)(base + inst->b2) = buf[1];
        *(uintptr_t *)(base + inst->b1) = buf[0];

        NEXT();
    }
    FWD(PushAggregateMem) {
        napi_value arg = call->args[inst->a];

        uint8_t *ptr = call->AllocHeap(inst->type->size);
        *(uint8_t **)(base + inst->b1) = ptr;

        if (!call->PushObject(arg, inst->type, ptr)) [[unlikely]]
            return call->env.Null();

        NEXT();
    }

    FWD(PushPair) {
        int *ptr = (int *)(base + inst->a);

        ptr[0] = inst->b1;
        ptr[1] = inst->b2;

        NEXT();
    }

#undef INTEGER64_SWAP
#undef INTEGER64
#undef INTEGER_SWAP
#undef INTEGER

#if defined(_WIN32)
    #define WRAP(Expr) \
        [&]() { \
            call->DebugForward(); \
             \
            TEB *teb = GetTEB(); \
            InstanceMemory *mem = call->mem; \
             \
            K_DEFER { mem->last_error = teb->LastErrorValue; }; \
            teb->LastErrorValue = mem->last_error; \
             \
            ADJUST_TEB(teb, mem->stack0.ptr, mem->stack0.end); \
             \
            return (Expr); \
        }()
#else
    #define WRAP(Expr) (call->DebugForward(), (Expr))
#endif

#define INTEGER(Suffix, CType) \
        do { \
            uint64_t ret = WRAP(Call ## Suffix(native, base, &call->saved_sp)); \
            return NewInt(call->env, (CType)ret); \
        } while (false)
#define INTEGER_SWAP(Suffix, CType) \
        do { \
            uint64_t ret = WRAP(Call ## Suffix(native, base, &call->saved_sp)); \
            return NewInt(call->env, ReverseBytes((CType)ret)); \
        } while (false)
#define DISPOSE(Ptr) \
        do { \
            if (inst->type->dispose) { \
                inst->type->dispose(call->instance, inst->type, (Ptr)); \
            } \
        } while (false)

    FWD(RunVoid) {
        WRAP(CallG(native, base, &call->saved_sp));
        return nullptr;
    }
    FWD(RunBool) {
        uint64_t ret = WRAP(CallG(native, base, &call->saved_sp));
        return Napi::Boolean::New(call->env, ret & 0x1);
    }
    FWD(RunInt8) { INTEGER(G, int8_t); }
    FWD(RunUInt8) { INTEGER(G, uint8_t); }
    FWD(RunInt16) { INTEGER(G, int16_t); }
    FWD(RunInt16S) { INTEGER_SWAP(G, int16_t); }
    FWD(RunUInt16) { INTEGER(G, uint16_t); }
    FWD(RunUInt16S) { INTEGER_SWAP(G, uint16_t); }
    FWD(RunInt32) { INTEGER(G, int32_t); }
    FWD(RunInt32S) { INTEGER_SWAP(G, int32_t); }
    FWD(RunUInt32) { INTEGER(G, uint32_t); }
    FWD(RunUInt32S) { INTEGER_SWAP(G, uint32_t); }
    FWD(RunInt64) { INTEGER(G, int64_t); }
    FWD(RunInt64S) { INTEGER_SWAP(G, int64_t); }
    FWD(RunUInt64) { INTEGER(G, uint64_t); }
    FWD(RunUInt64S) { INTEGER_SWAP(G, uint64_t); }
    FWD(RunString) {
        uint64_t ret = WRAP(CallG(native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunString16) {
        uint64_t ret = WRAP(CallG(native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char16_t *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunString32) {
        uint64_t ret = WRAP(CallG(native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char32_t *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunPointer) {
        uint64_t ret = WRAP(CallG(native, base, &call->saved_sp));
        napi_value value = WrapPointer(call->env, (void *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunCallback) {
        uint64_t ret = WRAP(CallG(native, base, &call->saved_sp));
        return WrapPointer(call->env, (void *)ret);
    }
    FWD(RunRecord) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(RunUnion) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(RunArray) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(RunFloat32) {
        float f = WRAP(CallF(native, base, &call->saved_sp));
        return NewFloat(call->env, f);
    }
    FWD(RunFloat64) {
        double d = WRAP(CallD(native, base, &call->saved_sp));
        return NewFloat(call->env, d);
    }
    FWD(RunPrototype) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(RunAggregateG) {
        auto ret = WRAP(CallG(native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    FWD(RunAggregateF) {
        auto ret = WRAP(CallF(native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    FWD(RunAggregateD) {
        auto ret = WRAP(CallD(native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    FWD(RunAggregateGG) {
        auto ret = WRAP(CallGG(native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    FWD(RunAggregateDD) {
        auto ret = WRAP(CallDD(native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    FWD(RunAggregateGD) {
        auto ret = WRAP(CallGD(native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    FWD(RunAggregateDG) {
        auto ret = WRAP(CallDG(native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    FWD(RunAggregateDDDD) {
        auto ret = WRAP(CallDDDD(native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    FWD(RunAggregateMem) {
        uint8_t *ptr = call->AllocHeap(inst->type->size);
        *(uint8_t **)(base + inst->b1) = ptr;
        WRAP(CallG(native, base, &call->saved_sp));
        return DecodeObject(call->instance, ptr, inst->type);
    }
    FWD(RunVoidX) {
        WRAP(CallGX(native, base, &call->saved_sp));
        return nullptr;
    }
    FWD(RunBoolX) {
        uint64_t ret = WRAP(CallGX(native, base, &call->saved_sp));
        return Napi::Boolean::New(call->env, ret & 0x1);
    }
    FWD(RunInt8X) { INTEGER(GX, int8_t); }
    FWD(RunUInt8X) { INTEGER(GX, uint8_t); }
    FWD(RunInt16X) { INTEGER(GX, int16_t); }
    FWD(RunInt16SX) { INTEGER_SWAP(GX, int16_t); }
    FWD(RunUInt16X) { INTEGER(GX, uint16_t); }
    FWD(RunUInt16SX) { INTEGER_SWAP(GX, uint16_t); }
    FWD(RunInt32X) { INTEGER(GX, int32_t); }
    FWD(RunInt32SX) { INTEGER_SWAP(GX, int32_t); }
    FWD(RunUInt32X) { INTEGER(GX, uint32_t); }
    FWD(RunUInt32SX) { INTEGER_SWAP(GX, uint32_t); }
    FWD(RunInt64X) { INTEGER(GX, int64_t); }
    FWD(RunInt64SX) { INTEGER_SWAP(GX, int64_t); }
    FWD(RunUInt64X) { INTEGER(GX, uint64_t); }
    FWD(RunUInt64SX) { INTEGER_SWAP(GX, uint64_t); }
    FWD(RunStringX) {
        uint64_t ret = WRAP(CallGX(native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunString16X) {
        uint64_t ret = WRAP(CallGX(native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char16_t *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunString32X) {
        uint64_t ret = WRAP(CallGX(native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char32_t *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunPointerX) {
        uint64_t ret = WRAP(CallGX(native, base, &call->saved_sp));
        napi_value value = WrapPointer(call->env, (void *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunCallbackX) {
        uint64_t ret = WRAP(CallGX(native, base, &call->saved_sp));
        return WrapPointer(call->env, (void *)ret);
    }
    FWD(RunRecordX) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(RunUnionX) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(RunArrayX) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(RunFloat32X) {
        float f = WRAP(CallFX(native, base, &call->saved_sp));
        return NewFloat(call->env, f);
    }
    FWD(RunFloat64X) {
        double d = WRAP(CallDX(native, base, &call->saved_sp));
        return NewFloat(call->env, d);
    }
    FWD(RunPrototypeX) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(RunAggregateGX) {
        auto ret = WRAP(CallGX(native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    FWD(RunAggregateFX) {
        auto ret = WRAP(CallFX(native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    FWD(RunAggregateDX) {
        auto ret = WRAP(CallDX(native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    FWD(RunAggregateGGX) {
        auto ret = WRAP(CallGGX(native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    FWD(RunAggregateDDX) {
        auto ret = WRAP(CallDDX(native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    FWD(RunAggregateGDX) {
        auto ret = WRAP(CallGDX(native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    FWD(RunAggregateDGX) {
        auto ret = WRAP(CallDGX(native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    FWD(RunAggregateDDDDX) {
        auto ret = WRAP(CallDDDDX(native, base, &call->saved_sp));
        return DecodeObject(call->instance, (const uint8_t *)&ret, inst->type);
    }
    FWD(RunAggregateMemX) {
        uint8_t *ptr = call->AllocHeap(inst->type->size);
        *(uint8_t **)(base + inst->b1) = ptr;
        WRAP(CallGX(native, base, &call->saved_sp));
        return DecodeObject(call->instance, ptr, inst->type);
    }

#undef DISPOSE
#undef INTEGER_SWAP
#undef INTEGER

#define CALL(Suffix) \
        do { \
            auto ret = WRAP(Call ## Suffix(native, base, &call->saved_sp)); \
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

    FWD(Yield) {
        call->async_ip = inst + 1;
        return nullptr;
    }

    FWD(CallG) { CALL(G); return nullptr; }
    FWD(CallF) { CALL(F); return nullptr; }
    FWD(CallD) { CALL(D); return nullptr; }
    FWD(CallGG) { CALL(GG); return nullptr; }
    FWD(CallDD) { CALL(DD); return nullptr; }
    FWD(CallGD) { CALL(GD); return nullptr; }
    FWD(CallDG) { CALL(DG); return nullptr; }
    FWD(CallDDDD) { CALL(DDDD); return nullptr; }
    FWD(CallMem) {
        uint8_t *ptr = call->AllocHeap(inst->type->size);
        *(uint8_t **)(base + inst->b1) = ptr;
        CALL(G);
        *(uint8_t **)base = ptr;
        return nullptr;
    }
    FWD(CallGX) { CALL(GX); return nullptr; }
    FWD(CallFX) { CALL(FX); return nullptr; }
    FWD(CallDX) { CALL(DX); return nullptr; }
    FWD(CallGGX) { CALL(GGX); return nullptr; }
    FWD(CallDDX) { CALL(DDX); return nullptr; }
    FWD(CallGDX) { CALL(GDX); return nullptr; }
    FWD(CallDGX) { CALL(DGX); return nullptr; }
    FWD(CallDDDDX) { CALL(DDDDX); return nullptr; }
    FWD(CallMemX) {
        uint8_t *ptr = call->AllocHeap(inst->type->size);
        *(uint8_t **)(base + inst->b1) = ptr;
        CALL(GX);
        *(uint8_t **)base = ptr;
        return nullptr;
    }

    FWD(ReturnVoid) { return nullptr; }
    FWD(ReturnBool) {
        uintptr_t ret = *(uintptr_t *)base;
        return Napi::Boolean::New(call->env, ret & 0x1);
    }
    FWD(ReturnInt8) { INTEGER(int8_t); }
    FWD(ReturnUInt8) { INTEGER(uint8_t); }
    FWD(ReturnInt16) { INTEGER(int16_t); }
    FWD(ReturnInt16S) { INTEGER_SWAP(int16_t); }
    FWD(ReturnUInt16) { INTEGER(uint16_t); }
    FWD(ReturnUInt16S) { INTEGER_SWAP(uint16_t); }
    FWD(ReturnInt32) { INTEGER(int32_t); }
    FWD(ReturnInt32S) { INTEGER_SWAP(int32_t); }
    FWD(ReturnUInt32) { INTEGER(uint32_t); }
    FWD(ReturnUInt32S) { INTEGER_SWAP(uint32_t); }
    FWD(ReturnInt64) { INTEGER(int64_t); }
    FWD(ReturnInt64S) { INTEGER_SWAP(int64_t); }
    FWD(ReturnUInt64) { INTEGER(uint64_t); }
    FWD(ReturnUInt64S) { INTEGER_SWAP(uint64_t); }
    FWD(ReturnString) {
        uintptr_t ret = *(uintptr_t *)base;
        napi_value value = NewString(call->env, (const char *)ret);
        DISPOSE();
        return value;
    }
    FWD(ReturnString16) {
        uintptr_t ret = *(uintptr_t *)base;
        napi_value value = NewString(call->env, (const char16_t *)ret);
        DISPOSE();
        return value;
    }
    FWD(ReturnString32) {
        uintptr_t ret = *(uintptr_t *)base;
        napi_value value = NewString(call->env, (const char32_t *)ret);
        DISPOSE();
        return value;
    }
    FWD(ReturnPointer) {
        uintptr_t ret = *(uintptr_t *)base;
        napi_value value = WrapPointer(call->env, (void *)ret);
        DISPOSE();
        return value;
    }
    FWD(ReturnCallback) {
        uintptr_t ret = *(uintptr_t *)base;
        return WrapPointer(call->env, (void *)ret);
    }
    FWD(ReturnRecord) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(ReturnUnion) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(ReturnArray) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(ReturnFloat32) {
        float f = *(float *)base;
        return NewFloat(call->env, f);
    }
    FWD(ReturnFloat64) {
        double d = *(double *)base;
        return NewFloat(call->env, d);
    }
    FWD(ReturnPrototype) { K_UNREACHABLE(); return call->env.Null(); }
    FWD(ReturnAggregateReg) { return DecodeObject(call->instance, base, inst->type); }
    FWD(ReturnAggregateMem) {
        uintptr_t ret = *(uintptr_t *)base;
        return DecodeObject(call->instance, (const uint8_t *)ret, inst->type);
    }

#undef INTEGER_SWAP
#undef INTEGER
#undef DISPOSE
#undef CALL

#if !defined(MUST_TAIL)
        }
    }

    K_UNREACHABLE();
}
#endif

#undef WRAP
#undef NEXT
#undef FWD

#if defined(MUST_TAIL)
    #define RELAY(Code) \
        static PRESERVE_NONE int Relay ## Code(CallData *call, TrampolineInfo *trampoline, uint8_t *base, const InstructionData *inst)
    #define NEXT() \
        do { \
            const InstructionData *next = inst + 1; \
            MUST_TAIL return ((RelayFunc *)next->op)(call, trampoline, base, next); \
        } while (false)
    #define ALIAS(From, To) \
        RelayFunc *const Relay ## From = Relay ## To;
#else
    #define RELAY(Code) \
        Code: \
        case (int)Opcode::Code:
    #define NEXT() \
        break
    #define ALIAS(From, To) \
        RELAY(From) { goto To; }

int RunRelay(CallData *call, TrampolineInfo *trampoline, uint8_t *base, const InstructionData *inst)
{
    for (;; ++inst) {
        switch ((intptr_t)inst->op) {
#endif

#define INTEGER(CType) \
        do { \
            const uint8_t *src = base + inst->b1; \
            CType v = *(const CType *)src; \
             \
            call->args[inst->a] = NewInt(trampoline->env, v); \
        } while (false)
#define INTEGER_SWAP(CType) \
        do { \
            const uint8_t *src = base + inst->b1; \
            CType v = *(const CType *)src; \
             \
            call->args[inst->a] = NewInt(trampoline->env, ReverseBytes(v)); \
        } while (false)
#define DISPOSE(Ptr) \
        do { \
            if (inst->type->dispose) { \
                inst->type->dispose(trampoline->instance, inst->type, (Ptr)); \
            } \
        } while (false)

    RELAY(PushVoid) { K_UNREACHABLE(); }
    RELAY(PushBool) {
        const uint8_t *src = base + inst->b1;
        call->args[inst->a] = Napi::Boolean::New(trampoline->env, *(bool *)src);

        NEXT();
    }
    RELAY(PushInt8) { INTEGER(int8_t); NEXT(); }
    RELAY(PushUInt8) { INTEGER(uint8_t); NEXT(); }
    RELAY(PushInt16) { INTEGER(int16_t); NEXT(); }
    RELAY(PushInt16S) { INTEGER_SWAP(int16_t); NEXT(); }
    RELAY(PushUInt16) { INTEGER(uint16_t); NEXT(); }
    RELAY(PushUInt16S) { INTEGER_SWAP(uint16_t); NEXT(); }
    RELAY(PushInt32) { INTEGER(int32_t); NEXT(); }
    RELAY(PushInt32S) { INTEGER_SWAP(int32_t); NEXT(); }
    RELAY(PushUInt32) { INTEGER(uint32_t); NEXT(); }
    RELAY(PushUInt32S) { INTEGER_SWAP(uint32_t); NEXT(); }
    RELAY(PushInt64) { INTEGER(int64_t); NEXT(); }
    RELAY(PushInt64S) { INTEGER_SWAP(int64_t); NEXT(); }
    RELAY(PushUInt64) { INTEGER(int64_t); NEXT(); }
    RELAY(PushUInt64S) { INTEGER_SWAP(int64_t); NEXT(); }
    RELAY(PushString) {
        const uint8_t *src = base + inst->b1;
        const char *str = *(const char **)src;

        call->args[inst->a] = NewString(trampoline->env, str);
        DISPOSE(str);

        NEXT();
    }
    RELAY(PushString16) {
        const uint8_t *src = base + inst->b1;
        const char16_t *str16 = *(const char16_t **)src;

        call->args[inst->a] = NewString(trampoline->env, str16);
        DISPOSE(str16);

        NEXT();
    }
    RELAY(PushString32) {
        const uint8_t *src = base + inst->b1;
        const char32_t *str32 = *(const char32_t **)src;

        call->args[inst->a] = NewString(trampoline->env, str32);
        DISPOSE(str32);

        NEXT();
    }
    RELAY(PushPointer) {
        const uint8_t *src = base + inst->b1;
        void *ptr2 = *(void **)src;

        call->args[inst->a] = WrapPointer(trampoline->env, ptr2);
        DISPOSE(ptr2);

        NEXT();
    }
    RELAY(PushRecord) { K_UNREACHABLE(); }
    RELAY(PushUnion) { K_UNREACHABLE(); }
    RELAY(PushArray) { K_UNREACHABLE(); }
    RELAY(PushFloat32) {
        const uint8_t *src = base + inst->b1;
        call->args[inst->a] = NewFloat(trampoline->env, *(float *)src);

        NEXT();
    }
    RELAY(PushFloat64) {
        const uint8_t *src = base + inst->b1;
        call->args[inst->a] = NewFloat(trampoline->env, *(double *)src);

        NEXT();
    }
    RELAY(PushCallback) {
        const uint8_t *src = base + inst->b1;
        call->args[inst->a] = WrapPointer(trampoline->env, *(void **)src);

        NEXT();
    }
    RELAY(PushPrototype) { K_UNREACHABLE(); }
    RELAY(PushAggregateReg) {
        const uint8_t *src = base + inst->b1;
        call->args[inst->a] = DecodeObject(trampoline->instance, src, inst->type);

        NEXT();
    }
    RELAY(PushAggregateSplit) {
        uintptr_t buf[2] = {
            *(uintptr_t *)(base + inst->b1),
            *(uintptr_t *)(base + inst->b2)
        };
        call->args[inst->a] = DecodeObject(trampoline->instance, (const uint8_t *)buf, inst->type);

        NEXT();
    }
    RELAY(PushAggregateMem) {
        const uint8_t *src = *(const uint8_t **)(base + inst->b1);
        call->args[inst->a] = DecodeObject(trampoline->instance, src, inst->type);

        NEXT();
    }

    RELAY(PushPair) {
        int *ptr = (int *)(base + inst->a);

        ptr[0] = inst->b1;
        ptr[1] = inst->b2;

        NEXT();
    }

#undef DISPOSE
#undef INTEGER_SWAP
#undef INTEGER

#define INTEGER(Suffix, CType) \
        do { \
            napi_value value = call->CallCallback(trampoline, call->args, inst->a); \
            if (!value) [[unlikely]] \
                return -1; \
             \
            CType v; \
            if (!TryNumber(trampoline->env, value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(trampoline->env, "Unexpected %1 value, expected number", GetValueType(trampoline->instance, value)); \
                return -1; \
            } \
             \
            *(uintptr_t *)(base + inst->b1) = (uintptr_t)v; \
            return 1; \
        } while (false)
#define INTEGER_SWAP(Suffix, CType) \
        do { \
            napi_value value = call->CallCallback(trampoline, call->args, inst->a); \
            if (!value) [[unlikely]] \
                return -1; \
             \
            CType v; \
            if (!TryNumber(trampoline->env, value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(trampoline->env, "Unexpected %1 value, expected number", GetValueType(trampoline->instance, value)); \
                return -1; \
            } \
             \
            *(uintptr_t *)(base + inst->b1) = (uintptr_t)ReverseBytes(v); \
            return 1; \
        } while (false)
#define INTEGER64(Suffix, CType) \
        do { \
            napi_value value = call->CallCallback(trampoline, call->args, inst->a); \
            if (!value) [[unlikely]] \
                return -1; \
             \
            CType v; \
            if (!TryNumber(trampoline->env, value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(trampoline->env, "Unexpected %1 value, expected number", GetValueType(trampoline->instance, value)); \
                return -1; \
            } \
             \
            *(uint64_t *)(base + inst->b1) = (uint64_t)v; \
            return 1; \
        } while (false)
#define INTEGER64_SWAP(Suffix, CType) \
        do { \
            napi_value value = call->CallCallback(trampoline, call->args, inst->a); \
            if (!value) [[unlikely]] \
                return -1; \
             \
            CType v; \
            if (!TryNumber(trampoline->env, value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(trampoline->env, "Unexpected %1 value, expected number", GetValueType(trampoline->instance, value)); \
                return -1; \
            } \
             \
            *(uint64_t *)(base + inst->b1) = (uint64_t)ReverseBytes(v); \
            return 1; \
        } while (false)

    RELAY(RunVoid) {
        call->CallCallback(trampoline, call->args, inst->a);
        return 1;
    }
    RELAY(RunBool) {
        napi_value value = call->CallCallback(trampoline, call->args, inst->a);
        bool b;

        if (napi_get_value_bool(trampoline->env, value, &b) == napi_boolean_expected) [[unlikely]] {
            ThrowError<Napi::TypeError>(trampoline->env, "Unexpected %1 value, expected boolean", GetValueType(trampoline->instance, value));
            return -1;
        }

        *(uintptr_t *)(base + inst->b1) = (uintptr_t)b;
        return 1;
    }
    RELAY(RunInt8) { INTEGER(G, int8_t); }
    RELAY(RunUInt8) { INTEGER(G, uint8_t); }
    RELAY(RunInt16) { INTEGER(G, int16_t); }
    RELAY(RunInt16S) { INTEGER_SWAP(G, int16_t); }
    RELAY(RunUInt16) { INTEGER(G, uint16_t); }
    RELAY(RunUInt16S) { INTEGER_SWAP(G, uint16_t); }
    RELAY(RunInt32) { INTEGER(G, int32_t); }
    RELAY(RunInt32S) { INTEGER_SWAP(G, int32_t); }
    RELAY(RunUInt32) { INTEGER(G, uint32_t); }
    RELAY(RunUInt32S) { INTEGER_SWAP(G, uint32_t); }
    RELAY(RunInt64) { INTEGER64(G, int64_t); }
    RELAY(RunInt64S) { INTEGER64_SWAP(G, int64_t); }
    RELAY(RunUInt64) { INTEGER64(G, uint64_t); }
    RELAY(RunUInt64S) { INTEGER64_SWAP(G, uint64_t); }
    RELAY(RunString) {
        napi_value value = call->CallCallback(trampoline, call->args, inst->a);
        const char *str;

        if (!value) [[unlikely]]
            return -1;
        if (!call->PushString(value, 1, &str)) [[unlikely]]
            return -1;

        *(uintptr_t *)(base + inst->b1) = (uintptr_t)str;
        return 1;
    }
    RELAY(RunString16) {
        napi_value value = call->CallCallback(trampoline, call->args, inst->a);
        const char16_t *str16;

        if (!value) [[unlikely]]
            return -1;
        if (!call->PushString16(value, 1, &str16)) [[unlikely]]
            return -1;

        *(uintptr_t *)(base + inst->b1) = (uintptr_t)str16;
        return 1;
    }
    RELAY(RunString32) {
        napi_value value = call->CallCallback(trampoline, call->args, inst->a);
        const char32_t *str32;

        if (!value) [[unlikely]]
            return -1;
        if (!call->PushString32(value, 1, &str32)) [[unlikely]]
            return -1;

        *(uintptr_t *)(base + inst->b1) = (uintptr_t)str32;
        return 1;
    }
    RELAY(RunPointer) {
        napi_value value = call->CallCallback(trampoline, call->args, inst->a);
        void *ptr;

        if (!value) [[unlikely]]
            return -1;
        if (!call->PushPointer(value, inst->type, 1, &ptr)) [[unlikely]]
            return -1;

        *(uintptr_t *)(base + inst->b1) = (uintptr_t)ptr;
        return 1;
    }
    RELAY(RunCallback) {
        napi_value value = call->CallCallback(trampoline, call->args, inst->a);
        void *ptr;

        if (!value) [[unlikely]]
            return -1;
        if (!call->PushCallback(value, inst->type, &ptr)) [[unlikely]]
            return -1;

        *(uintptr_t *)(base + inst->b1) = (uintptr_t)ptr;
        return 1;
    }
    RELAY(RunRecord) { K_UNREACHABLE(); }
    RELAY(RunUnion) { K_UNREACHABLE(); }
    RELAY(RunArray) { K_UNREACHABLE(); }
    RELAY(RunFloat32) {
        napi_value value = call->CallCallback(trampoline, call->args, inst->a);
        float f;

        if (!value) [[unlikely]]
            return -1;
        if (!TryNumber(trampoline->env, value, &f)) [[unlikely]] {
            ThrowError<Napi::TypeError>(trampoline->env, "Unexpected %1 value, expected number", GetValueType(trampoline->instance, value));
            return -1;
        }

#if K_SIZE_MAX == INT64_MAX
        memset(base + inst->b1, 0xFF, 8);
#endif
        *(float *)(base + inst->b1) = f;

        return 1;
    }
    RELAY(RunFloat64) {
        napi_value value = call->CallCallback(trampoline, call->args, inst->a);
        double d;

        if (!value) [[unlikely]]
            return -1;
        if (!TryNumber(trampoline->env, value, &d)) [[unlikely]] {
            ThrowError<Napi::TypeError>(trampoline->env, "Unexpected %1 value, expected number", GetValueType(trampoline->instance, value));
            return -1;
        }

        *(double *)(base + inst->b1) = d;
        return 1;
    }
    RELAY(RunPrototype) { K_UNREACHABLE(); }
    RELAY(RunAggregateG) {
        napi_value value = call->CallCallback(trampoline, call->args, inst->a);

        if (!value) [[unlikely]]
            return -1;
        if (!call->PushObject(value, inst->type, base + inst->b1)) [[unlikely]]
            return -1;

        return 1;
    }
    ALIAS(RunAggregateF, RunAggregateG)
    ALIAS(RunAggregateD, RunAggregateG)
    ALIAS(RunAggregateGG, RunAggregateG)
    ALIAS(RunAggregateDD, RunAggregateG)
    RELAY(RunAggregateGD) {
        napi_value value = call->CallCallback(trampoline, call->args, inst->a);
        uintptr_t buf[2] = {};

        if (!value) [[unlikely]]
            return -1;
        if (!call->PushObject(value, inst->type, (uint8_t *)buf)) [[unlikely]]
            return -1;

        memcpy(base + inst->b1, buf + 0, K_SIZE(uintptr_t));
        memcpy(base + inst->b2, buf + 1, K_SIZE(uintptr_t));

        return 1;
    }
    ALIAS(RunAggregateDG, RunAggregateGD)
    ALIAS(RunAggregateDDDD, RunAggregateG)
    RELAY(RunAggregateMem) {
        napi_value value = call->CallCallback(trampoline, call->args, inst->a);
        uint8_t *ptr = *(uint8_t **)(base + inst->b1);

        if (!value) [[unlikely]]
            return -1;
        if (!call->PushObject(value, inst->type, ptr)) [[unlikely]]
            return -1;

        *(uintptr_t *)(base + inst->b2) = (uintptr_t)ptr;
        return 1;
    }

#undef INTEGER64_SWAP
#undef INTEGER64
#undef INTEGER_SWAP
#undef INTEGER

    ALIAS(RunVoidX, RunVoid)
    ALIAS(RunBoolX, RunBool)
    ALIAS(RunInt8X, RunInt8)
    ALIAS(RunUInt8X, RunUInt8)
    ALIAS(RunInt16X, RunInt16)
    ALIAS(RunInt16SX, RunInt16S)
    ALIAS(RunUInt16X, RunUInt16)
    ALIAS(RunUInt16SX, RunUInt16S)
    ALIAS(RunInt32X, RunInt32)
    ALIAS(RunInt32SX, RunInt32S)
    ALIAS(RunUInt32X, RunUInt32)
    ALIAS(RunUInt32SX, RunUInt32S)
    ALIAS(RunInt64X, RunInt64)
    ALIAS(RunInt64SX, RunInt64S)
    ALIAS(RunUInt64X, RunUInt64)
    ALIAS(RunUInt64SX, RunUInt64S)
    ALIAS(RunStringX, RunString)
    ALIAS(RunString16X, RunString16)
    ALIAS(RunString32X, RunString32)
    ALIAS(RunPointerX, RunPointer)
    ALIAS(RunCallbackX, RunCallback)
    ALIAS(RunRecordX, RunRecord)
    ALIAS(RunUnionX, RunUnion)
    ALIAS(RunArrayX, RunArray)
    ALIAS(RunFloat32X, RunFloat32)
    ALIAS(RunFloat64X, RunFloat64)
    ALIAS(RunPrototypeX, RunPrototype)
    ALIAS(RunAggregateGX, RunAggregateG)
    ALIAS(RunAggregateFX, RunAggregateF)
    ALIAS(RunAggregateDX, RunAggregateD)
    ALIAS(RunAggregateGGX, RunAggregateGG)
    ALIAS(RunAggregateDDX, RunAggregateDD)
    ALIAS(RunAggregateGDX, RunAggregateGD)
    ALIAS(RunAggregateDGX, RunAggregateDG)
    ALIAS(RunAggregateDDDDX, RunAggregateDDDD)
    ALIAS(RunAggregateMemX, RunAggregateMem)

    RELAY(Yield) { K_UNREACHABLE(); }

    RELAY(CallG) { K_UNREACHABLE(); }
    RELAY(CallF) { K_UNREACHABLE(); }
    RELAY(CallD) { K_UNREACHABLE(); }
    RELAY(CallGG) { K_UNREACHABLE(); }
    RELAY(CallDD) { K_UNREACHABLE(); }
    RELAY(CallGD) { K_UNREACHABLE(); }
    RELAY(CallDG) { K_UNREACHABLE(); }
    RELAY(CallDDDD) { K_UNREACHABLE(); }
    RELAY(CallMem) { K_UNREACHABLE(); }
    RELAY(CallGX) { K_UNREACHABLE(); }
    RELAY(CallFX) { K_UNREACHABLE(); }
    RELAY(CallDX) { K_UNREACHABLE(); }
    RELAY(CallGGX) { K_UNREACHABLE(); }
    RELAY(CallDDX) { K_UNREACHABLE(); }
    RELAY(CallGDX) { K_UNREACHABLE(); }
    RELAY(CallDGX) { K_UNREACHABLE(); }
    RELAY(CallDDDDX) { K_UNREACHABLE(); }
    RELAY(CallMemX) { K_UNREACHABLE(); }

    RELAY(ReturnVoid) { K_UNREACHABLE(); }
    RELAY(ReturnBool) { K_UNREACHABLE(); }
    RELAY(ReturnInt8) { K_UNREACHABLE(); }
    RELAY(ReturnUInt8) { K_UNREACHABLE(); }
    RELAY(ReturnInt16) { K_UNREACHABLE(); }
    RELAY(ReturnInt16S) { K_UNREACHABLE(); }
    RELAY(ReturnUInt16) { K_UNREACHABLE(); }
    RELAY(ReturnUInt16S) { K_UNREACHABLE(); }
    RELAY(ReturnInt32) { K_UNREACHABLE(); }
    RELAY(ReturnInt32S) { K_UNREACHABLE(); }
    RELAY(ReturnUInt32) { K_UNREACHABLE(); }
    RELAY(ReturnUInt32S) { K_UNREACHABLE(); }
    RELAY(ReturnInt64) { K_UNREACHABLE(); }
    RELAY(ReturnInt64S) { K_UNREACHABLE(); }
    RELAY(ReturnUInt64) { K_UNREACHABLE(); }
    RELAY(ReturnUInt64S) { K_UNREACHABLE(); }
    RELAY(ReturnString) { K_UNREACHABLE(); }
    RELAY(ReturnString16) { K_UNREACHABLE(); }
    RELAY(ReturnString32) { K_UNREACHABLE(); }
    RELAY(ReturnPointer) { K_UNREACHABLE(); }
    RELAY(ReturnCallback) { K_UNREACHABLE(); }
    RELAY(ReturnRecord) { K_UNREACHABLE(); }
    RELAY(ReturnUnion) { K_UNREACHABLE(); }
    RELAY(ReturnArray) { K_UNREACHABLE(); }
    RELAY(ReturnFloat32) { K_UNREACHABLE(); }
    RELAY(ReturnFloat64) { K_UNREACHABLE(); }
    RELAY(ReturnPrototype) { K_UNREACHABLE(); }
    RELAY(ReturnAggregateReg) { K_UNREACHABLE(); }
    RELAY(ReturnAggregateMem) { K_UNREACHABLE(); }

#if !defined(MUST_TAIL)
        }
    }

    K_UNREACHABLE();
}
#endif

#undef ALIAS
#undef NEXT
#undef RELAY

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic pop
#else
    #pragma warning(pop)
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
    K_ASSERT(func->plan.async.len);
    K_ASSERT(func->plan.relay.len);
    K_ASSERT(func->plan.stk_size);

#if defined(MUST_TAIL)
    static ForwardFunc *const ForwardDispatch[256] = {
        #define PRIMITIVE(Name) ForwardPush ## Name,
        #include "primitives.inc"
        ForwardPushAggregateReg,
        ForwardPushAggregateSplit,
        ForwardPushAggregateMem,
        ForwardPushPair,
        #define PRIMITIVE(Name) ForwardRun ## Name,
        #include "primitives.inc"
        ForwardRunAggregateG,
        ForwardRunAggregateF,
        ForwardRunAggregateD,
        ForwardRunAggregateGG,
        ForwardRunAggregateDD,
        ForwardRunAggregateGD,
        ForwardRunAggregateDG,
        ForwardRunAggregateDDDD,
        ForwardRunAggregateMem,
        #define PRIMITIVE(Name) ForwardRun ## Name ## X,
        #include "primitives.inc"
        ForwardRunAggregateGX,
        ForwardRunAggregateFX,
        ForwardRunAggregateDX,
        ForwardRunAggregateGGX,
        ForwardRunAggregateDDX,
        ForwardRunAggregateGDX,
        ForwardRunAggregateDGX,
        ForwardRunAggregateDDDDX,
        ForwardRunAggregateMemX,
        ForwardYield,
        ForwardCallG,
        ForwardCallF,
        ForwardCallD,
        ForwardCallGG,
        ForwardCallDD,
        ForwardCallGD,
        ForwardCallDG,
        ForwardCallDDDD,
        ForwardCallMem,
        ForwardCallGX,
        ForwardCallFX,
        ForwardCallDX,
        ForwardCallGGX,
        ForwardCallDDX,
        ForwardCallGDX,
        ForwardCallDGX,
        ForwardCallDDDDX,
        ForwardCallMemX,
        #define PRIMITIVE(Name) ForwardReturn ## Name,
        #include "primitives.inc"
        ForwardReturnAggregateReg,
        ForwardReturnAggregateMem
    };

    static RelayFunc *const RelayDispatch[256] = {
        #define PRIMITIVE(Name) RelayPush ## Name,
        #include "primitives.inc"
        RelayPushAggregateReg,
        RelayPushAggregateSplit,
        RelayPushAggregateMem,
        RelayPushPair,
        #define PRIMITIVE(Name) RelayRun ## Name,
        #include "primitives.inc"
        RelayRunAggregateG,
        RelayRunAggregateF,
        RelayRunAggregateD,
        RelayRunAggregateGG,
        RelayRunAggregateDD,
        RelayRunAggregateGD,
        RelayRunAggregateDG,
        RelayRunAggregateDDDD,
        RelayRunAggregateMem,
        #define PRIMITIVE(Name) RelayRun ## Name ## X,
        #include "primitives.inc"
        RelayRunAggregateGX,
        RelayRunAggregateFX,
        RelayRunAggregateDX,
        RelayRunAggregateGGX,
        RelayRunAggregateDDX,
        RelayRunAggregateGDX,
        RelayRunAggregateDGX,
        RelayRunAggregateDDDDX,
        RelayRunAggregateMemX,
        RelayYield,
        RelayCallG,
        RelayCallF,
        RelayCallD,
        RelayCallGG,
        RelayCallDD,
        RelayCallGD,
        RelayCallDG,
        RelayCallDDDD,
        RelayCallMem,
        RelayCallGX,
        RelayCallFX,
        RelayCallDX,
        RelayCallGGX,
        RelayCallDDX,
        RelayCallGDX,
        RelayCallDGX,
        RelayCallDDDDX,
        RelayCallMemX,
        #define PRIMITIVE(Name) RelayReturn ## Name,
        #include "primitives.inc"
        RelayReturnAggregateReg,
        RelayReturnAggregateMem
    };

    for (InstructionData &inst: func->plan.sync) {
        inst.op = (void *)ForwardDispatch[(uintptr_t)inst.op];
    }
    for (InstructionData &inst: func->plan.async) {
        inst.op = (void *)ForwardDispatch[(uintptr_t)inst.op];
    }
    for (InstructionData &inst: func->plan.relay) {
        inst.op = (void *)RelayDispatch[(uintptr_t)inst.op];
    }
#endif

    return true;
}

void FillAsyncPlan(Span<const InstructionData> sync, HeapArray<InstructionData> *out_async)
{
    for (const InstructionData &inst: sync) {
        switch ((Opcode)(intptr_t)inst.op) {
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
            case Opcode::PushAggregateSplit:
            case Opcode::PushAggregateMem:
            case Opcode::PushPair: { out_async->Append(inst); } break;

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
                Opcode ret = (Opcode)((intptr_t)inst.op + delta);

                out_async->Append({ .op = Code2Op(Opcode::Yield) });
                out_async->Append({ .op = Code2Op(Opcode::CallG) });
                out_async->Append({ .op = Code2Op(ret), .type = inst.type });
            } break;
            case Opcode::RunRecord:
            case Opcode::RunUnion:
            case Opcode::RunArray: { K_UNREACHABLE(); } break;
            case Opcode::RunFloat32: {
                out_async->Append({ .op = Code2Op(Opcode::Yield) });
                out_async->Append({ .op = Code2Op(Opcode::CallF) });
                out_async->Append({ .op = Code2Op(Opcode::ReturnFloat32), .type = inst.type });
            } break;
            case Opcode::RunFloat64: {
                out_async->Append({ .op = Code2Op(Opcode::Yield) });
                out_async->Append({ .op = Code2Op(Opcode::CallD) });
                out_async->Append({ .op = Code2Op(Opcode::ReturnFloat64), .type = inst.type });
            } break;
            case Opcode::RunPrototype: { K_UNREACHABLE(); } break;
            case Opcode::RunAggregateG:
            case Opcode::RunAggregateF:
            case Opcode::RunAggregateD:
            case Opcode::RunAggregateGG:
            case Opcode::RunAggregateDD:
            case Opcode::RunAggregateGD:
            case Opcode::RunAggregateDG:
            case Opcode::RunAggregateDDDD: {
                int delta = (int)Opcode::CallG - (int)Opcode::RunAggregateG;
                Opcode call = (Opcode)((intptr_t)inst.op + delta);

                out_async->Append({ .op = Code2Op(Opcode::Yield) });
                out_async->Append({ .op = Code2Op(call) });
                out_async->Append({ .op = Code2Op(Opcode::ReturnAggregateReg), .type = inst.type });
            } break;
            case Opcode::RunAggregateMem: {
                out_async->Append({ .op = Code2Op(Opcode::Yield) });
                out_async->Append({ .op = Code2Op(Opcode::CallMem), .b1 = inst.b1, .type = inst.type });
                out_async->Append({ .op = Code2Op(Opcode::ReturnAggregateMem), .type = inst.type });
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
                Opcode ret = (Opcode)((intptr_t)inst.op + delta);

                out_async->Append({ .op = Code2Op(Opcode::Yield) });
                out_async->Append({ .op = Code2Op(Opcode::CallGX) });
                out_async->Append({ .op = Code2Op(ret), .type = inst.type });
            } break;
            case Opcode::RunRecordX:
            case Opcode::RunUnionX:
            case Opcode::RunArrayX: { K_UNREACHABLE(); } break;
            case Opcode::RunFloat32X: {
                out_async->Append({ .op = Code2Op(Opcode::Yield) });
                out_async->Append({ .op = Code2Op(Opcode::CallFX) });
                out_async->Append({ .op = Code2Op(Opcode::ReturnFloat32), .type = inst.type });
            } break;
            case Opcode::RunFloat64X: {
                out_async->Append({ .op = Code2Op(Opcode::Yield) });
                out_async->Append({ .op = Code2Op(Opcode::CallDX) });
                out_async->Append({ .op = Code2Op(Opcode::ReturnFloat64), .type = inst.type });
            } break;
            case Opcode::RunPrototypeX: { K_UNREACHABLE(); } break;
            case Opcode::RunAggregateGX:
            case Opcode::RunAggregateFX:
            case Opcode::RunAggregateDX:
            case Opcode::RunAggregateGGX:
            case Opcode::RunAggregateDDX:
            case Opcode::RunAggregateGDX:
            case Opcode::RunAggregateDGX:
            case Opcode::RunAggregateDDDDX: {
                int delta = (int)Opcode::CallGX - (int)Opcode::RunAggregateGX;
                Opcode call = (Opcode)((intptr_t)inst.op + delta);

                out_async->Append({ .op = Code2Op(Opcode::Yield) });
                out_async->Append({ .op = Code2Op(call) });
                out_async->Append({ .op = Code2Op(Opcode::ReturnAggregateReg), .type = inst.type });
            } break;
            case Opcode::RunAggregateMemX: {
                out_async->Append({ .op = Code2Op(Opcode::Yield) });
                out_async->Append({ .op = Code2Op(Opcode::CallMemX), .b1 = inst.b1, .type = inst.type });
                out_async->Append({ .op = Code2Op(Opcode::ReturnAggregateMem), .type = inst.type });
            } break;

            case Opcode::Yield: { K_UNREACHABLE(); } break;

            case Opcode::CallG:
            case Opcode::CallF:
            case Opcode::CallD:
            case Opcode::CallGG:
            case Opcode::CallDD:
            case Opcode::CallGD:
            case Opcode::CallDG:
            case Opcode::CallDDDD:
            case Opcode::CallMem:
            case Opcode::CallGX:
            case Opcode::CallFX:
            case Opcode::CallDX:
            case Opcode::CallGGX:
            case Opcode::CallDDX:
            case Opcode::CallGDX:
            case Opcode::CallDGX:
            case Opcode::CallDDDDX:
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
}

static int AnalyseFlatRec(const TypeInfo *type, int offset, int count, FunctionRef<void(const TypeInfo *type, int offset, int count)> func)
{
    if (type->primitive == PrimitiveKind::Record) {
        for (int i = 0; i < count; i++) {
            for (const RecordMember &member: type->members) {
                offset = AnalyseFlatRec(member.type, offset, 1, func);
            }
        }
    } else if (type->primitive == PrimitiveKind::Union) {
        for (int i = 0; i < count; i++) {
            for (const RecordMember &member: type->members) {
                AnalyseFlatRec(member.type, offset, 1, func);
            }
        }
        offset += count;
    } else if (type->primitive == PrimitiveKind::Array) {
        count *= type->size / type->ref.type->size;
        offset = AnalyseFlatRec(type->ref.type, offset, count, func);
    } else {
        func(type, offset, count);
        offset += count;
    }

    return offset;
}

int AnalyseFlat(const TypeInfo *type, FunctionRef<void(const TypeInfo *type, int offset, int count)> func)
{
    return AnalyseFlatRec(type, 0, 1, func);
}

}
