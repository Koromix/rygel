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

#include "vendor/libcc/libcc.hh"
#include "call.hh"
#include "ffi.hh"
#include "util.hh"

#include <napi.h>

namespace RG {

CallData::CallData(Napi::Env env, InstanceData *instance, const FunctionInfo *func)
    : env(env), instance(instance), func(func),
      stack_mem(&instance->stack_mem), heap_mem(&instance->heap_mem),
      old_stack_mem(instance->stack_mem), old_heap_mem(instance->heap_mem)
{
    RG_ASSERT(AlignUp(stack_mem->ptr, 16) == stack_mem->ptr);
    RG_ASSERT(AlignUp(stack_mem->end(), 16) == stack_mem->end());
}

CallData::~CallData()
{
    instance->stack_mem = old_stack_mem;
    instance->heap_mem = old_heap_mem;
}

const char *CallData::PushString(const Napi::Value &value)
{
    RG_ASSERT(value.IsString());

    Napi::Env env = value.Env();

    Span<char> buf;
    size_t len = 0;
    napi_status status;

    buf.ptr = (char *)heap_mem->ptr;
    buf.len = std::max((Size)0, heap_mem->len - Kibibytes(32));

    status = napi_get_value_string_utf8(env, value, buf.ptr, (size_t)buf.len, &len);
    RG_ASSERT(status == napi_ok);

    len++;

    if (RG_LIKELY(len < (size_t)buf.len)) {
        heap_mem->ptr += (Size)len;
        heap_mem->len -= (Size)len;
    } else {
        status = napi_get_value_string_utf8(env, value, nullptr, 0, &len);
        RG_ASSERT(status == napi_ok);

        len++;

        buf.ptr = (char *)Allocator::Allocate(&big_alloc, (Size)len);
        buf.len = (Size)len;

        status = napi_get_value_string_utf8(env, value, buf.ptr, (size_t)buf.len, &len);
        RG_ASSERT(status == napi_ok);
    }

    return buf.ptr;
}

const char16_t *CallData::PushString16(const Napi::Value &value)
{
    RG_ASSERT(value.IsString());

    Napi::Env env = value.Env();

    Span<char16_t> buf;
    size_t len = 0;
    napi_status status;

    buf.ptr = (char16_t *)heap_mem->ptr;
    buf.len = std::max((Size)0, heap_mem->len - Kibibytes(32)) / 2;

    status = napi_get_value_string_utf16(env, value, buf.ptr, (size_t)buf.len, &len);
    RG_ASSERT(status == napi_ok);

    len++;

    if (RG_LIKELY(len < (size_t)buf.len)) {
        heap_mem->ptr += (Size)len * 2;
        heap_mem->len -= (Size)len * 2;
    } else {
        status = napi_get_value_string_utf16(env, value, nullptr, 0, &len);
        RG_ASSERT(status == napi_ok);

        len++;

        buf.ptr = (char16_t *)Allocator::Allocate(&big_alloc, (Size)len * 2);
        buf.len = (Size)len;

        status = napi_get_value_string_utf16(env, value, buf.ptr, (size_t)buf.len, &len);
        RG_ASSERT(status == napi_ok);
    }

    return buf.ptr;
}

bool CallData::PushObject(const Napi::Object &obj, const TypeInfo *type, uint8_t *dest)
{
    Napi::Env env = obj.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    RG_ASSERT(IsObject(obj));
    RG_ASSERT(type->primitive == PrimitiveKind::Record);

    for (const RecordMember &member: type->members) {
        Napi::Value value = obj.Get(member.name);

        if (RG_UNLIKELY(value.IsUndefined())) {
            ThrowError<Napi::TypeError>(env, "Missing expected object property '%1'", member.name);
            return false;
        }

        dest = AlignUp(dest, member.align);

        switch (member.type->primitive) {
            case PrimitiveKind::Void: { RG_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                if (RG_UNLIKELY(!value.IsBoolean())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected value %1 for member '%2', expected boolean", GetValueType(instance, value), member.name);
                    return false;
                }

                bool b = value.As<Napi::Boolean>();
                *(bool *)dest = b;
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
                    ThrowError<Napi::TypeError>(env, "Unexpected value %1 for member '%2', expected number", GetValueType(instance, value), member.name);
                    return false;
                }

                int64_t v = CopyNumber<int64_t>(value);
                memcpy(dest, &v, member.type->size); // Little Endian
            } break;
            case PrimitiveKind::String: {
                if (RG_UNLIKELY(!value.IsString())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected value %1 for member '%2', expected string", GetValueType(instance, value), member.name);
                    return false;
                }

                const char *str = PushString(value);
                if (RG_UNLIKELY(!str))
                    return false;
                *(const char **)dest = str;
            } break;
            case PrimitiveKind::String16: {
                if (RG_UNLIKELY(!value.IsString())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected value %1 for member '%2', expected string", GetValueType(instance, value), member.name);
                    return false;
                }

                const char16_t *str16 = PushString16(value);
                if (RG_UNLIKELY(!str16))
                    return false;
                *(const char16_t **)dest = str16;
            } break;
            case PrimitiveKind::Pointer: {
                if (RG_UNLIKELY(!CheckValueTag(instance, value, member.type))) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected %3", GetValueType(instance, value), member.name, member.type->name);
                    return false;
                }

                Napi::External external = value.As<Napi::External<void>>();
                void *ptr = external.Data();
                *(void **)dest = ptr;
            } break;
            case PrimitiveKind::Record: {
                if (RG_UNLIKELY(!IsObject(value))) {
                    ThrowError<Napi::TypeError>(env, "Unexpected value %1 for member '%2', expected object", GetValueType(instance, value), member.name);
                    return false;
                }

                Napi::Object obj = value.As<Napi::Object>();
                if (!PushObject(obj, member.type, dest))
                    return false;
            } break;
            case PrimitiveKind::Float32: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected value %1 for member '%2', expected number", GetValueType(instance, value), member.name);
                    return false;
                }

                float f = CopyNumber<float>(value);
                memcpy(dest, &f, 4);
            } break;
            case PrimitiveKind::Float64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected value %1 for member '%2', expected number", GetValueType(instance, value), member.name);
                    return false;
                }

                double d = CopyNumber<double>(value);
                memcpy(dest, &d, 8);
            } break;
        }

        dest += member.type->size;
    }

    return true;
}

void CallData::PopObject(Napi::Object obj, const uint8_t *ptr, const TypeInfo *type)
{
    Napi::Env env = obj.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    RG_ASSERT(type->primitive == PrimitiveKind::Record);

    for (const RecordMember &member: type->members) {
        ptr = AlignUp(ptr, member.align);

        switch (member.type->primitive) {
            case PrimitiveKind::Void: { RG_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                bool b = *(bool *)ptr;
                obj.Set(member.name, Napi::Boolean::New(env, b));
            } break;
            case PrimitiveKind::Int8: {
                double d = (double)*(int8_t *)ptr;
                obj.Set(member.name, Napi::Number::New(env, d));
            } break;
            case PrimitiveKind::UInt8: {
                double d = (double)*(uint8_t *)ptr;
                obj.Set(member.name, Napi::Number::New(env, d));
            } break;
            case PrimitiveKind::Int16: {
                double d = (double)*(int16_t *)ptr;
                obj.Set(member.name, Napi::Number::New(env, d));
            } break;
            case PrimitiveKind::UInt16: {
                double d = (double)*(uint16_t *)ptr;
                obj.Set(member.name, Napi::Number::New(env, d));
            } break;
            case PrimitiveKind::Int32: {
                double d = (double)*(int32_t *)ptr;
                obj.Set(member.name, Napi::Number::New(env, d));
            } break;
            case PrimitiveKind::UInt32: {
                double d = (double)*(uint32_t *)ptr;
                obj.Set(member.name, Napi::Number::New(env, d));
            } break;
            case PrimitiveKind::Int64: {
                int64_t v = *(int64_t *)ptr;
                obj.Set(member.name, Napi::BigInt::New(env, v));
            } break;
            case PrimitiveKind::UInt64: {
                uint64_t v = *(uint64_t *)ptr;
                obj.Set(member.name, Napi::BigInt::New(env, v));
            } break;
            case PrimitiveKind::String: {
                const char *str = *(const char **)ptr;
                obj.Set(member.name, Napi::String::New(env, str));
            } break;
            case PrimitiveKind::String16: {
                const char16_t *str16 = *(const char16_t **)ptr;
                obj.Set(member.name, Napi::String::New(env, str16));
            } break;
            case PrimitiveKind::Pointer: {
                void *ptr2 = *(void **)ptr;

                Napi::External<void> external = Napi::External<void>::New(env, ptr2);
                SetValueTag(instance, external, member.type);

                obj.Set(member.name, external);
            } break;
            case PrimitiveKind::Record: {
                Napi::Object obj2 = PopObject(ptr, member.type);
                obj.Set(member.name, obj2);
            } break;
            case PrimitiveKind::Float32: {
                float f;
                memcpy(&f, ptr, 4);
                obj.Set(member.name, Napi::Number::New(env, (double)f));
            } break;
            case PrimitiveKind::Float64: {
                double d;
                memcpy(&d, ptr, 8);
                obj.Set(member.name, Napi::Number::New(env, d));
            } break;
        }

        ptr += member.type->size;
    }
}

Napi::Object CallData::PopObject(const uint8_t *ptr, const TypeInfo *type)
{
    Napi::Object obj = Napi::Object::New(env);
    PopObject(obj, ptr, type);
    return obj;
}

Napi::Value CallData::Complete()
{
    for (const OutObject &obj: out_objects) {
        PopObject(obj.obj, obj.ptr, obj.type);
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

static void DumpMemory(const char *type, Span<const uint8_t> bytes)
{
    if (bytes.len) {
        PrintLn(stderr, "%1 at 0x%2 (%3):", type, bytes.ptr, FmtMemSize(bytes.len));

        for (const uint8_t *ptr = bytes.begin(); ptr < bytes.end();) {
            Print(stderr, "  [0x%1 %2 %3]  ", FmtArg(ptr).Pad0(-16),
                                              FmtArg((ptr - bytes.begin()) / sizeof(void *)).Pad(-4),
                                              FmtArg(ptr - bytes.begin()).Pad(-4));
            for (int i = 0; ptr < bytes.end() && i < (int)sizeof(void *); i++, ptr++) {
                Print(stderr, " %1", FmtHex(*ptr).Pad0(-2));
            }
            PrintLn(stderr);
        }
    }
}

void CallData::DumpDebug() const
{
    PrintLn(stderr, "%!..+---- %1 (%2) ----%!0", func->name, CallConventionNames[(int)func->convention]);

    if (func->parameters.len) {
        PrintLn(stderr, "Parameters:");
        for (Size i = 0; i < func->parameters.len; i++) {
            const ParameterInfo &param = func->parameters[i];
            PrintLn(stderr, "  %1 = %2 (%3)", i, param.type->name, FmtMemSize(param.type->size));
        }
    }
    PrintLn(stderr, "Return: %1 (%2)", func->ret.type->name, FmtMemSize(func->ret.type->size));

    DumpMemory("Stack", GetStack());
    DumpMemory("Heap", GetHeap());
}

}
