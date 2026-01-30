// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#if defined(__i386__) || defined(_M_IX86)

#include "lib/native/base/base.hh"
#include "../ffi.hh"
#include "../call.hh"
#include "../util.hh"
#if defined(_WIN32)
    #include "../win32.hh"
#endif

#include <napi.h>

namespace K {

struct BackRegisters {
    uint32_t eax;
    uint32_t edx;
    union {
        double d;
        float f;
    } x87;
    int ret_type;
    int ret_pop;
};

extern "C" uint64_t ForwardCallG(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" float ForwardCallF(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" double ForwardCallD(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" uint64_t ForwardCallRG(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" float ForwardCallRF(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" double ForwardCallRD(const void *func, uint8_t *sp, uint8_t **out_old_sp);

extern "C" napi_value CallSwitchStack(Napi::Function *func, size_t argc, napi_value *argv,
                                      uint8_t *old_sp, Span<uint8_t> *new_stack,
                                      napi_value (*call)(Napi::Function *func, size_t argc, napi_value *argv));

enum class AbiOpcode : int8_t {
    #define PRIMITIVE(Name) Name,
    #include "../primitives.inc"
    End
};

bool AnalyseFunction(Napi::Env env, InstanceData *instance, FunctionInfo *func)
{
    if (!func->lib && func->convention != CallConvention::Cdecl &&
                      func->convention != CallConvention::Stdcall) {
        ThrowError<Napi::Error>(env, "Only Cdecl and Stdcall callbacks are supported");
        return false;
    }

    int fast = (func->convention == CallConvention::Fastcall) ? 2 :
               (func->convention == CallConvention::Thiscall) ? 1 : 0;
    func->fast = fast;

    if (func->ret.type->primitive != PrimitiveKind::Record &&
            func->ret.type->primitive != PrimitiveKind::Union) {
        func->ret.trivial = true;
#if defined(_WIN32) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    } else {
        func->ret.trivial = IsRegularSize(func->ret.type->size, 8);

    #if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
        if (func->ret.type->members.len == 1) {
            const RecordMember &member = func->ret.type->members[0];

            if (member.type->primitive == PrimitiveKind::Float32) {
                func->ret.fast = 1;
            } else if (member.type->primitive == PrimitiveKind::Float64) {
                func->ret.fast = 2;
            }
        }
    #endif
#endif
    }
#if !defined(_WIN32)
    if (fast && !func->ret.trivial) {
        func->ret.fast = 1;
        fast--;
    }
#endif

    Size params_size = 0;
    for (ParameterInfo &param: func->parameters) {
        if (fast && param.type->size <= 4) {
            param.fast = 1;
            fast--;
        }

        func->instructions.Append((AbiOpcode)param.type->primitive);
        params_size += std::max(4, AlignLen(param.type->size, 4));
    }

    func->instructions.Append(AbiOpcode::End);
    func->args_size = params_size + 4 * !func->ret.trivial;

    switch (func->convention) {
        case CallConvention::Cdecl: {
            func->decorated_name = Fmt(&instance->str_alloc, "_%1", func->name).ptr;
        } break;
        case CallConvention::Stdcall: {
            K_ASSERT(!func->variadic);
            func->decorated_name = Fmt(&instance->str_alloc, "_%1@%2", func->name, params_size).ptr;
        } break;
        case CallConvention::Fastcall: {
            K_ASSERT(!func->variadic);
            func->decorated_name = Fmt(&instance->str_alloc, "@%1@%2", func->name, params_size).ptr;
            func->args_size += 16;
        } break;
        case CallConvention::Thiscall: {
            K_ASSERT(!func->variadic);
            func->args_size += 16;
        } break;
    }

    return true;
}

bool CallData::Prepare(const FunctionInfo *func, const Napi::CallbackInfo &info)
{
    uint32_t *args_ptr = AllocStack<uint32_t>(func->args_size);
    uint32_t *fast_ptr = nullptr;

    if (!args_ptr) [[unlikely]]
        return false;
    if (func->fast) {
        fast_ptr = args_ptr;
        args_ptr += 4;
    }
    if (!func->ret.trivial) {
        return_ptr = AllocHeap(func->ret.type->size, 16);
        *((func->ret.fast ? fast_ptr : args_ptr)++) = (uint32_t)return_ptr;
    }

    Size i = -1;

#if defined(__GNUC__) || defined(__clang__)
    static const void *const DispatchTable[] = {
        #define PRIMITIVE(Name) && Name,
        #include "../primitives.inc"
        && End
    };

    #define LOOP
    #define CASE(Primitive) \
        do { \
            AbiOpcode next = func->instructions[++i]; \
            goto *DispatchTable[(int)next]; \
        } while (false); \
        Primitive:
    #define OR(Primitive) \
        Primitive:
#else
    #define LOOP \
        while (++i < func->parameters.len) \
            switch (func->instructions[i])
    #define CASE(Primitive) \
        break; \
        case AbiOpcode::Primitive:
    #define OR(Primitive) \
        case AbiOpcode::Primitive:
#endif

#define PUSH_INTEGER_32(CType) \
        do { \
            const ParameterInfo &param = func->parameters[i]; \
            Napi::Value value = info[param.offset]; \
             \
            CType v; \
            if (!TryNumber(value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value)); \
                return false; \
            } \
             \
            *((param.fast ? fast_ptr : args_ptr)++) = (uint32_t)v; \
        } while (false)
#define PUSH_INTEGER_32_SWAP(CType) \
        do { \
            const ParameterInfo &param = func->parameters[i]; \
            Napi::Value value = info[param.offset]; \
             \
            CType v; \
            if (!TryNumber(value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value)); \
                return false; \
            } \
             \
            *((param.fast ? fast_ptr : args_ptr)++) = (uint32_t)ReverseBytes(v); \
        } while (false)
#define PUSH_INTEGER_64(CType) \
        do { \
            const ParameterInfo &param = func->parameters[i]; \
            Napi::Value value = info[param.offset]; \
             \
            CType v; \
            if (!TryNumber(value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value)); \
                return false; \
            } \
             \
            *(uint64_t *)args_ptr = (uint64_t)v; \
            args_ptr += 2; \
        } while (false)
#define PUSH_INTEGER_64_SWAP(CType) \
        do { \
            const ParameterInfo &param = func->parameters[i]; \
            Napi::Value value = info[param.offset]; \
             \
            CType v; \
            if (!TryNumber(value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value)); \
                return false; \
            } \
             \
            *(uint64_t *)args_ptr = (uint64_t)ReverseBytes(v); \
            args_ptr += 2; \
        } while (false)

    LOOP {
        CASE(Void) { K_UNREACHABLE(); };

        CASE(Bool) {
            const ParameterInfo &param = func->parameters[i];
            Napi::Value value = info[param.offset];

            bool b;
            if (napi_get_value_bool(env, value, &b) != napi_ok) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected boolean", GetValueType(instance, value));
                return false;
            }

            *(bool *)((param.fast ? fast_ptr : args_ptr)++) = b;
        };

        CASE(Int8) { PUSH_INTEGER_32(int8_t); };
        CASE(UInt8) { PUSH_INTEGER_32(uint8_t); };
        CASE(Int16) { PUSH_INTEGER_32(int16_t); };
        CASE(Int16S) { PUSH_INTEGER_32_SWAP(int16_t); };
        CASE(UInt16) { PUSH_INTEGER_32(uint16_t); };
        CASE(UInt16S) { PUSH_INTEGER_32_SWAP(uint16_t); };
        CASE(Int32) { PUSH_INTEGER_32(int32_t); };
        CASE(Int32S) { PUSH_INTEGER_32_SWAP(int32_t); };
        CASE(UInt32) { PUSH_INTEGER_32(uint32_t); };
        CASE(UInt32S) { PUSH_INTEGER_32_SWAP(uint32_t); };
        CASE(Int64) { PUSH_INTEGER_64(int64_t); };
        CASE(Int64S) { PUSH_INTEGER_64_SWAP(int64_t); };
        CASE(UInt64) { PUSH_INTEGER_64(uint64_t); };
        CASE(UInt64S) { PUSH_INTEGER_64_SWAP(uint64_t); };

        CASE(String) {
            const ParameterInfo &param = func->parameters[i];
            Napi::Value value = info[param.offset];

            const char *str;
            if (!PushString(value, param.directions, &str)) [[unlikely]]
                return false;

            *(const char **)((param.fast ? fast_ptr : args_ptr)++) = str;
        };
        CASE(String16) {
            const ParameterInfo &param = func->parameters[i];
            Napi::Value value = info[param.offset];

            const char16_t *str16;
            if (!PushString16(value, param.directions, &str16)) [[unlikely]]
                return false;

            *(const char16_t **)((param.fast ? fast_ptr : args_ptr)++) = str16;
        };
        CASE(String32) {
            const ParameterInfo &param = func->parameters[i];
            Napi::Value value = info[param.offset];

            const char32_t *str32;
            if (!PushString32(value, param.directions, &str32)) [[unlikely]]
                return false;

            *(const char32_t **)((param.fast ? fast_ptr : args_ptr)++) = str32;
        };

        CASE(Pointer) {
            const ParameterInfo &param = func->parameters[i];
            Napi::Value value = info[param.offset];

            void *ptr;
            if (!PushPointer(value, param.type, param.directions, &ptr)) [[unlikely]]
                return false;

            *(void **)((param.fast ? fast_ptr : args_ptr)++) = ptr;
        };

        CASE(Record) OR(Union) {
            const ParameterInfo &param = func->parameters[i];
            Napi::Value value = info[param.offset];

            if (!IsObject(value)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected object", GetValueType(instance, value));
                return false;
            }

            Napi::Object obj = value.As<Napi::Object>();

            if (param.fast) {
                uint8_t *ptr = (uint8_t *)(fast_ptr++);
                if (!PushObject(obj, param.type, ptr))
                    return false;
            } else {
                uint8_t *ptr = (uint8_t *)args_ptr;
                if (!PushObject(obj, param.type, ptr))
                    return false;
                args_ptr = (uint32_t *)AlignUp(ptr + param.type->size, 4);
            }
        };
        CASE(Array) { K_UNREACHABLE(); };

        CASE(Float32) {
            const ParameterInfo &param = func->parameters[i];
            Napi::Value value = info[param.offset];

            float f;
            if (!TryNumber(value, &f)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                return false;
            }

            *(float *)((param.fast ? fast_ptr : args_ptr)++) = f;
        };
        CASE(Float64) {
            const ParameterInfo &param = func->parameters[i];
            Napi::Value value = info[param.offset];

            double d;
            if (!TryNumber(value, &d)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                return false;
            }

            *(double *)args_ptr = d;
            args_ptr += 2;
        };

        CASE(Callback) {
            const ParameterInfo &param = func->parameters[i];
            Napi::Value value = info[param.offset];

            void *ptr;
            if (!PushCallback(value, param.type, &ptr)) [[unlikely]]
                return false;

            *(void **)((param.fast ? fast_ptr : args_ptr)++) = ptr;
        };

        CASE(Prototype) { K_UNREACHABLE(); };

        CASE(End) { /* End loop */ };
    }

#undef PUSH_INTEGER_64_SWAP
#undef PUSH_INTEGER_64
#undef PUSH_INTEGER_32_SWAP
#undef PUSH_INTEGER_32

#undef OR
#undef CASE
#undef LOOP

    new_sp = mem->stack.end();

    return true;
}

void CallData::Execute(const FunctionInfo *func, void *native)
{
#if defined(_WIN32)
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
#endif

#define PERFORM_CALL(Suffix) \
        ([&]() { \
            auto ret = (func->fast ? ForwardCallR ## Suffix(native, new_sp, &old_sp) \
                                   : ForwardCall ## Suffix(native, new_sp, &old_sp)); \
            return ret; \
        })()

    // Execute and convert return value
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
        case PrimitiveKind::Callback: { result.u64 = PERFORM_CALL(G); } break;
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
        case PrimitiveKind::Record:
        case PrimitiveKind::Union: {
            if (!func->ret.fast) {
                result.u64 = PERFORM_CALL(G);
            } else if (func->ret.fast == 1) {
                result.f = PERFORM_CALL(F);
            } else if (func->ret.fast == 2) {
                result.d = PERFORM_CALL(D);
            } else {
                K_UNREACHABLE();
            }
        } break;
#else
        case PrimitiveKind::Record:
        case PrimitiveKind::Union: { result.u64 = PERFORM_CALL(G); } break;
#endif
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
            const uint8_t *ptr = return_ptr ? (const uint8_t *)return_ptr
                                            : (const uint8_t *)&result.buf;

            Napi::Object obj = DecodeObject(env, ptr, func->ret.type);
            return obj;
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: return Napi::Number::New(env, (double)result.f);
        case PrimitiveKind::Float64: return Napi::Number::New(env, result.d);

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

    K_UNREACHABLE();
}

void CallData::Relay(Size idx, uint8_t *, uint8_t *caller_sp, bool switch_stack, BackRegisters *out_reg)
{
    if (env.IsExceptionPending()) [[unlikely]]
        return;

#if defined(_WIN32)
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
#endif

    const TrampolineInfo &trampoline = shared.trampolines[idx];

    const FunctionInfo *proto = trampoline.proto;
    Napi::Function func = trampoline.func.Value();

    uint32_t *args_ptr = (uint32_t *)caller_sp;

    uint8_t *return_ptr = !proto->ret.trivial ? (uint8_t *)args_ptr[0] : nullptr;
    args_ptr += !proto->ret.trivial;

    if (proto->convention == CallConvention::Stdcall) {
        out_reg->ret_pop = (int)proto->args_size;
    } else {
#if defined(_WIN32)
        out_reg->ret_pop = 0;
#else
        out_reg->ret_pop = return_ptr ? 4 : 0;
#endif
    }

    K_DEFER_N(err_guard) {
        int pop = out_reg->ret_pop;
        memset(out_reg, 0, K_SIZE(*out_reg));
        out_reg->ret_type = 0;
        out_reg->ret_pop = pop;
    };

    if (trampoline.generation >= 0 && trampoline.generation != (int32_t)mem->generation) [[unlikely]] {
        ThrowError<Napi::Error>(env, "Cannot use non-registered callback beyond FFI call");
        return;
    }

    LocalArray<napi_value, MaxParameters + 1> arguments;

    arguments.Append(!trampoline.recv.IsEmpty() ? trampoline.recv.Value() : env.Undefined());

    // Convert to JS arguments
    for (Size i = 0; i < proto->parameters.len; i++) {
        const ParameterInfo &param = proto->parameters[i];
        K_ASSERT(param.directions >= 1 && param.directions <= 3);

        switch (param.type->primitive) {
            case PrimitiveKind::Void: { K_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                bool b = *(bool *)(args_ptr++);

                Napi::Value arg = Napi::Boolean::New(env, b);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int8: {
                int8_t v = *(int8_t *)(args_ptr++);

                Napi::Value arg = NewInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt8: {
                uint8_t v = *(uint8_t *)(args_ptr++);

                Napi::Value arg = NewInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int16: {
                int16_t v = *(int16_t *)(args_ptr++);

                Napi::Value arg = NewInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int16S: {
                int16_t v = *(int16_t *)(args_ptr++);

                Napi::Value arg = NewInt(env, ReverseBytes(v));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt16: {
                uint16_t v = *(uint16_t *)(args_ptr++);

                Napi::Value arg = NewInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt16S: {
                uint16_t v = *(uint16_t *)(args_ptr++);

                Napi::Value arg = NewInt(env, ReverseBytes(v));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int32: {
                int32_t v = *(int32_t *)(args_ptr++);

                Napi::Value arg = NewInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int32S: {
                int32_t v = *(int32_t *)(args_ptr++);

                Napi::Value arg = NewInt(env, ReverseBytes(v));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt32: {
                uint32_t v = *(uint32_t *)(args_ptr++);

                Napi::Value arg = NewInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt32S: {
                uint32_t v = *(uint32_t *)(args_ptr++);

                Napi::Value arg = NewInt(env, ReverseBytes(v));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int64: {
                int64_t v = *(int64_t *)args_ptr;
                args_ptr += 2;

                Napi::Value arg = NewInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int64S: {
                int64_t v = *(int64_t *)args_ptr;
                args_ptr += 2;

                Napi::Value arg = NewInt(env, ReverseBytes(v));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt64: {
                uint64_t v = *(uint64_t *)args_ptr;
                args_ptr += 2;

                Napi::Value arg = NewInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt64S: {
                uint64_t v = *(uint64_t *)args_ptr;
                args_ptr += 2;

                Napi::Value arg = NewInt(env, ReverseBytes(v));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::String: {
                const char *str = *(const char **)(args_ptr++);

                Napi::Value arg = str ? Napi::String::New(env, str) : env.Null();
                arguments.Append(arg);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, str);
                }
            } break;
            case PrimitiveKind::String16: {
                const char16_t *str16 = *(const char16_t **)(args_ptr++);

                Napi::Value arg = str16 ? Napi::String::New(env, str16) : env.Null();
                arguments.Append(arg);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, str16);
                }
            } break;
            case PrimitiveKind::String32: {
                const char32_t *str32 = *(const char32_t **)(args_ptr++);

                Napi::Value arg = str32 ? MakeStringFromUTF32(env, str32) : env.Null();
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Pointer: {
                void *ptr2 = *(void **)(args_ptr++);

                Napi::Value p = ptr2 ? WrapPointer(env, param.type->ref.type, ptr2) : env.Null();
                arguments.Append(p);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, ptr2);
                }
            } break;
            case PrimitiveKind::Callback: {
                void *ptr2 = *(void **)(args_ptr++);

                Napi::Value p = ptr2 ? WrapCallback(env, param.type->ref.type, ptr2) : env.Null();
                arguments.Append(p);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, ptr2);
                }
            } break;
            case PrimitiveKind::Record:
            case PrimitiveKind::Union: {
                K_ASSERT(!param.fast);

                uint8_t *ptr = (uint8_t *)args_ptr;

                Napi::Object obj2 = DecodeObject(env, ptr, param.type);
                arguments.Append(obj2);

                args_ptr = (uint32_t *)AlignUp(ptr + param.type->size, 4);
            } break;
            case PrimitiveKind::Array: { K_UNREACHABLE(); } break;
            case PrimitiveKind::Float32: {
                float f = *(float *)(args_ptr++);

                Napi::Value arg = Napi::Number::New(env, (double)f);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Float64: {
                double d = *(double *)args_ptr;
                args_ptr += 2;

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

#define RETURN_INTEGER_32(CType) \
        do { \
            CType v; \
            if (!TryNumber(value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value)); \
                return; \
            } \
             \
            out_reg->eax = (uint32_t)v; \
            out_reg->ret_type = 0; \
        } while (false)
#define RETURN_INTEGER_32_SWAP(CType) \
        do { \
            CType v; \
            if (!TryNumber(value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value)); \
                return; \
            } \
             \
            out_reg->eax = (uint32_t)ReverseBytes(v); \
            out_reg->ret_type = 0; \
        } while (false)
#define RETURN_INTEGER_64(CType) \
        do { \
            CType v; \
            if (!TryNumber(value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value)); \
                return; \
            } \
             \
            out_reg->eax = (uint32_t)((uint64_t)v >> 32); \
            out_reg->edx = (uint32_t)((uint64_t)v & 0xFFFFFFFFu); \
            out_reg->ret_type = 0; \
        } while (false)
#define RETURN_INTEGER_64_SWAP(CType) \
        do { \
            CType v; \
            if (!TryNumber(value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value)); \
                return; \
            } \
             \
            out_reg->eax = (uint32_t)((uint64_t)v >> 32); \
            out_reg->edx = (uint32_t)((uint64_t)v & 0xFFFFFFFFu); \
            out_reg->ret_type = 0; \
        } while (false)

    switch (type->primitive) {
        case PrimitiveKind::Void: { out_reg->ret_type = 0; } break;
        case PrimitiveKind::Bool: {
            bool b;
            if (napi_get_value_bool(env, value, &b) != napi_ok) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected boolean", GetValueType(instance, value));
                return;
            }

            out_reg->eax = (uint32_t)b;
            out_reg->ret_type = 0;
        } break;
        case PrimitiveKind::Int8: { RETURN_INTEGER_32(int8_t); } break;
        case PrimitiveKind::UInt8: { RETURN_INTEGER_32(uint8_t); } break;
        case PrimitiveKind::Int16: { RETURN_INTEGER_32(int16_t); } break;
        case PrimitiveKind::Int16S: { RETURN_INTEGER_32_SWAP(int16_t); } break;
        case PrimitiveKind::UInt16: { RETURN_INTEGER_32(uint16_t); } break;
        case PrimitiveKind::UInt16S: { RETURN_INTEGER_32_SWAP(uint16_t); } break;
        case PrimitiveKind::Int32: { RETURN_INTEGER_32(int32_t); } break;
        case PrimitiveKind::Int32S: { RETURN_INTEGER_32_SWAP(int32_t); } break;
        case PrimitiveKind::UInt32: { RETURN_INTEGER_32(uint32_t); } break;
        case PrimitiveKind::UInt32S: { RETURN_INTEGER_32_SWAP(uint32_t); } break;
        case PrimitiveKind::Int64: { RETURN_INTEGER_64(int64_t); } break;
        case PrimitiveKind::Int64S: { RETURN_INTEGER_64_SWAP(int64_t); } break;
        case PrimitiveKind::UInt64: { RETURN_INTEGER_64(uint64_t); } break;
        case PrimitiveKind::UInt64S: { RETURN_INTEGER_64_SWAP(uint64_t); } break;
        case PrimitiveKind::String: {
            const char *str;
            if (!PushString(value, 1, &str)) [[unlikely]]
                return;

            out_reg->eax = (uint32_t)str;
            out_reg->ret_type = 0;
        } break;
        case PrimitiveKind::String16: {
            const char16_t *str16;
            if (!PushString16(value, 1, &str16)) [[unlikely]]
                return;

            out_reg->eax = (uint32_t)str16;
            out_reg->ret_type = 0;
        } break;
        case PrimitiveKind::String32: {
            const char32_t *str32;
            if (!PushString32(value, 1, &str32)) [[unlikely]]
                return;

            out_reg->eax = (uint32_t)str32;
            out_reg->ret_type = 0;
        } break;
        case PrimitiveKind::Pointer: {
            void *ptr;
            if (!PushPointer(value, type, 1, &ptr)) [[unlikely]]
                return;

            out_reg->eax = (uint32_t)ptr;
            out_reg->ret_type = 0;
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
                out_reg->eax = (uint32_t)return_ptr;
            } else {
                PushObject(obj, type, (uint8_t *)&out_reg->eax);
            }

            out_reg->ret_type = 0;
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: {
            float f;
            if (!TryNumber(value, &f)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                return;
            }

            out_reg->x87.f = f;
            out_reg->ret_type = 1;
        } break;
        case PrimitiveKind::Float64: {
            double d;
            if (!TryNumber(value, &d)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                return;
            }

            out_reg->x87.d = d;
            out_reg->ret_type = 2;
        } break;
        case PrimitiveKind::Callback: {
            void *ptr;
            if (!PushCallback(value, type, &ptr)) [[unlikely]]
                return;

            out_reg->eax = (uint32_t)ptr;
            out_reg->ret_type = 0;
        } break;

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

#undef RETURN_INTEGER_64_SWAP
#undef RETURN_INTEGER_64
#undef RETURN_INTEGER_32_SWAP
#undef RETURN_INTEGER_32

    err_guard.Disable();
}

}

#endif
