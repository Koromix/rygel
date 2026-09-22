// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "ffi.hh"
#include "call.hh"
#include "interp.hh"
#include "parser.hh"
#include "type.hh"
#include "util.hh"
#include "uv.hh"
#if defined(_WIN32)
    #include "win32.hh"
#endif
#include "errno.inc"

#if defined(_WIN32)
    #if !defined(NOMINMAX)
        #define NOMINMAX
    #endif
    #if !defined(WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <ntsecapi.h>
#else
    #include <dlfcn.h>
    #include <unistd.h>
    #include <sys/mman.h>
    #if !defined(MAP_STACK)
        #define MAP_STACK 0
    #endif
#endif
#include <wchar.h>

#include <napi.h>

namespace K {

// Value does not matter, the tag system uses memory addresses
const napi_type_tag LibraryHandleMarker = { 0xdb9b066e6f700474, 0x0aecd7e4c63fbda9 };
const napi_type_tag TypeObjectMarker = { 0x1cc449675b294374, 0xbb13a50e97dcb017 };
const napi_type_tag DirectionMarker = { 0xf9c306238b480580, 0xc2e168524a0823f5 };
const napi_type_tag UnionValueMarker = { 0x5eaf2245526a4c7d, 0x8c86c9ee2b96ffc8 };
const napi_type_tag CastMarker = { 0x77f459614a0a412f, 0x80b3dda1341dc8df };

SharedData shared;

// Some Node-API functions are loaded dynamically to work around bugs or because they are recent
napi_status (NAPI_CDECL *node_api_delete_reference)(node_api_basic_env env, napi_ref ref);
napi_status (NAPI_CDECL *node_api_get_buffer_info)(napi_env env, napi_value value, void **data, size_t *length);
napi_status (NAPI_CDECL *node_api_create_property_key_utf8)(napi_env env, const char* str, size_t length, napi_value* result);
napi_status (NAPI_CDECL *node_api_post_finalizer)(node_api_basic_env env, napi_finalize finalize_cb, void* finalize_data, void* finalize_hint);
napi_status (NAPI_CDECL *node_api_create_object_with_properties)(napi_env env, napi_value prototype_or_null, const napi_value *property_names,
                                                                 const napi_value *property_values, size_t property_count, napi_value *result);

static bool ChangeSize(InstanceData *instance, const char *name, Napi::Value value, Size min_size, Size max_size, Size *out_size)
{
    Napi::Env env = instance->env;

    if (!value.IsNumber()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for '%2', expected number", GetValueType(instance, value), name);
        return false;
    }

    int64_t size = value.As<Napi::Number>().Int64Value();

    if (size < min_size || size > max_size) {
        ThrowError<Napi::Error>(env, "Setting '%1' must be between %2 and %3", name, FmtMemSize(min_size), FmtMemSize(max_size));
        return false;
    }

    *out_size = (Size)size;
    return true;
}

static bool ChangeMemorySize(InstanceData *instance, const char *name, Napi::Value value, Size *out_size)
{
    const Size MinSize = Kibibytes(1);
    const Size MaxSize = Mebibytes(16);

    return ChangeSize(instance, name, value, MinSize, MaxSize, out_size);
}

static bool ChangeAsyncLimit(InstanceData *instance, const char *name, Napi::Value value, int max, int *out_limit)
{
    Napi::Env env = value.Env();

    if (!value.IsNumber()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for '%2', expected number", GetValueType(instance, value), name);
        return false;
    }

    int64_t n = value.As<Napi::Number>().Int64Value();

    if (n < 0 || n > max) {
        ThrowError<Napi::Error>(env, "Setting '%1' must be between 0 and %2", name, max);
        return false;
    }

    *out_limit = (int)n;
    return true;
}

static napi_value GetSetConfig(napi_env env, napi_callback_info info)
{
    napi_value arg;
    size_t count = 1;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, &arg, nullptr, (void **)&instance));

    if (count >= 1) {
        if (instance->memories.len) {
            ThrowError<Napi::Error>(env, "Cannot change Koffi configuration once a library has been loaded");
            return GetNull(env);
        }

        if (!IsObject(env, arg)) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for config, expected object", GetValueType(instance, arg));
            return GetNull(env);
        }

        decltype(instance->config) new_config = instance->config;
        int max_async_calls = new_config.resident_async_pools + new_config.max_temporaries;

        Napi::Object obj { env, arg };
        Napi::Array keys = Napi::Array(env, GetOwnPropertyNames(env, obj));

        for (uint32_t i = 0; i < keys.Length(); i++) {
            std::string key = keys.Get(i).As<Napi::String>();
            Napi::Value value = obj[key];

            if (key == "sync_stack_size") {
                if (!ChangeMemorySize(instance, key.c_str(), value, &new_config.sync_stack_size))
                    return GetNull(env);
            } else if (key == "sync_heap_size") {
                if (!ChangeMemorySize(instance, key.c_str(), value, &new_config.sync_heap_size))
                    return GetNull(env);
            } else if (key == "async_stack_size") {
                if (!ChangeMemorySize(instance, key.c_str(), value, &new_config.async_stack_size))
                    return GetNull(env);
            } else if (key == "async_heap_size") {
                if (!ChangeMemorySize(instance, key.c_str(), value, &new_config.async_heap_size))
                    return GetNull(env);
            } else if (key == "resident_async_pools") {
                if (!ChangeAsyncLimit(instance, key.c_str(), value, K_LEN(instance->memories.data), &new_config.resident_async_pools))
                    return GetNull(env);
            } else if (key == "max_async_calls") {
                if (!ChangeAsyncLimit(instance, key.c_str(), value, MaxAsyncCalls, &max_async_calls))
                    return GetNull(env);
            } else if (key == "max_type_size") {
                if (!ChangeSize(instance, key.c_str(), value, 32, Mebibytes(512), &new_config.max_type_size))
                    return GetNull(env);
            } else {
                ThrowError<Napi::Error>(env, "Unexpected config member '%1'", key.c_str());
                return GetNull(env);
            }
        }

        if (max_async_calls < new_config.resident_async_pools) {
            ThrowError<Napi::Error>(env, "Setting max_async_calls must be >= to resident_async_pools");
            return GetNull(env);
        }

        new_config.max_temporaries =  max_async_calls - new_config.resident_async_pools;
        instance->config = new_config;
    }

    Napi::Object obj = Napi::Object::New(env);

    obj.Set("sync_stack_size", instance->config.sync_stack_size);
    obj.Set("sync_heap_size", instance->config.sync_heap_size);
    obj.Set("async_stack_size", instance->config.async_stack_size);
    obj.Set("async_heap_size", instance->config.async_heap_size);
    obj.Set("resident_async_pools", instance->config.resident_async_pools);
    obj.Set("max_async_calls", instance->config.resident_async_pools + instance->config.max_temporaries);
    obj.Set("max_type_size", instance->config.max_type_size);

    return obj;
}

static napi_value GetStats(napi_env env, napi_callback_info info)
{
    InstanceData *instance;
    NAPI_OK(napi_get_cb_info(env, info, nullptr, nullptr, nullptr, (void **)&instance));

    Size callbacks;
    {
        std::lock_guard<std::mutex> lock(shared.mutex);
        callbacks = MaxTrampolines - shared.available.len;
    }

    Napi::Object obj = Napi::Object::New(env);

    obj.Set("disposed", instance->stats.disposed);
    obj.Set("callbacks", callbacks);

    return obj;
}

static inline bool CheckAlignment(int64_t align)
{
    bool valid = (align > 0) && (align <= 8 && !(align & (align - 1)));
    return valid;
}

static bool IsNameValid(const char *name)
{
    if (!IsXidStart(name[0]))
        return false;

    for (Size i = 1; name[i]; i++) {
        if (!IsXidContinue(name[i])) [[unlikely]]
            return false;
    }

    return true;
}

static bool MapType(Napi::Env env, InstanceData *instance, const TypeInfo *type, const char *name)
{
    if (!IsNameValid(name)) {
        ThrowError<Napi::Error>(env, "Invalid type name '%1'", name);
        return false;
    }

    bool inserted;
    instance->types_map.InsertOrGet(name, type, &inserted);

    if (!inserted) {
        ThrowError<Napi::Error>(env, "Duplicate type name '%1'", name);
        return false;
    }

    return true;
}

static bool FinalizeCompositeType(Napi::Env env, TypeInfo *type, PrimitiveKind primitive, Size size)
{
    type->primitive = primitive;

    if (node_api_create_property_key_utf8) {
        for (RecordMember &member: type->members) {
            napi_value key = nullptr;

            NAPI_OK(node_api_create_property_key_utf8(env, member.name, NAPI_AUTO_LENGTH, &key));
            NAPI_OK(napi_create_reference(env, key, 1, &member.key));
        }
    }

    size = AlignLen(size, type->align);
    if (!size) {
        ThrowError<Napi::Error>(env, "Empty type '%1' is not allowed in C", type->name);
        return false;
    }
    type->size = (int32_t)size;

    return true;
}

static Napi::Value CreateStructType(const Napi::CallbackInfo &info, bool pad)
{
    Napi::Env env = info.Env();
    InstanceData *instance = (InstanceData *)info.Data();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 or 2 arguments, got %1", info.Length());
        return env.Null();
    }

    bool skip = (info.Length() > 1);
    bool named = skip && !IsNullOrUndefined(env, info[0]);
    bool redefine = named && info[0].IsObject() && CheckValueTag(env, info[0], &TypeObjectMarker);

    if (named && !info[0].IsString() && !redefine) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for name, expected string", GetValueType(instance, info[0]));
        return env.Null();
    }
    if (!IsObject(env, info[skip])) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for members, expected object", GetValueType(instance, info[1]));
        return env.Null();
    }

    Napi::String name = info[0].As<Napi::String>();
    Napi::Object obj = info[skip].As<Napi::Object>();
    Napi::Array keys = Napi::Array(env, GetOwnPropertyNames(env, obj));

    K_DEFER_NC(err_guard, count = instance->types.count) {
        Size start = count + !skip;

        for (Size i = start; i < instance->types.count; i++) {
            const TypeInfo *it = &instance->types[i];
            const TypeInfo **ptr = instance->types_map.Find(it->name);

            if (ptr && *ptr == it) {
                instance->types_map.Remove(ptr);
            }
        }

        instance->types.RemoveFrom(count);
    };

    TypeInfo *type = instance->types.AppendDefault();
    TypeInfo *replace = nullptr;

    if (redefine) {
        TypeObject *defn = nullptr;
        NAPI_OK(napi_unwrap(env, name, (void **)&defn));

        replace = (TypeInfo *)defn->GetType();

        type->instance = instance;
        type->name = replace->name;

        if (replace->primitive != PrimitiveKind::Void || replace == instance->void_type) {
            ThrowError<Napi::TypeError>(env, "Cannot redefine non-opaque type %1", replace->name);
            return env.Null();
        }
    } else if (named) {
        type->instance = instance;
        type->name = DuplicateString(name.Utf8Value().c_str(), &instance->str_alloc).ptr;

        if (!MapType(env, instance, type, type->name))
            return env.Null();
    } else {
        type->instance = instance;
        type->name = Fmt(&instance->str_alloc, "<anonymous_%1>", instance->types.count).ptr;
    }

    type->primitive = PrimitiveKind::Void;
    type->align = 1;

    HashSet<const char *> members;
    Size size = 0;

    for (uint32_t i = 0; i < keys.Length(); i++) {
        RecordMember member = {};

        std::string key = keys.Get(i).As<Napi::String>();
        Napi::Value value = obj[key];
        int16_t align = 0;

        member.name = DuplicateString(key.c_str(), &instance->str_alloc).ptr;

        if (value.IsArray()) {
            Napi::Array array = value.As<Napi::Array>();

            if (array.Length() != 2 || !array.Get(0u).IsNumber()) {
                ThrowError<Napi::Error>(env, "Member specifier array must contain alignement value and type");
                return env.Null();
            }

            int64_t align64 = array.Get(0u).As<Napi::Number>().Int64Value();

            if (!CheckAlignment(align64)) {
                ThrowError<Napi::Error>(env, "Alignment of member '%1' must be 1, 2, 4 or 8", member.name);
                return env.Null();
            }

            value = array[1u];
            align = (int16_t)align64;
        }

        member.type = ResolveType(instance, value);
        if (!member.type)
            return env.Null();
        if (!CanStoreType(member.type)) {
            ThrowError<Napi::TypeError>(env, "Type %1 cannot be used as a member (maybe try %1 *)", member.type->name);
            return env.Null();
        }

        if (!align) {
            align = pad ? member.type->align : 1;
        }
        member.offset = (int32_t)AlignLen(size, align);

        size = member.offset + member.type->size;
        type->align = std::max(type->align, align);

        member.countedby = -1;

        if (size > instance->config.max_type_size) {
            ThrowError<Napi::Error>(env, "Struct '%1' size is too high (max = %2)", type->name, FmtMemSize(size));
            return env.Null();
        }

        if (TestStr(member.name, "_"))
            continue;

        if (!IsNameValid(member.name)) {
            ThrowError<Napi::Error>(env, "Invalid member name '%1'", member.name);
            return env.Null();
        }

        bool inserted;
        members.InsertOrGet(member.name, &inserted);

        if (!inserted) {
            ThrowError<Napi::Error>(env, "Duplicate member '%1' in struct '%2'", member.name, type->name);
            return env.Null();
        }

        type->members.Append(member);
    }

    for (Size i = 0; i < type->members.len; i++) {
        RecordMember *member = &type->members[i];
        const char *countedby = member->type->countedby;

        if (countedby) {
            const RecordMember *by = std::find_if(type->members.begin(), type->members.end(),
                [&](const RecordMember &member) { return TestStr(member.name, countedby); });

            if (by == type->members.end()) {
                ThrowError<Napi::Error>(env, "Record type %1 does not have member '%2'", type->name, countedby);
                return env.Null();
            }
            if (!IsInteger(by->type)) {
                ThrowError<Napi::Error>(env, "Dynamic length member %1 is not an integer", countedby);
                return env.Null();
            }
            if (member->type->primitive == PrimitiveKind::Array && i < type->members.len - 1) {
                ThrowError<Napi::Error>(env, "Flexible array '%1' is not the last member of struct", member->name);
                return env.Null();
            }

            member->countedby = by - type->members.ptr;
        }
    }

    if (!FinalizeCompositeType(env, type, PrimitiveKind::Record, size))
        return env.Null();
    err_guard.Disable();

    if (replace) {
        *replace = std::move(*type);
        type = replace;
    }

    napi_value wrapper = WrapType(instance, type);
    return Napi::Value(env, wrapper);
}

static Napi::Value CreatePaddedStructType(const Napi::CallbackInfo &info)
{
    return CreateStructType(info, true);
}

static Napi::Value CreatePackedStructType(const Napi::CallbackInfo &info)
{
    return CreateStructType(info, false);
}

static Napi::Value CreateUnionType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = (InstanceData *)info.Data();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 or 2 arguments, got %1", info.Length());
        return env.Null();
    }

    bool skip = (info.Length() > 1);
    bool named = skip && !IsNullOrUndefined(env, info[0]);
    bool redefine = named && info[0].IsObject() && CheckValueTag(env, info[0], &TypeObjectMarker);

    if (named && !info[0].IsString() && !redefine) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for name, expected string", GetValueType(instance, info[0]));
        return env.Null();
    }
    if (!IsObject(env, info[skip])) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for members, expected object", GetValueType(instance, info[1]));
        return env.Null();
    }

    Napi::String name = info[0].As<Napi::String>();
    Napi::Object obj = info[skip].As<Napi::Object>();
    Napi::Array keys = Napi::Array(env, GetOwnPropertyNames(env, obj));

    K_DEFER_NC(err_guard, count = instance->types.count) {
        Size start = count + !skip;

        for (Size i = start; i < instance->types.count; i++) {
            const TypeInfo *it = &instance->types[i];
            const TypeInfo **ptr = instance->types_map.Find(it->name);

            if (ptr && *ptr == it) {
                instance->types_map.Remove(ptr);
            }
        }

        instance->types.RemoveFrom(count);
    };

    TypeInfo *type = instance->types.AppendDefault();
    TypeInfo *replace = nullptr;

    if (redefine) {
        TypeObject *defn = nullptr;
        NAPI_OK(napi_unwrap(env, name, (void **)&defn));

        replace = (TypeInfo *)defn->GetType();

        type->instance = instance;
        type->name = replace->name;

        if (replace->primitive != PrimitiveKind::Void || replace == instance->void_type) {
            ThrowError<Napi::TypeError>(env, "Cannot redefine non-opaque type %1", replace->name);
            return env.Null();
        }
    } else if (named) {
        type->instance = instance;
        type->name = DuplicateString(name.Utf8Value().c_str(), &instance->str_alloc).ptr;

        if (!MapType(env, instance, type, type->name))
            return env.Null();
    } else {
        type->instance = instance;
        type->name = Fmt(&instance->str_alloc, "<anonymous_%1>", instance->types.count).ptr;
    }

    type->primitive = PrimitiveKind::Void;
    type->align = 1;

    HashSet<const char *> members;
    int32_t size = 0;

    for (uint32_t i = 0; i < keys.Length(); i++) {
        RecordMember member = {};

        std::string key = keys.Get(i).As<Napi::String>();
        Napi::Value value = obj[key];
        int16_t align = 0;

        member.name = DuplicateString(key.c_str(), &instance->str_alloc).ptr;

        if (value.IsArray()) {
            Napi::Array array = value.As<Napi::Array>();

            if (array.Length() != 2 || !array.Get(0u).IsNumber()) {
                ThrowError<Napi::Error>(env, "Member specifier array must contain alignement value and type");
                return env.Null();
            }

            int64_t align64 = array.Get(0u).As<Napi::Number>().Int64Value();

            if (!CheckAlignment(align64)) {
                ThrowError<Napi::Error>(env, "Alignment of member '%1' must be 1, 2, 4 or 8", member.name);
                return env.Null();
            }

            value = array[1u];
            align = (int16_t)align64;
        }

        member.type = ResolveType(instance, value);
        if (!member.type)
            return env.Null();
        if (!CanStoreType(member.type)) {
            ThrowError<Napi::TypeError>(env, "Type %1 cannot be used as a member (maybe try %1 *)", member.type->name);
            return env.Null();
        }
        if (member.type->countedby) {
            ThrowError<Napi::TypeError>(env, "Cannot use dynamic-length array or pointer inside of union");
            return env.Null();
        }

        align = align ? align : member.type->align;
        size = std::max(size, member.type->size);
        type->align = std::max(type->align, align);

        member.countedby = -1;

        if (TestStr(member.name, "_"))
            continue;

        if (!IsNameValid(member.name)) {
            ThrowError<Napi::Error>(env, "Invalid member name '%1'", member.name);
            return env.Null();
        }

        bool inserted;
        members.InsertOrGet(member.name, &inserted);

        if (!inserted) {
            ThrowError<Napi::Error>(env, "Duplicate member '%1' in union '%2'", member.name, type->name);
            return env.Null();
        }

        type->members.Append(member);
    }

    if (!FinalizeCompositeType(env, type, PrimitiveKind::Union, size))
        return env.Null();
    err_guard.Disable();

    Napi::Object construct = UnionValue::InitClass(instance, type);
    NAPI_OK(napi_create_reference(env, construct, 1, &type->construct));

    if (replace) {
        *replace = std::move(*type);
        type = replace;
    }

    napi_value wrapper = WrapType(instance, type);
    return Napi::Value(env, wrapper);
}

Napi::Value InstantiateUnion(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = (InstanceData *)info.Data();

    if (!info.IsConstructCall()) {
        ThrowError<Napi::TypeError>(env, "This function is a constructor and must be called with new");
        return env.Null();
    }
    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }

    const TypeInfo *type = ResolveType(instance, info[0]);
    if (!type)
        return env.Null();
    if (type->primitive != PrimitiveKind::Union) {
        ThrowError<Napi::TypeError>(env, "Expected union type, got %1", PrimitiveKindNames[(int)type->primitive]);
        return env.Null();
    }

    Napi::Function construct;
    {
        napi_value value;
        NAPI_OK(napi_get_reference_value(env, type->construct, &value));

        construct = Napi::Function(env, value);
    }

    Napi::Object wrapper = construct.New({}).As<Napi::Object>();
    SetValueTag(env, wrapper, &UnionValueMarker);

    return wrapper;
}

static Napi::Value CreateOpaqueType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = (InstanceData *)info.Data();

    bool named = (info.Length() >= 1) && !IsNullOrUndefined(env, info[0]);

    if (named && !info[0].IsString()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for name, expected string", GetValueType(instance, info[0]));
        return env.Null();
    }

    Napi::String name = info[0].As<Napi::String>();

    TypeInfo *type = instance->types.AppendDefault();
    K_DEFER_N(err_guard) { instance->types.RemoveLast(1); };

    type->instance = instance;
    type->name = named ? DuplicateString(name.Utf8Value().c_str(), &instance->str_alloc).ptr
                       : Fmt(&instance->str_alloc, "<anonymous_%1>", instance->types.count).ptr;

    type->primitive = PrimitiveKind::Void;
    type->size = 0;
    type->align = 0;

    // If the insert succeeds, we cannot fail anymore
    if (named && !MapType(env, instance, type, type->name))
        return env.Null();
    err_guard.Disable();

    napi_value wrapper = WrapType(instance, type);
    return Napi::Value(env, wrapper);
}

static Napi::Value CreatePointerType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = (InstanceData *)info.Data();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 to 3 arguments, got %1", info.Length());
        return env.Null();
    }

    bool skip = (info.Length() > 1) && !info[1].IsNumber();
    bool named = skip && !IsNullOrUndefined(env, info[0]);

    if (named && !info[0].IsString()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for name, expected string", GetValueType(instance, info[0]));
        return env.Null();
    }

    std::string name = named ? info[0].As<Napi::String>() : std::string();

    const TypeInfo *ref = ResolveType(instance, info[skip]);
    if (!ref)
        return env.Null();

    Napi::Value countedby;
    int count = 1;

    if (info.Length() >= 2u + skip) {
        if (info[1 + skip].IsString()) {
            countedby = info[1 + skip];
        } else if (info[1 + skip].IsNumber()) {
            count = info[1 + skip].As<Napi::Number>();

            if (count < 1 || count > 4) {
                ThrowError<Napi::TypeError>(env, "Value of count must be between 1 and 4");
                return env.Null();
            }
        } else {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for count, expected number", GetValueType(instance, info[1 + skip]));
            return env.Null();
        }
    }

    TypeInfo *type = MakePointerType(instance, ref, count);
    K_ASSERT(type);

    if (named || !countedby.IsEmpty()) {
        TypeInfo *copy = instance->types.AppendDefault();
        K_DEFER_N(err_guard) { instance->types.RemoveLast(1); };

        memcpy((void *)copy, type, K_SIZE(*type));
        copy->name = named ? DuplicateString(name.c_str(), &instance->str_alloc).ptr : copy->name;
        copy->defn = nullptr;

        static_assert(!std::is_polymorphic_v<Napi::ObjectReference>);

        if (!countedby.IsEmpty()) {
            Napi::String str = countedby.As<Napi::String>();
            copy->countedby = DuplicateString(str.Utf8Value().c_str(), &instance->str_alloc).ptr;
        }

        // If the insert succeeds, we cannot fail anymore
        if (named && !MapType(env, instance, copy, copy->name))
            return env.Null();
        err_guard.Disable();

        type = copy;
    }

    napi_value wrapper = WrapType(instance, type);
    return Napi::Value(env, wrapper);
}

static napi_value MarkPointer(napi_env env, napi_callback_info info, int directions)
{
    K_ASSERT(directions >= 1 && directions <= 3);

    napi_value arg;
    size_t count = 1;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, &arg, nullptr, (void **)&instance));

    if (count < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", count);
        return GetNull(env);
    }

    const TypeInfo *type = ResolveType(instance, arg);
    if (!type)
        return GetNull(env);

    if (type->primitive != PrimitiveKind::Pointer &&
            type->primitive != PrimitiveKind::String &&
            type->primitive != PrimitiveKind::String16 &&
            type->primitive != PrimitiveKind::String32) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 type, expected pointer or string type", type->name);
        return GetNull(env);
    }

    // Embed direction in unused pointer bits
    const TypeInfo *marked = (const TypeInfo *)((uint8_t *)type + directions - 1);

    Napi::External<TypeInfo> external = Napi::External<TypeInfo>::New(env, (TypeInfo *)marked);
    SetValueTag(env, external, &DirectionMarker);

    return external;
}

static Napi::Value CreateDisposableType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = (InstanceData *)info.Data();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 or 2 arguments, got %1", info.Length());
        return env.Null();
    }

    bool skip = (info.Length() > 1) && !info[1].IsFunction();
    bool named = skip && !IsNullOrUndefined(env, info[0]);

    if (named && !info[0].IsString()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for name, expected string", GetValueType(instance, info[0]));
        return env.Null();
    }

    Napi::String name = info[0].As<Napi::String>();

    const TypeInfo *src = ResolveType(instance, info[skip]);
    if (!src)
        return env.Null();
    if (src->primitive != PrimitiveKind::Pointer &&
            src->primitive != PrimitiveKind::String &&
            src->primitive != PrimitiveKind::String16 &&
            src->primitive != PrimitiveKind::String32) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 type, expected pointer or string type", src->name);
        return env.Null();
    }
    if (src->dispose) {
        ThrowError<Napi::TypeError>(env, "Cannot use disposable type '%1' to create new disposable", src->name);
        return env.Null();
    }

    DisposeFunc *dispose;
    Napi::Function dispose_func;
    if (info.Length() >= 2u + skip && !IsNullOrUndefined(env, info[1 + skip])) {
        Napi::Function func = info[1 + skip].As<Napi::Function>();

        if (!func.IsFunction()) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for func, expected function", GetValueType(instance, func));
            return env.Null();
        }

        dispose = [](InstanceData *instance, const TypeInfo *type, const void *ptr) {
            Napi::Env env = instance->env;

            napi_value func;
            NAPI_OK(napi_get_reference_value(env, type->dispose_ref, &func));

            napi_value self = env.Null();
            napi_value wrapper = WrapPointer(env, (void *)ptr);

            NAPI_OK(napi_call_function(env, self, func, 1, &wrapper, nullptr));
            instance->stats.disposed++;
        };
        dispose_func = func;
    } else {
        dispose = [](InstanceData *instance, const TypeInfo *, const void *ptr) {
            free((void *)ptr);
            instance->stats.disposed++;
        };
    }

    TypeInfo *type = instance->types.AppendDefault();
    K_DEFER_N(err_guard) { instance->types.RemoveLast(1); };

    memcpy((void *)type, (const void *)src, K_SIZE(*src));
    type->defn = nullptr;
    K_ASSERT(!type->members.len);

    static_assert(!std::is_polymorphic_v<Napi::ObjectReference>);

    type->name = named ? DuplicateString(name.Utf8Value().c_str(), &instance->str_alloc).ptr
                       : Fmt(&instance->str_alloc, "<anonymous_%1>", instance->types.count).ptr;

    type->dispose = dispose;
    NAPI_OK(napi_create_reference(env, dispose_func, 1, &type->dispose_ref));

    // If the insert succeeds, we cannot fail anymore
    if (named) {
        bool inserted;
        instance->types_map.InsertOrGet(type->name, type, &inserted);

        if (!inserted) {
            ThrowError<Napi::Error>(env, "Duplicate type name '%1'", type->name);
            return env.Null();
        }
    }
    err_guard.Disable();

    napi_value wrapper = WrapType(instance, type);
    return Napi::Value(env, wrapper);
}

static napi_value CallAlloc(napi_env env, napi_callback_info info)
{
    napi_value args[2];
    size_t count = 2;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, args, nullptr, (void **)&instance));

    if (count < 2) {
        ThrowError<Napi::TypeError>(env, "Expected 2 arguments, got %1", count);
        return GetNull(env);
    }

    const TypeInfo *type = ResolveType(instance, args[0]);
    if (!type)
        return GetNull(env);
    if (!type->size) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Cannot allocate memory for zero-sized type %1", type->name);
        return GetNull(env);
    }

    int32_t len;
    if (napi_get_value_int32(env, args[1], &len) != napi_ok) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for length, expected number", GetValueType(instance, args[1]));
        return GetNull(env);
    }
    if (len <= 0) [[unlikely]] {
        ThrowError<Napi::Error>(env, "Size must be greater than 0");
        return GetNull(env);
    }
    if (len > INT32_MAX / type->size) [[unlikely]] {
        ThrowError<Napi::Error>(env, "Cannot allocate more than %1 objects of type %2", INT32_MAX / type->size, type->name);
        return GetNull(env);
    }

    void *ptr = calloc((size_t)len, (size_t)type->size);

    if (!ptr) [[unlikely]] {
        Size size = (Size)(len * type->size);

        ThrowError<Napi::Error>(env, "Failed to allocate %1 of memory", FmtMemSize((Size)size));
        return GetNull(env);
    }

    return WrapPointer(env, ptr);
}

static napi_value CallFree(napi_env env, napi_callback_info info)
{
    napi_value arg;
    size_t count = 1;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, &arg, nullptr, (void **)&instance));

    if (count < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", count);
        return GetNull(env);
    }

    void *ptr = nullptr;
    if (!TryPointer(env, arg, &ptr)) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for ptr, expected pointer", GetValueType(instance, arg));
        return GetNull(env);
    }

    free(ptr);

    return GetUndefined(env);
}

static napi_value GetOrSetErrno(napi_env env, napi_callback_info info)
{
    napi_value arg;
    size_t count = 1;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, &arg, nullptr, (void **)&instance));

    int32_t value;

    if (count >= 1) {
        if (napi_get_value_int32(env, arg, &value) != napi_ok) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for errno, expected integer", GetValueType(instance, arg));
            return GetNull(env);
        }
        errno = value;
    } else {
        value = (int32_t)errno;
    }

    return NewInt(env, value);
}

static ArrayHint SelectTypedHint(const TypeInfo *ref)
{
    switch (ref->primitive) {
        case PrimitiveKind::Int8: return ArrayHint::Int8Array;
        case PrimitiveKind::UInt8: return ArrayHint::Uint8Array;
        case PrimitiveKind::Int16:
        case PrimitiveKind::Int16S: return ArrayHint::Int16Array;
        case PrimitiveKind::UInt16:
        case PrimitiveKind::UInt16S: return ArrayHint::Uint16Array;
        case PrimitiveKind::Int32:
        case PrimitiveKind::Int32S: return ArrayHint::Int32Array;
        case PrimitiveKind::UInt32:
        case PrimitiveKind::UInt32S: return ArrayHint::Uint32Array;
        case PrimitiveKind::Int64:
        case PrimitiveKind::Int64S: return ArrayHint::BigInt64Array;
        case PrimitiveKind::UInt64:
        case PrimitiveKind::UInt64S: return ArrayHint::BigUint64Array;
        case PrimitiveKind::Float32: return ArrayHint::Float32Array;
        case PrimitiveKind::Float64: return ArrayHint::Float64Array;

        case PrimitiveKind::Void:
        case PrimitiveKind::Bool:
        case PrimitiveKind::String:
        case PrimitiveKind::String16:
        case PrimitiveKind::String32:
        case PrimitiveKind::Pointer:
        case PrimitiveKind::Record:
        case PrimitiveKind::Union:
        case PrimitiveKind::Array:
        case PrimitiveKind::Callback:
        case PrimitiveKind::Prototype: return ArrayHint::Array;
    }

    K_UNREACHABLE();
}

static Napi::Value CreateArrayType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = (InstanceData *)info.Data();

    if (info.Length() < 2) {
        ThrowError<Napi::TypeError>(env, "Expected 2 to 4 arguments, got %1", info.Length());
        return env.Null();
    }

    const TypeInfo *ref = ResolveType(instance, info[0]);
    if (!ref)
        return env.Null();

    bool dynamic = (info.Length() >= 3) && info[1].IsString();

    if (dynamic && !info[1].IsString()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for countedBy, expected string", GetValueType(instance, info[1]));
        return env.Null();
    }
    if (!info[1 + dynamic].IsNumber()) {
        if (info.Length() == 2 && info[1].IsString()) {
            ThrowError<Napi::TypeError>(env, "Missing maxLen argument");
        } else {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for %2, expected integer", GetValueType(instance, info[1]), dynamic ? "maxLen" : "len");
        }
        return env.Null();
    }

    int64_t len = info[1 + dynamic].As<Napi::Number>().Int64Value();

    if (len <= 0) {
        ThrowError<Napi::TypeError>(env, "Array length must be positive and non-zero");
        return env.Null();
    }
    if (len > instance->config.max_type_size / ref->size) {
        ThrowError<Napi::TypeError>(env, "Array length is too high (max = %1)", instance->config.max_type_size / ref->size);
        return env.Null();
    }

    TypeInfo *type = nullptr;

    if (info.Length() >= 3u + dynamic && !IsNullOrUndefined(env, info[2 + dynamic])) {
        if (!info[2 + dynamic].IsString()) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for hint, expected string", GetValueType(instance, info[2]));
            return env.Null();
        }

        std::string str = info[2 + dynamic].As<Napi::String>();
        ArrayHint hint = {};

        if (str == "Typed" || str == "typed") {
            if (ref->hint == ArrayHint::Array) {
                ThrowError<Napi::Error>(env, "Array hint 'Typed' cannot be used with type %1", ref->name);
                return env.Null();
            }

            hint = SelectTypedHint(ref);
        } else if (str == "Buffer" || str == "buffer") {
            if (ref->hint == ArrayHint::Array) {
                ThrowError<Napi::Error>(env, "Array hint 'Buffer' cannot be used with type %1", ref->name);
                return env.Null();
            }

            hint = ArrayHint::Buffer;
        } else if (str == "Array" || str == "array") {
            hint = ArrayHint::Array;
        } else if (str == "String" || str == "string") {
            if (ref->primitive == PrimitiveKind::Int8) {
                hint = ArrayHint::String8;
            } else if (ref->primitive == PrimitiveKind::Int16) {
                hint = ArrayHint::String16;
            } else if (ref->primitive == PrimitiveKind::Int32) {
                hint = ArrayHint::String32;
            } else {
                ThrowError<Napi::Error>(env, "Array hint 'String' can only be used with 8, 16 and 32-bit signed integer types");
                return env.Null();
            }
        } else {
            ThrowError<Napi::Error>(env, "Array conversion hint must be 'Typed', 'Array' or 'String'");
            return env.Null();
        }

        type = MakeArrayType(instance, ref, (Size)len, hint);
    } else {
        type = MakeArrayType(instance, ref, (Size)len);
    }

    if (dynamic) {
        Napi::String str = info[1].As<Napi::String>();
        type->countedby = DuplicateString(str.Utf8Value().c_str(), &instance->str_alloc).ptr;
    }

    napi_value wrapper = WrapType(instance, type);
    return Napi::Value(env, wrapper);
}

static bool ParseClassicFunction(const Napi::CallbackInfo &info, bool concrete, FunctionInfo *out_func)
{
    K_ASSERT(info.Length() >= 2);

    Napi::Env env = info.Env();
    InstanceData *instance = (InstanceData *)info.Data();

    Napi::String name = info[0u].As<Napi::String>();
    Napi::Value ret = info[1u];
    Napi::Array parameters = info[2u].As<Napi::Array>();

    // Detect optional call convention
    if (name.IsString() && DetectCallConvention(name.Utf8Value().c_str(), &out_func->convention)) {
        if (info.Length() < 3) {
            ThrowError<Napi::TypeError>(env, "Expected 3 or 4 arguments, got %1", info.Length());
            return false;
        }

        name = info[1u].As<Napi::String>();
        ret = info[2u];
        parameters = (info.Length() >= 4 ? info[3u] : env.Null()).As<Napi::Array>();
    }

    bool named = parameters.IsArray();

    if (named) {
#if defined(_WIN32)
        if (name.IsNumber()) {
            out_func->ordinal_name = name.As<Napi::Number>().Int32Value();
            name = name.ToString();
        }
#endif
        if (!name.IsString()) {
            if (!concrete && IsNullOrUndefined(env, name)) {
                named = false;
            } else {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for name, expected string or integer", GetValueType(instance, name));
                return false;
            }
        }
    } else {
        parameters = ret.As<Napi::Array>();
        ret = name;
    }

    // Leave anonymous naming responsibility to caller
    out_func->name = named ? DuplicateString(name.Utf8Value().c_str(), &instance->str_alloc).ptr : nullptr;

    out_func->ret = ResolveType(instance, ret);
    if (!out_func->ret)
        return false;
    if (!CanReturnType(out_func->ret)) {
        ThrowError<Napi::TypeError>(env, "You are not allowed to directly return %1 values (maybe try %1 *)", out_func->ret->name);
        return false;
    }

    if (!parameters.IsArray()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for parameters of '%2', expected an array", GetValueType(instance, parameters), out_func->name);
        return false;
    }

    uint32_t parameters_len = parameters.Length();

    if (parameters_len) {
        Napi::String str = parameters.Get(parameters_len - 1).As<Napi::String>();

        if (str.IsString() && str.Utf8Value() == "...") {
            out_func->variadic = true;
            parameters_len--;
        }
    }

    for (uint32_t j = 0; j < parameters_len; j++) {
        ParameterInfo param = {};

        param.type = ResolveType(instance, parameters[j].AsValue(), &param.directions);

        if (!param.type)
            return false;
        if (!CanPassType(param.type, param.directions)) {
            ThrowError<Napi::TypeError>(env, "Type %1 cannot be used as a parameter", param.type->name);
            return false;
        }
        if (out_func->parameters.len >= MaxParameters) {
            ThrowError<Napi::TypeError>(env, "Functions cannot have more than %1 parameters", MaxParameters);
            return false;
        }

        param.offset = (int8_t)j;

        out_func->parameters.Append(param);
    }

    out_func->required_parameters = (int8_t)out_func->parameters.len;

    return true;
}

static Napi::Value CreateFunctionType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = (InstanceData *)info.Data();

    FunctionInfo *func = instance->callbacks.AppendDefault();
    K_DEFER_N(err_guard) { instance->callbacks.RemoveLast(1); };

    func->env = env;
    func->instance = instance;

    if (info.Length() >= 2) {
        if (!ParseClassicFunction(info, false, func))
            return env.Null();
    } else if (info.Length() >= 1) {
        if (!info[0].IsString()) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for prototype, expected string", GetValueType(instance, info[0]));
            return env.Null();
        }

        std::string proto = info[0u].As<Napi::String>();
        if (!ParsePrototype(instance, proto.c_str(), false, func))
            return env.Null();
    } else {
        ThrowError<Napi::TypeError>(env, "Expected 1 to 4 arguments, got %1", info.Length());
        return env.Null();
    }

    bool named = func->name;

    if (!named) {
        func->name = Fmt(&instance->str_alloc, "<anonymous_%1>", instance->types.count).ptr;
    }

    if (!func->variadic && !PreparePlan(instance, func))
        return env.Null();

    // We cannot fail after this check
    if (named && instance->types_map.Find(func->name)) {
        ThrowError<Napi::Error>(env, "Duplicate type name '%1'", func->name);
        return env.Null();
    }
    err_guard.Disable();

    TypeInfo *type = instance->types.AppendDefault();

    type->instance = instance;
    type->name = func->name;

    type->primitive = PrimitiveKind::Prototype;
    type->align = alignof(void *);
    type->size = K_SIZE(void *);
    type->proto = func;

    instance->types_map.Set(type->name, type);

    napi_value wrapper = WrapType(instance, type);
    return Napi::Value(env, wrapper);
}

static Napi::Value CreateEnumType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = (InstanceData *)info.Data();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 or 2 arguments, got %1", info.Length());
        return env.Null();
    }

    bool named = (info.Length() >= 2 && !info[0].IsObject());
    bool typed = (info.Length() >= 2u + named && !IsNullOrUndefined(env, info[1 + named]));

    if (named && !info[0].IsString()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for name, expected string", GetValueType(instance, info[0]));
        return env.Null();
    }
    if (!IsObject(env, info[named])) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for values, expected object", GetValueType(instance, info[1]));
        return env.Null();
    }

    Napi::String name = info[0].As<Napi::String>();
    Napi::Object obj = info[named].As<Napi::Object>();
    Napi::Array keys = Napi::Array(env, GetOwnPropertyNames(env, obj));

    TypeInfo *type = instance->types.AppendDefault();
    K_DEFER_N(err_guard) { instance->types.RemoveLast(1); };

    type->instance = instance;
    type->name = named ? DuplicateString(name.Utf8Value().c_str(), &instance->str_alloc).ptr
                       : Fmt(&instance->str_alloc, "<anonymous_%1>", instance->types.count).ptr;

    Napi::Object values = Napi::Object::New(env);

    // Determine needed storage type
    if (typed) {
        const TypeInfo *storage = ResolveType(instance, info[1 + named]);
        if (!storage)
            return env.Null();

        if (!IsInteger(storage)) {
            ThrowError<Napi::TypeError>(env, "Expected integer type for underlying enum storage type");
            return env.Null();
        }

        type->primitive = storage->primitive;
        type->size = storage->size;
        type->align = storage->align;
    } else {
#if defined(_WIN32)
        type->primitive = PrimitiveKind::Int32;
        type->size = 4;
        type->align = 4;

        for (uint32_t i = 0; i < keys.Length(); i++) {
            std::string key = keys.Get(i).As<Napi::String>();
            Napi::Value value = obj[key];

            int64_t i64;
            bool lossless;

            if (value.IsNumber()) {
                i64 = value.As<Napi::Number>().Int64Value();
                lossless = true;
            } else if (value.IsBigInt()) {
                Napi::BigInt big = value.As<Napi::BigInt>();
                i64 = big.Int64Value(&lossless);
            } else {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for enumeration value, expected number", GetValueType(instance, info[0]));
                return env.Null();
            }

            if (!lossless || i64 < INT_MIN || i64 > INT_MAX) {
                ThrowError<Napi::Error>(env, "Cannot find storage type wide enough for enum values");
                return env.Null();
            }

            values.Set(key, value);
        }
#else
        bool negative = false;
        bool negative64 = false;
        uint64_t max = 0;

        for (uint32_t i = 0; i < keys.Length(); i++) {
            std::string key = keys.Get(i).As<Napi::String>();
            Napi::Value value = obj[key];

            if (value.IsNumber()) {
                int64_t i = value.As<Napi::Number>().Int64Value();

                if (i < 0) {
                    negative = true;
                    negative64 |= (i < INT_MIN);
                } else {
                    max = std::max(max, (uint64_t)i);
                }
            } else if (value.IsBigInt()) {
                Napi::BigInt big = value.As<Napi::BigInt>();

                bool lossless;
                int64_t i = big.Int64Value(&lossless);

                if (lossless && i < 0) {
                    negative = true;
                    negative64 |= (i < INT_MIN);
                } else {
                    uint64_t u = big.Uint64Value(&lossless);

                    if (!lossless) {
                        ThrowError<Napi::Error>(env, "Cannot find storage type wide enough for enum values");
                        return env.Null();
                    }

                    max = std::max(max, u);
                }
            } else {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for enumeration value, expected number",  GetValueType(instance, info[0]));
                return env.Null();
            }

            values.Set(key, value);
        }

        // The rules are implementation-defined, but tend to be the same
        if (max <= UINT_MAX && !negative) {
            type->primitive = PrimitiveKind::UInt32;
            type->size = 4;
            type->align = 4;
        } else if (max <= INT_MAX && !negative64) {
            type->primitive = PrimitiveKind::Int32;
            type->size = 4;
            type->align = 4;
        } else if (!negative) {
            type->primitive = PrimitiveKind::UInt64;
            type->size = 8;
            type->align = alignof(uint64_t);
        } else if (max <= INT64_MAX) {
            type->primitive = PrimitiveKind::Int64;
            type->size = 8;
            type->align = alignof(int64_t);
        } else {
            ThrowError<Napi::Error>(env, "Cannot find storage type wide enough for enum values");
            return env.Null();
        }
#endif
    }

    // If the insert succeeds, we cannot fail anymore
    if (named && !MapType(env, instance, type, type->name))
        return env.Null();
    err_guard.Disable();

    napi_value wrapper = WrapType(instance, type, false);
    Napi::Object defn(env, wrapper);

    defn.Set("values", values);
    defn.Freeze();

    return Napi::Value(env, wrapper);
}

static Napi::Value CreateTypeAlias(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = (InstanceData *)info.Data();

    if (info.Length() < 2) {
        ThrowError<Napi::TypeError>(env, "Expected 2 arguments, got %1", info.Length());
        return env.Null();
    }
    if (!info[0].IsString()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for name, expected string", GetValueType(instance, info[0]));
        return env.Null();
    }

    std::string name = info[0].As<Napi::String>();
    const char *alias = DuplicateString(name.c_str(), &instance->str_alloc).ptr;

    const TypeInfo *type = ResolveType(instance, info[1]);
    if (!type)
        return env.Null();

    // Alias the type
    if (!MapType(env, instance, type, alias))
        return env.Null();

    napi_value wrapper = WrapType(instance, type);
    return Napi::Value(env, wrapper);
}

static napi_value GetResolvedType(napi_env env, napi_callback_info info)
{
    napi_value arg;
    size_t count = 1;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, &arg, nullptr, (void **)&instance));

    if (count < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", count);
        return GetNull(env);
    }

    const TypeInfo *type = ResolveType(instance, arg);
    if (!type)
        return GetNull(env);

    return WrapType(instance, type);
}

static napi_value GetTypeSize(napi_env env, napi_callback_info info)
{
    napi_value arg;
    size_t count = 1;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, &arg, nullptr, (void **)&instance));

    if (count < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", count);
        return GetNull(env);
    }

    const TypeInfo *type = ResolveType(instance, arg);
    if (!type)
        return GetNull(env);

    return NewInt(env, type->size);
}

static napi_value GetTypeAlign(napi_env env, napi_callback_info info)
{
    napi_value arg;
    size_t count = 1;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, &arg, nullptr, (void **)&instance));

    if (count < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", count);
        return GetNull(env);
    }

    const TypeInfo *type = ResolveType(instance, arg);
    if (!type)
        return GetNull(env);

    return NewInt(env, type->align);
}

static napi_value GetMemberOffset(napi_env env, napi_callback_info info)
{
    napi_value args[2];
    size_t count = 2;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, args, nullptr, (void **)&instance));

    if (count < 2) {
        ThrowError<Napi::TypeError>(env, "Expected 2 arguments, got %1", count);
        return GetNull(env);
    }

    const TypeInfo *type = ResolveType(instance, args[0]);
    if (!type)
        return GetNull(env);
    if (type->primitive != PrimitiveKind::Record) {
        ThrowError<Napi::TypeError>(env, "The offsetof() function can only be used with record types");
        return GetNull(env);
    }

    char name[256];
    if (napi_get_value_string_utf8(env, args[1], name, K_SIZE(name), nullptr) != napi_ok) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member, expected string", GetValueType(instance, args[1]));
        return GetNull(env);
    }

    const RecordMember *member = std::find_if(type->members.begin(), type->members.end(),
                                              [&](const RecordMember &member) { return TestStr(member.name, name); });

    if (member == type->members.end()) {
        ThrowError<Napi::TypeError>(env, "Record type %1 does not have member '%2'", type->name, name);
        return GetNull(env);
    }

    return NewInt(env, member->offset);
}

static void InitSyncMemory(InstanceData *instance)
{
    if (instance->sync_memory.IsAllocated()) [[likely]]
        return;

    instance->sync_memory.Allocate(instance->config.sync_stack_size, instance->config.sync_heap_size);
}

InstanceMemory *AllocateAsyncMemory(InstanceData *instance)
{
    std::lock_guard<std::mutex> lock(instance->mem_mutex);

    for (Size i = 0; i < instance->memories.len; i++) {
        InstanceMemory *mem = instance->memories[i];

        if (!mem->busy) {
            mem->busy = true;
            return mem;
        }
    }

    bool temporary = (instance->memories.len >= instance->config.resident_async_pools);

    if (temporary && instance->temporaries >= instance->config.max_temporaries)
        return nullptr;

    InstanceMemory *mem = new InstanceMemory();
    K_DEFER_N(mem_guard) { delete mem; };

    mem->Allocate(instance->config.async_stack_size, instance->config.async_heap_size);

    if (temporary) {
        instance->temporaries++;
        mem->temporary = true;
    } else {
        instance->memories.Append(mem);
        mem->temporary = false;
    }

    mem->busy = true;

    mem_guard.Disable();
    return mem;
}

void ReleaseAsyncMemory(InstanceData *instance, InstanceMemory *mem)
{
    std::lock_guard<std::mutex> lock(instance->mem_mutex);

    if (mem->temporary) {
        instance->temporaries--;
        delete mem;
    } else {
        mem->busy = false;
    }
}

TypeInfo::~TypeInfo()
{
    if (!instance)
        return;

    Napi::Env env = instance->env;

    for (RecordMember &member: members) {
        node_api_delete_reference(env, member.key);
        member.key = nullptr;
    }

    node_api_delete_reference(env, dispose_ref);
    node_api_delete_reference(env, construct);
    node_api_delete_reference(env, defn);
}

Napi::Function LibraryHandle::InitClass(InstanceData *instance)
{
    Napi::Env env = instance->env;

    // node-addon-api wants std::vector
    std::vector<Napi::ClassPropertyDescriptor<LibraryHandle>> properties = {
        InstanceMethod("func", &LibraryHandle::Func, napi_default, instance),
        InstanceMethod("symbol", &LibraryHandle::Symbol, napi_default, instance),
        InstanceMethod("unload", &LibraryHandle::Unload, napi_default, instance)
    };

    if (Napi::Value dispose = env.RunScript("Symbol.dispose"); !IsNullOrUndefined(env, dispose)) {
        Napi::ClassPropertyDescriptor<LibraryHandle> prop = InstanceMethod(dispose.As<Napi::Symbol>(), &LibraryHandle::Unload);
        properties.push_back(prop);
    }

    Napi::Function constructor = DefineClass(env, "LibraryHandle", properties);
    return constructor;
}

LibraryHandle::LibraryHandle(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<LibraryHandle>(info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsExternal()) [[unlikely]] {
        ThrowError<Napi::Error>(env, "Library objects cannot be constructed manually");
        return;
    }

    Napi::External<void> external = info[0].As<Napi::External<void>>();
    lib = (LibraryHolder *)external.Data();
}

void LibraryHandle::Finalize(Napi::BasicEnv env)
{
    node_api_delete_reference(env, *this);
    SuppressDestruct();

    lib->Unref();
}

Napi::Value LibraryHandle::Func(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = (InstanceData *)info.Data();

    FunctionInfo *func = new FunctionInfo();
    K_DEFER { func->Unref(); };

    func->env = env;
    func->instance = instance;
    func->lib = lib->Ref();

    if (info.Length() >= 2) {
        if (!ParseClassicFunction(info, true, func))
            return env.Null();
    } else if (info.Length() >= 1) {
        if (!info[0].IsString()) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for prototype, expected string", GetValueType(instance, info[0]));
            return env.Null();
        }

        std::string proto = info[0u].As<Napi::String>();
        if (!ParsePrototype(instance, proto.c_str(), true, func))
            return env.Null();
    } else {
        ThrowError<Napi::TypeError>(env, "Expected 1 to 4 arguments, got %1", info.Length());
        return env.Null();
    }

    if (func->convention != CallConvention::Cdecl && func->variadic) {
        LogError("Call convention '%1' does not support variadic functions, ignoring",
                 CallConventionNames[(int)func->convention]);
        func->convention = CallConvention::Cdecl;
    }

    if (!func->variadic && !PreparePlan(instance, func))
        return env.Null();

#if defined(_WIN32)
    if (func->ordinal_name < 0) {
        if (func->decorated_name) {
            func->native = (void *)GetProcAddress((HMODULE)lib->module, func->decorated_name);
        }
        if (!func->native) {
            func->native = (void *)GetProcAddress((HMODULE)lib->module, func->name);
        }
    } else {
        uint16_t ordinal = (uint16_t)func->ordinal_name;

        func->decorated_name = nullptr;
        func->native = (void *)GetProcAddress((HMODULE)lib->module, (LPCSTR)(size_t)ordinal);
    }
#else
    if (func->decorated_name) {
        func->native = dlsym(lib->module, func->decorated_name);
    }
    if (!func->native) {
        func->native = dlsym(lib->module, func->name);
    }
#endif
    if (!func->native) {
        ThrowError<Napi::Error>(env, "Cannot find function '%1' in shared library", func->name);
        return env.Null();
    }

    napi_value wrapper = WrapFunction(instance, func);
    return Napi::Value(env, wrapper);
}

Napi::Value LibraryHandle::Symbol(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = (InstanceData *)info.Data();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }
    if (!info[0].IsString()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for name, expected string", GetValueType(instance, info[0]));
        return env.Null();
    }

    std::string name = info[0].As<Napi::String>();

#if defined(_WIN32)
    void *ptr = (void *)GetProcAddress((HMODULE)lib->module, name.c_str());
#else
    void *ptr = (void *)dlsym(lib->module, name.c_str());
#endif
    if (!ptr) {
        ThrowError<Napi::Error>(env, "Cannot find symbol '%1' in shared library", name.c_str());
        return env.Null();
    }

    napi_value wrapper = WrapPointer(env, ptr);
    return Napi::Value(env, wrapper);
}

Napi::Value LibraryHandle::Unload(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    lib->Unload();

    return env.Undefined();
}

static napi_value LoadSharedLibrary(napi_env env, napi_callback_info info)
{
    napi_value args[2];
    size_t count = 2;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, args, nullptr, (void **)&instance));

    if (count < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 or 2 arguments, got %1", count);
        return GetNull(env);
    }

    char filename[4096];
    [[maybe_unused]] int flags = 0;

    if (napi_status ret = napi_get_value_string_utf8(env, args[0], filename, K_SIZE(filename), nullptr); ret != napi_ok) {
        if (ret == napi_string_expected && IsNullOrUndefined(env, args[0])) {
            filename[0] = 0;
        } else {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for filename, expected string or null", GetValueType(instance, args[0]));
            return GetNull(env);
        }
    }

    if (count >= 2) {
        if (!IsObject(env, args[1])) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for options, expected object", GetValueType(instance, args[1]));
            return GetNull(env);
        }

#if !defined(_WIN32)
        Napi::Object options(env, args[1]);

        flags |= options.Get("lazy").ToBoolean() ? RTLD_LAZY : RTLD_NOW;
        flags |= options.Get("global").ToBoolean() ? RTLD_GLOBAL : RTLD_LOCAL;
#if defined(RTLD_DEEPBIND)
        flags |= options.Get("deep").ToBoolean() ? RTLD_DEEPBIND : 0;
#endif
    } else {
        flags = RTLD_NOW | RTLD_LOCAL;
#endif
    }

    InitSyncMemory(instance);

    // Load shared library
    void *module = nullptr;

#if defined(_WIN32)
    if (filename[0]) {
        module = LoadWindowsLibrary(env, filename);

        if (!module)
            return GetNull(env);
    } else {
        module = GetModuleHandle(nullptr);
        K_ASSERT(module);
    }
#else
    if (filename[0]) {
        module = dlopen(filename, flags);

        if (!module) {
            const char *msg = dlerror();

            if (StartsWith(msg, filename)) {
                msg += strlen(filename);

                while (strchr(": ", msg[0]) && msg[0]) {
                    msg++;
                }
            }

            ThrowError<Napi::Error>(env, "Failed to load shared library: %1", msg);
            return GetNull(env);
        }
    } else {
        module = RTLD_DEFAULT;
    }
#endif

    LibraryHolder *lib = new LibraryHolder(module);

    Napi::External<void> external = Napi::External<void>::New(env, lib);
    Napi::Object obj = instance->construct_lib.New({ external }).As<Napi::Object>();
    SetValueTag(env, obj, &LibraryHandleMarker);

    return obj;
}

static napi_value RegisterCallback(napi_env env, napi_callback_info info)
{
    napi_value args[2];
    size_t count = 2;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, args, nullptr, (void **)&instance));

    if (count < 2) {
        ThrowError<Napi::TypeError>(env, "Expected 2 arguments, got %1", count);
        return GetNull(env);
    }

    if (!InitAsyncBroker(instance)) [[unlikely]]
        return GetNull(env);
    InitSyncMemory(instance);

    napi_value func = args[0];

    if (GetKindOf(env, func) != napi_function) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for func, expected function", GetValueType(instance, args[0]));
        return GetNull(env);
    }

    const TypeInfo *type = ResolveType(instance, args[1]);
    if (!type)
        return GetNull(env);
    if (type->primitive != PrimitiveKind::Callback) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 type, expected <callback> * type", type->name);
        return GetNull(env);
    }

    int16_t idx;
    {
        std::lock_guard<std::mutex> lock(shared.mutex);

        if (!shared.available.len) [[unlikely]] {
            ThrowError<Napi::Error>(env, "Too many callbacks are in use (max = %1)", MaxTrampolines);
            return GetNull(env);
        }

        idx = shared.available.data[--shared.available.len];
    }

    TrampolineInfo *trampoline = &shared.trampolines[idx];

    trampoline->state = 1;
    trampoline->env = env;
    trampoline->instance = instance;
    trampoline->stack = instance->sync_memory.stack;
    trampoline->proto = type->proto;
    NAPI_OK(napi_create_reference(env, func, 1, &trampoline->func));

    void *ptr = GetTrampolinePointer(idx);

    return WrapPointer(env, ptr);
}

static napi_value UnregisterCallback(napi_env env, napi_callback_info info)
{
    napi_value arg;
    size_t count = 2;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, &arg, nullptr, (void **)&instance));

    if (count < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", count);
        return GetNull(env);
    }

    void *ptr;
    if (!TryPointer(env, arg, &ptr)) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for id, expected registered callback", GetValueType(instance, arg));
        return GetNull(env);
    }

    Size idx = GetTrampolineIndex(ptr);

    if (idx < 0 || idx >= MaxTrampolines) [[unlikely]] {
        ThrowError<Napi::Error>(env, "Could not find matching registered callback");
        return GetNull(env);
    }

    // Release shared trampoline safely
    {
        std::lock_guard<std::mutex> lock(shared.mutex);

        TrampolineInfo *trampoline = &shared.trampolines[idx];

        if (trampoline->instance != instance || !trampoline->func) [[unlikely]] {
            ThrowError<Napi::Error>(env, "Could not find matching registered callback");
            return GetNull(env);
        }

        trampoline->state = 0;
        node_api_delete_reference(env, trampoline->func);
        trampoline->func = nullptr;

        shared.available.Append((int16_t)idx);
    }

    return GetUndefined(env);
}

static napi_value CastValue(napi_env env, napi_callback_info info)
{
    napi_value args[2];
    size_t count = 2;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, args, nullptr, (void **)&instance));

    if (count < 2) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 2 arguments, got %1", count);
        return GetNull(env);
    }

    const TypeInfo *type = ResolveType(instance, args[1]);
    if (!type) [[unlikely]]
        return GetNull(env);
    if (type->primitive != PrimitiveKind::Pointer &&
            type->primitive != PrimitiveKind::Callback &&
            type->primitive != PrimitiveKind::String &&
            type->primitive != PrimitiveKind::String16 &&
            type->primitive != PrimitiveKind::String32) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Only pointer or string types can be used for casting");
        return GetNull(env);
    }

    ValueCast *cast = new ValueCast();

    cast->env = env;
    NAPI_OK(napi_create_reference(env, args[0], 1, &cast->ref));
    cast->type = type;

    Napi::External<ValueCast> external = Napi::External<ValueCast>::New(env, cast, [](Napi::BasicEnv, ValueCast *cast) { delete cast; });
    SetValueTag(env, external, &CastMarker);

    return external;
}

static napi_value DecodeValue(napi_env env, napi_callback_info info)
{
    napi_value args[4];
    size_t count = 4;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, args, nullptr, (void **)&instance));

    int64_t offset = 0;
    Size len = 0;
    const void *src = nullptr;
    Size src_len = 0;

    bool has_offset = (count >= 2) && TryNumber(env, args[1], &offset);
    bool has_len = (count >= 3 + has_offset) && TryNumber(env, args[2 + has_offset], &len);

    if (count < 2 + has_offset) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected %1 to 4 arguments, got %2", 2 + has_offset, count);
        return GetNull(env);
    }

    if (!TryPointer(env, args[0], (void **)&src, &src_len)) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for reference, expected pointer", GetValueType(instance, args[0]));
        return GetNull(env);
    }
    if (!src) [[unlikely]] {
        ThrowError<Napi::Error>(env, "Cannot encode data in NULL pointer");
        return GetNull(env);
    }

    src = (const void *)((const uint8_t *)src + offset);

    const TypeInfo *type = ResolveType(instance, args[1 + has_offset]);
    if (!type) [[unlikely]]
        return GetNull(env);

    if (has_len) {
        if (len >= 0) {
            type = MakeArrayType(instance, type, len);
        } else {
            switch (type->primitive) {
                case PrimitiveKind::Int8:
                case PrimitiveKind::UInt8: {
                    Size count = (src_len >= 0) ? strnlen((const char *)src, (size_t)src_len) : strlen((const char *)src);
                    type = MakeArrayType(instance, type, count);
                } break;
                case PrimitiveKind::Int16:
                case PrimitiveKind::UInt16: {
                    Size count = (src_len >= 0) ? NullTerminatedLength((const char16_t *)src, src_len) : NullTerminatedLength((const char16_t *)src);
                    type = MakeArrayType(instance, type, count);
                } break;
                case PrimitiveKind::Int32:
                case PrimitiveKind::UInt32: {
                    Size count = (src_len >= 0) ? NullTerminatedLength((const char32_t *)src, src_len) : NullTerminatedLength((const char32_t *)src);
                    type = MakeArrayType(instance, type, count);
                } break;

                case PrimitiveKind::Pointer: {
                    Size count = (src_len >= 0) ? NullTerminatedLength((const void **)src, src_len) : NullTerminatedLength((const void **)src);
                    type = MakeArrayType(instance, type, count);
                } break;

                default: {
                    ThrowError<Napi::TypeError>(env, "Cannot determine null-terminated length for type %1", type->name);
                    return GetNull(env);
                } break;
            }
        }
    }

    if (src_len >= 0) {
        if (offset < 0) [[unlikely]] {
            ThrowError<Napi::Error>(env, "Offset must be >= 0");
            return GetNull(env);
        }
        if (src_len - offset < type->size) [[unlikely]] {
            ThrowError<Napi::Error>(env, "Expected buffer with size superior or equal to type %1 (%2 bytes)",
                                    type->name, type->size + offset);
            return GetNull(env);
        }
    }

    return Decode(instance, (const uint8_t *)src, type);
}

template <typename T>
static K_FORCE_INLINE napi_value DecodeInteger(napi_env env, napi_callback_info info)
{
    napi_value arg;
    size_t count = 1;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, &arg, nullptr, (void **)&instance));

    if (count < 1) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", count);
        return GetNull(env);
    }

    void *ptr = nullptr;
    if (!TryPointer(env, arg, &ptr)) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for ptr, expected pointer", GetValueType(instance, arg));
        return GetNull(env);
    }

    T i;
    memcpy(&i, ptr, K_SIZE(i));

    return NewInt(env, i);
}

template <typename T>
static K_FORCE_INLINE napi_value DecodeIntegerSwap(napi_env env, napi_callback_info info)
{
    napi_value arg;
    size_t count = 1;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, &arg, nullptr, (void **)&instance));

    if (count < 1) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", count);
        return GetNull(env);
    }

    void *ptr = nullptr;
    if (!TryPointer(env, arg, &ptr)) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for ptr, expected pointer", GetValueType(instance, arg));
        return GetNull(env);
    }

    T i;
    memcpy(&i, ptr, K_SIZE(i));

    return NewInt(env, ReverseBytes(i));
}

#if defined(K_BIG_ENDIAN)

template <typename T>
static K_FORCE_INLINE napi_value DecodeIntegerLE(napi_env env, napi_callback_info info) { return DecodeIntegerSwap<T>(env, info); }
template <typename T>
static K_FORCE_INLINE napi_value DecodeIntegerBE(napi_env env, napi_callback_info info) { return DecodeInteger<T>(env, info); }

#else

template <typename T>
static K_FORCE_INLINE napi_value DecodeIntegerLE(napi_env env, napi_callback_info info) { return DecodeInteger<T>(env, info); }
template <typename T>
static K_FORCE_INLINE napi_value DecodeIntegerBE(napi_env env, napi_callback_info info) { return DecodeIntegerSwap<T>(env, info); }

#endif

static napi_value DecodeFloat(napi_env env, napi_callback_info info)
{
    napi_value arg;
    size_t count = 1;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, &arg, nullptr, (void **)&instance));

    if (count < 1) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", count);
        return GetNull(env);
    }

    void *ptr = nullptr;
    if (!TryPointer(env, arg, &ptr)) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for ptr, expected pointer", GetValueType(instance, arg));
        return GetNull(env);
    }

    float f;
    memcpy(&f, ptr, K_SIZE(f));

    return NewFloat(env, f);
}

static napi_value DecodeDouble(napi_env env, napi_callback_info info)
{
    napi_value arg;
    size_t count = 1;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, &arg, nullptr, (void **)&instance));

    if (count < 1) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", count);
        return GetNull(env);
    }

    void *ptr = nullptr;
    if (!TryPointer(env, arg, &ptr)) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for ptr, expected pointer", GetValueType(instance, arg));
        return GetNull(env);
    }

    double d;
    memcpy(&d, ptr, K_SIZE(d));

    return NewFloat(env, d);
}

static napi_value DecodeString(napi_env env, napi_callback_info info)
{
    napi_value args[2];
    size_t count = 2;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, args, nullptr, (void **)&instance));

    if (count < 1) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 1 to 2 arguments, got %1", count);
        return GetNull(env);
    }

    void *ptr = nullptr;
    if (!TryPointer(env, args[0], &ptr)) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for ptr, expected pointer", GetValueType(instance, args[0]));
        return GetNull(env);
    }

    if (count >= 2) {
        Size len;
        if (!TryNumber(env, args[1], &len)) [[unlikely]] {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for length, expected number", GetValueType(instance, args[1]));
            return GetNull(env);
        }

        return NewString(env, (const char *)ptr, (size_t)len);
    } else {
        return NewString(env, (const char *)ptr);
    }
}

static napi_value DecodeString16(napi_env env, napi_callback_info info)
{
    napi_value args[2];
    size_t count = 2;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, args, nullptr, (void **)&instance));

    if (count < 1) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 1 to 2 arguments, got %1", count);
        return GetNull(env);
    }

    void *ptr = nullptr;
    if (!TryPointer(env, args[0], &ptr)) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for ptr, expected pointer", GetValueType(instance, args[0]));
        return GetNull(env);
    }

    if (count >= 2) {
        Size len;
        if (!TryNumber(env, args[1], &len)) [[unlikely]] {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for length, expected number", GetValueType(instance, args[1]));
            return GetNull(env);
        }

        return NewString(env, (const char16_t *)ptr, (size_t)len);
    } else {
        return NewString(env, (const char16_t *)ptr);
    }
}

static napi_value DecodeString32(napi_env env, napi_callback_info info)
{
    napi_value args[2];
    size_t count = 2;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, args, nullptr, (void **)&instance));

    if (count < 1) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 1 to 2 arguments, got %1", count);
        return GetNull(env);
    }

    void *ptr = nullptr;
    if (!TryPointer(env, args[0], &ptr)) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for ptr, expected pointer", GetValueType(instance, args[0]));
        return GetNull(env);
    }

    if (count >= 2) {
        Size len;
        if (!TryNumber(env, args[1], &len)) [[unlikely]] {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for length, expected number", GetValueType(instance, args[1]));
            return GetNull(env);
        }

        return NewString(env, (const char32_t *)ptr, len);
    } else {
        return NewString(env, (const char32_t *)ptr);
    }
}

static napi_value GetPointerAddress(napi_env env, napi_callback_info info)
{
    napi_value arg;
    size_t count = 1;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, &arg, nullptr, (void **)&instance));

    if (count < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", count);
        return GetNull(env);
    }

    void *ptr = nullptr;
    if (!TryPointer(env, arg, &ptr)) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for ptr, expected pointer", GetValueType(instance, arg));
        return GetNull(env);
    }

    napi_value value;
    NAPI_OK(napi_create_bigint_uint64(env, (uint64_t)(uintptr_t)ptr, &value));

    return value;
}

static napi_value CallPointerSync(napi_env env, napi_callback_info info)
{
    static_assert(MaxParameters >= 8);

    napi_value args[MaxParameters];
    size_t count = 8;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, args, nullptr, (void **)&instance));

    if (count > 8) {
        NAPI_OK(napi_get_cb_info(env, info, &count, args, nullptr, nullptr));
        count = std::min(count, (size_t)MaxParameters);
    }
    if (count < 2) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 2 or more arguments, got %1", count);
        return GetNull(env);
    }

    void *ptr = nullptr;
    if (!TryPointer(env, args[0], &ptr)) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for ptr, expected pointer", GetValueType(instance, args[0]));
        return GetNull(env);
    }

    const TypeInfo *type = ResolveType(instance, args[1]);
    if (!type) [[unlikely]]
        return GetNull(env);
    if (type->primitive != PrimitiveKind::Prototype) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for type, expected function type", GetValueType(instance, args[1]));
        return GetNull(env);
    }

    return CallPointer(env, type->proto, ptr, args + 2, count - 2);
}

static napi_value EncodeValue(napi_env env, napi_callback_info info)
{
    napi_value args[5];
    size_t count = 5;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, args, nullptr, (void **)&instance));

    int64_t offset = 0;
    Size len = 0;
    void *dest = nullptr;
    Size dest_len = 0;

    bool has_offset = (count >= 2) && TryNumber(env, args[1], &offset);
    bool has_len = (count >= 4 + has_offset) && TryNumber(env, args[3 + has_offset], &len);

    if (count < 3 + has_offset) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected %1 to 5 arguments, got %2", 3 + has_offset, count);
        return GetNull(env);
    }

    const TypeInfo *type = ResolveType(instance, args[1 + has_offset]);
    if (!type) [[unlikely]]
        return GetNull(env);

    if (!TryPointer(env, args[0], &dest, &dest_len)) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for reference, expected pointer", GetValueType(instance, args[0]));
        return GetNull(env);
    }
    if (!dest) [[unlikely]] {
        ThrowError<Napi::Error>(env, "Cannot encode data in NULL pointer");
        return GetNull(env);
    }

    dest = (void *)((uint8_t *)dest + offset);

    if (has_len) {
        if (len >= 0) {
            type = MakeArrayType(instance, type, len);
        } else if (dest_len >= 0 && type->size > 0) {
            Size len = dest_len / type->size;
            type = MakeArrayType(instance, type, len);
        } else {
            ThrowError<Napi::TypeError>(env, "Automatic (negative) length cannot work with slim pointers");
            return GetNull(env);
        }
    }

    if (dest_len >= 0) {
        if (offset < 0) [[unlikely]] {
            ThrowError<Napi::Error>(env, "Offset must be >= 0");
            return GetNull(env);
        }
        if (dest_len - offset < type->size) [[unlikely]] {
            ThrowError<Napi::Error>(env, "Expected buffer with size superior or equal to type %1 (%2 bytes)",
                                    type->name, type->size + offset);
            return GetNull(env);
        }
    }

    if (!Encode(instance, (uint8_t *)dest, args[2 + has_offset], type))
        return GetNull(env);

    return GetUndefined(env);
}

static napi_value CreateView(napi_env env, napi_callback_info info)
{
    napi_value args[2];
    size_t count = 2;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, args, nullptr, (void **)&instance));

    if (count < 2) {
        ThrowError<Napi::TypeError>(env, "Expected 2 arguments, got %1", count);
        return GetNull(env);
    }

    void *ptr = nullptr;
    Size len = 0;

    if (!TryPointer(env, args[0], &ptr)) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for ptr, expected pointer", GetValueType(instance, args[0]));
        return GetNull(env);
    }
    if (!TryNumber(env, args[1], &len)) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for length, expected integer", GetValueType(instance, args[1]));
        return GetNull(env);
    }

    if (len > 0) [[likely]] {
        Napi::ArrayBuffer view = Napi::ArrayBuffer::New(env, ptr, (size_t)len);

        if (!view.ByteLength()) {
            ThrowError<Napi::Error>(env, "This runtime does not support external buffers");
            return GetNull(env);
        }

        return view;
    } else if (!len) {
        return Napi::ArrayBuffer::New(env, 0);
    } else {
        ThrowError<Napi::TypeError>(env, "Array length must be positive and non-zero");
        return GetNull(env);
    }
}

static napi_value ResetKoffi(napi_env env, napi_callback_info info)
{
    InstanceData *instance;
    NAPI_OK(napi_get_cb_info(env, info, nullptr, nullptr, nullptr, (void **)&instance));

    if (instance->broker) {
        napi_release_threadsafe_function(instance->broker, napi_tsfn_abort);
        instance->broker = nullptr;
    }

    instance->types.RemoveFrom(instance->base_types_count);

    // Reset type map
    {
        HashSet<const void *> base_types;
        HashMap<const char *, const TypeInfo *> new_map;

        for (const TypeInfo &type: instance->types) {
            base_types.Set(&type);
        }

        for (const auto &bucket: instance->types_map.table) {
            if (base_types.Find(bucket.value)) {
                new_map.Set(bucket.key, bucket.value);
            }
        }

        std::swap(instance->types_map, new_map);
    }

    instance->callbacks.Clear();

    for (InstanceMemory *mem: instance->memories) {
        delete mem;
    }
    instance->memories.Clear();

    return GetUndefined(env);
}

void LibraryHolder::Unload()
{
#if defined(_WIN32)
    if (module && module != GetModuleHandle(nullptr)) {
        FreeLibrary((HMODULE)module);
    }
#else
    if (module && module != RTLD_DEFAULT) {
        dlclose(module);
    }
#endif

    module = nullptr;
}

ValueCast::~ValueCast()
{
    node_api_delete_reference(env, ref);
}

FunctionInfo::~FunctionInfo()
{
    if (lib) {
        lib->Unref();
    }
}

InstanceMemory::~InstanceMemory()
{
#if defined(_WIN32)
    if (stack.ptr) {
        VirtualFree(stack.ptr, 0, MEM_RELEASE);
    }
    if (heap.ptr) {
        VirtualFree(heap.ptr, 0, MEM_RELEASE);
    }
#else
    if (stack.ptr) {
        munmap(stack.ptr, stack.end - stack.ptr);
    }
    if (heap.ptr) {
        munmap(heap.ptr, heap.end - heap.ptr);
    }
#endif
}

void InstanceMemory::Allocate(Size stack_size, Size heap_size)
{
    K_ASSERT(!stack.ptr);
    K_ASSERT(!heap.ptr);

    stack_size = AlignLen(stack_size, Kibibytes(64));

#if defined(_WIN32)
    // Allocate stack memory
    stack.ptr = (uint8_t *)VirtualAlloc(nullptr, stack_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    stack.end = stack.ptr + stack_size;

    K_CRITICAL(stack.ptr, "Failed to allocate %1 of memory", stack_size);
#else
    stack.ptr = (uint8_t *)mmap(nullptr, stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_STACK, -1, 0);
    stack.end = stack.ptr + stack_size;

    K_CRITICAL(stack.ptr != MAP_FAILED, "Failed to allocate %1 of memory", stack_size);
#endif

#if defined(__OpenBSD__)
    // Make sure the SP points inside the MAP_STACK area, or (void) functions may crash on OpenBSD i386
    stack.end -= 16;
#endif

#if defined(_WIN32)
    heap.ptr = (uint8_t *)VirtualAlloc(nullptr, heap_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    heap.end = heap.ptr + heap_size;

    K_CRITICAL(heap.ptr, "Failed to allocate %1 of memory", heap_size);
#else
    heap.ptr = (uint8_t *)mmap(nullptr, heap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    heap.end = heap.ptr + heap_size;

    K_CRITICAL(heap.ptr != MAP_FAILED, "Failed to allocate %1 of memory", heap_size);
#endif
}

static void RegisterPrimitiveType(InstanceData *instance, Napi::Object map, std::initializer_list<const char *> names,
                                  PrimitiveKind primitive, int32_t size, int16_t align, const char *ref = nullptr)
{
    K_ASSERT(names.size() > 0);
    K_ASSERT(align <= size);

    TypeInfo *type = instance->types.AppendDefault();

    type->instance = instance;
    type->name = *names.begin();

    type->primitive = primitive;
    type->size = size;
    type->align = align;

    if (TestStr(type->name, "char")) {
        type->hint = ArrayHint::String8;
    } else if (TestStr(type->name, "char16") || TestStr(type->name, "char16_t")) {
        type->hint = ArrayHint::String16;
    } else if (TestStr(type->name, "char32") || TestStr(type->name, "char32_t")) {
        type->hint = ArrayHint::String32;
    } else if (TestStr(type->name, "wchar") || TestStr(type->name, "wchar_t")) {
        if constexpr (K_SIZE(wchar_t) == 2) {
            type->hint = ArrayHint::String16;
        } else if constexpr (K_SIZE(wchar_t) == 4) {
            type->hint = ArrayHint::String32;
        }
        static_assert(K_SIZE(wchar_t) == 2 || K_SIZE(wchar_t) == 4);
    } else {
        type->hint = SelectTypedHint(type);
    }

    if (ref) {
        type->ref.type = instance->types_map.FindValue(ref, nullptr);
        K_ASSERT(type->ref.type);
    }

    napi_value wrapper = WrapType(instance, type);

    for (const char *name: names) {
        bool inserted;
        instance->types_map.InsertOrGet(name, type, &inserted);
        K_ASSERT(inserted);

        if (!EndsWith(name, "*")) {
            map.Set(name, wrapper);
        }
    }
}

static inline PrimitiveKind GetSignPrimitive(Size len, bool sign)
{
    switch (len) {
        case 1: return sign ? PrimitiveKind::Int8 : PrimitiveKind::UInt8;
        case 2: return sign ? PrimitiveKind::Int16 : PrimitiveKind::UInt16;
        case 4: return sign ? PrimitiveKind::Int32 : PrimitiveKind::UInt32;
        case 8: return sign ? PrimitiveKind::Int64 : PrimitiveKind::UInt64;
    }

    K_UNREACHABLE();
}

static inline PrimitiveKind GetLittleEndianPrimitive(PrimitiveKind kind)
{
#if defined(K_BIG_ENDIAN)
    return (PrimitiveKind)((int)kind + 1);
#else
    return kind;
#endif
}

static inline PrimitiveKind GetBigEndianPrimitive(PrimitiveKind kind)
{
#if defined(K_BIG_ENDIAN)
    return kind;
#else
    return (PrimitiveKind)((int)kind + 1);
#endif
}

static bool CanCallNapiGetBufferInfoDirectly(const napi_node_version &node, uint32_t napi)
{
    if (napi >= 10)
        return true;
    if (node.major >= 22)
        return true;

    // Made by looking at the git history of each release branch
    if (node.major == 21 && node.minor >= 7)
        return true;
    if (node.major == 20 && node.minor >= 12)
        return true;

    return false;
}

static bool CanReferencePrimitiveValues(const napi_node_version &, uint32_t napi)
{
    return napi >= 10;
}

static bool CanDeleteReferenceInFinalizer(const napi_node_version &node, uint32_t)
{
    if (node.major >= 24)
        return true;

    // Made by looking at the git history of each release branch
    if (node.major == 23 && node.minor >= 5)
        return true;
    if (node.major == 22 && node.minor >= 13)
        return true;
    if (node.major == 20 && node.minor >= 19)
        return true;
    if (node.major == 20 && node.minor == 18 && node.patch >= 3)
        return true;

    return false;
}

static napi_value CreateFunction(InstanceData *instance, napi_callback native, const char *name = nullptr)
{
    Napi::Env env = instance->env;

    napi_value func;
    NAPI_OK(napi_create_function(env, name, NAPI_AUTO_LENGTH, native, instance, &func));

    return func;
}

static Napi::Object InitModule(Napi::Env env, Napi::Object exports)
{
    // Load recent Node-API functions (version >= 9) functions dynamically
    {
        static std::once_flag flag;

        std::call_once(flag, [&]() {
            InitTranslateZeroCall(env);

#if defined(_WIN32)
            HMODULE h = GetModuleHandle(nullptr);
    #define SYMBOL(Symbol) ((decltype(Symbol))GetProcAddress(h, K_STRINGIFY(Symbol)))
#else
            void *h = RTLD_DEFAULT;
    #define SYMBOL(Symbol) ((decltype(Symbol))dlsym(h, K_STRINGIFY(Symbol)))
#endif

            const napi_node_version *node_version = nullptr;
            uint32_t napi_version = 0;
            napi_get_node_version(env, &node_version);
            napi_get_version(env, &napi_version);

            if (CanCallNapiGetBufferInfoDirectly(*node_version, napi_version)) {
                node_api_get_buffer_info = napi_get_buffer_info;
            } else {
                // Before Node 20.12, napi_get_buffer_info() would assert/crash
                // when used with something it did not support, instead of returning napi_invalid_arg.
                // So we need to call napi_is_buffer() for old versions before trying napi_get_buffer_info().

                node_api_get_buffer_info = [](napi_env env, napi_value value, void **data, size_t *length) {
                    if (!IsBuffer(env, value))
                        return napi_invalid_arg;
                    return napi_get_buffer_info(env, value, data, length);
                };
            }

            if (CanReferencePrimitiveValues(*node_version, napi_version)) {
                // We can't use optimized property keys in older versions because we need to create
                // references to them, but napi_create_reference() was not usable with primitive values.
                node_api_create_property_key_utf8 = SYMBOL(node_api_create_property_key_utf8);
            }

            if (!CanDeleteReferenceInFinalizer(*node_version, napi_version)) {
                // napi_delete_reference cannot be safely used in older Node versions because it
                // errors out (or even asserts) if it gets called in a finalizer. In this case,
                // use experimental API to try to run it later.
                node_api_post_finalizer = SYMBOL(node_api_post_finalizer);

                if (node_api_post_finalizer) {
                    node_api_delete_reference = [](node_api_basic_env env, napi_ref ref) {
                        node_api_post_finalizer((napi_env)env, [](napi_env env, void *data, void *) {
                            napi_ref ref = (napi_ref)data;
                            napi_delete_reference(env, ref);
                        }, (void *)ref, nullptr);

                        return napi_ok;
                    };
                } else {
                    node_api_delete_reference = napi_delete_reference;
                }
            } else {
                node_api_delete_reference = napi_delete_reference;
            }

            node_api_create_object_with_properties = SYMBOL(node_api_create_object_with_properties);

#undef SYMBOL
        });
    }

    InstanceData *instance = new InstanceData();
    K_CRITICAL(instance, "Failed to initialize Koffi");

    instance->env = env;

#if defined(__clang__)
    // First call to napi_create_double() does some weird stuff I can't explain
    // in clang-cl builds. I don't like this... but this fixes it, somehow ><
    NewFloat(env, 0.0);
#endif

    exports.Set("config", CreateFunction(instance, GetSetConfig, "config"));
    exports.Set("stats", CreateFunction(instance, GetStats, "stats"));

    exports.Set("struct", Napi::Function::New(env, CreatePaddedStructType, "struct", instance));
    exports.Set("pack", Napi::Function::New(env, CreatePackedStructType, "pack", instance));
    exports.Set("union", Napi::Function::New(env, CreateUnionType, "union", instance));
    exports.Set("Union", Napi::Function::New(env, InstantiateUnion, "Union", instance));
    exports.Set("opaque", Napi::Function::New(env, CreateOpaqueType, "opaque", instance));
    exports.Set("pointer", Napi::Function::New(env, CreatePointerType, "pointer", instance));
    exports.Set("disposable", Napi::Function::New(env, CreateDisposableType, "disposable", instance));
    exports.Set("array", Napi::Function::New(env, CreateArrayType, "array", instance));
    exports.Set("proto", Napi::Function::New(env, CreateFunctionType, "proto", instance));
    exports.Set("alias", Napi::Function::New(env, CreateTypeAlias, "alias", instance));
    exports.Set("enumeration", Napi::Function::New(env, CreateEnumType, "enumeration", instance));

    exports.Set("type", CreateFunction(instance, GetResolvedType, "type"));
    exports.Set("sizeof", CreateFunction(instance, GetTypeSize, "sizeof"));
    exports.Set("alignof", CreateFunction(instance, GetTypeAlign, "alignof"));
    exports.Set("offsetof", CreateFunction(instance, GetMemberOffset, "offsetof"));

    exports.Set("load", CreateFunction(instance, LoadSharedLibrary, "load"));

    exports.Set("in", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return MarkPointer(env, info, 1); }, "in"));
    exports.Set("out", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return MarkPointer(env, info, 2); }, "out"));
    exports.Set("inout", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return MarkPointer(env, info, 3); }, "inout"));

    exports.Set("alloc", CreateFunction(instance, CallAlloc, "alloc"));
    exports.Set("free", CreateFunction(instance, CallFree, "free"));

    exports.Set("register", CreateFunction(instance, RegisterCallback, "register"));
    exports.Set("unregister", CreateFunction(instance, UnregisterCallback, "unregister"));

    exports.Set("as", CreateFunction(instance, CastValue, "as"));
    exports.Set("address", CreateFunction(instance, GetPointerAddress, "address"));
    exports.Set("call", CreateFunction(instance, CallPointerSync, "call"));
    exports.Set("encode", CreateFunction(instance, EncodeValue, "encode"));
    exports.Set("view", CreateFunction(instance, CreateView, "view"));

    {
        napi_value decode = CreateFunction(instance, DecodeValue, "decode");
        Napi::Function obj(env, decode);

        obj.Set("char", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<char>(env, info); }, "char"));
        obj.Set("uchar", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<unsigned char>(env, info); }, "uchar"));
        obj.Set("short", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<short>(env, info); }, "short"));
        obj.Set("ushort", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<unsigned short>(env, info); }, "ushort"));
        obj.Set("int", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<int>(env, info); }, "int"));
        obj.Set("uint", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<unsigned int>(env, info); }, "uint"));
        obj.Set("long", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<long>(env, info); }, "long"));
        obj.Set("ulong", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<unsigned long>(env, info); }, "ulong"));
        obj.Set("longlong", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<long long>(env, info); }, "longlong"));
        obj.Set("ulonglong", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<unsigned long long>(env, info); }, "ulonglong"));
        obj.Set("int8", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<int8_t>(env, info); }, "int8"));
        obj.Set("uint8", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<uint8_t>(env, info); }, "uint8"));
        obj.Set("int16", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<int16_t>(env, info); }, "int16"));
        obj.Set("int16le", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeIntegerLE<int16_t>(env, info); }, "int16le"));
        obj.Set("int16be", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeIntegerBE<int16_t>(env, info); }, "int16be"));
        obj.Set("uint16", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<uint16_t>(env, info); }, "uint16"));
        obj.Set("uint16le", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeIntegerLE<uint16_t>(env, info); }, "uint16le"));
        obj.Set("uint16be", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeIntegerBE<uint16_t>(env, info); }, "uint16be"));
        obj.Set("int32", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<int32_t>(env, info); }, "int32"));
        obj.Set("int32le", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeIntegerLE<int32_t>(env, info); }, "int32le"));
        obj.Set("int32be", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeIntegerBE<int32_t>(env, info); }, "int32be"));
        obj.Set("uint32", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<uint32_t>(env, info); }, "uint32"));
        obj.Set("uint32le", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeIntegerLE<uint32_t>(env, info); }, "uint32le"));
        obj.Set("uint32be", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeIntegerBE<uint32_t>(env, info); }, "uint32be"));
        obj.Set("int64", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<int64_t>(env, info); }, "int64"));
        obj.Set("int64le", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeIntegerLE<int64_t>(env, info); }, "int64le"));
        obj.Set("int64be", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeIntegerBE<int64_t>(env, info); }, "int64be"));
        obj.Set("uint64", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeInteger<uint64_t>(env, info); }, "uint64"));
        obj.Set("uint64le", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeIntegerLE<uint64_t>(env, info); }, "uint64le"));
        obj.Set("uint64be", CreateFunction(instance, [](napi_env env, napi_callback_info info) { return DecodeIntegerBE<uint64_t>(env, info); }, "uint64be"));
        obj.Set("float", CreateFunction(instance, DecodeFloat, "float"));
        obj.Set("double", CreateFunction(instance, DecodeDouble, "double"));
        obj.Set("string", CreateFunction(instance, DecodeString, "string"));
        obj.Set("string16", CreateFunction(instance, DecodeString16, "string16"));
        obj.Set("string32", CreateFunction(instance, DecodeString32, "string32"));
        if constexpr (K_SIZE(wchar_t) == 2) {
            obj.Set("wstring", CreateFunction(instance, DecodeString16, "wstring"));
        } else if constexpr (K_SIZE(wchar_t) == 4) {
            obj.Set("wstring", CreateFunction(instance, DecodeString32, "wstring"));
        }
        static_assert(K_SIZE(wchar_t) == 2 || K_SIZE(wchar_t) == 4);

        exports.Set("decode", decode);
    }

    exports.Set("reset", CreateFunction(instance, ResetKoffi, "reset"));

    exports.Set("errno", CreateFunction(instance, GetOrSetErrno, "errno"));

    // Export useful OS info
    {
        Napi::Object os = Napi::Object::New(env);
        exports.Set("os", os);

        Napi::Object codes = Napi::Object::New(env);

        for (const ErrnoCodeInfo &info: ErrnoCodes) {
            codes.Set(info.name, NewInt(env, (int32_t)info.value));
        }

        os.Set("errno", codes);
    }

#if defined(_WIN32)
    exports.Set("extension", NewString(env, ".dll"));
#elif defined(__APPLE__)
    exports.Set("extension", NewString(env, ".dylib"));
#else
    exports.Set("extension", NewString(env, ".so"));
#endif

    // Init object classes and symbols
    {
        instance->object_constructor = Napi::Persistent(env.RunScript("Object.prototype").As<Napi::Object>());
        instance->construct_lib = Napi::Persistent(LibraryHandle::InitClass(instance));
        instance->construct_type = Napi::Persistent(TypeObject::InitClass(instance));
        instance->construct_poll = Napi::Persistent(PollHandle::InitClass(instance));
        instance->active_symbol = Napi::Persistent(Napi::Symbol::New(env, "active"));

        exports.Set("LibraryHandle", instance->construct_lib.Value());
        exports.Set("TypeObject", instance->construct_type.Value());
    }

    // Init base types
    {
        Napi::Object types = Napi::Object::New(env);
        exports.Set("types", types);

        RegisterPrimitiveType(instance, types, {"void"}, PrimitiveKind::Void, 0, 0);
        RegisterPrimitiveType(instance, types, {"bool"}, PrimitiveKind::Bool, K_SIZE(bool), alignof(bool));
        RegisterPrimitiveType(instance, types, {"int8_t", "int8"}, PrimitiveKind::Int8, 1, 1);
        RegisterPrimitiveType(instance, types, {"uint8_t", "uint8"}, PrimitiveKind::UInt8, 1, 1);
        RegisterPrimitiveType(instance, types, {"char"}, PrimitiveKind::Int8, 1, 1);
        RegisterPrimitiveType(instance, types, {"unsigned char", "uchar"}, PrimitiveKind::UInt8, 1, 1);
        RegisterPrimitiveType(instance, types, {"char16_t", "char16"}, PrimitiveKind::Int16, 2, 2);
        RegisterPrimitiveType(instance, types, {"char32_t", "char32"}, PrimitiveKind::Int32, 4, 4);
        if constexpr (K_SIZE(wchar_t) == 2) {
            RegisterPrimitiveType(instance, types, {"wchar_t", "wchar"}, PrimitiveKind::Int16, 2, 2);
        } else if constexpr (K_SIZE(wchar_t) == 4) {
            RegisterPrimitiveType(instance, types, {"wchar_t", "wchar"}, PrimitiveKind::Int32, 4, 4);
        }
        RegisterPrimitiveType(instance, types, {"int16_t", "int16"}, PrimitiveKind::Int16, 2, 2);
        RegisterPrimitiveType(instance, types, {"int16_le_t", "int16_le"}, GetLittleEndianPrimitive(PrimitiveKind::Int16), 2, 2);
        RegisterPrimitiveType(instance, types, {"int16_be_t", "int16_be"}, GetBigEndianPrimitive(PrimitiveKind::Int16), 2, 2);
        RegisterPrimitiveType(instance, types, {"uint16_t", "uint16"}, PrimitiveKind::UInt16, 2, 2);
        RegisterPrimitiveType(instance, types, {"uint16_le_t", "uint16_le"}, GetLittleEndianPrimitive(PrimitiveKind::UInt16), 2, 2);
        RegisterPrimitiveType(instance, types, {"uint16_be_t", "uint16_be"}, GetBigEndianPrimitive(PrimitiveKind::UInt16), 2, 2);
        RegisterPrimitiveType(instance, types, {"short"}, PrimitiveKind::Int16, 2, 2);
        RegisterPrimitiveType(instance, types, {"unsigned short", "ushort"}, PrimitiveKind::UInt16, 2, 2);
        RegisterPrimitiveType(instance, types, {"int32_t", "int32"}, PrimitiveKind::Int32, 4, 4);
        RegisterPrimitiveType(instance, types, {"int32_le_t", "int32_le"}, GetLittleEndianPrimitive(PrimitiveKind::Int32), 4, 4);
        RegisterPrimitiveType(instance, types, {"int32_be_t", "int32_be"}, GetBigEndianPrimitive(PrimitiveKind::Int32), 4, 4);
        RegisterPrimitiveType(instance, types, {"uint32_t", "uint32"}, PrimitiveKind::UInt32, 4, 4);
        RegisterPrimitiveType(instance, types, {"uint32_le_t", "uint32_le"}, GetLittleEndianPrimitive(PrimitiveKind::UInt32), 4, 4);
        RegisterPrimitiveType(instance, types, {"uint32_be_t", "uint32_be"}, GetBigEndianPrimitive(PrimitiveKind::UInt32), 4, 4);
        RegisterPrimitiveType(instance, types, {"int"}, PrimitiveKind::Int32, 4, 4);
        RegisterPrimitiveType(instance, types, {"unsigned int", "uint"}, PrimitiveKind::UInt32, 4, 4);
        RegisterPrimitiveType(instance, types, {"int64_t", "int64"}, PrimitiveKind::Int64, 8, alignof(int64_t));
        RegisterPrimitiveType(instance, types, {"int64_le_t", "int64_le"}, GetLittleEndianPrimitive(PrimitiveKind::Int64), 8, alignof(int64_t));
        RegisterPrimitiveType(instance, types, {"int64_be_t", "int64_be"}, GetBigEndianPrimitive(PrimitiveKind::Int64), 8, alignof(int64_t));
        RegisterPrimitiveType(instance, types, {"uint64_t", "uint64"}, PrimitiveKind::UInt64, 8, alignof(int64_t));
        RegisterPrimitiveType(instance, types, {"uint64_le_t", "uint64_le"}, GetLittleEndianPrimitive(PrimitiveKind::UInt64), 8, alignof(int64_t));
        RegisterPrimitiveType(instance, types, {"uint64_be_t", "uint64_be"}, GetBigEndianPrimitive(PrimitiveKind::UInt64), 8, alignof(int64_t));
        RegisterPrimitiveType(instance, types, {"intptr_t", "intptr"}, GetSignPrimitive(K_SIZE(intptr_t), true), K_SIZE(intptr_t), alignof(intptr_t));
        RegisterPrimitiveType(instance, types, {"uintptr_t", "uintptr"}, GetSignPrimitive(K_SIZE(intptr_t), false), K_SIZE(intptr_t), alignof(intptr_t));
        RegisterPrimitiveType(instance, types, {"size_t"}, GetSignPrimitive(K_SIZE(size_t), false), K_SIZE(size_t), alignof(size_t));
        RegisterPrimitiveType(instance, types, {"long"}, GetSignPrimitive(K_SIZE(long), true), K_SIZE(long), alignof(long));
        RegisterPrimitiveType(instance, types, {"unsigned long", "ulong"}, GetSignPrimitive(K_SIZE(long), false), K_SIZE(long), alignof(long));
        RegisterPrimitiveType(instance, types, {"long long", "longlong"}, PrimitiveKind::Int64, K_SIZE(int64_t), alignof(int64_t));
        RegisterPrimitiveType(instance, types, {"unsigned long long", "ulonglong"}, PrimitiveKind::UInt64, K_SIZE(uint64_t), alignof(uint64_t));
        RegisterPrimitiveType(instance, types, {"float", "float32"}, PrimitiveKind::Float32, 4, alignof(float));
        RegisterPrimitiveType(instance, types, {"double", "float64"}, PrimitiveKind::Float64, 8, alignof(double));
        RegisterPrimitiveType(instance, types, {"char *", "str", "string"}, PrimitiveKind::String, K_SIZE(void *), alignof(void *), "char");
        RegisterPrimitiveType(instance, types, {"char16_t *", "char16 *", "str16", "string16"}, PrimitiveKind::String16, K_SIZE(void *), alignof(void *), "char16_t");
        RegisterPrimitiveType(instance, types, {"char32_t *", "char32 *", "str32", "string32"}, PrimitiveKind::String32, K_SIZE(void *), alignof(void *), "char32_t");
        if constexpr (K_SIZE(wchar_t) == 2) {
            RegisterPrimitiveType(instance, types, {"wchar_t *", "wchar *", "wstring"}, PrimitiveKind::String16, K_SIZE(void *), alignof(void *), "wchar_t");
        } else if constexpr (K_SIZE(wchar_t) == 4) {
            RegisterPrimitiveType(instance, types, {"wchar_t *", "wchar *", "wstring"}, PrimitiveKind::String32, K_SIZE(void *), alignof(void *), "wchar_t");
        }
        static_assert(K_SIZE(wchar_t) == 2 || K_SIZE(wchar_t) == 4);

        instance->void_type = instance->types_map.FindValue("void", nullptr);
        instance->char_type = instance->types_map.FindValue("char", nullptr);
        instance->char16_type = instance->types_map.FindValue("char16", nullptr);
        instance->char32_type = instance->types_map.FindValue("char32", nullptr);
        instance->str_type = instance->types_map.FindValue("char *", nullptr);
        instance->str16_type = instance->types_map.FindValue("char16_t *", nullptr);
        instance->str32_type = instance->types_map.FindValue("char32_t *", nullptr);
        instance->double_type = instance->types_map.FindValue("double", nullptr);

        instance->base_types_count = instance->types.count;
    }

    // Expose internal Node stuff
    {
        Napi::Object node = Napi::Object::New(env);
        exports.Set("node", node);

        node.Set("env", WrapPointer(env, (napi_env)env));

        node.Set("poll", CreateFunction(instance, &Poll, "poll"));
        node.Set("PollHandle", instance->construct_poll.Value());
    }

    exports.Set("version", NewString(env, K_STRINGIFY(VERSION)));

#if defined(_WIN32)
    {
        TEB *teb = GetTEB();

        instance->real_stack.end = teb->StackBase;
        instance->real_stack.ptr = teb->DeallocationStack;
    }
#endif

    instance->main_thread_id = std::this_thread::get_id();

    napi_add_env_cleanup_hook(env, [](void *udata) {
        InstanceData *instance = (InstanceData *)udata;

        if (instance->broker) {
            // This deadlocks if we try to do this when the module is destroyed, when
            // the InstanceData destructor runs, so run in the env cleanup hook instead,
            // where it seems to go okay.
            napi_release_threadsafe_function(instance->broker, napi_tsfn_abort);
        }
    }, instance);

    return exports;
}

InstanceData::~InstanceData()
{
    delete variadic_func;

    for (InstanceMemory *mem: memories) {
        delete mem;
    }

    // Clean-up leftover trampoline references
    {
        std::lock_guard<std::mutex> lock(shared.mutex);

        for (int16_t idx = 0; idx < MaxTrampolines; idx++) {
            TrampolineInfo *trampoline = &shared.trampolines[idx];

            if (trampoline->instance == this) {
                trampoline->instance = nullptr;
                if (trampoline->func) {
                    node_api_delete_reference(env, trampoline->func);
                    trampoline->func = nullptr;
                }
                trampoline->state = 0;
            }
        }
    }
}

NODE_API_MODULE(koffi, InitModule);

}
