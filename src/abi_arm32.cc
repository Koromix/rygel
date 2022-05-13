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
    } else if (func->ret.type->primitive != PrimitiveKind::Record ||
               func->ret.type->size <= 4) {
        func->ret.gpr_count = (func->ret.type->size > 4) ? 2 : 1;
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
            case PrimitiveKind::String16:
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
                bool vfp = !param.variadic;

                if (vfp) {
                    if (need <= vec_avail) {
                        param.vec_count = need;
                        vec_avail -= need;
                    } else {
                        started_stack = true;
                    }
                } else {
                    if (need <= gpr_avail) {
                        param.gpr_count = need;
                        gpr_avail -= need;
                    } else {
                        started_stack = true;
                    }
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
                } else {
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

    RG_ASSERT(IsObject(obj));
    RG_ASSERT(IsHFA(type));
    RG_ASSERT(type->primitive == PrimitiveKind::Record);
    RG_ASSERT(AlignUp(dest, type->members[0].type->size) == dest);

    bool float32 = (type->members[0].type->primitive == PrimitiveKind::Float32);

    for (const RecordMember &member: type->members) {
        Napi::Value value = obj.Get(member.name);

        if (!value.IsNumber() && !value.IsBigInt()) {
            ThrowError<Napi::TypeError>(env, "Unexpected value %1 for member '%2', expected number", GetValueType(instance, value), member.name);
            return false;
        }

        if (float32) {
            *(float *)dest = CopyNumber<float>(value);
            dest += 4;
        } else {
            *(double *)dest = CopyNumber<double>(value);
            dest += 8;
        }
    }

    return true;
}

bool CallData::Prepare(const Napi::CallbackInfo &info)
{
    uint8_t *args_ptr = nullptr;
    uint32_t *gpr_ptr = nullptr;
    uint32_t *vec_ptr = nullptr;

    // Unlike other call conventions, here we put the general-purpose
    // registers just before the stack (so behind the vector ones).
    // In the armv7hf calling convention, some arguments can end up
    // partially in GPR, partially in the stack.
    if (RG_UNLIKELY(!AllocStack(func->args_size, 16, &args_ptr)))
        return false;
    if (RG_UNLIKELY(!AllocStack(4 * 4, 8, &gpr_ptr)))
        return false;
    if (RG_UNLIKELY(!AllocStack(8 * 8, 8, &vec_ptr)))
        return false;
    if (func->ret.use_memory) {
        if (RG_UNLIKELY(!AllocHeap(func->ret.type->size, 16, &return_ptr)))
            return false;
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
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected boolean", GetValueType(instance, value), i + 1);
                    return false;
                }

                bool b = value.As<Napi::Boolean>();

                if (RG_LIKELY(param.gpr_count)) {
                    *(gpr_ptr++) = (uint32_t)b;
                } else {
                    *args_ptr = (uint8_t)b;
                    args_ptr += 4;
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
                    return false;
                }

                int64_t v = CopyNumber<int64_t>(value);

                if (RG_LIKELY(param.gpr_count)) {
                    *(gpr_ptr++) = (uint32_t)v;
                } else {
                    memcpy(args_ptr, &v, param.type->size); // Little Endian
                    args_ptr += 4;
                }
            } break;
            case PrimitiveKind::Int64:
            case PrimitiveKind::UInt64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return false;
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
                    *(const char **)args_ptr = str;
                    args_ptr += 4;
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
                    *(const char16_t **)args_ptr = str16;
                    args_ptr += 4;
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
                        OutObject out = {obj, ptr, param.type->ref};
                        out_objects.Append(out);
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
                    *(void **)args_ptr = ptr;
                    args_ptr += 4;
                }
            } break;
            case PrimitiveKind::Record: {
                if (RG_UNLIKELY(!IsObject(value))) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected object", GetValueType(instance, value), i + 1);
                    return false;
                }

                Napi::Object obj = value.As<Napi::Object>();

                if (param.vec_count) {
                    if (!PushHFA(obj, param.type, (uint8_t *)vec_ptr))
                        return false;
                    vec_ptr += param.vec_count;
                } else {
                    if (param.gpr_count) {
                        RG_ASSERT(param.type->align <= 8);

                        if (!PushObject(obj, param.type, (uint8_t *)gpr_ptr))
                            return false;

                        gpr_ptr += param.gpr_count;
                        args_ptr += AlignLen(param.type->size - param.gpr_count * 4, 4);
                    } else if (param.type->size) {
                        int16_t align = (param.type->align <= 4) ? 4 : 8;

                        args_ptr = AlignUp(args_ptr, align);
                        if (!PushObject(obj, param.type, args_ptr))
                            return false;
                        args_ptr += AlignLen(param.type->size, 4);
                    }
                }
            } break;
            case PrimitiveKind::Float32: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return false;
                }

                float f = CopyNumber<float>(value);

                if (RG_LIKELY(param.vec_count)) {
                    memcpy(vec_ptr++, &f, 4);
                } else if (param.gpr_count) {
                    memcpy(gpr_ptr++, &f, 4);
                } else {
                    memcpy(args_ptr, &f, 4);
                    args_ptr += 4;
                }
            } break;
            case PrimitiveKind::Float64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return false;
                }

                double d = CopyNumber<double>(value);

                if (RG_LIKELY(param.vec_count)) {
                    memcpy(vec_ptr, &d, 8);
                    vec_ptr += 2;
                } else if (param.gpr_count) {
                    memcpy(gpr_ptr, &d, 8);
                    gpr_ptr += 2;
                } else {
                    args_ptr = AlignUp(args_ptr, 8);
                    memcpy(args_ptr, &d, 8);
                    args_ptr += 8;
                }
            } break;
        }
    }

    return true;
}

void CallData::Execute()
{
#define PERFORM_CALL(Suffix) \
        ([&]() { \
            auto ret = (func->forward_fp ? ForwardCallX ## Suffix(func->func, GetSP()) \
                                         : ForwardCall ## Suffix(func->func, GetSP())); \
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
        case PrimitiveKind::Pointer: { result.u64 = PERFORM_CALL(GG); } break;
        case PrimitiveKind::Record: {
            if (func->ret.vec_count) {
                HfaRet ret = PERFORM_CALL(DDDD);
                memcpy_safe(&result.buf, &ret, RG_SIZE(ret));
            } else {
                result.u64 = PERFORM_CALL(GG);
            }
        } break;
        case PrimitiveKind::Float32: { result.f = PERFORM_CALL(F); } break;
        case PrimitiveKind::Float64: { result.d = PERFORM_CALL(DDDD).d0; } break;
    }

#undef PERFORM_CALL
}

}

#endif
