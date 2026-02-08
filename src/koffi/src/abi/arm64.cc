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

extern "C" X0X1Ret ForwardCallGG(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" float ForwardCallF(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" HfaRet ForwardCallDDDD(const void *func, uint8_t *sp, uint8_t **out_saved_sp);

extern "C" X0X1Ret ForwardCallGGX(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" float ForwardCallFX(const void *func, uint8_t *sp, uint8_t **out_saved_sp);
extern "C" HfaRet ForwardCallDDDDX(const void *func, uint8_t *sp, uint8_t **out_saved_sp);

extern "C" napi_value CallSwitchStack(Napi::Function *func, size_t argc, napi_value *argv,
                                      uint8_t *saved_sp, Span<uint8_t> *new_stack,
                                      napi_value (*call)(Napi::Function *func, size_t argc, napi_value *argv));

enum class AbiOpcode : int16_t {
    #define PRIMITIVE(Name) Push ## Name,
    #include "../primitives.inc"
    PushAggregateReg,
    PushAggregateStack,
    PushHfa32,
    #define PRIMITIVE(Name) Run ## Name,
    #include "../primitives.inc"
    RunAggregateGG,
    RunAggregateDDDD,
    RunHfa32,
    RunAggregateStack,
    #define PRIMITIVE(Name) Run ## Name ## X,
    #include "../primitives.inc"
    RunAggregateGGX,
    RunAggregateDDDDX,
    RunHfa32X,
    RunAggregateStackX,
    Yield,
    CallGG,
    CallF,
    CallDDDD,
    CallStack,
    CallGGX,
    CallFX,
    CallDDDDX,
    CallStackX,
    #define PRIMITIVE(Name) Return ## Name,
    #include "../primitives.inc"
    ReturnAggregateReg,
    ReturnAggregateStack,
    ReturnHfa32,
#if defined(_M_ARM64EC)
    SetVariadicRegisters
#endif
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

bool AnalyseFunction(Napi::Env, InstanceData *, FunctionInfo *func)
{
    Size gpr_index = 0;
    Size vec_index = 0;
    Size stack_offset = 0;

#if defined(_M_ARM64EC)
    int gpr_max = func->variadic ? 4 : 8;
    int vec_max = 8;
#else
    int gpr_max = 8;
    int vec_max = 8;
#endif

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
                    param.abi.offset = 17 * 8 + stack_offset;
                    stack_offset += param.type->size;
#else
                    param.abi.offset = 17 * 8 + stack_offset;
                    stack_offset += 8;
#endif
                }

                int delta = (int)AbiOpcode::PushVoid - (int)PrimitiveKind::Void;
                AbiOpcode code = (AbiOpcode)((int)param.type->primitive + delta);

                func->sync.Append({ .code = code, .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)param.directions, .type = param.type });
                func->async.Append({ .code = code, .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)param.directions, .type = param.type });
            } break;

            case PrimitiveKind::Record:
            case PrimitiveKind::Union: {
#if defined(_M_ARM64EC)
                if (func->variadic) {
                    if (IsRegularSize(param.type->size, 8) && gpr_index < gpr_max) {
                        param.abi.regular = true;
                        param.abi.offset = gpr_index;
                        gpr_index++;

                        func->sync.Append({ .code = AbiOpcode::PushAggregateReg, .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                        func->async.Append({ .code = AbiOpcode::PushAggregateReg, .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                    } else {
                        if (gpr_index < gpr_max) {
                            param.abi.regular = true;
                            param.abi.offset = gpr_index * 8;
                            gpr_index++;
                        } else {
                            param.abi.offset = 17 * 8 + stack_offset;
                            stack_offset += 8;
                        }

                        param.abi.indirect = true;

                        func->sync.Append({ .code = AbiOpcode::PushAggregateStack, .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                        func->async.Append({ .code = AbiOpcode::PushAggregateStack, .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                    }

                    break;
                }
#endif

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
                            param.abi.offset = 17 * 8 + stack_offset;
                            stack_offset += registers * 8;
                        }

                        func->sync.Append({ .code = AbiOpcode::PushAggregateReg, .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                        func->async.Append({ .code = AbiOpcode::PushAggregateReg, .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                    } else {
                        if (gpr_index < gpr_max) {
                            param.abi.regular = true;
                            param.abi.offset = gpr_index * 8;
                            gpr_index++;
                        } else {
                            stack_offset = AlignLen(stack_offset, 8);
                            param.abi.offset = 17 * 8 + stack_offset;
                            stack_offset += 8;
                        }

                        param.abi.indirect = true;

                        func->sync.Append({ .code = AbiOpcode::PushAggregateStack, .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                        func->async.Append({ .code = AbiOpcode::PushAggregateStack, .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                    }

                    break;
                }
#endif

#if defined(_WIN32)
                // Windows ignores HFA optimization for variadic parameters
                HfaInfo hfa = !param.variadic ? IsHFA(param.type) : {};
#else
                HfaInfo hfa = IsHFA(param.type);
#endif

                if (hfa.count) {
                    if (hfa.count <= vec_max - vec_index) {
                        param.abi.regular = true;
                        param.abi.offset = 9 * 8 + vec_index * 8;
                        vec_index += hfa.count;

                        if (hfa.float32) {
                            param.abi.hfa32 = hfa.count;

                            func->sync.Append({ .code = AbiOpcode::PushHfa32, .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)hfa.count, .type = param.type });
                            func->async.Append({ .code = AbiOpcode::PushHfa32, .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)hfa.count, .type = param.type });
                        } else {
                            func->sync.Append({ .code = AbiOpcode::PushAggregateReg, .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                            func->async.Append({ .code = AbiOpcode::PushAggregateReg, .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                        }
                    } else {
                        vec_index = vec_max;

#if defined(__APPLE__)
                        stack_offset = AlignLen(stack_offset, param.type->align);
#endif
                        param.abi.offset = 17 * 8 + stack_offset;
                        stack_offset += 8;

                        func->sync.Append({ .code = AbiOpcode::PushAggregateReg, .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                        func->async.Append({ .code = AbiOpcode::PushAggregateReg, .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
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
                        param.abi.offset = 17 * 8 + stack_offset;
                        stack_offset += registers * 8;
                    }

                    func->sync.Append({ .code = AbiOpcode::PushAggregateReg, .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                    func->async.Append({ .code = AbiOpcode::PushAggregateReg, .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
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
                        param.abi.offset = 17 * 8 + stack_offset;
                        stack_offset += 8;
                    }

                    param.abi.indirect = true;

                    func->sync.Append({ .code = AbiOpcode::PushAggregateStack, .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
                    func->async.Append({ .code = AbiOpcode::PushAggregateStack, .a = param.offset, .b1 = (int16_t)param.abi.offset, .type = param.type });
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
                        param.abi.offset = 17 * 8 + stack_offset;
                        stack_offset += 8;
                    }

                    func->sync.Append({ .code = AbiOpcode::PushFloat32, .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)param.directions, .type = param.type });
                    func->async.Append({ .code = AbiOpcode::PushFloat32, .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)param.directions, .type = param.type });

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
                    param.abi.offset = 17 * 8 + stack_offset;
                    stack_offset += 4;
#else
                    param.abi.offset = 17 * 8 + stack_offset;
                    stack_offset += 8;
#endif
                }

                func->sync.Append({ .code = AbiOpcode::PushFloat32, .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)param.directions, .type = param.type });
                func->async.Append({ .code = AbiOpcode::PushFloat32, .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)param.directions, .type = param.type });
            } break;
            case PrimitiveKind::Float64: {
#if defined(_WIN32)
                if (param.variadic) {
                    if (gpr_index < gpr_max) {
                        param.abi.regular = true;
                        param.abi.offset = gpr_index * 8;
                        gpr_index++;
                    } else {
                        param.abi.offset = 17 * 8 + stack_offset;
                        stack_offset += 8;
                    }

                    func->sync.Append({ .code = AbiOpcode::PushFloat64, .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)param.directions, .type = param.type });
                    func->async.Append({ .code = AbiOpcode::PushFloat64, .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)param.directions, .type = param.type });

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
                    param.abi.offset = 17 * 8 + stack_offset;
                    stack_offset += 8;
                }

                func->sync.Append({ .code = AbiOpcode::PushFloat64, .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)param.directions, .type = param.type });
                func->async.Append({ .code = AbiOpcode::PushFloat64, .a = param.offset, .b1 = (int16_t)param.abi.offset, .b2 = (int16_t)param.directions, .type = param.type });
            } break;

            case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
        }
    }

    func->stk_size = AlignLen(17 * 8 + stack_offset, 16) + 8;
    func->forward_fp = vec_index;

    func->async.Append({ .code = AbiOpcode::Yield });

#if defined(_M_ARM64EC)
    if (func->variadic) {
        func->sync.Append({ .code = AbiOpcode::SetVariadicRegisters, .b = (int32_t)stack_offset });
        func->async.Append({ .code = AbiOpcode::SetVariadicRegisters, .b = (int32_t)stack_offset });
    }
#endif

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

                func->sync.Append({ .code = run, .type = func->ret.type });
            } else {
                int delta = (int)AbiOpcode::RunVoid - (int)PrimitiveKind::Void;
                AbiOpcode run = (AbiOpcode)((int)func->ret.type->primitive + delta);

                func->sync.Append({ .code = run, .type = func->ret.type });
            }

            // Async
            {
                int delta = (int)AbiOpcode::ReturnVoid - (int)PrimitiveKind::Void;
                AbiOpcode call = func->forward_fp ? AbiOpcode::CallGGX : AbiOpcode::CallGG;
                AbiOpcode ret = (AbiOpcode)((int)func->ret.type->primitive + delta);

                func->async.Append({ .code = call });
                func->async.Append({ .code = ret, .type = func->ret.type });
            }
        } break;

        case PrimitiveKind::Record:
        case PrimitiveKind::Union: {
            HfaInfo hfa = IsHFA(func->ret.type);

            if (hfa.count && hfa.float32) {
                func->ret.abi.regular = true;
                func->ret.abi.offset = offsetof(BackRegisters, d0);
                func->ret.abi.hfa32 = hfa.count;

                AbiOpcode run = func->forward_fp ? AbiOpcode::RunHfa32X : AbiOpcode::RunHfa32;
                AbiOpcode call = func->forward_fp ? AbiOpcode::CallDDDDX : AbiOpcode::CallDDDD;

                func->sync.Append({ .code = run, .b = hfa.count, .type = func->ret.type });
                func->async.Append({ .code = call });
                func->async.Append({ .code = AbiOpcode::ReturnHfa32, .b = hfa.count, .type = func->ret.type });
            } else if (hfa.count) {
                func->ret.abi.regular = true;
                func->ret.abi.offset = offsetof(BackRegisters, d0);

                AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateDDDDX : AbiOpcode::RunAggregateDDDD;
                AbiOpcode call = func->forward_fp ? AbiOpcode::CallDDDDX : AbiOpcode::CallDDDD;

                func->sync.Append({ .code = run, .b = hfa.count, .type = func->ret.type });
                func->async.Append({ .code = call });
                func->async.Append({ .code = AbiOpcode::ReturnAggregateReg, .b = hfa.count, .type = func->ret.type });
            } else if (func->ret.type->size <= 16) {
                func->ret.abi.regular = true;
                func->ret.abi.offset = offsetof(BackRegisters, x0);

                AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateGGX : AbiOpcode::RunAggregateGG;
                AbiOpcode call = func->forward_fp ? AbiOpcode::CallGGX : AbiOpcode::CallGG;

                func->sync.Append({ .code = run, .type = func->ret.type });
                func->async.Append({ .code = call });
                func->async.Append({ .code = AbiOpcode::ReturnAggregateReg, .type = func->ret.type });
            } else {
                AbiOpcode run = func->forward_fp ? AbiOpcode::RunAggregateStackX : AbiOpcode::RunAggregateStack;
                AbiOpcode call = func->forward_fp ? AbiOpcode::CallStackX : AbiOpcode::CallStack;

                func->sync.Append({ .code = run, .b = (int32_t)func->ret.type->size, .type = func->ret.type });
                func->async.Append({ .code = call, .b = (int32_t)func->ret.type->size });
                func->async.Append({ .code = AbiOpcode::ReturnAggregateStack, .type = func->ret.type });
            }
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;

        case PrimitiveKind::Float32: {
            AbiOpcode run = func->forward_fp ? AbiOpcode::RunFloat32X : AbiOpcode::RunFloat32;
            AbiOpcode call = func->forward_fp ? AbiOpcode::CallFX : AbiOpcode::CallF;

            func->sync.Append({ .code = run, .type = func->ret.type });
            func->async.Append({ .code = call });
            func->async.Append({ .code = AbiOpcode::ReturnFloat32, .type = func->ret.type });
        } break;
        case PrimitiveKind::Float64: {
            AbiOpcode run = func->forward_fp ? AbiOpcode::RunFloat64X : AbiOpcode::RunFloat64;
            AbiOpcode call = func->forward_fp ? AbiOpcode::CallDDDDX : AbiOpcode::CallDDDD;

            func->sync.Append({ .code = run, .type = func->ret.type });
            func->async.Append({ .code = call });
            func->async.Append({ .code = AbiOpcode::ReturnFloat64, .type = func->ret.type });
        } break;

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

    return true;
}

static inline void ExpandFloats(uint8_t *ptr, Size len)
{
    for (Size i = len - 1; i >= 0; i--) {
        const uint8_t *src = ptr + i * 4;
        uint8_t *dest = ptr + i * 8;

        memmove(dest, src, 4);
    }
}

static inline void CompactFloats(uint8_t *ptr, Size len)
{
    for (Size i = 0; i < len; i++) {
        const uint8_t *src = ptr + i * 8;
        uint8_t *dest = ptr + i * 4;

        memmove(dest, src, 4);
    }
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

#if defined(_WIN32)
    #define WRAP(Expr) \
        [&]() { \
            TEB *teb = GetTEB(); \
             \
            K_DEFER { call->instance->last_error = teb->LastErrorValue; }; \
            teb->LastErrorValue = call->instance->last_error; \
             \
            ADJUST_TEB(teb, call->mem->stack.ptr, call->mem->stack.end() + 128); \
             \
            return (Expr); \
        }()
#else
    #define WRAP(Expr) (Expr)
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

        uint8_t *ptr = base + inst->b1;

        Napi::Object obj = value.As<Napi::Object>();
        if (!call->PushObject(obj, inst->type, ptr))
            return call->env.Null();

        NEXT();
    }
    OP(PushAggregateStack) {
        Napi::Value value(call->env, args[inst->a]);

        if (!IsObject(value)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected object", GetValueType(call->instance, value));
            return call->env.Null();
        }

        uint8_t *ptr = call->AllocHeap(inst->type->size, 16);
        *(uint8_t **)(base + inst->b1) = ptr;

        Napi::Object obj = value.As<Napi::Object>();
        if (!call->PushObject(obj, inst->type, ptr))
            return call->env.Null();

        NEXT();
    }
    OP(PushHfa32) {
        Napi::Value value(call->env, args[inst->a]);

        if (!IsObject(value)) [[unlikely]] {
            ThrowError<Napi::TypeError>(call->env, "Unexpected %1 value, expected object", GetValueType(call->instance, value));
            return call->env.Null();
        }

        uint8_t *ptr = base + inst->b1;

        Napi::Object obj = value.As<Napi::Object>();
        if (!call->PushObject(obj, inst->type, ptr))
            return call->env.Null();

        ExpandFloats(ptr, inst->b2);

        NEXT();
    }

#undef INTEGER_SWAP
#undef INTEGER

#define INTEGER(Suffix, CType) \
        do { \
            uint64_t x0 = WRAP(ForwardCall ## Suffix(call->native, base, &call->saved_sp)).x0; \
            call->PopOutArguments(); \
            return NewInt(call->env, (CType)x0); \
        } while (false)
#define INTEGER_SWAP(Suffix, CType) \
        do { \
            uint64_t x0 = WRAP(ForwardCall ## Suffix(call->native, base, &call->saved_sp)).x0; \
            call->PopOutArguments(); \
            return NewInt(call->env, ReverseBytes((CType)x0)); \
        } while (false)
#define DISPOSE(Ptr) \
        do { \
            if (inst->type->dispose) { \
                inst->type->dispose(call->env, inst->type, (Ptr)); \
            } \
        } while (false)

    OP(RunVoid) {
        WRAP(ForwardCallGG(call->native, base, &call->saved_sp));
        call->PopOutArguments();
        return call->env.Undefined();
    }
    OP(RunBool) {
        uint64_t x0 = WRAP(ForwardCallGG(call->native, base, &call->saved_sp).x0);
        call->PopOutArguments();
        return Napi::Boolean::New(call->env, x0 & 0x1);
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
        uint64_t x0 = WRAP(ForwardCallGG(call->native, base, &call->saved_sp)).x0;
        Napi::Value value = x0 ? Napi::String::New(call->env, (const char *)x0) : call->env.Null();
        DISPOSE((void *)x0);
        call->PopOutArguments();
        return value;
    }
    OP(RunString16) {
        uint64_t x0 = WRAP(ForwardCallGG(call->native, base, &call->saved_sp)).x0;
        Napi::Value value = x0 ? Napi::String::New(call->env, (const char16_t *)x0) : call->env.Null();
        DISPOSE((void *)x0);
        call->PopOutArguments();
        return value;
    }
    OP(RunString32) {
        uint64_t x0 = WRAP(ForwardCallGG(call->native, base, &call->saved_sp)).x0;
        Napi::Value value = x0 ? MakeStringFromUTF32(call->env, (const char32_t *)x0) : call->env.Null();
        DISPOSE((void *)x0);
        call->PopOutArguments();
        return value;
    }
    OP(RunPointer) {
        uint64_t x0 = WRAP(ForwardCallGG(call->native, base, &call->saved_sp)).x0;
        Napi::Value value = x0 ? WrapPointer(call->env, inst->type, (void *)x0) : call->env.Null();
        DISPOSE((void *)x0);
        call->PopOutArguments();
        return value;
    }
    OP(RunCallback) {
        uint64_t x0 = WRAP(ForwardCallGG(call->native, base, &call->saved_sp)).x0;
        call->PopOutArguments();
        return x0 ? WrapCallback(call->env, inst->type, (void *)x0) : call->env.Null();
    }
    OP(RunRecord) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunUnion) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunArray) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunFloat32) {
        float f = WRAP(ForwardCallF(call->native, base, &call->saved_sp));
        call->PopOutArguments();
        return Napi::Number::New(call->env, (double)f);
    }
    OP(RunFloat64) {
        double d = WRAP(ForwardCallDDDD(call->native, base, &call->saved_sp)).d0;
        call->PopOutArguments();
        return Napi::Number::New(call->env, d);
    }
    OP(RunPrototype) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunAggregateGG) {
        auto ret = WRAP(ForwardCallGG(call->native, base, &call->saved_sp));
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateDDDD) {
        auto ret = WRAP(ForwardCallDDDD(call->native, base, &call->saved_sp));
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)&ret, inst->type);
    }
    OP(RunHfa32) {
        auto ret = WRAP(ForwardCallDDDD(call->native, base, &call->saved_sp));
        uint8_t *ptr = (uint8_t *)&ret;
        CompactFloats(ptr, inst->b);
        call->PopOutArguments();
        return DecodeObject(call->env, ptr, inst->type);
    }
    OP(RunAggregateStack) {
        uint8_t *ptr = call->AllocHeap(inst->b, 16);
        *(uint8_t **)(base + 8 * 8) = ptr; // x8
        WRAP(ForwardCallGG(call->native, base, &call->saved_sp));
        call->PopOutArguments();
        return DecodeObject(call->env, ptr, inst->type);
    }
    OP(RunVoidX) {
        WRAP(ForwardCallGGX(call->native, base, &call->saved_sp));
        call->PopOutArguments();
        return call->env.Undefined();
    }
    OP(RunBoolX) {
        uint64_t x0 = WRAP(ForwardCallGGX(call->native, base, &call->saved_sp)).x0;
        call->PopOutArguments();
        return Napi::Boolean::New(call->env, x0 & 0x1);
    }
    OP(RunInt8X) { INTEGER(GGX, int8_t); }
    OP(RunUInt8X) { INTEGER(GGX, uint8_t); }
    OP(RunInt16X) { INTEGER(GGX, int16_t); }
    OP(RunInt16SX) { INTEGER_SWAP(GGX, int16_t); }
    OP(RunUInt16X) { INTEGER(GGX, uint16_t); }
    OP(RunUInt16SX) { INTEGER_SWAP(GGX, uint16_t); }
    OP(RunInt32X) { INTEGER(GGX, int32_t); }
    OP(RunInt32SX) { INTEGER_SWAP(GGX, int32_t); }
    OP(RunUInt32X) { INTEGER(GGX, uint32_t); }
    OP(RunUInt32SX) { INTEGER_SWAP(GGX, uint32_t); }
    OP(RunInt64X) { INTEGER(GGX, int64_t); }
    OP(RunInt64SX) { INTEGER_SWAP(GGX, int64_t); }
    OP(RunUInt64X) { INTEGER(GGX, uint64_t); }
    OP(RunUInt64SX) { INTEGER_SWAP(GGX, uint64_t); }
    OP(RunStringX) {
        uint64_t x0 = WRAP(ForwardCallGGX(call->native, base, &call->saved_sp)).x0;
        Napi::Value value = x0 ? Napi::String::New(call->env, (const char *)x0) : call->env.Null();
        DISPOSE((void *)x0);
        call->PopOutArguments();
        return value;
    }
    OP(RunString16X) {
        uint64_t x0 = WRAP(ForwardCallGGX(call->native, base, &call->saved_sp)).x0;
        Napi::Value value = x0 ? Napi::String::New(call->env, (const char16_t *)x0) : call->env.Null();
        DISPOSE((void *)x0);
        call->PopOutArguments();
        return value;
    }
    OP(RunString32X) {
        uint64_t x0 = WRAP(ForwardCallGGX(call->native, base, &call->saved_sp)).x0;
        Napi::Value value = x0 ? MakeStringFromUTF32(call->env, (const char32_t *)x0) : call->env.Null();
        DISPOSE((void *)x0);
        call->PopOutArguments();
        return value;
    }
    OP(RunPointerX) {
        uint64_t x0 = WRAP(ForwardCallGGX(call->native, base, &call->saved_sp)).x0;
        Napi::Value value = x0 ? WrapPointer(call->env, inst->type, (void *)x0) : call->env.Null();
        DISPOSE((void *)x0);
        call->PopOutArguments();
        return value;
    }
    OP(RunCallbackX) {
        uint64_t x0 = WRAP(ForwardCallGGX(call->native, base, &call->saved_sp)).x0;
        call->PopOutArguments();
        return x0 ? WrapCallback(call->env, inst->type, (void *)x0) : call->env.Null();
    }
    OP(RunRecordX) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunUnionX) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunArrayX) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunFloat32X) {
        float f = WRAP(ForwardCallFX(call->native, base, &call->saved_sp));
        call->PopOutArguments();
        return Napi::Number::New(call->env, (double)f);
    }
    OP(RunFloat64X) {
        double d = WRAP(ForwardCallDDDDX(call->native, base, &call->saved_sp)).d0;
        call->PopOutArguments();
        return Napi::Number::New(call->env, d);
    }
    OP(RunPrototypeX) { K_UNREACHABLE(); return call->env.Null(); }
    OP(RunAggregateGGX) {
        auto ret = WRAP(ForwardCallGGX(call->native, base, &call->saved_sp));
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)&ret, inst->type);
    }
    OP(RunAggregateDDDDX) {
        auto ret = WRAP(ForwardCallDDDDX(call->native, base, &call->saved_sp));
        call->PopOutArguments();
        return DecodeObject(call->env, (const uint8_t *)&ret, inst->type);
    }
    OP(RunHfa32X) {
        auto ret = WRAP(ForwardCallDDDDX(call->native, base, &call->saved_sp));
        uint8_t *ptr = (uint8_t *)&ret;
        CompactFloats(ptr, inst->b);
        call->PopOutArguments();
        return DecodeObject(call->env, ptr, inst->type);
    }
    OP(RunAggregateStackX) {
        uint8_t *ptr = call->AllocHeap(inst->b, 16);
        *(uint8_t **)(base + 8 * 8) = ptr; // x8
        WRAP(ForwardCallGGX(call->native, base, &call->saved_sp));
        call->PopOutArguments();
        return DecodeObject(call->env, ptr, inst->type);
    }

#undef DISPOSE
#undef INTEGER_SWAP
#undef INTEGER

#define CALL(Suffix) \
        do { \
            auto ret = WRAP(ForwardCall ## Suffix(call->native, base, &call->saved_sp)); \
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
            uint64_t x0 = *(uint64_t *)base; \
            call->PopOutArguments(); \
            return NewInt(call->env, (CType)x0); \
        } while (false)
#define INTEGER_SWAP(CType) \
        do { \
            uint64_t x0 = *(uint64_t *)base; \
            call->PopOutArguments(); \
            return NewInt(call->env, ReverseBytes((CType)x0)); \
        } while (false)

    OP(Yield) {
        call->async_ip = inst + 1;
        return call->env.Null();
    }

    OP(CallGG) {
        *(uint8_t **)(base + 8 * 8) = base; // x8
        CALL(GG);
        return call->env.Null();
    }
    OP(CallF) { CALL(F); return call->env.Null(); }
    OP(CallDDDD) { CALL(DDDD); return call->env.Null(); }
    OP(CallStack) {
        uint8_t *ptr = call->AllocHeap(inst->b, 16);
        *(uint8_t **)(base + 8 * 8) = ptr; // x8
        CALL(GG);
        *(uint8_t **)base = ptr; // Store pointer for ReturnAggregateStack
        return call->env.Null();
    }
    OP(CallGGX) {
        *(uint8_t **)(base + 8 * 8) = base; // x8
        CALL(GGX);
        return call->env.Null();
    }
    OP(CallFX) { CALL(FX); return call->env.Null(); }
    OP(CallDDDDX) { CALL(DDDDX); return call->env.Null(); }
    OP(CallStackX) {
        uint8_t *ptr = call->AllocHeap(inst->b, 16);
        *(uint8_t **)(base + 8 * 8) = ptr; // x8
        CALL(GGX);
        *(uint8_t **)base = ptr; // Store pointer for ReturnAggregateStack
        return call->env.Null();
    }

    OP(ReturnVoid) {
        call->PopOutArguments();
        return call->env.Undefined();
    }
    OP(ReturnBool) {
        uint64_t x0 = *(uint64_t *)base;
        call->PopOutArguments();
        return Napi::Boolean::New(call->env, x0 & 0x1);
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
        uint64_t x0 = *(uint64_t *)base;
        call->PopOutArguments();
        Napi::Value value = x0 ? Napi::String::New(call->env, (const char *)x0) : call->env.Null();
        DISPOSE();
        return value;
    }
    OP(ReturnString16) {
        uint64_t x0 = *(uint64_t *)base;
        call->PopOutArguments();
        Napi::Value value = x0 ? Napi::String::New(call->env, (const char16_t *)x0) : call->env.Null();
        DISPOSE();
        return value;
    }
    OP(ReturnString32) {
        uint64_t x0 = *(uint64_t *)base;
        call->PopOutArguments();
        Napi::Value value = x0 ? MakeStringFromUTF32(call->env, (const char32_t *)x0) : call->env.Null();
        DISPOSE();
        return value;
    }
    OP(ReturnPointer) {
        uint64_t x0 = *(uint64_t *)base;
        call->PopOutArguments();
        Napi::Value value = x0 ? WrapPointer(call->env, inst->type, (void *)x0) : call->env.Null();
        DISPOSE();
        return value;
    }
    OP(ReturnCallback) {
        uint64_t x0 = *(uint64_t *)base;
        call->PopOutArguments();
        return x0 ? WrapCallback(call->env, inst->type, (void *)x0) : call->env.Null();
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
    OP(ReturnAggregateReg) {
        call->PopOutArguments();
        return DecodeObject(call->env, base, inst->type);
    }
    OP(ReturnAggregateStack) {
        const uint8_t *ptr = *(const uint8_t **)base;
        call->PopOutArguments();
        return DecodeObject(call->env, ptr, inst->type);
    }
    OP(ReturnHfa32) {
        CompactFloats(base, inst->b);
        call->PopOutArguments();
        return DecodeObject(call->env, base, inst->type);
    }

#if defined(_M_ARM64EC)
    OP(SetVariadicRegisters) {
        uint64_t *gpr_ptr = (uint64_t *)base;

        gpr_ptr[4] = (uint64_t)base + 17 * 8;
        gpr_ptr[5] = inst->b;

        NEXT();
    }
#endif

#undef INTEGER_SWAP
#undef INTEGER
#undef DISPOSE
#undef CALL

#if defined(MUST_TAIL)
    ForwardFunc *const ForwardDispatch[256] = {
        #define PRIMITIVE(Name) HandlePush ## Name,
        #include "../primitives.inc"
        HandlePushAggregateReg,
        HandlePushAggregateStack,
        HandlePushHfa32,
        #define PRIMITIVE(Name) HandleRun ## Name,
        #include "../primitives.inc"
        HandleRunAggregateGG,
        HandleRunAggregateDDDD,
        HandleRunHfa32,
        HandleRunAggregateStack,
        #define PRIMITIVE(Name) HandleRun ## Name ## X,
        #include "../primitives.inc"
        HandleRunAggregateGGX,
        HandleRunAggregateDDDDX,
        HandleRunHfa32X,
        HandleRunAggregateStackX,
        HandleYield,
        HandleCallGG,
        HandleCallF,
        HandleCallDDDD,
        HandleCallStack,
        HandleCallGGX,
        HandleCallFX,
        HandleCallDDDDX,
        HandleCallStackX,
        #define PRIMITIVE(Name) HandleReturn ## Name,
        #include "../primitives.inc"
        HandleReturnAggregateReg,
        HandleReturnAggregateStack,
        HandleReturnHfa32,
#if defined(_M_ARM64EC)
        HandleSetVariadicRegisters
#endif
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

#undef WRAP

#undef NEXT
#undef OP
}

#pragma GCC diagnostic pop

Napi::Value CallData::Run(const Napi::CallbackInfo &info)
{
    uint8_t *base = AllocStack<uint8_t>(func->stk_size);
    if (!base) [[unlikely]]
        return env.Null();

    const AbiInstruction *first = func->sync.ptr;
    return RunLoop(this, info.First(), base, first);
}

bool CallData::PrepareAsync(const Napi::CallbackInfo &info)
{
    uint8_t *base = AllocStack<uint8_t>(func->stk_size);
    if (!base) [[unlikely]]
        return env.Null();
    async_base = base;

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

    // Account for the fact that the stack offsets are optimized for the forward call code,
    // and they start after the GPR (6 registers) and the XMM (8 registers).
    caller_sp -= 17 * 8;

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
            const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offset; \
            CType v = *(const CType *)src; \
             \
            Napi::Value arg = NewInt(env, v); \
            arguments.Append(arg); \
        } while (false)
#define POP_INTEGER_SWAP(CType) \
        do { \
            const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offset; \
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
                const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offset;
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
                const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offset;
                const char *str = *(const char **)src;

                Napi::Value arg = str ? Napi::String::New(env, str) : env.Null();
                arguments.Append(arg);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, str);
                }
            } break;
            case PrimitiveKind::String16: {
                const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offset;
                const char16_t *str16 = *(const char16_t **)src;

                Napi::Value arg = str16 ? Napi::String::New(env, str16) : env.Null();
                arguments.Append(arg);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, str16);
                }
            } break;
            case PrimitiveKind::String32: {
                const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offset;
                const char32_t *str32 = *(const char32_t **)src;

                Napi::Value arg = str32 ? MakeStringFromUTF32(env, str32) : env.Null();
                arguments.Append(arg);
            } break;

            case PrimitiveKind::Pointer: {
                const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offset;
                void *ptr2 = *(void **)src;

                Napi::Value p = ptr2 ? WrapPointer(env, param.type->ref.type, ptr2) : env.Null();
                arguments.Append(p);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, ptr2);
                }
            } break;

            case PrimitiveKind::Record:
            case PrimitiveKind::Union: {
                uint8_t *ptr = (param.abi.regular ? own_sp : caller_sp) + param.abi.offset;
                uint8_t *src = param.abi.indirect ? *(uint8_t **)ptr : ptr;

                CompactFloats(src, param.abi.hfa32);

                Napi::Object obj = DecodeObject(env, src, param.type);
                arguments.Append(obj);
            } break;
            case PrimitiveKind::Array: { K_UNREACHABLE(); } break;

            case PrimitiveKind::Float32: {
                const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offset;
                float f = *(float *)src;

                Napi::Value arg = Napi::Number::New(env, (double)f);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Float64: {
                const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offset;
                double d = *(double *)src;

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;

            case PrimitiveKind::Callback: {
                const uint8_t *src = (param.abi.regular ? own_sp : caller_sp) + param.abi.offset;
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

            if (proto->ret.abi.regular) {
                uint8_t *dest = (uint8_t *)&out_reg + proto->ret.abi.offset;
                if (!PushObject(obj, type, dest))
                    return;
                ExpandFloats(dest, proto->ret.abi.hfa32);
            } else {
                uint64_t *gpr_ptr = (uint64_t *)own_sp;
                uint8_t *dest = (uint8_t *)gpr_ptr[8]; // r8

                if (!PushObject(obj, type, dest))
                    return;
            }
        } break;
        case PrimitiveKind::Array: { K_UNREACHABLE(); } break;

        case PrimitiveKind::Float32: {
            float f;
            if (!TryNumber(value, &f)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                return;
            }

            memset((uint8_t *)&out_reg->d0, 0, 8);
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
