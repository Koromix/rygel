// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#if defined(__aarch64__) || defined(_M_ARM64)

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

struct HfaInfo {
    int count;
    bool float32;
};

struct BackRegisters {
    uint64_t x0;
    uint64_t x1;

    double d0;
    double d1;
    double d2;
    double d3;
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

bool AnalyseFunction(Napi::Env, InstanceData *instance, FunctionInfo *func)
{
    int gpr_index = 0;
    int vec_index = 0;
    int stack_offset = 0;

    int gpr_max = 8;
    int vec_max = 8;

    for (ParameterInfo &param: func->parameters) {
#if defined(__APPLE__)
        if (param.variadic) {
            gpr_index = gpr_max;
            vec_index = vec_max;
        }
#endif

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
                if (gpr_index < gpr_max) {
                    param.abi.regular = true;
                    param.abi.offset = gpr_index * 8;
                    gpr_index++;
                } else {
#if defined(__APPLE__)
                    stack_offset = AlignLen(stack_offset, param.variadic ? 8 : param.type->align);
                    param.abi.offset = 19 * 8 + stack_offset;
                    stack_offset += param.type->size;
#else
                    param.abi.offset = 19 * 8 + stack_offset;
                    stack_offset += 8;
#endif
                }

                int delta = (int)Opcode::PushVoid - (int)PrimitiveKind::Void;
                Opcode code = (Opcode)((int)param.type->primitive + delta);

                func->sync.Append({ .op = Code2Op(code), .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)param.directions, .type = param.type });
            } break;

            case PrimitiveKind::Record:
            case PrimitiveKind::Union: {
#if defined(__APPLE__)
                if (param.variadic) {
                    if (param.type->size <= 16) {
                        int registers = (param.type->size + 7) / 8;

                        if (registers <= gpr_max - gpr_index) {
                            K_ASSERT(param.type->align <= 8);

                            param.abi.regular = true;
                            param.abi.offset = gpr_index * 8;
                            gpr_index += registers;
                        } else {
                            gpr_index = gpr_max;

                            stack_offset = AlignLen(stack_offset, param.type->align);
                            param.abi.offset = 19 * 8 + stack_offset;
                            stack_offset += registers * 8;
                        }

                        func->sync.Append({ .op = Code2Op(Opcode::PushAggregateReg), .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                    } else {
                        if (gpr_index < gpr_max) {
                            param.abi.regular = true;
                            param.abi.offset = gpr_index * 8;
                            gpr_index++;
                        } else {
                            stack_offset = AlignLen(stack_offset, 8);
                            param.abi.offset = 19 * 8 + stack_offset;
                            stack_offset += 8;
                        }

                        param.abi.indirect = true;

                        func->sync.Append({ .op = Code2Op(Opcode::PushAggregateMem), .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                    }

                    break;
                }
#endif

                HfaInfo hfa = IsHFA(param.type);

#if defined(_WIN32)
                if (param.variadic) {
                    // Windows ignores HFA optimization for variadic parameters
                    hfa.count = 0;
                }
#endif

                if (hfa.count) {
                    if (hfa.count <= vec_max - vec_index) {
                        param.abi.regular = true;
                        param.abi.offset = 9 * 8 + vec_index * 8;
                        vec_index += hfa.count;

                        if (hfa.float32) {
                            param.type = ReshapeType(instance, param.type, 8, 0);
                        }

                        func->sync.Append({ .op = Code2Op(Opcode::PushAggregateReg), .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                    } else {
                        vec_index = vec_max;

#if defined(__APPLE__)
                        stack_offset = AlignLen(stack_offset, param.type->align);
#endif
                        param.abi.offset = 19 * 8 + stack_offset;
                        stack_offset += 8;

                        func->sync.Append({ .op = Code2Op(Opcode::PushAggregateReg), .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                    }
                } else if (param.type->size <= 16) {
                    int registers = (param.type->size + 7) / 8;

                    if (registers <= gpr_max - gpr_index) {
                        K_ASSERT(param.type->align <= 8);

                        param.abi.regular = true;
                        param.abi.offset = gpr_index * 8;
                        gpr_index += registers;
                    } else {
                        gpr_index = gpr_max;

#if defined(__APPLE__)
                        stack_offset = AlignLen(stack_offset, 8);
#endif
                        param.abi.offset = 19 * 8 + stack_offset;
                        stack_offset += registers * 8;
                    }

                    func->sync.Append({ .op = Code2Op(Opcode::PushAggregateReg), .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                } else {
                    // Big types (more than 16 bytes) are replaced by a pointer
                    if (gpr_index < gpr_max) {
                        param.abi.regular = true;
                        param.abi.offset = gpr_index * 8;
                        gpr_index++;
                    } else {
#if defined(__APPLE__)
                        stack_offset = AlignLen(stack_offset, 8);
#endif
                        param.abi.offset = 19 * 8 + stack_offset;
                        stack_offset += 8;
                    }

                    param.abi.indirect = true;

                    func->sync.Append({ .op = Code2Op(Opcode::PushAggregateMem), .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                }
            } break;
            case PrimitiveKind::Array: { K_UNREACHABLE(); } break;

            case PrimitiveKind::Float32: {
#if defined(_WIN32)
                if (param.variadic) {
                    if (gpr_index < gpr_max) {
                        param.abi.regular = true;
                        param.abi.offset = gpr_index * 8;
                        gpr_index++;
                    } else {
                        param.abi.offset = 19 * 8 + stack_offset;
                        stack_offset += 8;
                    }

                    func->sync.Append({ .op = Code2Op(Opcode::PushFloat32), .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)param.directions, .type = param.type });

                    break;
                }
#endif

                if (vec_index < vec_max) {
                    param.abi.regular = true;
                    param.abi.offset = 9 * 8 + vec_index * 8;
                    vec_index++;
                } else {
#if defined(__APPLE__)
                    stack_offset = AlignLen(stack_offset, param.variadic ? 8 : 4);
                    param.abi.offset = 19 * 8 + stack_offset;
                    stack_offset += 4;
#else
                    param.abi.offset = 19 * 8 + stack_offset;
                    stack_offset += 8;
#endif
                }

                func->sync.Append({ .op = Code2Op(Opcode::PushFloat32), .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)param.directions, .type = param.type });
            } break;
            case PrimitiveKind::Float64: {
#if defined(_WIN32)
                if (param.variadic) {
                    if (gpr_index < gpr_max) {
                        param.abi.regular = true;
                        param.abi.offset = gpr_index * 8;
                        gpr_index++;
                    } else {
                        param.abi.offset = 19 * 8 + stack_offset;
                        stack_offset += 8;
                    }

                    func->sync.Append({ .op = Code2Op(Opcode::PushFloat64), .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)param.directions, .type = param.type });

                    break;
                }
#endif

                if (vec_index < vec_max) {
                    param.abi.regular = true;
                    param.abi.offset = 9 * 8 + vec_index * 8;
                    vec_index++;
                } else {
#if defined(__APPLE__)
                    stack_offset = AlignLen(stack_offset, 8);
#endif
                    param.abi.offset = 19 * 8 + stack_offset;
                    stack_offset += 8;
                }

                func->sync.Append({ .op = Code2Op(Opcode::PushFloat64), .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)param.directions, .type = param.type });
            } break;

            case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
        }
    }

    func->stk_size = AlignLen(19 * 8 + stack_offset, 16) + 8;
    func->forward_fp = vec_index;

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

                func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
            } else {
                int delta = (int)Opcode::RunVoid - (int)PrimitiveKind::Void;
                Opcode run = (Opcode)((int)func->ret.type->primitive + delta);

                func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
            }
        } break;

        case PrimitiveKind::Record:
        case PrimitiveKind::Union: {
            HfaInfo hfa = IsHFA(func->ret.type);

            if (hfa.count) {
                func->ret.abi.regular = true;
                func->ret.abi.offset = offsetof(BackRegisters, d0);

                if (hfa.float32) {
                    func->ret.type = ReshapeType(instance, func->ret.type, 8, 0);
                }

                Opcode run = func->forward_fp ? Opcode::RunAggregateDDDDX : Opcode::RunAggregateDDDD;
                func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
            } else if (func->ret.type->size <= 16) {
                func->ret.abi.regular = true;
                func->ret.abi.offset = offsetof(BackRegisters, x0);

                Opcode run = func->forward_fp ? Opcode::RunAggregateGGX : Opcode::RunAggregateGG;
                func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
            } else {
                Opcode run = func->forward_fp ? Opcode::RunAggregateMemX : Opcode::RunAggregateMem;
                int16_t offset = 8 * 8; // x8

                func->sync.Append({ .op = Code2Op(run), .a = (int32_t)func->ret.type->size, .b1 = offset, .type = func->ret.type });
            }
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;

        case PrimitiveKind::Float32: {
            Opcode run = func->forward_fp ? Opcode::RunFloat32X : Opcode::RunFloat32;
            func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
        } break;
        case PrimitiveKind::Float64: {
            Opcode run = func->forward_fp ? Opcode::RunFloat64X : Opcode::RunFloat64;
            func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
        } break;

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

    return true;
}

void CallData::Relay(Size idx, uint8_t *sp)
{
    TrampolineInfo *trampoline = &shared.trampolines[idx];
    const FunctionInfo *proto = trampoline->proto;

    uint8_t *in_ptr = sp + 48;
    BackRegisters *out_reg = (BackRegisters *)sp;

    K_DEFER_N(err_guard) {
        trampoline->state = -1;
        memset(out_reg, 0, K_SIZE(*out_reg));
    };

    napi_value arguments[MaxParameters];

#define POP_INTEGER(CType) \
        do { \
            const uint8_t *src = in_ptr + param.abi.offset; \
            CType v = *(const CType *)src; \
             \
            arguments[i] = NewInt(env, v); \
        } while (false)
#define POP_INTEGER_SWAP(CType) \
        do { \
            const uint8_t *src = in_ptr + param.abi.offset; \
            CType v = *(const CType *)src; \
             \
            arguments[i] = NewInt(env, ReverseBytes(v)); \
        } while (false)

    // Convert to JS arguments
    for (Size i = 0; i < proto->parameters.len; i++) {
        const ParameterInfo &param = proto->parameters[i];
        K_ASSERT(param.directions >= 1 && param.directions <= 3);

        switch (param.type->primitive) {
            case PrimitiveKind::Void: { K_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                const uint8_t *src = in_ptr + param.abi.offset;
                bool b = *(bool *)src;

                arguments[i] = Napi::Boolean::New(env, b);
            } break;

            case PrimitiveKind::Int8: { POP_INTEGER(int8_t); } break;
            case PrimitiveKind::UInt8: { POP_INTEGER(uint8_t); } break;
            case PrimitiveKind::Int16: { POP_INTEGER(int16_t); } break;
            case PrimitiveKind::Int16S: { POP_INTEGER_SWAP(int16_t); } break;
            case PrimitiveKind::UInt16: { POP_INTEGER(uint16_t); } break;
            case PrimitiveKind::UInt16S: { POP_INTEGER_SWAP(uint16_t); } break;
            case PrimitiveKind::Int32: { POP_INTEGER(int32_t); } break;
            case PrimitiveKind::Int32S: { POP_INTEGER_SWAP(int32_t); } break;
            case PrimitiveKind::UInt32: { POP_INTEGER(uint32_t); } break;
            case PrimitiveKind::UInt32S: { POP_INTEGER_SWAP(uint32_t); } break;
            case PrimitiveKind::Int64: { POP_INTEGER(int64_t); } break;
            case PrimitiveKind::Int64S: { POP_INTEGER_SWAP(int64_t); } break;
            case PrimitiveKind::UInt64: { POP_INTEGER(uint64_t); } break;
            case PrimitiveKind::UInt64S: { POP_INTEGER_SWAP(uint64_t); } break;

            case PrimitiveKind::String: {
                const uint8_t *src = in_ptr + param.abi.offset;
                const char *str = *(const char **)src;

                arguments[i] = NewString(env, str);

                if (param.type->dispose) {
                    param.type->dispose(instance, param.type, str);
                }
            } break;
            case PrimitiveKind::String16: {
                const uint8_t *src = in_ptr + param.abi.offset;
                const char16_t *str16 = *(const char16_t **)src;

                arguments[i] = NewString(env, str16);

                if (param.type->dispose) {
                    param.type->dispose(instance, param.type, str16);
                }
            } break;
            case PrimitiveKind::String32: {
                const uint8_t *src = in_ptr + param.abi.offset;
                const char32_t *str32 = *(const char32_t **)src;

                arguments[i] = NewString(env, str32);

                if (param.type->dispose) {
                    param.type->dispose(instance, param.type, str32);
                }
            } break;

            case PrimitiveKind::Pointer: {
                const uint8_t *src = in_ptr + param.abi.offset;
                void *ptr2 = *(void **)src;

                arguments[i] = WrapPointer(env, ptr2);

                if (param.type->dispose) {
                    param.type->dispose(instance, param.type, ptr2);
                }
            } break;

            case PrimitiveKind::Record:
            case PrimitiveKind::Union: {
                uint8_t *ptr = in_ptr + param.abi.offset;
                uint8_t *src = param.abi.indirect ? *(uint8_t **)ptr : ptr;

                arguments[i] = DecodeObject(instance, src, param.type);
            } break;
            case PrimitiveKind::Array: { K_UNREACHABLE(); } break;

            case PrimitiveKind::Float32: {
                const uint8_t *src = in_ptr + param.abi.offset;
                float f = *(float *)src;

                arguments[i] = NewFloat(env, f);
            } break;
            case PrimitiveKind::Float64: {
                const uint8_t *src = in_ptr + param.abi.offset;
                double d = *(double *)src;

                arguments[i] = NewFloat(env, d);
            } break;

            case PrimitiveKind::Callback: {
                const uint8_t *src = in_ptr + param.abi.offset;
                void *ptr2 = *(void **)src;

                arguments[i] = WrapPointer(env, ptr2);
            } break;

            case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
        }
    }

#undef POP_INTEGER_SWAP
#undef POP_INTEGER

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
            out_reg->x0 = (uint64_t)v; \
        } while (false)
#define RETURN_INTEGER_SWAP(CType) \
        do { \
            CType v; \
            if (!TryNumber(env, value, &v)) [[unlikely]] { \
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
            uint64_t *gpr_ptr = (uint64_t *)in_ptr;
            uint8_t *dest = proto->ret.abi.regular ? (uint8_t *)&out_reg + proto->ret.abi.offset : (uint8_t *)gpr_ptr[8]; // x8

            if (!PushObject(value, type, dest)) [[unlikely]]
                return;
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;

        case PrimitiveKind::Float32: {
            float f;
            if (!TryNumber(env, value, &f)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                return;
            }

            memset((uint8_t *)&out_reg->d0, 0, 8);
            memcpy(&out_reg->d0, &f, 4);
        } break;
        case PrimitiveKind::Float64: {
            double d;
            if (!TryNumber(env, value, &d)) [[unlikely]] {
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
