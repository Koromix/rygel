// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#if defined(__x86_64__) && !defined(_WIN32)

#include "lib/native/base/base.hh"
#include "../ffi.hh"
#include "../call.hh"
#include "../interp.hh"
#include "../type.hh"
#include "../util.hh"

#include <napi.h>

namespace K {

struct BackRegisters {
    uint64_t rax;
    uint64_t rdx;
    double xmm0;
    double xmm1;
};

enum class AbiMethod {
    Stack,
    Gpr,
    GprGpr,
    Xmm,
    XmmXmm,
    GprXmm,
    XmmGpr
};

struct ClassResult {
    AbiMethod method;

    int gpr_index;
    int xmm_index;
    Size stack_offset;
};

class ClassAnalyser {
    enum class RegisterClass {
        NoClass = 0, // Explicitly 0
        Integer,
        SSE,
        Memory
    };

    int gpr_max;
    int xmm_max;
    int gpr_avail;
    int xmm_avail;

    Size stack_offset = 0;

public:
    ClassAnalyser(int gpr_index, int gpr_max, int xmm_index, int xmm_max)
        : gpr_max(gpr_max), xmm_max(xmm_max), gpr_avail(gpr_max - gpr_index), xmm_avail(xmm_max - xmm_index) {}

    ClassResult Analyse(const TypeInfo *type);

    Size Classify(Span<RegisterClass> classes, const TypeInfo *type, Size offset);
    RegisterClass MergeClasses(RegisterClass cls1, RegisterClass cls2);

    int GprCount() const { return gpr_max - gpr_avail; }
    int XmmCount() const { return xmm_max - xmm_avail; }
    Size StackOffset() const { return stack_offset; }
};

ClassResult ClassAnalyser::Analyse(const TypeInfo *type)
{
    ClassResult ret = {};

    LocalArray<RegisterClass, 8> classes = {};
    classes.len = Classify(classes.data, type, 0);

    if (classes.len <= 2) {
        int gpr_count = 0;
        int xmm_count = 0;

        for (RegisterClass cls: classes) {
            switch (cls) {
                case RegisterClass::NoClass: { K_UNREACHABLE(); } break;
                case RegisterClass::Integer: { gpr_count++; } break;
                case RegisterClass::SSE: { xmm_count++; } break;
                case RegisterClass::Memory: goto stack;
            }
        }

        if (gpr_count > gpr_avail || xmm_count > xmm_avail)
            goto stack;

        if (gpr_count && xmm_count) {
            bool gpr_xmm = (classes.len && classes[0] == RegisterClass::Integer);
            ret.method = gpr_xmm ? AbiMethod::GprXmm : AbiMethod::XmmGpr;
        } else if (xmm_count == 2) {
            ret.method = AbiMethod::XmmXmm;
        } else if (xmm_count == 1) {
            ret.method = AbiMethod::Xmm;
        } else if (gpr_count == 2) {
            ret.method = AbiMethod::GprGpr;
        } else {
            K_ASSERT(gpr_count <= 1 && !xmm_count);
            ret.method = AbiMethod::Gpr;
        }

        ret.gpr_index = (gpr_max - gpr_avail);
        ret.xmm_index = (xmm_max - xmm_avail);

        gpr_avail -= gpr_count;
        xmm_avail -= xmm_count;

        return ret;
    }

stack:
    {
        ret.method = AbiMethod::Stack;
        ret.stack_offset = stack_offset;

        stack_offset += AlignLen(type->size, 8);
    }

    return ret;
}

Size ClassAnalyser::Classify(Span<RegisterClass> classes, const TypeInfo *type, Size offset)
{
    K_ASSERT(classes.len > 0);

    switch (type->primitive) {
        case PrimitiveKind::Void: { return 0; } break;

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
            classes[0] = MergeClasses(classes[0], RegisterClass::Integer);
            return 1;
        } break;
        case PrimitiveKind::Record: {
            if (type->size > 64) {
                classes[0] = MergeClasses(classes[0], RegisterClass::Memory);
                return 1;
            }

            for (const RecordMember &member: type->members) {
                Size member_offset = offset + member.offset;
                Size start = member_offset / 8;
                Classify(classes.Take(start, classes.len - start), member.type, member_offset % 8);
            }
            offset += type->size;

            return (offset + 7) / 8;
        } break;
        case PrimitiveKind::Union: {
            if (type->size > 64) {
                classes[0] = MergeClasses(classes[0], RegisterClass::Memory);
                return 1;
            }

            for (const RecordMember &member: type->members) {
                Size start = offset / 8;
                Classify(classes.Take(start, classes.len - start), member.type, offset % 8);
            }
            offset += type->size;

            return (offset + 7) / 8;
        } break;
        case PrimitiveKind::Array: {
            if (type->size > 64) {
                classes[0] = MergeClasses(classes[0], RegisterClass::Memory);
                return 1;
            }

            Size len = type->size / type->ref.type->size;

            for (Size i = 0; i < len; i++) {
                Size start = offset / 8;
                Classify(classes.Take(start, classes.len - start), type->ref.type, offset % 8);
                offset += type->ref.type->size;
            }

            return (offset + 7) / 8;
        } break;
        case PrimitiveKind::Float32:
        case PrimitiveKind::Float64: {
            classes[0] = MergeClasses(classes[0], RegisterClass::SSE);
            return 1;
        } break;

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

    K_UNREACHABLE();
}

ClassAnalyser::RegisterClass ClassAnalyser::MergeClasses(RegisterClass cls1, RegisterClass cls2)
{
    if (cls1 == cls2)
        return cls1;

    if (cls1 == RegisterClass::NoClass)
        return cls2;
    if (cls2 == RegisterClass::NoClass)
        return cls1;

    if (cls1 == RegisterClass::Memory || cls2 == RegisterClass::Memory)
        return RegisterClass::Memory;
    if (cls1 == RegisterClass::Integer || cls2 == RegisterClass::Integer)
        return RegisterClass::Integer;

    return RegisterClass::SSE;
}

bool AnalyseFunction(Napi::Env, InstanceData *, FunctionInfo *func)
{
    // Handle return value
    {
        ClassAnalyser analyser(0, 2, 0, 2);
        ClassResult ret = analyser.Analyse(func->ret.type);

        func->ret.abi.method = ret.method;
    }

    // Handle parameters
    {
        int gpr_result = (func->ret.abi.method == AbiMethod::Stack);
        ClassAnalyser analyser(gpr_result, 6, 0, 8);

        for (ParameterInfo &param: func->parameters) {
            ClassResult ret = analyser.Analyse(param.type);

            switch (ret.method) {
                case AbiMethod::Stack: {
                    param.abi.regular = false;
                    param.abi.offsets[0] = 16 * 8 + ret.stack_offset;
                } break;
                case AbiMethod::Gpr: {
                    param.abi.regular = true;
                    param.abi.offsets[0] = (0 + ret.gpr_index) * 8;
                    param.abi.offsets[1] = param.abi.offsets[0];
                } break;
                case AbiMethod::GprGpr: {
                    param.abi.regular = true;
                    param.abi.offsets[0] = (0 + ret.gpr_index) * 8;
                    param.abi.offsets[1] = param.abi.offsets[0] + 8;
                } break;
                case AbiMethod::Xmm: {
                    param.abi.regular = true;
                    param.abi.offsets[0] = (6 + ret.xmm_index) * 8;
                    param.abi.offsets[1] = param.abi.offsets[0];
                } break;
                case AbiMethod::XmmXmm: {
                    param.abi.regular = true;
                    param.abi.offsets[0] = (6 + ret.xmm_index) * 8;
                    param.abi.offsets[1] = param.abi.offsets[0] + 8;
                } break;
                case AbiMethod::GprXmm: {
                    param.abi.regular = true;
                    param.abi.offsets[0] = (0 + ret.gpr_index) * 8;
                    param.abi.offsets[1] = (6 + ret.xmm_index) * 8;
                } break;
                case AbiMethod::XmmGpr: {
                    param.abi.regular = true;
                    param.abi.offsets[0] = (6 + ret.xmm_index) * 8;
                    param.abi.offsets[1] = (0 + ret.gpr_index) * 8;
                } break;
            }

            if (param.type->primitive == PrimitiveKind::Record || param.type->primitive == PrimitiveKind::Union) {
                Opcode code = param.abi.regular ? Opcode::PushAggregateSplit : Opcode::PushAggregateStack;

                func->sync.Append({ .op = Code2Op(code), .a = param.offset, .b1 = (int16_t)param.abi.offsets[0], .b2 = (int16_t)param.abi.offsets[1], .type = param.type });
                func->async.Append({ .op = Code2Op(code), .a = param.offset, .b1 = (int16_t)param.abi.offsets[0], .b2 = (int16_t)param.abi.offsets[1], .type = param.type });
            } else {
                int delta = (int)Opcode::PushVoid - (int)PrimitiveKind::Void;
                Opcode code = (Opcode)((int)param.type->primitive + delta);

                func->sync.Append({ .op = Code2Op(code), .a = param.offset, .b1 = (int16_t)param.abi.offsets[0], .b2 = (int16_t)param.directions, .type = param.type });
                func->async.Append({ .op = Code2Op(code), .a = param.offset, .b1 = (int16_t)param.abi.offsets[0], .b2 = (int16_t)param.directions, .type = param.type });
            }
        }

        func->stk_size = AlignLen(16 * 8 + analyser.StackOffset(), 16);
        func->forward_fp = analyser.XmmCount();
    }

    func->async.Append({ .op = Code2Op(Opcode::Yield) });

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

            // Async
            {
                int delta = (int)Opcode::ReturnVoid - (int)PrimitiveKind::Void;
                Opcode call = func->forward_fp ? Opcode::CallGX : Opcode::CallG;
                Opcode ret = (Opcode)((int)func->ret.type->primitive + delta);

                func->async.Append({ .op = Code2Op(call) });
                func->async.Append({ .op = Code2Op(ret), .type = func->ret.type });
            }
        } break;

        case PrimitiveKind::Record:
        case PrimitiveKind::Union: {
            switch (func->ret.abi.method) {
                case AbiMethod::Stack: {
                    Opcode run = func->forward_fp ? Opcode::RunAggregateMemX : Opcode::RunAggregateMem;
                    Opcode call = func->forward_fp ? Opcode::CallMemX : Opcode::CallMem;

                    func->sync.Append({ .op = Code2Op(run), .a = (int32_t)func->ret.type->size, .type = func->ret.type });
                    func->async.Append({ .op = Code2Op(call), .a = (int32_t)func->ret.type->size });
                    func->async.Append({ .op = Code2Op(Opcode::ReturnAggregateMem), .type = func->ret.type });

                    // Allocate stack space for return value
                    func->stk_size += AlignLen(func->ret.type->size, 16);
                } break;
                case AbiMethod::Gpr: {
                    Opcode run = func->forward_fp ? Opcode::RunAggregateGGX : Opcode::RunAggregateGG;
                    Opcode call = func->forward_fp ? Opcode::CallGX : Opcode::CallG;

                    func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
                    func->async.Append({ .op = Code2Op(call) });
                    func->async.Append({ .op = Code2Op(Opcode::ReturnAggregateReg), .type = func->ret.type });
                } break;
                case AbiMethod::GprGpr: {
                    Opcode run = func->forward_fp ? Opcode::RunAggregateGGX : Opcode::RunAggregateGG;
                    Opcode call = func->forward_fp ? Opcode::CallGGX : Opcode::CallGG;

                    func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
                    func->async.Append({ .op = Code2Op(call) });
                    func->async.Append({ .op = Code2Op(Opcode::ReturnAggregateReg), .type = func->ret.type });
                } break;
                case AbiMethod::Xmm: {
                    Opcode run = func->forward_fp ? Opcode::RunAggregateDDX : Opcode::RunAggregateDD;
                    Opcode call = func->forward_fp ? Opcode::CallDX : Opcode::CallD;

                    func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
                    func->async.Append({ .op = Code2Op(call) });
                    func->async.Append({ .op = Code2Op(Opcode::ReturnAggregateReg), .type = func->ret.type });
                } break;
                case AbiMethod::XmmXmm: {
                    Opcode run = func->forward_fp ? Opcode::RunAggregateDDX : Opcode::RunAggregateDD;
                    Opcode call = func->forward_fp ? Opcode::CallDDX : Opcode::CallDD;

                    func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
                    func->async.Append({ .op = Code2Op(call) });
                    func->async.Append({ .op = Code2Op(Opcode::ReturnAggregateReg), .type = func->ret.type });
                } break;
                case AbiMethod::GprXmm: {
                    Opcode run = func->forward_fp ? Opcode::RunAggregateGDX : Opcode::RunAggregateGD;
                    Opcode call = func->forward_fp ? Opcode::CallGDX : Opcode::CallGD;

                    func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
                    func->async.Append({ .op = Code2Op(call) });
                    func->async.Append({ .op = Code2Op(Opcode::ReturnAggregateReg), .type = func->ret.type });
                } break;
                case AbiMethod::XmmGpr: {
                    Opcode run = func->forward_fp ? Opcode::RunAggregateDGX : Opcode::RunAggregateDG;
                    Opcode call = func->forward_fp ? Opcode::CallDGX : Opcode::CallDG;

                    func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
                    func->async.Append({ .op = Code2Op(call) });
                    func->async.Append({ .op = Code2Op(Opcode::ReturnAggregateReg), .type = func->ret.type });
                } break;
            }
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;

        case PrimitiveKind::Float32: {
            Opcode run = func->forward_fp ? Opcode::RunFloat32X : Opcode::RunFloat32;
            Opcode call = func->forward_fp ? Opcode::CallFX : Opcode::CallF;

            func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
            func->async.Append({ .op = Code2Op(call) });
            func->async.Append({ .op = Code2Op(Opcode::ReturnFloat32), .type = func->ret.type });
        } break;
        case PrimitiveKind::Float64: {
            Opcode run = func->forward_fp ? Opcode::RunFloat64X : Opcode::RunFloat64;
            Opcode call = func->forward_fp ? Opcode::CallDX : Opcode::CallD;

            func->sync.Append({ .op = Code2Op(run), .type = func->ret.type });
            func->async.Append({ .op = Code2Op(call) });
            func->async.Append({ .op = Code2Op(Opcode::ReturnFloat64), .type = func->ret.type });
        } break;

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

    return true;
}

void CallData::Relay(Size idx, uint8_t *sp)
{
    TrampolineInfo *trampoline = &shared.trampolines[idx];
    const FunctionInfo *proto = trampoline->proto;

    uint8_t *in_ptr = sp + 32;
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
                if (param.abi.regular) {
                    uint64_t buf[2];
                    buf[0] = *(uint64_t *)(in_ptr + param.abi.offsets[0]);
                    buf[1] = *(uint64_t *)(in_ptr + param.abi.offsets[1]);

                    arguments[i] = DecodeObject(instance, (const uint8_t *)buf, param.type);
                } else {
                    arguments[i] = DecodeObject(instance, in_ptr + param.abi.offsets[0], param.type);
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

    // Convert the result
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
            if (proto->ret.abi.method == AbiMethod::Stack) {
                uint64_t *gpr_ptr = (uint64_t *)in_ptr;
                uint8_t *dest = (uint8_t *)gpr_ptr[0];

                if (!PushObject(value, type, dest)) [[unlikely]]
                    return;

                out_reg->rax = (uint64_t)dest;
            } else {
                K_ASSERT(type->size <= 16);

                uint8_t buf[16] = {};
                if (!PushObject(value, type, buf)) [[unlikely]]
                    return;

                switch (proto->ret.abi.method) {
                    case AbiMethod::Stack: { K_UNREACHABLE(); } break;

                    case AbiMethod::Gpr: {
                        memcpy(&out_reg->rax, buf + 0, 8);
                    } break;
                    case AbiMethod::GprGpr: {
                        memcpy(&out_reg->rax, buf + 0, 8);
                        memcpy(&out_reg->rdx, buf + 8, 8);
                    } break;
                    case AbiMethod::Xmm: {
                        memcpy(&out_reg->xmm0, buf + 0, 8);
                    } break;
                    case AbiMethod::XmmXmm: {
                        memcpy(&out_reg->xmm0, buf + 0, 8);
                        memcpy(&out_reg->xmm1, buf + 8, 8);
                    } break;
                    case AbiMethod::GprXmm: {
                        memcpy(&out_reg->rax, buf + 0, 8);
                        memcpy(&out_reg->xmm0, buf + 8, 8);
                    } break;
                    case AbiMethod::XmmGpr: {
                        memcpy(&out_reg->xmm0, buf + 0, 8);
                        memcpy(&out_reg->rax, buf + 8, 8);
                    } break;
                }
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
