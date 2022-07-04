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

const TypeInfo *ResolveType(const InstanceData *instance, Napi::Value value, int *out_directions)
{
    if (value.IsString()) {
        std::string str = value.As<Napi::String>();

        const TypeInfo *type = instance->types_map.FindValue(str.c_str(), nullptr);

        if (!type) {
            ThrowError<Napi::TypeError>(value.Env(), "Unknown type name '%1'", str.c_str());
            return nullptr;
        }

        if (out_directions) {
            *out_directions = 1;
        }
        return type;
    } else if (CheckValueTag(instance, value, &TypeInfoMarker)) {
        Napi::External<TypeInfo> external = value.As<Napi::External<TypeInfo>>();

        const TypeInfo *raw = external.Data();
        const TypeInfo *type = AlignDown(raw, 4);
        RG_ASSERT(type);

        if (out_directions) {
            Size delta = (uint8_t *)raw - (uint8_t *)type;
            *out_directions = 1 + (int)delta;
        }
        return type;
    } else {
        ThrowError<Napi::TypeError>(value.Env(), "Unexpected %1 value as type specifier, expected string or type", GetValueType(instance, value));
        return nullptr;
    }
}

const TypeInfo *MakePointerType(InstanceData *instance, const TypeInfo *ref, int count)
{
    RG_ASSERT(count >= 1);

    // Special cases
    if (TestStr(ref->name, "char")) {
        ref = instance->types_map.FindValue("string", nullptr);
        count--;
    } else if (TestStr(ref->name, "char16") || TestStr(ref->name, "char16_t")) {
        ref = instance->types_map.FindValue("string16", nullptr);
        count--;
    }

    for (int i = 0; i < count; i++) {
        char name_buf[256];
        Fmt(name_buf, "%1%2*", ref->name, ref->primitive == PrimitiveKind::Pointer ? "" : " ");

        TypeInfo *type = instance->types_map.FindValue(name_buf, nullptr);

        if (!type) {
            type = instance->types.AppendDefault();

            type->name = DuplicateString(name_buf, &instance->str_alloc).ptr;

            type->primitive = PrimitiveKind::Pointer;
            type->size = RG_SIZE(void *);
            type->align = RG_SIZE(void *);
            type->ref = ref;

            instance->types_map.Set(type);
        }

        ref = type;
    }

    return ref;
}

const char *GetValueType(const InstanceData *instance, Napi::Value value)
{
    for (const TypeInfo &type: instance->types) {
        if (CheckValueTag(instance, value, &type))
            return type.name;
    }

    if (value.IsArray()) {
        return "Array";
    } else if (value.IsTypedArray()) {
        Napi::TypedArray array = value.As<Napi::TypedArray>();

        switch (array.TypedArrayType()) {
            case napi_int8_array: return "Int8Array";
            case napi_uint8_array: return "Uint8Array";
            case napi_uint8_clamped_array: return "Uint8ClampedArray";
            case napi_int16_array: return "Int16Array";
            case napi_uint16_array: return "Uint16Array";
            case napi_int32_array: return "Int32Array";
            case napi_uint32_array: return "Uint32Array";
            case napi_float32_array: return "Float32Array";
            case napi_float64_array: return "Float64Array";
            case napi_bigint64_array: return "BigInt64Array";
            case napi_biguint64_array: return "BigUint64Array";
        }
    }

    switch (value.Type()) {
        case napi_undefined: return "Undefined";
        case napi_null: return "Null";
        case napi_boolean: return "Boolean";
        case napi_number: return "Number";
        case napi_string: return "String";
        case napi_symbol: return "Symbol";
        case napi_object: return "Object";
        case napi_function: return "Function";
        case napi_external: return "External";
        case napi_bigint: return "BigInt";
    }

    // This should not be possible, but who knows...
    return "Unknown";
}

void SetValueTag(const InstanceData *instance, Napi::Value value, const void *marker)
{
    napi_type_tag tag = { instance->tag_lower, (uint64_t)marker };
    napi_status status = napi_type_tag_object(value.Env(), value, &tag);
    RG_ASSERT(status == napi_ok);
}

bool CheckValueTag(const InstanceData *instance, Napi::Value value, const void *marker)
{
    bool match = false;

    if (!IsNullOrUndefined(value)) {
        napi_type_tag tag = { instance->tag_lower, (uint64_t)marker };
        napi_check_object_type_tag(value.Env(), value, &tag, &match);
    }

    return match;
}

int GetTypedArrayType(const TypeInfo *type)
{
    switch (type->primitive) {
        case PrimitiveKind::Int8: return napi_int8_array;
        case PrimitiveKind::UInt8: return napi_uint8_array;
        case PrimitiveKind::Int16: return napi_int16_array;
        case PrimitiveKind::UInt16: return napi_uint16_array;
        case PrimitiveKind::Int32: return napi_int32_array;
        case PrimitiveKind::UInt32: return napi_uint32_array;
        case PrimitiveKind::Float32: return napi_float32_array;
        case PrimitiveKind::Float64: return napi_float64_array;

        default: return -1;
    }

    RG_UNREACHABLE();
}

static int AnalyseFlatRec(const TypeInfo *type, int offset, int count, FunctionRef<void(const TypeInfo *type, int offset, int count)> func)
{
    if (type->primitive == PrimitiveKind::Record) {
        for (int i = 0; i < count; i++) {
            for (const RecordMember &member: type->members) {
                offset = AnalyseFlatRec(member.type, offset, 1, func);
            }
        }
    } else if (type->primitive == PrimitiveKind::Array) {
        count *= type->size / type->ref->size;
        offset = AnalyseFlatRec(type->ref, offset, count, func);
    } else {
        func(type, offset, count);
        offset += count;
    }

    return offset;
}

int AnalyseFlat(const TypeInfo *type, FunctionRef<void(const TypeInfo *type, int offset, int count)> func)
{
    return AnalyseFlatRec(type, 0, 1, func);
}

int IsHFA(const TypeInfo *type, int min, int max)
{
    uint32_t primitives = 0;
    int count = 0;

    count = AnalyseFlat(type, [&](const TypeInfo *type, int, int) {
        if (IsFloat(type)) {
            primitives |= 1u << (int)type->primitive;
        } else {
            primitives = UINT32_MAX;
        }
    });

    bool hfa = (count >= min && count <= max && PopCount(primitives) == 1);
    return hfa ? count : 0;
}

void DumpMemory(const char *type, Span<const uint8_t> bytes)
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

}
