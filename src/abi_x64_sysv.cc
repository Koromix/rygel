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

#if defined(__x86_64__) && !defined(_WIN64)

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

extern "C" RaxRdxRet ForwardCallGG(const void *func, uint8_t *sp);
extern "C" float ForwardCallF(const void *func, uint8_t *sp);
extern "C" Xmm0RaxRet ForwardCallDG(const void *func, uint8_t *sp);
extern "C" RaxXmm0Ret ForwardCallGD(const void *func, uint8_t *sp);
extern "C" Xmm0Xmm1Ret ForwardCallDD(const void *func, uint8_t *sp);

extern "C" RaxRdxRet ForwardCallXGG(const void *func, uint8_t *sp);
extern "C" float ForwardCallXF(const void *func, uint8_t *sp);
extern "C" Xmm0RaxRet ForwardCallXDG(const void *func, uint8_t *sp);
extern "C" RaxXmm0Ret ForwardCallXGD(const void *func, uint8_t *sp);
extern "C" Xmm0Xmm1Ret ForwardCallXDD(const void *func, uint8_t *sp);

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
        case PrimitiveKind::Pointer: {
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
        case PrimitiveKind::Float32:
        case PrimitiveKind::Float64: {
            classes[0] = MergeClasses(classes[0], RegisterClass::SSE);
            return 1;
        } break;
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

    if (gpr_count <= gpr_avail && xmm_count <= xmm_avail){
        param->gpr_count = (int8_t)gpr_count;
        param->xmm_count = (int8_t)xmm_count;
        param->gpr_first = (classes[0] == RegisterClass::Integer);
    } else {
        param->use_memory = true;
    }
}

bool AnalyseFunction(InstanceData *, FunctionInfo *func)
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
    uint8_t *args_ptr = nullptr;
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
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argmument %2, expected boolean", GetValueType(instance, value), i + 1);
                    return false;
                }

                bool b = value.As<Napi::Boolean>();

                if (RG_LIKELY(param.gpr_count)) {
                    *(gpr_ptr++) = (uint64_t)b;
                } else {
                    *args_ptr = (uint8_t)b;
                    args_ptr += 8;
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
                    return false;
                }

                int64_t v = CopyNumber<int64_t>(value);

                if (RG_LIKELY(param.gpr_count)) {
                    *(gpr_ptr++) = (uint64_t)v;
                } else {
                    args_ptr = AlignUp(args_ptr, param.type->align);
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

                if (param.gpr_count || param.xmm_count) {
                    RG_ASSERT(param.type->size <= 16);

                    uint64_t buf[2] = {};
                    if (!PushObject(obj, param.type, (uint8_t *)buf))
                        return false;

                    if (param.gpr_first) {
                        uint64_t *ptr = buf;

                        *(gpr_ptr++) = *(ptr++);
                        if (param.gpr_count == 2) {
                            *(gpr_ptr++) = *(ptr++);
                        } else if (param.xmm_count == 1) {
                            *(xmm_ptr++) = *(ptr++);
                        }
                    } else {
                        uint64_t *ptr = buf;

                        *(xmm_ptr++) = *(ptr++);
                        if (param.xmm_count == 2) {
                            *(xmm_ptr++) = *(ptr++);
                        } else if (param.gpr_count == 1) {
                            *(gpr_ptr++) = *(ptr++);
                        }
                    }
                } else if (param.use_memory) {
                    args_ptr = AlignUp(args_ptr, param.type->align);
                    if (!PushObject(obj, param.type, args_ptr))
                        return false;
                    args_ptr += AlignLen(param.type->size, 8);
                }
            } break;
            case PrimitiveKind::Float32: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return false;
                }

                float f = CopyNumber<float>(value);

                if (RG_LIKELY(param.xmm_count)) {
                    memcpy(xmm_ptr++, &f, 4);
                } else {
                    args_ptr = AlignUp(args_ptr, 4);
                    memcpy(args_ptr, &f, 4);
                    args_ptr += 8;
                }
            } break;
            case PrimitiveKind::Float64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return false;
                }

                double d = CopyNumber<double>(value);

                if (RG_LIKELY(param.xmm_count)) {
                    memcpy(xmm_ptr++, &d, 8);
                } else {
                    args_ptr = AlignUp(args_ptr, 8);
                    memcpy(args_ptr, &d, 8);
                    args_ptr += 8;
                }
            } break;
        }
    }

    stack = MakeSpan(mem->stack.end(), old_stack_mem.end() - mem->stack.end());
    heap = MakeSpan(old_heap_mem.ptr, mem->heap.ptr - old_heap_mem.ptr);

    return true;
}

void CallData::Execute()
{
#define PERFORM_CALL(Suffix) \
        ([&]() { \
            auto ret = (func->forward_fp ? ForwardCallX ## Suffix(func->func, stack.ptr) \
                                         : ForwardCall ## Suffix(func->func, stack.ptr)); \
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
        case PrimitiveKind::Pointer: { result.u64 = PERFORM_CALL(GG).rax; } break;
        case PrimitiveKind::Record: {
            if (func->ret.gpr_first && !func->ret.xmm_count) {
                RaxRdxRet ret = PERFORM_CALL(GG);
                memcpy_safe(&result.buf, &ret, RG_SIZE(ret));
            } else if (func->ret.gpr_first) {
                RaxXmm0Ret ret = PERFORM_CALL(GD);
                memcpy_safe(&result.buf, &ret, RG_SIZE(ret));
            } else if (func->ret.xmm_count == 2) {
                Xmm0Xmm1Ret ret = PERFORM_CALL(DD);
                memcpy_safe(&result.buf, &ret, RG_SIZE(ret));
            } else {
                Xmm0RaxRet ret = PERFORM_CALL(DG);
                memcpy_safe(&result.buf, &ret, RG_SIZE(ret));
            }
        } break;
        case PrimitiveKind::Float32: { result.f = PERFORM_CALL(F); } break;
        case PrimitiveKind::Float64: { result.d = PERFORM_CALL(DG).xmm0; } break;
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
        case PrimitiveKind::Pointer: {
            Napi::External<void> external = Napi::External<void>::New(env, result.ptr);
            SetValueTag(instance, external, func->ret.type);

            return external;
        } break;
        case PrimitiveKind::Record: {
            const uint8_t *ptr = return_ptr ? (const uint8_t *)return_ptr
                                            : (const uint8_t *)&result.buf;

            Napi::Object obj = PopObject(ptr, func->ret.type);
            return obj;
        } break;
        case PrimitiveKind::Float32: return Napi::Number::New(env, (double)result.f);
        case PrimitiveKind::Float64: return Napi::Number::New(env, result.d);
    }

    RG_UNREACHABLE();
}

}

#endif
