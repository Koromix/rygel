// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

// Not compiled directly, but through interp.cc (multiple times)
#if defined(INTERP_SUFFIX)

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    #pragma GCC diagnostic ignored "-Wunused-label"
#else
    #pragma warning(push)
    #pragma warning(disable:4102)
#endif

#if defined(INTERP_COMPAT)
    #define ARG() (*(napi_value *)argp++)
#else
    #define ARG() ((napi_value)(argp++))
#endif

#if defined(INTERP_TAIL)
    #define FWD(Code) \
        static INTERP_TAIL napi_value K_CONCAT(Forward ## Code, INTERP_SUFFIX)(CallData *call, uint8_t *base, uintptr_t *argp, const OpData *op)
    #define NEXT() \
        do { \
            const OpData *next = op + 1; \
            MUST_TAIL return ((K_CONCAT(ForwardFunc, INTERP_SUFFIX) *)next->o)(call, base, argp, next); \
        } while (false)
#else
    #define FWD(Code) \
        case Opcode::Code:
    #define NEXT() \
        break

napi_value K_CONCAT(RunForward, INTERP_SUFFIX)(CallData *call, uint8_t *base, uintptr_t *argp, const OpData *op)
{
    for (;; ++op) {
        switch (Op2Code(op->o)) {
#endif

#define INTEGER(CType) \
        do { \
            napi_value arg = ARG(); \
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
            napi_value arg = ARG(); \
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
            napi_value arg = ARG(); \
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
            napi_value arg = ARG(); \
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
        napi_value arg = ARG();

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
        napi_value arg = ARG();

        if (!call->PushString(arg, GetTypeDirections(op->b), (const char **)(base + op->a))) [[unlikely]]
            return nullptr;

        NEXT();
    }
    FWD(PushString16) {
        napi_value arg = ARG();

        if (!call->PushString16(arg, GetTypeDirections(op->b), (const char16_t **)(base + op->a))) [[unlikely]]
            return nullptr;

        NEXT();
    }
    FWD(PushString32) {
        napi_value arg = ARG();

        if (!call->PushString32(arg, GetTypeDirections(op->b), (const char32_t **)(base + op->a))) [[unlikely]]
            return nullptr;

        NEXT();
    }
    FWD(PushPointer) {
        napi_value arg = ARG();

        const TypeInfo *type = GetType(call->instance, op->b);
        int directions = GetTypeDirections(op->b);

        if (!call->PushPointer(arg, type, directions, (void **)(base + op->a))) [[unlikely]]
            return nullptr;

        NEXT();
    }
    FWD(PushRecord) { K_UNREACHABLE(); return nullptr; }
    FWD(PushUnion) { K_UNREACHABLE(); return nullptr; }
    FWD(PushArray) { K_UNREACHABLE(); return nullptr; }
    FWD(PushFloat32) {
        napi_value arg = ARG();

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
        napi_value arg = ARG();

        double d;
        if (!TryNumber(call->env, arg, &d)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, arg));
            return nullptr;
        }

        *(double *)(base + op->a) = d;

        NEXT();
    }
    FWD(PushCallback) {
        napi_value arg = ARG();

        if (!call->PushCallback(arg, GetType(call->instance, op->b), (void **)(base + op->a))) [[unlikely]]
            return nullptr;

        NEXT();
    }
    FWD(PushPrototype) { K_UNREACHABLE(); return nullptr; }
    FWD(PushAggregateReg) {
        napi_value arg = ARG();

        if (!call->PushObject(arg, GetType(call->instance, op->b), (uint8_t *)(base + op->a))) [[unlikely]]
            return nullptr;

        NEXT();
    }
    FWD(PushAggregatePair) {
        napi_value arg = ARG();

        uintptr_t buf[2];
        if (!call->PushObject(arg, GetType(call->instance, op->b), (uint8_t *)buf)) [[unlikely]]
            return nullptr;

        *(uintptr_t *)(base + op->a1) = buf[0];
        *(uintptr_t *)(base + op->a2) = buf[1];

        NEXT();
    }
    FWD(PushAggregateSplit) {
        napi_value arg = ARG();

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
        napi_value arg = ARG();

        const TypeInfo *type = GetType(call->instance, op->b);
        uint8_t *ptr = call->AllocHeap(type->size);
        *(uint8_t **)(base + op->a) = ptr;

        if (!call->PushObject(arg, type, ptr)) [[unlikely]]
            return nullptr;

        NEXT();
    }

    FWD(PushStringFast) {
        napi_value arg = ARG();

        if (!call->PushString(arg, 1, (const char **)(base + op->a))) [[unlikely]]
            return nullptr;

        NEXT();
    }
    FWD(PushPointerFast) {
        napi_value arg = ARG();

        if (int ret = TryPointerFast(call->env, arg, (void **)(base + op->a))) [[unlikely]] {
            const TypeInfo *type = GetType(call->instance, op->b);
            int directions = GetTypeDirections(op->b);
            napi_valuetype kind = (napi_valuetype)ret;

            if (!call->PushPointerFallback(arg, kind, type, directions, (void **)(base + op->a)))
                return nullptr;
        }

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
            uint64_t ret = WRAP(Call ## Suffix(call->native, base, &call->saved_sp)); \
            return NewNumber(call->env, (CType)ret); \
        } while (false)
#define INTEGER_SWAP(Suffix, CType) \
        do { \
            uint64_t ret = WRAP(Call ## Suffix(call->native, base, &call->saved_sp)); \
            return NewNumber(call->env, ReverseBytes((CType)ret)); \
        } while (false)

    FWD(RunVoid) {
        WRAP(CallG(call->native, base, &call->saved_sp));
        return nullptr;
    }
    FWD(RunBool) {
        uint64_t ret = WRAP(CallG(call->native, base, &call->saved_sp));
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
        uint64_t ret = WRAP(CallG(call->native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunString16) {
        uint64_t ret = WRAP(CallG(call->native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char16_t *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunString32) {
        uint64_t ret = WRAP(CallG(call->native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char32_t *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunPointer) {
        uint64_t ret = WRAP(CallG(call->native, base, &call->saved_sp));
        napi_value value = WrapPointer(call->env, (void *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunCallback) {
        uint64_t ret = WRAP(CallG(call->native, base, &call->saved_sp));
        return WrapPointer(call->env, (void *)ret);
    }
    FWD(RunRecord) { K_UNREACHABLE(); return nullptr; }
    FWD(RunUnion) { K_UNREACHABLE(); return nullptr; }
    FWD(RunArray) { K_UNREACHABLE(); return nullptr; }
    FWD(RunFloat32) {
        float f = WRAP(CallF(call->native, base, &call->saved_sp));
        return NewNumber(call->env, f);
    }
    FWD(RunFloat64) {
        double d = WRAP(CallD(call->native, base, &call->saved_sp));
        return NewNumber(call->env, d);
    }
    FWD(RunPrototype) { K_UNREACHABLE(); return nullptr; }
    FWD(RunAggregateG) {
        auto ret = WRAP(CallG(call->native, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateF) {
        auto ret = WRAP(CallF(call->native, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateD) {
        auto ret = WRAP(CallD(call->native, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateGG) {
        auto ret = WRAP(CallGG(call->native, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateDD) {
        auto ret = WRAP(CallDD(call->native, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateGD) {
        auto ret = WRAP(CallGD(call->native, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateDG) {
        auto ret = WRAP(CallDG(call->native, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateHfa) {
        WRAP(CallHfa(call->native, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, base, type);
    }
    FWD(RunAggregateMem) {
        const TypeInfo *type = GetType(call->instance, op->b);
        uint8_t *ptr = call->AllocHeap(type->size);

        *(uint8_t **)(base + op->a1l) = ptr;
        WRAP(CallG(call->native, base, &call->saved_sp));

        return DecodeObject(call->instance, ptr, type);
    }
    FWD(RunVoidX) {
        WRAP(CallGX(call->native, base, &call->saved_sp));
        return nullptr;
    }
    FWD(RunBoolX) {
        uint64_t ret = WRAP(CallGX(call->native, base, &call->saved_sp));
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
        uint64_t ret = WRAP(CallGX(call->native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunString16X) {
        uint64_t ret = WRAP(CallGX(call->native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char16_t *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunString32X) {
        uint64_t ret = WRAP(CallGX(call->native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char32_t *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunPointerX) {
        uint64_t ret = WRAP(CallGX(call->native, base, &call->saved_sp));
        napi_value value = WrapPointer(call->env, (void *)ret);
        DISPOSE((void *)ret);
        return value;
    }
    FWD(RunCallbackX) {
        uint64_t ret = WRAP(CallGX(call->native, base, &call->saved_sp));
        return WrapPointer(call->env, (void *)ret);
    }
    FWD(RunRecordX) { K_UNREACHABLE(); return nullptr; }
    FWD(RunUnionX) { K_UNREACHABLE(); return nullptr; }
    FWD(RunArrayX) { K_UNREACHABLE(); return nullptr; }
    FWD(RunFloat32X) {
        float f = WRAP(CallFX(call->native, base, &call->saved_sp));
        return NewNumber(call->env, f);
    }
    FWD(RunFloat64X) {
        double d = WRAP(CallDX(call->native, base, &call->saved_sp));
        return NewNumber(call->env, d);
    }
    FWD(RunPrototypeX) { K_UNREACHABLE(); return nullptr; }
    FWD(RunAggregateGX) {
        auto ret = WRAP(CallGX(call->native, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateFX) {
        auto ret = WRAP(CallFX(call->native, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateDX) {
        auto ret = WRAP(CallDX(call->native, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateGGX) {
        auto ret = WRAP(CallGGX(call->native, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateDDX) {
        auto ret = WRAP(CallDDX(call->native, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateGDX) {
        auto ret = WRAP(CallGDX(call->native, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateDGX) {
        auto ret = WRAP(CallDGX(call->native, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, (const uint8_t *)&ret, type);
    }
    FWD(RunAggregateHfaX) {
        WRAP(CallHfaX(call->native, base, &call->saved_sp));
        const TypeInfo *type = GetType(call->instance, op->b);

        return DecodeObject(call->instance, base, type);
    }
    FWD(RunAggregateMemX) {
        const TypeInfo *type = GetType(call->instance, op->b);
        uint8_t *ptr = call->AllocHeap(type->size);

        *(uint8_t **)(base + op->a1l) = ptr;
        WRAP(CallGX(call->native, base, &call->saved_sp));

        return DecodeObject(call->instance, ptr, type);
    }

#undef INTEGER_SWAP
#undef INTEGER

    FWD(Yield) {
        call->async_ip = op + 1;
        return (napi_value)-1;
    }

#define NUMBER_TO_NUMBER(Suffix, SrcType, DestType) \
        do { \
            napi_value arg = ARG(); \
             \
            if (!TryNumber(call->env, arg, (SrcType *)(base + op->a))) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, arg)); \
                return nullptr; \
            } \
             \
            auto ret = WRAP(Call ## Suffix(call->native, base, &call->saved_sp)); \
            return NewNumber(call->env, (DestType)ret); \
        } while (false)

    FWD(Int8ToInt8) { NUMBER_TO_NUMBER(G, int8_t, int8_t); }
    FWD(UInt8ToUInt8) { NUMBER_TO_NUMBER(G, uint8_t, uint8_t); }
    FWD(Int16ToInt16) { NUMBER_TO_NUMBER(G, int16_t, int16_t); }
    FWD(UInt16ToUInt16) { NUMBER_TO_NUMBER(G, uint16_t, uint16_t); }
    FWD(Int32ToInt32) { NUMBER_TO_NUMBER(G, int32_t, int32_t); }
    FWD(UInt32ToUInt32) { NUMBER_TO_NUMBER(G, uint32_t, uint32_t); }
    FWD(Int64ToInt64) { NUMBER_TO_NUMBER(G, int64_t, int64_t); }
    FWD(UInt64ToUInt64) { NUMBER_TO_NUMBER(G, uint64_t, uint64_t); }
    FWD(Float32ToFloat32) { NUMBER_TO_NUMBER(FX, float, float); }
    FWD(Float64ToFloat64) { NUMBER_TO_NUMBER(DX, double, double); }
    FWD(Int64ToFloat64) { NUMBER_TO_NUMBER(D, int64_t, double); }
    FWD(Float64ToInt64) { NUMBER_TO_NUMBER(GX, double, int64_t); }
    FWD(Int32ToFloat32) { NUMBER_TO_NUMBER(F, int32_t, float); }
    FWD(Float32ToInt32) { NUMBER_TO_NUMBER(GX, float, int32_t); }
    FWD(Int64ToFloat32) { NUMBER_TO_NUMBER(F, int64_t, float); }
    FWD(Float32ToInt64) { NUMBER_TO_NUMBER(GX, float, int64_t); }
    FWD(Int32ToFloat64) { NUMBER_TO_NUMBER(D, int32_t, double); }
    FWD(Float64ToInt32) { NUMBER_TO_NUMBER(GX, double, int32_t); }

    FWD(StringToInt32) {
        napi_value arg = ARG();

        if (!call->PushString(arg, 1, (const char **)(base + op->a))) [[unlikely]]
            return nullptr;

        uint64_t ret = WRAP(CallG(call->native, base, &call->saved_sp));
        return NewNumber(call->env, (int32_t)ret);
    }
    FWD(Int32ToString) {
        napi_value arg = ARG();

        if (!TryNumber(call->env, arg, (int32_t *)(base + op->a))) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, arg));
            return nullptr;
        }

        uint64_t ret = WRAP(CallG(call->native, base, &call->saved_sp));
        napi_value value = NewString(call->env, (const char *)ret);
        DISPOSE((void *)ret);
        return value;
    }

    FWD(PointerToInt32) {
        napi_value arg = ARG();

        if (int ret = TryPointerFast(call->env, arg, (void **)(base + op->a))) [[unlikely]] {
            const TypeInfo *type = GetType(call->instance, op->b);
            int directions = GetTypeDirections(op->b);
            napi_valuetype kind = (napi_valuetype)ret;

            if (!call->PushPointerFallback(arg, kind, type, directions, (void **)(base + op->a)))
                return nullptr;
        }

        uint64_t ret = WRAP(CallG(call->native, base, &call->saved_sp));
        return NewNumber(call->env, (int32_t)ret);
    }
    FWD(Int32ToPointer) {
        napi_value arg = ARG();

        if (!TryNumber(call->env, arg, (int32_t *)(base + op->a))) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, arg));
            return nullptr;
        }

        uint64_t ret = WRAP(CallG(call->native, base, &call->saved_sp));
        napi_value value = WrapPointer(call->env, (void *)ret);
        DISPOSE((void *)ret);
        return value;
    }

#undef INTEGER_TO_INTEGER
#undef DISPOSE

#define CALL(Suffix) \
        do { \
            auto ret = WRAP(Call ## Suffix(call->native, base, &call->saved_sp)); \
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
            return NewNumber(call->env, (CType)ret); \
        } while (false)
#define INTEGER_SWAP(CType) \
        do { \
            uint64_t ret = *(uint64_t *)base; \
            return NewNumber(call->env, ReverseBytes((CType)ret)); \
        } while (false)

    FWD(CallG) { CALL(G); return nullptr; }
    FWD(CallF) { CALL(F); return nullptr; }
    FWD(CallD) { CALL(D); return nullptr; }
    FWD(CallGG) { CALL(GG); return nullptr; }
    FWD(CallDD) { CALL(DD); return nullptr; }
    FWD(CallGD) { CALL(GD); return nullptr; }
    FWD(CallDG) { CALL(DG); return nullptr; }
    FWD(CallHfa) { WRAP(CallHfa(call->native, base, &call->saved_sp)); return nullptr; }
    FWD(CallMem) {
        uint8_t *ptr = call->AllocHeap(op->b);
        *(uint8_t **)(base + op->a1l) = ptr;

        WRAP(CallG(call->native, base, &call->saved_sp));
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
    FWD(CallHfaX) { WRAP(CallHfaX(call->native, base, &call->saved_sp)); return nullptr; }
    FWD(CallMemX) {
        uint8_t *ptr = call->AllocHeap(op->b);
        *(uint8_t **)(base + op->a1l) = ptr;

        WRAP(CallGX(call->native, base, &call->saved_sp));
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
        return NewNumber(call->env, f);
    }
    FWD(ReturnFloat64) {
        double d = *(double *)base;
        return NewNumber(call->env, d);
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

#if !defined(INTERP_TAIL)
        }
    }

    K_UNREACHABLE();
}
#endif

#undef WRAP
#undef NEXT
#undef FWD
#undef ARG

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic pop
#else
    #pragma warning(pop)
#endif

#if defined(INTERP_TAIL)

static K_CONCAT(ForwardFunc, INTERP_SUFFIX) *const K_CONCAT(ForwardDispatch, INTERP_SUFFIX)[256] = {
    #define INSTRUCTION(Inst) K_CONCAT(Forward ## Inst, INTERP_SUFFIX),
    #include "instructions.inc"
};

#endif

#undef INTERP_COMPAT
#undef INTERP_SUFFIX
#undef INTERP_TAIL

#endif
