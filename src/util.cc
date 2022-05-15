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
            ThrowError<Napi::TypeError>(value.Env(), "Unknown type string '%1'", str.c_str());
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

const TypeInfo *GetPointerType(InstanceData *instance, const TypeInfo *ref)
{
    // Special cases
    if (TestStr(ref->name, "char")) {
        return instance->types_map.FindValue("string", nullptr);
    } else if (TestStr(ref->name, "char16") || TestStr(ref->name, "char16_t")) {
        return instance->types_map.FindValue("string16", nullptr);
    }

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

    return type;
}

const char *GetValueType(const InstanceData *instance, Napi::Value value)
{
    for (const TypeInfo &type: instance->types) {
        if (CheckValueTag(instance, value, &type))
            return type.name;
    }

    switch (value.Type()) {
        case napi_undefined: return "undefined";
        case napi_null: return "null";
        case napi_boolean: return "boolean";
        case napi_number: return "number";
        case napi_string: return "string";
        case napi_symbol: return "symbol";
        case napi_object: return "object";
        case napi_function: return "fucntion";
        case napi_external: return "external";
        case napi_bigint: return "bigint";
    }

    return "unknown";
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

class HfaAnalyser {
    uint32_t primitives;
    Size count;

public:
    bool Analyse(const TypeInfo *type, int min, int max);

private:
    void AnalyseStruct(const TypeInfo *type);
    void AnalyseArray(const TypeInfo *type);
};

bool HfaAnalyser::Analyse(const TypeInfo *type, int min, int max)
{
    primitives = 0;
    count = 0;

    if (type->primitive == PrimitiveKind::Record) {
        AnalyseStruct(type);
    } else if (type->primitive == PrimitiveKind::Array) {
        AnalyseArray(type);
    } else {
        return false;
    }

    bool hfa = (count >= min && count <= max && PopCount(primitives) == 1);
    return hfa;
}

void HfaAnalyser::AnalyseStruct(const TypeInfo *type)
{
    RG_ASSERT(type->primitive == PrimitiveKind::Record);

    for (const RecordMember &member: type->members) {
        if (member.type->primitive == PrimitiveKind::Record) {
            AnalyseStruct(member.type);
        } else if (member.type->primitive == PrimitiveKind::Array) {
            AnalyseArray(member.type);
        } else if (member.type->primitive == PrimitiveKind::Float32 ||
                   member.type->primitive == PrimitiveKind::Float64) {
            primitives |= 1u << (int)member.type->primitive;
            count++;
        } else {
            primitives = UINT_MAX;
            return;
        }
    }
}

void HfaAnalyser::AnalyseArray(const TypeInfo *type)
{
    RG_ASSERT(type->primitive == PrimitiveKind::Array);

    if (type->ref->primitive == PrimitiveKind::Record) {
        AnalyseStruct(type->ref);
    } else if (type->ref->primitive == PrimitiveKind::Array) {
        AnalyseArray(type->ref);
    } else if (type->ref->primitive == PrimitiveKind::Float32 ||
               type->ref->primitive == PrimitiveKind::Float64) {
        primitives |= 1u << (int)type->ref->primitive;
        count += type->size / type->ref->size;;
    } else {
        primitives = UINT_MAX;
    }
}

bool IsHFA(const TypeInfo *type, int min, int max)
{
    HfaAnalyser analyser;
    return analyser.Analyse(type, min, max);
}

}
