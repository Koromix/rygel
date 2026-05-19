// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "call.hh"
#include "ffi.hh"
#include "util.hh"

#include <napi.h>

namespace K {

// Value does not matter, the tag system uses memory addresses
const napi_type_tag LibraryHandleMarker = { 0xdb9b066e6f700474, 0x0aecd7e4c63fbda9 };
const napi_type_tag TypeObjectMarker = { 0x1cc449675b294374, 0xbb13a50e97dcb017 };
const napi_type_tag DirectionMarker = { 0xf9c306238b480580, 0xc2e168524a0823f5 };
const napi_type_tag UnionValueMarker = { 0x5eaf2245526a4c7d, 0x8c86c9ee2b96ffc8 };
const napi_type_tag CastMarker = { 0x77f459614a0a412f, 0x80b3dda1341dc8df };

#if !defined(EXTERNAL_TYPES)

Napi::Function TypeObject::InitClass(Napi::Env env)
{
    Napi::Function constructor = DefineClass(env, "TypeObject", {});
    return constructor;
}

TypeObject::TypeObject(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<TypeObject>(info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0u].IsExternal()) [[unlikely]] {
        ThrowError<Napi::Error>(env, "Type objects cannot be constructed manually");
        return;
    }

    Napi::External<TypeInfo> external = info[0u].As<Napi::External<TypeInfo>>();
    type = external.Data();
}

void TypeObject::Finalize(Napi::BasicEnv env)
{
    DeleteReferenceSafe(env, *this);
    SuppressDestruct();
}

#endif

Napi::Function UnionValue::InitClass(Napi::Env env, const TypeInfo *type)
{
    K_ASSERT(type->primitive == PrimitiveKind::Union);

    // node-addon-api wants std::vector
    std::vector<Napi::ClassPropertyDescriptor<UnionValue>> properties;
    properties.reserve(type->members.len);

    for (Size i = 0; i < type->members.len; i++) {
        const RecordMember &member = type->members[i];

        napi_property_attributes attr = (napi_property_attributes)(napi_writable | napi_enumerable);
        Napi::ClassPropertyDescriptor<UnionValue> prop = InstanceAccessor(member.name, &UnionValue::Getter,
                                                                          &UnionValue::Setter, attr, (void *)i);

        properties.push_back(prop);
    }

    Napi::Function constructor = DefineClass(env, type->name, properties, (void *)type);
    return constructor;
}

UnionValue::UnionValue(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<UnionValue>(info), type((const TypeInfo *)info.Data())
{
    Napi::Env env = info.Env();
    instance = env.GetInstanceData<InstanceData>();

    if (info.Length() >= 1) {
        if (!info[0].IsExternal()) [[unlikely]] {
            ThrowError<Napi::Error>(env, "Union objects cannot be constructed manually");
            return;
        }

        Napi::External<void> external = info[0].As<Napi::External<void>>();
        const uint8_t *ptr = (const uint8_t *)external.Data();

        SetRaw(ptr);
    }
}

void UnionValue::Finalize(Napi::BasicEnv env)
{
    DeleteReferenceSafe(env, *this);
    SuppressDestruct();
}

void UnionValue::SetRaw(const uint8_t *ptr)
{
    raw.RemoveFrom(0);
    raw.Append(MakeSpan(ptr, type->size));

    Value().Set(instance->active_symbol.Value(), Env().Undefined());
    active_idx = -1;
}

Napi::Value UnionValue::Getter(const Napi::CallbackInfo &info)
{
    Size idx = (Size)info.Data();
    const RecordMember &member = type->members[idx];

    napi_value value = nullptr;

    if (idx == active_idx) {
        value = Value().Get(instance->active_symbol.Value());
    } else {
        Napi::Env env = info.Env();

        if (!raw.len) [[unlikely]] {
            ThrowError<Napi::Error>(env, "Cannont convert %1 union value", active_idx < 0 ? "empty" : "assigned");
            return env.Null();
        }

        value = Decode(instance, raw.ptr, member.type);

        Value().Set(instance->active_symbol.Value(), value);
        active_idx = idx;
    }

    K_ASSERT(value);
    return Napi::Value(info.Env(), value);
}

void UnionValue::Setter(const Napi::CallbackInfo &info, const Napi::Value &value)
{
    Size idx = (Size)info.Data();

    Value().Set(instance->active_symbol.Value(), value);
    active_idx = idx;

    raw.Clear();
}

static inline bool IsIdentifierStart(char c)
{
    return IsAsciiAlpha(c) || c == '_';
}

static inline bool IsIdentifierChar(char c)
{
    return IsAsciiAlphaOrDigit(c) || c == '_';
}

static inline Span<const char> SplitIdentifier(Span<const char> str)
{
    Size offset = 0;

    if (str.len && IsIdentifierStart(str[0])) {
        offset++;

        while (offset < str.len && IsIdentifierChar(str[offset])) {
            offset++;
        }
    }

    Span<const char> token = str.Take(0, offset);
    return token;
}

int ResolveDirections(Span<const char> str)
{
    if (str == "_In_") {
        return 1;
    } else if (str == "_Out_") {
        return 2;
    } else if (str == "_Inout_") {
        return 3;
    } else {
        return 0;
    }
}

const TypeInfo *ResolveType(Napi::Value value, int *out_directions)
{
    Napi::Env env = value.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (value.IsString()) {
        std::string str = value.As<Napi::String>();
        Span<const char> remain = str.c_str();

        // Quick path for known types (int, float *, etc.)
        const TypeInfo *type = instance->types_map.FindValue(remain.ptr, nullptr);

        if (!type) {
            if (out_directions) {
                Span<const char> prefix = SplitIdentifier(remain);
                int directions = ResolveDirections(prefix);

                if (directions) {
                    remain = remain.Take(prefix.len, remain.len - prefix.len);
                    remain = TrimStrLeft(remain);

                    *out_directions = directions;
                } else {
                    *out_directions = 1;
                }
            }

            type = ResolveType(env, remain.ptr);

            if (!type) {
                if (!env.IsExceptionPending()) {
                    ThrowError<Napi::TypeError>(env, "Unknown or invalid type name '%1'", str.c_str());
                }
                return nullptr;
            }

            // Cache for quick future access
            bool inserted;
            auto bucket = instance->types_map.InsertOrGetDefault(remain.ptr, &inserted);

            if (inserted) {
                bucket->key = DuplicateString(remain, &instance->str_alloc).ptr;
                bucket->value = type;
            }
        } else if (out_directions) {
            *out_directions = 1;
        }

        return type;
    } else {
        napi_valuetype kind = GetKindOf(env, value);

        if (kind == napi_external && CheckValueTag(env, value, &DirectionMarker)) {
            Napi::External<TypeInfo> external = Napi::External<TypeInfo>(env, value);
            const TypeInfo *raw = external.Data();

            const TypeInfo *type = AlignDown(raw, 4);
            K_ASSERT(type);

            if (out_directions) {
                Size delta = (uint8_t *)raw - (uint8_t *)type;
                *out_directions = 1 + (int)delta;
            }

            return type;
#if defined(EXTERNAL_TYPES)
        } else if (kind == napi_external && CheckValueTag(env, value, &TypeObjectMarker)) {
            Napi::External<TypeInfo> external = Napi::External<TypeInfo>(env, value);
            const TypeInfo *type = external.Data();

            if (out_directions) {
                *out_directions = 1;
            }
            return type;
#else
        } else if (kind == napi_object && CheckValueTag(env, value, &TypeObjectMarker)) {
            TypeObject *defn = nullptr;
            napi_unwrap(env, value, (void **)&defn);

            if (out_directions) {
                *out_directions = 1;
            }
            return defn->GetType();
#endif
        } else {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value as type specifier, expected string or type", GetValueType(instance, value));
            return nullptr;
        }
    }
}

const TypeInfo *ResolveType(Napi::Env env, Span<const char> str)
{
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    // Each item can be > 0 for array or 0 for a pointer
    LocalArray<Size, 8> arrays;
    uint8_t disposables = 0;

    Span<const char> name;
    Span<const char> after;
    {
        Span<const char> remain = str;

        // Skip initial const qualifiers
        remain = TrimStrLeft(remain);
        while (SplitIdentifier(remain) == "const") {
            remain = remain.Take(6, remain.len - 6);
            remain = TrimStrLeft(remain);
        }
        remain = TrimStrLeft(remain);

        after = remain;

        // Consume one or more identifiers (e.g. unsigned int)
        for (;;) {
            after = TrimStrLeft(after);

            Span<const char> token = SplitIdentifier(after);
            if (!token.len)
                break;
            after = after.Take(token.len, after.len - token.len);
        }

        name = TrimStr(MakeSpan(remain.ptr, after.ptr - remain.ptr));
    }

    // Consume type indirections (pointer, array, etc.)
    while (after.len) {
        if (after[0] == '*') {
            after = after.Take(1, after.len - 1);

            if (!arrays.Available()) [[unlikely]] {
                ThrowError<Napi::Error>(env, "Too many type indirections");
                return nullptr;
            }

            arrays.Append(0);
        } else if (after[0] == '!') {
            after = after.Take(1, after.len - 1);
            disposables |= (1u << arrays.len);
        } else if (after[0] == '[') {
            after = after.Take(1, after.len - 1);

            Size len = 0;

            after = TrimStrLeft(after);
            if (!ParseInt(after, &len, 0, &after) || len < 0) [[unlikely]] {
                ThrowError<Napi::Error>(env, "Invalid array length");
                return nullptr;
            }
            after = TrimStrLeft(after);
            if (!after.len || after[0] != ']') [[unlikely]] {
                ThrowError<Napi::Error>(env, "Expected ']' after array length");
                return nullptr;
            }
            after = after.Take(1, after.len - 1);

            if (!arrays.Available()) [[unlikely]] {
                ThrowError<Napi::Error>(env, "Too many type indirections");
                return nullptr;
            }

            arrays.Append(len);
        } else if (SplitIdentifier(after) == "const") {
            after = after.Take(6, after.len - 6);
        } else {
            after = TrimStrRight(after);

            if (after.len) [[unlikely]] {
                ThrowError<Napi::Error>(env, "Unexpected character '%1' in type specifier", after[0]);
                return nullptr;
            }

            break;
        }

        after = TrimStrLeft(after);
    }

    const TypeInfo *type = instance->types_map.FindValue(name, nullptr);

    if (!type) {
        // Try with cleaned up spaces
        if (name.len < 256) {
            LocalArray<char, 256> buf;
            for (Size i = 0; i < name.len; i++) {
                char c = name[i];

                if (IsAsciiWhite(c)) {
                    buf.Append(' ');
                    while (++i < name.len && IsAsciiWhite(name[i]));
                    i--;
                } else {
                    buf.Append(c);
                }
            }

            type = instance->types_map.FindValue(buf, nullptr);
        }

        if (!type)
            return nullptr;
    }

    for (int i = 0;; i++) {
        if (disposables & (1u << i)) {
            if (type->primitive != PrimitiveKind::Pointer &&
                    type->primitive != PrimitiveKind::String &&
                    type->primitive != PrimitiveKind::String16 &&
                    type->primitive != PrimitiveKind::String32) [[unlikely]] {
                ThrowError<Napi::Error>(env, "Cannot create disposable type for non-pointer");
                return nullptr;
            }

            TypeInfo *copy = instance->types.AppendDefault();

            memcpy((void *)copy, (const void *)type, K_SIZE(*type));
            copy->name = Fmt(&instance->str_alloc, "<anonymous_%1>", instance->types.count).ptr;
            copy->members.allocator = GetNullAllocator();
            copy->members.allocator = GetNullAllocator();
            memset((void *)&copy->defn, 0, K_SIZE(copy->defn));

            static_assert(!std::is_polymorphic_v<Napi::ObjectReference>);

            copy->dispose = [](Napi::Env env, const TypeInfo *, const void *ptr) {
                InstanceData *instance = env.GetInstanceData<InstanceData>();

                free((void *)ptr);
                instance->stats.disposed++;
            };

            type = copy;
        }

        if (i >= arrays.len)
            break;
        Size len = arrays[i];

        if (len > 0) {
            if (type->primitive == PrimitiveKind::Void) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Cannot make array of empty or incomplete type");
                return nullptr;
            }

            if (len > instance->config.max_type_size / type->size) {
                ThrowError<Napi::TypeError>(env, "Array length is too high (max = %1)", instance->config.max_type_size / type->size);
                return nullptr;
            }

            type = MakeArrayType(instance, type, len);
            K_ASSERT(type);
        } else {
            K_ASSERT(!len);

            type = MakePointerType(instance, type);
            K_ASSERT(type);
        }
    }

    return type;
}

TypeInfo *MakePointerType(InstanceData *instance, const TypeInfo *ref, int count)
{
    K_ASSERT(count >= 1);

    for (int i = 0; i < count; i++) {
        char name_buf[256];
        Fmt(name_buf, "%1%2*", ref->name, EndsWith(ref->name, "*") ? "" : " ");

        bool inserted;
        auto bucket = instance->types_map.InsertOrGetDefault(name_buf, &inserted);

        if (inserted) {
            TypeInfo *type = instance->types.AppendDefault();

            type->name = DuplicateString(name_buf, &instance->str_alloc).ptr;

            if (ref->primitive != PrimitiveKind::Prototype) {
                type->primitive = PrimitiveKind::Pointer;
                type->size = K_SIZE(void *);
                type->align = K_SIZE(void *);
                type->ref.type = ref;
                type->ref.stride = ref->size;
                type->hint = (ref->flags & (int)TypeFlag::HasTypedArray) ? ArrayHint::Typed : ArrayHint::Array;
            } else {
                type->primitive = PrimitiveKind::Callback;
                type->size = K_SIZE(void *);
                type->align = K_SIZE(void *);
                type->ref.type = instance->void_type; // Dummy
                type->proto = ref->proto;
            }

            bucket->key = type->name;
            bucket->value = type;
        }

        ref = bucket->value;
    }

    return (TypeInfo *)ref;
}

static TypeInfo *MakeArrayType(InstanceData *instance, const TypeInfo *ref, Size len, ArrayHint hint, bool insert)
{
    K_ASSERT(len >= 0);
    K_ASSERT(len <= instance->config.max_type_size / ref->size);

    TypeInfo *type = instance->types.AppendDefault();

    type->name = Fmt(&instance->str_alloc, "%1[%2]", ref->name, len).ptr;

    type->primitive = PrimitiveKind::Array;
    type->align = ref->align;
    type->size = (int32_t)(len * ref->size);
    type->ref.type = ref;
    type->ref.stride = ref->size;
    type->hint = hint;

    if (insert) {
        bool inserted;
        type = (TypeInfo *)*instance->types_map.InsertOrGet(type->name, type, &inserted);
        instance->types.RemoveLast(!inserted);
    }

    return type;
}

TypeInfo *MakeArrayType(InstanceData *instance, const TypeInfo *ref, Size len)
{
    ArrayHint hint = {};

    if (ref->flags & (int)TypeFlag::IsCharLike) {
        hint = ArrayHint::String;
    } else if (ref->flags & (int)TypeFlag::HasTypedArray) {
        hint = ArrayHint::Typed;
    } else {
        hint = ArrayHint::Array;
    }

    return MakeArrayType(instance, ref, len, hint, true);
}

TypeInfo *MakeArrayType(InstanceData *instance, const TypeInfo *ref, Size len, ArrayHint hint)
{
    return MakeArrayType(instance, ref, len, hint, false);
}

Napi::Value WrapType(Napi::Env env, const TypeInfo *type, bool freeze)
{
    if (type->defn.IsEmpty()) {
#if defined(EXTERNAL_TYPES)
        Napi::Object defn = Napi::Object::New(env);
#else
        InstanceData *instance = env.GetInstanceData<InstanceData>();

        Napi::External<TypeInfo> external = Napi::External<TypeInfo>::New(env, (TypeInfo *)type);
        Napi::Object defn = instance->construct_type.New({ external });
        SetValueTag(env, defn, &TypeObjectMarker);
#endif

        defn.Set("name", Napi::String::New(env, type->name));
        defn.Set("primitive", PrimitiveKindNames[(int)type->primitive]);
        defn.Set("size", Napi::Number::New(env, (double)type->size));
        defn.Set("alignment", Napi::Number::New(env, (double)type->align));
        defn.Set("disposable", Napi::Boolean::New(env, !!type->dispose));

        // Assign before to avoid possible recursion crash
        type->defn = Napi::Persistent(defn);

        switch (type->primitive) {
            case PrimitiveKind::Void:
            case PrimitiveKind::Bool:
            case PrimitiveKind::Int8:
            case PrimitiveKind::UInt8:
            case PrimitiveKind::Int16:
            case PrimitiveKind::Int16S:
            case PrimitiveKind::UInt16:
            case PrimitiveKind::UInt16S:
            case PrimitiveKind::Int32:
            case PrimitiveKind::Int32S:
            case PrimitiveKind::UInt32:
            case PrimitiveKind::UInt32S:
            case PrimitiveKind::Int64:
            case PrimitiveKind::Int64S:
            case PrimitiveKind::UInt64:
            case PrimitiveKind::UInt64S:
            case PrimitiveKind::String:
            case PrimitiveKind::String16:
            case PrimitiveKind::String32:
            case PrimitiveKind::Float32:
            case PrimitiveKind::Float64: {} break;

            case PrimitiveKind::Array: {
                uint32_t len = type->size / type->ref.type->size;
                defn.Set("length", Napi::Number::New(env, (double)len));
                defn.Set("hint", ArrayHintNames[(int)type->hint]);
            } [[fallthrough]];
            case PrimitiveKind::Pointer: {
                Napi::Value value = WrapType(env, type->ref.type);
                defn.Set("ref", value);
            } break;
            case PrimitiveKind::Record:
            case PrimitiveKind::Union: {
                Napi::Object members = Napi::Object::New(env);

                for (const RecordMember &member: type->members) {
                    Napi::Object obj = Napi::Object::New(env);

                    obj.Set("name", member.name);
                    obj.Set("type", WrapType(env, member.type));
                    obj.Set("offset", member.offset);

                    members.Set(member.name, obj);
                }

                members.Freeze();
                defn.Set("members", members);
            } break;

            case PrimitiveKind::Prototype:
            case PrimitiveKind::Callback: {
                defn.Set("proto", DescribeFunction(env, type->proto));
            } break;
        }

        if (freeze) {
            defn.Freeze();
        }
    }

#if defined(EXTERNAL_TYPES)
    Napi::External<TypeInfo> external = Napi::External<TypeInfo>::New(env, (TypeInfo *)type);
    SetValueTag(env, external, &TypeObjectMarker);

    return external;
#else
    return type->defn.Value();
#endif
}

const TypeInfo *ReshapeType(InstanceData *instance, const TypeInfo *type, int32_t stride, uint16_t flags)
{
    K_ASSERT(!type->defn.IsEmpty());

    if (!type->reshaped) {
        TypeInfo *reshaped = nullptr;

        switch (type->primitive) {
            case PrimitiveKind::Record: {
                reshaped = instance->types.AppendDefault();

                memcpy((void *)reshaped, (const void *)type, K_SIZE(*type));
                memset((void *)&reshaped->members, 0, K_SIZE(reshaped->members));
                reshaped->members.Reserve(type->members.len);
                reshaped->size = 0;
                reshaped->flags |= flags;
                memset((void *)&reshaped->defn, 0, K_SIZE(reshaped->defn));

                Napi::Object defn = type->defn.Value();
                reshaped->defn = Napi::Persistent(defn);

                for (RecordMember member: type->members) {
                    member.offset = reshaped->size;
                    member.type = ReshapeType(instance, member.type, stride, flags);

                    reshaped->members.Append(member);
                    reshaped->size += AlignLen(member.type->size, stride);
                }
            } break;

            case PrimitiveKind::Array: {
                reshaped = instance->types.AppendDefault();

                memcpy((void *)reshaped, (const void *)type, K_SIZE(*type));
                reshaped->ref.stride = stride;
                reshaped->size = (type->size / type->ref.stride) * stride;
                memset((void *)&reshaped->defn, 0, K_SIZE(reshaped->defn));
                reshaped->flags |= flags;

                Napi::Object defn = type->defn.Value();
                reshaped->defn = Napi::Persistent(defn);
            } break;

            default: { reshaped = (TypeInfo *)type; } break;
        }

        type->reshaped = reshaped;
    }

    return type->reshaped;
}

bool CanPassType(const TypeInfo *type, int directions)
{
    if (type->countedby)
        return false;

    if (directions & 2) {
        if (type->primitive == PrimitiveKind::Pointer)
            return true;
        if (type->primitive == PrimitiveKind::String)
            return true;
        if (type->primitive == PrimitiveKind::String16)
            return true;
        if (type->primitive == PrimitiveKind::String32)
            return true;

        return false;
    } else {
        if (type->primitive == PrimitiveKind::Void)
            return false;
        if (type->primitive == PrimitiveKind::Array)
            return false;
        if (type->primitive == PrimitiveKind::Prototype)
            return false;
        if (type->primitive == PrimitiveKind::Callback && type->proto->variadic)
            return false;

        return true;
    }
}

bool CanReturnType(const TypeInfo *type)
{
    if (type->countedby)
        return false;

    if (type->primitive == PrimitiveKind::Void && !TestStr(type->name, "void"))
        return false;
    if (type->primitive == PrimitiveKind::Array)
        return false;
    if (type->primitive == PrimitiveKind::Prototype)
        return false;

    return true;
}

bool CanStoreType(const TypeInfo *type)
{
    if (type->primitive == PrimitiveKind::Void)
        return false;
    if (type->primitive == PrimitiveKind::Prototype)
        return false;
    if (type->primitive == PrimitiveKind::Callback && type->proto->variadic)
        return false;

    return true;
}

const char *GetValueType(const InstanceData *instance, napi_value value)
{
    Napi::Env env = instance->env;
    napi_valuetype kind = GetKindOf(env, value);

    if (kind == napi_external) {
        if (CheckValueTag(env, value, &CastMarker)) {
            Napi::External<ValueCast> external = Napi::External<ValueCast>(env, value);
            ValueCast *cast = external.Data();

            return cast->type->name;
        }

        if (CheckValueTag(env, value, &LibraryHandleMarker))
            return "LibraryHandle";
        if (CheckValueTag(env, value, &TypeObjectMarker))
            return "TypeObject";

        if (CheckValueTag(env, value, &UnionValueMarker)) {
            UnionValue *u = nullptr;
            napi_unwrap(env, value, (void **)&u);

            return u->GetType()->name;
        }

        for (const TypeInfo &type: instance->types) {
            if (type.ref.type && CheckValueTag(env, value, type.ref.type))
                return type.name;
        }
    }

    if (IsArray(env, value)) {
        return "Array";
    } else if (IsTypedArray(env, value)) {
        Napi::TypedArray array = Napi::TypedArray(env, value);

        switch (array.TypedArrayType()) {
            case napi_int8_array: return "Int8Array";
            case napi_uint8_array: return "Uint8Array";
            case napi_uint8_clamped_array: return "Uint8ClampedArray";
            case napi_int16_array: return "Int16Array";
            case napi_uint16_array: return "Uint16Array";
            case napi_int32_array: return "Int32Array";
            case napi_uint32_array: return "Uint32Array";
            case napi_float16_array: return "Float16Array";
            case napi_float32_array: return "Float32Array";
            case napi_float64_array: return "Float64Array";
            case napi_bigint64_array: return "BigInt64Array";
            case napi_biguint64_array: return "BigUint64Array";
        }
    } else if (IsArrayBuffer(env, value)) {
        return "ArrayBuffer";
    } else if (IsBuffer(env, value)) {
        return "Buffer";
    }

    switch (kind) {
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

void SetValueTag(napi_env env, napi_value value, const void *marker)
{
    static_assert(K_SIZE(TypeInfo) >= 16);

    // We used to make a temporary tag on the stack with lower set to a constant value and
    // upper to the pointer address, but this broke in Node 20.12 and Node 21.6 due to ExternalWrapper
    // storing a pointer to the tag and not the tag value itself (which seems wrong, but anyway).
    //
    // Since this is no longer an option, we just type alias whatever marker points to to napi_type_tag.
    // This may seem gross, but we disable strict aliasing anyway, so as long as what marker points
    // to is bigger than 16 bytes and does not change, it works!
    // Which holds true for us: the main thing pointed to is TypeInfo, which is constant and big enough,
    // and the few other markers we use, such as CastMarker, are actual const napi_type_tag structs.
    const napi_type_tag *tag = (const napi_type_tag *)marker;

    napi_status status = napi_type_tag_object(env, value, tag);
    K_ASSERT(status == napi_ok);
}

bool CheckValueTag(napi_env env, napi_value value, const void *marker)
{
    K_ASSERT(IsObject(env, value) || GetKindOf(env, value) == napi_external);

    bool match = false;

    const napi_type_tag *tag = (const napi_type_tag *)marker;
    napi_check_object_type_tag(env, value, tag, &match);

    return match;
}

void DeleteReferenceSafe(napi_env env, napi_ref ref)
{
    if (node_api_post_finalizer) {
        node_api_post_finalizer(env, [](napi_env env, void *data, void *) {
            napi_ref ref = (napi_ref)data;
            napi_delete_reference(env, ref);
        }, (void *)ref, nullptr);
    } else {
        napi_delete_reference(env, ref);
    }
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

    K_UNREACHABLE();
}

Napi::String MakeStringFromUTF32(Napi::Env env, const char32_t *ptr, Size len)
{
    static const char16_t ReplacementChar = 0xFFFD;

    HeapArray<char16_t> buf;
    buf.Reserve(len * 2);

    for (Size i = 0; i < len; i++) {
        char32_t uc = ptr[i];

        if (uc <= 0xFFFF) {
            if (uc < 0xD800 || uc > 0xDFFF) {
                buf.Append((char16_t)uc);
            } else {
                buf.Append(ReplacementChar);
            }
        } else if (uc <= 0x10FFFF) {
            uc -= 0x0010000UL;

            buf.Append((char16_t)((uc >> 10) + 0xD800));
            buf.Append((char16_t)((uc & 0x3FFul) + 0xDC00));
        } else {
            buf.Append(ReplacementChar);
        }
    }

    Napi::String str = Napi::String::New(env, buf.ptr, buf.len);
    return str;
}

static uint32_t DecodeDynamicLength(const uint8_t *origin, const RecordMember &by)
{
    const uint8_t *src = origin + by.offset;

    switch (by.type->primitive) {
        case PrimitiveKind::Int8: {
            int8_t i = *(int8_t *)src;
            return (uint32_t)i;
        } break;
        case PrimitiveKind::UInt8: {
            uint8_t u = *(uint8_t *)src;
            return (uint32_t)u;
        } break;
        case PrimitiveKind::Int16: {
            int16_t i;
            memcpy(&i, src, 2);
            return (uint32_t)i;
        } break;
        case PrimitiveKind::Int16S: {
            int16_t i;
            memcpy(&i, src, 2);
            return (uint32_t)ReverseBytes(i);
        } break;
        case PrimitiveKind::UInt16: {
            uint16_t u;
            memcpy(&u, src, 2);
            return (uint32_t)u;
        } break;
        case PrimitiveKind::UInt16S: {
            uint16_t u;
            memcpy(&u, src, 2);
            return (uint32_t)ReverseBytes(u);
        } break;
        case PrimitiveKind::Int32: {
            int32_t i;
            memcpy(&i, src, 4);
            return (uint32_t)i;
        } break;
        case PrimitiveKind::Int32S: {
            int32_t i;
            memcpy(&i, src, 4);
            return (uint32_t)ReverseBytes(i);
        } break;
        case PrimitiveKind::UInt32: {
            uint32_t u;
            memcpy(&u, src, 4);
            return (uint32_t)u;
        } break;
        case PrimitiveKind::UInt32S: {
            uint32_t u;
            memcpy(&u, src, 4);
            return (uint32_t)ReverseBytes(u);
        } break;
        case PrimitiveKind::Int64: {
            int64_t i;
            memcpy(&i, src, 8);
            return (uint32_t)i;
        } break;
        case PrimitiveKind::Int64S: {
            int64_t i;
            memcpy(&i, src, 8);
            return (uint32_t)ReverseBytes(i);
        } break;
        case PrimitiveKind::UInt64: {
            uint64_t u;
            memcpy(&u, src, 8);
            return (uint32_t)u;
        } break;
        case PrimitiveKind::UInt64S: {
            uint64_t u;
            memcpy(&u, src, 8);
            return (uint32_t)ReverseBytes(u);
        } break;

        case PrimitiveKind::Void:
        case PrimitiveKind::Bool:
        case PrimitiveKind::String:
        case PrimitiveKind::String16:
        case PrimitiveKind::String32:
        case PrimitiveKind::Pointer:
        case PrimitiveKind::Callback:
        case PrimitiveKind::Record:
        case PrimitiveKind::Union:
        case PrimitiveKind::Array:
        case PrimitiveKind::Float32:
        case PrimitiveKind::Float64:
        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

    K_UNREACHABLE();
}

template <typename SetFunc>
static FORCE_INLINE void DecodeObject(InstanceData *instance, const uint8_t *origin, const TypeInfo *type, SetFunc set)
{
    K_ASSERT(type->primitive == PrimitiveKind::Record);

    Napi::Env env = instance->env;
    Span<const RecordMember> members = type->members;

    for (Size i = 0; i < members.len; i++) {
        const RecordMember &member = members[i];
        const uint8_t *src = origin + member.offset;

        switch (member.type->primitive) {
            case PrimitiveKind::Void: { K_UNREACHABLE(); } break;

            case PrimitiveKind::Bool: {
                bool b = *(bool *)src;
                set(i, member, Napi::Boolean::New(env, b));
            } break;
            case PrimitiveKind::Int8: {
                int8_t v = *(int8_t *)src;
                set(i, member, NewInt(env, v));
            } break;
            case PrimitiveKind::UInt8: {
                uint8_t v = *(uint8_t *)src;
                set(i, member, NewInt(env, v));
            } break;
            case PrimitiveKind::Int16: {
                int16_t v;
                memcpy(&v, src, 2);
                set(i, member, NewInt(env, v));
            } break;
            case PrimitiveKind::Int16S: {
                int16_t v;
                memcpy(&v, src, 2);
                set(i, member, NewInt(env, ReverseBytes(v)));
            } break;
            case PrimitiveKind::UInt16: {
                uint16_t v;
                memcpy(&v, src, 2);
                set(i, member, NewInt(env, v));
            } break;
            case PrimitiveKind::UInt16S: {
                uint16_t v;
                memcpy(&v, src, 2);
                set(i, member, NewInt(env, ReverseBytes(v)));
            } break;
            case PrimitiveKind::Int32: {
                int32_t v;
                memcpy(&v, src, 4);
                set(i, member, NewInt(env, v));
            } break;
            case PrimitiveKind::Int32S: {
                int32_t v;
                memcpy(&v, src, 4);
                set(i, member, NewInt(env, ReverseBytes(v)));
            } break;
            case PrimitiveKind::UInt32: {
                uint32_t v;
                memcpy(&v, src, 4);
                set(i, member, NewInt(env, v));
            } break;
            case PrimitiveKind::UInt32S: {
                uint32_t v;
                memcpy(&v, src, 4);
                set(i, member, NewInt(env, ReverseBytes(v)));
            } break;
            case PrimitiveKind::Int64: {
                int64_t v;
                memcpy(&v, src, 8);
                set(i, member, NewInt(env, v));
            } break;
            case PrimitiveKind::Int64S: {
                int64_t v;
                memcpy(&v, src, 8);
                set(i, member, NewInt(env, ReverseBytes(v)));
            } break;
            case PrimitiveKind::UInt64: {
                uint64_t v;
                memcpy(&v, src, 8);
                set(i, member, NewInt(env, v));
            } break;
            case PrimitiveKind::UInt64S: {
                uint64_t v;
                memcpy(&v, src, 8);
                set(i, member, NewInt(env, ReverseBytes(v)));
            } break;
            case PrimitiveKind::String: {
                const char *str;
                memcpy(&str, src, K_SIZE(void *));
                set(i, member, str ? Napi::String::New(env, str) : env.Null());

                if (member.type->dispose) {
                    member.type->dispose(env, member.type, str);
                }
            } break;
            case PrimitiveKind::String16: {
                const char16_t *str16;
                memcpy(&str16, src, K_SIZE(void *));
                set(i, member, str16 ? Napi::String::New(env, str16) : env.Null());

                if (member.type->dispose) {
                    member.type->dispose(env, member.type, str16);
                }
            } break;
            case PrimitiveKind::String32: {
                const char32_t *str32;
                memcpy(&str32, src, K_SIZE(void *));
                set(i, member, str32 ? MakeStringFromUTF32(env, str32) : env.Null());
            } break;
            case PrimitiveKind::Pointer: {
                void *ptr2;
                memcpy(&ptr2, src, K_SIZE(void *));

                if (member.countedby >= 0) {
                    const RecordMember &by = members[member.countedby];
                    uint32_t len = DecodeDynamicLength(origin, by);

                    napi_value value = DecodeArray(instance, (const uint8_t *)ptr2, member.type, len);
                    set(i, member, value);
                } else {
                    napi_value p = ptr2 ? WrapPointer(env, member.type->ref.type, ptr2) : env.Null();
                    set(i, member, p);
                }

                if (member.type->dispose) {
                    member.type->dispose(env, member.type, ptr2);
                }
            } break;
            case PrimitiveKind::Callback: {
                void *ptr2;
                memcpy(&ptr2, src, K_SIZE(void *));

                napi_value p = ptr2 ? WrapPointer(env, member.type->ref.type, ptr2) : env.Null();
                set(i, member, p);

                if (member.type->dispose) {
                    member.type->dispose(env, member.type, ptr2);
                }
            } break;
            case PrimitiveKind::Record:
            case PrimitiveKind::Union: {
                napi_value obj2 = DecodeObject(instance, src, member.type);
                set(i, member, obj2);
            } break;
            case PrimitiveKind::Array: {
                if (member.countedby >= 0) {
                    const RecordMember &by = members[member.countedby];

                    uint32_t len = DecodeDynamicLength(origin, by);
                    uint32_t max = member.type->size / member.type->ref.type->size;

                    // Silently truncate result
                    len = std::min(len, max);

                    napi_value value = DecodeArray(instance, src, member.type, len);
                    set(i, member, value);
                } else {
                    napi_value value = DecodeArray(instance, src, member.type);
                    set(i, member, value);
                }
            } break;
            case PrimitiveKind::Float32: {
                float f;
                memcpy(&f, src, 4);
                set(i, member, Napi::Number::New(env, (double)f));
            } break;
            case PrimitiveKind::Float64: {
                double d;
                memcpy(&d, src, 8);
                set(i, member, Napi::Number::New(env, d));
            } break;

            case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
        }
    }
}

napi_value DecodeObject(InstanceData *instance, const uint8_t *origin, const TypeInfo *type)
{
    Napi::Env env = instance->env;

    // We can't decode unions because we don't know which member is valid
    if (type->primitive == PrimitiveKind::Union) {
        Napi::External<void> external = Napi::External<void>::New(env, (void *)origin);
        Napi::Object wrapper = type->construct.New({ external }).As<Napi::Object>();
        SetValueTag(env, wrapper, &UnionValueMarker);

        return wrapper;
    }

    // Only supported in recent Node versions, and still experimental at this time
    if (node_api_create_object_with_properties && type->members.len <= 256) {
        napi_value properties[256];
        napi_value values[256];

        DecodeObject(instance, origin, type, [&](Size i, const RecordMember &member, napi_value value) {
            napi_status status = napi_get_reference_value(env, member.key, &properties[i]);
            K_ASSERT(status == napi_ok);

            values[i] = value;
        });

        napi_value obj;
        napi_status status = node_api_create_object_with_properties(env, instance->object_constructor.Value(), properties, values, type->members.len, &obj);
        K_ASSERT(status == napi_ok);

        return obj;
    }

    Napi::Object obj = Napi::Object::New(env);
    DecodeObject(instance, obj, origin, type);

    return obj;
}

void DecodeObject(InstanceData *instance, napi_value obj, const uint8_t *origin, const TypeInfo *type)
{
    Napi::Env env = instance->env;

    if (node_api_create_property_key_utf8) {
        DecodeObject(instance, origin, type, [&](Size i, const RecordMember &member, napi_value value) {
            napi_value key = nullptr;
            napi_get_reference_value(env, member.key, &key);

            napi_status status = napi_set_property(env, obj, key, value);
            K_ASSERT(status == napi_ok);
        });
    } else {
        DecodeObject(instance, origin, type, [&](Size i, const RecordMember &member, napi_value value) {
            napi_status status = napi_set_named_property(env, obj, member.name, value);
            K_ASSERT(status == napi_ok);
        });
    }
}

napi_value DecodeArray(InstanceData *instance, const uint8_t *origin, const TypeInfo *type)
{
    K_ASSERT(type->primitive == PrimitiveKind::Array);

    uint32_t len = type->size / type->ref.stride;
    return DecodeArray(instance, origin, type, len);
}

napi_value DecodeArray(InstanceData *instance, const uint8_t *origin, const TypeInfo *type, uint32_t len)
{
    K_ASSERT(type->primitive == PrimitiveKind::Array || type->primitive == PrimitiveKind::Pointer);

    Napi::Env env = instance->env;
    const TypeInfo *ref = type->ref.type;
    int32_t stride = type->ref.stride;

    if (type->hint == ArrayHint::Typed) {
#define POP_TYPEDARRAY(TypedArrayType, CType) \
            do { \
                Napi::TypedArrayType array = Napi::TypedArrayType::New(env, len); \
                Span<uint8_t> buffer = MakeSpan((uint8_t *)array.ArrayBuffer().Data(), (Size)len * K_SIZE(CType)); \
                 \
                DecodeBuffer(buffer, origin, type); \
                 \
                return array; \
            } while (false)

        switch (ref->primitive) {
            case PrimitiveKind::Int8: { POP_TYPEDARRAY(Int8Array, int8_t); } break;
            case PrimitiveKind::UInt8: { POP_TYPEDARRAY(Uint8Array, uint8_t); } break;
            case PrimitiveKind::Int16: { POP_TYPEDARRAY(Int16Array, int16_t); } break;
            case PrimitiveKind::Int16S: { POP_TYPEDARRAY(Int16Array, int16_t); } break;
            case PrimitiveKind::UInt16: { POP_TYPEDARRAY(Uint16Array, uint16_t); } break;
            case PrimitiveKind::UInt16S: { POP_TYPEDARRAY(Uint16Array, uint16_t); } break;
            case PrimitiveKind::Int32: { POP_TYPEDARRAY(Int32Array, int32_t); } break;
            case PrimitiveKind::Int32S: { POP_TYPEDARRAY(Int32Array, int32_t); } break;
            case PrimitiveKind::UInt32: { POP_TYPEDARRAY(Uint32Array, uint32_t); } break;
            case PrimitiveKind::UInt32S: { POP_TYPEDARRAY(Uint32Array, uint32_t); } break;
            case PrimitiveKind::Float32: { POP_TYPEDARRAY(Float32Array, float); } break;
            case PrimitiveKind::Float64: { POP_TYPEDARRAY(Float64Array, double); } break;

            case PrimitiveKind::Void:
            case PrimitiveKind::Bool:
            case PrimitiveKind::Int64:
            case PrimitiveKind::Int64S:
            case PrimitiveKind::UInt64:
            case PrimitiveKind::UInt64S:
            case PrimitiveKind::String:
            case PrimitiveKind::String16:
            case PrimitiveKind::String32:
            case PrimitiveKind::Pointer:
            case PrimitiveKind::Callback:
            case PrimitiveKind::Record:
            case PrimitiveKind::Union:
            case PrimitiveKind::Array:
            case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
        }

#undef POP_TYPEDARRAY
    } else if (type->hint == ArrayHint::String) {
        K_ASSERT(stride == ref->size);

        switch (ref->primitive) {
            case PrimitiveKind::Int8: {
                const char *ptr = (const char *)origin;
                size_t count = strnlen(ptr, (size_t)len);

                Napi::String str = Napi::String::New(env, ptr, count);
                return str;
            } break;
            case PrimitiveKind::Int16: {
                const char16_t *ptr = (const char16_t *)origin;
                Size count = NullTerminatedLength(ptr, len);

                Napi::String str = Napi::String::New(env, ptr, count);
                return str;
            } break;
            case PrimitiveKind::Int32: {
                const char32_t *ptr = (const char32_t *)origin;
                Size count = NullTerminatedLength(ptr, len);

                Napi::String str = MakeStringFromUTF32(env, ptr, count);
                return str;
            } break;

            case PrimitiveKind::Void:
            case PrimitiveKind::Bool:
            case PrimitiveKind::UInt8:
            case PrimitiveKind::Int16S:
            case PrimitiveKind::UInt16:
            case PrimitiveKind::UInt16S:
            case PrimitiveKind::Int32S:
            case PrimitiveKind::UInt32:
            case PrimitiveKind::UInt32S:
            case PrimitiveKind::Int64:
            case PrimitiveKind::Int64S:
            case PrimitiveKind::UInt64:
            case PrimitiveKind::UInt64S:
            case PrimitiveKind::String:
            case PrimitiveKind::String16:
            case PrimitiveKind::String32:
            case PrimitiveKind::Pointer:
            case PrimitiveKind::Callback:
            case PrimitiveKind::Record:
            case PrimitiveKind::Union:
            case PrimitiveKind::Array:
            case PrimitiveKind::Float32:
            case PrimitiveKind::Float64:
            case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
        }
    } else {
        K_ASSERT(type->hint == ArrayHint::Array);

        Napi::Array array = Napi::Array::New(env);
        DecodeElements(instance, array, origin, type, len);

        return array;
    }

    K_UNREACHABLE();
}

void DecodeElements(InstanceData *instance, napi_value array, const uint8_t *origin, const TypeInfo *type, uint32_t len)
{
    K_ASSERT(IsArray(instance->env, array));

    Napi::Env env = instance->env;
    const TypeInfo *ref = type->ref.type;
    int32_t stride = type->ref.stride;

    Size offset = 0;

#define POP_ARRAY(SetCode) \
        do { \
            for (uint32_t i = 0; i < len; i++) { \
                const uint8_t *src = origin + offset; \
                SetCode \
                offset += stride; \
            } \
        } while (false)

#define POP_INTEGERS(CType) \
        do { \
            POP_ARRAY({ \
                CType v = *(CType *)src; \
                napi_set_element(env, array, i, NewInt(env, v)); \
            }); \
        } while (false)
#define POP_INTEGERS_SWAP(CType) \
        do { \
            POP_ARRAY({ \
                CType v = *(CType *)src; \
                napi_set_element(env, array, i, NewInt(env, ReverseBytes(v))); \
            }); \
        } while (false)

    switch (ref->primitive) {
        case PrimitiveKind::Void: { K_UNREACHABLE(); } break;

        case PrimitiveKind::Bool: {
            POP_ARRAY({
                bool b = *(bool *)src;
                napi_set_element(env, array, i, Napi::Boolean::New(env, b));
            });
        } break;
        case PrimitiveKind::Int8: { POP_INTEGERS(int8_t); } break;
        case PrimitiveKind::UInt8: { POP_INTEGERS(uint8_t); } break;
        case PrimitiveKind::Int16: { POP_INTEGERS(int16_t); } break;
        case PrimitiveKind::Int16S: { POP_INTEGERS_SWAP(int16_t); } break;
        case PrimitiveKind::UInt16: { POP_INTEGERS(uint16_t); } break;
        case PrimitiveKind::UInt16S: { POP_INTEGERS_SWAP(uint16_t); } break;
        case PrimitiveKind::Int32: { POP_INTEGERS(int32_t); } break;
        case PrimitiveKind::Int32S: { POP_INTEGERS_SWAP(int32_t); } break;
        case PrimitiveKind::UInt32: { POP_INTEGERS(uint32_t); } break;
        case PrimitiveKind::UInt32S: { POP_INTEGERS_SWAP(uint32_t); } break;
        case PrimitiveKind::Int64: { POP_INTEGERS(int64_t); } break;
        case PrimitiveKind::Int64S: { POP_INTEGERS_SWAP(int64_t); } break;
        case PrimitiveKind::UInt64: { POP_INTEGERS(uint64_t); } break;
        case PrimitiveKind::UInt64S: { POP_INTEGERS_SWAP(uint64_t); } break;
        case PrimitiveKind::String: {
            POP_ARRAY({
                const char *str = *(const char **)src;
                napi_set_element(env, array, i, str ? Napi::String::New(env, str) : env.Null());

                if (ref->dispose) {
                    ref->dispose(env, ref, str);
                }
            });
        } break;
        case PrimitiveKind::String16: {
            POP_ARRAY({
                const char16_t *str16 = *(const char16_t **)src;
                napi_set_element(env, array, i, str16 ? Napi::String::New(env, str16) : env.Null());

                if (ref->dispose) {
                    ref->dispose(env, ref, str16);
                }
            });
        } break;
        case PrimitiveKind::String32: {
            POP_ARRAY({
                const char32_t *str32 = *(const char32_t **)src;
                napi_set_element(env, array, i, str32 ? MakeStringFromUTF32(env, str32) : env.Null());

                if (ref->dispose) {
                    ref->dispose(env, ref, str32);
                }
            });
        } break;
        case PrimitiveKind::Pointer: {
            POP_ARRAY({
                void *ptr2 = *(void **)src;

                napi_value p = ptr2 ? WrapPointer(env, ref->ref.type, ptr2) : env.Null();
                napi_set_element(env, array, i, p);

                if (ref->dispose) {
                    ref->dispose(env, ref, ptr2);
                }
            });
        } break;
        case PrimitiveKind::Callback: {
            POP_ARRAY({
                void *ptr2 = *(void **)src;

                napi_value p = ptr2 ? WrapPointer(env, ref->ref.type, ptr2) : env.Null();
                napi_set_element(env, array, i, p);

                if (ref->dispose) {
                    ref->dispose(env, ref, ptr2);
                }
            });
        } break;
        case PrimitiveKind::Record:
        case PrimitiveKind::Union: {
            POP_ARRAY({
                napi_value obj = DecodeObject(instance, src, ref);
                napi_set_element(env, array, i, obj);
            });
        } break;
        case PrimitiveKind::Array: {
            POP_ARRAY({
                napi_value value = DecodeArray(instance, src, ref);
                napi_set_element(env, array, i, value);
            });
        } break;
        case PrimitiveKind::Float32: {
            POP_ARRAY({
                float f;
                memcpy(&f, src, 4);

                napi_set_element(env, array, i, Napi::Number::New(env, (double)f));
            });
        } break;
        case PrimitiveKind::Float64: {
            POP_ARRAY({
                double d;
                memcpy(&d, src, 8);

                napi_set_element(env, array, i, Napi::Number::New(env, d));
            });
        } break;

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

#undef POP_INTEGERS_SWAP
#undef POP_INTEGERS

#undef POP_ARRAY
}

void DecodeBuffer(Span<uint8_t> buffer, const uint8_t *origin, const TypeInfo *type)
{
    const TypeInfo *ref = type->ref.type;
    int32_t stride = type->ref.stride;

    // Go fast if possible. Brrrrr!
    if (stride == ref->size) {
        MemCpy(buffer.ptr, origin, (size_t)buffer.len);
    } else {
        Size len = buffer.len / ref->size;

        for (Size i = 0; i < len; i++) {
            const uint8_t *src = origin + i * stride;
            uint8_t *dest = buffer.ptr + i * ref->size;

            memcpy(dest, src, ref->size);
        }
    }

#define SWAP(CType) \
        do { \
            CType *data = (CType *)buffer.ptr; \
            Size len = buffer.len / K_SIZE(CType); \
             \
            for (Size i = 0; i < len; i++) { \
                data[i] = ReverseBytes(data[i]); \
            } \
        } while (false)

    if (ref->primitive == PrimitiveKind::Int16S || ref->primitive == PrimitiveKind::UInt16S) {
        SWAP(uint16_t);
    } else if (ref->primitive == PrimitiveKind::Int32S || ref->primitive == PrimitiveKind::UInt32S) {
        SWAP(uint32_t);
    } else if (ref->primitive == PrimitiveKind::Int64S || ref->primitive == PrimitiveKind::UInt64S) {
        SWAP(uint64_t);
    }

#undef SWAP
}

napi_value Decode(Napi::Value value, Size offset, const TypeInfo *type, const Size *len)
{
    Napi::Env env = value.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    const uint8_t *src = nullptr;

    if (Span<uint8_t> buffer = {}; TryBuffer(env, value, &buffer)) {
        if (offset < 0) [[unlikely]] {
            ThrowError<Napi::Error>(env, "Offset must be >= 0");
            return env.Null();
        }
        if (buffer.len - offset < type->size) [[unlikely]] {
            ThrowError<Napi::Error>(env, "Expected buffer with size superior or equal to type %1 (%2 bytes)",
                                    type->name, type->size + offset);
            return env.Null();
        }

        src = (const uint8_t *)buffer.ptr;
    } else if (void *ptr = nullptr; TryPointer(env, value, &ptr)) {
        src = (const uint8_t *)ptr;
    } else {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for variable, expected pointer", GetValueType(instance, value));
        return env.Null();
    }

    if (!src)
        return env.Null();
    src += offset;

    return Decode(instance, src, type, len);
}

napi_value Decode(InstanceData *instance, const uint8_t *ptr, const TypeInfo *type, const Size *len)
{
    Napi::Env env = instance->env;

    if (len && type->primitive != PrimitiveKind::String &&
               type->primitive != PrimitiveKind::String16 &&
               type->primitive != PrimitiveKind::String32 &&
               type->primitive != PrimitiveKind::Prototype) {
        if (*len >= 0) {
            type = MakeArrayType(instance, type, *len);
        } else {
            switch (type->primitive) {
                case PrimitiveKind::Int8:
                case PrimitiveKind::UInt8: {
                    Size count = strlen((const char *)ptr);
                    type = MakeArrayType(instance, type, count);
                } break;
                case PrimitiveKind::Int16:
                case PrimitiveKind::UInt16: {
                    Size count = NullTerminatedLength((const char16_t *)ptr);
                    type = MakeArrayType(instance, type, count);
                } break;
                case PrimitiveKind::Int32:
                case PrimitiveKind::UInt32: {
                    Size count = NullTerminatedLength((const char32_t *)ptr);
                    type = MakeArrayType(instance, type, count);
                } break;

                case PrimitiveKind::Pointer: {
                    Size count = NullTerminatedLength((const void **)ptr);
                    type = MakeArrayType(instance, type, count);
                } break;

                default: {
                    ThrowError<Napi::TypeError>(env, "Cannot determine null-terminated length for type %1", type->name);
                    return env.Null();
                } break;
            }
        }
    }

#define RETURN_INT(Type, NewCall) \
        do { \
            Type v = *(Type *)ptr; \
            return NewCall(env, v); \
        } while (false)
#define RETURN_INT_SWAP(Type, NewCall) \
        do { \
            Type v = ReverseBytes(*(Type *)ptr); \
            return NewCall(env, v); \
        } while (false)

    switch (type->primitive) {
        case PrimitiveKind::Void: {
            ThrowError<Napi::TypeError>(env, "Cannot decode value of type %1", type->name);
            return env.Null();
        } break;

        case PrimitiveKind::Bool: {
            bool v = *(bool *)ptr;
            return Napi::Boolean::New(env, v);
        } break;
        case PrimitiveKind::Int8: { RETURN_INT(int8_t, NewInt); } break;
        case PrimitiveKind::UInt8: { RETURN_INT(uint8_t, NewInt); } break;
        case PrimitiveKind::Int16: { RETURN_INT(int16_t, NewInt); } break;
        case PrimitiveKind::Int16S: { RETURN_INT_SWAP(int16_t, NewInt); } break;
        case PrimitiveKind::UInt16: { RETURN_INT(uint16_t, NewInt); } break;
        case PrimitiveKind::UInt16S: { RETURN_INT_SWAP(uint16_t, NewInt); } break;
        case PrimitiveKind::Int32: { RETURN_INT(int32_t, NewInt); } break;
        case PrimitiveKind::Int32S: { RETURN_INT_SWAP(int32_t, NewInt); } break;
        case PrimitiveKind::UInt32: { RETURN_INT(uint32_t, NewInt); } break;
        case PrimitiveKind::UInt32S: { RETURN_INT_SWAP(uint32_t, NewInt); } break;
        case PrimitiveKind::Int64: { RETURN_INT(int64_t, NewInt); } break;
        case PrimitiveKind::Int64S: { RETURN_INT_SWAP(int64_t, NewInt); } break;
        case PrimitiveKind::UInt64: { RETURN_INT(uint64_t, NewInt); } break;
        case PrimitiveKind::UInt64S: { RETURN_INT_SWAP(uint64_t, NewInt); } break;
        case PrimitiveKind::String: {
            if (len) {
                const char *str = *(const char **)ptr;
                return str ? Napi::String::New(env, str, *len) : env.Null();
            } else {
                const char *str = *(const char **)ptr;
                return str ? Napi::String::New(env, str) : env.Null();
            }
        } break;
        case PrimitiveKind::String16: {
            if (len) {
                const char16_t *str16 = *(const char16_t **)ptr;
                return str16 ? Napi::String::New(env, str16, *len) : env.Null();
            } else {
                const char16_t *str16 = *(const char16_t **)ptr;
                return str16 ? Napi::String::New(env, str16) : env.Null();
            }
        } break;
        case PrimitiveKind::String32: {
            if (len) {
                const char32_t *str32 = *(const char32_t **)ptr;
                return str32 ? MakeStringFromUTF32(env, str32, *len) : env.Null();
            } else {
                const char32_t *str32 = *(const char32_t **)ptr;
                return str32 ? MakeStringFromUTF32(env, str32) : env.Null();
            }
        } break;
        case PrimitiveKind::Pointer: {
            void *ptr2 = *(void **)ptr;
            return ptr2 ? WrapPointer(env, type->ref.type, ptr2) : env.Null();
        } break;
        case PrimitiveKind::Callback: {
            void *ptr2 = *(void **)ptr;
            return ptr2 ? WrapPointer(env, type->ref.type, ptr2) : env.Null();
        } break;
        case PrimitiveKind::Record:
        case PrimitiveKind::Union: { return DecodeObject(instance, ptr, type); } break;
        case PrimitiveKind::Array: {
            napi_value array = DecodeArray(instance, ptr, type);
            return array;
        } break;
        case PrimitiveKind::Float32: {
            float f;
            memcpy(&f, ptr, 4);

            return Napi::Number::New(env, (double)f);
        } break;
        case PrimitiveKind::Float64: {
            double d;
            memcpy(&d, ptr, 8);

            return Napi::Number::New(env, d);
        } break;

        case PrimitiveKind::Prototype: {
            const FunctionInfo *proto = type->proto;
            K_ASSERT(!proto->variadic);
            K_ASSERT(!proto->lib);

            FunctionInfo *func = new FunctionInfo();
            K_DEFER { func->Unref(); };

            memcpy((void *)func, proto, K_SIZE(*proto));
            memset((void *)&func->parameters, 0, K_SIZE(func->parameters));
            memset((void *)&func->sync, 0, K_SIZE(func->sync));
            memset((void *)&func->async, 0, K_SIZE(func->async));

            func->name = "<anonymous>";
            func->native = (void *)ptr;
            func->parameters = proto->parameters;
            func->sync = proto->sync;
            func->async = proto->async;

            return WrapFunction(env, func);
        } break;
    }

#undef RETURN_BIGINT
#undef RETURN_INT

    return env.Null();
}

bool Encode(Napi::Value ref, Size offset, Napi::Value value, const TypeInfo *type, const Size *len)
{
    Napi::Env env = ref.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    uint8_t *dest = nullptr;

    if (Span<uint8_t> buffer = {}; TryBuffer(env, ref, &buffer)) {
        if (offset < 0) [[unlikely]] {
            ThrowError<Napi::Error>(env, "Offset must be >= 0");
            return env.Null();
        }
        if (buffer.len - offset < type->size) [[unlikely]] {
            ThrowError<Napi::Error>(env, "Expected buffer with size superior or equal to type %1 (%2 bytes)",
                                    type->name, type->size + offset);
            return env.Null();
        }

        dest = (uint8_t *)buffer.ptr;
    } else if (void *ptr = nullptr; TryPointer(env, ref, &ptr)) {
        dest = (uint8_t *)ptr;
    } else {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for reference, expected pointer", GetValueType(instance, ref));
        return env.Null();
    }

    if (!dest) [[unlikely]] {
        ThrowError<Napi::Error>(env, "Cannot encode data in NULL pointer");
        return env.Null();
    }
    dest += offset;

    return Encode(instance, dest, value, type, len);
}

bool Encode(InstanceData *instance, uint8_t *origin, Napi::Value value, const TypeInfo *type, const Size *len)
{
    Napi::Env env = instance->env;

    if (len && type->primitive != PrimitiveKind::String &&
               type->primitive != PrimitiveKind::String16 &&
               type->primitive != PrimitiveKind::String32 &&
               type->primitive != PrimitiveKind::Prototype) {
        if (*len < 0) [[unlikely]] {
            ThrowError<Napi::TypeError>(env, "Automatic (negative) length is only supported when decoding");
            return env.Null();
        }

        type = MakeArrayType(instance, type, *len);
    }

    InstanceMemory mem = {};
    CallData call(env, instance, &mem, nullptr);

#define PUSH_INTEGER(CType) \
        do { \
            CType v; \
            if (!TryNumber(env, value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value)); \
                return false; \
            } \
             \
            *(CType *)origin = v; \
        } while (false)
#define PUSH_INTEGER_SWAP(CType) \
        do { \
            CType v; \
            if (!TryNumber(env, value, &v)) [[unlikely]] { \
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value)); \
                return false; \
            } \
             \
            *(CType *)origin = ReverseBytes(v); \
        } while (false)

    switch (type->primitive) {
        case PrimitiveKind::Void: { K_UNREACHABLE(); } break;

        case PrimitiveKind::Bool: {
            bool b;
            napi_status status = napi_get_value_bool(env, value, &b);

            if (status != napi_ok) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected boolean", GetValueType(instance, value));
                return false;
            }

            *(bool *)origin = b;
        } break;
        case PrimitiveKind::Int8: { PUSH_INTEGER(int8_t); } break;
        case PrimitiveKind::UInt8: { PUSH_INTEGER(uint8_t); } break;
        case PrimitiveKind::Int16: { PUSH_INTEGER(int16_t); } break;
        case PrimitiveKind::Int16S: { PUSH_INTEGER_SWAP(int16_t); } break;
        case PrimitiveKind::UInt16: { PUSH_INTEGER(uint16_t); } break;
        case PrimitiveKind::UInt16S: { PUSH_INTEGER_SWAP(uint16_t); } break;
        case PrimitiveKind::Int32: { PUSH_INTEGER(int32_t); } break;
        case PrimitiveKind::Int32S: { PUSH_INTEGER_SWAP(int32_t); } break;
        case PrimitiveKind::UInt32: { PUSH_INTEGER(uint32_t); } break;
        case PrimitiveKind::UInt32S: { PUSH_INTEGER_SWAP(uint32_t); } break;
        case PrimitiveKind::Int64: { PUSH_INTEGER(int64_t); } break;
        case PrimitiveKind::Int64S: { PUSH_INTEGER_SWAP(int64_t); } break;
        case PrimitiveKind::UInt64: { PUSH_INTEGER(uint64_t); } break;
        case PrimitiveKind::UInt64S: { PUSH_INTEGER_SWAP(uint64_t); } break;
        case PrimitiveKind::String: {
            const char *str;
            if (!call.PushString(value, 1, &str)) [[unlikely]]
                return false;
            *(const char **)origin = str;
        } break;
        case PrimitiveKind::String16: {
            const char16_t *str16;
            if (!call.PushString16(value, 1, &str16)) [[unlikely]]
                return false;
            *(const char16_t **)origin = str16;
        } break;
        case PrimitiveKind::String32: {
            const char32_t *str32;
            if (!call.PushString32(value, 1, &str32)) [[unlikely]]
                return false;
            *(const char32_t **)origin = str32;
        } break;
        case PrimitiveKind::Pointer: {
            void *ptr;
            if (!call.PushPointer(value, type, 1, &ptr)) [[unlikely]]
                return false;
            *(void **)origin = ptr;
        } break;
        case PrimitiveKind::Record:
        case PrimitiveKind::Union: {
            if (!IsObject(env, value)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected object", GetValueType(instance, value));
                return false;
            }

            Napi::Object obj = value.As<Napi::Object>();

            if (!call.PushObject(obj, type, origin))
                return false;
        } break;
        case PrimitiveKind::Array: {
            if (value.IsArray()) {
                Napi::Array array = Napi::Array(env, value);
                if (!call.PushNormalArray(array, type, type->size, origin))
                    return false;
            } else if (Span<uint8_t> buffer = {}; TryBuffer(env, value, &buffer)) {
                call.PushBuffer(buffer, type, origin);
            } else if (value.IsString()) {
                if (!call.PushStringArray(value, type, origin))
                    return false;
            } else {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected array", GetValueType(instance, value));
                return false;
            }
        } break;
        case PrimitiveKind::Float32: {
            float f;
            if (!TryNumber(env, value, &f)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                return false;
            }

            memcpy(origin, &f, 4);
        } break;
        case PrimitiveKind::Float64: {
            double d;
            if (!TryNumber(env, value, &d)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected number", GetValueType(instance, value));
                return false;
            }

            memcpy(origin, &d, 8);
        } break;
        case PrimitiveKind::Callback: {
            void *ptr;
            if (!TryPointer(env, value, &ptr)) [[unlikely]] {
                if (value.IsFunction()) {
                    ThrowError<Napi::Error>(env, "Cannot encode non-registered callback");
                } else {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value, expected %2", GetValueType(instance, value), type->name);
                }
                return false;
            }

            *(void **)origin = ptr;
        } break;

        case PrimitiveKind::Prototype: { K_UNREACHABLE(); } break;
    }

#undef PUSH_INTEGER_SWAP
#undef PUSH_INTEGER

    // Keep memory around if any was allocated
    if (mem.allocator.IsUsed()) {
        LinkedAllocator *copy = instance->encode_map.FindValue(origin, nullptr);

        if (!copy) {
            copy = instance->encode_allocators.AppendDefault();
            instance->encode_map.Set(origin, copy);
        }

        std::swap(mem.allocator, *copy);
    }

    return true;
}

static bool CanTypeAcceptCallbacks(const TypeInfo *type)
{
    if (type->primitive == PrimitiveKind::Pointer)
        return true;
    if (type->primitive == PrimitiveKind::Callback)
        return true;

    if (type->primitive == PrimitiveKind::Record ||
            type->primitive == PrimitiveKind::Union) {
        for (const RecordMember &member: type->members) {
            if (CanTypeAcceptCallbacks(member.type))
                return false;
        }
    }

    return true;
}

static bool CanUseFastCall(const FunctionInfo *func)
{
    if (func->parameters.len > 6)
        return false;

    // Fast calls basically skip CallData::Finalize(), which handles output arguments
    // and temporary callback trampolines. If the function does not use any
    // output argument and cannot accept callbacks (so no pointer or callback arguments),
    // we can skip finalization!

    for (const ParameterInfo &param: func->parameters) {
        if (param.directions & 2)
            return false;
        if (CanTypeAcceptCallbacks(param.type))
            return false;
    }

    return true;
}

Napi::Object DescribeFunction(Napi::Env env, const FunctionInfo *func)
{
    static const char *const DirectionNames[] = {
        nullptr,
        "Input",
        "Output",
        "Input/Output"
    };

    Napi::Object meta = Napi::Object::New(env);
    Napi::Array arguments = Napi::Array::New(env, func->parameters.len);

    meta.Set("name", Napi::String::New(env, func->name));
    meta.Set("arguments", arguments);
    meta.Set("result", WrapType(env, func->ret.type));

    for (Size i = 0; i < func->parameters.len; i++) {
        const ParameterInfo &param = func->parameters[i];
        Napi::Object obj = Napi::Object::New(env);

        obj.Set("type", WrapType(env, param.type));
        obj.Set("direction", Napi::String::New(env, DirectionNames[param.directions]));

        arguments.Set((uint32_t)i, obj);
    }

    meta.Freeze();

    return meta;
}

static bool IsDebugAsyncEnabled()
{
    static bool debug = GetDebugFlag("DEBUG_ASYNC");
    return debug;
}

Napi::Function WrapFunction(Napi::Env env, const FunctionInfo *func)
{
    Napi::Function wrapper;

    // Pick appropriate wrapper
    {
        napi_value value;

        if (func->variadic) {
            napi_status status = napi_create_function(env, func->name, NAPI_AUTO_LENGTH, TranslateVariadicCall, (void *)func->Ref(), &value);
            K_ASSERT(status == napi_ok);
        } else if (IsDebugAsyncEnabled()) {
            napi_status status = napi_create_function(env, func->name, NAPI_AUTO_LENGTH, TranslateNormalCallDebugAsync, (void *)func->Ref(), &value);
            K_ASSERT(status == napi_ok);
        } else if (!func->parameters.len) {
            InstanceData *instance = env.GetInstanceData<InstanceData>();
            napi_status status = napi_create_function(env, func->name, NAPI_AUTO_LENGTH, instance->translate_zero_call, (void *)func->Ref(), &value);
            K_ASSERT(status == napi_ok);
        } else if (CanUseFastCall(func)) {
            napi_status status = napi_create_function(env, func->name, NAPI_AUTO_LENGTH, TranslateFastCall, (void *)func->Ref(), &value);
            K_ASSERT(status == napi_ok);
        } else {
            napi_status status = napi_create_function(env, func->name, NAPI_AUTO_LENGTH, TranslateNormalCall, (void *)func->Ref(), &value);
            K_ASSERT(status == napi_ok);
        }

        wrapper = Napi::Function(env, value);
        wrapper.AddFinalizer([](Napi::Env, FunctionInfo *func) { func->Unref(); }, (FunctionInfo *)func);
    }

    if (!func->variadic) {
        napi_value value;
        napi_status status = napi_create_function(env, func->name, NAPI_AUTO_LENGTH, TranslateAsyncCall, (void *)func->Ref(), &value);
        K_ASSERT(status == napi_ok);

        Napi::Function async = Napi::Function(env, value);
        async.AddFinalizer([](Napi::Env, FunctionInfo *func) { func->Unref(); }, (FunctionInfo *)func);

        wrapper.Set("async", async);
    }

    Napi::Object meta = DescribeFunction(env, func);
    wrapper.Set("info", meta);

    return wrapper;
}

bool DetectCallConvention(Span<const char> name, CallConvention *out_convention)
{
    if (name == "__cdecl") {
        *out_convention = CallConvention::Cdecl;
        return true;
    } else if (name == "__stdcall") {
        *out_convention = CallConvention::Stdcall;
        return true;
    } else if (name == "__fastcall") {
        *out_convention = CallConvention::Fastcall;
        return true;
    } else if (name == "__thiscall") {
        *out_convention = CallConvention::Thiscall;
        return true;
    } else {
        return false;
    }
}

static int AnalyseFlatRec(const TypeInfo *type, int offset, int count, FunctionRef<void(const TypeInfo *type, int offset, int count)> func)
{
    if (type->primitive == PrimitiveKind::Record) {
        for (int i = 0; i < count; i++) {
            for (const RecordMember &member: type->members) {
                offset = AnalyseFlatRec(member.type, offset, 1, func);
            }
        }
    } else if (type->primitive == PrimitiveKind::Union) {
        for (int i = 0; i < count; i++) {
            for (const RecordMember &member: type->members) {
                AnalyseFlatRec(member.type, offset, 1, func);
            }
        }
        offset += count;
    } else if (type->primitive == PrimitiveKind::Array) {
        count *= type->size / type->ref.type->size;
        offset = AnalyseFlatRec(type->ref.type, offset, count, func);
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

void DumpMemory(const char *type, Span<const uint8_t> bytes)
{
    if (bytes.len) {
        PrintLn(StdErr, "%1 at 0x%2 (%3):", type, bytes.ptr, FmtMemSize(bytes.len));

        for (const uint8_t *ptr = bytes.begin(); ptr < bytes.end();) {
            Print(StdErr, "  [0x%1 %2 %3]  ", FmtHex((uintptr_t)ptr, 16),
                                              FmtInt((ptr - bytes.begin()) / sizeof(void *), 4, ' '),
                                              FmtInt(ptr - bytes.begin(), 4, ' '));
            for (int i = 0; ptr < bytes.end() && i < (int)sizeof(void *); i++, ptr++) {
                Print(StdErr, " %1", FmtHex(*ptr, 2));
            }
            PrintLn(StdErr);
        }
    }
}

}
