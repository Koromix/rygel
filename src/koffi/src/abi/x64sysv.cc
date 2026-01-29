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
                                      uint8_t *old_sp, Span<uint8_t> *new_stack,
                                      napi_value (*call)(Napi::Function *func, size_t argc, napi_value *argv));
enum class AbiOpcode : int8_t {
    #define PRIMITIVE(Name) Name,
    #include "../primitives.inc"
    AggregateReg,
    AggregateStack,
    End
};

enum class AbiMethod {
    Stack,
    GprGpr,
    XmmXmm,
    GprXmm,
    XmmGpr
};

struct ClassResult {
    AbiMethod method;

    int gpr_index;
    int gpr_count;
    int xmm_index;
    int xmm_count;
    int stack_offset;
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

    int stack_offset = 0;

public:
    ClassAnalyser(int gprs, int xmms)
        : gpr_max(gprs), xmm_max(xmms), gpr_avail(gprs), xmm_avail(xmms) {}

    ClassResult Analyse(const TypeInfo *type);

    Size Classify(Span<RegisterClass> classes, const TypeInfo *type, Size offset);
    RegisterClass MergeClasses(RegisterClass cls1, RegisterClass cls2);

    int GprCount() const { return gpr_max - gpr_avail; }
    int XmmCount() const { return xmm_max - xmm_avail; }
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
            K_ASSERT(cls != RegisterClass::NoClass);

            if (cls == RegisterClass::Memory) {
                ret.method = AbiMethod::Stack;
                return ret;
            }

            gpr_count += (cls == RegisterClass::Integer);
            xmm_count += (cls == RegisterClass::SSE);
        }

        if (gpr_count <= gpr_avail && xmm_count <= xmm_avail) {
            if (gpr_count && xmm_count) {
                bool gpr_xmm = (classes.len && classes[0] == RegisterClass::Integer);
                ret.method = gpr_xmm ? AbiMethod::GprXmm : AbiMethod::XmmGpr;
            } else if (gpr_count) {
                ret.method = AbiMethod::GprGpr;
            } else {
                ret.method = AbiMethod::XmmXmm;
            }

            ret.gpr_index = (gpr_max - gpr_avail);
            ret.gpr_count = gpr_count;
            ret.xmm_index = (xmm_max - xmm_avail);
            ret.xmm_count = xmm_count;

            gpr_avail -= gpr_count;
            xmm_avail -= xmm_count;

            return ret;
        }
    }

    // Fall back to the stack
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

                case AbiMethod::GprGpr: {
                    param.abi.regular = true;
                    param.abi.offsets[0] = (gpr_base + ret.gpr_index) * 8;
                    param.abi.offsets[1] = param.abi.offsets[0] + (ret.gpr_count == 2) * 8;
                } break;
                case AbiMethod::XmmXmm: {
                    param.abi.regular = true;
                    param.abi.offsets[0] = (6 + ret.xmm_index) * 8;
                    param.abi.offsets[1] = param.abi.offsets[0] + (ret.xmm_count == 2) * 8;
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
                AbiOpcode code = param.abi.regular ? AbiOpcode::AggregateReg : AbiOpcode::AggregateStack;
                func->instructions.Append(code);
            } else {
                func->instructions.Append((AbiOpcode)param.type->primitive);
            }

            func->args_size += AlignLen(param.type->size, 16);
        }

        func->forward_fp = analyser.XmmCount();
    }

    func->instructions.Append(AbiOpcode::End);

    return true;
}

namespace {
#if  __has_attribute(musttail) && __has_attribute(preserve_none)
    #define TAIL

    #define OP(Code) \
            __attribute__((preserve_none)) bool Handle ## Code(CallData *call, const FunctionInfo *func, const Napi::CallbackInfo &info, uint8_t *base, Size i)
    #define DISPATCH() \
            do { \
                AbiOpcode next = func->instructions[i + 1]; \
                [[clang::musttail]] return ForwardDispatch[(int)next](call, func, info, base, i + 1); \
            } while (false)

    __attribute__((preserve_none)) typedef bool ForwardFunc(CallData *call, const FunctionInfo *func, const Napi::CallbackInfo &info, uint8_t *base, Size i);
#else
    #warning Falling back to inlining instead of tail calls (missing attributes)

    #define OP(Code) \
        inline __attribute__((always_inline)) bool Handle ## Code(CallData *call, const FunctionInfo *func, const Napi::CallbackInfo &info, uint8_t *base, Size i)
    #define DISPATCH() \
        return true

    typedef bool ForwardFunc(CallData *call, const FunctionInfo *func, const Napi::CallbackInfo &info, uint8_t *base, Size i);
#endif

#define PUSH_INTEGER(CType) \
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
            *(uint64_t *)(base + param.abi.offsets[0]) = (uint64_t)v; \
        } while (false)
#define PUSH_INTEGER_SWAP(CType) \
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
            *(uint64_t *)(base + param.abi.offsets[0]) = (uint64_t)ReverseBytes(v); \
        } while (false)

    extern ForwardFunc *const ForwardDispatch[256];

    OP(Bool) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        bool b;
        if (napi_get_value_bool(call->env, value, &b) != napi_ok) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected boolean", GetValueType(call->instance, value));
            return false;
        }

        *(uint64_t *)(base + param.abi.offsets[0]) = (uint64_t)b;

        DISPATCH();
    }

    OP(Void) { K_UNREACHABLE(); return false; }

    OP(Int8) { PUSH_INTEGER(int8_t); DISPATCH(); }
    OP(UInt8) { PUSH_INTEGER(uint8_t); DISPATCH(); }
    OP(Int16) { PUSH_INTEGER(int16_t); DISPATCH(); }
    OP(Int16S) { PUSH_INTEGER_SWAP(int16_t); DISPATCH(); }
    OP(UInt16) { PUSH_INTEGER(uint16_t); DISPATCH(); }
    OP(UInt16S) { PUSH_INTEGER_SWAP(uint16_t); DISPATCH(); }
    OP(Int32) { PUSH_INTEGER(int32_t); DISPATCH(); }
    OP(Int32S) { PUSH_INTEGER_SWAP(int32_t); DISPATCH(); }
    OP(UInt32) { PUSH_INTEGER(uint32_t); DISPATCH(); }
    OP(UInt32S) { PUSH_INTEGER_SWAP(uint32_t); DISPATCH(); }
    OP(Int64) { PUSH_INTEGER(int64_t); DISPATCH(); }
    OP(Int64S) { PUSH_INTEGER_SWAP(int64_t); DISPATCH(); }
    OP(UInt64) { PUSH_INTEGER(int64_t); DISPATCH(); }
    OP(UInt64S) { PUSH_INTEGER_SWAP(int64_t); DISPATCH(); }

    OP(String) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        const char *str;
        if (!call->PushString(value, param.directions, &str)) [[unlikely]]
            return false;

        *(const char **)(base + param.abi.offsets[0]) = str;

        DISPATCH();
    }

    OP(String16) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        const char16_t *str16;
        if (!call->PushString16(value, param.directions, &str16)) [[unlikely]]
            return false;

        *(const char16_t **)(base + param.abi.offsets[0]) = str16;

        DISPATCH();
    }

    OP(String32) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        const char32_t *str32;
        if (!call->PushString32(value, param.directions, &str32)) [[unlikely]]
            return false;

        *(const char32_t **)(base + param.abi.offsets[0]) = str32;

        DISPATCH();
    }

    OP(Pointer) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        void *ptr;
        if (!call->PushPointer(value, param.type, param.directions, &ptr)) [[unlikely]]
            return false;

        *(void **)(base + param.abi.offsets[0]) = ptr;

        DISPATCH();
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

        *(uint32_t *)(base + param.abi.offsets[0] + 4) = 0;
        *(float *)(base + param.abi.offsets[0]) = f;

        DISPATCH();
    }

    OP(Float64) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        double d;
        if (!TryNumber(value, &d)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected number", GetValueType(call->instance, value));
            return false;
        }

        *(double *)(base + param.abi.offsets[0]) = d;

        DISPATCH();
    }

    OP(Callback) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        void *ptr;
        if (!call->PushCallback(value, param.type, &ptr)) [[unlikely]]
            return false;

        *(void **)(base + param.abi.offsets[0]) = ptr;

        DISPATCH();
    }

    OP(Prototype) { K_UNREACHABLE(); return false; }

    OP(AggregateReg) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        if (!IsObject(value)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected object", GetValueType(call->instance, value));
            return false;
        }

        Napi::Object obj = value.As<Napi::Object>();

        uint64_t buf[2] = {};
        if (!call->PushObject(obj, param.type, (uint8_t *)buf))
            return false;

        // The second part might be useless (if object fits in one register), in
        // which case the analysis code will put the same value in both offsets to
        // make sure we don't overwrite something else. Well, if we copy the second
        // part first, that is, as we do below.
        *(uint64_t *)(base + param.abi.offsets[1]) = buf[1];
        *(uint64_t *)(base + param.abi.offsets[0]) = buf[0];

        DISPATCH();
    }

    OP(AggregateStack) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[param.offset];

        if (!IsObject(value)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected object", GetValueType(call->instance, value));
            return false;
        }

        Napi::Object obj = value.As<Napi::Object>();

        if (!call->PushObject(obj, param.type, base + param.abi.offsets[0]))
            return false;

        DISPATCH();
    }

    OP(End) { return true; }

    ForwardFunc *const ForwardDispatch[256] = {
        #define PRIMITIVE(Name) [(int)AbiOpcode::Name] = Handle ## Name,
        #include "../primitives.inc"
        [(int)AbiOpcode::AggregateReg] = HandleAggregateReg,
        [(int)AbiOpcode::AggregateStack] = HandleAggregateStack,
        [(int)AbiOpcode::End] = HandleEnd
    };

#undef PUSH_INTEGER_SWAP
#undef PUSH_INTEGER

#undef DISPATCH
#undef OP
}

bool CallData::Prepare(const FunctionInfo *func, const Napi::CallbackInfo &info)
{
    uint8_t *base = AllocStack<uint8_t>(14 * 8 + func->args_size);
    if (!base) [[unlikely]]
        return false;
    new_sp = base;

    if (func->ret.abi.method == AbiMethod::Stack) {
        return_ptr = AllocHeap(func->ret.type->size, 16);
        *(uint8_t **)base = return_ptr;
    } else {
        return_ptr = result.buf;
    }

#if defined(TAIL)
    AbiOpcode first = func->instructions[0];
    return ForwardDispatch[(int)first](this, func, info, base, 0);
#else
    for (Size i = 0;; i++) {
        AbiOpcode code = func->instructions[i];
        ForwardFunc *op = ForwardDispatch[(int)code];

        if (code == AbiOpcode::End)
            break;
        if (!op(this, func, info, base, i)) [[unlikely]]
            return false;
    }

    return true;
#endif
}

void CallData::Execute(const FunctionInfo *func, void *native)
{
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
        case PrimitiveKind::Callback: { result.u64 = PERFORM_CALL(GG).rax; } break;
        case PrimitiveKind::Record:
        case PrimitiveKind::Union: {
            switch (func->ret.abi.method) {
                case AbiMethod::Stack: { PERFORM_CALL(GG); } break;

                case AbiMethod::GprGpr: {
                    RaxRdxRet ret = PERFORM_CALL(GG);
                    memcpy(return_ptr, &ret, K_SIZE(ret));
                } break;
                case AbiMethod::XmmXmm: {
                    Xmm0Xmm1Ret ret = PERFORM_CALL(DD);
                    memcpy(return_ptr, &ret, K_SIZE(ret));
                } break;
                case AbiMethod::GprXmm: {
                    RaxXmm0Ret ret = PERFORM_CALL(GD);
                    memcpy(return_ptr, &ret, K_SIZE(ret));
                } break;
                case AbiMethod::XmmGpr: {
                    Xmm0RaxRet ret = PERFORM_CALL(DG);
                    memcpy(return_ptr, &ret, K_SIZE(ret));
                } break;
            }
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: { result.f = PERFORM_CALL(F); } break;
        case PrimitiveKind::Float64: { result.d = PERFORM_CALL(DG).xmm0; } break;

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

                    case AbiMethod::GprGpr: {
                        memcpy(&out_reg->rax, buf + 0, 8);
                        memcpy(&out_reg->rdx, buf + 8, 8);
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

            memset((uint8_t *)&out_reg->xmm0 + 4, 0, 4);
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
