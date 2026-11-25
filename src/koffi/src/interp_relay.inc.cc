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

#if defined(INTERP_TAIL)
    #define RELAY(Code) \
        static INTERP_TAIL int K_CONCAT(Relay ## Code, INTERP_SUFFIX)(CallData *call, uint8_t *base, napi_value *argp, const OpData *op)
    #define NEXT() \
        do { \
            const OpData *next = op + 1; \
            MUST_TAIL return ((K_CONCAT(RelayFunc, INTERP_SUFFIX) *)next->o)(call, base, argp, next); \
        } while (false)
    #define ALIAS(From, To) \
        K_CONCAT(RelayFunc, INTERP_SUFFIX) *const K_CONCAT(Relay ## From, INTERP_SUFFIX) = K_CONCAT(Relay ## To, INTERP_SUFFIX);
#else
    #define RELAY(Code) \
        Code: \
        case Opcode::Code:
    #define NEXT() \
        break
    #define ALIAS(From, To) \
        RELAY(From) { goto To; }

int K_CONCAT(RunRelay, INTERP_SUFFIX)(CallData *call, uint8_t *base, napi_value *argp, const OpData *op)
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

#if !defined(INTERP_TAIL)
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

#if defined(INTERP_TAIL)

static K_CONCAT(RelayFunc, INTERP_SUFFIX) *const K_CONCAT(RelayDispatch, INTERP_SUFFIX)[256] = {
    #define INSTRUCTION(Inst) K_CONCAT(Relay ## Inst, INTERP_SUFFIX),
    #include "instructions.inc"
};

#endif

#undef INTERP_SUFFIX
#undef INTERP_TAIL

#endif
