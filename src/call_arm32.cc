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

#if defined(__arm__)

#include "vendor/libcc/libcc.hh"
#include "ffi.hh"
#include "call.hh"
#include "util.hh"

#include <napi.h>

namespace RG {

struct HfaRet {
    double d0;
    double d1;
    double d2;
    double d3;
};

extern "C" uint64_t ForwardCallGG(const void *func, uint8_t *sp);
extern "C" float ForwardCallF(const void *func, uint8_t *sp);
extern "C" HfaRet ForwardCallDDDD(const void *func, uint8_t *sp);

extern "C" uint64_t ForwardCallXGG(const void *func, uint8_t *sp);
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
        func->ret.vec_count = func->ret.type->members.len *
                              (func->ret.type->members[0].type->size / 4);
    } else if (func->ret.type->size <= 4) {
        func->ret.gpr_count = 1;
    } else {
        func->ret.use_memory = true;
    }

    int gpr_avail = 4 - func->ret.use_memory;
    int vec_avail = 16;
    bool started_stack = false;

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
            case PrimitiveKind::String:
            case PrimitiveKind::Pointer: {
                if (gpr_avail) {
                    param.gpr_count = 1;
                    gpr_avail--;
                } else {
                    started_stack = true;
                }
            } break;

            case PrimitiveKind::Int64:
            case PrimitiveKind::UInt64: {
                if (gpr_avail >= 2) {
                    param.gpr_count = 2;
                    gpr_avail -= 2;
                } else {
                    started_stack = true;
                }
            } break;

            case PrimitiveKind::Float32:
            case PrimitiveKind::Float64: {
                Size need = param.type->size / 4;

                if (need <= vec_avail) {
                    param.vec_count = need;
                    vec_avail -= need;
                } else {
                    started_stack = true;
                }
            } break;

            case PrimitiveKind::Record: {
                if (IsHFA(param.type)) {
                    int vec_count = (int)(param.type->members.len *
                                          param.type->members[0].type->size / 4);

                    if (vec_count <= vec_avail) {
                        param.vec_count = vec_count;
                        vec_avail -= vec_count;
                    } else {
                        vec_avail = 0;
                        started_stack = true;
                    }
                } else if (param.type->size) {
                    int gpr_count = (param.type->size + 3) / 4;

                    if (gpr_count <= gpr_avail) {
                        param.gpr_count = gpr_count;
                        gpr_avail -= gpr_count;
                    } else if (!started_stack) {
                        param.gpr_count = gpr_avail;
                        gpr_avail = 0;

                        started_stack = true;
                    }
                }
            } break;
        }

        func->args_size += AlignLen(param.type->size, 16);
    }

    func->forward_fp = (vec_avail < 16);

    return true;
}

static bool PushHFA(const Napi::Object &obj, const TypeInfo *type, uint8_t *dest)
{
    Napi::Env env = obj.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    RG_ASSERT(obj.IsObject());
    RG_ASSERT(type->primitive == PrimitiveKind::Record);
    RG_ASSERT(AlignUp(dest, type->members[0].type->size) == dest);

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

        dest += type->members[0].type->size;
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

        ptr += member.type->size;
    }

    return obj;
}

Napi::Value TranslateCall(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();
    FunctionInfo *func = (FunctionInfo *)info.Data();

    CallData call(env, instance, func);

    // Sanity checks
    if (info.Length() < (uint32_t)func->parameters.len) {
        ThrowError<Napi::TypeError>(env, "Expected %1 arguments, got %2", func->parameters.len, info.Length());
        return env.Null();
    }

    uint8_t *return_ptr = nullptr;
    uint8_t *args_ptr = nullptr;
    uint32_t *gpr_ptr = nullptr;
    uint32_t *vec_ptr = nullptr;

    // Return through registers unless it's too big
    if (RG_UNLIKELY(!call.AllocStack(func->args_size, 16, &args_ptr)))
        return env.Null();
    if (RG_UNLIKELY(!call.AllocStack(4 * 4, 8, &gpr_ptr)))
        return env.Null();
    if (RG_UNLIKELY(!call.AllocStack(8 * 8, 8, &vec_ptr)))
        return env.Null();
    if (func->ret.use_memory) {
        if (RG_UNLIKELY(!call.AllocHeap(func->ret.type->size, 16, &return_ptr)))
            return env.Null();
        *(uint8_t **)(gpr_ptr++) = return_ptr;
    }
    RG_ASSERT((uint8_t *)gpr_ptr + 16 == args_ptr);

    // Push arguments
    for (Size i = 0; i < func->parameters.len; i++) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Value value = info[i];

        switch (param.type->primitive) {
            case PrimitiveKind::Void: { RG_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                if (RG_UNLIKELY(!value.IsBoolean())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected boolean", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                bool b = value.As<Napi::Boolean>();

                if (RG_LIKELY(param.gpr_count)) {
                    *(gpr_ptr++) = (uint32_t)b;
                } else {
                    *(args_ptr++) = (uint8_t)b;
                }
            } break;
            case PrimitiveKind::Int8:
            case PrimitiveKind::UInt8:
            case PrimitiveKind::Int16:
            case PrimitiveKind::UInt16:
            case PrimitiveKind::Int32:
            case PrimitiveKind::UInt32: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                int64_t v = CopyNumber<int64_t>(value);

                if (RG_LIKELY(param.gpr_count)) {
                    *(gpr_ptr++) = (uint32_t)v;
                } else {
                    args_ptr = AlignUp(args_ptr, param.type->align);
                    memcpy(args_ptr, &v, param.type->size); // Little Endian
                    args_ptr += param.type->size;
                }
            } break;
            case PrimitiveKind::Int64:
            case PrimitiveKind::UInt64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                int64_t v = CopyNumber<int64_t>(value);

                if (RG_LIKELY(param.gpr_count)) {
                    *(uint64_t *)gpr_ptr = (uint64_t)v;
                    gpr_ptr += 2;
                } else {
                    args_ptr = AlignUp(args_ptr, 8);
                    memcpy(args_ptr, &v, param.type->size); // Little Endian
                    args_ptr += 8;
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
                    args_ptr += 4;
                }
            } break;
            case PrimitiveKind::Float64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                double d = CopyNumber<double>(value);

                if (RG_LIKELY(param.vec_count)) {
                    memcpy(vec_ptr, &d, 8);
                    vec_ptr += 2;
                } else {
                    args_ptr = AlignUp(args_ptr, 8);
                    memcpy(args_ptr, &d, 8);
                    args_ptr += 8;
                }
            } break;
            case PrimitiveKind::String: {
                if (RG_UNLIKELY(!value.IsString())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected string", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                const char *str = call.CopyString(value);
                if (RG_UNLIKELY(!str))
                    return env.Null();

                if (RG_LIKELY(param.gpr_count)) {
                    *(gpr_ptr++) = (uint64_t)str;
                } else {
                    args_ptr = AlignUp(args_ptr, 4);
                    *(const char **)args_ptr = str;
                    args_ptr += 4;
                }
            } break;
            case PrimitiveKind::Pointer: {
                if (RG_UNLIKELY(!CheckValueTag(instance, value, param.type))) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected %3", GetValueType(instance, value), i + 1, param.type->name);
                    return env.Null();
                }

                void *ptr = value.As<Napi::External<void>>();

                if (RG_LIKELY(param.gpr_count)) {
                    *(gpr_ptr++) = (uint64_t)ptr;
                } else {
                    args_ptr = AlignUp(args_ptr, 4);
                    *(void **)args_ptr = ptr;
                    args_ptr += 4;
                }
            } break;

            case PrimitiveKind::Record: {
                if (RG_UNLIKELY(!value.IsObject())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected object", GetValueType(instance, value), i + 1);
                    return env.Null();
                }

                Napi::Object obj = value.As<Napi::Object>();

                if (param.vec_count) {
                    if (!PushHFA(obj, param.type, (uint8_t *)vec_ptr))
                        return env.Null();
                    vec_ptr += param.vec_count;
                } else {
                    if (param.gpr_count) {
                        RG_ASSERT(param.type->align <= 8);

                        if (!call.PushObject(obj, param.type, (uint8_t *)gpr_ptr))
                            return env.Null();

                        gpr_ptr += param.gpr_count;
                        args_ptr += AlignLen(param.type->size - param.gpr_count * 4, 4);
                    } else if (param.type->size) {
                        int16_t align = (param.type->align <= 4) ? 4 : 8;

                        args_ptr = AlignUp(args_ptr, align);
                        if (!call.PushObject(obj, param.type, args_ptr))
                            return env.Null();
                        args_ptr += AlignLen(param.type->size, 4);
                    }
                }
            } break;
        }
    }

    if (instance->debug) {
        call.DumpDebug();
    }

#define PERFORM_CALL(Suffix) \
        (func->forward_fp ? ForwardCallX ## Suffix(func->func, call.GetSP()) \
                          : ForwardCall ## Suffix(func->func, call.GetSP()))

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
                uint64_t ret = PERFORM_CALL(GG);
                uint32_t r0 = (uint32_t)ret;

                Napi::Object obj = PopObject(env, (const uint8_t *)&r0, func->ret.type);
                return obj;
            } else if (func->ret.vec_count) {
                HfaRet ret = PERFORM_CALL(DDDD);

                Napi::Object obj = PopHFA(env, (const uint8_t *)&ret, func->ret.type);
                return obj;
            } else if (func->ret.type->size) {
                RG_ASSERT(return_ptr);

                uint64_t ret = PERFORM_CALL(GG);
                uint64_t r0 = (uint32_t)ret;
                RG_ASSERT(r0 == (uint64_t)return_ptr);

                Napi::Object obj = PopObject(env, return_ptr, func->ret.type);
                return obj;
            } else {
                PERFORM_CALL(GG);

                Napi::Object obj = Napi::Object::New(env);
                return obj;
            }
        } break;

        default: {
            uint64_t ret = PERFORM_CALL(GG);
            uint32_t r0 = (uint32_t)ret;

            switch (func->ret.type->primitive) {
                case PrimitiveKind::Void: return env.Null();
                case PrimitiveKind::Bool: return Napi::Boolean::New(env, r0);
                case PrimitiveKind::Int8: return Napi::Number::New(env, (double)r0);
                case PrimitiveKind::UInt8: return Napi::Number::New(env, (double)r0);
                case PrimitiveKind::Int16: return Napi::Number::New(env, (double)r0);
                case PrimitiveKind::UInt16: return Napi::Number::New(env, (double)r0);
                case PrimitiveKind::Int32: return Napi::Number::New(env, (double)r0);
                case PrimitiveKind::UInt32: return Napi::Number::New(env, (double)r0);
                case PrimitiveKind::Int64: return Napi::BigInt::New(env, (int64_t)ret);
                case PrimitiveKind::UInt64: return Napi::BigInt::New(env, ret);
                case PrimitiveKind::Float32: { RG_UNREACHABLE(); } break;
                case PrimitiveKind::Float64: { RG_UNREACHABLE(); } break;
                case PrimitiveKind::String: return Napi::String::New(env, (const char *)r0);
                case PrimitiveKind::Pointer: {
                    void *ptr = (void *)r0;

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
