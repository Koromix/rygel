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

#if defined(__aarch64__)

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

extern "C" X0X1Ret ForwardCallGG(const void *func, uint8_t *sp);
extern "C" float ForwardCallF(const void *func, uint8_t *sp);
extern "C" HfaRet ForwardCallDDDD(const void *func, uint8_t *sp);

extern "C" X0X1Ret ForwardCallXGG(const void *func, uint8_t *sp);
extern "C" float ForwardCallXF(const void *func, uint8_t *sp);
extern "C" HfaRet ForwardCallXDDDD(const void *func, uint8_t *sp);

static bool IsHFA(const TypeInfo *type)
{
    if (type->primitive != PrimitiveKind::Record)
        return false;

    if (type->members.len < 1 || type->members.len > 4)
        return false;
    if (type->members[0].type->primitive != PrimitiveKind::Float32 &&
            type->members[0].type->primitive != PrimitiveKind::Float64)
        return false;

    for (Size i = 1; i < type->members.len; i++) {
        if (type->members[i].type != type->members[0].type)
            return false;
    }

    return true;
}

bool AnalyseFunction(InstanceData *, FunctionInfo *func)
{
    if (IsHFA(func->ret.type)) {
        func->ret.vec_count = func->ret.type->members.len;
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
            case PrimitiveKind::Pointer: {
                if (gpr_avail) {
                    param.gpr_count = 1;
                    gpr_avail--;
                }
            } break;

            case PrimitiveKind::Float32:
            case PrimitiveKind::Float64: {
                if (vec_avail) {
                    param.vec_count = 1;
                    vec_avail--;
                }
            } break;

            case PrimitiveKind::Record: {
                if (IsHFA(param.type)) {
                    int vec_count = (int)param.type->members.len;

                    if (vec_count <= vec_avail) {
                        param.vec_count = vec_count;
                        vec_avail -= vec_count;
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
        }
    }

    func->args_size = 16 * func->parameters.len;
    func->forward_fp = (vec_avail < 8);

    return true;
}

static bool PushHFA(const Napi::Object &obj, const TypeInfo *type, uint8_t *dest)
{
    Napi::Env env = obj.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    RG_ASSERT(IsObject(obj));
    RG_ASSERT(type->primitive == PrimitiveKind::Record);
    RG_ASSERT(AlignUp(dest, 8) == dest);

    for (const RecordMember &member: type->members) {
        Napi::Value value = obj.Get(member.name);

        if (member.type->primitive == PrimitiveKind::Float32) {
            if (!value.IsNumber() && !value.IsBigInt()) {
                ThrowError<Napi::TypeError>(env, "Unexpected value %1 for member '%2', expected number", GetValueType(instance, value), member.name);
                return false;
            }

            *(float *)dest = CopyNumber<float>(value);
        } else if (member.type->primitive == PrimitiveKind::Float64) {
            if (!value.IsNumber() && !value.IsBigInt()) {
                ThrowError<Napi::TypeError>(env, "Unexpected value %1 for member '%2', expected number", GetValueType(instance, value), member.name);
                return false;
            }

            *(double *)dest = CopyNumber<double>(value);
        } else {
            RG_UNREACHABLE();
        }

        dest += 8;
    }

    return true;
}

static Napi::Object PopHFA(napi_env env, const uint8_t *ptr, const TypeInfo *type)
{
    RG_ASSERT(type->primitive == PrimitiveKind::Record);

    Napi::Object obj = Napi::Object::New(env);

    for (const RecordMember &member: type->members) {
        if (member.type->primitive == PrimitiveKind::Float32) {
            float f = *(float *)ptr;
            obj.Set(member.name, Napi::Number::New(env, (double)f));
        } else if (member.type->primitive == PrimitiveKind::Float64) {
            double d = *(double *)ptr;
            obj.Set(member.name, Napi::Number::New(env, d));
        } else {
            RG_UNREACHABLE();
        }

        ptr += 8;
    }

    return obj;
}

Napi::Value TranslateCall(InstanceData *instance, const FunctionInfo *func, const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    CallData call(env, instance, func);

    // Sanity checks
    if (info.Length() < (uint32_t)func->parameters.len) {
        ThrowError<Napi::TypeError>(env, "Expected %1 arguments, got %2", func->parameters.len, info.Length());
        return env.Null();
    }

    uint8_t *return_ptr = nullptr;
    uint8_t *args_ptr = nullptr;
    uint64_t *gpr_ptr = nullptr;
    uint64_t *vec_ptr = nullptr;

    // Return through registers unless it's too big
    if (RG_UNLIKELY(!call.AllocStack(func->args_size, 16, &args_ptr)))
        return env.Null();
    if (RG_UNLIKELY(!call.AllocStack(8 * 8, 8, &vec_ptr)))
        return env.Null();
    if (RG_UNLIKELY(!call.AllocStack(9 * 8, 8, &gpr_ptr)))
        return env.Null();
    if (func->ret.use_memory) {
        if (RG_UNLIKELY(!call.AllocHeap(func->ret.type->size, 16, &return_ptr)))
            return env.Null();
        gpr_ptr[8] = (uint64_t)return_ptr;
    }

    LocalArray<OutObject, MaxOutParameters> out_objects;

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
                    return env.Null();
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
            case PrimitiveKind::Int64:
            case PrimitiveKind::UInt64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                int64_t v = CopyNumber<int64_t>(value);

                if (RG_LIKELY(param.gpr_count)) {
                    *(gpr_ptr++) = (uint64_t)v;
                } else {
                    args_ptr = AlignUp(args_ptr, param.type->align);
                    memcpy(args_ptr, &v, param.type->size); // Little Endian
#ifdef __APPLE__
                    args_ptr += param.type->size;
#else
                    args_ptr += 8;
#endif
                }
            } break;
            case PrimitiveKind::Float32: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                float f = CopyNumber<float>(value);

                if (RG_LIKELY(param.vec_count)) {
                    memcpy(vec_ptr++, &f, 4);
                } else {
                    args_ptr = AlignUp(args_ptr, 4);
                    memcpy(args_ptr, &f, 4);
#ifdef __APPLE__
                    args_ptr += 4;
#else
                    args_ptr += 8;
#endif
                }
            } break;
            case PrimitiveKind::Float64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                double d = CopyNumber<double>(value);

                if (RG_LIKELY(param.vec_count)) {
                    memcpy(vec_ptr++, &d, 8);
                } else {
                    args_ptr = AlignUp(args_ptr, 8);
                    memcpy(args_ptr, &d, 8);
                    args_ptr += 8;
                }
            } break;
            case PrimitiveKind::String: {
                const char *str;
                if (RG_LIKELY(value.IsString())) {
                    str = call.PushString(value);
                    if (RG_UNLIKELY(!str))
                        return env.Null();
                } else if (IsNullOrUndefined(value)) {
                    str = nullptr;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected string", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                if (RG_LIKELY(param.gpr_count)) {
                    *(gpr_ptr++) = (uint64_t)str;
                } else {
                    args_ptr = AlignUp(args_ptr, 8);
                    *(uint64_t *)args_ptr = (uint64_t)str;
                    args_ptr += 8;
                }
            } break;
            case PrimitiveKind::Pointer: {
                uint8_t *ptr;

                if (CheckValueTag(instance, value, param.type)) {
                    ptr = value.As<Napi::External<uint8_t>>().Data();
                } else if (IsObject(value) && param.type->ref->primitive == PrimitiveKind::Record) {
                    Napi::Object obj = value.As<Napi::Object>();

                    if (RG_UNLIKELY(!call.AllocHeap(param.type->ref->size, 16, &ptr)))
                        return env.Null();

                    if ((param.directions & 1) && !call.PushObject(obj, param.type->ref, ptr))
                        return env.Null();
                    if (param.directions & 2) {
                        OutObject out = {obj, ptr, param.type->ref};
                        out_objects.Append(out);
                    }
                } else if (IsNullOrUndefined(value)) {
                    ptr = nullptr;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected %3", GetValueType(instance, value), i + 1, param.type->name);
                    return env.Null();
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
                    return env.Null();
                }

                Napi::Object obj = value.As<Napi::Object>();

                if (param.vec_count) {
                    if (!PushHFA(obj, param.type, (uint8_t *)vec_ptr))
                        return env.Null();
                    vec_ptr += param.vec_count;
                } else if (!param.use_memory) {
                    if (param.gpr_count) {
                        RG_ASSERT(param.type->align <= 8);

                        if (!call.PushObject(obj, param.type, (uint8_t *)gpr_ptr))
                            return env.Null();
                        gpr_ptr += param.gpr_count;
                    } else if (param.type->size) {
                        args_ptr = AlignUp(args_ptr, 8);
                        if (!call.PushObject(obj, param.type, args_ptr))
                            return env.Null();
                        args_ptr += AlignLen(param.type->size, 8);
                    }
                } else {
                    uint8_t *ptr;
                    if (RG_UNLIKELY(!call.AllocHeap(param.type->size, 16, &ptr)))
                        return env.Null();

                    if (param.gpr_count) {
                        RG_ASSERT(param.gpr_count == 1);
                        RG_ASSERT(param.vec_count == 0);

                        *(gpr_ptr++) = (uint64_t)ptr;
                    } else {
                        args_ptr = AlignUp(args_ptr, 8);
                        *(uint8_t **)args_ptr = ptr;
                        args_ptr += 8;
                    }

                    if (!call.PushObject(obj, param.type, ptr))
                        return env.Null();
                }
            } break;
        }
    }

    if (instance->debug) {
        call.DumpDebug();
    }

#define PERFORM_CALL(Suffix) \
        ([&]() { \
            auto ret = (func->forward_fp ? ForwardCallX ## Suffix(func->func, call.GetSP()) \
                                         : ForwardCall ## Suffix(func->func, call.GetSP())); \
            PopOutArguments(out_objects); \
            return ret; \
        })()

    // Execute and convert return value
    switch (func->ret.type->primitive) {
        case PrimitiveKind::Float32: {
            float f = PERFORM_CALL(F);

            return Napi::Number::New(env, (double)f);
        } break;

        case PrimitiveKind::Float64: {
            HfaRet ret = PERFORM_CALL(DDDD);

            return Napi::Number::New(env, (double)ret.d0);
        } break;

        case PrimitiveKind::Record: {
            if (func->ret.gpr_count) {
                X0X1Ret ret = PERFORM_CALL(GG);

                Napi::Object obj = PopObject(env, (const uint8_t *)&ret, func->ret.type);
                return obj;
            } else if (func->ret.vec_count) {
                HfaRet ret = PERFORM_CALL(DDDD);

                Napi::Object obj = PopHFA(env, (const uint8_t *)&ret, func->ret.type);
                return obj;
            } else if (func->ret.type->size) {
                RG_ASSERT(return_ptr);

                X0X1Ret ret = PERFORM_CALL(GG);
                RG_ASSERT(ret.x0 == (uint64_t)return_ptr);

                Napi::Object obj = PopObject(env, return_ptr, func->ret.type);
                return obj;
            } else {
                PERFORM_CALL(GG);

                Napi::Object obj = Napi::Object::New(env);
                return obj;
            }
        } break;

        default: {
            X0X1Ret ret = PERFORM_CALL(GG);

            switch (func->ret.type->primitive) {
                case PrimitiveKind::Void: return env.Null();
                case PrimitiveKind::Bool: return Napi::Boolean::New(env, ret.x0);
                case PrimitiveKind::Int8: return Napi::Number::New(env, (double)ret.x0);
                case PrimitiveKind::UInt8: return Napi::Number::New(env, (double)ret.x0);
                case PrimitiveKind::Int16: return Napi::Number::New(env, (double)ret.x0);
                case PrimitiveKind::UInt16: return Napi::Number::New(env, (double)ret.x0);
                case PrimitiveKind::Int32: return Napi::Number::New(env, (double)ret.x0);
                case PrimitiveKind::UInt32: return Napi::Number::New(env, (double)ret.x0);
                case PrimitiveKind::Int64: return Napi::BigInt::New(env, (int64_t)ret.x0);
                case PrimitiveKind::UInt64: return Napi::BigInt::New(env, ret.x0);
                case PrimitiveKind::Float32: { RG_UNREACHABLE(); } break;
                case PrimitiveKind::Float64: { RG_UNREACHABLE(); } break;
                case PrimitiveKind::String: return Napi::String::New(env, (const char *)ret.x0);
                case PrimitiveKind::Pointer: {
                    void *ptr = (void *)ret.x0;

                    Napi::External<void> external = Napi::External<void>::New(env, ptr);
                    SetValueTag(instance, external, func->ret.type);

                    return external;
                } break;

                case PrimitiveKind::Record: { RG_UNREACHABLE(); } break;
            }
        } break;
    }

#undef PERFORM_CALL

    RG_UNREACHABLE();
}

}

#endif
