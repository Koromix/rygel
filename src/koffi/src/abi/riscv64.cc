// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#if __riscv_xlen == 64 || defined(__loongarch64)

#include "lib/native/base/base.hh"
#include "../ffi.hh"
#include "../call.hh"
#include "../forward.hh"
#include "../type.hh"
#include "../util.hh"

#include <napi.h>

namespace K {

struct BackRegisters {
    uint64_t a0;
    uint64_t a1;

    double fa0;
    double fa1;
};

enum class AbiMethod {
    Memory,
    Gpr,
    GprGpr,
    Vec,
    VecVec,
    GprVec,
    VecGpr,
    GprStack
};

struct ClassResult {
    AbiMethod method;

    int gpr_index;
    int vec_index;
};

class ClassAnalyser {
    int gpr_max;
    int vec_max;
    int gpr_index; // Can go beyond gpr_max when using stack
    int vec_index;
    int stack_start;

public:
    ClassAnalyser(int gpr_index, int gpr_max, int vec_index, int vec_max, int stack_start = -1)
        : gpr_max(gpr_max), vec_max(vec_max), gpr_index(gpr_index), vec_index(vec_index), stack_start(stack_start) {}

    ClassResult Analyse(const TypeInfo *type, bool variadic);

    int GprCount() const { return gpr_index; }
    int VecCount() const { return vec_index; }
};

ClassResult ClassAnalyser::Analyse(const TypeInfo *type, bool variadic)
{
    ClassResult ret = {};

    // Use memory for values bigger than two registers
    if (type->size > 16) {
        ret.method = AbiMethod::Memory;
        ret.gpr_index = gpr_index;

        gpr_index++;

        return ret;
    }

    int gpr_avail = std::min(2, gpr_max - gpr_index); // Can go negative
    int vec_avail = std::min(2, vec_max - vec_index);

#if defined(__riscv_float_abi_double) || defined(__loongarch64)
    if (type->primitive != PrimitiveKind::Union && !variadic) {
        int gpr_count = 0;
        int vec_count = 0;
        bool gpr_vec = false;

        AnalyseFlat(type, [&](const TypeInfo *type, int offset, int count) {
            if (IsFloat(type)) {
                vec_count += count;
            } else {
                gpr_count += count;
                gpr_vec |= !vec_count;
            }
        });

        // Pass mixed float-integer structs in one GPR and one FP register
        if (gpr_count == 1 && vec_count == 1 && gpr_avail > 0 && vec_avail) {
            ret.method = gpr_vec ? AbiMethod::GprVec : AbiMethod::VecGpr;
            ret.gpr_index = gpr_index;
            ret.vec_index = vec_index;

            gpr_index++;
            vec_index++;

            return ret;
        }

        // HFA rules
        if (vec_count && !gpr_count && vec_count <= vec_avail) {
            ret.method = (vec_count > 1) ? AbiMethod::VecVec : AbiMethod::Vec;
            ret.vec_index = vec_index;

            vec_index += vec_count;

            return ret;
        }
    }
#elif defined(__riscv_float_abi_single)
    #error The RISC-V single-precision float ABI (LP64F) is not supported
#elif defined(__riscv_float_abi_soft)
    // Use integer conventions
#else
    #error Unknown or unsupported floating-point ABI
#endif

    // Default case: GPR, GPR-stack or stack if no GPR is left
    if (type->size > 8 && gpr_avail == 1) {
        ret.method = AbiMethod::GprStack;
        ret.gpr_index = gpr_index;

        gpr_index = stack_start + 1;
    } else if (type->size > 8) {
        ret.method = AbiMethod::GprGpr;
        ret.gpr_index = gpr_index;

        gpr_index += 2;
    } else {
        ret.method = AbiMethod::Gpr;
        ret.gpr_index = gpr_index;

        gpr_index++;
    }

    // Switch to stack once GPRs are exhausted
    if (gpr_index >= gpr_max && gpr_index < stack_start) {
        gpr_index = stack_start;
    };

    return ret;
}

bool AnalyseFunction(Napi::Env, InstanceData *instance, FunctionInfo *func)
{
    // Handle return value
    {
        ClassAnalyser analyser(0, 2, 0, 2);
        ClassResult ret = analyser.Analyse(func->ret.type, false);

        func->ret.abi.method = ret.method;

        switch (ret.method) {
            case AbiMethod::Memory: {} break;

            case AbiMethod::Gpr: {
                func->ret.abi.offsets[0] = offsetof(BackRegisters, a0);
            } break;
            case AbiMethod::GprGpr: {
                func->ret.abi.offsets[0] = offsetof(BackRegisters, a0);
                func->ret.abi.offsets[1] = offsetof(BackRegisters, a1);
            } break;
            case AbiMethod::Vec: {
                func->ret.abi.offsets[0] = offsetof(BackRegisters, fa0);
            } break;
            case AbiMethod::VecVec: {
                func->ret.abi.offsets[0] = offsetof(BackRegisters, fa0);
                func->ret.abi.offsets[1] = offsetof(BackRegisters, fa1);

                func->ret.type = ReshapeType(instance, func->ret.type, 8, (int)TypeFlag::FillWithOnes);
            } break;
            case AbiMethod::GprVec: {
                func->ret.abi.offsets[0] = offsetof(BackRegisters, a0);
                func->ret.abi.offsets[1] = offsetof(BackRegisters, fa0);

                func->ret.type = ReshapeType(instance, func->ret.type, 8, (int)TypeFlag::FillWithOnes);
            } break;
            case AbiMethod::VecGpr: {
                func->ret.abi.offsets[0] = offsetof(BackRegisters, fa0);
                func->ret.abi.offsets[1] = offsetof(BackRegisters, a0);

                func->ret.type = ReshapeType(instance, func->ret.type, 8, (int)TypeFlag::FillWithOnes);
            } break;

            case AbiMethod::GprStack: { K_UNREACHABLE(); } break;
        }
    }

    // Handle parameters
    {
        int gpr_result = (func->ret.abi.method == AbiMethod::Memory);
        ClassAnalyser analyser(gpr_result, 8, 0, 8, 18);

        for (ParameterInfo &param: func->parameters) {
            ClassResult ret = analyser.Analyse(param.type, param.variadic);

            param.abi.method = ret.method;

            switch (param.abi.method) {
                case AbiMethod::Memory: {
                    param.abi.offsets[0] = 8 * (0 + ret.gpr_index);
                } break;

                case AbiMethod::Gpr: {
                    param.abi.offsets[0] = 8 * (0 + ret.gpr_index);
                    param.abi.offsets[1] = 8 * (0 + ret.gpr_index);
                } break;
                case AbiMethod::GprGpr: {
                    param.abi.offsets[0] = 8 * (0 + ret.gpr_index);
                    param.abi.offsets[1] = 8 * (1 + ret.gpr_index);
                } break;
                case AbiMethod::Vec: {
                    param.abi.offsets[0] = 8 * (8 + ret.vec_index);
                    param.abi.offsets[1] = 8 * (8 + ret.vec_index);
                } break;
                case AbiMethod::VecVec: {
                    param.type = ReshapeType(instance, param.type, 8, (int)TypeFlag::FillWithOnes);

                    param.abi.offsets[0] = 8 * (8 + ret.vec_index);
                    param.abi.offsets[1] = 8 * (9 + ret.vec_index);
                } break;
                case AbiMethod::GprVec: {
                    param.type = ReshapeType(instance, param.type, 8, (int)TypeFlag::FillWithOnes);

                    param.abi.offsets[0] = 8 * (0 + ret.gpr_index);
                    param.abi.offsets[1] = 8 * (8 + ret.vec_index);
                } break;
                case AbiMethod::VecGpr: {
                    param.type = ReshapeType(instance, param.type, 8, (int)TypeFlag::FillWithOnes);

                    param.abi.offsets[0] = 8 * (8 + ret.vec_index);
                    param.abi.offsets[1] = 8 * (0 + ret.gpr_index);
                } break;
                case AbiMethod::GprStack: {
                    param.abi.offsets[0] = 8 * (0 + ret.gpr_index);
                    param.abi.offsets[1] = 18 * 8;
                } break;
            }

            if (param.type->primitive == PrimitiveKind::Record || param.type->primitive == PrimitiveKind::Union) {
                AbiOpcode code = (param.abi.method != AbiMethod::Memory) ? AbiOpcode::PushAggregateSplit : AbiOpcode::PushAggregateMem;

                func->sync.Append({ .op = Code2Op(code), .a = param.offset, .b1 = (int16_t)param.abi.offsets[0], .b2 = (int16_t)param.abi.offsets[1], .type = param.type });
                func->async.Append({ .op = Code2Op(code), .a = param.offset, .b1 = (int16_t)param.abi.offsets[0], .b2 = (int16_t)param.abi.offsets[1], .type = param.type });
            } else {
                int delta = (int)AbiOpcode::PushVoid - (int)PrimitiveKind::Void;
                AbiOpcode code = (AbiOpcode)((int)param.type->primitive + delta);

                func->sync.Append({ .op = Code2Op(code), .a = param.offset, .b1 = (int16_t)param.abi.offsets[0], .b2 = (int16_t)param.directions, .type = param.type });
                func->async.Append({ .op = Code2Op(code), .a = param.offset, .b1 = (int16_t)param.abi.offsets[0], .b2 = (int16_t)param.directions, .type = param.type });
            }
        }

        func->stk_size = AlignLen((10 + std::max(8, analyser.GprCount())) * 8, 16);
        func->forward_fp = analyser.VecCount();
    }

    func->async.Append({ .op = Code2Op(AbiOpcode::Yield) });

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
                int delta = (int)AbiOpcode::RunVoidX - (int)PrimitiveKind::Void;
                AbiOpcode run = (AbiOpcode)((int)func->ret.type->primitive + delta);

                func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
            } else {
                int delta = (int)AbiOpcode::RunVoid - (int)PrimitiveKind::Void;
                AbiOpcode run = (AbiOpcode)((int)func->ret.type->primitive + delta);

                func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
            }

            // Async
            {
                int delta = (int)AbiOpcode::ReturnVoid - (int)PrimitiveKind::Void;
                AbiOpcode call = func->forward_fp ? AbiOpcode::CallGX : AbiOpcode::CallG;
                AbiOpcode ret = (AbiOpcode)((int)func->ret.type->primitive + delta);

                func->async.Append({ .op = Code2Op(call) });
                func->async.Append({ .op = Code2Op(ret), .type = func->ret.type });
            }
        } break;

        case PrimitiveKind::Record:
        case PrimitiveKind::Union: {
            switch (func->ret.abi.method) {
                case AbiMethod::Memory: {
                    AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateMemX : AbiOpcode::RunAggregateMem;
                    AbiOpcode call = func->forward_fp ? AbiOpcode::CallMemX : AbiOpcode::CallMem;

                    func->sync.Append({ .op = Code2Op(run), .a = (int32_t)func->ret.type->size, .type = func->ret.type });
                    func->async.Append({ .op = Code2Op(call), .a = (int32_t)func->ret.type->size });
                    func->async.Append({ .op = Code2Op(AbiOpcode::ReturnAggregateMem), .type = func->ret.type });
                } break;

                case AbiMethod::Gpr:
                case AbiMethod::GprGpr:
                case AbiMethod::GprStack: {
                    AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateGGX : AbiOpcode::RunAggregateGG;
                    AbiOpcode call = func->forward_fp ? AbiOpcode::CallGGX : AbiOpcode::CallGG;

                    func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
                    func->async.Append({ .op = Code2Op(call) });
                    func->async.Append({ .op = Code2Op(AbiOpcode::ReturnAggregateReg), .type = func->ret.type });
                } break;
                case AbiMethod::Vec:
                case AbiMethod::VecVec: {
                    AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateDDX : AbiOpcode::RunAggregateDD;
                    AbiOpcode call = func->forward_fp ? AbiOpcode::CallDDX : AbiOpcode::CallDD;

                    func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
                    func->async.Append({ .op = Code2Op(call) });
                    func->async.Append({ .op = Code2Op(AbiOpcode::ReturnAggregateReg), .type = func->ret.type });
                } break;
                case AbiMethod::GprVec: {
                    AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateGDX : AbiOpcode::RunAggregateGD;
                    AbiOpcode call = func->forward_fp ? AbiOpcode::CallGDX : AbiOpcode::CallGD;

                    func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
                    func->async.Append({ .op = Code2Op(call) });
                    func->async.Append({ .op = Code2Op(AbiOpcode::ReturnAggregateReg), .type = func->ret.type });
                } break;
                case AbiMethod::VecGpr: {
                    AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateDGX : AbiOpcode::RunAggregateDG;
                    AbiOpcode call = func->forward_fp ? AbiOpcode::CallDGX : AbiOpcode::CallDG;

                    func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
                    func->async.Append({ .op = Code2Op(call) });
                    func->async.Append({ .op = Code2Op(AbiOpcode::ReturnAggregateReg), .type = func->ret.type });
                } break;
            }
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;

        case PrimitiveKind::Float32: {
            AbiOpcode run = func->forward_fp ? AbiOpcode::RunFloat32X : AbiOpcode::RunFloat32;
            AbiOpcode call = func->forward_fp ? AbiOpcode::CallFX : AbiOpcode::CallF;

            func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
            func->async.Append({ .op = Code2Op(call) });
            func->async.Append({ .op = Code2Op(AbiOpcode::ReturnFloat32), .type = func->ret.type });
        } break;
        case PrimitiveKind::Float64: {
            AbiOpcode run = func->forward_fp ? AbiOpcode::RunFloat64X : AbiOpcode::RunFloat64;
            AbiOpcode call = func->forward_fp ? AbiOpcode::CallDDX : AbiOpcode::CallDD;

            func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
            func->async.Append({ .op = Code2Op(call) });
            func->async.Append({ .op = Code2Op(AbiOpcode::ReturnFloat64), .type = func->ret.type });
        } break;

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

    return true;
}

void CallData::Relay(Size idx, uint8_t *sp)
{
    TrampolineInfo *trampoline = &shared.trampolines[idx];
    const FunctionInfo *proto = trampoline->proto;

    uint8_t *in_ptr = sp + 40;
    BackRegisters *out_reg = (BackRegisters *)sp;

    K_DEFER_N(err_guard) {
        trampoline->state = -1;
        memset(out_reg, 0, K_SIZE(*out_reg));
    };

    napi_value arguments[MaxParameters];

#define POP_INTEGER(CType) \
        do { \
            const uint8_t *src = in_ptr + param.abi.offsets[0]; \
            CType v = *(const CType *)src; \
             \
            arguments[i] = NewInt(env, v); \
        } while (false)
#define POP_INTEGER_SWAP(CType) \
        do { \
            const uint8_t *src = in_ptr + param.abi.offsets[0]; \
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
                const uint8_t *src = in_ptr + param.abi.offsets[0];
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
                const uint8_t *src = in_ptr + param.abi.offsets[0];
                const char *str = *(const char **)src;

                arguments[i] = NewString(env, str);

                if (param.type->dispose) {
                    param.type->dispose(instance, param.type, str);
                }
            } break;
            case PrimitiveKind::String16: {
                const uint8_t *src = in_ptr + param.abi.offsets[0];
                const char16_t *str16 = *(const char16_t **)src;

                arguments[i] = NewString(env, str16);

                if (param.type->dispose) {
                    param.type->dispose(instance, param.type, str16);
                }
            } break;
            case PrimitiveKind::String32: {
                const uint8_t *src = in_ptr + param.abi.offsets[0];
                const char32_t *str32 = *(const char32_t **)src;

                arguments[i] = NewString(env, str32);

                if (param.type->dispose) {
                    param.type->dispose(instance, param.type, str32);
                }
            } break;

            case PrimitiveKind::Pointer: {
                const uint8_t *src = in_ptr + param.abi.offsets[0];
                void *ptr2 = *(void **)src;

                arguments[i] = WrapPointer(env, ptr2);

                if (param.type->dispose) {
                    param.type->dispose(instance, param.type, ptr2);
                }
            } break;

            case PrimitiveKind::Record:
            case PrimitiveKind::Union: {
                if (param.abi.method != AbiMethod::Memory) {
                    uint8_t buf[16];
                    memcpy(buf, in_ptr + param.abi.offsets[0], 8);
                    memcpy(buf + 8, in_ptr + param.abi.offsets[1], 8);

                    arguments[i] = DecodeObject(instance, buf, param.type);
                } else {
                    uint8_t *ptr = *(uint8_t **)(in_ptr + param.abi.offsets[0]);
                    arguments[i] = DecodeObject(instance, ptr, param.type);
                }
            } break;
            case PrimitiveKind::Array: { K_UNREACHABLE(); } break;

            case PrimitiveKind::Float32: {
                const uint8_t *src = in_ptr + param.abi.offsets[0];
                float f = *(float *)src;

                arguments[i] = NewFloat(env, f);
            } break;
            case PrimitiveKind::Float64: {
                const uint8_t *src = in_ptr + param.abi.offsets[0];
                double d = *(double *)src;

                arguments[i] = NewFloat(env, d);
            } break;

            case PrimitiveKind::Callback: {
                const uint8_t *src = in_ptr + param.abi.offsets[0];
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
            out_reg->a0 = (uint64_t)v; \
        } while (false)
#define RETURN_INTEGER_SWAP(CType) \
        do { \
            CType v; \
            if (!TryNumber(env, value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value)); \
                return; \
            } \
             \
            out_reg->a0 = (uint64_t)ReverseBytes(v); \
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

            out_reg->a0 = (uint64_t)b;
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

            out_reg->a0 = (uint64_t)str;
        } break;
        case PrimitiveKind::String16: {
            const char16_t *str16;
            if (!PushString16(value, 1, &str16)) [[unlikely]]
                return;

            out_reg->a0 = (uint64_t)str16;
        } break;
        case PrimitiveKind::String32: {
            const char32_t *str32;
            if (!PushString32(value, 1, &str32)) [[unlikely]]
                return;

            out_reg->a0 = (uint64_t)str32;
        } break;
        case PrimitiveKind::Pointer: {
            void *ptr;
            if (!PushPointer(value, type, 1, &ptr)) [[unlikely]]
                return;

            out_reg->a0 = (uint64_t)ptr;
        } break;
        case PrimitiveKind::Record:
        case PrimitiveKind::Union: {
            if (proto->ret.abi.method != AbiMethod::Memory) {
                uint64_t buf[2];
                if (!PushObject(value, type, (uint8_t *)buf)) [[unlikely]]
                    return;

                memcpy((uint8_t *)&out_reg + proto->ret.abi.offsets[0], (const uint8_t *)buf, 8);
                memcpy((uint8_t *)&out_reg + proto->ret.abi.offsets[1], (const uint8_t *)buf + 8, 8);
            } else {
                uint64_t *gpr_ptr = (uint64_t *)in_ptr;
                uint8_t *dest = (uint8_t *)gpr_ptr[0]; // a0

                if (!PushObject(value, type, dest)) [[unlikely]]
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

            memset(&out_reg->fa0, 0xFF, 8);
            memcpy(&out_reg->fa0, &f, 4);
        } break;
        case PrimitiveKind::Float64: {
            double d;
            if (!TryNumber(env, value, &d)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                return;
            }

            out_reg->fa0 = d;
        } break;
        case PrimitiveKind::Callback: {
            void *ptr;
            if (!PushCallback(value, type, &ptr)) [[unlikely]]
                return;

            out_reg->a0 = (uint64_t)ptr;
        } break;

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

#undef RETURN_INTEGER_SWAP
#undef RETURN_INTEGER

    err_guard.Disable();
}

}

#endif
