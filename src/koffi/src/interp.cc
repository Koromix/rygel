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

void *Code2Op(Opcode code) { return (void *)code; }

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

static inline Opcode Op2Code(void *o) { return (Opcode)(uintptr_t)o; }

static K_FORCE_INLINE const TypeInfo *GetType(const InstanceData *instance, uint32_t packed)
{
    uint32_t idx = packed & ((1u << 29) - 1);
    return &instance->types[idx];
}

static K_FORCE_INLINE int GetTypeDirections(uint32_t packed) { return (packed >> 29) & 3; }
static K_FORCE_INLINE bool IsTypeDisposable(uint32_t packed) {  return packed & (1u << 31); }

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
        static PRESERVE_NONE NO_STACK_PROTECTOR napi_value Forward ## Code(CallData *call, uint8_t *base, napi_value *argp, const OpData *op)
    #define NEXT() \
        do { \
            const OpData *next = op + 1; \
            MUST_TAIL return ((ForwardFunc *)next->o)(call, base, argp, next); \
        } while (false)
#else
    #define FWD(Code) \
        case Opcode::Code:
    #define NEXT() \
        break

napi_value RunForward(CallData *call, uint8_t *base, napi_value *argp, const OpData *op)
{
    for (;; ++op) {
        switch (Op2Code(op->o)) {
#endif

#define INTEGER(CType) \
        do { \
            napi_value arg = *argp++; \
             \
            CType v; \
            if (!TryNumber(call->env, arg, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, arg)); \
                return nullptr; \
            } \
             \
            *(uintptr_t *)(base + op->a) = (uintptr_t)v; \
        } while (false)
#define INTEGER_SWAP(CType) \
        do { \
            napi_value arg = *argp++; \
             \
            CType v; \
            if (!TryNumber(call->env, arg, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, arg)); \
                return nullptr; \
            } \
             \
            *(uintptr_t *)(base + op->a) = (uintptr_t)ReverseBytes(v); \
        } while (false)
#define INTEGER64(CType) \
        do { \
            napi_value arg = *argp++; \
             \
            CType v; \
            if (!TryNumber(call->env, arg, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, arg)); \
                return nullptr; \
            } \
             \
            *(uint64_t *)(base + op->a) = (uint64_t)v; \
        } while (false)
#define INTEGER64_SWAP(CType) \
        do { \
            napi_value arg = *argp++; \
             \
            CType v; \
            if (!TryNumber(call->env, arg, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, arg)); \
                return nullptr; \
            } \
             \
            *(uint64_t *)(base + op->a) = (uint64_t)ReverseBytes(v); \
        } while (false)

    FWD(PushVoid) { K_UNREACHABLE(); return nullptr; }
    FWD(PushBool) {
        napi_value arg = *argp++;

        bool b;
        if (napi_get_value_bool(call->env, arg, &b) != napi_ok) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected boolean", GetValueType(call->instance, arg));
            return nullptr;
        }

        *(uintptr_t *)(base + op->a) = (uintptr_t)b;

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
        napi_value arg = *argp++;

        const char *str;
        if (!call->PushString(arg, GetTypeDirections(op->b), &str)) [[unlikely]]
            return nullptr;

        *(const char **)(base + op->a) = str;

        NEXT();
    }
    FWD(PushString16) {
        napi_value arg = *argp++;

        const char16_t *str16;
        if (!call->PushString16(arg, GetTypeDirections(op->b), &str16)) [[unlikely]]
            return nullptr;

        *(const char16_t **)(base + op->a) = str16;

        NEXT();
    }
    FWD(PushString32) {
        napi_value arg = *argp++;

        const char32_t *str32;
        if (!call->PushString32(arg, GetTypeDirections(op->b), &str32)) [[unlikely]]
            return nullptr;

        *(const char32_t **)(base + op->a) = str32;

        NEXT();
    }
    FWD(PushPointer) {
        napi_value arg = *argp++;

        const TypeInfo *type = GetType(call->instance, op->b);
        int directions = GetTypeDirections(op->b);

        void *ptr;
        if (!call->PushPointer(arg, type, directions, &ptr)) [[unlikely]]
            return nullptr;

        *(void **)(base + op->a) = ptr;

        NEXT();
    }
    FWD(PushRecord) { K_UNREACHABLE(); return nullptr; }
    FWD(PushUnion) { K_UNREACHABLE(); return nullptr; }
    FWD(PushArray) { K_UNREACHABLE(); return nullptr; }
    FWD(PushFloat32) {
        napi_value arg = *argp++;

        float f;
        if (!TryNumber(call->env, arg, &f)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, arg));
            return nullptr;
        }

#if K_SIZE_MAX == INT64_MAX
        memset(base + op->a, 0xFF, 8);
#endif
        *(float *)(base + op->a) = f;

        NEXT();
    }
    FWD(PushFloat64) {
        napi_value arg = *argp++;

        double d;
        if (!TryNumber(call->env, arg, &d)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, arg));
            return nullptr;
        }

        *(double *)(base + op->a) = d;

        NEXT();
    }
    FWD(PushCallback) {
        napi_value arg = *argp++;

        void *ptr;
        if (!call->PushCallback(arg, GetType(call->instance, op->b), &ptr)) [[unlikely]]
            return nullptr;

        *(void **)(base + op->a) = ptr;

        NEXT();
    }
    FWD(PushPrototype) { K_UNREACHABLE(); return nullptr; }
    FWD(PushAggregateReg) {
        napi_value arg = *argp++;

        uint8_t *ptr = base + op->a;

        if (!call->PushObject(arg, GetType(call->instance, op->b), ptr)) [[unlikely]]
            return nullptr;

        NEXT();
    }
    FWD(PushAggregatePair) {
        napi_value arg = *argp++;

        uintptr_t buf[2];
        if (!call->PushObject(arg, GetType(call->instance, op->b), (uint8_t *)buf)) [[unlikely]]
            return nullptr;

        *(uintptr_t *)(base + op->a1) = buf[0];
        *(uintptr_t *)(base + op->a2) = buf[1];

        NEXT();
    }
    FWD(PushAggregateSplit) {
        napi_value arg = *argp++;

        const TypeInfo *type = GetType(call->instance, op->b);
        int size = type->size;
        int split = op->a1l;
        uint8_t *buf = call->AllocHeap(size); // Wasteful, but rare

        if (!call->PushObject(arg, type, buf)) [[unlikely]]
            return nullptr;

        memcpy(base + op->a1h, buf, split);
        memcpy(base + op->a2, buf + split, size - split);

        NEXT();
    }
    FWD(PushAggregateMem) {
        napi_value arg = *argp++;

        const TypeInfo *type = GetType(call->instance, op->b);
        uint8_t *ptr = call->AllocHeap(type->size);
        *(uint8_t **)(base + op->a) = ptr;

        if (!call->PushObject(arg, type, ptr)) [[unlikely]]
            return nullptr;

        NEXT();
    }

    FWD(PushStringFast) {
        napi_value arg = *argp++;

        const char *str;
        if (!call->PushString(arg, 1, &str)) [[unlikely]]
            return nullptr;

        *(const char **)(base + op->a) = str;

        NEXT();
    }
    FWD(PushPointerFast) {
        napi_value arg = *argp++;

        void *ptr;
        napi_valuetype kind;

        if (!call->TryPointerFast(arg, &ptr, &kind)) [[unlikely]] {
            const TypeInfo *type = GetType(call->instance, op->b);
            int directions = GetTypeDirections(op->b);

            if (!call->PushPointerFallback(arg, kind, type, directions, &ptr))
                return nullptr;
        }

        *(void **)(base + op->a) = ptr;

        NEXT();
    }

    FWD(ConfigRelay) { NEXT(); }

#undef INTEGER64_SWAP
#undef INTEGER64
#undef INTEGER_SWAP
#undef INTEGER

#if defined(_WIN32)
    // Sane people don't use an unused-since-OS2-days field of the TEB (named EnvironmentPointer) to
    // preserve a copy the last GetLastError() value. Nobody sane seems to be using it, Node and V8
    // certainly don't. So we should be good! Except for the fact that I should check myself in.
    // Maybe some obscure malware stuffs something inside this field, for whatever purpose, but it's
    // unlikely to be loaded through Koffi anyway, and if it is, this code is unlikely to like Koffi's
    // custom stack. So we're good (I think).
    // A bit more info here about this field: https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/api/pebteb/teb/index.htm

    #define WRAP(Expr) \
        [&]() { \
            call->DebugForward(); \
             \
            TEB *teb = GetTEB(); \
             \
            K_DEFER { teb->EnvironmentPointer = (void *)(uintptr_t)teb->LastErrorValue; }; \
            teb->LastErrorValue = (unsigned long)(uintptr_t)teb->EnvironmentPointer; \
             \
            ADJUST_TEB(teb, call->stack.ptr, call->stack.end); \
             \
            return (Expr); \
        }()
#else
    #define WRAP(Expr) (call->DebugForward(), (Expr))
#endif

#define DISPOSE(Ptr) \
        do { \
            if (IsTypeDisposable(op->b)) { \
                const TypeInfo *type = GetType(call->instance, op->b); \
                type->dispose(call->instance, type, (Ptr)); \
            } \
        } while (false)

#define INTEGER(Suffix, CType) \
        do { \
            uint64_t ret = WRAP(Call ## Suffix(*argp, base, &call->saved_sp)); \
            return NewInt(call->env, (CType)ret); \
        } while (false)
#define INTEGER_SWAP(Suffix, CType) \
        do { \
            uint64_t ret = WRAP(Call ## Suffix(*argp, base, &call->saved_sp)); \
            return NewInt(call->env, ReverseBytes((CType)ret)); \
        } while (false)

    FWD(RunVoid) {
        WRAP(CallG(*argp, base, &call->saved_sp));
        return nullptr;
    }
    FWD(RunBool) {
        uint64_t ret = WRAP(CallG(*argp, base, &call->saved_sp));
        return NewBool(call->env, ret & 0x1);
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
        uint64_t ret = WRAP(CallG(*argp, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunString16) {
        uint64_t ret = WRAP(CallG(*argp, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char16_t *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunString32) {
        uint64_t ret = WRAP(CallG(*argp, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char32_t *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunPointer) {
        uint64_t ret = WRAP(CallG(*argp, base, &call->saved_sp));
        napi_value value = WrapPointer(call->env, (void *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunCallback) {
        uint64_t ret = WRAP(CallG(*argp, base, &call->saved_sp));
        return WrapPointer(call->env, (void *)ret);
    }
    FWD(RunRecord) { K_UNREACHABLE(); return nullptr; }
    FWD(RunUnion) { K_UNREACHABLE(); return nullptr; }
    FWD(RunArray) { K_UNREACHABLE(); return nullptr; }
    FWD(RunFloat32) {
        float f = WRAP(CallF(*argp, base, &call->saved_sp));
        return NewFloat(call->env, f);
    }
    FWD(RunFloat64) {
        double d = WRAP(CallD(*argp, base, &call->saved_sp));
        return NewFloat(call->env, d);
    }
    FWD(RunPrototype) { K_UNREACHABLE(); return nullptr; }
    FWD(RunAggregateG) {
        auto ret = WRAP(CallG(*argp, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateF) {
        auto ret = WRAP(CallF(*argp, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateD) {
        auto ret = WRAP(CallD(*argp, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateGG) {
        auto ret = WRAP(CallGG(*argp, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateDD) {
        auto ret = WRAP(CallDD(*argp, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateGD) {
        auto ret = WRAP(CallGD(*argp, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateDG) {
        auto ret = WRAP(CallDG(*argp, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateHfa) {
        WRAP(CallHfa(*argp, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, base, type);
    }
    FWD(RunAggregateMem) {
        const TypeInfo *type = GetType(call->instance, op->b);
        uint8_t *ptr = call->AllocHeap(type->size);

        *(uint8_t **)(base + op->a1l) = ptr;
        WRAP(CallG(*argp, base, &call->saved_sp));

        return DecodeObject(call->instance, ptr, type);
    }
    FWD(RunVoidX) {
        WRAP(CallGX(*argp, base, &call->saved_sp));
        return nullptr;
    }
    FWD(RunBoolX) {
        uint64_t ret = WRAP(CallGX(*argp, base, &call->saved_sp));
        return NewBool(call->env, ret & 0x1);
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
        uint64_t ret = WRAP(CallGX(*argp, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunString16X) {
        uint64_t ret = WRAP(CallGX(*argp, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char16_t *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunString32X) {
        uint64_t ret = WRAP(CallGX(*argp, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char32_t *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunPointerX) {
        uint64_t ret = WRAP(CallGX(*argp, base, &call->saved_sp));
        napi_value value = WrapPointer(call->env, (void *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunCallbackX) {
        uint64_t ret = WRAP(CallGX(*argp, base, &call->saved_sp));
        return WrapPointer(call->env, (void *)ret);
    }
    FWD(RunRecordX) { K_UNREACHABLE(); return nullptr; }
    FWD(RunUnionX) { K_UNREACHABLE(); return nullptr; }
    FWD(RunArrayX) { K_UNREACHABLE(); return nullptr; }
    FWD(RunFloat32X) {
        float f = WRAP(CallFX(*argp, base, &call->saved_sp));
        return NewFloat(call->env, f);
    }
    FWD(RunFloat64X) {
        double d = WRAP(CallDX(*argp, base, &call->saved_sp));
        return NewFloat(call->env, d);
    }
    FWD(RunPrototypeX) { K_UNREACHABLE(); return nullptr; }
    FWD(RunAggregateGX) {
        auto ret = WRAP(CallGX(*argp, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateFX) {
        auto ret = WRAP(CallFX(*argp, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateDX) {
        auto ret = WRAP(CallDX(*argp, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateGGX) {
        auto ret = WRAP(CallGGX(*argp, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateDDX) {
        auto ret = WRAP(CallDDX(*argp, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateGDX) {
        auto ret = WRAP(CallGDX(*argp, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateDGX) {
        auto ret = WRAP(CallDGX(*argp, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateHfaX) {
        WRAP(CallHfaX(*argp, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, base, type);
    }
    FWD(RunAggregateMemX) {
        const TypeInfo *type = GetType(call->instance, op->b);
        uint8_t *ptr = call->AllocHeap(type->size);

        *(uint8_t **)(base + op->a1l) = ptr;
        WRAP(CallGX(*argp, base, &call->saved_sp));

        return DecodeObject(call->instance, ptr, type);
    }

#undef INTEGER_SWAP
#undef INTEGER

    FWD(Yield) {
        call->async_ip = op + 1;
        return (napi_value)-1;
    }

#define INTEGER_TO_INTEGER(SrcType, DestType) \
        do { \
            napi_value arg = *argp++; \
             \
            if (!TryNumber(call->env, arg, (SrcType *)(base + op->a))) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, arg)); \
                return nullptr; \
            } \
             \
            uint64_t ret = WRAP(CallG(*argp, base, &call->saved_sp)); \
            return NewInt(call->env, (DestType)ret); \
        } while (false)

    FWD(Int8ToInt8) { INTEGER_TO_INTEGER(int8_t, int8_t); }
    FWD(UInt8ToUInt8) { INTEGER_TO_INTEGER(uint8_t, uint8_t); }
    FWD(Int16ToInt16) { INTEGER_TO_INTEGER(int16_t, int16_t); }
    FWD(UInt16ToUInt16) { INTEGER_TO_INTEGER(uint16_t, uint16_t); }
    FWD(Int32ToInt32) { INTEGER_TO_INTEGER(int32_t, int32_t); }
    FWD(UInt32ToUInt32) { INTEGER_TO_INTEGER(uint32_t, uint32_t); }
    FWD(Int64ToInt64) { INTEGER_TO_INTEGER(int64_t, int64_t); }
    FWD(UInt64ToUInt64) { INTEGER_TO_INTEGER(uint64_t, uint64_t); }

    FWD(StringToInt32) {
        napi_value arg = *argp++;

        const char *str;
        if (!call->PushString(arg, 1, &str)) [[unlikely]]
            return nullptr;
        *(const char **)(base + op->a) = str;

        uint64_t ret = WRAP(CallG(*argp, base, &call->saved_sp)); \
        return NewInt(call->env, (int32_t)ret); \
    }
    FWD(Int32ToString) {
        napi_value arg = *argp++;

        if (!TryNumber(call->env, arg, (int32_t *)(base + op->a))) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, arg));
            return nullptr;
        }

        uint64_t ret = WRAP(CallG(*argp, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char *)ret);
        DISPOSE((void *)ret);
        return value;
    }

#undef INTEGER_TO_INTEGER
#undef DISPOSE

#define CALL(Suffix) \
        do { \
            auto ret = WRAP(Call ## Suffix(*argp, base, &call->saved_sp)); \
            memcpy(base, &ret, K_SIZE(ret)); \
        } while (false)
#define DISPOSE() \
        do { \
            if (IsTypeDisposable(op->b)) { \
                const TypeInfo *type = GetType(call->instance, op->b); \
                void *ptr = *(void **)base; \
                 \
                type->dispose(call->instance, type, ptr); \
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

    FWD(CallG) { CALL(G); return nullptr; }
    FWD(CallF) { CALL(F); return nullptr; }
    FWD(CallD) { CALL(D); return nullptr; }
    FWD(CallGG) { CALL(GG); return nullptr; }
    FWD(CallDD) { CALL(DD); return nullptr; }
    FWD(CallGD) { CALL(GD); return nullptr; }
    FWD(CallDG) { CALL(DG); return nullptr; }
    FWD(CallHfa) { WRAP(CallHfa(*argp, base, &call->saved_sp)); return nullptr; }
    FWD(CallMem) {
        uint8_t *ptr = call->AllocHeap(op->b);
        *(uint8_t **)(base + op->a1l) = ptr;

        WRAP(CallG(*argp, base, &call->saved_sp));
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
    FWD(CallHfaX) { WRAP(CallHfaX(*argp, base, &call->saved_sp)); return nullptr; }
    FWD(CallMemX) {
        uint8_t *ptr = call->AllocHeap(op->b);
        *(uint8_t **)(base + op->a1l) = ptr;

        WRAP(CallGX(*argp, base, &call->saved_sp));
        *(uint8_t **)base = ptr;

        return nullptr;
    }

    FWD(ReturnVoid) { return nullptr; }
    FWD(ReturnBool) {
        uintptr_t ret = *(uintptr_t *)base;
        return NewBool(call->env, ret & 0x1);
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
    FWD(ReturnRecord) { K_UNREACHABLE(); return nullptr; }
    FWD(ReturnUnion) { K_UNREACHABLE(); return nullptr; }
    FWD(ReturnArray) { K_UNREACHABLE(); return nullptr; }
    FWD(ReturnFloat32) {
        float f = *(float *)base;
        return NewFloat(call->env, f);
    }
    FWD(ReturnFloat64) {
        double d = *(double *)base;
        return NewFloat(call->env, d);
    }
    FWD(ReturnPrototype) { K_UNREACHABLE(); return nullptr; }
    FWD(ReturnAggregateReg) {
        const TypeInfo *type = GetType(call->instance, op->b);
        return DecodeObject(call->instance, base, type);
    }
    FWD(ReturnAggregateMem) {
        const TypeInfo *type = GetType(call->instance, op->b);
        uintptr_t ret = *(uintptr_t *)base;

        return DecodeObject(call->instance, (const uint8_t *)ret, type);
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
        static PRESERVE_NONE NO_STACK_PROTECTOR int Relay ## Code(CallData *call, uint8_t *base, napi_value *argp, const OpData *op)
    #define NEXT() \
        do { \
            const OpData *next = op + 1; \
            MUST_TAIL return ((RelayFunc *)next->o)(call, base, argp, next); \
        } while (false)
    #define ALIAS(From, To) \
        RelayFunc *const Relay ## From = Relay ## To;
#else
    #define RELAY(Code) \
        Code: \
        case Opcode::Code:
    #define NEXT() \
        break
    #define ALIAS(From, To) \
        RELAY(From) { goto To; }

int RunRelay(CallData *call, uint8_t *base, napi_value *argp, const OpData *op)
{
    for (;; ++op) {
        switch (Op2Code(op->o)) {
#endif

#define INTEGER(CType) \
        do { \
            const uint8_t *src = base + op->a; \
            CType v = *(const CType *)src; \
             \
            *argp++ = NewInt(call->env, v); \
        } while (false)
#define INTEGER_SWAP(CType) \
        do { \
            const uint8_t *src = base + op->a; \
            CType v = *(const CType *)src; \
             \
            *argp++ = NewInt(call->env, ReverseBytes(v)); \
        } while (false)
#define DISPOSE(Ptr) \
        do { \
            if (IsTypeDisposable(op->b)) { \
                const TypeInfo *type = GetType(call->instance, op->b); \
                type->dispose(call->instance, type, (Ptr)); \
            } \
        } while (false)

    RELAY(PushVoid) { K_UNREACHABLE(); }
    RELAY(PushBool) {
        const uint8_t *src = base + op->a;
        *argp++ = NewBool(call->env, *(bool *)src);

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
        const uint8_t *src = base + op->a;
        const char *str = *(const char **)src;

        *argp++ = NewString(call->env, str);
        DISPOSE(str);

        NEXT();
    }
    RELAY(PushString16) {
        const uint8_t *src = base + op->a;
        const char16_t *str16 = *(const char16_t **)src;

        *argp++ = NewString(call->env, str16);
        DISPOSE(str16);

        NEXT();
    }
    RELAY(PushString32) {
        const uint8_t *src = base + op->a;
        const char32_t *str32 = *(const char32_t **)src;

        *argp++ = NewString(call->env, str32);
        DISPOSE(str32);

        NEXT();
    }
    RELAY(PushPointer) {
        const uint8_t *src = base + op->a;
        void *ptr2 = *(void **)src;

        *argp++ = WrapPointer(call->env, ptr2);
        DISPOSE(ptr2);

        NEXT();
    }
    RELAY(PushRecord) { K_UNREACHABLE(); }
    RELAY(PushUnion) { K_UNREACHABLE(); }
    RELAY(PushArray) { K_UNREACHABLE(); }
    RELAY(PushFloat32) {
        const uint8_t *src = base + op->a;
        *argp++ = NewFloat(call->env, *(float *)src);

        NEXT();
    }
    RELAY(PushFloat64) {
        const uint8_t *src = base + op->a;
        *argp++ = NewFloat(call->env, *(double *)src);

        NEXT();
    }
    RELAY(PushCallback) {
        const uint8_t *src = base + op->a;
        *argp++ = WrapPointer(call->env, *(void **)src);

        NEXT();
    }
    RELAY(PushPrototype) { K_UNREACHABLE(); }
    RELAY(PushAggregateReg) {
        const uint8_t *src = base + op->a;
        const TypeInfo *type = GetType(call->instance, op->b);

        *argp++ = DecodeObject(call->instance, src, type);

        NEXT();
    }
    RELAY(PushAggregatePair) {
        uintptr_t buf[2] = {
            *(uintptr_t *)(base + op->a1),
            *(uintptr_t *)(base + op->a2)
        };
        const TypeInfo *type = GetType(call->instance, op->b);

        *argp++ = DecodeObject(call->instance, (const uint8_t *)buf, type);

        NEXT();
    }
    RELAY(PushAggregateSplit) {
        const TypeInfo *type = GetType(call->instance, op->b);
        int size = type->size;
        int split = op->a1l;
        uint8_t *buf = call->AllocHeap(size); // Wasteful, but rare

        memcpy(buf, base + op->a1h, split);
        memcpy(buf + split, base + op->a2, size - split);

        *argp++ = DecodeObject(call->instance, (const uint8_t *)buf, type);

        NEXT();
    }
    RELAY(PushAggregateMem) {
        const uint8_t *src = *(const uint8_t **)(base + op->a);
        const TypeInfo *type = GetType(call->instance, op->b);

        *argp++ = DecodeObject(call->instance, src, type);

        NEXT();
    }

    ALIAS(PushStringFast, PushString)
    ALIAS(PushPointerFast, PushPointer)

    RELAY(ConfigRelay) {
        int *ptr = (int *)(base + op->a);

        ptr[0] = op->b1;
        ptr[1] = op->b2;

        NEXT();
    }

#undef DISPOSE
#undef INTEGER_SWAP
#undef INTEGER

#define INTEGER(Suffix, CType) \
        do { \
            napi_value value = call->CallCallback(argp, op->a2); \
            if (!value) [[unlikely]] \
                return -1; \
             \
            CType v; \
            if (!TryNumber(call->env, value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, value)); \
                return -1; \
            } \
             \
            *(uintptr_t *)(base - op->a1) = (uintptr_t)v; \
            return 1; \
        } while (false)
#define INTEGER_SWAP(Suffix, CType) \
        do { \
            napi_value value = call->CallCallback(argp, op->a2); \
            if (!value) [[unlikely]] \
                return -1; \
             \
            CType v; \
            if (!TryNumber(call->env, value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, value)); \
                return -1; \
            } \
             \
            *(uintptr_t *)(base - op->a1) = (uintptr_t)ReverseBytes(v); \
            return 1; \
        } while (false)
#define INTEGER64(Suffix, CType) \
        do { \
            napi_value value = call->CallCallback(argp, op->a2); \
            if (!value) [[unlikely]] \
                return -1; \
             \
            CType v; \
            if (!TryNumber(call->env, value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, value)); \
                return -1; \
            } \
             \
            *(uint64_t *)(base - op->a1) = (uint64_t)v; \
            return 1; \
        } while (false)
#define INTEGER64_SWAP(Suffix, CType) \
        do { \
            napi_value value = call->CallCallback(argp, op->a2); \
            if (!value) [[unlikely]] \
                return -1; \
             \
            CType v; \
            if (!TryNumber(call->env, value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, value)); \
                return -1; \
            } \
             \
            *(uint64_t *)(base - op->a1) = (uint64_t)ReverseBytes(v); \
            return 1; \
        } while (false)

    RELAY(RunVoid) {
        call->CallCallback(argp, op->a2);
        return 1;
    }
    RELAY(RunBool) {
        napi_value value = call->CallCallback(argp, op->a2);
        bool b;

        if (napi_get_value_bool(call->env, value, &b) == napi_boolean_expected) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected boolean", GetValueType(call->instance, value));
            return -1;
        }

        *(uintptr_t *)(base - op->a1) = (uintptr_t)b;
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
        napi_value value = call->CallCallback(argp, op->a2);
        const char *str;

        if (!value) [[unlikely]]
            return -1;
        if (!call->PushString(value, 1, &str)) [[unlikely]]
            return -1;

        *(uintptr_t *)(base - op->a1) = (uintptr_t)str;
        return 1;
    }
    RELAY(RunString16) {
        napi_value value = call->CallCallback(argp, op->a2);
        const char16_t *str16;

        if (!value) [[unlikely]]
            return -1;
        if (!call->PushString16(value, 1, &str16)) [[unlikely]]
            return -1;

        *(uintptr_t *)(base - op->a1) = (uintptr_t)str16;
        return 1;
    }
    RELAY(RunString32) {
        napi_value value = call->CallCallback(argp, op->a2);
        const char32_t *str32;

        if (!value) [[unlikely]]
            return -1;
        if (!call->PushString32(value, 1, &str32)) [[unlikely]]
            return -1;

        *(uintptr_t *)(base - op->a1) = (uintptr_t)str32;
        return 1;
    }
    RELAY(RunPointer) {
        napi_value value = call->CallCallback(argp, op->a2);
        void *ptr;

        const TypeInfo *type = GetType(call->instance, op->b);

        if (!value) [[unlikely]]
            return -1;
        if (!call->PushPointer(value, type, 1, &ptr)) [[unlikely]]
            return -1;

        *(uintptr_t *)(base - op->a1) = (uintptr_t)ptr;
        return 1;
    }
    RELAY(RunCallback) {
        napi_value value = call->CallCallback(argp, op->a2);
        void *ptr;

        const TypeInfo *type = GetType(call->instance, op->b);

        if (!value) [[unlikely]]
            return -1;
        if (!call->PushCallback(value, type, &ptr)) [[unlikely]]
            return -1;

        *(uintptr_t *)(base - op->a1) = (uintptr_t)ptr;
        return 1;
    }
    RELAY(RunRecord) { K_UNREACHABLE(); }
    RELAY(RunUnion) { K_UNREACHABLE(); }
    RELAY(RunArray) { K_UNREACHABLE(); }
    RELAY(RunFloat32) {
        napi_value value = call->CallCallback(argp, op->a2);
        float f;

        if (!value) [[unlikely]]
            return -1;
        if (!TryNumber(call->env, value, &f)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, value));
            return -1;
        }

#if K_SIZE_MAX == INT64_MAX
        memset(base - op->a1, 0xFF, 8);
#endif
        *(float *)(base - op->a1) = f;

        return 1;
    }
    RELAY(RunFloat64) {
        napi_value value = call->CallCallback(argp, op->a2);
        double d;

        if (!value) [[unlikely]]
            return -1;
        if (!TryNumber(call->env, value, &d)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, value));
            return -1;
        }

        *(double *)(base - op->a1) = d;
        return 1;
    }
    RELAY(RunPrototype) { K_UNREACHABLE(); }
    RELAY(RunAggregateG) {
        napi_value value = call->CallCallback(argp, op->a2);

        uint8_t *ptr = base - op->a1;
        const TypeInfo *type = GetType(call->instance, op->b);

        if (!value) [[unlikely]]
            return -1;
        if (!call->PushObject(value, type, ptr)) [[unlikely]]
            return -1;

        return 1;
    }
    ALIAS(RunAggregateF, RunAggregateG)
    ALIAS(RunAggregateD, RunAggregateG)
    ALIAS(RunAggregateGG, RunAggregateG)
    ALIAS(RunAggregateDD, RunAggregateG)
    RELAY(RunAggregateGD) {
        napi_value value = call->CallCallback(argp, op->a2);

        uintptr_t buf[2] = {};
        const TypeInfo *type = GetType(call->instance, op->b);

        if (!value) [[unlikely]]
            return -1;
        if (!call->PushObject(value, type, (uint8_t *)buf)) [[unlikely]]
            return -1;

        memcpy(base - op->a1l, buf + 0, K_SIZE(uintptr_t));
        memcpy(base - op->a1h, buf + 1, K_SIZE(uintptr_t));

        return 1;
    }
    ALIAS(RunAggregateDG, RunAggregateGD)
    ALIAS(RunAggregateHfa, RunAggregateG)
    RELAY(RunAggregateMem) {
        napi_value value = call->CallCallback(argp, op->a2);

        uint8_t *ptr = *(uint8_t **)(base + op->a1l);
        const TypeInfo *type = GetType(call->instance, op->b);

        if (!value) [[unlikely]]
            return -1;
        if (!call->PushObject(value, type, ptr)) [[unlikely]]
            return -1;

        *(uintptr_t *)(base - op->a1h) = (uintptr_t)ptr;
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
    ALIAS(RunAggregateHfaX, RunAggregateHfa)
    ALIAS(RunAggregateMemX, RunAggregateMem)

    RELAY(Int8ToInt8) { K_UNREACHABLE(); }
    RELAY(UInt8ToUInt8) { K_UNREACHABLE(); }
    RELAY(Int16ToInt16) { K_UNREACHABLE(); }
    RELAY(UInt16ToUInt16) { K_UNREACHABLE(); }
    RELAY(Int32ToInt32) { K_UNREACHABLE(); }
    RELAY(UInt32ToUInt32) { K_UNREACHABLE(); }
    RELAY(Int64ToInt64) { K_UNREACHABLE(); }
    RELAY(UInt64ToUInt64) { K_UNREACHABLE(); }
    RELAY(StringToInt32) { K_UNREACHABLE(); }
    RELAY(Int32ToString) { K_UNREACHABLE(); }

    RELAY(Yield) { K_UNREACHABLE(); }

    RELAY(CallG) { K_UNREACHABLE(); }
    RELAY(CallF) { K_UNREACHABLE(); }
    RELAY(CallD) { K_UNREACHABLE(); }
    RELAY(CallGG) { K_UNREACHABLE(); }
    RELAY(CallDD) { K_UNREACHABLE(); }
    RELAY(CallGD) { K_UNREACHABLE(); }
    RELAY(CallDG) { K_UNREACHABLE(); }
    RELAY(CallHfa) { K_UNREACHABLE(); }
    RELAY(CallMem) { K_UNREACHABLE(); }
    RELAY(CallGX) { K_UNREACHABLE(); }
    RELAY(CallFX) { K_UNREACHABLE(); }
    RELAY(CallDX) { K_UNREACHABLE(); }
    RELAY(CallGGX) { K_UNREACHABLE(); }
    RELAY(CallDDX) { K_UNREACHABLE(); }
    RELAY(CallGDX) { K_UNREACHABLE(); }
    RELAY(CallDGX) { K_UNREACHABLE(); }
    RELAY(CallHfaX) { K_UNREACHABLE(); }
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

#if defined(MUST_TAIL)

static ForwardFunc *const ForwardDispatch[256] = {
    #define INSTRUCTION(Inst) Forward ## Inst,
    #include "instructions.inc"
};

static RelayFunc *const RelayDispatch[256] = {
    #define INSTRUCTION(Inst) Relay ## Inst,
    #include "instructions.inc"
};

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
                case Opcode::StringToInt32:
                case Opcode::Int32ToString: { K_UNREACHABLE(); } break;

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
            op.o = (void *)ForwardDispatch[(uintptr_t)op.o];
        }

        for (OpData &op: func->plan.relay) {
            op.o = (void *)RelayDispatch[(uintptr_t)op.o];
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
        FUSE(PushInt32, RunString, Int32ToString);
        FUSE(PushStringFast, RunInt32, StringToInt32);

#undef FUSE
    }

#if defined(MUST_TAIL)
    for (OpData &op: func->plan.sync) {
        op.o = (void *)ForwardDispatch[(uintptr_t)op.o];
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
