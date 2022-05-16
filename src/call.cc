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

CallData::CallData(Napi::Env env, InstanceData *instance, const FunctionInfo *func, InstanceMemory *mem)
    : env(env), instance(instance), func(func), debug(instance->debug),
      mem(mem), old_stack_mem(mem->stack), old_heap_mem(mem->heap)
{
    mem->depth++;

    RG_ASSERT(AlignUp(mem->stack.ptr, 16) == mem->stack.ptr);
    RG_ASSERT(AlignUp(mem->stack.end(), 16) == mem->stack.end());
}

CallData::~CallData()
{
    mem->stack = old_stack_mem;
    mem->heap = old_heap_mem;

    if (!--mem->depth && mem->temporary) {
        delete mem;
    }
}

const char *CallData::PushString(const Napi::Value &value)
{
    RG_ASSERT(value.IsString());

    Napi::Env env = value.Env();

    Span<char> buf;
    size_t len = 0;
    napi_status status;

    buf.ptr = (char *)mem->heap.ptr;
    buf.len = std::max((Size)0, mem->heap.len - Kibibytes(32));

    status = napi_get_value_string_utf8(env, value, buf.ptr, (size_t)buf.len, &len);
    RG_ASSERT(status == napi_ok);

    len++;

    if (RG_LIKELY(len < (size_t)buf.len)) {
        mem->heap.ptr += (Size)len;
        mem->heap.len -= (Size)len;
    } else {
        status = napi_get_value_string_utf8(env, value, nullptr, 0, &len);
        RG_ASSERT(status == napi_ok);

        len++;

        buf.ptr = (char *)Allocator::Allocate(&call_alloc, (Size)len);
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

    buf.ptr = (char16_t *)mem->heap.ptr;
    buf.len = std::max((Size)0, mem->heap.len - Kibibytes(32)) / 2;

    status = napi_get_value_string_utf16(env, value, buf.ptr, (size_t)buf.len, &len);
    RG_ASSERT(status == napi_ok);

    len++;

    if (RG_LIKELY(len < (size_t)buf.len)) {
        mem->heap.ptr += (Size)len * 2;
        mem->heap.len -= (Size)len * 2;
    } else {
        status = napi_get_value_string_utf16(env, value, nullptr, 0, &len);
        RG_ASSERT(status == napi_ok);

        len++;

        buf.ptr = (char16_t *)Allocator::Allocate(&call_alloc, (Size)len * 2);
        buf.len = (Size)len;

        status = napi_get_value_string_utf16(env, value, buf.ptr, (size_t)buf.len, &len);
        RG_ASSERT(status == napi_ok);
    }

    return buf.ptr;
}

bool CallData::PushObject(const Napi::Object &obj, const TypeInfo *type, uint8_t *dest, int16_t realign)
{
    RG_ASSERT(IsObject(obj));
    RG_ASSERT(type->primitive == PrimitiveKind::Record);

    for (const RecordMember &member: type->members) {
        Napi::Value value = obj.Get(member.name);

        if (RG_UNLIKELY(value.IsUndefined())) {
            ThrowError<Napi::TypeError>(env, "Missing expected object property '%1'", member.name);
            return false;
        }

        int16_t align = std::max(member.align, realign);
        dest = AlignUp(dest, align);

        switch (member.type->primitive) {
            case PrimitiveKind::Void: { RG_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                if (RG_UNLIKELY(!value.IsBoolean())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected boolean", GetValueType(instance, value), member.name);
                    return false;
                }

                bool b = value.As<Napi::Boolean>();
                *(bool *)dest = b;
            } break;
            case PrimitiveKind::Int8: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected number", GetValueType(instance, value), member.name);
                    return false;
                }

                int8_t v = CopyNumber<int8_t>(value);
                *(int8_t *)dest = v;
            } break;
            case PrimitiveKind::UInt8: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected number", GetValueType(instance, value), member.name);
                    return false;
                }

                uint8_t v = CopyNumber<uint8_t>(value);
                *(uint8_t *)dest = v;
            } break;
            case PrimitiveKind::Int16: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected number", GetValueType(instance, value), member.name);
                    return false;
                }

                int16_t v = CopyNumber<int16_t>(value);
                *(int16_t *)dest = v;
            } break;
            case PrimitiveKind::UInt16: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected number", GetValueType(instance, value), member.name);
                    return false;
                }

                uint16_t v = CopyNumber<uint16_t>(value);
                *(uint16_t *)dest = v;
            } break;
            case PrimitiveKind::Int32: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected number", GetValueType(instance, value), member.name);
                    return false;
                }

                int32_t v = CopyNumber<int32_t>(value);
                *(int32_t *)dest = v;
            } break;
            case PrimitiveKind::UInt32: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected number", GetValueType(instance, value), member.name);
                    return false;
                }

                uint32_t v = CopyNumber<uint32_t>(value);
                *(uint32_t *)dest = v;
            } break;
            case PrimitiveKind::Int64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected number", GetValueType(instance, value), member.name);
                    return false;
                }

                int64_t v = CopyNumber<int64_t>(value);
                *(int64_t *)dest = v;
            } break;
            case PrimitiveKind::UInt64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected number", GetValueType(instance, value), member.name);
                    return false;
                }

                uint64_t v = CopyNumber<uint64_t>(value);
                *(uint64_t *)dest = v;
            } break;
            case PrimitiveKind::String: {
                if (RG_UNLIKELY(!value.IsString())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected string", GetValueType(instance, value), member.name);
                    return false;
                }

                const char *str = PushString(value);
                if (RG_UNLIKELY(!str))
                    return false;
                *(const char **)dest = str;
            } break;
            case PrimitiveKind::String16: {
                if (RG_UNLIKELY(!value.IsString())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected string", GetValueType(instance, value), member.name);
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
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected object", GetValueType(instance, value), member.name);
                    return false;
                }

                Napi::Object obj = value.As<Napi::Object>();
                if (!PushObject(obj, member.type, dest, realign))
                    return false;
            } break;
            case PrimitiveKind::Array: {
                if (RG_UNLIKELY(!value.IsArray() && !value.IsTypedArray())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected value %1 for member '%2', expected array", GetValueType(instance, value), member.name);
                    return false;
                }

                Napi::Array array = value.As<Napi::Array>();
                if (!PushArray(array, member.type, dest, realign))
                    return false;
            } break;
            case PrimitiveKind::Float32: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected number", GetValueType(instance, value), member.name);
                    return false;
                }

                float f = CopyNumber<float>(value);
                *(float *)dest = f;
            } break;
            case PrimitiveKind::Float64: {
                if (RG_UNLIKELY(!value.IsNumber() && !value.IsBigInt())) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected number", GetValueType(instance, value), member.name);
                    return false;
                }

                double d = CopyNumber<double>(value);
                *(double *)dest = d;
            } break;
        }

        dest += member.type->size;
    }

    return true;
}

bool CallData::PushArray(const Napi::Object &obj, const TypeInfo *type, uint8_t *dest, int16_t realign)
{
    RG_ASSERT(obj.IsArray() || obj.IsTypedArray());
    RG_ASSERT(type->primitive == PrimitiveKind::Array);

    uint32_t len = type->size / type->ref->size;

    if (obj.IsArray()) {
        Napi::Array array = obj.As<Napi::Array>();

        if (RG_UNLIKELY(array.Length() != len)) {
            ThrowError<Napi::Error>(env, "Expected array of length %1, got %2", len, array.Length());
            return false;
        }

#define PUSH_ARRAY(Check, Expected, GetCode) \
            do { \
                for (uint32_t i = 0; i < len; i++) { \
                    Napi::Value value = array[i]; \
                     \
                    int16_t align = std::max(type->ref->align, realign); \
                    dest = AlignUp(dest, align); \
                     \
                    if (RG_UNLIKELY(!(Check))) { \
                        ThrowError<Napi::TypeError>(env, "Unexpected value %1 in array, expected %2", GetValueType(instance, value), (Expected)); \
                        return false; \
                    } \
                     \
                    GetCode \
                     \
                    dest += type->ref->size; \
                } \
            } while (false)

        switch (type->ref->primitive) {
            case PrimitiveKind::Void: { RG_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                PUSH_ARRAY(value.IsBoolean(), "boolean", {
                    bool b = value.As<Napi::Boolean>();
                    *(bool *)dest = b;
                });
            } break;
            case PrimitiveKind::Int8: {
                PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                    int8_t v = CopyNumber<int8_t>(value);
                    *(int8_t *)dest = v;
                });
            } break;
            case PrimitiveKind::UInt8: {
                PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                    uint8_t v = CopyNumber<uint8_t>(value);
                    *(uint8_t *)dest = v;
                });
            } break;
            case PrimitiveKind::Int16: {
                PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                    int16_t v = CopyNumber<int16_t>(value);
                    *(int16_t *)dest = v;
                });
            } break;
            case PrimitiveKind::UInt16: {
                PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                    uint16_t v = CopyNumber<uint16_t>(value);
                    *(uint16_t *)dest = v;
                });
            } break;
            case PrimitiveKind::Int32: {
                PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                    int32_t v = CopyNumber<int32_t>(value);
                    *(int32_t *)dest = v;
                });
            } break;
            case PrimitiveKind::UInt32: {
                PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                    uint32_t v = CopyNumber<uint32_t>(value);
                    *(uint32_t *)dest = v;
                });
            } break;
            case PrimitiveKind::Int64: {
                PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                    int64_t v = CopyNumber<int64_t>(value);
                    *(int64_t *)dest = v;
                });
            } break;
            case PrimitiveKind::UInt64: {
                PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                    uint64_t v = CopyNumber<uint64_t>(value);
                    *(uint64_t *)dest = v;
                });
            } break;
            case PrimitiveKind::String: {
                PUSH_ARRAY(value.IsString(), "string", {
                    const char *str = PushString(value);
                    if (RG_UNLIKELY(!str))
                        return false;
                    *(const char **)dest = str;
                });
            } break;
            case PrimitiveKind::String16: {
                PUSH_ARRAY(value.IsString(), "string", {
                    const char16_t *str16 = PushString16(value);
                    if (RG_UNLIKELY(!str16))
                        return false;
                    *(const char16_t **)dest = str16;
                });
            } break;
            case PrimitiveKind::Pointer: {
                PUSH_ARRAY(CheckValueTag(instance, value, type->ref), type->ref->name, {
                    Napi::External external = value.As<Napi::External<void>>();
                    *(void **)dest = external.Data();
                });
            } break;
            case PrimitiveKind::Record: {
                PUSH_ARRAY(IsObject(value), "object", {
                    Napi::Object obj = value.As<Napi::Object>();
                    if (!PushObject(obj, type->ref, dest, realign))
                        return false;
                });
            } break;
            case PrimitiveKind::Array: {
                PUSH_ARRAY(value.IsArray() || value.IsTypedArray(), "array", {
                    Napi::Object array = value.As<Napi::Array>();
                    if (!PushArray(array, type->ref, dest, realign))
                        return false;
                });
            } break;
            case PrimitiveKind::Float32: {
                PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                    float f = CopyNumber<float>(value);
                    *(float *)dest = f;
                });
            } break;
            case PrimitiveKind::Float64: {
                PUSH_ARRAY(value.IsNumber() || value.IsBigInt(), "number", {
                    double d = CopyNumber<double>(value);
                    *(double *)dest = d;
                });
            } break;
        }

#undef PUSH_ARRAY
    } else {
        Napi::TypedArray array = obj.As<Napi::TypedArray>();
        const uint8_t *buf = (const uint8_t *)array.ArrayBuffer().Data();

        if (RG_UNLIKELY(array.ElementLength() != len)) {
            ThrowError<Napi::Error>(env, "Expected array of length %1, got %2", len, array.ElementLength());
            return false;
        }

        bool match;
        switch (type->ref->primitive) {
            case PrimitiveKind::Int8: { match = (array.TypedArrayType() == napi_int8_array); } break;
            case PrimitiveKind::UInt8: { match = (array.TypedArrayType() == napi_uint8_array); } break;
            case PrimitiveKind::Int16: { match = (array.TypedArrayType() == napi_int16_array); } break;
            case PrimitiveKind::UInt16: { match = (array.TypedArrayType() == napi_uint16_array); } break;
            case PrimitiveKind::Int32: { match = (array.TypedArrayType() == napi_int32_array); } break;
            case PrimitiveKind::UInt32: { match = (array.TypedArrayType() == napi_uint32_array); } break;
            case PrimitiveKind::Float32: { match = (array.TypedArrayType() == napi_float32_array); } break;
            case PrimitiveKind::Float64: { match = (array.TypedArrayType() == napi_float64_array); } break;

            default: { match = false; } break;
        }
        if (RG_UNLIKELY(!match)) {
            ThrowError<Napi::TypeError>(env, "TypedArray is not approriate for %1 array", type->ref->name);
            return false;
        }

        for (uint32_t i = 0; i < len; i++) {
            int16_t align = std::max(type->ref->align, realign);
            dest = AlignUp(dest, align);

            memcpy(dest, buf + i * type->ref->size, type->ref->size);

            dest += type->ref->size;
        }
    }

    return true;
}

void CallData::PopObject(Napi::Object obj, const uint8_t *src, const TypeInfo *type, int16_t realign)
{
    RG_ASSERT(type->primitive == PrimitiveKind::Record);

    for (const RecordMember &member: type->members) {
        int16_t align = std::max(realign, member.align);
        src = AlignUp(src, align);

        switch (member.type->primitive) {
            case PrimitiveKind::Void: { RG_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                bool b = *(bool *)src;
                obj.Set(member.name, Napi::Boolean::New(env, b));
            } break;
            case PrimitiveKind::Int8: {
                double d = (double)*(int8_t *)src;
                obj.Set(member.name, Napi::Number::New(env, d));
            } break;
            case PrimitiveKind::UInt8: {
                double d = (double)*(uint8_t *)src;
                obj.Set(member.name, Napi::Number::New(env, d));
            } break;
            case PrimitiveKind::Int16: {
                double d = (double)*(int16_t *)src;
                obj.Set(member.name, Napi::Number::New(env, d));
            } break;
            case PrimitiveKind::UInt16: {
                double d = (double)*(uint16_t *)src;
                obj.Set(member.name, Napi::Number::New(env, d));
            } break;
            case PrimitiveKind::Int32: {
                double d = (double)*(int32_t *)src;
                obj.Set(member.name, Napi::Number::New(env, d));
            } break;
            case PrimitiveKind::UInt32: {
                double d = (double)*(uint32_t *)src;
                obj.Set(member.name, Napi::Number::New(env, d));
            } break;
            case PrimitiveKind::Int64: {
                int64_t v = *(int64_t *)src;
                obj.Set(member.name, Napi::BigInt::New(env, v));
            } break;
            case PrimitiveKind::UInt64: {
                uint64_t v = *(uint64_t *)src;
                obj.Set(member.name, Napi::BigInt::New(env, v));
            } break;
            case PrimitiveKind::String: {
                const char *str = *(const char **)src;
                obj.Set(member.name, Napi::String::New(env, str));
            } break;
            case PrimitiveKind::String16: {
                const char16_t *str16 = *(const char16_t **)src;
                obj.Set(member.name, Napi::String::New(env, str16));
            } break;
            case PrimitiveKind::Pointer: {
                void *ptr2 = *(void **)src;

                Napi::External<void> external = Napi::External<void>::New(env, ptr2);
                SetValueTag(instance, external, member.type);

                obj.Set(member.name, external);
            } break;
            case PrimitiveKind::Record: {
                Napi::Object obj2 = PopObject(src, member.type, realign);
                obj.Set(member.name, obj2);
            } break;
            case PrimitiveKind::Array: {
                Napi::Object obj2 = PopArray(src, member.type, realign);
                obj.Set(member.name, obj2);
            } break;
            case PrimitiveKind::Float32: {
                float f;
                memcpy(&f, src, 4);
                obj.Set(member.name, Napi::Number::New(env, (double)f));
            } break;
            case PrimitiveKind::Float64: {
                double d;
                memcpy(&d, src, 8);
                obj.Set(member.name, Napi::Number::New(env, d));
            } break;
        }

        src += member.type->size;
    }
}

Napi::Object CallData::PopObject(const uint8_t *src, const TypeInfo *type, int16_t realign)
{
    Napi::Object obj = Napi::Object::New(env);
    PopObject(obj, src, type, realign);
    return obj;
}

Napi::Object CallData::PopArray(const uint8_t *src, const TypeInfo *type, int16_t realign)
{
    RG_ASSERT(type->primitive == PrimitiveKind::Array);

    uint32_t len = type->size / type->ref->size;

#define POP_ARRAY(SetCode) \
        do { \
            Napi::Array array = Napi::Array::New(env); \
             \
            for (uint32_t i = 0; i < len; i++) { \
                int16_t align = std::max(realign, type->ref->align); \
                src = AlignUp(src, align); \
                 \
                SetCode \
                 \
                src += type->ref->size; \
            } \
             \
            return array; \
        } while (false)
#define POP_NUMBER_ARRAY(TypedArrayType, CType) \
        do { \
            if (type->hint == TypeInfo::ArrayHint::TypedArray) { \
                Napi::TypedArrayType array = Napi::TypedArrayType::New(env, len); \
                 \
                for (uint32_t i = 0; i < len; i++) { \
                    int16_t align = std::max(realign, type->ref->align); \
                    src = AlignUp(src, align); \
                     \
                    CType f = *(CType *)src; \
                    array[i] = f; \
                     \
                    src += type->ref->size; \
                } \
                 \
                return array; \
            } else { \
                POP_ARRAY({ \
                    double d = (double)*(CType *)src; \
                    array.Set(i, Napi::Number::New(env, d)); \
                }); \
            } \
        } while (false)

    switch (type->ref->primitive) {
        case PrimitiveKind::Void: { RG_UNREACHABLE(); } break;

        case PrimitiveKind::Bool: {
            POP_ARRAY({
                bool b = *(bool *)src;
                array.Set(i, Napi::Boolean::New(env, b));
            });
        } break;
        case PrimitiveKind::Int8: { POP_NUMBER_ARRAY(Int8Array, int8_t); } break;
        case PrimitiveKind::UInt8: { POP_NUMBER_ARRAY(Uint8Array, uint8_t); } break;
        case PrimitiveKind::Int16: { POP_NUMBER_ARRAY(Int16Array, int16_t); } break;
        case PrimitiveKind::UInt16: { POP_NUMBER_ARRAY(Uint16Array, uint16_t); } break;
        case PrimitiveKind::Int32: { POP_NUMBER_ARRAY(Int32Array, int32_t); } break;
        case PrimitiveKind::UInt32: { POP_NUMBER_ARRAY(Uint32Array, uint32_t); } break;
        case PrimitiveKind::Int64: {
            POP_ARRAY({
                int64_t v = *(int64_t *)src;
                array.Set(i, Napi::BigInt::New(env, v));
            });
        } break;
        case PrimitiveKind::UInt64: {
            POP_ARRAY({
                uint64_t v = *(uint64_t *)src;
                array.Set(i, Napi::BigInt::New(env, v));
            });
        } break;
        case PrimitiveKind::String: {
            POP_ARRAY({
                const char *str = *(const char **)src;
                array.Set(i, Napi::String::New(env, str));
            });
        } break;
        case PrimitiveKind::String16: {
            POP_ARRAY({
                const char16_t *str16 = *(const char16_t **)src;
                array.Set(i, Napi::String::New(env, str16));
            });
        } break;
        case PrimitiveKind::Pointer: {
            POP_ARRAY({
                void *ptr2 = *(void **)src;

                Napi::External<void> external = Napi::External<void>::New(env, ptr2);
                SetValueTag(instance, external, type->ref);

                array.Set(i, external);
            });
        } break;
        case PrimitiveKind::Record: {
            POP_ARRAY({
                Napi::Object obj = PopObject(src, type->ref, realign);
                array.Set(i, obj);
            });
        } break;
        case PrimitiveKind::Array: {
            POP_ARRAY({
                Napi::Object obj = PopArray(src, type->ref, realign);
                array.Set(i, obj);
            });
        } break;
        case PrimitiveKind::Float32: { POP_NUMBER_ARRAY(Float32Array, float); } break;
        case PrimitiveKind::Float64: { POP_NUMBER_ARRAY(Float64Array, double); } break;
    }

#undef POP_NUMBER_ARRAY
#undef POP_ARRAY

    RG_UNREACHABLE();
}

Napi::Value CallData::Run(const Napi::CallbackInfo &info)
{
    if (!RG_UNLIKELY(Prepare(info)))
        return env.Null();

    if (debug) {
        DumpDebug();
    }
    Execute();

    return Complete();
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

    DumpMemory("Stack", stack);
    DumpMemory("Heap", heap);
}

}
