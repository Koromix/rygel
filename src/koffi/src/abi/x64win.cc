// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#if defined(_WIN32) && (defined(__x86_64__) || defined(_M_AMD64))

#include "lib/native/base/base.hh"
#include "../ffi.hh"
#include "../call.hh"
#include "../interp.hh"
#include "../type.hh"
#include "../util.hh"
#include "../win32.hh"

#include <napi.h>

namespace K {

struct BackRegisters {
    uint64_t rax;
    double xmm0;
};

bool AnalyseFunction(Napi::Env, InstanceData *, FunctionInfo *func)
{
    func->ret.abi.regular = IsRegularSize(func->ret.type->size, 8);

    for (Size i = 0; i < func->parameters.len; i++) {
        int16_t arg = (int16_t)(!func->ret.abi.regular + i);
        ParameterInfo &param = func->parameters[i];

        param.abi.regular = IsRegularSize(param.type->size, 8);

        if (param.type->primitive == PrimitiveKind::Record || param.type->primitive == PrimitiveKind::Union) {
            Opcode code = param.abi.regular ? Opcode::PushAggregateReg : Opcode::PushAggregateMem;
            func->sync.Append({ .op = Code2Op(code), .a = param.offset, .b1 = (int16_t)(arg * 8), .type = param.type });
        } else {
            int delta = (int)Opcode::PushVoid - (int)PrimitiveKind::Void;
            Opcode code = (Opcode)((int)param.type->primitive + delta);

            func->sync.Append({ .op = Code2Op(code), .a = param.offset, .b1 = (int16_t)(arg * 8), .b2 = (int16_t)param.directions, .type = param.type });
        }

        func->forward_fp |= IsFloat(param.type);
    }

    // At least 4 parameter registers
    {
        Size count = std::max((Size)4, func->parameters.len + !func->ret.abi.regular);
        func->stk_size = AlignLen(8 * count, 16);
    }

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
            if (func->forward_fp) {
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
            if (func->ret.abi.regular) {
                Opcode run = func->forward_fp ? Opcode::RunAggregateGX : Opcode::RunAggregateG;
                func->sync.Append({ .op = Code2Op(run), .b1 = (int16_t)func->parameters.len, .type = func->ret.type });
            } else {
                Opcode run = func->forward_fp ? Opcode::RunAggregateMemX : Opcode::RunAggregateMem;
                func->sync.Append({ .op = Code2Op(run), .a = (int32_t)func->ret.type->size, .b1 = (int16_t)func->parameters.len, .type = func->ret.type });
            }
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;

        case PrimitiveKind::Float32: {
            Opcode run = func->forward_fp ? Opcode::RunFloat32X : Opcode::RunFloat32;
            func->sync.Append({ .op = Code2Op(run), .b1 = (int16_t)func->parameters.len, .b2 = 8, .type = func->ret.type });
        } break;
        case PrimitiveKind::Float64: {
            Opcode run = func->forward_fp ? Opcode::RunFloat64X : Opcode::RunFloat64;
            func->sync.Append({ .op = Code2Op(run), .b1 = (int16_t)func->parameters.len, .b2 = 8, .type = func->ret.type });
        } break;

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

    return true;
}

void CallData::Relay(Size idx, uint8_t *sp)
{
    TrampolineInfo *trampoline = &shared.trampolines[idx];
    const FunctionInfo *proto = trampoline->proto;

    uint8_t *own_sp = sp;
    uint8_t *caller_sp = sp + 128;
    BackRegisters *out_reg = (BackRegisters *)(sp + 64);

    uint64_t *gpr_ptr = (uint64_t *)own_sp;
    uint64_t *xmm_ptr = gpr_ptr + 4;
    uint64_t *stk_ptr = (uint64_t *)caller_sp;

    uint8_t *return_ptr = !proto->ret.abi.regular ? (uint8_t *)gpr_ptr[0] : nullptr;

    K_DEFER_N(err_guard) {
        trampoline->state = -1;
        memset(out_reg, 0, K_SIZE(*out_reg));
    };

    napi_value arguments[MaxParameters];

    // Convert to JS arguments
    for (Size i = 0, j = !!return_ptr; i < proto->parameters.len; i++, j++) {
        const ParameterInfo &param = proto->parameters[i];
        K_ASSERT(param.directions >= 1 && param.directions <= 3);

        switch (param.type->primitive) {
            case PrimitiveKind::Void: { K_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                bool b = *(bool *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = Napi::Boolean::New(env, b);
            } break;
            case PrimitiveKind::Int8: {
                int8_t v = *(int8_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewInt(env, v);
            } break;
            case PrimitiveKind::UInt8: {
                uint8_t v = *(uint8_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewInt(env, v);
            } break;
            case PrimitiveKind::Int16: {
                int16_t v = *(int16_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewInt(env, v);
            } break;
            case PrimitiveKind::Int16S: {
                int16_t v = *(int16_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewInt(env, ReverseBytes(v));
            } break;
            case PrimitiveKind::UInt16: {
                uint16_t v = *(uint16_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewInt(env, v);
            } break;
            case PrimitiveKind::UInt16S: {
                uint16_t v = *(uint16_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewInt(env, ReverseBytes(v));
            } break;
            case PrimitiveKind::Int32: {
                int32_t v = *(int32_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewInt(env, v);
            } break;
            case PrimitiveKind::Int32S: {
                int32_t v = *(int32_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewInt(env, ReverseBytes(v));
            } break;
            case PrimitiveKind::UInt32: {
                uint32_t v = *(uint32_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewInt(env, v);
            } break;
            case PrimitiveKind::UInt32S: {
                uint32_t v = *(uint32_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewInt(env, ReverseBytes(v));
            } break;
            case PrimitiveKind::Int64: {
                int64_t v = *(int64_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewInt(env, v);
            } break;
            case PrimitiveKind::Int64S: {
                int64_t v = *(int64_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewInt(env, ReverseBytes(v));
            } break;
            case PrimitiveKind::UInt64: {
                uint64_t v = *(uint64_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewInt(env, v);
            } break;
            case PrimitiveKind::UInt64S: {
                uint64_t v = *(uint64_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewInt(env, ReverseBytes(v));
            } break;
            case PrimitiveKind::String: {
                const char *str = *(const char **)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewString(env, str);

                if (param.type->dispose) {
                    param.type->dispose(instance, param.type, str);
                }
            } break;
            case PrimitiveKind::String16: {
                const char16_t *str16 = *(const char16_t **)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewString(env, str16);

                if (param.type->dispose) {
                    param.type->dispose(instance, param.type, str16);
                }
            } break;
            case PrimitiveKind::String32: {
                const char32_t *str32 = *(const char32_t **)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewString(env, str32);

                if (param.type->dispose) {
                    param.type->dispose(instance, param.type, str32);
                }
            } break;
            case PrimitiveKind::Pointer: {
                void *ptr2 = *(void **)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = WrapPointer(env, ptr2);

                if (param.type->dispose) {
                    param.type->dispose(instance, param.type, ptr2);
                }
            } break;
            case PrimitiveKind::Callback: {
                void *ptr2 = *(void **)(j < 4 ? gpr_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = WrapPointer(env, ptr2);
            } break;
            case PrimitiveKind::Record:
            case PrimitiveKind::Union: {
                uint8_t *ptr;
                if (param.abi.regular) {
                    ptr = (uint8_t *)(j < 4 ? gpr_ptr + j : stk_ptr);
                } else {
                    ptr = *(uint8_t **)(j < 4 ? gpr_ptr + j : stk_ptr);
                }
                stk_ptr += (j >= 4);

                arguments[i] = DecodeObject(instance, ptr, param.type);
            } break;
            case PrimitiveKind::Array: { K_UNREACHABLE(); } break;
            case PrimitiveKind::Float32: {
                float f = *(float *)(j < 4 ? xmm_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

                arguments[i] = NewFloat(env, f);
            } break;
            case PrimitiveKind::Float64: {
                double d = *(double *)(j < 4 ? xmm_ptr + j : stk_ptr);
                stk_ptr += (j >= 4);

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

#define RETURN_INTEGER(CType) \
        do { \
            CType v; \
            if (!TryNumber(env, value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value)); \
                return; \
            } \
             \
            out_reg->rax = (uint64_t)v; \
        } while (false)
#define RETURN_INTEGER_SWAP(CType) \
        do { \
            CType v; \
            if (!TryNumber(env, value, &v)) [[unlikely]] { \
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
            if (return_ptr) {
                if (!PushObject(value, type, return_ptr)) [[unlikely]]
                    return;
                out_reg->rax = (uint64_t)return_ptr;
            } else {
                if (!PushObject(value, type, (uint8_t *)&out_reg->rax)) [[unlikely]]
                    return;
            }
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: {
            float f;
            if (!TryNumber(env, value, &f)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                return;
            }

            memset(&out_reg->xmm0, 0, 8);
            memcpy(&out_reg->xmm0, &f, 4);
        } break;
        case PrimitiveKind::Float64: {
            double d;
            if (!TryNumber(env, value, &d)) [[unlikely]] {
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
