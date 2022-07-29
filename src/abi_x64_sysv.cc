// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#if defined(__x86_64__) && !defined(_WIN32)

#include "vendor/libcc/libcc.hh"
#include "ffi.hh"
#include "call.hh"
#include "util.hh"

#include <napi.h>

namespace RG {

enum class RegisterClass {
    NoClass = 0, // Explicitly 0
    Integer,
    SSE,
    Memory
};

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

extern "C" int Trampoline0; extern "C" int TrampolineX0;
extern "C" int Trampoline1; extern "C" int TrampolineX1;
extern "C" int Trampoline2; extern "C" int TrampolineX2;
extern "C" int Trampoline3; extern "C" int TrampolineX3;
extern "C" int Trampoline4; extern "C" int TrampolineX4;
extern "C" int Trampoline5; extern "C" int TrampolineX5;
extern "C" int Trampoline6; extern "C" int TrampolineX6;
extern "C" int Trampoline7; extern "C" int TrampolineX7;
extern "C" int Trampoline8; extern "C" int TrampolineX8;
extern "C" int Trampoline9; extern "C" int TrampolineX9;
extern "C" int Trampoline10; extern "C" int TrampolineX10;
extern "C" int Trampoline11; extern "C" int TrampolineX11;
extern "C" int Trampoline12; extern "C" int TrampolineX12;
extern "C" int Trampoline13; extern "C" int TrampolineX13;
extern "C" int Trampoline14; extern "C" int TrampolineX14;
extern "C" int Trampoline15; extern "C" int TrampolineX15;
extern "C" int Trampoline16; extern "C" int TrampolineX16;
extern "C" int Trampoline17; extern "C" int TrampolineX17;
extern "C" int Trampoline18; extern "C" int TrampolineX18;
extern "C" int Trampoline19; extern "C" int TrampolineX19;
extern "C" int Trampoline20; extern "C" int TrampolineX20;
extern "C" int Trampoline21; extern "C" int TrampolineX21;
extern "C" int Trampoline22; extern "C" int TrampolineX22;
extern "C" int Trampoline23; extern "C" int TrampolineX23;
extern "C" int Trampoline24; extern "C" int TrampolineX24;
extern "C" int Trampoline25; extern "C" int TrampolineX25;
extern "C" int Trampoline26; extern "C" int TrampolineX26;
extern "C" int Trampoline27; extern "C" int TrampolineX27;
extern "C" int Trampoline28; extern "C" int TrampolineX28;
extern "C" int Trampoline29; extern "C" int TrampolineX29;
extern "C" int Trampoline30; extern "C" int TrampolineX30;
extern "C" int Trampoline31; extern "C" int TrampolineX31;

extern "C" napi_value CallSwitchStack(Napi::Function *func, size_t argc, napi_value *argv,
                                      uint8_t *old_sp, Span<uint8_t> *new_stack,
                                      napi_value (*call)(Napi::Function *func, size_t argc, napi_value *argv));

static void *const Trampolines[][2] = {
    { &Trampoline0, &TrampolineX0 },
    { &Trampoline1, &TrampolineX1 },
    { &Trampoline2, &TrampolineX2 },
    { &Trampoline3, &TrampolineX3 },
    { &Trampoline4, &TrampolineX4 },
    { &Trampoline5, &TrampolineX5 },
    { &Trampoline6, &TrampolineX6 },
    { &Trampoline7, &TrampolineX7 },
    { &Trampoline8, &TrampolineX8 },
    { &Trampoline9, &TrampolineX9 },
    { &Trampoline10, &TrampolineX10 },
    { &Trampoline11, &TrampolineX11 },
    { &Trampoline12, &TrampolineX12 },
    { &Trampoline13, &TrampolineX13 },
    { &Trampoline14, &TrampolineX14 },
    { &Trampoline15, &TrampolineX15 },
    { &Trampoline16, &TrampolineX16 },
    { &Trampoline17, &TrampolineX17 },
    { &Trampoline18, &TrampolineX18 },
    { &Trampoline19, &TrampolineX19 },
    { &Trampoline20, &TrampolineX20 },
    { &Trampoline21, &TrampolineX21 },
    { &Trampoline22, &TrampolineX22 },
    { &Trampoline23, &TrampolineX23 },
    { &Trampoline24, &TrampolineX24 },
    { &Trampoline25, &TrampolineX25 },
    { &Trampoline26, &TrampolineX26 },
    { &Trampoline27, &TrampolineX27 },
    { &Trampoline28, &TrampolineX28 },
    { &Trampoline29, &TrampolineX29 },
    { &Trampoline30, &TrampolineX30 },
    { &Trampoline31, &TrampolineX31 }
};
RG_STATIC_ASSERT(RG_LEN(Trampolines) == MaxTrampolines * 2);

static RG_THREAD_LOCAL CallData *exec_call;

static inline RegisterClass MergeClasses(RegisterClass cls1, RegisterClass cls2)
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

static Size ClassifyType(const TypeInfo *type, Size offset, Span<RegisterClass> classes)
{
    RG_ASSERT(classes.len > 0);

    switch (type->primitive) {
        case PrimitiveKind::Void: { return 0; } break;

        case PrimitiveKind::Bool:
        case PrimitiveKind::Int8:
        case PrimitiveKind::UInt8:
        case PrimitiveKind::Int16:
        case PrimitiveKind::UInt16:
        case PrimitiveKind::Int32:
        case PrimitiveKind::UInt32:
        case PrimitiveKind::Int64:
        case PrimitiveKind::UInt64:
        case PrimitiveKind::String:
        case PrimitiveKind::String16:
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
                Size start = offset / 8;
                ClassifyType(member.type, offset % 8, classes.Take(start, classes.len - start));
                offset += member.type->size;
            }

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
                ClassifyType(type->ref.type, offset % 8, classes.Take(start, classes.len - start));
                offset += type->ref.type->size;
            }

            return (offset + 7) / 8;
        } break;
        case PrimitiveKind::Float32:
        case PrimitiveKind::Float64: {
            classes[0] = MergeClasses(classes[0], RegisterClass::SSE);
            return 1;
        } break;

        case PrimitiveKind::Prototype: { RG_UNREACHABLE(); } break;
    }

    RG_UNREACHABLE();
}

static void AnalyseParameter(ParameterInfo *param, int gpr_avail, int xmm_avail)
{
    LocalArray<RegisterClass, 8> classes = {};
    classes.len = ClassifyType(param->type, 0, classes.data);

    if (!classes.len)
        return;
    if (classes.len > 2) {
        param->use_memory = true;
        return;
    }

    int gpr_count = 0;
    int xmm_count = 0;

    for (RegisterClass cls: classes) {
        RG_ASSERT(cls != RegisterClass::NoClass);

        if (cls == RegisterClass::Memory) {
            param->use_memory = true;
            return;
        }

        gpr_count += (cls == RegisterClass::Integer);
        xmm_count += (cls == RegisterClass::SSE);
    }

    if (gpr_count <= gpr_avail && xmm_count <= xmm_avail) {
        param->gpr_count = (int8_t)gpr_count;
        param->xmm_count = (int8_t)xmm_count;
        param->gpr_first = (classes[0] == RegisterClass::Integer);
    } else {
        param->use_memory = true;
    }
}

bool AnalyseFunction(Napi::Env, InstanceData *, FunctionInfo *func)
{
    AnalyseParameter(&func->ret, 2, 2);

    int gpr_avail = 6 - func->ret.use_memory;
    int xmm_avail = 8;

    for (ParameterInfo &param: func->parameters) {
        AnalyseParameter(&param, gpr_avail, xmm_avail);

        gpr_avail -= param.gpr_count;
        xmm_avail -= param.xmm_count;

        func->args_size += AlignLen(param.type->size, 16);
    }

    func->forward_fp = (xmm_avail < 8);

    return true;
}

bool CallData::Prepare(const Napi::CallbackInfo &info)
{
    uint64_t *args_ptr = nullptr;
    uint64_t *gpr_ptr = nullptr;
    uint64_t *xmm_ptr = nullptr;

    // Return through registers unless it's too big
    if (RG_UNLIKELY(!AllocStack(func->args_size, 16, &args_ptr)))
        return false;
    if (RG_UNLIKELY(!AllocStack(8 * 8, 8, &xmm_ptr)))
        return false;
    if (RG_UNLIKELY(!AllocStack(6 * 8, 8, &gpr_ptr)))
        return false;
    if (func->ret.use_memory) {
        return_ptr = AllocHeap(func->ret.type->size, 16);
        *(uint8_t **)(gpr_ptr++) = return_ptr;
    }

    // Push arguments
    for (Size i = 0; i < func->parameters.len; i++) {
        const ParameterInfo &param = func->parameters[i];
        RG_ASSERT(param.directions >= 1 && param.directions <= 3);

        Napi::Value value = info[param.offset];

        switch (param.type->primitive) {
            case PrimitiveKind::Void: { RG_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                if (RG_UNLIKELY(!value.IsBoolean())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argmument %2, expected boolean", GetValueType(instance, value), param.offset + 1);
                    return false;
                }

                bool b = value.As<Napi::Boolean>();
                *((param.gpr_count ? gpr_ptr : args_ptr)++) = (uint64_t)b;
            } break;
            case PrimitiveKind::Int8:
            case PrimitiveKind::UInt8:
            case PrimitiveKind::Int16:
            case PrimitiveKind::UInt16:
            case PrimitiveKind::Int32:
            case PrimitiveKind::UInt32:
            case PrimitiveKind::Int64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), param.offset + 1);
                    return false;
                }

                int64_t v = CopyNumber<int64_t>(value);
                *(int64_t *)((param.gpr_count ? gpr_ptr : args_ptr)++) = v;
            } break;
            case PrimitiveKind::UInt64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), param.offset + 1);
                    return false;
                }

                uint64_t v = CopyNumber<uint64_t>(value);
                *((param.gpr_count ? gpr_ptr : args_ptr)++) = v;
            } break;
            case PrimitiveKind::String: {
                const char *str;
                if (RG_LIKELY(value.IsString())) {
                    str = PushString(value);
                    if (RG_UNLIKELY(!str))
                        return false;
                } else if (IsNullOrUndefined(value)) {
                    str = nullptr;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected string", GetValueType(instance, value), param.offset + 1);
                    return false;
                }

                *(const char **)((param.gpr_count ? gpr_ptr : args_ptr)++) = str;
            } break;
            case PrimitiveKind::String16: {
                const char16_t *str16;
                if (RG_LIKELY(value.IsString())) {
                    str16 = PushString16(value);
                    if (RG_UNLIKELY(!str16))
                        return false;
                } else if (IsNullOrUndefined(value)) {
                    str16 = nullptr;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected string", GetValueType(instance, value), param.offset + 1);
                    return false;
                }

                *(const char16_t **)((param.gpr_count ? gpr_ptr : args_ptr)++) = str16;
            } break;
            case PrimitiveKind::Pointer: {
                void *ptr;
                if (RG_UNLIKELY(!PushPointer(value, param, &ptr)))
                    return false;

                *(void **)((param.gpr_count ? gpr_ptr : args_ptr)++) = ptr;
            } break;
            case PrimitiveKind::Record: {
                if (RG_UNLIKELY(!IsObject(value))) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected object", GetValueType(instance, value), param.offset + 1);
                    return false;
                }

                Napi::Object obj = value.As<Napi::Object>();

                if (param.gpr_count || param.xmm_count) {
                    RG_ASSERT(param.type->size <= 16);

                    uint64_t buf[2] = {};
                    if (!PushObject(obj, param.type, (uint8_t *)buf))
                        return false;
                    uint64_t *ptr = buf;

                    if (param.gpr_first) {
                        *(gpr_ptr++) = *(ptr++);
                        if (param.gpr_count == 2) {
                            *(gpr_ptr++) = *(ptr++);
                        } else if (param.xmm_count == 1) {
                            *(xmm_ptr++) = *(ptr++);
                        }
                    } else {
                        *(xmm_ptr++) = *(ptr++);
                        if (param.xmm_count == 2) {
                            *(xmm_ptr++) = *(ptr++);
                        } else if (param.gpr_count == 1) {
                            *(gpr_ptr++) = *(ptr++);
                        }
                    }
                } else if (param.use_memory) {
                    args_ptr = AlignUp(args_ptr, param.type->align);
                    if (!PushObject(obj, param.type, (uint8_t *)args_ptr))
                        return false;
                    args_ptr += (param.type->size + 7) / 8;
                }
            } break;
            case PrimitiveKind::Array: { RG_UNREACHABLE(); } break;
            case PrimitiveKind::Float32: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), param.offset + 1);
                    return false;
                }

                float f = CopyNumber<float>(value);
                uint64_t *ptr = (param.xmm_count ? xmm_ptr : args_ptr)++;

                memset((uint8_t *)ptr + 4, 0, 4);
                *(float *)ptr = f;
            } break;
            case PrimitiveKind::Float64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), param.offset + 1);
                    return false;
                }

                double d = CopyNumber<double>(value);
                *(double *)((param.xmm_count ? xmm_ptr : args_ptr)++) = d;
            } break;
            case PrimitiveKind::Callback: {
                void *ptr;

                if (value.IsFunction()) {
                    Napi::Function func = value.As<Napi::Function>();

                    ptr = ReserveTrampoline(param.type->ref.proto, func);
                    if (RG_UNLIKELY(!ptr))
                        return false;
                } else if (CheckValueTag(instance, value, param.type->ref.marker)) {
                    ptr = value.As<Napi::External<void>>().Data();
                } else if (IsNullOrUndefined(value)) {
                    ptr = nullptr;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected %3", GetValueType(instance, value), param.offset + 1, param.type->name);
                    return false;
                }

                *(void **)((param.gpr_count ? gpr_ptr : args_ptr)++) = ptr;
            } break;

            case PrimitiveKind::Prototype: { RG_UNREACHABLE(); } break;
        }
    }

    new_sp = mem->stack.end();

    return true;
}

void CallData::Execute()
{
    exec_call = this;

#define PERFORM_CALL(Suffix) \
        ([&]() { \
            auto ret = (func->forward_fp ? ForwardCallX ## Suffix(func->func, new_sp, &old_sp) \
                                         : ForwardCall ## Suffix(func->func, new_sp, &old_sp)); \
            return ret; \
        })()

    // Execute and convert return value
    switch (func->ret.type->primitive) {
        case PrimitiveKind::Void:
        case PrimitiveKind::Bool:
        case PrimitiveKind::Int8:
        case PrimitiveKind::UInt8:
        case PrimitiveKind::Int16:
        case PrimitiveKind::UInt16:
        case PrimitiveKind::Int32:
        case PrimitiveKind::UInt32:
        case PrimitiveKind::Int64: 
        case PrimitiveKind::UInt64:
        case PrimitiveKind::String:
        case PrimitiveKind::String16:
        case PrimitiveKind::Pointer:
        case PrimitiveKind::Callback: { result.u64 = PERFORM_CALL(GG).rax; } break;
        case PrimitiveKind::Record: {
            if (func->ret.gpr_first && !func->ret.xmm_count) {
                RaxRdxRet ret = PERFORM_CALL(GG);
                memcpy(&result.buf, &ret, RG_SIZE(ret));
            } else if (func->ret.gpr_first) {
                RaxXmm0Ret ret = PERFORM_CALL(GD);
                memcpy(&result.buf, &ret, RG_SIZE(ret));
            } else if (func->ret.xmm_count == 2) {
                Xmm0Xmm1Ret ret = PERFORM_CALL(DD);
                memcpy(&result.buf, &ret, RG_SIZE(ret));
            } else {
                Xmm0RaxRet ret = PERFORM_CALL(DG);
                memcpy(&result.buf, &ret, RG_SIZE(ret));
            }
        } break;
        case PrimitiveKind::Array: { RG_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: { result.f = PERFORM_CALL(F); } break;
        case PrimitiveKind::Float64: { result.d = PERFORM_CALL(DG).xmm0; } break;

        case PrimitiveKind::Prototype: { RG_UNREACHABLE(); } break;
    }

#undef PERFORM_CALL
}

Napi::Value CallData::Complete()
{
    RG_DEFER {
       PopOutArguments();

        if (func->ret.type->dispose) {
            func->ret.type->dispose(env, func->ret.type, result.ptr);
        }
    };

    switch (func->ret.type->primitive) {
        case PrimitiveKind::Void: return env.Undefined();
        case PrimitiveKind::Bool: return Napi::Boolean::New(env, result.u32);
        case PrimitiveKind::Int8: return Napi::Number::New(env, (double)result.i8);
        case PrimitiveKind::UInt8: return Napi::Number::New(env, (double)result.u8);
        case PrimitiveKind::Int16: return Napi::Number::New(env, (double)result.i16);
        case PrimitiveKind::UInt16: return Napi::Number::New(env, (double)result.u16);
        case PrimitiveKind::Int32: return Napi::Number::New(env, (double)result.i32);
        case PrimitiveKind::UInt32: return Napi::Number::New(env, (double)result.u32);
        case PrimitiveKind::Int64: return NewBigInt(env, result.i64);
        case PrimitiveKind::UInt64: return NewBigInt(env, result.u64);
        case PrimitiveKind::String: return result.ptr ? Napi::String::New(env, (const char *)result.ptr) : env.Null();
        case PrimitiveKind::String16: return result.ptr ? Napi::String::New(env, (const char16_t *)result.ptr) : env.Null();
        case PrimitiveKind::Pointer:
        case PrimitiveKind::Callback: {
            if (result.ptr) {
                Napi::External<void> external = Napi::External<void>::New(env, result.ptr);
                SetValueTag(instance, external, func->ret.type->ref.marker);

                return external;
            } else {
                return env.Null();
            }
        } break;
        case PrimitiveKind::Record: {
            const uint8_t *ptr = return_ptr ? (const uint8_t *)return_ptr
                                            : (const uint8_t *)&result.buf;

            Napi::Object obj = PopObject(ptr, func->ret.type);
            return obj;
        } break;
        case PrimitiveKind::Array: { RG_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: return Napi::Number::New(env, (double)result.f);
        case PrimitiveKind::Float64: return Napi::Number::New(env, result.d);

        case PrimitiveKind::Prototype: { RG_UNREACHABLE(); } break;
    }

    RG_UNREACHABLE();
}

void CallData::Relay(Size idx, uint8_t *own_sp, uint8_t *caller_sp, BackRegisters *out_reg)
{
    if (RG_UNLIKELY(env.IsExceptionPending()))
        return;

    const TrampolineInfo &trampoline = instance->trampolines[idx];

    const FunctionInfo *proto = trampoline.proto;
    Napi::Function func = trampoline.func.Value();

    uint64_t *gpr_ptr = (uint64_t *)own_sp;
    uint64_t *xmm_ptr = gpr_ptr + 6;
    uint64_t *args_ptr = (uint64_t *)caller_sp;

    uint8_t *return_ptr = proto->ret.use_memory ? (uint8_t *)gpr_ptr[0] : nullptr;
    gpr_ptr += proto->ret.use_memory;

    RG_DEFER_N(err_guard) { memset(out_reg, 0, RG_SIZE(*out_reg)); };

    if (RG_UNLIKELY(trampoline.generation >= 0 && trampoline.generation != (int32_t)mem->generation)) {
        ThrowError<Napi::Error>(env, "Cannot use non-registered callback beyond FFI call");
        return;
    }

    LocalArray<napi_value, MaxParameters> arguments;

    // Convert to JS arguments
    for (Size i = 0; i < proto->parameters.len; i++) {
        const ParameterInfo &param = proto->parameters[i];
        RG_ASSERT(param.directions >= 1 && param.directions <= 3);

        switch (param.type->primitive) {
            case PrimitiveKind::Void: { RG_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                bool b = *(bool *)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = Napi::Boolean::New(env, b);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int8: {
                double d = (double)*(int8_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt8: {
                double d = (double)*(uint8_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int16: {
                double d = (double)*(int16_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt16: {
                double d = (double)*(uint16_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int32: {
                double d = (double)*(int32_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt32: {
                double d = (double)*(uint32_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int64: {
                int64_t v = *(int64_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = NewBigInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt64: {
                uint64_t v = *(uint64_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = NewBigInt(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::String: {
                const char *str = *(const char **)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = str ? Napi::String::New(env, str) : env.Null();
                arguments.Append(arg);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, str);
                }
            } break;
            case PrimitiveKind::String16: {
                const char16_t *str16 = *(const char16_t **)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = str16 ? Napi::String::New(env, str16) : env.Null();
                arguments.Append(arg);

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, str16);
                }
            } break;
            case PrimitiveKind::Pointer:
            case PrimitiveKind::Callback: {
                void *ptr2 = *(void **)((param.gpr_count ? gpr_ptr : args_ptr)++);

                if (ptr2) {
                    Napi::External<void> external = Napi::External<void>::New(env, ptr2);
                    SetValueTag(instance, external, param.type->ref.marker);

                    arguments.Append(external);
                } else {
                    arguments.Append(env.Null());
                }

                if (param.type->dispose) {
                    param.type->dispose(env, param.type, ptr2);
                }
            } break;
            case PrimitiveKind::Record: {
                if (param.gpr_count || param.xmm_count) {
                    RG_ASSERT(param.type->size <= 16);

                    uint64_t buf[2] = {};
                    uint64_t *ptr = buf;

                    if (param.gpr_first) {
                        *(ptr++) = *(gpr_ptr++);
                        if (param.gpr_count == 2) {
                            *(ptr++) = *(gpr_ptr++);
                        } else if (param.xmm_count == 1) {
                            *(ptr++) = *(xmm_ptr++);
                        }
                    } else {
                        *(ptr++) = *(xmm_ptr++);
                        if (param.xmm_count == 2) {
                            *(ptr++) = *(xmm_ptr++);
                        } else if (param.gpr_count == 1) {
                            *(ptr++) = *(gpr_ptr++);
                        }
                    }

                    Napi::Object obj = PopObject((const uint8_t *)buf, param.type);
                    arguments.Append(obj);
                } else if (param.use_memory) {
                    args_ptr = AlignUp(args_ptr, param.type->align);

                    Napi::Object obj = PopObject((const uint8_t *)args_ptr, param.type);
                    arguments.Append(obj);

                    args_ptr += (param.type->size + 7) / 8;
                }
            } break;
            case PrimitiveKind::Array: { RG_UNREACHABLE(); } break;
            case PrimitiveKind::Float32: {
                float f = *(float *)((param.xmm_count ? xmm_ptr : args_ptr)++);

                Napi::Value arg = Napi::Number::New(env, (double)f);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Float64: {
                double d = *(double *)((param.xmm_count ? xmm_ptr : args_ptr)++);

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;

            case PrimitiveKind::Prototype: { RG_UNREACHABLE(); } break;
        }
    }

    const TypeInfo *type = proto->ret.type;

    // Make the call
    napi_value ret = CallSwitchStack(&func, (size_t)arguments.len, arguments.data, old_sp, &mem->stack,
                                     [](Napi::Function *func, size_t argc, napi_value *argv) { return (napi_value)func->Call(argc, argv); });
    Napi::Value value(env, ret);

    if (RG_UNLIKELY(env.IsExceptionPending()))
        return;

    // Convert the result
    switch (type->primitive) {
        case PrimitiveKind::Void: {} break;
        case PrimitiveKind::Bool: {
            if (RG_UNLIKELY(!value.IsBoolean())) {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for return value, expected boolean", GetValueType(instance, value));
                return;
            }

            bool b = value.As<Napi::Boolean>();
            out_reg->rax = (uint64_t)b;
        } break;
        case PrimitiveKind::Int8:
        case PrimitiveKind::UInt8:
        case PrimitiveKind::Int16:
        case PrimitiveKind::UInt16:
        case PrimitiveKind::Int32:
        case PrimitiveKind::UInt32:
        case PrimitiveKind::Int64:
        case PrimitiveKind::UInt64: {
            if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for return value, expected number", GetValueType(instance, value));
                return;
            }

            int64_t v = CopyNumber<int64_t>(value);
            out_reg->rax = (uint64_t)v;
        } break;
        case PrimitiveKind::String: {
            const char *str;
            if (RG_LIKELY(value.IsString())) {
                str = PushString(value);
                if (RG_UNLIKELY(!str))
                    return;
            } else if (IsNullOrUndefined(value)) {
                str = nullptr;
            } else {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for return value, expected string", GetValueType(instance, value));
                return;
            }

            out_reg->rax = (uint64_t)str;
        } break;
        case PrimitiveKind::String16: {
            const char16_t *str16;
            if (RG_LIKELY(value.IsString())) {
                str16 = PushString16(value);
                if (RG_UNLIKELY(!str16))
                    return;
            } else if (IsNullOrUndefined(value)) {
                str16 = nullptr;
            } else {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for return value, expected string", GetValueType(instance, value));
                return;
            }

            out_reg->rax = (uint64_t)str16;
        } break;
        case PrimitiveKind::Pointer: {
            uint8_t *ptr;

            if (CheckValueTag(instance, value, type->ref.marker)) {
                ptr = value.As<Napi::External<uint8_t>>().Data();
            } else if (IsObject(value) && type->ref.type->primitive == PrimitiveKind::Record) {
                Napi::Object obj = value.As<Napi::Object>();

                ptr = AllocHeap(type->ref.type->size, 16);

                if (!PushObject(obj, type->ref.type, ptr))
                    return;
            } else if (IsNullOrUndefined(value)) {
                ptr = nullptr;
            } else {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for return value, expected %2", GetValueType(instance, value), type->name);
                return;
            }

            out_reg->rax = (uint64_t)ptr;
        } break;
        case PrimitiveKind::Record: {
            if (RG_UNLIKELY(!IsObject(value))) {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for return value, expected object", GetValueType(instance, value));
                return;
            }

            Napi::Object obj = value.As<Napi::Object>();

            if (return_ptr) {
                if (!PushObject(obj, type, return_ptr))
                    return;
                out_reg->rax = (uint64_t)return_ptr;
            } else {
                RG_ASSERT(type->size <= 16);

                uint8_t buf[16] = {};
                if (!PushObject(obj, type, buf))
                    return;

                if (proto->ret.gpr_first && !proto->ret.xmm_count) {
                    memcpy(&out_reg->rax, buf + 0, 8);
                    memcpy(&out_reg->rdx, buf + 8, 8);
                } else if (proto->ret.gpr_first) {
                    memcpy(&out_reg->rax, buf + 0, 8);
                    memcpy(&out_reg->xmm0, buf + 8, 8);
                } else if (proto->ret.xmm_count == 2) {
                    memcpy(&out_reg->xmm0, buf + 0, 8);
                    memcpy(&out_reg->xmm1, buf + 8, 8);
                } else {
                    memcpy(&out_reg->xmm0, buf + 0, 8);
                    memcpy(&out_reg->rax, buf + 8, 8);
                }
            }
        } break;
        case PrimitiveKind::Array: { RG_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: {
            if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for return value, expected number", GetValueType(instance, value));
                return;
            }

            float f = CopyNumber<float>(value);

            memset((uint8_t *)&out_reg->xmm0 + 4, 0, 4);
            memcpy(&out_reg->xmm0, &f, 4);
        } break;
        case PrimitiveKind::Float64: {
            if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for return value, expected number", GetValueType(instance, value));
                return;
            }

            double d = CopyNumber<double>(value);
            out_reg->xmm0 = d;
        } break;
        case PrimitiveKind::Callback: {
            void *ptr;

            if (value.IsFunction()) {
                Napi::Function func2 = value.As<Napi::Function>();

                ptr = ReserveTrampoline(type->ref.proto, func2);
                if (RG_UNLIKELY(!ptr))
                    return;
            } else if (CheckValueTag(instance, value, type->ref.marker)) {
                ptr = value.As<Napi::External<uint8_t>>().Data();
            } else if (IsNullOrUndefined(value)) {
                ptr = nullptr;
            } else {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for return value, expected %2", GetValueType(instance, value), type->name);
                return;
            }

            out_reg->rax = (uint64_t)ptr;
        } break;

        case PrimitiveKind::Prototype: { RG_UNREACHABLE(); } break;
    }

    err_guard.Disable();
}

void *GetTrampoline(Size idx, const FunctionInfo *proto)
{
    bool xmm = proto->forward_fp || IsFloat(proto->ret.type);
    return Trampolines[idx][xmm];
}

extern "C" void RelayCallback(Size idx, uint8_t *own_sp, uint8_t *caller_sp, BackRegisters *out_reg)
{
    exec_call->Relay(idx, own_sp, caller_sp, out_reg);
}

}

#endif
