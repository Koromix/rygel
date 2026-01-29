// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#if defined(__aarch64__) || defined(_M_ARM64)

#include "lib/native/base/base.hh"
#include "../ffi.hh"
#include "../call.hh"
#include "../util.hh"
#if defined(_WIN32)
    #include "../win32.hh"
#endif

#include <napi.h>

namespace K {

struct HfaInfo {
    int count;
    bool float32;
};

struct X0X1Ret {
    uint64_t x0;
    uint64_t x1;
};
struct HfaRet {
    double d0;
    double d1;
    double d2;
    double d3;
};

struct BackRegisters {
    uint64_t x0;
    uint64_t x1;
    double d0;
    double d1;
    double d2;
    double d3;
};

extern "C" X0X1Ret ForwardCallGG(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" float ForwardCallF(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" HfaRet ForwardCallDDDD(const void *func, uint8_t *sp, uint8_t **out_old_sp);

extern "C" X0X1Ret ForwardCallXGG(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" float ForwardCallXF(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" HfaRet ForwardCallXDDDD(const void *func, uint8_t *sp, uint8_t **out_old_sp);

extern "C" napi_value CallSwitchStack(Napi::Function *func, size_t argc, napi_value *argv,
                                      uint8_t *old_sp, Span<uint8_t> *new_stack,
                                      napi_value (*call)(Napi::Function *func, size_t argc, napi_value *argv));

enum class AbiOpcode : int8_t {
    #define PRIMITIVE(Name) Name,
    #include "../primitives.inc"
    End
};

static HfaInfo IsHFA(const TypeInfo *type)
{
    bool float32 = false;
    bool float64 = false;
    int count = 0;

    count = AnalyseFlat(type, [&](const TypeInfo *type, int, int) {
        if (type->primitive == PrimitiveKind::Float32) {
            float32 = true;
        } else if (type->primitive == PrimitiveKind::Float64) {
            float64 = true;
        } else {
            float32 = true;
            float64 = true;
        }
    });

    HfaInfo info = {};

    if (count < 1 || count > 4)
        return info;
    if (float32 && float64)
        return info;

    info.count = count;
    info.float32 = float32;

    return info;
}

static inline void ExpandFloats(uint8_t *ptr, Size len, Size bytes)
{
    for (Size i = len - 1; i >= 0; i--) {
        const uint8_t *src = ptr + i * bytes;
        uint8_t *dest = ptr + i * 8;

        memmove(dest, src, bytes);
    }
}

static inline void CompactFloats(uint8_t *ptr, Size len, Size bytes)
{
    for (Size i = 0; i < len; i++) {
        const uint8_t *src = ptr + i * 8;
        uint8_t *dest = ptr + i * bytes;

        memmove(dest, src, bytes);
    }
}

bool AnalyseFunction(Napi::Env, InstanceData *, FunctionInfo *func)
{
    if (HfaInfo hfa = IsHFA(func->ret.type); hfa.count) {
        func->ret.vec_count = (int8_t)hfa.count;
        func->ret.vec_bytes = hfa.float32 ? 4 : 8;
    } else if (func->ret.type->size <= 16) {
        func->ret.gpr_count = (int8_t)((func->ret.type->size + 7) / 8);
    } else {
        func->ret.use_memory = true;
    }

    int gpr_avail = 8;
    int vec_avail = 8;
#if defined(_M_ARM64EC)
    if (func->variadic) {
        gpr_avail = 4;
    }
#endif

    for (ParameterInfo &param: func->parameters) {
        switch (param.type->primitive) {
            case PrimitiveKind::Void: { K_UNREACHABLE(); } break;

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
            case PrimitiveKind::Callback: {
#if defined(__APPLE__)
                if (param.variadic)
                    break;
#endif

                if (gpr_avail) {
                    param.gpr_count = 1;
                    gpr_avail--;
                }
            } break;
            case PrimitiveKind::Record:
            case PrimitiveKind::Union: {
                HfaInfo hfa = IsHFA(param.type);

#if defined(_M_ARM64EC)
                if (func->variadic) {
                    if (IsRegularSize(param.type->size, 8) && gpr_avail) {
                        param.gpr_count = 1;
                        gpr_avail--;
                    } else {
                        if (gpr_avail) {
                            param.gpr_count = 1;
                            gpr_avail--;
                        }
                        param.use_memory = true;
                    }

                    break;
                }
#endif

#if defined(_WIN32)
                if (param.variadic) {
                    hfa.count = 0;
                }
#elif defined(__APPLE__)
                if (param.variadic) {
                    param.use_memory = (param.type->size > 16);
                    break;
                }
#endif

                if (hfa.count) {
                    if (hfa.count <= vec_avail) {
                        param.vec_count = (int8_t)hfa.count;
                        param.vec_bytes = hfa.float32 ? 4 : 8;
                        vec_avail -= hfa.count;
                    } else {
                        vec_avail = 0;
                    }
                } else if (param.type->size <= 16) {
                    int gpr_count = (param.type->size + 7) / 8;

                    if (gpr_count <= gpr_avail) {
                        param.gpr_count = (int8_t)gpr_count;
                        gpr_avail -= gpr_count;
                    } else {
                        gpr_avail = 0;
                    }
                } else {
                    // Big types (more than 16 bytes) are replaced by a pointer
                    if (gpr_avail) {
                        param.gpr_count = 1;
                        gpr_avail--;
                    }
                    param.use_memory = true;
                }
            } break;
            case PrimitiveKind::Array: { K_UNREACHABLE(); } break;
            case PrimitiveKind::Float32:
            case PrimitiveKind::Float64: {
#if defined(_WIN32)
                if (param.variadic) {
                    if (gpr_avail) {
                        param.gpr_count = 1;
                        gpr_avail--;
                    }
                    break;
                }
#elif defined(__APPLE__)
                if (param.variadic)
                    break;
#endif

                if (vec_avail) {
                    param.vec_count = 1;
                    vec_avail--;
                }
            } break;

            case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
        }

        func->instructions.Append((AbiOpcode)param.type->primitive);
    }

    func->instructions.Append(AbiOpcode::End);
    func->args_size = 16 * func->parameters.len;
    func->forward_fp = (vec_avail < 8);

    return true;
}

bool CallData::Prepare(const FunctionInfo *func, const Napi::CallbackInfo &info)
{
    uint64_t *gpr_ptr = AllocStack<uint64_t>(17 * 8 + func->args_size);
    uint64_t *vec_ptr = gpr_ptr + 9;
    uint64_t *args_ptr = gpr_ptr + 17;

    if (!gpr_ptr) [[unlikely]]
        return false;
    if (func->ret.use_memory) {
        return_ptr = AllocHeap(func->ret.type->size, 16);
        gpr_ptr[8] = (uint64_t)return_ptr;
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

#if defined(_M_ARM64EC)
    if (func->variadic) {
        gpr_ptr[4] = (uint64_t)args_ptr;
        gpr_ptr[5] = 0;

        for (Size i = 4; i < func->parameters.len; i++) {
            const ParameterInfo &param = func->parameters[i];
            gpr_ptr[5] += std::max((Size)8, param.type->size);
        }
    }
#endif

#if defined(__APPLE__)
    #define PUSH_INTEGER(CType) \
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
            if (param.gpr_count) [[likely]] { \
                *(gpr_ptr++) = (uint64_t)v; \
            } else { \
                args_ptr = AlignUp(args_ptr, param.variadic ? 8 : param.type->align); \
                *args_ptr = (uint64_t)v; \
                args_ptr = (uint64_t *)((uint8_t *)args_ptr + param.type->size); \
            } \
        } while (false)
    #define PUSH_INTEGER_SWAP(CType) \
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
            if (param.gpr_count) [[likely]] { \
                *(gpr_ptr++) = (uint64_t)ReverseBytes(v); \
            } else { \
                args_ptr = AlignUp(args_ptr, param.variadic ? 8 : param.type->align); \
                *args_ptr = (uint64_t)ReverseBytes(v); \
                args_ptr = (uint64_t *)((uint8_t *)args_ptr + param.type->size); \
            } \
        } while (false)
#else
    #define PUSH_INTEGER(CType) \
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
            *((param.gpr_count ? gpr_ptr : args_ptr)++) = (uint64_t)v; \
        } while (false)
    #define PUSH_INTEGER_SWAP(CType) \
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
            *((param.gpr_count ? gpr_ptr : args_ptr)++) = (uint64_t)ReverseBytes(v); \
        } while (false)
#endif

    // Push arguments
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

#if defined(__APPLE__)
            if (param.gpr_count) [[likely]] {
                *(gpr_ptr++) = (uint64_t)b;
            } else {
                *(uint8_t *)args_ptr = b;
                args_ptr = (uint64_t *)((uint8_t *)args_ptr + 1);
            }
#else
            *((param.gpr_count ? gpr_ptr : args_ptr)++) = (uint64_t)b;
#endif
        };

        CASE(Int8) { PUSH_INTEGER(int8_t); };
        CASE(UInt8) { PUSH_INTEGER(uint8_t); };
        CASE(Int16) { PUSH_INTEGER(int16_t); };
        CASE(Int16S) { PUSH_INTEGER_SWAP(int16_t); };
        CASE(UInt16) { PUSH_INTEGER(uint16_t); };
        CASE(UInt16S) { PUSH_INTEGER_SWAP(uint16_t); };
        CASE(Int32) { PUSH_INTEGER(int32_t); };
        CASE(Int32S) { PUSH_INTEGER_SWAP(int32_t); };
        CASE(UInt32) { PUSH_INTEGER(uint32_t); };
        CASE(UInt32S) { PUSH_INTEGER_SWAP(uint32_t); };
        CASE(Int64) { PUSH_INTEGER(int64_t); };
        CASE(Int64S) { PUSH_INTEGER_SWAP(int64_t); };
        CASE(UInt64) { PUSH_INTEGER(uint64_t); };
        CASE(UInt64S) { PUSH_INTEGER_SWAP(uint64_t); };

        CASE(String) {
            const ParameterInfo &param = func->parameters[i];
            Napi::Value value = info[param.offset];

            const char *str;
            if (!PushString(value, param.directions, &str)) [[unlikely]]
                return false;

#if defined(__APPLE__)
            args_ptr = param.gpr_count ? args_ptr : AlignUp(args_ptr, 8);
#endif
            *(const char **)((param.gpr_count ? gpr_ptr : args_ptr)++) = str;
        };
        CASE(String16) {
            const ParameterInfo &param = func->parameters[i];
            Napi::Value value = info[param.offset];

            const char16_t *str16;
            if (!PushString16(value, param.directions, &str16)) [[unlikely]]
                return false;

#if defined(__APPLE__)
            args_ptr = param.gpr_count ? args_ptr : AlignUp(args_ptr, 8);
#endif
            *(const char16_t **)((param.gpr_count ? gpr_ptr : args_ptr)++) = str16;
        };
        CASE(String32) {
            const ParameterInfo &param = func->parameters[i];
            Napi::Value value = info[param.offset];

            const char32_t *str32;
            if (!PushString32(value, param.directions, &str32)) [[unlikely]]
                return false;

#if defined(__APPLE__)
            args_ptr = param.gpr_count ? args_ptr : AlignUp(args_ptr, 8);
#endif
            *(const char32_t **)((param.gpr_count ? gpr_ptr : args_ptr)++) = str32;
        };

        CASE(Pointer) {
            const ParameterInfo &param = func->parameters[i];
            Napi::Value value = info[param.offset];

            void *ptr;
            if (!PushPointer(value, param.type, param.directions, &ptr)) [[unlikely]]
                return false;

#if defined(__APPLE__)
            args_ptr = param.gpr_count ? args_ptr : AlignUp(args_ptr, 8);
#endif
            *(void **)((param.gpr_count ? gpr_ptr : args_ptr)++) = ptr;
        };

        CASE(Record) OR(Union) {
            const ParameterInfo &param = func->parameters[i];
            Napi::Value value = info[param.offset];

            if (!IsObject(value)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected object", GetValueType(instance, value));
                return false;
            }

            Napi::Object obj = value.As<Napi::Object>();

            if (param.vec_count) { // HFA
                uint8_t *ptr = (uint8_t *)vec_ptr;

                if (!PushObject(obj, param.type, ptr))
                    return false;
                ExpandFloats(ptr, param.vec_count, param.vec_bytes);

                vec_ptr += param.vec_count;
            } else if (!param.use_memory) {
                if (param.gpr_count) {
                    K_ASSERT(param.type->align <= 8);

                    if (!PushObject(obj, param.type, (uint8_t *)gpr_ptr))
                        return false;
                    gpr_ptr += param.gpr_count;
                } else if (param.type->size) {
#if defined(__APPLE__)
                    args_ptr = AlignUp(args_ptr, param.type->align);
#endif
                    if (!PushObject(obj, param.type, (uint8_t *)args_ptr))
                        return false;
                    args_ptr += (param.type->size + 7) / 8;
                }
            } else {
                uint8_t *ptr = AllocHeap(param.type->size, 16);

                if (param.gpr_count) {
                    K_ASSERT(param.gpr_count == 1);
                    K_ASSERT(param.vec_count == 0);

                    *(uint8_t **)(gpr_ptr++) = ptr;
                } else {
#if defined(__APPLE__)
                    args_ptr = AlignUp(args_ptr, 8);
#endif
                    *(uint8_t **)(args_ptr++) = ptr;
                }

                if (!PushObject(obj, param.type, ptr))
                    return false;
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

            if (param.vec_count) [[likely]] {
                memset((uint8_t *)vec_ptr + 4, 0, 4);
                *(float *)(vec_ptr++) = f;
#if defined(_WIN32)
            } else if (param.gpr_count) {
                memset((uint8_t *)gpr_ptr + 4, 0, 4);
                *(float *)(gpr_ptr++) = f;
#endif
            } else {
#if defined(__APPLE__)
                args_ptr = AlignUp(args_ptr, 4);
                *(float *)args_ptr = f;
                args_ptr = (uint64_t *)((uint8_t *)args_ptr + 4);
#else
                memset((uint8_t *)args_ptr + 4, 0, 4);
                *(float *)(args_ptr++) = f;
#endif
            }
        };
        CASE(Float64) {
            const ParameterInfo &param = func->parameters[i];
            Napi::Value value = info[param.offset];

            double d;
            if (!TryNumber(value, &d)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                return false;
            }

            if (param.vec_count) [[likely]] {
                *(double *)(vec_ptr++) = d;
#if defined(_WIN32)
            } else if (param.gpr_count) {
                *(double *)(gpr_ptr++) = d;
#endif
            } else {
#if defined(__APPLE__)
                args_ptr = AlignUp(args_ptr, 8);
#endif
                *(double *)(args_ptr++) = d;
            }
        };

        CASE(Callback) {
            const ParameterInfo &param = func->parameters[i];
            Napi::Value value = info[param.offset];

            void *ptr;
            if (!PushCallback(value, param.type, &ptr)) [[unlikely]]
                return false;

#if defined(__APPLE__)
            args_ptr = param.gpr_count ? args_ptr : AlignUp(args_ptr, 8);
#endif
            *(void **)((param.gpr_count ? gpr_ptr : args_ptr)++) = ptr;
        };

        CASE(Prototype) { K_UNREACHABLE(); };

        CASE(End) { /* End loop */ };
    }

#undef PUSH_INTEGER_SWAP
#undef PUSH_INTEGER

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
               guaranteed = teb->GuaranteedStackBytes) {
        teb->ExceptionList = exception_list;
        teb->StackBase = base;
        teb->StackLimit = limit;
        teb->DeallocationStack = dealloc;
        teb->GuaranteedStackBytes = guaranteed;

        instance->last_error = teb->LastErrorValue;
    };

    // Adjust stack limits so SEH works correctly
    teb->ExceptionList = (void *)-1; // EXCEPTION_CHAIN_END
    teb->StackBase = mem->stack0.end();
    teb->StackLimit = mem->stack0.ptr;
    teb->DeallocationStack = mem->stack0.ptr;
    teb->GuaranteedStackBytes = 0;

    teb->LastErrorValue = instance->last_error;
#endif

#define PERFORM_CALL(Suffix) \
        ([&]() { \
            auto ret = (func->forward_fp ? ForwardCallX ## Suffix(native, new_sp, &old_sp) \
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
        case PrimitiveKind::Callback: { result.u64 = PERFORM_CALL(GG).x0; } break;
        case PrimitiveKind::Record:
        case PrimitiveKind::Union: {
            if (func->ret.gpr_count) {
                X0X1Ret ret = PERFORM_CALL(GG);
                memcpy(&result.buf, &ret, K_SIZE(ret));
            } else if (func->ret.vec_count) {
                HfaRet ret = PERFORM_CALL(DDDD);
                memcpy(&result.buf, &ret, K_SIZE(ret));
            } else {
                PERFORM_CALL(GG);
            }
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: { result.f = PERFORM_CALL(F); } break;
        case PrimitiveKind::Float64: { result.d = PERFORM_CALL(DDDD).d0; } break;

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
            if (func->ret.vec_count) { // HFA
                uint8_t *ptr = (uint8_t *)&result.buf;

                CompactFloats(ptr, func->ret.vec_count, func->ret.vec_bytes);

                Napi::Object obj = DecodeObject(env, ptr, func->ret.type);
                return obj;
            } else {
                const uint8_t *ptr = return_ptr ? (const uint8_t *)return_ptr
                                                : (const uint8_t *)&result.buf;

                Napi::Object obj = DecodeObject(env, ptr, func->ret.type);
                return obj;
            }
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

    uint64_t *gpr_ptr = (uint64_t *)own_sp;
    uint64_t *vec_ptr = gpr_ptr + 9;
    uint64_t *args_ptr = (uint64_t *)caller_sp;

    uint8_t *return_ptr = proto->ret.use_memory ? (uint8_t *)gpr_ptr[8] : nullptr;

    K_DEFER_N(err_guard) { memset(out_reg, 0, K_SIZE(*out_reg)); };

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
#if defined(__APPLE__)
                bool b;
                if (param.gpr_count) {
                    b = *(bool *)(gpr_ptr++);
                } else {
                    b = *(bool *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 1);
                }
#else
                bool b = *(bool *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = Napi::Boolean::New(env, b);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int8: {
#if defined(__APPLE__)
                int8_t i;
                if (param.gpr_count) {
                    i = *(int8_t *)(gpr_ptr++);
                } else {
                    i = *(int8_t *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 1);
                }
#else
                int8_t i = *(int8_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = NewInt(env, i);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt8: {
#if defined(__APPLE__)
                uint8_t i;
                if (param.gpr_count) {
                    i = *(uint8_t *)(gpr_ptr++);
                } else {
                    i = *(uint8_t *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 1);
                }
#else
                uint8_t i = *(uint8_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = NewInt(env, i);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int16: {
#if defined(__APPLE__)
                int16_t i;
                if (param.gpr_count) {
                    i = *(int16_t *)(gpr_ptr++);
                } else {
                    args_ptr = AlignUp(args_ptr, 2);
                    i = *(int16_t *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 2);
                }
#else
                int16_t i = *(int16_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = NewInt(env, i);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int16S: {
#if defined(__APPLE__)
                int16_t i;
                if (param.gpr_count) {
                    i = *(int16_t *)(gpr_ptr++);
                } else {
                    args_ptr = AlignUp(args_ptr, 2);
                    i = *(int16_t *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 2);
                }
#else
                int16_t i = *(int16_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = NewInt(env, ReverseBytes(i));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt16: {
#if defined(__APPLE__)
                uint16_t i;
                if (param.gpr_count) {
                    i = *(uint16_t *)(gpr_ptr++);
                } else {
                    args_ptr = AlignUp(args_ptr, 2);
                    i = *(uint16_t *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 2);
                }
#else
                uint16_t i = *(uint16_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = NewInt(env, i);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt16S: {
#if defined(__APPLE__)
                uint16_t i;
                if (param.gpr_count) {
                    i = *(uint16_t *)(gpr_ptr++);
                } else {
                    args_ptr = AlignUp(args_ptr, 2);
                    i = *(uint16_t *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 2);
                }
#else
                uint16_t i = *(uint16_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = NewInt(env, ReverseBytes(i));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int32: {
#if defined(__APPLE__)
                int32_t i;
                if (param.gpr_count) {
                    i = *(int32_t *)(gpr_ptr++);
                } else {
                    args_ptr = AlignUp(args_ptr, 4);
                    i = *(int32_t *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 4);
                }
#else
                int32_t i = *(int32_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = NewInt(env, i);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int32S: {
#if defined(__APPLE__)
                int32_t i;
                if (param.gpr_count) {
                    i = *(int32_t *)(gpr_ptr++);
                } else {
                    args_ptr = AlignUp(args_ptr, 4);
                    i = *(int32_t *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 4);
                }
#else
                int32_t i = *(int32_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = NewInt(env, ReverseBytes(i));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt32: {
#if defined(__APPLE__)
                uint32_t i;
                if (param.gpr_count) {
                    i = *(uint32_t *)(gpr_ptr++);
                } else {
                    args_ptr = AlignUp(args_ptr, 4);
                    i = *(uint32_t *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 4);
                }
#else
                uint32_t i = *(uint32_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = NewInt(env, i);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt32S: {
#if defined(__APPLE__)
                uint32_t i;
                if (param.gpr_count) {
                    i = *(uint32_t *)(gpr_ptr++);
                } else {
                    args_ptr = AlignUp(args_ptr, 4);
                    i = *(uint32_t *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 4);
                }
#else
                uint32_t i = *(uint32_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = NewInt(env, ReverseBytes(i));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int64: {
#if defined(__APPLE__)
                args_ptr = AlignUp(args_ptr, 8);
#endif

                int64_t i = *(int64_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = NewInt(env, i);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int64S: {
#if defined(__APPLE__)
                args_ptr = AlignUp(args_ptr, 8);
#endif

                int64_t i = *(int64_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = NewInt(env, ReverseBytes(i));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt64: {
#if defined(__APPLE__)
                args_ptr = AlignUp(args_ptr, 8);
#endif

                uint64_t i = *(uint64_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = NewInt(env, i);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt64S: {
#if defined(__APPLE__)
                args_ptr = AlignUp(args_ptr, 8);
#endif

                uint64_t i = *(uint64_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = NewInt(env, ReverseBytes(i));
                arguments.Append(arg);
            } break;
            case PrimitiveKind::String: {
#if defined(__APPLE__)
                args_ptr = AlignUp(args_ptr, 8);
#endif

                const char *str = *(const char **)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = str ? Napi::String::New(env, str) : env.Null();
                arguments.Append(arg);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, str);
                }
            } break;
            case PrimitiveKind::String16: {
#if defined(__APPLE__)
                args_ptr = AlignUp(args_ptr, 8);
#endif

                const char16_t *str16 = *(const char16_t **)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = str16 ? Napi::String::New(env, str16) : env.Null();
                arguments.Append(arg);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, str16);
                }
            } break;
            case PrimitiveKind::String32: {
#if defined(__APPLE__)
                args_ptr = AlignUp(args_ptr, 8);
#endif

                const char32_t *str32 = *(const char32_t **)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = str32 ? MakeStringFromUTF32(env, str32) : env.Null();
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Pointer: {
#if defined(__APPLE__)
                args_ptr = AlignUp(args_ptr, 8);
#endif

                void *ptr2 = *(void **)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value p = ptr2 ? WrapPointer(env, param.type->ref.type, ptr2) : env.Null();
                arguments.Append(p);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, ptr2);
                }
            } break;
            case PrimitiveKind::Callback: {
#if defined(__APPLE__)
                args_ptr = AlignUp(args_ptr, 8);
#endif

                void *ptr2 = *(void **)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value p = ptr2 ? WrapCallback(env, param.type->ref.type, ptr2) : env.Null();
                arguments.Append(p);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, ptr2);
                }
            } break;
            case PrimitiveKind::Record:
            case PrimitiveKind::Union: {
                if (param.vec_count) { // HFA
                    uint8_t *ptr = (uint8_t *)vec_ptr;

                    CompactFloats(ptr, param.vec_count, param.vec_bytes);

                    Napi::Object obj = DecodeObject(env, ptr, param.type);
                    arguments.Append(obj);

                    vec_ptr += param.vec_count;
                } else if (!param.use_memory) {
                    if (param.gpr_count) {
                        K_ASSERT(param.type->align <= 8);

                        Napi::Object obj = DecodeObject(env, (uint8_t *)gpr_ptr, param.type);
                        arguments.Append(obj);

                        gpr_ptr += param.gpr_count;
                    } else if (param.type->size) {
                        args_ptr = AlignUp(args_ptr, param.type->align);

                        Napi::Object obj = DecodeObject(env, (uint8_t *)args_ptr, param.type);
                        arguments.Append(obj);

                        args_ptr += (param.type->size + 7) / 8;
                    }
                } else {
#if defined(__APPLE__)
                    args_ptr = AlignUp(args_ptr, 8);
#endif

                    void *ptr2 = *(void **)((param.gpr_count ? gpr_ptr : args_ptr)++);

                    Napi::Object obj = DecodeObject(env, (uint8_t *)ptr2, param.type);
                    arguments.Append(obj);
                }
            } break;
            case PrimitiveKind::Array: { K_UNREACHABLE(); } break;
            case PrimitiveKind::Float32: {
                float f;
                if (param.vec_count) [[likely]] {
                    f = *(float *)(vec_ptr++);
#if defined(_WIN32)
                } else if (param.gpr_count) {
                    f = *(float *)(gpr_ptr++);
#endif
                } else {
#if defined(__APPLE__)
                    args_ptr = AlignUp(args_ptr, 4);
                    f = *(float *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 4);
#else
                    f = *(float *)(args_ptr++);
#endif
                }

                Napi::Value arg = Napi::Number::New(env, (double)f);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Float64: {
                double d;
                if (param.vec_count) [[likely]] {
                    d = *(double *)(vec_ptr++);
#if defined(_WIN32)
                } else if (param.gpr_count) {
                    d = *(double *)(gpr_ptr++);
#endif
                } else {
#if defined(__APPLE__)
                    args_ptr = AlignUp(args_ptr, 8);
#endif

                    d = *(double *)(args_ptr++);
                }

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
            out_reg->x0 = (uint64_t)v; \
        } while (false)
#define RETURN_INTEGER_SWAP(CType) \
        do { \
            CType v; \
            if (!TryNumber(value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value)); \
                return; \
            } \
             \
            out_reg->x0 = (uint64_t)ReverseBytes(v); \
        } while (false)

    // Convert the result
    switch (type->primitive) {
        case PrimitiveKind::Void: {} break;
        case PrimitiveKind::Bool: {
            bool b;
            if (napi_get_value_bool(env, value, &b) != napi_ok) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected boolean", GetValueType(instance, value));
                return;
            }

            out_reg->x0 = (uint64_t)b;
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

            out_reg->x0 = (uint64_t)str;
        } break;
        case PrimitiveKind::String16: {
            const char16_t *str16;
            if (!PushString16(value, 1, &str16)) [[unlikely]]
                return;

            out_reg->x0 = (uint64_t)str16;
        } break;
        case PrimitiveKind::String32: {
            const char32_t *str32;
            if (!PushString32(value, 1, &str32)) [[unlikely]]
                return;

            out_reg->x0 = (uint64_t)str32;
        } break;
        case PrimitiveKind::Pointer: {
            void *ptr;
            if (!PushPointer(value, type, 1, &ptr)) [[unlikely]]
                return;

            out_reg->x0 = (uint64_t)ptr;
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
                out_reg->x0 = (uint64_t)return_ptr;
            } else if (proto->ret.vec_count) { // HFA
                uint8_t *ptr = (uint8_t *)&out_reg->d0;

                ExpandFloats(ptr, proto->ret.vec_count, proto->ret.vec_bytes);
                PushObject(obj, type, ptr);
            } else {
                PushObject(obj, type, (uint8_t *)&out_reg->x0);
            }
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: {
            float f;
            if (!TryNumber(value, &f)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                return;
            }

            memset((uint8_t *)&out_reg->d0 + 4, 0, 4);
            memcpy(&out_reg->d0, &f, 4);
        } break;
        case PrimitiveKind::Float64: {
            double d;
            if (!TryNumber(value, &d)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                return;
            }

            out_reg->d0 = d;
        } break;
        case PrimitiveKind::Callback: {
            void *ptr;
            if (!PushCallback(value, type, &ptr)) [[unlikely]]
                return;

            out_reg->x0 = (uint64_t)ptr;
        } break;

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

#undef RETURN_INTEGER_SWAP
#undef RETURN_INTEGER

    err_guard.Disable();
}

}

#endif
