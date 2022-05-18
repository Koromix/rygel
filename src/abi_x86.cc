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

#if defined(__i386__) || defined(_M_IX86)

#include "vendor/libcc/libcc.hh"
#include "ffi.hh"
#include "call.hh"
#include "util.hh"

#include <napi.h>

namespace RG {

extern "C" uint64_t ForwardCallG(const void *func, uint8_t *sp);
extern "C" float ForwardCallF(const void *func, uint8_t *sp);
extern "C" double ForwardCallD(const void *func, uint8_t *sp);
extern "C" uint64_t ForwardCallRG(const void *func, uint8_t *sp);
extern "C" float ForwardCallRF(const void *func, uint8_t *sp);
extern "C" double ForwardCallRD(const void *func, uint8_t *sp);

static inline bool IsRegular(Size size)
{
    bool regular = (size <= 8 && !(size & (size - 1)));
    return regular;
}

bool AnalyseFunction(InstanceData *instance, FunctionInfo *func)
{
    int fast = (func->convention == CallConvention::Fastcall) ? 2 :
               (func->convention == CallConvention::Thiscall) ? 1 : 0;
    func->fast = fast;

    if (func->ret.type->primitive != PrimitiveKind::Record) {
        func->ret.trivial = true;
#if defined(_WIN32) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    } else {
        func->ret.trivial = IsRegular(func->ret.type->size);
#endif
    }
#ifndef _WIN32
    if (fast && !func->ret.trivial) {
        func->ret.fast = true;
        fast--;
    }
#endif

    Size params_size = 0;
    for (ParameterInfo &param: func->parameters) {
        if (fast && param.type->size <= 4) {
            param.fast = true;
            fast--;
        }

        params_size += std::max((int16_t)4, param.type->size);
    }
    func->args_size = params_size + 4 * !func->ret.trivial;

    switch (func->convention) {
        case CallConvention::Cdecl: {
            func->decorated_name = Fmt(&instance->str_alloc, "_%1", func->name).ptr;
        } break;
        case CallConvention::Stdcall: {
            RG_ASSERT(!func->variadic);
            func->decorated_name = Fmt(&instance->str_alloc, "_%1@%2", func->name, params_size).ptr;
        } break;
        case CallConvention::Fastcall: {
            RG_ASSERT(!func->variadic);
            func->decorated_name = Fmt(&instance->str_alloc, "@%1@%2", func->name, params_size).ptr;
            func->args_size += 16;
        } break;
        case CallConvention::Thiscall: {
            RG_ASSERT(!func->variadic);
            func->args_size += 16;
        } break;
    }

    return true;
}

bool CallData::Prepare(const Napi::CallbackInfo &info)
{
    uint32_t *args_ptr = nullptr;
    uint32_t *fast_ptr = nullptr;

    // Pass return value in register or through memory
    if (RG_UNLIKELY(!AllocStack(func->args_size, 16, &args_ptr)))
        return false;
    if (func->fast) {
        fast_ptr = args_ptr;
        args_ptr += 4;
    }
    if (!func->ret.trivial) {
        if (RG_UNLIKELY(!AllocHeap(func->ret.type->size, 16, &return_ptr)))
            return false;
        *((func->ret.fast ? fast_ptr : args_ptr)++) = (uint32_t)return_ptr;
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
                *(bool *)((param.fast ? fast_ptr : args_ptr)++) = b;
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

                int32_t v = CopyNumber<int32_t>(value);
                *((param.fast ? fast_ptr : args_ptr)++) = (uint32_t)v;
            } break;
            case PrimitiveKind::Int64:
            case PrimitiveKind::UInt64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return false;
                }

                int64_t v = CopyNumber<int64_t>(value);
                *(uint64_t *)args_ptr = (uint64_t)v;
                args_ptr += 2;
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

                *(const char **)((param.fast ? fast_ptr : args_ptr)++) = str;
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

                *(const char16_t **)((param.fast ? fast_ptr : args_ptr)++) = str16;
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

                *(uint8_t **)((param.fast ? fast_ptr : args_ptr)++) = ptr;
            } break;
            case PrimitiveKind::Record: {
                if (RG_UNLIKELY(!IsObject(value))) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected object", GetValueType(instance, value), i + 1);
                    return false;
                }

                Napi::Object obj = value.As<Napi::Object>();

                if (param.fast) {
                    uint8_t *ptr = (uint8_t *)(fast_ptr++);
                    if (!PushObject(obj, param.type, ptr))
                        return false;
                } else {
                    uint8_t *ptr = (uint8_t *)AlignUp(args_ptr, param.type->align);
                    if (!PushObject(obj, param.type, ptr))
                        return false;
                    args_ptr = (uint32_t *)AlignUp(ptr + param.type->size, 4);
                }
            } break;
            case PrimitiveKind::Array: { RG_UNREACHABLE(); } break;
            case PrimitiveKind::Float32: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return false;
                }

                float f = CopyNumber<float>(value);
                *(float *)((param.fast ? fast_ptr : args_ptr)++) = f;
            } break;
            case PrimitiveKind::Float64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected number", GetValueType(instance, value), i + 1);
                    return false;
                }

                double d = CopyNumber<double>(value);
                *(double *)args_ptr = d;
                args_ptr += 2;
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
            auto ret = (func->fast ? ForwardCallR ## Suffix(func->func, stack.ptr) \
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
        case PrimitiveKind::Pointer:
        case PrimitiveKind::Record: { result.u64 = PERFORM_CALL(G); } break;
        case PrimitiveKind::Array: { RG_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: { result.f = PERFORM_CALL(F); } break;
        case PrimitiveKind::Float64: { result.d = PERFORM_CALL(D); } break;
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
        case PrimitiveKind::Array: { RG_UNREACHABLE(); } break;
        case PrimitiveKind::Float32: return Napi::Number::New(env, (double)result.f);
        case PrimitiveKind::Float64: return Napi::Number::New(env, result.d);
    }

    RG_UNREACHABLE();
}

}

#endif
