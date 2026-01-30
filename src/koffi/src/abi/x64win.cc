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

extern "C" uint64_t ForwardCallG(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" float ForwardCallF(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" double ForwardCallD(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" uint64_t ForwardCallXG(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" float ForwardCallXF(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" double ForwardCallXD(const void *func, uint8_t *sp, uint8_t **out_old_sp);

extern "C" napi_value CallSwitchStack(Napi::Function *func, size_t argc, napi_value *argv,
                                      uint8_t *old_sp, Span<uint8_t> *new_stack,
                                      napi_value (*call)(Napi::Function *func, size_t argc, napi_value *argv));

enum class AbiOpcode : int8_t {
    #define PRIMITIVE(Name) Name,
    #include "../primitives.inc"
    Aggregate,
    End
};

bool AnalyseFunction(Napi::Env, InstanceData *, FunctionInfo *func)
{
    func->ret.regular = IsRegularSize(func->ret.type->size, 8);

    for (ParameterInfo &param: func->parameters) {
        param.regular = IsRegularSize(param.type->size, 8);

        if (param.type->primitive == PrimitiveKind::Record || param.type->primitive == PrimitiveKind::Union) {
            func->instructions.Append(AbiOpcode::Aggregate);
        } else {
            func->instructions.Append((AbiOpcode)param.type->primitive);
        }

        func->forward_fp |= IsFloat(param.type);
    }

    func->instructions.Append(AbiOpcode::End);
    func->args_size = AlignLen(8 * std::max((Size)4, func->parameters.len + !func->ret.regular), 16);

    return true;
}

namespace {
#if defined(MUST_TAIL)
    #define OP(Code) \
            PRESERVE_NONE bool Handle ## Code(CallData *call, const FunctionInfo *func, const Napi::CallbackInfo &info, uint64_t *base, Size i)
    #define NEXT() \
            do { \
                AbiOpcode next = func->instructions[i + 1]; \
                MUST_TAIL return ForwardDispatch[(int)next](call, func, info, base, i + 1); \
            } while (false)

    PRESERVE_NONE typedef bool ForwardFunc(CallData *call, const FunctionInfo *func, const Napi::CallbackInfo &info, uint64_t *base, Size i);

    extern ForwardFunc *const ForwardDispatch[256];
#else
    #define OP(Code) \
        case AbiOpcode::Code:
    #define NEXT() \
        break

    bool PushArguments(CallData *call, const FunctionInfo *func, const Napi::CallbackInfo &info, uint64_t *base)
    {
        for (Size i = 0;; i++) {
            switch (func->instructions[i]) {
#endif

#define INTEGER(CType) \
        do { \
            const ParameterInfo &param = func->parameters[i]; \
            Napi::Value value = info[param.offset]; \
             \
            CType v; \
            if (!TryNumber(value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, value)); \
                return false; \
            } \
             \
            *(base + i) = (uint64_t)v; \
        } while (false)
#define INTEGER_SWAP(CType) \
        do { \
            const ParameterInfo &param = func->parameters[i]; \
            Napi::Value value = info[param.offset]; \
             \
            CType v; \
            if (!TryNumber(value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, value)); \
                return false; \
            } \
             \
            *(base + i) = (uint64_t)ReverseBytes(v); \
        } while (false)

    OP(Void) { K_UNREACHABLE(); return false; }

    OP(Bool) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        bool b;
        if (napi_get_value_bool(call->env, value, &b) != napi_ok) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected boolean", GetValueType(call->instance, value));
            return false;
        }

        *(bool *)(base + i) = b;

        NEXT();
    }

    OP(Int8) { INTEGER(int8_t); NEXT(); }
    OP(UInt8) { INTEGER(uint8_t); NEXT(); }
    OP(Int16) { INTEGER(int16_t); NEXT(); }
    OP(Int16S) { INTEGER_SWAP(int16_t); NEXT(); }
    OP(UInt16) { INTEGER(uint16_t); NEXT(); }
    OP(UInt16S) { INTEGER_SWAP(uint16_t); NEXT(); }
    OP(Int32) { INTEGER(int32_t); NEXT(); }
    OP(Int32S) { INTEGER_SWAP(int32_t); NEXT(); }
    OP(UInt32) { INTEGER(uint32_t); NEXT(); }
    OP(UInt32S) { INTEGER_SWAP(uint32_t); NEXT(); }
    OP(Int64) { INTEGER(int64_t); NEXT(); }
    OP(Int64S) { INTEGER_SWAP(int64_t); NEXT(); }
    OP(UInt64) { INTEGER(uint64_t); NEXT(); }
    OP(UInt64S) { INTEGER_SWAP(uint64_t); NEXT(); }

    OP(String) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        const char *str;
        if (!call->PushString(value, param.directions, &str)) [[unlikely]]
            return false;

        *(const char **)(base + i) = str;

        NEXT();
    }

    OP(String16) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        const char16_t *str16;
        if (!call->PushString16(value, param.directions, &str16)) [[unlikely]]
            return false;

        *(const char16_t **)(base + i) = str16;

        NEXT();
    }

    OP(String32) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        const char32_t *str32;
        if (!call->PushString32(value, param.directions, &str32)) [[unlikely]]
            return false;

        *(const char32_t **)(base + i) = str32;

        NEXT();
    }

    OP(Pointer) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        void *ptr;
        if (!call->PushPointer(value, param.type, param.directions, &ptr)) [[unlikely]]
            return false;

        *(void **)(base + i) = ptr;

        NEXT();
    }

    OP(Record) { K_UNREACHABLE(); return false; }
    OP(Union) { K_UNREACHABLE(); return false; }
    OP(Array) { K_UNREACHABLE(); return false; }

    OP(Float32) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        float f;
        if (!TryNumber(value, &f)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, value));
            return false;
        }

        memset(base + i, 0, 8);
        memcpy(base + i, &f, 4);

        NEXT();
    }

    OP(Float64) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        double d;
        if (!TryNumber(value, &d)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, value));
            return false;
        }

        *(double *)(base + i) = d;

        NEXT();
    }

    OP(Callback) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        void *ptr;
        if (!call->PushCallback(value, param.type, &ptr)) [[unlikely]]
            return false;

        *(void **)(base + i) = ptr;

        NEXT();
    }

    OP(Prototype) { K_UNREACHABLE(); return false; }

    OP(Aggregate) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        if (!IsObject(value)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected object", GetValueType(call->instance, value));
            return false;
        }

        uint8_t *ptr;
        if (param.regular) {
            ptr = (uint8_t *)(base + i);
        } else {
            ptr = call->AllocHeap(param.type->size, 16);
            *(uint8_t **)(base + i) = ptr;
        }

        Napi::Object obj = value.As<Napi::Object>();
        if (!call->PushObject(obj, param.type, ptr))
            return false;

        NEXT();
    }

    OP(End) { return true; }

#if defined(MUST_TAIL)
    ForwardFunc *const ForwardDispatch[256] = {
        #define PRIMITIVE(Name) Handle ## Name,
        #include "../primitives.inc"
        HandleAggregate,
        HandleEnd
    };
#else
            }
        }

        K_UNREACHABLE();
    }
#endif

#undef NEXT
#undef OP
}

bool CallData::Prepare(const FunctionInfo *func, const Napi::CallbackInfo &info)
{
    uint64_t *base = AllocStack<uint64_t>(func->args_size);
    if (!base) [[unlikely]]
        return false;
    new_sp = (uint8_t *)base;

    if (!func->ret.regular) {
        return_ptr = AllocHeap(func->ret.type->size, 16);
        *(uint8_t **)(base++) = return_ptr;
    } else {
        return_ptr = result.buf;
    }

#if defined(MUST_TAIL)
    AbiOpcode first = func->instructions[0];
    return ForwardDispatch[(int)first](this, func, info, base, 0);
#else
    return PushArguments(this, func, info, base);
#endif
}

void CallData::Execute(const FunctionInfo *func, void *native)
{
    TEB *teb = GetTEB();

    // Restore previous stack limits at the end
    K_DEFER_C(exception_list = teb->ExceptionList,
              base = teb->StackBase,
              limit = teb->StackLimit,
              dealloc = teb->DeallocationStack,
              guaranteed = teb->GuaranteedStackBytes,
              stfs = teb->SameTebFlags) {
        teb->ExceptionList = exception_list;
        teb->StackBase = base;
        teb->StackLimit = limit;
        teb->DeallocationStack = dealloc;
        teb->GuaranteedStackBytes = guaranteed;
        teb->SameTebFlags = stfs;

        instance->last_error = teb->LastErrorValue;
    };

    // Adjust stack limits so SEH works correctly
    teb->ExceptionList = (void *)-1; // EXCEPTION_CHAIN_END
    teb->StackBase = mem->stack0.end();
    teb->StackLimit = mem->stack0.ptr;
    teb->DeallocationStack = mem->stack0.ptr;
    teb->GuaranteedStackBytes = 0;
    teb->SameTebFlags &= ~0x200;

    teb->LastErrorValue = instance->last_error;

#define PERFORM_CALL(Suffix) \
        ([&]() { \
            auto ret = (func->forward_fp ? ForwardCallX ## Suffix(native, new_sp, &old_sp) \
                                         : ForwardCall ## Suffix(native, new_sp, &old_sp)); \
            return ret; \
        })()

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
        case PrimitiveKind::String32:
        case PrimitiveKind::Pointer:
        case PrimitiveKind::Record:
        case PrimitiveKind::Union:
        case PrimitiveKind::Callback: { result.u64 = PERFORM_CALL(G); } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: { result.f = PERFORM_CALL(F); } break;
        case PrimitiveKind::Float64: { result.d = PERFORM_CALL(D); } break;

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

#undef PERFORM_CALL
}

Napi::Value CallData::Complete(const FunctionInfo *func)
{
    K_DEFER {
       PopOutArguments();

        if (func->ret.type->dispose) {
            func->ret.type->dispose(env, func->ret.type, result.ptr);
        }
    };

    switch (func->ret.type->primitive) {
        case PrimitiveKind::Void: return env.Undefined();
        case PrimitiveKind::Bool: return Napi::Boolean::New(env, result.u8 & 0x1);
        case PrimitiveKind::Int8: return NewInt(env, result.i8);
        case PrimitiveKind::UInt8: return NewInt(env, result.u8);
        case PrimitiveKind::Int16: return NewInt(env, result.i16);
        case PrimitiveKind::Int16S: return NewInt(env, ReverseBytes(result.i16));
        case PrimitiveKind::UInt16: return NewInt(env, result.u16);
        case PrimitiveKind::UInt16S: return NewInt(env, ReverseBytes(result.u16));
        case PrimitiveKind::Int32: return NewInt(env, result.i32);
        case PrimitiveKind::Int32S: return NewInt(env, ReverseBytes(result.i32));
        case PrimitiveKind::UInt32: return NewInt(env, result.u32);
        case PrimitiveKind::UInt32S: return NewInt(env, ReverseBytes(result.u32));
        case PrimitiveKind::Int64: return NewInt(env, result.i64);
        case PrimitiveKind::Int64S: return NewInt(env, ReverseBytes(result.i64));
        case PrimitiveKind::UInt64: return NewInt(env, result.u64);
        case PrimitiveKind::UInt64S: return NewInt(env, ReverseBytes(result.u64));
        case PrimitiveKind::String: return result.ptr ? Napi::String::New(env, (const char *)result.ptr) : env.Null();
        case PrimitiveKind::String16: return result.ptr ? Napi::String::New(env, (const char16_t *)result.ptr) : env.Null();
        case PrimitiveKind::String32: return result.ptr ? MakeStringFromUTF32(env, (const char32_t *)result.ptr) : env.Null();
        case PrimitiveKind::Pointer: return result.ptr ? WrapPointer(env, func->ret.type->ref.type, result.ptr) : env.Null();
        case PrimitiveKind::Callback: return result.ptr ? WrapCallback(env, func->ret.type->ref.type, result.ptr) : env.Null();
        case PrimitiveKind::Record:
        case PrimitiveKind::Union: {
            Napi::Object obj = DecodeObject(env, return_ptr, func->ret.type);
            return obj;
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: return Napi::Number::New(env, (double)result.f);
        case PrimitiveKind::Float64: return Napi::Number::New(env, result.d);

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

    K_UNREACHABLE();
}

void CallData::Relay(Size idx, uint8_t *own_sp, uint8_t *caller_sp, bool switch_stack, BackRegisters *out_reg)
{
    if (env.IsExceptionPending()) [[unlikely]]
        return;

    TEB *teb = GetTEB();

    // Restore previous stack limits at the end
    K_DEFER_C(base = teb->StackBase,
               limit = teb->StackLimit,
               dealloc = teb->DeallocationStack) {
        teb->StackBase = base;
        teb->StackLimit = limit;
        teb->DeallocationStack = dealloc;
    };

    // Adjust stack limits so SEH works correctly
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
                int8_t i = *(int8_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, i);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt8: {
                uint8_t i = *(uint8_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, i);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int16: {
                int16_t i = *(int16_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, i);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int16S: {
                int16_t i = *(int16_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, ReverseBytes(i));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt16: {
                uint16_t i = *(uint16_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, i);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt16S: {
                uint16_t i = *(uint16_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, ReverseBytes(i));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int32: {
                int32_t i = *(int32_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, i);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int32S: {
                int32_t i = *(int32_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, ReverseBytes(i));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt32: {
                uint32_t i = *(uint32_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, i);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt32S: {
                uint32_t i = *(uint32_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                Napi::Value arg = NewInt(env, ReverseBytes(i));
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
        ret = CallSwitchStack(&func, (size_t)arguments.len, arguments.data, old_sp, &mem->stack,
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
