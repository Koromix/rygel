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

#if defined(__aarch64__) || defined(_M_ARM64)

#include "vendor/libcc/libcc.hh"
#include "ffi.hh"
#include "call.hh"
#include "util.hh"

#include <napi.h>

namespace RG {

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
    { &Trampoline15, &TrampolineX15 }
};
RG_STATIC_ASSERT(RG_LEN(Trampolines) == MaxTrampolines);

static RG_THREAD_LOCAL CallData *exec_call;

static inline int IsHFA(const TypeInfo *type)
{
    return IsHFA(type, 1, 4);
}

bool AnalyseFunction(InstanceData *, FunctionInfo *func)
{
    if (int hfa = IsHFA(func->ret.type); hfa) {
        func->ret.vec_count = hfa;
    } else if (func->ret.type->size <= 16) {
        func->ret.gpr_count = (func->ret.type->size + 7) / 8;
    } else {
        func->ret.use_memory = true;
    }

    int gpr_avail = 8;
    int vec_avail = 8;

    for (ParameterInfo &param: func->parameters) {
        switch (param.type->primitive) {
            case PrimitiveKind::Void: { RG_UNREACHABLE(); } break;

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
#ifdef __APPLE__
                if (param.variadic)
                    break;
#endif

                if (gpr_avail) {
                    param.gpr_count = 1;
                    gpr_avail--;
                }
            } break;
            case PrimitiveKind::Record: {
                int hfa = IsHFA(param.type);

#if defined(_WIN32)
                if (!param.variadic) {
                    hfa = 0;
                }
#elif defined(__APPLE__)
                if (param.variadic) {
                    param.use_memory = (param.type->size > 16);
                    break;
                }
#endif

                if (hfa) {
                    if (hfa <= vec_avail) {
                        param.vec_count = hfa;
                        vec_avail -= hfa;
                    } else {
                        vec_avail = 0;
                    }
                } else if (param.type->size <= 16) {
                    int gpr_count = (param.type->size + 7) / 8;

                    if (gpr_count <= gpr_avail) {
                        param.gpr_count = gpr_count;
                        gpr_avail -= gpr_count;
                    } else {
                        gpr_avail = 0;
                    }
                } else {
                    // Big types (more than 16 bytes) are replaced by a pointer
                    if (gpr_avail) {
                        param.gpr_count = 1;
                        gpr_avail -= 1;
                    }
                    param.use_memory = true;
                }
            } break;
            case PrimitiveKind::Array: { RG_UNREACHABLE(); } break;
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
        }
    }

    func->args_size = 16 * func->parameters.len;
    func->forward_fp = (vec_avail < 8);

    return true;
}

bool CallData::Prepare(const Napi::CallbackInfo &info)
{
    uint8_t *args_ptr = nullptr;
    uint64_t *gpr_ptr = nullptr;
    uint64_t *vec_ptr = nullptr;

    // Return through registers unless it's too big
    if (RG_UNLIKELY(!AllocStack(func->args_size, 16, &args_ptr)))
        return false;
    if (RG_UNLIKELY(!AllocStack(8 * 8, 8, &vec_ptr)))
        return false;
    if (RG_UNLIKELY(!AllocStack(9 * 8, 8, &gpr_ptr)))
        return false;
    if (func->ret.use_memory) {
        if (RG_UNLIKELY(!AllocHeap(func->ret.type->size, 16, &return_ptr)))
            return false;
        gpr_ptr[8] = (uint64_t)return_ptr;
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
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected boolean", GetValueType(instance, value), i + 1);
                    return false;
                }

                bool b = value.As<Napi::Boolean>();

                if (RG_LIKELY(param.gpr_count)) {
                    *(gpr_ptr++) = (uint64_t)b;
                } else {
                    *args_ptr = (uint8_t)b;
#ifdef __APPLE__
                    args_ptr++;
#else
                    args_ptr += 8;
#endif
                }
            } break;
            case PrimitiveKind::Int8:
            case PrimitiveKind::UInt8:
            case PrimitiveKind::Int16:
            case PrimitiveKind::UInt16:
            case PrimitiveKind::Int32:
            case PrimitiveKind::UInt32:
            case PrimitiveKind::Int64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return false;
                }

                int64_t v = CopyNumber<int64_t>(value);

                if (RG_LIKELY(param.gpr_count)) {
                    *(int64_t *)(gpr_ptr++) = v;
                } else {
                    args_ptr = AlignUp(args_ptr, param.type->align);
                    *(int64_t *)args_ptr = v;
#ifdef __APPLE__
                    args_ptr += param.type->size;
#else
                    args_ptr += 8;
#endif
                }
            } break;
            case PrimitiveKind::UInt64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return false;
                }

                uint64_t v = CopyNumber<uint64_t>(value);

                if (RG_LIKELY(param.gpr_count)) {
                    *(gpr_ptr++) = v;
                } else {
                    args_ptr = AlignUp(args_ptr, param.type->align);
                    *(uint64_t *)args_ptr = v;
                    args_ptr += 8;
                }
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
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected string", GetValueType(instance, value), i + 1);
                    return false;
                }

                if (RG_LIKELY(param.gpr_count)) {
                    *(gpr_ptr++) = (uint64_t)str;
                } else {
                    args_ptr = AlignUp(args_ptr, 8);
                    *(uint64_t *)args_ptr = (uint64_t)str;
                    args_ptr += 8;
                }
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
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected string", GetValueType(instance, value), i + 1);
                    return false;
                }

                if (RG_LIKELY(param.gpr_count)) {
                    *(gpr_ptr++) = (uint64_t)str16;
                } else {
                    args_ptr = AlignUp(args_ptr, 8);
                    *(uint64_t *)args_ptr = (uint64_t)str16;
                    args_ptr += 8;
                }
            } break;
            case PrimitiveKind::Pointer: {
                uint8_t *ptr;

                if (CheckValueTag(instance, value, param.type)) {
                    ptr = value.As<Napi::External<uint8_t>>().Data();
                } else if (IsObject(value) && param.type->ref->primitive == PrimitiveKind::Record) {
                    Napi::Object obj = value.As<Napi::Object>();

                    if (RG_UNLIKELY(!AllocHeap(param.type->ref->size, 16, &ptr)))
                        return false;

                    if (param.directions & 1) {
                        if (!PushObject(obj, param.type->ref, ptr))
                            return false;
                    } else {
                        memset(ptr, 0, param.type->size);
                    }
                    if (param.directions & 2) {
                        OutObject *out = out_objects.AppendDefault();

                        out->ref.Reset(obj, 1);
                        out->ptr = ptr;
                        out->type = param.type->ref;
                    }
                } else if (IsNullOrUndefined(value)) {
                    ptr = nullptr;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected %3", GetValueType(instance, value), i + 1, param.type->name);
                    return false;
                }

                if (RG_LIKELY(param.gpr_count)) {
                    *(gpr_ptr++) = (uint64_t)ptr;
                } else {
                    args_ptr = AlignUp(args_ptr, 8);
                    *(uint64_t *)args_ptr = (uint64_t)ptr;
                    args_ptr += 8;
                }
            } break;
            case PrimitiveKind::Record: {
                if (RG_UNLIKELY(!IsObject(value))) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected object", GetValueType(instance, value), i + 1);
                    return false;
                }

                Napi::Object obj = value.As<Napi::Object>();

                if (param.vec_count) { // HFA
                    if (!PushObject(obj, param.type, (uint8_t *)vec_ptr, 8))
                        return false;
                    vec_ptr += param.vec_count;
                } else if (!param.use_memory) {
                    if (param.gpr_count) {
                        RG_ASSERT(param.type->align <= 8);

                        if (!PushObject(obj, param.type, (uint8_t *)gpr_ptr))
                            return false;
                        gpr_ptr += param.gpr_count;
                    } else if (param.type->size) {
                        args_ptr = AlignUp(args_ptr, 8);
                        if (!PushObject(obj, param.type, args_ptr))
                            return false;
                        args_ptr += AlignLen(param.type->size, 8);
                    }
                } else {
                    uint8_t *ptr;
                    if (RG_UNLIKELY(!AllocHeap(param.type->size, 16, &ptr)))
                        return false;

                    if (param.gpr_count) {
                        RG_ASSERT(param.gpr_count == 1);
                        RG_ASSERT(param.vec_count == 0);

                        *(gpr_ptr++) = (uint64_t)ptr;
                    } else {
                        args_ptr = AlignUp(args_ptr, 8);
                        *(uint8_t **)args_ptr = ptr;
                        args_ptr += 8;
                    }

                    if (!PushObject(obj, param.type, ptr))
                        return false;
                }
            } break;
            case PrimitiveKind::Array: { RG_UNREACHABLE(); } break;
            case PrimitiveKind::Float32: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return false;
                }

                float f = CopyNumber<float>(value);

                if (RG_LIKELY(param.vec_count)) {
                    memset((uint8_t *)vec_ptr + 4, 0, 4);
                    *(float *)(vec_ptr++) = f;
                } else {
                    args_ptr = AlignUp(args_ptr, 4);
                    *(float *)args_ptr = f;
#ifdef __APPLE__
                    args_ptr += 4;
#else
                    memset((uint8_t *)args_ptr + 4, 0, 4);
                    args_ptr += 8;
#endif
                }
            } break;
            case PrimitiveKind::Float64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return false;
                }

                double d = CopyNumber<double>(value);

                if (RG_LIKELY(param.vec_count)) {
                    *(double *)(vec_ptr++) = d;
                } else {
                    args_ptr = AlignUp(args_ptr, 8);
                    *(double *)args_ptr = d;
                    args_ptr += 8;
                }
            } break;
            case PrimitiveKind::Callback: {
                void *ptr;

                if (value.IsFunction()) {
                    Napi::Function func = value.As<Napi::Function>();

                    Size idx = ReserveTrampoline(param.type->proto, func);
                    if (RG_UNLIKELY(idx < 0))
                        return false;

                    ptr = GetTrampoline(idx, param.type->proto);
                } else if (CheckValueTag(instance, value, param.type)) {
                    ptr = value.As<Napi::External<void>>().Data();
                } else if (IsNullOrUndefined(value)) {
                    ptr = nullptr;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected %3", GetValueType(instance, value), i + 1, param.type->name);
                    return false;
                }

                if (RG_LIKELY(param.gpr_count)) {
                    *(gpr_ptr++) = (uint64_t)ptr;
                } else {
                    args_ptr = AlignUp(args_ptr, 8);
                    *(uint64_t *)args_ptr = (uint64_t)ptr;
                    args_ptr += 8;
                }
            } break;
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
        case PrimitiveKind::Callback: { result.u64 = PERFORM_CALL(GG).x0; } break;
        case PrimitiveKind::Record: {
            if (func->ret.gpr_count) {
                X0X1Ret ret = PERFORM_CALL(GG);
                memcpy(&result.buf, &ret, RG_SIZE(ret));
            } else if (func->ret.vec_count) {
                HfaRet ret = PERFORM_CALL(DDDD);
                memcpy(&result.buf, &ret, RG_SIZE(ret));
            } else {
                PERFORM_CALL(GG);
            }
        } break;
        case PrimitiveKind::Array: { RG_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: { result.f = PERFORM_CALL(F); } break;
        case PrimitiveKind::Float64: { result.d = PERFORM_CALL(DDDD).d0; } break;
    }

#undef PERFORM_CALL
}

Napi::Value CallData::Complete()
{
    for (const OutObject &out: out_objects) {
        Napi::Object obj = out.ref.Value().As<Napi::Object>();
        PopObject(obj, out.ptr, out.type);
    }

    switch (func->ret.type->primitive) {
        case PrimitiveKind::Void: return env.Null();
        case PrimitiveKind::Bool: return Napi::Boolean::New(env, result.u32);
        case PrimitiveKind::Int8:
        case PrimitiveKind::UInt8:
        case PrimitiveKind::Int16:
        case PrimitiveKind::UInt16:
        case PrimitiveKind::Int32:
        case PrimitiveKind::UInt32: return Napi::Number::New(env, (double)result.u32);
        case PrimitiveKind::Int64: return Napi::BigInt::New(env, (int64_t)result.u64);
        case PrimitiveKind::UInt64: return Napi::BigInt::New(env, result.u64);
        case PrimitiveKind::String: return Napi::String::New(env, (const char *)result.ptr);
        case PrimitiveKind::String16: return Napi::String::New(env, (const char16_t *)result.ptr);
        case PrimitiveKind::Pointer:
        case PrimitiveKind::Callback: {
            if (result.ptr) {
                Napi::External<void> external = Napi::External<void>::New(env, result.ptr);
                SetValueTag(instance, external, func->ret.type);

                return external;
            } else {
                return env.Null();
            }
        } break;
        case PrimitiveKind::Record: {
            if (func->ret.vec_count) { // HFA
                Napi::Object obj = PopObject((const uint8_t *)&result.buf, func->ret.type, 8);
                return obj;
            } else {
                const uint8_t *ptr = return_ptr ? (const uint8_t *)return_ptr
                                                : (const uint8_t *)&result.buf;

                Napi::Object obj = PopObject(ptr, func->ret.type);
                return obj;
            }
        } break;
        case PrimitiveKind::Array: { RG_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: return Napi::Number::New(env, (double)result.f);
        case PrimitiveKind::Float64: return Napi::Number::New(env, result.d);
    }

    RG_UNREACHABLE();
}

void CallData::Relay(Size idx, uint8_t *own_sp, uint8_t *caller_sp, BackRegisters *out_reg)
{
    const FunctionInfo *proto = instance->trampolines[idx].proto;
    Napi::Function func = instance->trampolines[idx].func;

    // Allow reuse of static trampoline
    instance->free_trampolines |= 1u << idx;
    used_trampolines &= ~(1u << idx);

    uint64_t *gpr_ptr = (uint64_t *)own_sp;
    uint64_t *vec_ptr = gpr_ptr + 9;
    uint64_t *args_ptr = (uint64_t *)caller_sp;

    uint8_t *return_ptr = proto->ret.use_memory ? (uint8_t *)gpr_ptr[8] : nullptr;

    LocalArray<napi_value, MaxParameters> arguments;

    // Convert to JS arguments
    for (Size i = 0; i < proto->parameters.len; i++) {
        const ParameterInfo &param = proto->parameters[i];
        RG_ASSERT(param.directions >= 1 && param.directions <= 3);

        switch (param.type->primitive) {
            case PrimitiveKind::Void: { RG_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
#ifdef __APPLE__
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
#ifdef __APPLE__
                double d;
                if (param.gpr_count) {
                    d = (double)*(int8_t *)(gpr_ptr++);
                } else {
                    d = (double)*(int8_t *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 1);
                }
#else
                double d = (double)*(int8_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt8: {
#ifdef __APPLE__
                double d;
                if (param.gpr_count) {
                    d = (double)*(uint8_t *)(gpr_ptr++);
                } else {
                    d = (double)*(uint8_t *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 1);
                }
#else
                double d = (double)*(uint8_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int16: {
#ifdef __APPLE__
                double d;
                if (param.gpr_count) {
                    d = (double)*(int16_t *)(gpr_ptr++);
                } else {
                    args_ptr = AlignUp(args_ptr, 2);
                    d = (double)*(int16_t *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 2);
                }
#else
                double d = (double)*(int16_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt16: {
#ifdef __APPLE__
                double d;
                if (param.gpr_count) {
                    d = (double)*(uint16_t *)(gpr_ptr++);
                } else {
                    args_ptr = AlignUp(args_ptr, 2);
                    d = (double)*(uint16_t *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 2);
                }
#else
                double d = (double)*(uint16_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int32: {
#ifdef __APPLE__
                double d;
                if (param.gpr_count) {
                    d = (double)*(int32_t *)(gpr_ptr++);
                } else {
                    args_ptr = AlignUp(args_ptr, 4);
                    d = (double)*(int32_t *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 4);
                }
#else
                double d = (double)*(int32_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt32: {
#ifdef __APPLE__
                double d;
                if (param.gpr_count) {
                    d = (double)*(uint32_t *)(gpr_ptr++);
                } else {
                    args_ptr = AlignUp(args_ptr, 4);
                    d = (double)*(uint32_t *)args_ptr;
                    args_ptr = (uint64_t *)((uint8_t *)args_ptr + 4);
                }
#else
                double d = (double)*(uint32_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);
#endif

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Int64: {
#ifdef __APPLE__
                args_ptr = AlignUp(args_ptr, 8);
#endif

                int64_t v = *(int64_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = Napi::BigInt::New(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::UInt64: {
#ifdef __APPLE__
                args_ptr = AlignUp(args_ptr, 8);
#endif

                uint64_t v = *(uint64_t *)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = Napi::BigInt::New(env, v);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::String: {
#ifdef __APPLE__
                args_ptr = AlignUp(args_ptr, 8);
#endif

                const char *str = *(const char **)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = Napi::String::New(env, str);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::String16: {
#ifdef __APPLE__
                args_ptr = AlignUp(args_ptr, 8);
#endif

                const char16_t *str16 = *(const char16_t **)((param.gpr_count ? gpr_ptr : args_ptr)++);

                Napi::Value arg = Napi::String::New(env, str16);
                arguments.Append(arg);
            } break;
            case PrimitiveKind::Pointer:
            case PrimitiveKind::Callback: {
#ifdef __APPLE__
                args_ptr = AlignUp(args_ptr, 8);
#endif

                void *ptr2 = *(void **)((param.gpr_count ? gpr_ptr : args_ptr)++);

                if (ptr2) {
                    Napi::External<void> external = Napi::External<void>::New(env, ptr2);
                    SetValueTag(instance, external, param.type);

                    arguments.Append(external);
                } else {
                    arguments.Append(env.Null());
                }
            } break;
            case PrimitiveKind::Record: {
                if (param.vec_count) { // HFA
                    Napi::Object obj = PopObject((uint8_t *)vec_ptr, param.type, 8);
                    arguments.Append(obj);

                    vec_ptr += param.vec_count;
                } else if (!param.use_memory) {
                    if (param.gpr_count) {
                        RG_ASSERT(param.type->align <= 8);

                        Napi::Object obj = PopObject((uint8_t *)gpr_ptr, param.type);
                        arguments.Append(obj);

                        gpr_ptr += param.gpr_count;
                    } else if (param.type->size) {
                        args_ptr = AlignUp(args_ptr, param.type->align);

                        Napi::Object obj = PopObject((uint8_t *)args_ptr, param.type);
                        arguments.Append(obj);

                        args_ptr += (param.type->size + 7) / 8;
                    }
                } else {
#ifdef __APPLE__
                    args_ptr = AlignUp(args_ptr, 8);
#endif

                    void *ptr2 = *(void **)((param.gpr_count ? gpr_ptr : args_ptr)++);

                    Napi::Object obj = PopObject((uint8_t *)ptr2, param.type);
                    arguments.Append(obj);
                }
            } break;
            case PrimitiveKind::Array: { RG_UNREACHABLE(); } break;
            case PrimitiveKind::Float32: {
                float f;
                if (RG_LIKELY(param.vec_count)) {
                    f = *(float *)(vec_ptr++);
                } else if (param.gpr_count) {
                    f = *(float *)(gpr_ptr++);
                } else {
#ifdef __APPLE__
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
                if (RG_LIKELY(param.vec_count)) {
                    d = *(double *)(vec_ptr++);
                } else if (param.gpr_count) {
                    d = *(double *)(gpr_ptr++);
                } else {
#ifdef __APPLE__
                    args_ptr = AlignUp(args_ptr, 8);
#endif

                    d = *(double *)(args_ptr++);
                }

                Napi::Value arg = Napi::Number::New(env, d);
                arguments.Append(arg);
            } break;
        }
    }

    const TypeInfo *type = proto->ret.type;

    // Make the call
    napi_value ret = CallSwitchStack(&func, (size_t)arguments.len, arguments.data, old_sp, &mem->stack,
                                     [](Napi::Function *func, size_t argc, napi_value *argv) { return (napi_value)func->Call(argc, argv); });
    Napi::Value value(env, ret);

    // Convert the result
    switch (type->primitive) {
        case PrimitiveKind::Void: {} break;
        case PrimitiveKind::Bool: {
            if (RG_UNLIKELY(!value.IsBoolean())) {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for return value, expected boolean", GetValueType(instance, value));
                return;
            }

            bool b = value.As<Napi::Boolean>();
            out_reg->x0 = (uint64_t)b;
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
            out_reg->x0 = (uint64_t)v;
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

            out_reg->x0 = (uint64_t)str;
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

            out_reg->x0 = (uint64_t)str16;
        } break;
        case PrimitiveKind::Pointer: {
            uint8_t *ptr;

            if (CheckValueTag(instance, value, type)) {
                ptr = value.As<Napi::External<uint8_t>>().Data();
            } else if (IsObject(value) && type->ref->primitive == PrimitiveKind::Record) {
                Napi::Object obj = value.As<Napi::Object>();

                if (RG_UNLIKELY(!AllocHeap(type->ref->size, 16, &ptr)))
                    return;

                if (!PushObject(obj, type->ref, ptr))
                    return;
            } else if (IsNullOrUndefined(value)) {
                ptr = nullptr;
            } else {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for return value, expected %2", GetValueType(instance, value), type->name);
                return;
            }

            out_reg->x0 = (uint64_t)ptr;
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
                out_reg->x0 = (uint64_t)return_ptr;
            } else if (proto->ret.vec_count) { // HFA
                PushObject(obj, type, (uint8_t *)&out_reg->d0, 8);
            } else {
                PushObject(obj, type, (uint8_t *)&out_reg->x0);
            }
        } break;
        case PrimitiveKind::Array: { RG_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: {
            if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for return value, expected number", GetValueType(instance, value));
                return;
            }

            float f = CopyNumber<float>(value);

            memset((uint8_t *)&out_reg->d0 + 4, 0, 4);
            memcpy(&out_reg->d0, &f, 4);
        } break;
        case PrimitiveKind::Float64: {
            if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for return value, expected number", GetValueType(instance, value));
                return;
            }

            double d = CopyNumber<double>(value);
            out_reg->d0 = d;
        } break;
        case PrimitiveKind::Callback: {
            void *ptr;

            if (value.IsFunction()) {
                Napi::Function func = value.As<Napi::Function>();

                Size idx = ReserveTrampoline(type->proto, func);
                if (RG_UNLIKELY(idx < 0))
                    return;

                ptr = GetTrampoline(idx, type->proto);
            } else if (CheckValueTag(instance, value, type)) {
                ptr = value.As<Napi::External<uint8_t>>().Data();
            } else if (IsNullOrUndefined(value)) {
                ptr = nullptr;
            } else {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for return value, expected %2", GetValueType(instance, value), type->name);
                return;
            }

            out_reg->x0 = (uint64_t)ptr;
        } break;
    }
}

void *GetTrampoline(Size idx, const FunctionInfo *proto)
{
    bool vec = proto->forward_fp || IsFloat(proto->ret.type);
    return Trampolines[idx][vec];
}

extern "C" void RelayCallBack(Size idx, uint8_t *own_sp, uint8_t *caller_sp, BackRegisters *out_reg)
{
    exec_call->Relay(idx, own_sp, caller_sp, out_reg);
}

}

#endif
