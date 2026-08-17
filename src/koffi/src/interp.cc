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

#if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

namespace {
#if defined(MUST_TAIL)
    PRESERVE_NONE typedef napi_value ForwardFunc(CallData *call, uint8_t *base, void *native, const InstructionData *inst);

    #define FWD(Code) \
        PRESERVE_NONE napi_value Forward ## Code(CallData *call, uint8_t *base, void *native, const InstructionData *inst)
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
    FWD(PushAggregateMem) {
        napi_value arg = call->args[inst->a];

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
        uint8_t *ptr = call->AllocHeap(inst->a);
        *(uint8_t **)(base + inst->b2) = ptr;
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
        uint8_t *ptr = call->AllocHeap(inst->a);
        *(uint8_t **)(base + inst->b2) = ptr;
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
        uint8_t *ptr = call->AllocHeap(inst->a);
        *(uint8_t **)(base + inst->b2) = ptr;
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
        uint8_t *ptr = call->AllocHeap(inst->a);
        *(uint8_t **)(base + inst->b2) = ptr;
        CALL(GX);
        *(uint8_t **)base = ptr;
        return nullptr;
    }

    FWD(ReturnVoid) { return nullptr; }
    FWD(ReturnBool) {
        uint64_t ret = *(uint64_t *)base;
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
        uint64_t ret = *(uint64_t *)base;
        napi_value value = NewString(call->env, (const char *)ret);
        DISPOSE();
        return value;
    }
    FWD(ReturnString16) {
        uint64_t ret = *(uint64_t *)base;
        napi_value value = NewString(call->env, (const char16_t *)ret);
        DISPOSE();
        return value;
    }
    FWD(ReturnString32) {
        uint64_t ret = *(uint64_t *)base;
        napi_value value = NewString(call->env, (const char32_t *)ret);
        DISPOSE();
        return value;
    }
    FWD(ReturnPointer) {
        uint64_t ret = *(uint64_t *)base;
        napi_value value = WrapPointer(call->env, (void *)ret);
        DISPOSE();
        return value;
    }
    FWD(ReturnCallback) {
        uint64_t ret = *(uint64_t *)base;
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
        uint64_t ret = *(uint64_t *)base;
        return DecodeObject(call->instance, (const uint8_t *)ret, inst->type);
    }

#undef INTEGER_SWAP
#undef INTEGER
#undef DISPOSE
#undef CALL

#if defined(MUST_TAIL)
    FORCE_INLINE napi_value RunForward(CallData *call, uint8_t *base, void *native, const InstructionData *inst)
    {
        return ((ForwardFunc *)inst->op)(call, base, native, inst);
    }
#else
            }
        }

        K_UNREACHABLE();
    }
#endif

#undef WRAP

#undef NEXT
#undef FWD
}

#if defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif

bool PreparePlan(Napi::Env env, InstanceData *instance, FunctionInfo *func)
{
    if (!AnalyseFunction(env, instance, func))
        return false;

    for (const InstructionData &inst: func->sync) {
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
            case Opcode::PushAggregateMem: { func->async.Append(inst); } break;

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

                func->async.Append({ .op = Code2Op(Opcode::Yield) });
                func->async.Append({ .op = Code2Op(Opcode::CallG) });
                func->async.Append({ .op = Code2Op(ret), .type = inst.type });
            } break;
            case Opcode::RunRecord:
            case Opcode::RunUnion:
            case Opcode::RunArray: { K_UNREACHABLE(); } break;
            case Opcode::RunFloat32: {
                func->async.Append({ .op = Code2Op(Opcode::Yield) });
                func->async.Append({ .op = Code2Op(Opcode::CallF) });
                func->async.Append({ .op = Code2Op(Opcode::ReturnFloat32), .type = inst.type });
            } break;
            case Opcode::RunFloat64: {
                func->async.Append({ .op = Code2Op(Opcode::Yield) });
                func->async.Append({ .op = Code2Op(Opcode::CallD) });
                func->async.Append({ .op = Code2Op(Opcode::ReturnFloat64), .type = inst.type });
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

                func->async.Append({ .op = Code2Op(Opcode::Yield) });
                func->async.Append({ .op = Code2Op(call) });
                func->async.Append({ .op = Code2Op(Opcode::ReturnAggregateReg), .type = inst.type });
            } break;
            case Opcode::RunAggregateMem: {
                func->async.Append({ .op = Code2Op(Opcode::Yield) });
                func->async.Append({ .op = Code2Op(Opcode::CallMem), .a = inst.a, .b1 = inst.b1, .b2 = inst.b2 });
                func->async.Append({ .op = Code2Op(Opcode::ReturnAggregateMem), .type = inst.type });
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

                func->async.Append({ .op = Code2Op(Opcode::Yield) });
                func->async.Append({ .op = Code2Op(Opcode::CallGX) });
                func->async.Append({ .op = Code2Op(ret), .type = inst.type });
            } break;
            case Opcode::RunRecordX:
            case Opcode::RunUnionX:
            case Opcode::RunArrayX: { K_UNREACHABLE(); } break;
            case Opcode::RunFloat32X: {
                func->async.Append({ .op = Code2Op(Opcode::Yield) });
                func->async.Append({ .op = Code2Op(Opcode::CallFX) });
                func->async.Append({ .op = Code2Op(Opcode::ReturnFloat32), .type = inst.type });
            } break;
            case Opcode::RunFloat64X: {
                func->async.Append({ .op = Code2Op(Opcode::Yield) });
                func->async.Append({ .op = Code2Op(Opcode::CallDX) });
                func->async.Append({ .op = Code2Op(Opcode::ReturnFloat64), .type = inst.type });
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

                func->async.Append({ .op = Code2Op(Opcode::Yield) });
                func->async.Append({ .op = Code2Op(call) });
                func->async.Append({ .op = Code2Op(Opcode::ReturnAggregateReg), .type = inst.type });
            } break;
            case Opcode::RunAggregateMemX: {
                func->async.Append({ .op = Code2Op(Opcode::Yield) });
                func->async.Append({ .op = Code2Op(Opcode::CallMemX), .a = inst.a, .b1 = inst.b1, .b2 = inst.b2 });
                func->async.Append({ .op = Code2Op(Opcode::ReturnAggregateMem), .type = inst.type });
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

#if defined(MUST_TAIL)
    static ForwardFunc *const ForwardDispatch[256] = {
        #define PRIMITIVE(Name) ForwardPush ## Name,
        #include "primitives.inc"
        ForwardPushAggregateReg,
        ForwardPushAggregateSplit,
        ForwardPushAggregateMem,
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

    for (InstructionData &inst: func->sync) {
        inst.op = (void *)ForwardDispatch[(uintptr_t)inst.op];
    }

    for (InstructionData &inst: func->async) {
        inst.op = (void *)ForwardDispatch[(uintptr_t)inst.op];
    }
#endif

    return true;
}

napi_value CallData::Run(const FunctionInfo *func, void *native)
{
    uint8_t *base = AllocStack<uint8_t>(func->stk_size);
    if (!base) [[unlikely]]
        return env.Null();

    const InstructionData *first = func->sync.ptr;
    return RunForward(this, base, native, first);
}

bool CallData::PrepareAsync(const FunctionInfo *func)
{
    uint8_t *base = AllocStack<uint8_t>(func->stk_size);
    if (!base) [[unlikely]]
        return env.Null();
    async_base = base;

    const InstructionData *first = func->async.ptr;
    return !RunForward(this, base, nullptr, first); // Yield returns nullptr
}

void CallData::ExecuteAsync(void *native)
{
    const InstructionData *next = async_ip++;
    RunForward(this, async_base, native, next);
}

napi_value CallData::EndAsync()
{
    const InstructionData *next = async_ip++;
    return RunForward(this, async_base, nullptr, next);
}

}
