// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

// Not compiled directly, but through interp.cc (multiple times)
#if defined(RUN_SUFFIX)

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    #pragma GCC diagnostic ignored "-Wunused-label"
#else
    #pragma warning(push)
    #pragma warning(disable:4102)
#endif

#if defined(RUN_TAIL)
    #define FWD(Code) \
        static RUN_ATTRIBUTES NO_STACK_PROTECTOR napi_value K_CONCAT(Forward ## Code, RUN_SUFFIX)(CallData *call, uint8_t *base, napi_value *argp, const OpData *op)
    #define NEXT() \
        do { \
            const OpData *next = op + 1; \
            MUST_TAIL return ((K_CONCAT(ForwardFunc, RUN_SUFFIX) *)next->o)(call, base, argp, next); \
        } while (false)
#else
    #define FWD(Code) \
        case Opcode::Code:
    #define NEXT() \
        break

napi_value K_CONCAT(RunForward, RUN_SUFFIX)(CallData *call, uint8_t *base, napi_value *argp, const OpData *op)
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

        if (!call->PushString(arg, GetTypeDirections(op->b), (const char **)(base + op->a))) [[unlikely]]
            return nullptr;

        NEXT();
    }
    FWD(PushString16) {
        napi_value arg = *argp++;

        if (!call->PushString16(arg, GetTypeDirections(op->b), (const char16_t **)(base + op->a))) [[unlikely]]
            return nullptr;

        NEXT();
    }
    FWD(PushString32) {
        napi_value arg = *argp++;

        if (!call->PushString32(arg, GetTypeDirections(op->b), (const char32_t **)(base + op->a))) [[unlikely]]
            return nullptr;

        NEXT();
    }
    FWD(PushPointer) {
        napi_value arg = *argp++;

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

        if (!call->PushCallback(arg, GetType(call->instance, op->b), (void **)(base + op->a))) [[unlikely]]
            return nullptr;

        NEXT();
    }
    FWD(PushPrototype) { K_UNREACHABLE(); return nullptr; }
    FWD(PushAggregateReg) {
        napi_value arg = *argp++;

        if (!call->PushObject(arg, GetType(call->instance, op->b), (uint8_t *)(base + op->a))) [[unlikely]]
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

        if (!call->PushString(arg, 1, (const char **)(base + op->a))) [[unlikely]]
            return nullptr;

        NEXT();
    }
    FWD(PushPointerFast) {
        napi_value arg = *argp++;

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
            napi_value arg = *argp++; \
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
        napi_value arg = *argp++;

        if (!call->PushString(arg, 1, (const char **)(base + op->a))) [[unlikely]]
            return nullptr;

        uint64_t ret = WRAP(CallG(call->native, base, &call->saved_sp));
        return NewNumber(call->env, (int32_t)ret);
    }
    FWD(Int32ToString) {
        napi_value arg = *argp++;

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
        napi_value arg = *argp++;

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
        napi_value arg = *argp++;

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

#if !defined(RUN_TAIL)
        }
    }

    K_UNREACHABLE();
}
#endif

#undef WRAP
#undef NEXT
#undef FWD

#if defined(RUN_TAIL)
    #define RELAY(Code) \
        static RUN_ATTRIBUTES NO_STACK_PROTECTOR int K_CONCAT(Relay ## Code, RUN_SUFFIX)(CallData *call, uint8_t *base, napi_value *argp, const OpData *op)
    #define NEXT() \
        do { \
            const OpData *next = op + 1; \
            MUST_TAIL return ((K_CONCAT(RelayFunc, RUN_SUFFIX) *)next->o)(call, base, argp, next); \
        } while (false)
    #define ALIAS(From, To) \
        K_CONCAT(RelayFunc, RUN_SUFFIX) *const K_CONCAT(Relay ## From, RUN_SUFFIX) = K_CONCAT(Relay ## To, RUN_SUFFIX);
#else
    #define RELAY(Code) \
        Code: \
        case Opcode::Code:
    #define NEXT() \
        break
    #define ALIAS(From, To) \
        RELAY(From) { goto To; }

int K_CONCAT(RunRelay, RUN_SUFFIX)(CallData *call, uint8_t *base, napi_value *argp, const OpData *op)
{
    for (;; ++op) {
        switch (Op2Code(op->o)) {
#endif

#define INTEGER(CType) \
        do { \
            const uint8_t *src = base + op->a; \
            CType v = *(const CType *)src; \
             \
            *argp++ = NewNumber(call->env, v); \
        } while (false)
#define INTEGER_SWAP(CType) \
        do { \
            const uint8_t *src = base + op->a; \
            CType v = *(const CType *)src; \
             \
            *argp++ = NewNumber(call->env, ReverseBytes(v)); \
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
        *argp++ = NewNumber(call->env, *(float *)src);

        NEXT();
    }
    RELAY(PushFloat64) {
        const uint8_t *src = base + op->a;
        *argp++ = NewNumber(call->env, *(double *)src);

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
    RELAY(Float32ToFloat32) { K_UNREACHABLE(); }
    RELAY(Float64ToFloat64) { K_UNREACHABLE(); }
    RELAY(StringToInt32) { K_UNREACHABLE(); }
    RELAY(Int32ToString) { K_UNREACHABLE(); }
    RELAY(PointerToInt32) { K_UNREACHABLE(); }
    RELAY(Int32ToPointer) { K_UNREACHABLE(); }
    RELAY(Int64ToFloat64) { K_UNREACHABLE(); }
    RELAY(Float64ToInt64) { K_UNREACHABLE(); }
    RELAY(Int32ToFloat32) { K_UNREACHABLE(); }
    RELAY(Float32ToInt32) { K_UNREACHABLE(); }
    RELAY(Int64ToFloat32) { K_UNREACHABLE(); }
    RELAY(Float32ToInt64) { K_UNREACHABLE(); }
    RELAY(Int32ToFloat64) { K_UNREACHABLE(); }
    RELAY(Float64ToInt32) { K_UNREACHABLE(); }

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

#if !defined(RUN_TAIL)
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

#if defined(RUN_TAIL)

static K_CONCAT(ForwardFunc, RUN_SUFFIX) *const K_CONCAT(ForwardDispatch, RUN_SUFFIX)[256] = {
    #define INSTRUCTION(Inst) K_CONCAT(Forward ## Inst, RUN_SUFFIX),
    #include "instructions.inc"
};

static K_CONCAT(RelayFunc, RUN_SUFFIX) *const K_CONCAT(RelayDispatch, RUN_SUFFIX)[256] = {
    #define INSTRUCTION(Inst) K_CONCAT(Relay ## Inst, RUN_SUFFIX),
    #include "instructions.inc"
};

#endif

#undef RUN_SUFFIX
#undef RUN_ATTRIBUTES
#undef RUN_TAIL

#endif
