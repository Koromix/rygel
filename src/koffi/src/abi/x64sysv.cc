// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#if defined(__x86_64__) && !defined(_WIN32)

#include "lib/native/base/base.hh"
#include "../ffi.hh"
#include "../call.hh"
#include "../util.hh"

#include <napi.h>

namespace K {

struct RaxRdxRet {
    uint64_t rax;
    uint64_t rdx;
};
struct RaxXmm0Ret {
    uint64_t rax;
    double xmm0;
};
struct Xmm0RaxRet {
    double xmm0;
    uint64_t rax;
};
struct Xmm0Xmm1Ret {
    double xmm0;
    double xmm1;
};

struct BackRegisters {
    uint64_t rax;
    uint64_t rdx;
    double xmm0;
    double xmm1;
};

extern "C" RaxRdxRet ForwardCallGG(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" float ForwardCallF(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" Xmm0RaxRet ForwardCallDG(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" RaxXmm0Ret ForwardCallGD(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" Xmm0Xmm1Ret ForwardCallDD(const void *func, uint8_t *sp, uint8_t **out_old_sp);

extern "C" RaxRdxRet ForwardCallXGG(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" float ForwardCallXF(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" Xmm0RaxRet ForwardCallXDG(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" RaxXmm0Ret ForwardCallXGD(const void *func, uint8_t *sp, uint8_t **out_old_sp);
extern "C" Xmm0Xmm1Ret ForwardCallXDD(const void *func, uint8_t *sp, uint8_t **out_old_sp);

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
    RunAggregateStack,
    RunAggregateGG,
    RunAggregateDD,
    RunAggregateGD,
    RunAggregateDG,
    #define PRIMITIVE(Name) Run ## Name ## X,
    #include "../primitives.inc"
    RunAggregateXStack,
    RunAggregateXGG,
    RunAggregateXDD,
    RunAggregateXGD,
    RunAggregateXDG,
    Yield,
    CallGG,
    CallF,
    CallDG,
    CallGD,
    CallDD,
    CallStack,
    CallXGG,
    CallXF,
    CallXDG,
    CallXGD,
    CallXDD,
    CallXStack,
    #define PRIMITIVE(Name) Return ## Name,
    #include "../primitives.inc"
    ReturnAggregate
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
    ClassAnalyser(int gprs, int xmms)
        : gpr_max(gprs), xmm_max(xmms), gpr_avail(gprs), xmm_avail(xmms) {}

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
        ClassAnalyser analyser(2, 2);
        ClassResult ret = analyser.Analyse(func->ret.type);

        func->ret.abi.method = ret.method;
    }

    // Handle parameters
    {
        int gpr_base = (func->ret.abi.method == AbiMethod::Stack);
        ClassAnalyser analyser(6 - gpr_base, 8);

        for (ParameterInfo &param: func->parameters) {
            ClassResult ret = analyser.Analyse(param.type);

            switch (ret.method) {
                case AbiMethod::Stack: {
                    param.abi.regular = false;
                    param.abi.offsets[0] = 14 * 8 + ret.stack_offset;
                } break;
                case AbiMethod::Gpr: {
                    param.abi.regular = true;
                    param.abi.offsets[0] = (gpr_base + ret.gpr_index) * 8;
                    param.abi.offsets[1] = param.abi.offsets[0];
                } break;
                case AbiMethod::GprGpr: {
                    param.abi.regular = true;
                    param.abi.offsets[0] = (gpr_base + ret.gpr_index) * 8;
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
                    param.abi.offsets[0] = (gpr_base + ret.gpr_index) * 8;
                    param.abi.offsets[1] = (6 + ret.xmm_index) * 8;
                } break;
                case AbiMethod::XmmGpr: {
                    param.abi.regular = true;
                    param.abi.offsets[0] = (6 + ret.xmm_index) * 8;
                    param.abi.offsets[1] = (gpr_base + ret.gpr_index) * 8;
                } break;
            }

            if (param.type->primitive == PrimitiveKind::Record || param.type->primitive == PrimitiveKind::Union) {
                AbiOpcode code = param.abi.regular ? AbiOpcode::PushAggregateReg : AbiOpcode::PushAggregateStack;

                func->sync.Append({ .code = code, .a = param.offset, .b1 = (int16_t)param.abi.offsets[0], .b2 = (int16_t)param.abi.offsets[1], .type = param.type });
                func->async.Append({ .code = code, .a = param.offset, .b1 = (int16_t)param.abi.offsets[0], .b2 = (int16_t)param.abi.offsets[1], .type = param.type });
            } else {
                int delta = (int)AbiOpcode::PushVoid - (int)PrimitiveKind::Void;
                AbiOpcode code = (AbiOpcode)((int)param.type->primitive + delta);

                func->sync.Append({ .code = code, .a = param.offset, .b1 = (int16_t)param.abi.offsets[0], .b2 = (int16_t)param.directions, .type = param.type });
                func->async.Append({ .code = code, .a = param.offset, .b1 = (int16_t)param.abi.offsets[0], .b2 = (int16_t)param.directions, .type = param.type });
            }
        }

        func->stk_size = AlignLen(14 * 8 + analyser.StackOffset(), 16);
        func->forward_fp = analyser.XmmCount();
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
                AbiOpcode call = func->forward_fp ? AbiOpcode::CallXGG : AbiOpcode::CallGG;
                AbiOpcode ret = (AbiOpcode)((int)func->ret.type->primitive + delta);

                func->async.Append({ .code = call });
                func->async.Append({ .code = ret, .type = func->ret.type });
            }
        } break;

        case PrimitiveKind::Pointer: {
            AbiOpcode run = func->forward_fp ? AbiOpcode::RunPointerX : AbiOpcode::RunPointer;
            AbiOpcode call = func->forward_fp ? AbiOpcode::CallXGG : AbiOpcode::CallGG;

            func->sync.Append({ .code = run, .type = func->ret.type->ref.type });
            func->async.Append({ .code = call });
            func->async.Append({ .code = AbiOpcode::ReturnPointer, .type = func->ret.type->ref.type });
        } break;
        case PrimitiveKind::Callback: {
            AbiOpcode run = func->forward_fp ? AbiOpcode::RunCallbackX : AbiOpcode::RunCallback;
            AbiOpcode call = func->forward_fp ? AbiOpcode::CallXGG : AbiOpcode::CallGG;

            func->sync.Append({ .code = run, .type = func->ret.type });
            func->async.Append({ .code = call });
            func->async.Append({ .code = AbiOpcode::ReturnCallback, .type = func->ret.type });
        } break;

        case PrimitiveKind::Record:
        case PrimitiveKind::Union: {
            switch (func->ret.abi.method) {
                case AbiMethod::Stack: {
                    AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateXStack : AbiOpcode::RunAggregateStack;
                    AbiOpcode call = func->forward_fp ? AbiOpcode::CallXStack : AbiOpcode::CallStack;

                    func->sync.Append({ .code = run, .b = (int32_t)func->stk_size, .type = func->ret.type });
                    func->async.Append({ .code = call, .b = (int32_t)func->stk_size });
                    func->async.Append({ .code = AbiOpcode::ReturnAggregate, .type = func->ret.type });

                    // Allocate stack space for return value
                    func->stk_size += AlignLen(func->ret.type->size, 16);
                } break;
                case AbiMethod::Gpr: {
                    AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateXGG : AbiOpcode::RunAggregateGG;
                    AbiOpcode call = func->forward_fp ? AbiOpcode::CallXGG : AbiOpcode::CallGG;

                    func->sync.Append({ .code = run, .type = func->ret.type });
                    func->async.Append({ .code = call });
                    func->async.Append({ .code = AbiOpcode::ReturnAggregate, .type = func->ret.type });
                } break;
                case AbiMethod::GprGpr: {
                    AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateXGG : AbiOpcode::RunAggregateGG;
                    AbiOpcode call = func->forward_fp ? AbiOpcode::CallXGG : AbiOpcode::CallGG;

                    func->sync.Append({ .code = run, .type = func->ret.type });
                    func->async.Append({ .code = call });
                    func->async.Append({ .code = AbiOpcode::ReturnAggregate, .type = func->ret.type });
                } break;
                case AbiMethod::Xmm: {
                    AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateXDD : AbiOpcode::RunAggregateDD;
                    AbiOpcode call = func->forward_fp ? AbiOpcode::CallXDD : AbiOpcode::CallDD;

                    func->sync.Append({ .code = run, .type = func->ret.type });
                    func->async.Append({ .code = call });
                    func->async.Append({ .code = AbiOpcode::ReturnAggregate, .type = func->ret.type });
                } break;
                case AbiMethod::XmmXmm: {
                    AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateXDD : AbiOpcode::RunAggregateDD;
                    AbiOpcode call = func->forward_fp ? AbiOpcode::CallXDD : AbiOpcode::CallDD;

                    func->sync.Append({ .code = run, .type = func->ret.type });
                    func->async.Append({ .code = call });
                    func->async.Append({ .code = AbiOpcode::ReturnAggregate, .type = func->ret.type });
                } break;
                case AbiMethod::GprXmm: {
                    AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateXGD : AbiOpcode::RunAggregateGD;
                    AbiOpcode call = func->forward_fp ? AbiOpcode::CallXGD : AbiOpcode::CallGD;

                    func->sync.Append({ .code = run, .type = func->ret.type });
                    func->async.Append({ .code = call });
                    func->async.Append({ .code = AbiOpcode::ReturnAggregate, .type = func->ret.type });
                } break;
                case AbiMethod::XmmGpr: {
                    AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateXDG : AbiOpcode::RunAggregateDG;
                    AbiOpcode call = func->forward_fp ? AbiOpcode::CallXDG : AbiOpcode::CallDG;

                    func->sync.Append({ .code = run, .type = func->ret.type });
                    func->async.Append({ .code = call });
                    func->async.Append({ .code = AbiOpcode::ReturnAggregate, .type = func->ret.type });
                } break;
            }
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;

        case PrimitiveKind::Float32: {
            AbiOpcode run = func->forward_fp ? AbiOpcode::RunFloat32X : AbiOpcode::RunFloat32;
            AbiOpcode call = func->forward_fp ? AbiOpcode::CallXF : AbiOpcode::CallF;

            func->sync.Append({ .code = run, .type = func->ret.type });
            func->async.Append({ .code = call });
            func->async.Append({ .code = AbiOpcode::ReturnFloat32, .type = func->ret.type });
        } break;
        case PrimitiveKind::Float64: {
            AbiOpcode run = func->forward_fp ? AbiOpcode::RunFloat64X : AbiOpcode::RunFloat64;
            AbiOpcode call = func->forward_fp ? AbiOpcode::CallXDD : AbiOpcode::CallDD;

            func->sync.Append({ .code = run, .type = func->ret.type });
            func->async.Append({ .code = call });
            func->async.Append({ .code = AbiOpcode::ReturnFloat64, .type = func->ret.type });
        } break;

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

    return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

namespace {
#if defined(MUST_TAIL)
    #define OP(Code) \
            PRESERVE_NONE Napi::Value Handle ## Code(CallData *call, napi_value *args, uint8_t *base, const AbiInstruction *inst)
    #define NEXT() \
            do { \
                const AbiInstruction *next = inst + 1; \
                MUST_TAIL return ForwardDispatch[(int)next->code](call, args, base, next); \
            } while (false)

    PRESERVE_NONE typedef Napi::Value ForwardFunc(CallData *call, napi_value *args, uint8_t *base, const AbiInstruction *inst);

    extern ForwardFunc *const ForwardDispatch[256];
#else
    #warning Falling back to inlining instead of tail calls (missing attributes)

    #define OP(Code) \
        case AbiOpcode::Code:
    #define NEXT() \
        break

    Napi::Value RunLoop(CallData *call, napi_value *args, uint8_t *base, const AbiInstruction *inst)
    {
        for (;; ++inst) {
            switch (inst->code) {
#endif

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
            *(uint64_t *)(base + inst->b1) = (uint64_t)v; \
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
            *(uint64_t *)(base + inst->b1) = (uint64_t)ReverseBytes(v); \
        } while (false)

    OP(PushVoid) { K_UNREACHABLE(); return call->env.Null(); }
    OP(PushBool) {
        Napi::Value value(call->env, args[inst->a]);

        bool b;
        if (napi_get_value_bool(call->env, value, &b) != napi_ok) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected boolean", GetValueType(call->instance, value));
            return call->env.Null();
        }

        *(uint64_t *)(base + inst->b1) = (uint64_t)b;

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
    OP(PushUInt64) { INTEGER(int64_t); NEXT(); }
    OP(PushUInt64S) { INTEGER_SWAP(int64_t); NEXT(); }
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

        Napi::Object obj = value.As<Napi::Object>();

        uint64_t buf[2] = {};
        if (!call->PushObject(obj, inst->type, (uint8_t *)buf))
            return call->env.Null();

        // The second part might be useless (if object fits in one register), in
        // which case the analysis code will put the same value in both offsets to
        // make sure we don't overwrite something else. Well, if we copy the second
        // part first, that is, as we do below.
        *(uint64_t *)(base + inst->b2) = buf[1];
        *(uint64_t *)(base + inst->b1) = buf[0];

        NEXT();
    }
    OP(PushAggregateStack) {
        Napi::Value value(call->env, args[inst->a]);

        if (!IsObject(value)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected object", GetValueType(call->instance, value));
            return call->env.Null();
        }

        Napi::Object obj = value.As<Napi::Object>();

        if (!call->PushObject(obj, inst->type, base + inst->b1))
            return call->env.Null();

        NEXT();
    }

#undef INTEGER_SWAP
#undef INTEGER

#define INTEGER(Suffix, CType) \
        do { \
            uint64_t rax = ForwardCall ## Suffix(call->native, base, &call->saved_sp).rax; \
            call->PopOutArguments(); \
            return NewInt(call->env, (CType)rax); \
        } while (false)
#define INTEGER_SWAP(Suffix, CType) \
        do { \
            uint64_t rax = ForwardCall ## Suffix(call->native, base, &call->saved_sp).rax; \
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
        ForwardCallGG(call->native, base, &call->saved_sp);
        call->PopOutArguments();
        return call->env.Undefined();
    }
    OP(RunBool) {
        uint64_t rax = ForwardCallGG(call->native, base, &call->saved_sp).rax;
        call->PopOutArguments();
        return Napi::Boolean::New(call->env, rax & 0x1);
    }
    OP(RunInt8) { INTEGER(GG, int8_t); }
    OP(RunUInt8) { INTEGER(GG, uint8_t); }
    OP(RunInt16) { INTEGER(GG, int16_t); }
    OP(RunInt16S) { INTEGER_SWAP(GG, int16_t); }
    OP(RunUInt16) { INTEGER(GG, uint16_t); }
    OP(RunUInt16S) { INTEGER_SWAP(GG, uint16_t); }
    OP(RunInt32) { INTEGER(GG, int32_t); }
    OP(RunInt32S) { INTEGER_SWAP(GG, int32_t); }
    OP(RunUInt32) { INTEGER(GG, uint32_t); }
    OP(RunUInt32S) { INTEGER_SWAP(GG, uint32_t); }
    OP(RunInt64) { INTEGER(GG, int64_t); }
    OP(RunInt64S) { INTEGER_SWAP(GG, int64_t); }
    OP(RunUInt64) { INTEGER(GG, uint64_t); }
    OP(RunUInt64S) { INTEGER_SWAP(GG, uint64_t); }
    OP(RunString) {
        uint64_t rax = ForwardCallGG(call->native, base, &call->saved_sp).rax;
        Napi::Value value = rax ? Napi::String::New(call->env, (const char *)rax) : call->env.Null();
        DISPOSE((void *)rax);
        call->PopOutArguments();
        return value;
    }
    OP(RunString16) {
        uint64_t rax = ForwardCallGG(call->native, base, &call->saved_sp).rax;
        Napi::Value value = rax ? Napi::String::New(call->env, (const char16_t *)rax) : call->env.Null();
        DISPOSE((void *)rax);
        call->PopOutArguments();
        return value;
    }
    OP(RunString32) {
        uint64_t rax = ForwardCallGG(call->native, base, &call->saved_sp).rax;
        Napi::Value value = rax ? MakeStringFromUTF32(call->env, (const char32_t *)rax) : call->env.Null();
        DISPOSE((void *)rax);
        call->PopOutArguments();
        return value;
    }
    OP(RunPointer) {
        uint64_t rax = ForwardCallGG(call->native, base, &call->saved_sp).rax;
        Napi::Value value = rax ? WrapPointer(call->env, inst->type, (void *)rax) : call->env.Null();
        DISPOSE((void *)rax);
        call->PopOutArguments();
        return value;
    }
    OP(RunCallback) {
        uint64_t rax = ForwardCallGG(call->native, base, &call->saved_sp).rax;
        call->PopOutArguments();
        return rax ? WrapCallback(call->env, inst->type, (void *)rax) : call->env.Null();
    }
    OP(RunRecord) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunUnion) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunArray) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunFloat32) {
        float f = ForwardCallF(call->native, base, &call->saved_sp);
        call->PopOutArguments();
        return Napi::Number::New(call->env, (double)f);
    }
    OP(RunFloat64) {
        double d = ForwardCallDD(call->native, base, &call->saved_sp).xmm0;
        call->PopOutArguments();
        return Napi::Number::New(call->env, d);
    }
    OP(RunPrototype) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunAggregateStack) {
        *(uint8_t **)base = base + inst->b;
        uint64_t rax = ForwardCallGG(call->native, base, &call->saved_sp).rax;
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)rax, inst->type);
    }
    OP(RunAggregateGG) {
        auto ret = ForwardCallGG(call->native, base, &call->saved_sp);
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateDD) {
        auto ret = ForwardCallDD(call->native, base, &call->saved_sp);
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateGD) {
        auto ret = ForwardCallGD(call->native, base, &call->saved_sp);
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateDG) {
        auto ret = ForwardCallDG(call->native, base, &call->saved_sp);
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)&ret, inst->type);
    }
    OP(RunVoidX) {
        ForwardCallXGG(call->native, base, &call->saved_sp);
        call->PopOutArguments();
        return call->env.Undefined();
    }
    OP(RunBoolX) {
        uint64_t rax = ForwardCallXGG(call->native, base, &call->saved_sp).rax;
        call->PopOutArguments();
        return Napi::Boolean::New(call->env, rax & 0x1);
    }
    OP(RunInt8X) { INTEGER(XGG, int8_t); }
    OP(RunUInt8X) { INTEGER(XGG, uint8_t); }
    OP(RunInt16X) { INTEGER(XGG, int16_t); }
    OP(RunInt16SX) { INTEGER_SWAP(XGG, int16_t); }
    OP(RunUInt16X) { INTEGER(XGG, uint16_t); }
    OP(RunUInt16SX) { INTEGER_SWAP(XGG, uint16_t); }
    OP(RunInt32X) { INTEGER(XGG, int32_t); }
    OP(RunInt32SX) { INTEGER_SWAP(XGG, int32_t); }
    OP(RunUInt32X) { INTEGER(XGG, uint32_t); }
    OP(RunUInt32SX) { INTEGER_SWAP(XGG, uint32_t); }
    OP(RunInt64X) { INTEGER(XGG, int64_t); }
    OP(RunInt64SX) { INTEGER_SWAP(XGG, int64_t); }
    OP(RunUInt64X) { INTEGER(XGG, uint64_t); }
    OP(RunUInt64SX) { INTEGER_SWAP(XGG, uint64_t); }
    OP(RunStringX) {
        uint64_t rax = ForwardCallXGG(call->native, base, &call->saved_sp).rax;
        Napi::Value value = rax ? Napi::String::New(call->env, (const char *)rax) : call->env.Null();
        DISPOSE((void *)rax);
        call->PopOutArguments();
        return value;
    }
    OP(RunString16X) {
        uint64_t rax = ForwardCallXGG(call->native, base, &call->saved_sp).rax;
        Napi::Value value = rax ? Napi::String::New(call->env, (const char16_t *)rax) : call->env.Null();
        DISPOSE((void *)rax);
        call->PopOutArguments();
        return value;
    }
    OP(RunString32X) {
        uint64_t rax = ForwardCallXGG(call->native, base, &call->saved_sp).rax;
        Napi::Value value = rax ? MakeStringFromUTF32(call->env, (const char32_t *)rax) : call->env.Null();
        DISPOSE((void *)rax);
        call->PopOutArguments();
        return value;
    }
    OP(RunPointerX) {
        uint64_t rax = ForwardCallXGG(call->native, base, &call->saved_sp).rax;
        Napi::Value value = rax ? WrapPointer(call->env, inst->type, (void *)rax) : call->env.Null();
        DISPOSE((void *)rax);
        call->PopOutArguments();
        return value;
    }
    OP(RunCallbackX) {
        uint64_t rax = ForwardCallXGG(call->native, base, &call->saved_sp).rax;
        call->PopOutArguments();
        return rax ? WrapCallback(call->env, inst->type, (void *)rax) : call->env.Null();
    }
    OP(RunRecordX) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunUnionX) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunArrayX) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunFloat32X) {
        float f = ForwardCallXF(call->native, base, &call->saved_sp);
        call->PopOutArguments();
        return Napi::Number::New(call->env, (double)f);
    }
    OP(RunFloat64X) {
        double d = ForwardCallXDD(call->native, base, &call->saved_sp).xmm0;
        call->PopOutArguments();
        return Napi::Number::New(call->env, d);
    }
    OP(RunPrototypeX) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunAggregateXStack) {
        *(uint8_t **)base = base + inst->b;
        uint64_t rax = ForwardCallXGG(call->native, base, &call->saved_sp).rax;
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)rax, inst->type);
    }
    OP(RunAggregateXGG) {
        auto ret = ForwardCallXGG(call->native, base, &call->saved_sp);
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateXDD) {
        auto ret = ForwardCallXDD(call->native, base, &call->saved_sp);
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateXGD) {
        auto ret = ForwardCallXGD(call->native, base, &call->saved_sp);
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateXDG) {
        auto ret = ForwardCallXDG(call->native, base, &call->saved_sp);
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)&ret, inst->type);
    }

#undef DISPOSE
#undef INTEGER_SWAP
#undef INTEGER

#define CALL(Suffix) \
        do { \
            auto ret = ForwardCall ## Suffix(call->native, base, &call->saved_sp); \
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

    OP(CallGG) { CALL(GG); return call->env.Null(); }
    OP(CallF) { CALL(F); return call->env.Null(); }
    OP(CallDG) { CALL(DG); return call->env.Null(); }
    OP(CallGD) { CALL(GD); return call->env.Null(); }
    OP(CallDD) { CALL(DD); return call->env.Null(); }
    OP(CallStack) {
        *(uint8_t **)base = base + inst->b;
        CALL(GG);
        return call->env.Null();
    }
    OP(CallXGG) { CALL(XGG); return call->env.Null(); }
    OP(CallXF) { CALL(XF); return call->env.Null(); }
    OP(CallXDG) { CALL(XDG); return call->env.Null(); }
    OP(CallXGD) { CALL(XGD); return call->env.Null(); }
    OP(CallXDD) { CALL(XDD); return call->env.Null(); }
    OP(CallXStack) {
        *(uint8_t **)base = base + inst->b;
        CALL(XGG);
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
        HandleRunAggregateStack,
        HandleRunAggregateGG,
        HandleRunAggregateDD,
        HandleRunAggregateGD,
        HandleRunAggregateDG,
        #define PRIMITIVE(Name) HandleRun ## Name ## X,
        #include "../primitives.inc"
        HandleRunAggregateXStack,
        HandleRunAggregateXGG,
        HandleRunAggregateXDD,
        HandleRunAggregateXGD,
        HandleRunAggregateXDG,
        HandleYield,
        HandleCallGG,
        HandleCallF,
        HandleCallDG,
        HandleCallGD,
        HandleCallDD,
        HandleCallStack,
        HandleCallXGG,
        HandleCallXF,
        HandleCallXDG,
        HandleCallXGD,
        HandleCallXDD,
        HandleCallXStack,
        #define PRIMITIVE(Name) HandleReturn ## Name,
        #include "../primitives.inc"
        HandleReturnAggregate
    };

    FORCE_INLINE Napi::Value RunLoop(CallData *call, napi_value *args, uint8_t *base, const AbiInstruction *inst)
    {
        return ForwardDispatch[(int)inst->code](call, args, base, inst);
    }
#else
            }
        }

        K_UNREACHABLE();
    }
#endif

#undef NEXT
#undef OP
}

#pragma GCC diagnostic pop

Napi::Value CallData::Run(const Napi::CallbackInfo &info)
{
    uint8_t *base = AllocStack<uint8_t>(func->stk_size);
    if (!base) [[unlikely]]
        return env.Null();

    // Follow sync bytecode
    const AbiInstruction *first = func->sync.ptr;

    return RunLoop(this, info.First(), base, first);
}

bool CallData::PrepareAsync(const Napi::CallbackInfo &info)
{
    uint8_t *base = AllocStack<uint8_t>(func->stk_size);
    if (!base) [[unlikely]]
        return env.Null();
    async_base = base;

    // Follow async bytecode
    const AbiInstruction *first = func->async.ptr;

    return RunLoop(this, info.First(), base, first);
}

void CallData::ExecuteAsync()
{
    const AbiInstruction *next = async_ip++;
    RunLoop(this, nullptr, async_base, next);
}

Napi::Value CallData::EndAsync()
{
    const AbiInstruction *next = async_ip++;
    return RunLoop(this, nullptr, async_base, next);
}

void CallData::Relay(Size idx, uint8_t *own_sp, uint8_t *caller_sp, bool switch_stack, BackRegisters *out_reg)
{
    if (env.IsExceptionPending()) [[unlikely]]
        return;

    // Account for the fact that the stack offsets are optimized for the forward call code,
    // and they start after the GPR (6 registers) and the XMM (8 registers).
    caller_sp -= 14 * 8;

    const TrampolineInfo &trampoline = shared.trampolines[idx];

    const FunctionInfo *proto = trampoline.proto;
    Napi::Function func = trampoline.func.Value();

    K_DEFER_N(err_guard) { memset(out_reg, 0, K_SIZE(*out_reg)); };

    if (trampoline.generation >= 0 && trampoline.generation != (int32_t)mem->generation) [[unlikely]] {
        ThrowError<Napi::Error>(env, "Cannot use non-registered callback beyond FFI call");
        return;
    }

    LocalArray<napi_value, MaxParameters + 1> arguments;

    arguments.Append(!trampoline.recv.IsEmpty() ? trampoline.recv.Value() : env.Undefined());

#define POP_INTEGER(CType) \
        do { \
            const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offsets[0]; \
            CType v = *(const CType *)src; \
             \
            Napi::Value arg = NewInt(env, v); \
            arguments.Append(arg); \
        } while (false)
#define POP_INTEGER_SWAP(CType) \
        do { \
            const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offsets[0]; \
            CType v = *(const CType *)src; \
             \
            Napi::Value arg = NewInt(env, ReverseBytes(v)); \
            arguments.Append(arg); \
        } while (false)

    // Convert to JS arguments
    for (Size i = 0; i < proto->parameters.len; i++) {
        const ParameterInfo &param = proto->parameters[i];
        K_ASSERT(param.directions >= 1 && param.directions <= 3);

        switch (param.type->primitive) {
            case PrimitiveKind::Void: { K_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offsets[0];
                bool b = *(bool *)src;

                Napi::Value arg = Napi::Boolean::New(env, b);
                arguments.Append(arg);
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
                const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offsets[0];
                const char *str = *(const char **)src;

                Napi::Value arg = str ? Napi::String::New(env, str) : env.Null();
                arguments.Append(arg);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, str);
                }
            } break;
            case PrimitiveKind::String16: {
                const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offsets[0];
                const char16_t *str16 = *(const char16_t **)src;

                Napi::Value arg = str16 ? Napi::String::New(env, str16) : env.Null();
                arguments.Append(arg);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, str16);
                }
            } break;
            case PrimitiveKind::String32: {
                const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offsets[0];
                const char32_t *str32 = *(const char32_t **)src;

                Napi::Value arg = str32 ? MakeStringFromUTF32(env, str32) : env.Null();
                arguments.Append(arg);
            } break;

            case PrimitiveKind::Pointer: {
                const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offsets[0];
                void *ptr2 = *(void **)src;

                Napi::Value p = ptr2 ? WrapPointer(env, param.type->ref.type, ptr2) : env.Null();
                arguments.Append(p);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, ptr2);
                }
            } break;

            case PrimitiveKind::Record:
            case PrimitiveKind::Union: {
                if (param.abi.regular) {
                    uint64_t buf[2];

                    buf[0] = *(uint64_t *)(own_sp + param.abi.offsets[0]);
                    buf[1] = *(uint64_t *)(own_sp + param.abi.offsets[1]);

                    Napi::Object obj = DecodeObject(env, (const uint8_t *)buf, param.type);
                    arguments.Append(obj);
                } else {
                    Napi::Object obj = DecodeObject(env, caller_sp + param.abi.offsets[0], param.type);
                    arguments.Append(obj);
                }
            } break;
            case PrimitiveKind::Array: { K_UNREACHABLE(); } break;

            case PrimitiveKind::Float32: {
                const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offsets[0];
                float f = *(float *)src;

                Napi::Value arg = Napi::Number::New(env, (double)f);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Float64: {
                const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offsets[0];
                double d = *(double *)src;

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;

            case PrimitiveKind::Callback: {
                const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offsets[0];
                void *ptr2 = *(void **)src;

                Napi::Value p = ptr2 ? WrapCallback(env, param.type->ref.type, ptr2) : env.Null();
                arguments.Append(p);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, ptr2);
                }
            } break;

            case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
        }
    }

#undef POP_INTEGER_SWAP
#undef POP_INTEGER

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
            if (!IsObject(value)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected object", GetValueType(instance, value));
                return;
            }

            Napi::Object obj = value.As<Napi::Object>();

            if (proto->ret.abi.method == AbiMethod::Stack) {
                uint64_t *gpr_ptr = (uint64_t *)own_sp;
                uint8_t *dest = (uint8_t *)gpr_ptr[0];

                if (!PushObject(obj, type, dest))
                    return;

                out_reg->rax = (uint64_t)dest;
            } else {
                K_ASSERT(type->size <= 16);

                uint8_t buf[16] = {};
                if (!PushObject(obj, type, buf))
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
