// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#if defined(_WIN32) && (defined(__x86_64__) || defined(_M_AMD64))

#include "lib/native/base/base.hh"
#include "../ffi.hh"
#include "../call.hh"
#include "../util.hh"
#include "../win32.hh"

#include <napi.h>

namespace K {

struct BackRegisters {
    uint64_t rax;
    double xmm0;
};

extern "C" uint64_t ForwardCallG(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" float ForwardCallF(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" double ForwardCallD(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" uint64_t ForwardCallGX(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" float ForwardCallFX(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" double ForwardCallDX(const void *func, uint8_t *sp, uint8_t **out_saved_sp);

extern "C" napi_value CallSwitchStack(Napi::Function *func, size_t argc, napi_value *argv,
                                      uint8_t *saved_sp, Span<uint8_t> *new_stack,
                                      napi_value (*call)(Napi::Function *func, size_t argc, napi_value *argv));

enum class AbiOpcode : int16_t {
    #define PRIMITIVE(Name) Push ## Name,
    #include "../primitives.inc"
    PushAggregateReg,
    PushAggregateStack,
    #define PRIMITIVE(Name) Run ## Name,
    #include "../primitives.inc"
    RunAggregateReg,
    RunAggregateStack,
    #define PRIMITIVE(Name) Run ## Name ## X,
    #include "../primitives.inc"
    RunAggregateRegX,
    RunAggregateStackX,
    Yield,
    CallG,
    CallF,
    CallD,
    CallStack,
    CallGX,
    CallFX,
    CallDX,
    CallStackX,
    #define PRIMITIVE(Name) Return ## Name,
    #include "../primitives.inc"
    ReturnAggregate
};

bool AnalyseFunction(Napi::Env, InstanceData *, FunctionInfo *func)
{
    func->ret.regular = IsRegularSize(func->ret.type->size, 8);

    for (Size i = 0; i < func->parameters.len; i++) {
        int16_t arg = (int16_t)(!func->ret.regular + i);
        ParameterInfo &param = func->parameters[i];

        param.regular = IsRegularSize(param.type->size, 8);

        if (param.type->primitive == PrimitiveKind::Record || param.type->primitive == PrimitiveKind::Union) {
            AbiOpcode code = param.regular ? AbiOpcode::PushAggregateReg : AbiOpcode::PushAggregateStack;

            func->sync.Append({ .code = code, .a = param.offset, .b1 = arg, .b2 = (int16_t)param.directions, .type = param.type });
            func->async.Append({ .code = code, .a = param.offset, .b1 = arg, .b2 = (int16_t)param.directions, .type = param.type });
        } else {
            int delta = (int)AbiOpcode::PushVoid - (int)PrimitiveKind::Void;
            AbiOpcode code = (AbiOpcode)((int)param.type->primitive + delta);

            func->sync.Append({ .code = code, .a = param.offset, .b1 = arg, .b2 = (int16_t)param.directions, .type = param.type });
            func->async.Append({ .code = code, .a = param.offset, .b1 = arg, .b2 = (int16_t)param.directions, .type = param.type });
        }

        func->forward_fp |= IsFloat(param.type);
    }

    // At least 4 parameter registers
    {
        Size count = std::max((Size)4, func->parameters.len + !func->ret.regular);
        func->stk_size = AlignLen(8 * count, 16);
    }

    func->async.Append({ .code = AbiOpcode::Yield });

    switch (func->ret.type->primitive) {
        case PrimitiveKind::Void:
        case PrimitiveKind::Bool:
        case PrimitiveKind::Int8:
        case PrimitiveKind::UInt8:
        case PrimitiveKind::Int16:
        case PrimitiveKind::Int16S:
        case PrimitiveKind::UInt16:
        case PrimitiveKind::UInt16S:
        case PrimitiveKind::Int32:
        case PrimitiveKind::Int32S:
        case PrimitiveKind::UInt32:
        case PrimitiveKind::UInt32S:
        case PrimitiveKind::Int64:
        case PrimitiveKind::Int64S:
        case PrimitiveKind::UInt64:
        case PrimitiveKind::UInt64S:
        case PrimitiveKind::String:
        case PrimitiveKind::String16:
        case PrimitiveKind::String32: {
            if (func->forward_fp) {
                int delta = (int)AbiOpcode::RunVoidX - (int)PrimitiveKind::Void;
                AbiOpcode run = (AbiOpcode)((int)func->ret.type->primitive + delta);

                func->sync.Append({ .code = run, .type = func->ret.type });
            } else {
                int delta = (int)AbiOpcode::RunVoid - (int)PrimitiveKind::Void;
                AbiOpcode run = (AbiOpcode)((int)func->ret.type->primitive + delta);

                func->sync.Append({ .code = run, .type = func->ret.type });
            }

            // Async
            {
                int delta = (int)AbiOpcode::ReturnVoid - (int)PrimitiveKind::Void;
                AbiOpcode call = func->forward_fp ? AbiOpcode::CallGX : AbiOpcode::CallG;
                AbiOpcode ret = (AbiOpcode)((int)func->ret.type->primitive + delta);

                func->async.Append({ .code = call });
                func->async.Append({ .code = ret, .type = func->ret.type });
            }
        } break;

        case PrimitiveKind::Pointer: {
            AbiOpcode run = func->forward_fp ? AbiOpcode::RunPointerX : AbiOpcode::RunPointer;
            AbiOpcode call = func->forward_fp ? AbiOpcode::CallGX : AbiOpcode::CallG;

            func->sync.Append({ .code = run, .type = func->ret.type->ref.type });
            func->async.Append({ .code = call });
            func->async.Append({ .code = AbiOpcode::ReturnPointer, .type = func->ret.type->ref.type });
        } break;
        case PrimitiveKind::Callback: {
            AbiOpcode run = func->forward_fp ? AbiOpcode::RunCallbackX : AbiOpcode::RunCallback;
            AbiOpcode call = func->forward_fp ? AbiOpcode::CallGX : AbiOpcode::CallG;

            func->sync.Append({ .code = run, .type = func->ret.type });
            func->async.Append({ .code = call });
            func->async.Append({ .code = AbiOpcode::ReturnCallback, .type = func->ret.type });
        } break;

        case PrimitiveKind::Record:
        case PrimitiveKind::Union: {
            if (func->ret.regular) {
                AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateRegX : AbiOpcode::RunAggregateReg;
                AbiOpcode call = func->forward_fp ? AbiOpcode::CallGX : AbiOpcode::CallG;

                func->sync.Append({ .code = run, .type = func->ret.type });
                func->async.Append({ .code = call });
                func->async.Append({ .code = AbiOpcode::ReturnAggregate, .type = func->ret.type });
            } else {
                AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateStackX : AbiOpcode::RunAggregateStack;
                AbiOpcode call = func->forward_fp ? AbiOpcode::CallStackX : AbiOpcode::CallStack;

                func->sync.Append({ .code = run, .b = (int32_t)func->ret.type->size, .type = func->ret.type });
                func->async.Append({ .code = call, .b = (int32_t)func->ret.type->size });
                func->async.Append({ .code = AbiOpcode::ReturnAggregate, .type = func->ret.type });
            }
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;

        case PrimitiveKind::Float32: {
            AbiOpcode run = func->forward_fp ? AbiOpcode::RunFloat32X : AbiOpcode::RunFloat32;
            AbiOpcode call = func->forward_fp ? AbiOpcode::CallFX : AbiOpcode::CallF;

            func->sync.Append({ .code = run, .type = func->ret.type });
            func->async.Append({ .code = call });
            func->async.Append({ .code = AbiOpcode::ReturnFloat32, .type = func->ret.type });
        } break;
        case PrimitiveKind::Float64: {
            AbiOpcode run = func->forward_fp ? AbiOpcode::RunFloat64X : AbiOpcode::RunFloat64;
            AbiOpcode call = func->forward_fp ? AbiOpcode::CallDX : AbiOpcode::CallD;

            func->sync.Append({ .code = run, .type = func->ret.type });
            func->async.Append({ .code = call });
            func->async.Append({ .code = AbiOpcode::ReturnFloat64, .type = func->ret.type });
        } break;

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

    return true;
}

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

namespace {
#if defined(MUST_TAIL)
    #define OP(Code) \
            PRESERVE_NONE Napi::Value Handle ## Code(CallData *call, napi_value *args, uint64_t *base, const AbiInstruction *inst)
    #define NEXT() \
            do { \
                const AbiInstruction *next = inst + 1; \
                MUST_TAIL return ForwardDispatch[(int)next->code](call, args, base, next); \
            } while (false)

    PRESERVE_NONE typedef Napi::Value ForwardFunc(CallData *call, napi_value *args, uint64_t *base, const AbiInstruction *inst);

    extern ForwardFunc *const ForwardDispatch[256];
#else
    #define OP(Code) \
        case AbiOpcode::Code:
    #define NEXT() \
        break

    Napi::Value RunLoop(CallData *call, napi_value *args, uint64_t *base, const AbiInstruction *inst)
    {
        for (;; ++inst) {
            switch (inst->code) {
#endif

#define WRAP(Expr) \
        [&]() { \
            TEB *teb = GetTEB(); \
             \
            K_DEFER { call->instance->last_error = teb->LastErrorValue; }; \
            teb->LastErrorValue = call->instance->last_error; \
             \
            ADJUST_TEB(teb, call->mem->stack.ptr, call->mem->stack.end()); \
             \
            return (Expr); \
        }()

#define INTEGER(CType) \
        do { \
            Napi::Value value(call->env, args[inst->a]); \
             \
            CType v; \
            if (!TryNumber(value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, value)); \
                return call->env.Null(); \
            } \
             \
            *(base + inst->b1) = (uint64_t)v; \
        } while (false)
#define INTEGER_SWAP(CType) \
        do { \
            Napi::Value value(call->env, args[inst->a]); \
             \
            CType v; \
            if (!TryNumber(value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, value)); \
                return call->env.Null(); \
            } \
             \
            *(base + inst->b1) = (uint64_t)ReverseBytes(v); \
        } while (false)

    OP(PushVoid) { K_UNREACHABLE(); return call->env.Null(); }
    OP(PushBool) {
        Napi::Value value(call->env, args[inst->a]);

        bool b;
        if (napi_get_value_bool(call->env, value, &b) != napi_ok) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected boolean", GetValueType(call->instance, value));
            return call->env.Null();
        }

        *(bool *)(base + inst->b1) = b;

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
    OP(PushInt64) { INTEGER(int64_t); NEXT(); }
    OP(PushInt64S) { INTEGER_SWAP(int64_t); NEXT(); }
    OP(PushUInt64) { INTEGER(uint64_t); NEXT(); }
    OP(PushUInt64S) { INTEGER_SWAP(uint64_t); NEXT(); }
    OP(PushString) {
        Napi::Value value(call->env, args[inst->a]);

        const char *str;
        if (!call->PushString(value, inst->b2, &str)) [[unlikely]]
            return call->env.Null();

        *(const char **)(base + inst->b1) = str;

        NEXT();
    }
    OP(PushString16) {
        Napi::Value value(call->env, args[inst->a]);

        const char16_t *str16;
        if (!call->PushString16(value, inst->b2, &str16)) [[unlikely]]
            return call->env.Null();

        *(const char16_t **)(base + inst->b1) = str16;

        NEXT();
    }
    OP(PushString32) {
        Napi::Value value(call->env, args[inst->a]);

        const char32_t *str32;
        if (!call->PushString32(value, inst->b2, &str32)) [[unlikely]]
            return call->env.Null();

        *(const char32_t **)(base + inst->b1) = str32;

        NEXT();
    }
    OP(PushPointer) {
        Napi::Value value(call->env, args[inst->a]);

        void *ptr;
        if (!call->PushPointer(value, inst->type, inst->b2, &ptr)) [[unlikely]]
            return call->env.Null();

        *(void **)(base + inst->b1) = ptr;

        NEXT();
    }
    OP(PushRecord) { K_UNREACHABLE(); return call->env.Null(); }
    OP(PushUnion) { K_UNREACHABLE(); return call->env.Null(); }
    OP(PushArray) { K_UNREACHABLE(); return call->env.Null(); }
    OP(PushFloat32) {
        Napi::Value value(call->env, args[inst->a]);

        float f;
        if (!TryNumber(value, &f)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, value));
            return call->env.Null();
        }

        memset(base + inst->b1, 0, 8);
        memcpy(base + inst->b1, &f, 4);

        NEXT();
    }
    OP(PushFloat64) {
        Napi::Value value(call->env, args[inst->a]);

        double d;
        if (!TryNumber(value, &d)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, value));
            return call->env.Null();
        }

        *(double *)(base + inst->b1) = d;

        NEXT();
    }
    OP(PushCallback) {
        Napi::Value value(call->env, args[inst->a]);

        void *ptr;
        if (!call->PushCallback(value, inst->type, &ptr)) [[unlikely]]
            return call->env.Null();

        *(void **)(base + inst->b1) = ptr;

        NEXT();
    }
    OP(PushPrototype) { K_UNREACHABLE(); return call->env.Null(); }
    OP(PushAggregateReg) {
        Napi::Value value(call->env, args[inst->a]);

        if (!IsObject(value)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected object", GetValueType(call->instance, value));
            return call->env.Null();
        }

        uint8_t *ptr = (uint8_t *)(base + inst->b1);

        Napi::Object obj = value.As<Napi::Object>();
        if (!call->PushObject(obj, inst->type, ptr))
            return call->env.Null();

        NEXT();
    }
    OP(PushAggregateStack) {
        Napi::Value value(call->env, args[inst->a]);

        if (!IsObject(value)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected object", GetValueType(call->instance, value));
            return call->env.Null();
        }

        uint8_t *ptr = call->AllocHeap(inst->type->size, 16);
        *(uint8_t **)(base + inst->b1) = ptr;

        Napi::Object obj = value.As<Napi::Object>();
        if (!call->PushObject(obj, inst->type, ptr))
            return call->env.Null();

        NEXT();
    }

#undef INTEGER_SWAP
#undef INTEGER

#define INTEGER(Suffix, CType) \
        do { \
            uint64_t rax = WRAP(ForwardCall ## Suffix(call->native, (uint8_t *)base, &call->saved_sp)); \
            call->PopOutArguments(); \
            return NewInt(call->env, (CType)rax); \
        } while (false)
#define INTEGER_SWAP(Suffix, CType) \
        do { \
            uint64_t rax = WRAP(ForwardCall ## Suffix(call->native, (uint8_t *)base, &call->saved_sp)); \
            call->PopOutArguments(); \
            return NewInt(call->env, ReverseBytes((CType)rax)); \
        } while (false)
#define DISPOSE(Ptr) \
        do { \
            if (inst->type->dispose) { \
                inst->type->dispose(call->env, inst->type, (Ptr)); \
            } \
        } while (false)

    OP(RunVoid) {
        WRAP(ForwardCallG(call->native, (uint8_t *)base, &call->saved_sp));
        call->PopOutArguments();
        return call->env.Undefined();
    }
    OP(RunBool) {
        uint64_t rax = WRAP(ForwardCallG(call->native, (uint8_t *)base, &call->saved_sp));
        call->PopOutArguments();
        return Napi::Boolean::New(call->env, rax & 0x1);
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
        uint64_t rax = WRAP(ForwardCallG(call->native, (uint8_t *)base, &call->saved_sp));
        Napi::Value value = rax ? Napi::String::New(call->env, (const char *)rax) : call->env.Null();
        DISPOSE((void *)rax);
        call->PopOutArguments();
        return value;
    }
    OP(RunString16) {
        uint64_t rax = WRAP(ForwardCallG(call->native, (uint8_t *)base, &call->saved_sp));
        Napi::Value value = rax ? Napi::String::New(call->env, (const char16_t *)rax) : call->env.Null();
        DISPOSE((void *)rax);
        call->PopOutArguments();
        return value;
    }
    OP(RunString32) {
        uint64_t rax = WRAP(ForwardCallG(call->native, (uint8_t *)base, &call->saved_sp));
        Napi::Value value = rax ? MakeStringFromUTF32(call->env, (const char32_t *)rax) : call->env.Null();
        DISPOSE((void *)rax);
        call->PopOutArguments();
        return value;
    }
    OP(RunPointer) {
        uint64_t rax = WRAP(ForwardCallG(call->native, (uint8_t *)base, &call->saved_sp));
        Napi::Value value = rax ? WrapPointer(call->env, inst->type, (void *)rax) : call->env.Null();
        DISPOSE((void *)rax);
        call->PopOutArguments();
        return value;
    }
    OP(RunCallback) {
        uint64_t rax = WRAP(ForwardCallG(call->native, (uint8_t *)base, &call->saved_sp));
        call->PopOutArguments();
        return rax ? WrapCallback(call->env, inst->type, (void *)rax) : call->env.Null();
    }
    OP(RunRecord) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunUnion) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunArray) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunFloat32) {
        float f = WRAP(ForwardCallF(call->native, (uint8_t *)base, &call->saved_sp));
        call->PopOutArguments();
        return Napi::Number::New(call->env, (double)f);
    }
    OP(RunFloat64) {
        double d = WRAP(ForwardCallD(call->native, (uint8_t *)base, &call->saved_sp));
        call->PopOutArguments();
        return Napi::Number::New(call->env, d);
    }
    OP(RunPrototype) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunAggregateReg) {
        auto ret = WRAP(ForwardCallG(call->native, (uint8_t *)base, &call->saved_sp));
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateStack) {
        *(uint8_t **)base = call->AllocHeap(inst->b, 16);
        uint64_t rax = WRAP(ForwardCallG(call->native, (uint8_t *)base, &call->saved_sp));
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)rax, inst->type);
    }
    OP(RunVoidX) {
        WRAP(ForwardCallGX(call->native, (uint8_t *)base, &call->saved_sp));
        call->PopOutArguments();
        return call->env.Undefined();
    }
    OP(RunBoolX) {
        uint64_t rax = WRAP(ForwardCallGX(call->native, (uint8_t *)base, &call->saved_sp));
        call->PopOutArguments();
        return Napi::Boolean::New(call->env, rax & 0x1);
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
        uint64_t rax = WRAP(ForwardCallGX(call->native, (uint8_t *)base, &call->saved_sp));
        Napi::Value value = rax ? Napi::String::New(call->env, (const char *)rax) : call->env.Null();
        DISPOSE((void *)rax);
        call->PopOutArguments();
        return value;
    }
    OP(RunString16X) {
        uint64_t rax = WRAP(ForwardCallGX(call->native, (uint8_t *)base, &call->saved_sp));
        Napi::Value value = rax ? Napi::String::New(call->env, (const char16_t *)rax) : call->env.Null();
        DISPOSE((void *)rax);
        call->PopOutArguments();
        return value;
    }
    OP(RunString32X) {
        uint64_t rax = WRAP(ForwardCallGX(call->native, (uint8_t *)base, &call->saved_sp));
        Napi::Value value = rax ? MakeStringFromUTF32(call->env, (const char32_t *)rax) : call->env.Null();
        DISPOSE((void *)rax);
        call->PopOutArguments();
        return value;
    }
    OP(RunPointerX) {
        uint64_t rax = WRAP(ForwardCallGX(call->native, (uint8_t *)base, &call->saved_sp));
        Napi::Value value = rax ? WrapPointer(call->env, inst->type, (void *)rax) : call->env.Null();
        DISPOSE((void *)rax);
        call->PopOutArguments();
        return value;
    }
    OP(RunCallbackX) {
        uint64_t rax = WRAP(ForwardCallGX(call->native, (uint8_t *)base, &call->saved_sp));
        call->PopOutArguments();
        return rax ? WrapCallback(call->env, inst->type, (void *)rax) : call->env.Null();
    }
    OP(RunRecordX) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunUnionX) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunArrayX) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunFloat32X) {
        float f = WRAP(ForwardCallFX(call->native, (uint8_t *)base, &call->saved_sp));
        call->PopOutArguments();
        return Napi::Number::New(call->env, (double)f);
    }
    OP(RunFloat64X) {
        double d = WRAP(ForwardCallDX(call->native, (uint8_t *)base, &call->saved_sp));
        call->PopOutArguments();
        return Napi::Number::New(call->env, d);
    }
    OP(RunPrototypeX) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunAggregateRegX) {
        auto ret = WRAP(ForwardCallGX(call->native, (uint8_t *)base, &call->saved_sp));
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateStackX) {
        *(uint8_t **)base = call->AllocHeap(inst->b, 16);
        uint64_t rax = WRAP(ForwardCallGX(call->native, (uint8_t *)base, &call->saved_sp));
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)rax, inst->type);
    }

#undef DISPOSE
#undef INTEGER_SWAP
#undef INTEGER

#define CALL(Suffix) \
        do { \
            auto ret = WRAP(ForwardCall ## Suffix(call->native, (uint8_t *)base, &call->saved_sp)); \
            memcpy(base, &ret, K_SIZE(ret)); \
        } while (false)
#define DISPOSE() \
        do { \
            if (inst->type->dispose) { \
                void *ptr = *(void **)base; \
                inst->type->dispose(call->env, inst->type, ptr); \
            } \
        } while (false)
#define INTEGER(CType) \
        do { \
            uint64_t rax = *(uint64_t *)base; \
            call->PopOutArguments(); \
            return NewInt(call->env, (CType)rax); \
        } while (false)
#define INTEGER_SWAP(CType) \
        do { \
            uint64_t rax = *(uint64_t *)base; \
            call->PopOutArguments(); \
            return NewInt(call->env, ReverseBytes((CType)rax)); \
        } while (false)

    OP(Yield) {
        call->async_ip = inst + 1;
        return call->env.Null();
    }

    OP(CallG) { CALL(G); return call->env.Null(); }
    OP(CallF) { CALL(F); return call->env.Null(); }
    OP(CallD) { CALL(D); return call->env.Null(); }
    OP(CallStack) {
        *(uint8_t **)base = call->AllocHeap(inst->b, 16);
        CALL(G);
        return call->env.Null();
    }
    OP(CallGX) { CALL(GX); return call->env.Null(); }
    OP(CallFX) { CALL(FX); return call->env.Null(); }
    OP(CallDX) { CALL(DX); return call->env.Null(); }
    OP(CallStackX) {
        *(uint8_t **)base = call->AllocHeap(inst->b, 16);
        CALL(GX);
        return call->env.Null();
    }

    OP(ReturnVoid) {
        call->PopOutArguments();
        return call->env.Undefined();
    }
    OP(ReturnBool) {
        uint64_t rax = *(uint64_t *)base;
        call->PopOutArguments();
        return Napi::Boolean::New(call->env, rax & 0x1);
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
        uint64_t rax = *(uint64_t *)base;
        call->PopOutArguments();
        Napi::Value value = rax ? Napi::String::New(call->env, (const char *)rax) : call->env.Null();
        DISPOSE();
        return value;
    }
    OP(ReturnString16) {
        uint64_t rax = *(uint64_t *)base;
        call->PopOutArguments();
        Napi::Value value = rax ? Napi::String::New(call->env, (const char16_t *)rax) : call->env.Null();
        DISPOSE();
        return value;
    }
    OP(ReturnString32) {
        uint64_t rax = *(uint64_t *)base;
        call->PopOutArguments();
        Napi::Value value = rax ? MakeStringFromUTF32(call->env, (const char32_t *)rax) : call->env.Null();
        DISPOSE();
        return value;
    }
    OP(ReturnPointer) {
        uint64_t rax = *(uint64_t *)base;
        call->PopOutArguments();
        Napi::Value value = rax ? WrapPointer(call->env, inst->type, (void *)rax) : call->env.Null();
        DISPOSE();
        return value;
    }
    OP(ReturnCallback) {
        uint64_t rax = *(uint64_t *)base;
        call->PopOutArguments();
        return rax ? WrapCallback(call->env, inst->type, (void *)rax) : call->env.Null();
    }
    OP(ReturnRecord) { K_UNREACHABLE(); return call->env.Null(); }
    OP(ReturnUnion) { K_UNREACHABLE(); return call->env.Null(); }
    OP(ReturnArray) { K_UNREACHABLE(); return call->env.Null(); }
    OP(ReturnFloat32) {
        float f = *(float *)base;
        call->PopOutArguments();
        return Napi::Number::New(call->env, (double)f);
    }
    OP(ReturnFloat64) {
        double d = *(double *)base;
        call->PopOutArguments();
        return Napi::Number::New(call->env, d);
    }
    OP(ReturnPrototype) { K_UNREACHABLE(); return call->env.Null(); }
    OP(ReturnAggregate) {
        uint64_t rax = *(uint64_t *)base;
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)rax, inst->type);
    }

#undef INTEGER_SWAP
#undef INTEGER
#undef DISPOSE
#undef CALL

#if defined(MUST_TAIL)
    ForwardFunc *const ForwardDispatch[256] = {
        #define PRIMITIVE(Name) HandlePush ## Name,
        #include "../primitives.inc"
        HandlePushAggregateReg,
        HandlePushAggregateStack,
        #define PRIMITIVE(Name) HandleRun ## Name,
        #include "../primitives.inc"
        HandleRunAggregateReg,
        HandleRunAggregateStack,
        #define PRIMITIVE(Name) HandleRun ## Name ## X,
        #include "../primitives.inc"
        HandleRunAggregateRegX,
        HandleRunAggregateStackX,
        HandleYield,
        HandleCallG,
        HandleCallF,
        HandleCallD,
        HandleCallStack,
        HandleCallGX,
        HandleCallFX,
        HandleCallDX,
        HandleCallStackX,
        #define PRIMITIVE(Name) HandleReturn ## Name,
        #include "../primitives.inc"
        HandleReturnAggregate
    };

    FORCE_INLINE Napi::Value RunLoop(CallData *call, napi_value *args, uint64_t *base, const AbiInstruction *inst)
    {
        return ForwardDispatch[(int)inst->code](call, args, base, inst);
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

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic pop
#endif

Napi::Value CallData::Run(const Napi::CallbackInfo &info)
{
    uint8_t *base = AllocStack<uint8_t>(func->stk_size);
    if (!base) [[unlikely]]
        return env.Null();

    const AbiInstruction *first = func->sync.ptr;
    return RunLoop(this, info.First(), (uint64_t *)base, first);
}

bool CallData::PrepareAsync(const Napi::CallbackInfo &info)
{
    uint8_t *base = AllocStack<uint8_t>(func->stk_size);
    if (!base) [[unlikely]]
        return env.Null();
    async_base = base;

    const AbiInstruction *first = func->async.ptr;
    return RunLoop(this, info.First(), (uint64_t *)base, first);
}

void CallData::ExecuteAsync()
{
    const AbiInstruction *next = async_ip++;
    RunLoop(this, nullptr, (uint64_t *)async_base, next);
}

Napi::Value CallData::EndAsync()
{
    const AbiInstruction *next = async_ip++;
    return RunLoop(this, nullptr, (uint64_t *)async_base, next);
}

void CallData::Relay(Size idx, uint8_t *own_sp, uint8_t *caller_sp, bool switch_stack, BackRegisters *out_reg)
{
    if (env.IsExceptionPending()) [[unlikely]]
        return;

    TEB *teb = GetTEB();

    K_DEFER_C(base = teb->StackBase,
              limit = teb->StackLimit,
              dealloc = teb->DeallocationStack) {
        teb->StackBase = base;
        teb->StackLimit = limit;
        teb->DeallocationStack = dealloc;
    };
    teb->StackBase = instance->main_stack_max;
    teb->StackLimit = instance->main_stack_min;
    teb->DeallocationStack = instance->main_stack_min;

    const TrampolineInfo &trampoline = shared.trampolines[idx];

    const FunctionInfo *proto = trampoline.proto;
    Napi::Function func = trampoline.func.Value();

    uint64_t *gpr_ptr = (uint64_t *)own_sp;
    uint64_t *xmm_ptr = gpr_ptr + 4;
    uint64_t *stk_ptr = (uint64_t *)caller_sp;

    uint8_t *return_ptr = !proto->ret.regular ? (uint8_t *)gpr_ptr[0] : nullptr;

    K_DEFER_N(err_guard) { memset(out_reg, 0, K_SIZE(*out_reg)); };

    if (trampoline.generation >= 0 && trampoline.generation != (int32_t)mem->generation) [[unlikely]] {
        ThrowError<Napi::Error>(env, "Cannot use non-registered callback beyond FFI call");
        return;
    }

    LocalArray<napi_value, MaxParameters + 1> arguments;

    arguments.Append(!trampoline.recv.IsEmpty() ? trampoline.recv.Value() : env.Undefined());

    // Convert to JS arguments
    for (Size i = 0, j = !!return_ptr; i < proto->parameters.len; i++, j++) {
        const ParameterInfo &param = proto->parameters[i];
        K_ASSERT(param.directions >= 1 && param.directions <= 3);

        switch (param.type->primitive) {
            case PrimitiveKind::Void: { K_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                bool b = *(bool *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = Napi::Boolean::New(env, b);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int8: {
                int8_t v = *(int8_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt8: {
                uint8_t v = *(uint8_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int16: {
                int16_t v = *(int16_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int16S: {
                int16_t v = *(int16_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, ReverseBytes(v));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt16: {
                uint16_t v = *(uint16_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt16S: {
                uint16_t v = *(uint16_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, ReverseBytes(v));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int32: {
                int32_t v = *(int32_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int32S: {
                int32_t v = *(int32_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, ReverseBytes(v));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt32: {
                uint32_t v = *(uint32_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt32S: {
                uint32_t v = *(uint32_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, ReverseBytes(v));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int64: {
                int64_t v = *(int64_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int64S: {
                int64_t v = *(int64_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, ReverseBytes(v));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt64: {
                uint64_t v = *(uint64_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt64S: {
                uint64_t v = *(uint64_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, ReverseBytes(v));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::String: {
                const char *str = *(const char **)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = str ? Napi::String::New(env, str) : env.Null();
                arguments.Append(arg);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, str);
                }
            } break;
            case PrimitiveKind::String16: {
                const char16_t *str16 = *(const char16_t **)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = str16 ? Napi::String::New(env, str16) : env.Null();
                arguments.Append(arg);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, str16);
                }
            } break;
            case PrimitiveKind::String32: {
                const char32_t *str32 = *(const char32_t **)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = str32 ? MakeStringFromUTF32(env, str32) : env.Null();
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Pointer: {
                void *ptr2 = *(void **)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value p = ptr2 ? WrapPointer(env, param.type->ref.type, ptr2) : env.Null();
                arguments.Append(p);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, ptr2);
                }
            } break;
            case PrimitiveKind::Callback: {
                void *ptr2 = *(void **)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value p = ptr2 ? WrapCallback(env, param.type->ref.type, ptr2) : env.Null();
                arguments.Append(p);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, ptr2);
                }
            } break;
            case PrimitiveKind::Record:
            case PrimitiveKind::Union: {
                uint8_t *ptr;
                if (param.regular) {
                    ptr = (uint8_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                } else {
                    ptr = *(uint8_t **)(j < 4 ? gpr_ptr + j : stk_ptr);
                }
                stk_ptr += (j >= 4);

                Napi::Object obj2 = DecodeObject(env, ptr, param.type);
                arguments.Append(obj2);
            } break;
            case PrimitiveKind::Array: { K_UNREACHABLE(); } break;
            case PrimitiveKind::Float32: {
                float f = *(float *)(j < 4 ? xmm_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = Napi::Number::New(env, (double)f);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Float64: {
                double d = *(double *)(j < 4 ? xmm_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;

            case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
        }
    }

    const TypeInfo *type = proto->ret.type;

    // Make the call
    napi_value ret;
    if (switch_stack) {
        ret = CallSwitchStack(&func, (size_t)arguments.len, arguments.data, saved_sp, &mem->stack,
                              [](Napi::Function *func, size_t argc, napi_value *argv) { return (napi_value)func->Call(argv[0], argc - 1, argv + 1); });
    } else {
        ret = (napi_value)func.Call(arguments[0], arguments.len - 1, arguments.data + 1);
    }
    Napi::Value value(env, ret);

    if (env.IsExceptionPending()) [[unlikely]]
        return;

#define RETURN_INTEGER(CType) \
        do { \
            CType v; \
            if (!TryNumber(value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value)); \
                return; \
            } \
             \
            out_reg->rax = (uint64_t)v; \
        } while (false)
#define RETURN_INTEGER_SWAP(CType) \
        do { \
            CType v; \
            if (!TryNumber(value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value)); \
                return; \
            } \
             \
            out_reg->rax = (uint64_t)ReverseBytes(v); \
        } while (false)

    switch (type->primitive) {
        case PrimitiveKind::Void: {} break;
        case PrimitiveKind::Bool: {
            bool b;
            if (napi_get_value_bool(env, value, &b) != napi_ok) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected boolean", GetValueType(instance, value));
                return;
            }

            out_reg->rax = (uint64_t)b;
        } break;
        case PrimitiveKind::Int8: { RETURN_INTEGER(int8_t); } break;
        case PrimitiveKind::UInt8: { RETURN_INTEGER(uint8_t); } break;
        case PrimitiveKind::Int16: { RETURN_INTEGER(int16_t); } break;
        case PrimitiveKind::Int16S: { RETURN_INTEGER_SWAP(int16_t); } break;
        case PrimitiveKind::UInt16: { RETURN_INTEGER(uint16_t); } break;
        case PrimitiveKind::UInt16S: { RETURN_INTEGER_SWAP(uint16_t); } break;
        case PrimitiveKind::Int32: { RETURN_INTEGER(int32_t); } break;
        case PrimitiveKind::Int32S: { RETURN_INTEGER_SWAP(int32_t); } break;
        case PrimitiveKind::UInt32: { RETURN_INTEGER(uint32_t); } break;
        case PrimitiveKind::UInt32S: { RETURN_INTEGER_SWAP(uint32_t); } break;
        case PrimitiveKind::Int64: { RETURN_INTEGER(int64_t); } break;
        case PrimitiveKind::Int64S: { RETURN_INTEGER_SWAP(int64_t); } break;
        case PrimitiveKind::UInt64: { RETURN_INTEGER(uint64_t); } break;
        case PrimitiveKind::UInt64S: { RETURN_INTEGER_SWAP(uint64_t); } break;
        case PrimitiveKind::String: {
            const char *str;
            if (!PushString(value, 1, &str)) [[unlikely]]
                return;

            out_reg->rax = (uint64_t)str;
        } break;
        case PrimitiveKind::String16: {
            const char16_t *str16;
            if (!PushString16(value, 1, &str16)) [[unlikely]]
                return;

            out_reg->rax = (uint64_t)str16;
        } break;
        case PrimitiveKind::String32: {
            const char32_t *str32;
            if (!PushString32(value, 1, &str32)) [[unlikely]]
                return;

            out_reg->rax = (uint64_t)str32;
        } break;
        case PrimitiveKind::Pointer: {
            void *ptr;
            if (!PushPointer(value, type, 1, &ptr)) [[unlikely]]
                return;

            out_reg->rax = (uint64_t)ptr;
        } break;
        case PrimitiveKind::Record:
        case PrimitiveKind::Union: {
            if (!IsObject(value)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected object", GetValueType(instance, value));
                return;
            }

            Napi::Object obj = value.As<Napi::Object>();

            if (return_ptr) {
                if (!PushObject(obj, type, return_ptr))
                    return;
                out_reg->rax = (uint64_t)return_ptr;
            } else {
                PushObject(obj, type, (uint8_t *)&out_reg->rax);
            }
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: {
            float f;
            if (!TryNumber(value, &f)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                return;
            }

            memset(&out_reg->xmm0, 0, 8);
            memcpy(&out_reg->xmm0, &f, 4);
        } break;
        case PrimitiveKind::Float64: {
            double d;
            if (!TryNumber(value, &d)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                return;
            }

            out_reg->xmm0 = d;
        } break;
        case PrimitiveKind::Callback: {
            void *ptr;
            if (!PushCallback(value, type, &ptr)) [[unlikely]]
                return;

            out_reg->rax = (uint64_t)ptr;
        } break;

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

#undef RETURN_INTEGER_SWAP
#undef RETURN_INTEGER

    err_guard.Disable();
}

}

#endif
