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
    : env(env), instance(instance), func(func),
      mem(mem), old_stack_mem(mem->stack), old_heap_mem(mem->heap)
{
    mem->generation += !mem->depth;
    mem->depth++;

    RG_ASSERT(AlignUp(mem->stack.ptr, 16) == mem->stack.ptr);
    RG_ASSERT(AlignUp(mem->stack.end(), 16) == mem->stack.end());
}

CallData::~CallData()
{
    for (const OutArgument &out: out_arguments) {
        napi_delete_reference(env, out.ref);
    }

    mem->stack = old_stack_mem;
    mem->heap = old_heap_mem;

    instance->temp_trampolines -= used_trampolines;
    instance->temporaries -= mem->temporary;

    if (!--mem->depth && mem->temporary) {
        delete mem;
    }
}

const char *CallData::PushString(Napi::Value value)
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

const char16_t *CallData::PushString16(Napi::Value value)
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

bool CallData::PushObject(Napi::Object obj, const TypeInfo *type, uint8_t *origin, int16_t realign)
{
    RG_ASSERT(IsObject(obj));
    RG_ASSERT(type->primitive == PrimitiveKind::Record);

    for (Size i = 0; i < type->members.len; i++) {
        const RecordMember &member = type->members[i];
        Napi::Value value = obj.Get(member.name);

        if (RG_UNLIKELY(value.IsUndefined())) {
            ThrowError<Napi::TypeError>(env, "Missing expected object property '%1'", member.name);
            return false;
        }

        Size offset = realign ? (i * realign) : member.offset;
        uint8_t *dest = origin + offset;

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
                const char *str;
                if (RG_LIKELY(value.IsString())) {
                    str = PushString(value);
                    if (RG_UNLIKELY(!str))
                        return false;
                } else if (IsNullOrUndefined(value)) {
                    str = nullptr;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected string", GetValueType(instance, value), member.name);
                    return false;
                }

                *(const char **)dest = str;
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
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected string", GetValueType(instance, value), member.name);
                    return false;
                }

                *(const char16_t **)dest = str16;
            } break;
            case PrimitiveKind::Pointer: {
                if (CheckValueTag(instance, value, member.type)) {
                    Napi::External<void> external = value.As<Napi::External<void>>();
                    void *ptr = external.Data();
                    *(void **)dest = ptr;
                } else if (IsNullOrUndefined(value)) {
                    *(void **)dest = nullptr;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected %3", GetValueType(instance, value), member.name, member.type->name);
                    return false;
                }
            } break;
            case PrimitiveKind::Record: {
                if (RG_UNLIKELY(!IsObject(value))) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected object", GetValueType(instance, value), member.name);
                    return false;
                }

                Napi::Object obj2 = value.As<Napi::Object>();
                if (!PushObject(obj2, member.type, dest, realign))
                    return false;
            } break;
            case PrimitiveKind::Array: {
                if (value.IsArray()) {
                    Napi::Array array = value.As<Napi::Array>();
                    Size len = (Size)member.type->size / member.type->ref->size;

                    if (!PushNormalArray(array, len, member.type->ref, dest, realign))
                        return false;
                } else if (value.IsTypedArray()) {
                    Napi::TypedArray array = value.As<Napi::TypedArray>();
                    Size len = (Size)member.type->size / member.type->ref->size;

                    if (!PushTypedArray(array, len, member.type->ref, dest, realign))
                        return false;
                } else if (value.IsString() && !realign) {
                    if (!PushStringArray(value, member.type, dest))
                        return false;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected value %1 for member '%2', expected array", GetValueType(instance, value), member.name);
                    return false;
                }
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
            case PrimitiveKind::Callback: {
                void *ptr;

                if (value.IsFunction()) {
                    Napi::Function func = value.As<Napi::Function>();

                    ptr = ReserveTrampoline(member.type->proto, func);
                    if (RG_UNLIKELY(!ptr))
                        return false;
                } else if (CheckValueTag(instance, value, member.type)) {
                    Napi::External<void> external = value.As<Napi::External<void>>();
                    ptr = external.Data();
                } else if (IsNullOrUndefined(value)) {
                    ptr = nullptr;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member '%2', expected %3", GetValueType(instance, value), member.name, member.type->name);
                    return false;
                }

                *(void **)dest = ptr;
            } break;
        }
    }

    return true;
}

bool CallData::PushNormalArray(Napi::Array array, Size len, const TypeInfo *ref, uint8_t *origin, int16_t realign)
{
    RG_ASSERT(array.IsArray());

    if (RG_UNLIKELY(array.Length() != len)) {
        ThrowError<Napi::Error>(env, "Expected array of length %1, got %2", len, array.Length());
        return false;
    }

    Size offset = 0;

#define PUSH_ARRAY(Check, Expected, GetCode) \
        do { \
            for (uint32_t i = 0; i < len; i++) { \
                Napi::Value value = array[i]; \
                 \
                int16_t align = std::max(ref->align, realign); \
                 \
                offset = AlignLen(offset, align); \
                uint8_t *dest = origin + offset; \
                 \
                if (RG_UNLIKELY(!(Check))) { \
                    ThrowError<Napi::TypeError>(env, "Unexpected value %1 in array, expected %2", GetValueType(instance, value), (Expected)); \
                    return false; \
                } \
                 \
                GetCode \
                 \
                offset += ref->size; \
            } \
        } while (false)

    switch (ref->primitive) {
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
            PUSH_ARRAY(value.IsString() || IsNullOrUndefined(value), "string", {
                if (!IsNullOrUndefined(value)) {
                    const char *str = PushString(value);
                    if (RG_UNLIKELY(!str))
                        return false;
                    *(const char **)dest = str;
                } else {
                    *(const char **)dest = nullptr;
                }
            });
        } break;
        case PrimitiveKind::String16: {
            PUSH_ARRAY(value.IsString() || IsNullOrUndefined(value), "string", {
                if (!IsNullOrUndefined(value)) {
                    const char16_t *str16 = PushString16(value);
                    if (RG_UNLIKELY(!str16))
                        return false;
                    *(const char16_t **)dest = str16;
                } else {
                    *(const char16_t **)dest = nullptr;
                }
            });
        } break;
        case PrimitiveKind::Pointer: {
            PUSH_ARRAY(CheckValueTag(instance, value, ref) || IsNullOrUndefined(value), ref->name, {
                if (!IsNullOrUndefined(value)) {
                    Napi::External<void> external = value.As<Napi::External<void>>();
                    *(void **)dest = external.Data();
                } else {
                    *(void **)dest = nullptr;
                }
            });
        } break;
        case PrimitiveKind::Record: {
            PUSH_ARRAY(IsObject(value), "object", {
                Napi::Object obj2 = value.As<Napi::Object>();
                if (!PushObject(obj2, ref, dest, realign))
                    return false;
            });
        } break;
        case PrimitiveKind::Array: {
            for (uint32_t i = 0; i < len; i++) {
                Napi::Value value = array[i];

                int16_t align = std::max(ref->align, realign);
                offset = AlignLen(offset, align);

                uint8_t *dest = origin + offset;

                if (value.IsArray()) {
                    Napi::Array array2 = value.As<Napi::Array>();
                    Size len2 = (Size)ref->size / ref->ref->size;

                    if (!PushNormalArray(array2, len2, ref->ref, dest, realign))
                        return false;
                } else if (value.IsTypedArray()) {
                    Napi::TypedArray array2 = value.As<Napi::TypedArray>();
                    Size len2 = (Size)ref->size / ref->ref->size;

                    if (!PushTypedArray(array2, len2, ref->ref, dest, realign))
                        return false;
                } else if (value.IsString() && !realign) {
                    if (!PushStringArray(value, ref, dest))
                        return false;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected value %1 in array, expected array", GetValueType(instance, value));
                    return false;
                }

                offset += ref->size;
            }
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
        case PrimitiveKind::Callback: {
            for (uint32_t i = 0; i < len; i++) {
                Napi::Value value = array[i];

                int16_t align = std::max(ref->align, realign);
                offset = AlignLen(offset, align);

                uint8_t *dest = origin + offset;

                void *ptr;

                if (value.IsFunction()) {
                    Napi::Function func = value.As<Napi::Function>();

                    ptr = ReserveTrampoline(ref->proto, func);
                    if (RG_UNLIKELY(!ptr))
                        return false;
                } else if (CheckValueTag(instance, value, ref)) {
                    Napi::External<void> external = value.As<Napi::External<void>>();
                    ptr = external.Data();
                } else if (IsNullOrUndefined(value)) {
                    ptr = nullptr;
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected value %1 in array, expected %2", GetValueType(instance, value), ref->name);
                    return false;
                }

                *(void **)dest = ptr;

                offset += ref->size;
            }
        } break;
    }

#undef PUSH_ARRAY

    return true;
}

bool CallData::PushTypedArray(Napi::TypedArray array, Size len, const TypeInfo *ref, uint8_t *origin, int16_t realign)
{
    RG_ASSERT(array.IsTypedArray());

    if (RG_UNLIKELY(array.ElementLength() != (size_t)len)) {
        ThrowError<Napi::Error>(env, "Expected array of length %1, got %2", len, array.ElementLength());
        return false;
    }

    Size offset = 0;
    const uint8_t *buf = (const uint8_t *)array.ArrayBuffer().Data();

    if (RG_UNLIKELY(array.TypedArrayType() != GetTypedArrayType(ref))) {
        ThrowError<Napi::TypeError>(env, "Cannot use %1 value for %2 array", GetValueType(instance, array), ref->name);
        return false;
    }

    if (realign) {
        for (uint32_t i = 0; i < len; i++) {
            int16_t align = std::max(ref->align, realign);
            offset = AlignLen(offset, align);

            uint8_t *dest = origin + offset;

            memcpy(dest, buf + i * ref->size, ref->size);

            offset += ref->size;
        }
    } else {
        memcpy_safe(origin, buf, (size_t)array.ByteLength());
    }

    return true;
}

bool CallData::PushStringArray(Napi::Value obj, const TypeInfo *type, uint8_t *origin)
{
    RG_ASSERT(obj.IsString());
    RG_ASSERT(type->primitive == PrimitiveKind::Array);

    size_t encoded = 0;

    switch (type->ref->primitive) {
        case PrimitiveKind::Int8: {
            napi_status status = napi_get_value_string_utf8(env, obj, (char *)origin, type->size, &encoded);
            RG_ASSERT(status == napi_ok);
        } break;
        case PrimitiveKind::Int16: {
            napi_status status = napi_get_value_string_utf16(env, obj, (char16_t *)origin, type->size / 2, &encoded);
            RG_ASSERT(status == napi_ok);

            encoded *= 2;
        } break;

        default: {
            ThrowError<Napi::TypeError>(env, "Strings cannot be converted to %1 array", type->ref->name);
            return false;
        } break;
    }

    memset_safe(origin + encoded, 0, type->size - encoded);

    return true;
}

bool CallData::PushPointer(Napi::Value value, const ParameterInfo &param, void **out_ptr)
{
    switch (value.Type()) {
        case napi_undefined:
        case napi_null: {
            *out_ptr = nullptr;
            return true;
        } break;

        case napi_external: {
            if (RG_UNLIKELY(!CheckValueTag(instance, value, param.type)))
                goto unexpected;

            *out_ptr = value.As<Napi::External<uint8_t>>().Data();
            return true;
        } break;

        case napi_object: {
            uint8_t *ptr = nullptr;

            if (value.IsArray()) {
                Napi::Array array = value.As<Napi::Array>();

                Size len = (Size)array.Length();
                Size size = len * param.type->ref->size;

                ptr = AllocHeap(size, 16);

                if (param.directions & 1) {
                    if (!PushNormalArray(array, len, param.type->ref, ptr))
                        return false;
                } else {
                    memset(ptr, 0, size);
                }
            } else if (value.IsTypedArray()) {
                Napi::TypedArray array = value.As<Napi::TypedArray>();

                Size len = (Size)array.ElementLength();
                Size size = (Size)array.ByteLength();

                ptr = AllocHeap(size, 16);

                if (param.directions & 1) {
                    if (!PushTypedArray(array, len, param.type->ref, ptr))
                        return false;
                } else {
                    if (RG_UNLIKELY(array.TypedArrayType() != GetTypedArrayType(param.type->ref))) {
                        ThrowError<Napi::TypeError>(env, "Cannot use %1 value for %2 array", GetValueType(instance, array), param.type->ref->name);
                        return false;
                    }

                    memset(ptr, 0, size);
                }
            } else if (RG_LIKELY(param.type->ref->primitive == PrimitiveKind::Record)) {
                Napi::Object obj = value.As<Napi::Object>();
                RG_ASSERT(IsObject(value));

                ptr = AllocHeap(param.type->ref->size, 16);

                if (param.directions & 1) {
                    if (!PushObject(obj, param.type->ref, ptr))
                        return false;
                } else {
                    memset(ptr, 0, param.type->size);
                }
            } else {
                goto unexpected;
            }

            if (param.directions & 2) {
                OutArgument *out = out_arguments.AppendDefault();

                napi_status status = napi_create_reference(env, value, 1, &out->ref);
                RG_ASSERT(status == napi_ok);

                out->ptr = ptr;
                out->type = param.type->ref;
            }

            *out_ptr = ptr;
            return true;
        } break;

        default: {} break;
    }

unexpected:
    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for argument %2, expected %3", GetValueType(instance, value), param.offset + 1, param.type->name);
    return false;
}

static inline Napi::Value GetReferenceValue(Napi::Env env, napi_ref ref)
{
    napi_value value;

    napi_status status = napi_get_reference_value(env, ref, &value);
    RG_ASSERT(status == napi_ok);

    return Napi::Value(env, value);
}

void CallData::PopOutArguments()
{
    for (const OutArgument &out: out_arguments) {
        Napi::Value value = GetReferenceValue(env, out.ref);
        RG_ASSERT(!value.IsEmpty());

        if (value.IsArray()) {
            Napi::Array array(env, value);
            PopNormalArray(array, out.ptr, out.type);
        } else if (value.IsTypedArray()) {
            Napi::TypedArray array(env, value);
            PopTypedArray(array, out.ptr, out.type);
        } else {
            Napi::Object obj(env, value);
            PopObject(obj, out.ptr, out.type);
        }

        if (out.type->dispose) {
            out.type->dispose(env, out.type, out.ptr);
        }
    }
}

void *CallData::ReserveTrampoline(const FunctionInfo *proto, Napi::Function func)
{
    if (RG_UNLIKELY(instance->temp_trampolines >= MaxTrampolines)) {
        ThrowError<Napi::Error>(env, "Too many temporary callbacks are in use (max = %1)", MaxTrampolines);
        return nullptr;
    }

    int idx = instance->next_trampoline;

    instance->next_trampoline = (int16_t)((instance->next_trampoline + 1) % MaxTrampolines);
    instance->temp_trampolines++;
    used_trampolines++;

    TrampolineInfo *trampoline = &instance->trampolines[idx];

    trampoline->proto = proto;
    trampoline->func.Reset(func, 1);
    trampoline->generation = (int32_t)mem->generation;

    void *ptr = GetTrampoline(idx, proto);
    return ptr;
}

void CallData::PopObject(Napi::Object obj, const uint8_t *origin, const TypeInfo *type, int16_t realign)
{
    Napi::Env env = obj.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    RG_ASSERT(type->primitive == PrimitiveKind::Record);

    for (Size i = 0; i < type->members.len; i++) {
        const RecordMember &member = type->members[i];

        Size offset = realign ? (i * realign) : member.offset;
        const uint8_t *src = origin + offset;

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
                obj.Set(member.name, NewBigInt(env, v));
            } break;
            case PrimitiveKind::UInt64: {
                uint64_t v = *(uint64_t *)src;
                obj.Set(member.name, NewBigInt(env, v));
            } break;
            case PrimitiveKind::String: {
                const char *str = *(const char **)src;
                obj.Set(member.name, str ? Napi::String::New(env, str) : env.Null());

                if (member.type->dispose) {
                    member.type->dispose(env, member.type, str);
                }
            } break;
            case PrimitiveKind::String16: {
                const char16_t *str16 = *(const char16_t **)src;
                obj.Set(member.name, str16 ? Napi::String::New(env, str16) : env.Null());

                if (member.type->dispose) {
                    member.type->dispose(env, member.type, str16);
                }
            } break;
            case PrimitiveKind::Pointer:
            case PrimitiveKind::Callback: {
                void *ptr2 = *(void **)src;

                if (ptr2) {
                    Napi::External<void> external = Napi::External<void>::New(env, ptr2);
                    SetValueTag(instance, external, member.type);

                    obj.Set(member.name, external);
                } else {
                    obj.Set(member.name, env.Null());
                }

                if (member.type->dispose) {
                    member.type->dispose(env, member.type, ptr2);
                }
            } break;
            case PrimitiveKind::Record: {
                Napi::Object obj2 = PopObject(src, member.type, realign);
                obj.Set(member.name, obj2);
            } break;
            case PrimitiveKind::Array: {
                Napi::Value value = PopArray(src, member.type, realign);
                obj.Set(member.name, value);
            } break;
            case PrimitiveKind::Float32: {
                float f = *(float *)src;
                obj.Set(member.name, Napi::Number::New(env, (double)f));
            } break;
            case PrimitiveKind::Float64: {
                double d = *(double *)src;
                obj.Set(member.name, Napi::Number::New(env, d));
            } break;
        }
    }
}

Napi::Object CallData::PopObject(const uint8_t *origin, const TypeInfo *type, int16_t realign)
{
    Napi::Object obj = Napi::Object::New(env);
    PopObject(obj, origin, type, realign);
    return obj;
}

static Size WideStringLength(const char16_t *str16, Size max)
{
    Size len = 0;

    while (len < max && str16[len]) {
        len++;
    }

    return len;
}

void CallData::PopNormalArray(Napi::Array array, const uint8_t *origin, const TypeInfo *ref, int16_t realign)
{
    RG_ASSERT(array.IsArray());

    Size offset = 0;
    uint32_t len = array.Length();

#define POP_ARRAY(SetCode) \
        do { \
            for (uint32_t i = 0; i < len; i++) { \
                int16_t align = std::max(realign, ref->align); \
                offset = AlignLen(offset, align); \
                 \
                const uint8_t *src = origin + offset; \
                 \
                SetCode \
                 \
                offset += ref->size; \
            } \
        } while (false)
#define POP_NUMBER_ARRAY(CType) \
        do { \
            POP_ARRAY({ \
                double d = (double)*(CType *)src; \
                array.Set(i, Napi::Number::New(env, d)); \
            }); \
        } while (false)

    switch (ref->primitive) {
        case PrimitiveKind::Void: { RG_UNREACHABLE(); } break;

        case PrimitiveKind::Bool: {
            POP_ARRAY({
                bool b = *(bool *)src;
                array.Set(i, Napi::Boolean::New(env, b));
            });
        } break;
        case PrimitiveKind::Int8: { POP_NUMBER_ARRAY(int8_t); } break;
        case PrimitiveKind::UInt8: { POP_NUMBER_ARRAY(uint8_t); } break;
        case PrimitiveKind::Int16: { POP_NUMBER_ARRAY(int16_t); } break;
        case PrimitiveKind::UInt16: { POP_NUMBER_ARRAY(uint16_t); } break;
        case PrimitiveKind::Int32: { POP_NUMBER_ARRAY(int32_t); } break;
        case PrimitiveKind::UInt32: { POP_NUMBER_ARRAY(uint32_t); } break;
        case PrimitiveKind::Int64: {
            POP_ARRAY({
                int64_t v = *(int64_t *)src;
                array.Set(i, NewBigInt(env, v));
            });
        } break;
        case PrimitiveKind::UInt64: {
            POP_ARRAY({
                uint64_t v = *(uint64_t *)src;
                array.Set(i, NewBigInt(env, v));
            });
        } break;
        case PrimitiveKind::String: {
            POP_ARRAY({
                const char *str = *(const char **)src;
                array.Set(i, str ? Napi::String::New(env, str) : env.Null());

                if (ref->dispose) {
                    ref->dispose(env, ref, str);
                }
            });
        } break;
        case PrimitiveKind::String16: {
            POP_ARRAY({
                const char16_t *str16 = *(const char16_t **)src;
                array.Set(i, str16 ? Napi::String::New(env, str16) : env.Null());

                if (ref->dispose) {
                    ref->dispose(env, ref, str16);
                }
            });
        } break;
        case PrimitiveKind::Pointer:
        case PrimitiveKind::Callback: {
            POP_ARRAY({
                void *ptr2 = *(void **)src;

                if (ptr2) {
                    Napi::External<void> external = Napi::External<void>::New(env, ptr2);
                    SetValueTag(instance, external, ref);

                    array.Set(i, external);
                } else {
                    array.Set(i, env.Null());
                }

                if (ref->dispose) {
                    ref->dispose(env, ref, ptr2);
                }
            });
        } break;
        case PrimitiveKind::Record: {
            POP_ARRAY({
                Napi::Object obj = PopObject(src, ref, realign);
                array.Set(i, obj);
            });
        } break;
        case PrimitiveKind::Array: {
            POP_ARRAY({
                Napi::Value value = PopArray(src, ref, realign);
                array.Set(i, value);
            });
        } break;
        case PrimitiveKind::Float32: { POP_NUMBER_ARRAY(float); } break;
        case PrimitiveKind::Float64: { POP_NUMBER_ARRAY(double); } break;
    }

#undef POP_NUMBER_ARRAY
#undef POP_ARRAY
}

void CallData::PopTypedArray(Napi::TypedArray array, const uint8_t *origin, const TypeInfo *ref, int16_t realign)
{
    RG_ASSERT(array.IsTypedArray());
    RG_ASSERT(GetTypedArrayType(ref) == array.TypedArrayType());

    uint8_t *buf = (uint8_t *)array.ArrayBuffer().Data();

    if (realign) {
        Size offset = 0;
        Size len = (Size)array.ElementLength();

        for (Size i = 0; i < len; i++) {
            int16_t align = std::max(ref->align, realign);
            offset = AlignLen(offset, align);

            uint8_t *dest = buf + i * ref->size;
            const uint8_t *src = origin + offset;

            memcpy(dest, src, ref->size);

            offset += ref->size;
        }
    } else {
        memcpy_safe(buf, origin, (size_t)array.ByteLength());
    }
}

Napi::Value CallData::PopArray(const uint8_t *origin, const TypeInfo *type, int16_t realign)
{
    RG_ASSERT(type->primitive == PrimitiveKind::Array);

    uint32_t len = type->size / type->ref->size;
    Size offset = 0;

#define POP_ARRAY(SetCode) \
        do { \
            Napi::Array array = Napi::Array::New(env); \
             \
            for (uint32_t i = 0; i < len; i++) { \
                int16_t align = std::max(realign, type->ref->align); \
                offset = AlignLen(offset, align); \
                 \
                const uint8_t *src = origin + offset; \
                 \
                SetCode \
                 \
                offset += type->ref->size; \
            } \
             \
            return array; \
        } while (false)
#define POP_NUMBER_ARRAY(TypedArrayType, CType) \
        do { \
            if (type->hint == TypeInfo::ArrayHint::Array) { \
                POP_ARRAY({ \
                    double d = (double)*(CType *)src; \
                    array.Set(i, Napi::Number::New(env, d)); \
                }); \
            } else { \
                Napi::TypedArrayType array = Napi::TypedArrayType::New(env, len); \
                PopTypedArray(array, origin, type->ref, realign); \
                 \
                return array; \
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
        case PrimitiveKind::Int8: {
            if (type->hint == TypeInfo::ArrayHint::String) {
                RG_ASSERT(!realign);

                const char *ptr = (const char *)origin;
                size_t count = strnlen(ptr, (size_t)len);

                Napi::String str = Napi::String::New(env, ptr, count);
                return str;
            }

            POP_NUMBER_ARRAY(Int8Array, int8_t);
        } break;
        case PrimitiveKind::UInt8: { POP_NUMBER_ARRAY(Uint8Array, uint8_t); } break;
        case PrimitiveKind::Int16: {
            if (type->hint == TypeInfo::ArrayHint::String) {
                RG_ASSERT(!realign);

                const char16_t *ptr = (const char16_t *)origin;
                Size count = WideStringLength(ptr, len);

                Napi::String str = Napi::String::New(env, ptr, count);
                return str;
            }

            POP_NUMBER_ARRAY(Int16Array, int16_t);
        } break;
        case PrimitiveKind::UInt16: { POP_NUMBER_ARRAY(Uint16Array, uint16_t); } break;
        case PrimitiveKind::Int32: { POP_NUMBER_ARRAY(Int32Array, int32_t); } break;
        case PrimitiveKind::UInt32: { POP_NUMBER_ARRAY(Uint32Array, uint32_t); } break;
        case PrimitiveKind::Int64: {
            POP_ARRAY({
                int64_t v = *(int64_t *)src;
                array.Set(i, NewBigInt(env, v));
            });
        } break;
        case PrimitiveKind::UInt64: {
            POP_ARRAY({
                uint64_t v = *(uint64_t *)src;
                array.Set(i, NewBigInt(env, v));
            });
        } break;
        case PrimitiveKind::String: {
            POP_ARRAY({
                const char *str = *(const char **)src;
                array.Set(i, str ? Napi::String::New(env, str) : env.Null());
            });
        } break;
        case PrimitiveKind::String16: {
            POP_ARRAY({
                const char16_t *str16 = *(const char16_t **)src;
                array.Set(i, str16 ? Napi::String::New(env, str16) : env.Null());
            });
        } break;
        case PrimitiveKind::Pointer:
        case PrimitiveKind::Callback: {
            POP_ARRAY({
                void *ptr2 = *(void **)src;

                if (ptr2) {
                    Napi::External<void> external = Napi::External<void>::New(env, ptr2);
                    SetValueTag(instance, external, type->ref);

                    array.Set(i, external);
                } else {
                    array.Set(i, env.Null());
                }
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
                Napi::Value value = PopArray(src, type->ref, realign);
                array.Set(i, value);
            });
        } break;
        case PrimitiveKind::Float32: { POP_NUMBER_ARRAY(Float32Array, float); } break;
        case PrimitiveKind::Float64: { POP_NUMBER_ARRAY(Float64Array, double); } break;
    }

#undef POP_NUMBER_ARRAY
#undef POP_ARRAY

    RG_UNREACHABLE();
}

void CallData::DumpForward() const
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

    Span<const uint8_t> stack = MakeSpan(mem->stack.end(), old_stack_mem.end() - mem->stack.end());
    Span<const uint8_t> heap = MakeSpan(old_heap_mem.ptr, mem->heap.ptr - old_heap_mem.ptr);

    DumpMemory("Stack", stack);
    DumpMemory("Heap", heap);
}

}
