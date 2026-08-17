// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#if defined(__i386__) || defined(_M_IX86)

#include "lib/native/base/base.hh"
#include "../ffi.hh"
#include "../call.hh"
#include "../interp.hh"
#include "../type.hh"
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

bool AnalyseFunction(Napi::Env env, InstanceData *instance, FunctionInfo *func)
{
    if (!func->lib && func->convention != CallConvention::Cdecl &&
                      func->convention != CallConvention::Stdcall) {
        ThrowError<Napi::Error>(env, "Only Cdecl and Stdcall callbacks are supported");
        return false;
    }

    if (func->ret.type->primitive != PrimitiveKind::Record &&
            func->ret.type->primitive != PrimitiveKind::Union) {
        K_ASSERT(IsRegularSize(func->ret.type->size, 8));
        func->ret.abi.regular = true;
#if defined(_WIN32) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    } else {
        func->ret.abi.regular = IsRegularSize(func->ret.type->size, 8);
#endif
    }

    int fast_regs = (func->convention == CallConvention::Fastcall) ? 2 :
                    (func->convention == CallConvention::Thiscall) ? 1 : 0;
    bool fast = fast_regs;

    Size fast_offset = 0;
    Size stk_offset = fast ? 4 : 0;

    if (!func->ret.abi.regular) {
#if defined(_WIN32)
        stk_offset++;
#else
        if (fast_regs) {
            fast_offset++;
            fast_regs--;
        } else {
            stk_offset++;
        }
#endif
    }

    for (ParameterInfo &param: func->parameters) {
        int16_t offset = 0;

        if (fast_regs && param.type->size <= 4) {
            offset = (int16_t)fast_offset++;
            fast_regs--;
        } else {
            offset = (int16_t)stk_offset;
            stk_offset += (param.type->size + 3) / 4;
        }

        if (param.type->primitive == PrimitiveKind::Record || param.type->primitive == PrimitiveKind::Union) {
            func->sync.Append({ .op = Code2Op(Opcode::PushAggregateReg), .a = param.offset, .b1 = (int16_t)(offset * 4), .type = param.type });
        } else {
            int delta = (int)Opcode::PushVoid - (int)PrimitiveKind::Void;
            Opcode code = (Opcode)((int)param.type->primitive + delta);

            func->sync.Append({ .op = Code2Op(code), .a = param.offset, .b1 = (int16_t)(offset * 4), .b2 = (int16_t)param.directions, .type = param.type });
        }
    }

    // We need enough space to memcpy result in CallX instructions
    func->ret_pop = (int)(4 * stk_offset);
    func->stk_size = stk_offset ? AlignLen(4 * stk_offset, 16) : 16;

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
        case PrimitiveKind::Callback: {
            if (fast) {
                int delta = (int)Opcode::RunVoidX - (int)PrimitiveKind::Void;
                Opcode run = (Opcode)((int)func->ret.type->primitive + delta);

                func->sync.Append({ .op = Code2Op(run), .b1 = (int16_t)func->parameters.len, .type = func->ret.type });
            } else {
                int delta = (int)Opcode::RunVoid - (int)PrimitiveKind::Void;
                Opcode run = (Opcode)((int)func->ret.type->primitive + delta);

                func->sync.Append({ .op = Code2Op(run), .b1 = (int16_t)func->parameters.len, .type = func->ret.type });
            }
        } break;

        case PrimitiveKind::Record:
        case PrimitiveKind::Union: {
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
            if (func->ret.type->members.len == 1) {
                const RecordMember &member = func->ret.type->members[0];

                if (member.type->primitive == PrimitiveKind::Float32) {
                    Opcode run = fast ? Opcode::RunAggregateFX : Opcode::RunAggregateF;
                    func->sync.Append({ .op = Code2Op(run), .b1 = (int16_t)func->parameters.len, .b2 = 8, .type = func->ret.type });

                    break;
                } else if (member.type->primitive == PrimitiveKind::Float64) {
                    Opcode run = fast ? Opcode::RunAggregateDX : Opcode::RunAggregateD;
                    func->sync.Append({ .op = Code2Op(run), .b1 = (int16_t)func->parameters.len, .b2 = 8, .type = func->ret.type });

                    break;
                }
            }
#endif

            if (func->ret.abi.regular) {
                Opcode run = fast ? Opcode::RunAggregateGX : Opcode::RunAggregateG;
                func->sync.Append({ .op = Code2Op(run), .b1 = (int16_t)func->parameters.len, .type = func->ret.type });
            } else {
                Opcode run = fast ? Opcode::RunAggregateMemX : Opcode::RunAggregateMem;

#if defined(_WIN32)
                int16_t offset = fast ? 16 : 0;
#else
                int16_t offset = 0;
#endif

                func->sync.Append({ .op = Code2Op(run), .a = (int32_t)func->ret.type->size, .b1 = (int16_t)func->parameters.len, .b2 = offset, .type = func->ret.type });
            }
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;

        case PrimitiveKind::Float32: {
            Opcode run = fast ? Opcode::RunFloat32X : Opcode::RunFloat32;
            func->sync.Append({ .op = Code2Op(run), .b1 = (int16_t)func->parameters.len, .b2 = 8, .type = func->ret.type });
        } break;
        case PrimitiveKind::Float64: {
            Opcode run = fast ? Opcode::RunFloat64X : Opcode::RunFloat64;
            func->sync.Append({ .op = Code2Op(run), .b1 = (int16_t)func->parameters.len, .b2 = 8, .type = func->ret.type });
        } break;

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

    switch (func->convention) {
        case CallConvention::Cdecl: {
            func->decorated_name = Fmt(&instance->str_alloc, "_%1", func->name).ptr;
        } break;
        case CallConvention::Stdcall: {
            K_ASSERT(!func->variadic);

            Size suffix = (stk_offset - !func->ret.abi.regular) * 4;
            func->decorated_name = Fmt(&instance->str_alloc, "_%1@%2", func->name, suffix).ptr;
        } break;
        case CallConvention::Fastcall: {
            K_ASSERT(!func->variadic);

            Size suffix = (fast_offset + stk_offset - 4 - !func->ret.abi.regular) * 4;
            func->decorated_name = Fmt(&instance->str_alloc, "@%1@%2", func->name, suffix).ptr;
        } break;
        case CallConvention::Thiscall: {
            K_ASSERT(!func->variadic);
            // Name does not change
        } break;
    }

    return true;
}

void CallData::Relay(Size idx, uint8_t *sp)
{
    TrampolineInfo *trampoline = &shared.trampolines[idx];
    const FunctionInfo *proto = trampoline->proto;

    uint8_t *caller_sp = sp + 48;
    BackRegisters *out_reg = (BackRegisters *)(sp + 16);

    uint32_t *args_ptr = (uint32_t *)caller_sp;

    uint8_t *return_ptr = !proto->ret.abi.regular ? (uint8_t *)args_ptr[0] : nullptr;
    args_ptr += !proto->ret.abi.regular;

    if (proto->convention == CallConvention::Stdcall) {
        out_reg->ret_pop = (int)proto->ret_pop;
    } else {
#if defined(_WIN32)
        out_reg->ret_pop = 0;
#else
        out_reg->ret_pop = return_ptr ? 4 : 0;
#endif
    }

    K_DEFER_N(err_guard) {
        trampoline->state = -1;

        int pop = out_reg->ret_pop;
        memset(out_reg, 0, K_SIZE(*out_reg));
        out_reg->ret_type = 0;
        out_reg->ret_pop = pop;
    };

    napi_value arguments[MaxParameters];

    // Convert to JS arguments
    for (Size i = 0; i < proto->parameters.len; i++) {
        const ParameterInfo &param = proto->parameters[i];
        K_ASSERT(param.directions >= 1 && param.directions <= 3);

        switch (param.type->primitive) {
            case PrimitiveKind::Void: { K_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                bool b = *(bool *)(args_ptr++);
                arguments[i] = Napi::Boolean::New(env, b);
            } break;
            case PrimitiveKind::Int8: {
                int8_t v = *(int8_t *)(args_ptr++);
                arguments[i] = NewInt(env, v);
            } break;
            case PrimitiveKind::UInt8: {
                uint8_t v = *(uint8_t *)(args_ptr++);
                arguments[i] = NewInt(env, v);
            } break;
            case PrimitiveKind::Int16: {
                int16_t v = *(int16_t *)(args_ptr++);
                arguments[i] = NewInt(env, v);
            } break;
            case PrimitiveKind::Int16S: {
                int16_t v = *(int16_t *)(args_ptr++);
                arguments[i] = NewInt(env, ReverseBytes(v));
            } break;
            case PrimitiveKind::UInt16: {
                uint16_t v = *(uint16_t *)(args_ptr++);
                arguments[i] = NewInt(env, v);
            } break;
            case PrimitiveKind::UInt16S: {
                uint16_t v = *(uint16_t *)(args_ptr++);
                arguments[i] = NewInt(env, ReverseBytes(v));
            } break;
            case PrimitiveKind::Int32: {
                int32_t v = *(int32_t *)(args_ptr++);
                arguments[i] = NewInt(env, v);
            } break;
            case PrimitiveKind::Int32S: {
                int32_t v = *(int32_t *)(args_ptr++);
                arguments[i] = NewInt(env, ReverseBytes(v));
            } break;
            case PrimitiveKind::UInt32: {
                uint32_t v = *(uint32_t *)(args_ptr++);
                arguments[i] = NewInt(env, v);
            } break;
            case PrimitiveKind::UInt32S: {
                uint32_t v = *(uint32_t *)(args_ptr++);
                arguments[i] = NewInt(env, ReverseBytes(v));
            } break;
            case PrimitiveKind::Int64: {
                int64_t v = *(int64_t *)args_ptr;
                args_ptr += 2;

                arguments[i] = NewInt(env, v);
            } break;
            case PrimitiveKind::Int64S: {
                int64_t v = *(int64_t *)args_ptr;
                args_ptr += 2;

                arguments[i] = NewInt(env, ReverseBytes(v));
            } break;
            case PrimitiveKind::UInt64: {
                uint64_t v = *(uint64_t *)args_ptr;
                args_ptr += 2;

                arguments[i] = NewInt(env, v);
            } break;
            case PrimitiveKind::UInt64S: {
                uint64_t v = *(uint64_t *)args_ptr;
                args_ptr += 2;

                arguments[i] = NewInt(env, ReverseBytes(v));
            } break;
            case PrimitiveKind::String: {
                const char *str = *(const char **)(args_ptr++);
                arguments[i] = NewString(env, str);

                if (param.type->dispose) {
                    param.type->dispose(instance, param.type, str);
                }
            } break;
            case PrimitiveKind::String16: {
                const char16_t *str16 = *(const char16_t **)(args_ptr++);
                arguments[i] = NewString(env, str16);

                if (param.type->dispose) {
                    param.type->dispose(instance, param.type, str16);
                }
            } break;
            case PrimitiveKind::String32: {
                const char32_t *str32 = *(const char32_t **)(args_ptr++);
                arguments[i] = NewString(env, str32);

                if (param.type->dispose) {
                    param.type->dispose(instance, param.type, str32);
                }
            } break;
            case PrimitiveKind::Pointer: {
                void *ptr2 = *(void **)(args_ptr++);
                arguments[i] = WrapPointer(env, ptr2);

                if (param.type->dispose) {
                    param.type->dispose(instance, param.type, ptr2);
                }
            } break;
            case PrimitiveKind::Callback: {
                void *ptr2 = *(void **)(args_ptr++);
                arguments[i] = WrapPointer(env, ptr2);
            } break;
            case PrimitiveKind::Record:
            case PrimitiveKind::Union: {
                uint8_t *ptr = (uint8_t *)args_ptr;
                arguments[i] = DecodeObject(instance, ptr, param.type);

                args_ptr = (uint32_t *)AlignUp(ptr + param.type->size, 4);
            } break;
            case PrimitiveKind::Array: { K_UNREACHABLE(); } break;
            case PrimitiveKind::Float32: {
                float f = *(float *)(args_ptr++);
                arguments[i] = NewFloat(env, f);
            } break;
            case PrimitiveKind::Float64: {
                double d = *(double *)args_ptr;
                args_ptr += 2;

                arguments[i] = NewFloat(env, d);
            } break;

            case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
        }
    }

    const TypeInfo *type = proto->ret.type;

    // We're ready, make the call!
    napi_value value = CallCallback(trampoline, arguments, proto->parameters.len);

    if (!value) [[unlikely]]
        return;

#define RETURN_INTEGER_32(CType) \
        do { \
            CType v; \
            if (!TryNumber(env, value, &v)) [[unlikely]] { \
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
            if (!TryNumber(env, value, &v)) [[unlikely]] { \
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
            if (!TryNumber(env, value, &v)) [[unlikely]] { \
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
            if (!TryNumber(env, value, &v)) [[unlikely]] { \
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
            if (return_ptr) {
                if (!PushObject(value, type, return_ptr)) [[unlikely]]
                    return;
                out_reg->eax = (uint32_t)return_ptr;
            } else {
                if (!PushObject(value, type, (uint8_t *)&out_reg->eax))
                    return;
            }

            out_reg->ret_type = 0;
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: {
            float f;
            if (!TryNumber(env, value, &f)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                return;
            }

            out_reg->x87.f = f;
            out_reg->ret_type = 1;
        } break;
        case PrimitiveKind::Float64: {
            double d;
            if (!TryNumber(env, value, &d)) [[unlikely]] {
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
