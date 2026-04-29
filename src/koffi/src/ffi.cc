// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "ffi.hh"
#include "call.hh"
#include "parser.hh"
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

extern "C" napi_value SwitchAndRelay(CallData *call, Size idx, uint8_t *sp, uint8_t *saved_sp, MemoryRange<uint8_t> *new_stack);

SharedData shared;

// Recent N-API functions are loaded dynamically
napi_status (NAPI_CDECL *node_api_create_property_key_utf8)(napi_env env, const char* str, size_t length, napi_value* result);
napi_status (NAPI_CDECL *node_api_post_finalizer)(node_api_basic_env env, napi_finalize finalize_cb, void* finalize_data, void* finalize_hint);
napi_value (*translate_zero_call)(napi_env env, napi_callback_info info);

static bool ChangeSize(const char *name, Napi::Value value, Size min_size, Size max_size, Size *out_size)
{
    Napi::Env env = value.Env();

    if (!value.IsNumber()) {
        InstanceData *instance = env.GetInstanceData<InstanceData>();

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

static bool ChangeMemorySize(const char *name, Napi::Value value, Size *out_size)
{
    const Size MinSize = Kibibytes(1);
    const Size MaxSize = Mebibytes(16);

    return ChangeSize(name, value, MinSize, MaxSize, out_size);
}

static bool ChangeAsyncLimit(const char *name, Napi::Value value, int max, int *out_limit)
{
    Napi::Env env = value.Env();

    if (!value.IsNumber()) {
        InstanceData *instance = env.GetInstanceData<InstanceData>();

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

static Napi::Value GetSetConfig(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length()) {
        if (instance->memories.len) {
            ThrowError<Napi::Error>(env, "Cannot change Koffi configuration once a library has been loaded");
            return env.Null();
        }

        if (!IsObject(env, info[0])) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for config, expected object", GetValueType(instance, info[0]));
            return env.Null();
        }

        decltype(instance->config) new_config = instance->config;
        int max_async_calls = new_config.resident_async_pools + new_config.max_temporaries;

        Napi::Object obj = info[0].As<Napi::Object>();
        Napi::Array keys = GetOwnPropertyNames(env, obj);

        for (uint32_t i = 0; i < keys.Length(); i++) {
            std::string key = keys.Get(i).As<Napi::String>();
            Napi::Value value = obj[key];

            if (key == "sync_stack_size") {
                if (!ChangeMemorySize(key.c_str(), value, &new_config.sync_stack_size))
                    return env.Null();
            } else if (key == "sync_heap_size") {
                if (!ChangeMemorySize(key.c_str(), value, &new_config.sync_heap_size))
                    return env.Null();
            } else if (key == "async_stack_size") {
                if (!ChangeMemorySize(key.c_str(), value, &new_config.async_stack_size))
                    return env.Null();
            } else if (key == "async_heap_size") {
                if (!ChangeMemorySize(key.c_str(), value, &new_config.async_heap_size))
                    return env.Null();
            } else if (key == "resident_async_pools") {
                if (!ChangeAsyncLimit(key.c_str(), value, K_LEN(instance->memories.data) - 1, &new_config.resident_async_pools))
                    return env.Null();
            } else if (key == "max_async_calls") {
                if (!ChangeAsyncLimit(key.c_str(), value, MaxAsyncCalls, &max_async_calls))
                    return env.Null();
            } else if (key == "max_type_size") {
                if (!ChangeSize(key.c_str(), value, 32, Mebibytes(512), &new_config.max_type_size))
                    return env.Null();
            } else {
                ThrowError<Napi::Error>(env, "Unexpected config member '%1'", key.c_str());
                return env.Null();
            }
        }

        if (max_async_calls < new_config.resident_async_pools) {
            ThrowError<Napi::Error>(env, "Setting max_async_calls must be >= to resident_async_pools");
            return env.Null();
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

static Napi::Value GetStats(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    Napi::Object obj = Napi::Object::New(env);

    obj.Set("disposed", instance->stats.disposed);

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
            node_api_create_property_key_utf8(env, member.name, NAPI_AUTO_LENGTH, &key);

            napi_create_reference(env, key, 1, &member.key);
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
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 or 2 arguments, got %1", info.Length());
        return env.Null();
    }

    bool skip = (info.Length() > 1);
    bool named = skip && !IsNullOrUndefined(env, info[0]);
    bool redefine = named && info[0].IsObject() && CheckValueTag(env, info[0], &TypeInfoMarker);

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
    Napi::Array keys = GetOwnPropertyNames(env, obj);

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
        TypeClass *defn = TypeClass::Unwrap(name.As<Napi::Object>());

        replace = (TypeInfo *)defn->GetType();
        type->name = replace->name;

        if (replace->primitive != PrimitiveKind::Void || replace == instance->void_type) {
            ThrowError<Napi::TypeError>(env, "Cannot redefine non-opaque type %1", replace->name);
            return env.Null();
        }
    } else if (named) {
        type->name = DuplicateString(name.Utf8Value().c_str(), &instance->str_alloc).ptr;

        if (!MapType(env, instance, type, type->name))
            return env.Null();
    } else {
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

        member.type = ResolveType(value);
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
        std::swap(*type, *replace);
        type = replace;
    }

    return WrapType(env, type);
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
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 or 2 arguments, got %1", info.Length());
        return env.Null();
    }

    bool skip = (info.Length() > 1);
    bool named = skip && !IsNullOrUndefined(env, info[0]);
    bool redefine = named && info[0].IsObject() && CheckValueTag(env, info[0], &TypeInfoMarker);

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
    Napi::Array keys = GetOwnPropertyNames(env, obj);

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
        TypeClass *defn = TypeClass::Unwrap(name.As<Napi::Object>());

        replace = (TypeInfo *)defn->GetType();
        type->name = replace->name;

        if (replace->primitive != PrimitiveKind::Void || replace == instance->void_type) {
            ThrowError<Napi::TypeError>(env, "Cannot redefine non-opaque type %1", replace->name);
            return env.Null();
        }
    } else if (named) {
        type->name = DuplicateString(name.Utf8Value().c_str(), &instance->str_alloc).ptr;

        if (!MapType(env, instance, type, type->name))
            return env.Null();
    } else {
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

        member.type = ResolveType(value);
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

    // Union constructor
    Napi::Function constructor = UnionClass::InitClass(env, type);
    type->construct.Reset(constructor, 1);

    if (replace) {
        std::swap(*type, *replace);
        type = replace;
    }

    return WrapType(env, type);
}

Napi::Value InstantiateUnion(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!info.IsConstructCall()) {
        ThrowError<Napi::TypeError>(env, "This function is a constructor and must be called with new");
        return env.Null();
    }
    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }

    const TypeInfo *type = ResolveType(info[0]);
    if (!type)
        return env.Null();
    if (type->primitive != PrimitiveKind::Union) {
        ThrowError<Napi::TypeError>(env, "Expected union type, got %1", PrimitiveKindNames[(int)type->primitive]);
        return env.Null();
    }

    Napi::Object wrapper = type->construct.New({}).As<Napi::Object>();
    SetValueTag(env, wrapper, &UnionClassMarker);

    return wrapper;
}

static Napi::Value CreateOpaqueType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    bool named = (info.Length() >= 1) && !IsNullOrUndefined(env, info[0]);

    if (named && !info[0].IsString()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for name, expected string", GetValueType(instance, info[0]));
        return env.Null();
    }

    Napi::String name = info[0].As<Napi::String>();

    TypeInfo *type = instance->types.AppendDefault();
    K_DEFER_N(err_guard) { instance->types.RemoveLast(1); };

    type->name = named ? DuplicateString(name.Utf8Value().c_str(), &instance->str_alloc).ptr
                       : Fmt(&instance->str_alloc, "<anonymous_%1>", instance->types.count).ptr;

    type->primitive = PrimitiveKind::Void;
    type->size = 0;
    type->align = 0;

    // If the insert succeeds, we cannot fail anymore
    if (named && !MapType(env, instance, type, type->name))
        return env.Null();
    err_guard.Disable();

    return WrapType(env, type);
}

static Napi::Value CreatePointerType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

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

    const TypeInfo *ref = ResolveType(info[skip]);
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
        memset(&copy->defn, 0, K_SIZE(copy->defn));

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

    return WrapType(env, type);
}

static Napi::Value EncodePointerDirection(const Napi::CallbackInfo &info, int directions)
{
    K_ASSERT(directions >= 1 && directions <= 3);

    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }

    const TypeInfo *type = ResolveType(info[0]);
    if (!type)
        return env.Null();

    if (type->primitive != PrimitiveKind::Pointer &&
            type->primitive != PrimitiveKind::String &&
            type->primitive != PrimitiveKind::String16 &&
            type->primitive != PrimitiveKind::String32) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 type, expected pointer or string type", type->name);
        return env.Null();
    }

    // Embed direction in unused pointer bits
    const TypeInfo *marked = (const TypeInfo *)((uint8_t *)type + directions - 1);

    Napi::External<TypeInfo> external = Napi::External<TypeInfo>::New(env, (TypeInfo *)marked);
    SetValueTag(env, external, &DirectionMarker);

    return external;
}

static Napi::Value MarkIn(const Napi::CallbackInfo &info)
{
    return EncodePointerDirection(info, 1);
}

static Napi::Value MarkOut(const Napi::CallbackInfo &info)
{
    return EncodePointerDirection(info, 2);
}

static Napi::Value MarkInOut(const Napi::CallbackInfo &info)
{
    return EncodePointerDirection(info, 3);
}

static Napi::Value CreateDisposableType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

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

    const TypeInfo *src = ResolveType(info[skip]);
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

        dispose = [](Napi::Env env, const TypeInfo *type, const void *ptr) {
            InstanceData *instance = env.GetInstanceData<InstanceData>();
            const Napi::FunctionReference &ref = type->dispose_ref;

            Napi::Value p = WrapPointer(env, type->ref.type, (void *)ptr);

            napi_value self = env.Null();
            napi_value args[] = { p };

            ref.Call(self, K_LEN(args), args);
            instance->stats.disposed++;
        };
        dispose_func = func;
    } else {
        dispose = [](Napi::Env env, const TypeInfo *, const void *ptr) {
            InstanceData *instance = env.GetInstanceData<InstanceData>();

            free((void *)ptr);
            instance->stats.disposed++;
        };
    }

    TypeInfo *type = instance->types.AppendDefault();
    K_DEFER_N(err_guard) { instance->types.RemoveLast(1); };

    memcpy((void *)type, (const void *)src, K_SIZE(*src));
    type->members.allocator = GetNullAllocator();
    memset(&type->defn, 0, K_SIZE(type->defn));

    type->name = named ? DuplicateString(name.Utf8Value().c_str(), &instance->str_alloc).ptr
                       : Fmt(&instance->str_alloc, "<anonymous_%1>", instance->types.count).ptr;

    type->dispose = dispose;
    type->dispose_ref = Napi::Persistent(dispose_func);

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

    return WrapType(env, type);
}

static inline bool GetExternalPointer(napi_env env, napi_value value, void **out_ptr)
{
    if (!TryPointer(env, value, out_ptr)) {
        InstanceData *instance = Napi::Env(env).GetInstanceData<InstanceData>();

        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for ptr, expected pointer", GetValueType(instance, value));
        return false;
    }

    return true;
}

static Napi::Value CallAlloc(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 2) {
        ThrowError<Napi::TypeError>(env, "Expected 2 arguments, got %1", info.Length());
        return env.Null();
    }
    if (!info[1].IsNumber()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for length, expected number", GetValueType(instance, info[1]));
        return env.Null();
    }

    const TypeInfo *type = ResolveType(info[0]);
    if (!type)
        return env.Null();

    if (!type->size) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Cannot allocate memory for zero-sized type %1", type->name);
        return env.Null();
    }

    int32_t len = info[1].As<Napi::Number>();

    if (len <= 0) [[unlikely]] {
        ThrowError<Napi::Error>(env, "Size must be greater than 0");
        return env.Null();
    }
    if (len > INT32_MAX / type->size) [[unlikely]] {
        ThrowError<Napi::Error>(env, "Cannot allocate more than %1 objects of type %2", INT32_MAX / type->size, type->name);
        return env.Null();
    }

    void *ptr = calloc((size_t)len, (size_t)type->size);

    if (!ptr) [[unlikely]] {
        Size size = (Size)(len * type->size);

        ThrowError<Napi::Error>(env, "Failed to allocate %1 of memory", FmtMemSize((Size)size));
        return env.Null();
    }

    return WrapPointer(env, type, ptr);
}

static Napi::Value CallFree(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }

    void *ptr = nullptr;
    if (!GetExternalPointer(env, info[0], &ptr))
        return env.Null();

    free(ptr);

    return env.Undefined();
}

static Napi::Value GetOrSetErrNo(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() >= 1) {
        Napi::Number value = info[0].As<Napi::Number>();

        if (!value.IsNumber()) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for errno, expected integer", GetValueType(instance, value));
            return env.Null();
        }

        errno = value;
    }

    return NewInt(env, (int32_t)errno);
}

static Napi::Value CreateArrayType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 2) {
        ThrowError<Napi::TypeError>(env, "Expected 2 to 4 arguments, got %1", info.Length());
        return env.Null();
    }

    const TypeInfo *ref = ResolveType(info[0]);
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
            if (!(ref->flags & (int)TypeFlag::HasTypedArray)) {
                ThrowError<Napi::Error>(env, "Array hint 'Typed' cannot be used with type %1", ref->name);
                return env.Null();
            }

            hint = ArrayHint::Typed;
        } else if (str == "Array" || str == "array") {
            hint = ArrayHint::Array;
        } else if (str == "String" || str == "string") {
            if (ref->primitive != PrimitiveKind::Int8 && ref->primitive != PrimitiveKind::Int16) {
                ThrowError<Napi::Error>(env, "Array hint 'String' can only be used with 8, 16 and 32-bit signed integer types");
                return env.Null();
            }

            hint = ArrayHint::String;
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

    return WrapType(env, type);
}

static bool ParseClassicFunction(const Napi::CallbackInfo &info, bool concrete, FunctionInfo *out_func)
{
    K_ASSERT(info.Length() >= 2);

    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

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

    out_func->ret.type = ResolveType(ret);
    if (!out_func->ret.type)
        return false;
    if (!CanReturnType(out_func->ret.type)) {
        ThrowError<Napi::TypeError>(env, "You are not allowed to directly return %1 values (maybe try %1 *)", out_func->ret.type->name);
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

        param.type = ResolveType(parameters[j], &param.directions);

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
    InstanceData *instance = env.GetInstanceData<InstanceData>();

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
        if (!ParsePrototype(env, proto.c_str(), false, func))
            return env.Null();
    } else {
        ThrowError<Napi::TypeError>(env, "Expected 1 to 4 arguments, got %1", info.Length());
        return env.Null();
    }

    bool named = func->name;

    if (!named) {
        func->name = Fmt(&instance->str_alloc, "<anonymous_%1>", instance->types.count).ptr;
    }

    if (!AnalyseFunction(env, instance, func))
        return env.Null();

    // We cannot fail after this check
    if (named && instance->types_map.Find(func->name)) {
        ThrowError<Napi::Error>(env, "Duplicate type name '%1'", func->name);
        return env.Null();
    }
    err_guard.Disable();

    TypeInfo *type = instance->types.AppendDefault();

    type->name = func->name;

    type->primitive = PrimitiveKind::Prototype;
    type->align = alignof(void *);
    type->size = K_SIZE(void *);
    type->ref.proto = func;

    instance->types_map.Set(type->name, type);

    return WrapType(env, type);
}

static Napi::Value CreateEnumType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 or 2 arguments, got %1", info.Length());
        return env.Null();
    }

    bool named = info.Length() > 1;

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
    Napi::Array keys = GetOwnPropertyNames(env, obj);

    TypeInfo *type = instance->types.AppendDefault();
    K_DEFER_N(err_guard) { instance->types.RemoveLast(1); };

    type->name = named ? DuplicateString(name.Utf8Value().c_str(), &instance->str_alloc).ptr
                       : Fmt(&instance->str_alloc, "<anonymous_%1>", instance->types.count).ptr;

    Napi::Object values = Napi::Object::New(env);

    // Determine needed storage type
    {
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
    }

    // If the insert succeeds, we cannot fail anymore
    if (named && !MapType(env, instance, type, type->name))
        return env.Null();
    err_guard.Disable();

    Napi::Object defn = WrapType(env, type, false);

    defn.Set("values", values);
    defn.Freeze();

    return defn;
}

static Napi::Value CreateTypeAlias(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

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

    const TypeInfo *type = ResolveType(info[1]);
    if (!type)
        return env.Null();

    // Alias the type
    if (!MapType(env, instance, type, alias))
        return env.Null();

    return WrapType(env, type);
}

static Napi::Value GetTypeSize(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }

    const TypeInfo *type = ResolveType(info[0]);
    if (!type)
        return env.Null();

    return NewInt(env, type->size);
}

static Napi::Value GetTypeAlign(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }

    const TypeInfo *type = ResolveType(info[0]);
    if (!type)
        return env.Null();

    return NewInt(env, type->align);
}

static Napi::Value GetMemberOffset(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 2) {
        ThrowError<Napi::TypeError>(env, "Expected 2 arguments, got %1", info.Length());
        return env.Null();
    }
    if (!info[1].IsString()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for member, expected string", GetValueType(instance, info[1]));
        return env.Null();
    }

    const TypeInfo *type = ResolveType(info[0]);
    if (!type)
        return env.Null();
    if (type->primitive != PrimitiveKind::Record) {
        ThrowError<Napi::TypeError>(env, "The offsetof() function can only be used with record types");
        return env.Null();
    }

    std::string name = info[1].As<Napi::String>();

    const RecordMember *member = std::find_if(type->members.begin(), type->members.end(),
        [&](const RecordMember &member) { return TestStr(member.name, name.c_str()); });
    if (member == type->members.end()) {
        ThrowError<Napi::Error>(env, "Record type %1 does not have member '%2'", type->name, name.c_str());
        return env.Null();
    }

    return NewInt(env, member->offset);
}

static Napi::Value GetResolvedType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }

    const TypeInfo *type = ResolveType(info[0]);
    if (!type)
        return env.Null();

    return WrapType(env, type);
}

InstanceMemory *AllocateMemory(InstanceData *instance, Size stack_size, Size heap_size)
{
    std::lock_guard<std::mutex> lock(instance->mem_mutex);

    for (Size i = 1; i < instance->memories.len; i++) {
        InstanceMemory *mem = instance->memories[i];

        if (!mem->busy) {
            mem->busy = true;
            return mem;
        }
    }

    bool temporary = (instance->memories.len > instance->config.resident_async_pools);

    if (temporary && instance->temporaries > instance->config.max_temporaries)
        return nullptr;

    InstanceMemory *mem = new InstanceMemory();
    K_DEFER_N(mem_guard) { delete mem; };

    stack_size = AlignLen(stack_size, Kibibytes(64));

#if defined(_WIN32)
    // Allocate stack memory
    mem->stack.ptr = (uint8_t *)VirtualAlloc(nullptr, stack_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    mem->stack.end = mem->stack.ptr + stack_size;

    K_CRITICAL(mem->stack.ptr, "Failed to allocate %1 of memory", stack_size);
#else
    mem->stack.ptr = (uint8_t *)mmap(nullptr, stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_STACK, -1, 0);
    mem->stack.end = mem->stack.ptr + stack_size;

    K_CRITICAL(mem->stack.ptr, "Failed to allocate %1 of memory", stack_size);
#endif

#if defined(__OpenBSD__)
    // Make sure the SP points inside the MAP_STACK area, or (void) functions may crash on OpenBSD i386
    mem->stack.end -= 16;
#endif

    // Keep real stack limits intact, in case we need them
    mem->stack0 = mem->stack;

#if defined(_WIN32) && !defined(_WIN64)
    mem->stack.end -= K_SIZE(SehFrame);

    // Prepare at the top SEH frame record
    {
        SehFrame *seh = (SehFrame *)mem->stack.end;

        seh->Next = (void *)-1;
        seh->Handler = (void *)SehHandler;
    }
#endif

#if defined(_WIN32)
    mem->heap.ptr = (uint8_t *)VirtualAlloc(nullptr, heap_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    mem->heap.end = mem->heap.ptr + heap_size;

    K_CRITICAL(mem->heap.ptr, "Failed to allocate %1 of memory", heap_size);
#else
    mem->heap.ptr = (uint8_t *)mmap(nullptr, heap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    mem->heap.end = mem->heap.ptr + heap_size;

    K_CRITICAL(mem->heap.ptr, "Failed to allocate %1 of memory", heap_size);
#endif

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

void ReleaseMemory(InstanceData *instance, InstanceMemory *mem)
{
    std::lock_guard<std::mutex> lock(instance->mem_mutex);

    if (mem->temporary) {
        instance->temporaries--;
        delete mem;
    } else {
        mem->busy = false;
    }
}

static napi_value TranslateZeroCallNode(napi_env env, napi_callback_info info)
{
    struct CallbackBundle {
        napi_env env;
        void *data;
    };

    CallbackBundle **ptr = (CallbackBundle **)((uint8_t *)info + K_SIZE(void *));
    FunctionInfo *func = (FunctionInfo *)(*ptr)->data;

    InstanceData *instance = func->instance;
    InstanceMemory *mem = instance->memories[0];
    CallData call(env, instance, mem, func->native);

    K_DEFER_C(prev_call = instance->sync_call) { instance->sync_call = prev_call; };
    instance->sync_call = &call;

    napi_value ret = call.Run(func, nullptr);
    call.FinalizeFast();

    return ret;
}

static napi_value TranslateZeroCallBun(napi_env env, napi_callback_info info)
{
    FunctionInfo *func = *(FunctionInfo **)((uint8_t *)info + K_SIZE(void *));

    InstanceData *instance = func->instance;
    InstanceMemory *mem = instance->memories[0];
    CallData call(env, instance, mem, func->native);

    K_DEFER_C(prev_call = instance->sync_call) { instance->sync_call = prev_call; };
    instance->sync_call = &call;

    napi_value ret = call.Run(func, nullptr);
    call.FinalizeFast();

    return ret;
}

napi_value TranslateFastCall(napi_env env, napi_callback_info info)
{
    napi_value args[6];
    size_t count = 6;
    FunctionInfo *func;

    napi_status status = napi_get_cb_info(env, info, &count, args, nullptr, (void **)&func);
    K_ASSERT(status == napi_ok);

    InstanceData *instance = func->instance;

    if (count < (size_t)func->required_parameters) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected %1 arguments, got %2", func->parameters.len, count);
        return Napi::Env(env).Null();
    }

    InstanceMemory *mem = instance->memories[0];
    CallData call(env, instance, mem, func->native);

    K_DEFER_C(prev_call = instance->sync_call) { instance->sync_call = prev_call; };
    instance->sync_call = &call;

    napi_value ret = call.Run(func, args);
    call.FinalizeFast();

    return ret;
}

static FORCE_INLINE napi_value TranslateNormalCall(napi_env env, const FunctionInfo *func, void *native, napi_value *args, Size count)
{
    static_assert(MaxParameters >= 6);

    InstanceData *instance = func->instance;

    if (count < func->required_parameters) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected %1 arguments, got %2", func->parameters.len, count);
        return Napi::Env(env).Null();
    }

    InstanceMemory *mem = instance->memories[0];
    CallData call(env, instance, mem, native);

    K_DEFER_C(prev_call = instance->sync_call) { instance->sync_call = prev_call; };
    instance->sync_call = &call;

    napi_value ret = call.Run(func, args);
    call.Finalize();

    return ret;
}

napi_value TranslateNormalCall(napi_env env, napi_callback_info info)
{
    napi_value args[MaxParameters];
    size_t count = 6;
    FunctionInfo *func;

    napi_status status = napi_get_cb_info(env, info, &count, args, nullptr, (void **)&func);
    K_ASSERT(status == napi_ok);

    if (count > 6) {
        napi_status status = napi_get_cb_info(env, info, &count, args, nullptr, nullptr);
        K_ASSERT(status == napi_ok);

        count = std::min(count, (size_t)MaxParameters);
    }

    return TranslateNormalCall(env, func, func->native, args, (Size)count);
}

static napi_value TranslateVariadicCall(napi_env env, const FunctionInfo *func, void *native, napi_value *args, Size count)
{
    static_assert(MaxParameters >= 6);

    InstanceData *instance = func->instance;

    FunctionInfo *variadic = nullptr;
    K_DEFER_N(err_guard) { delete variadic; };

    // Try cached function
    {
        FunctionInfo *prev = instance->variadic_func;

        if (prev && prev->native == native) {
            Size specified = (count - prev->required_parameters);
            Size processed = (prev->parameters.len - prev->required_parameters) * 2;

            if (specified == processed) {
                bool match = true;

                for (Size i = prev->required_parameters, j = prev->required_parameters; i < (Size)count; i += 2, j++) {
                    int directions;
                    const TypeInfo *type = ResolveType(Napi::Value(env, args[i]), &directions);

                    if (type != prev->parameters[j].type || directions != prev->parameters[j].directions) [[unlikely]] {
                        match = false;
                        break;
                    }
                }

                if (match) [[likely]] {
                    variadic = prev;

                    // If an error happens it'll get destroyed, so don't keep it around
                    instance->variadic_func = nullptr;
                }
            }
        }
    }

    if (!variadic) {
        variadic = new FunctionInfo();

        memcpy((void *)variadic, func, K_SIZE(*func));
        memset((void *)&variadic->parameters, 0, K_SIZE(variadic->parameters));
        memset((void *)&variadic->sync, 0, K_SIZE(variadic->sync));
        memset((void *)&variadic->async, 0, K_SIZE(variadic->async));

        variadic->parameters = func->parameters;
        variadic->lib = nullptr;

        if (count < variadic->required_parameters) [[unlikely]] {
            ThrowError<Napi::TypeError>(env, "Expected %1 arguments or more, got %2", variadic->parameters.len, count);
            return Napi::Env(env).Null();
        }
        if ((count - variadic->required_parameters) % 2) [[unlikely]] {
            ThrowError<Napi::Error>(env, "Missing value argument for variadic call");
            return Napi::Env(env).Null();
        }

        for (Size i = variadic->required_parameters; i < count; i += 2) {
            ParameterInfo param = {};

            param.type = ResolveType(Napi::Value(env, args[i]), &param.directions);

            if (!param.type) [[unlikely]]
                return Napi::Env(env).Null();
            if (!CanPassType(param.type, param.directions)) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Type %1 cannot be used as a parameter", param.type->name);
                return Napi::Env(env).Null();
            }
            if (variadic->parameters.len >= MaxParameters) [[unlikely]] {
                ThrowError<Napi::TypeError>(env, "Functions cannot have more than %1 parameters", MaxParameters);
                return Napi::Env(env).Null();
            }

            param.variadic = true;
            param.offset = (int8_t)(i + 1);

            variadic->parameters.Append(param);
        }

        if (!AnalyseFunction(env, instance, variadic)) [[unlikely]]
            return Napi::Env(env).Null();
    }

    InstanceMemory *mem = instance->memories[0];
    CallData call(env, instance, mem, native);

    K_DEFER_C(prev_call = instance->sync_call) { instance->sync_call = prev_call; };
    instance->sync_call = &call;

    napi_value ret = call.Run(variadic, args);
    call.Finalize();

    if (variadic != instance->variadic_func) {
        err_guard.Disable();

        delete instance->variadic_func;
        instance->variadic_func = variadic;
    }

    return ret;
}

napi_value TranslateVariadicCall(napi_env env, napi_callback_info info)
{
    napi_value args[MaxParameters];
    size_t count = 6;
    FunctionInfo *func;

    napi_status status = napi_get_cb_info(env, info, &count, args, nullptr, (void **)&func);
    K_ASSERT(status == napi_ok);

    if (count > 6) {
        napi_status status = napi_get_cb_info(env, info, &count, args, nullptr, nullptr);
        K_ASSERT(status == napi_ok);

        count = std::min(count, (size_t)MaxParameters);
    }

    return TranslateVariadicCall(env, func, func->native, args, (Size)count);
}

class AsyncCall: public Napi::AsyncWorker {
    Napi::Env env;

    const FunctionInfo *func;
    NoDestroy<CallData> call;

    bool prepared = false;

public:
    AsyncCall(Napi::Env env, InstanceData *instance, InstanceMemory *mem, const FunctionInfo *func, Napi::Function &callback)
        : Napi::AsyncWorker(callback), env(env), func(func->Ref()), call(env, instance, mem, func->native) {}
    ~AsyncCall();

    bool Prepare(napi_value *args) {
        prepared = call->PrepareAsync(func, args);

        if (!prepared) [[unlikely]] {
            Napi::Error err = env.GetAndClearPendingException();
            SetError(err.Message());
        }

        return prepared;
    }

    void Execute() override;
    void OnOK() override;
    void OnError(const Napi::Error& err) override;
};

AsyncCall::~AsyncCall()
{
#if defined(K_DEBUG)
    call->~CallData();
#endif

    ReleaseMemory(call->instance, call->mem);
}

void AsyncCall::Execute()
{
    if (prepared) [[likely]] {
        call->ExecuteAsync();
    }
}

void AsyncCall::OnOK()
{
    K_ASSERT(prepared);

    Napi::FunctionReference &callback = Callback();

    napi_value ret = call->EndAsync();
    call->Finalize();

    napi_value self = env.Null();
    napi_value args[] = { env.Null(), ret };

    callback.Call(self, K_LEN(args), args);
}

void AsyncCall::OnError(const Napi::Error& err)
{
    Napi::FunctionReference &callback = Callback();

    call->Finalize();

    napi_value self = env.Null();
    napi_value args[] = { err.Value(), env.Undefined() };

    callback.Call(self, K_LEN(args), args);
}

napi_value TranslateAsyncCall(napi_env env, napi_callback_info info)
{
    static_assert(MaxParameters >= 6);

    napi_value args[MaxParameters];
    size_t count = 6;
    FunctionInfo *func;

    napi_status status = napi_get_cb_info(env, info, &count, args, nullptr, (void **)&func);
    K_ASSERT(status == napi_ok);

    if (count > 6) {
        napi_status status = napi_get_cb_info(env, info, &count, args, nullptr, nullptr);
        K_ASSERT(status == napi_ok);

        count = std::min(count, (size_t)MaxParameters);
    }

    InstanceData *instance = func->instance;

    if (count <= (size_t)func->required_parameters) {
        ThrowError<Napi::TypeError>(env, "Expected %1 arguments, got %2", func->required_parameters + 1, count);
        return Napi::Env(env).Null();
    }

    Napi::Function callback = Napi::Value(env, args[func->required_parameters]).As<Napi::Function>();

    if (!callback.IsFunction()) {
        ThrowError<Napi::TypeError>(env, "Expected callback function as last argument, got %1", GetValueType(instance, callback));
        return Napi::Env(env).Null();
    }

    InstanceMemory *mem = AllocateMemory(instance, instance->config.async_stack_size, instance->config.async_heap_size);
    if (!mem) [[unlikely]] {
        ThrowError<Napi::Error>(env, "Too many asynchronous calls are running");
        return Napi::Env(env).Null();
    }
    AsyncCall *async = new AsyncCall(env, instance, mem, func, callback);

    async->Prepare(args);
    async->Queue();

    return Napi::Env(env).Undefined();
}

static FORCE_INLINE bool CheckTrampolineStatus(TrampolineInfo *trampoline)
{
    if (trampoline->state == 1) [[likely]]
        return true;

    Napi::Env env = trampoline->func.Env();

    if (!trampoline->state) {
        ThrowError<Napi::Error>(env, "Cannot use non-registered callback beyond FFI call");
        trampoline->state = -1;
    }

    // We use trampoline->state < 0 as a signal than an exception has occured, because we want
    // to avoid calling IsExceptionPending() in the happy path. But trampolines get reused, so
    // there might not be an exception anymore!
    if (!env.IsExceptionPending()) {
        trampoline->state = 1;
        return true;
    }

    return false;
}

extern "C" void RelayCallback(Size idx, uint8_t *sp)
{
    TrampolineInfo *trampoline = &shared.trampolines[idx];
    InstanceData *instance = trampoline->instance;

    // Fast path: main thread and we are running a native call through Koffi.
    // But this means we are running on the custom Koffi stack, which could trip up
    // Node and V8, so we need to stwich back to the normal/main stack.
    if (InstanceMemory *mem0 = instance->memories[0]; sp >= mem0->stack0.ptr && sp <= mem0->stack0.end) {
        CallData *call = instance->sync_call;

        SwitchAndRelay(call, idx, sp, call->saved_sp, &call->mem->stack);
        return;
    }

    if (!CheckTrampolineStatus(trampoline))
        return;

    // Otherwise, we need to allocate memory to perform the callback.
    // Since the necessary machinery live in CallData, just use a temporary instance.
    // In some cases we would reuse the existing call, but it is rare so let's ignore
    // this for simplicity.

    Napi::Env env = trampoline->func.Env();

    InstanceMemory *mem = AllocateMemory(instance, instance->config.async_stack_size, instance->config.async_heap_size);
    if (!mem) [[unlikely]] {
        ThrowError<Napi::Error>(env, "Too many asynchronous calls are running");
        return;
    }
    K_DEFER { ReleaseMemory(instance, mem); };

    if (std::this_thread::get_id() == instance->main_thread_id) {
        CallData call(env, instance, mem, nullptr);
        K_DEFER { call.Finalize(); };

        napi_handle_scope scope;
        napi_open_handle_scope(env, &scope);
        K_DEFER { napi_close_handle_scope(env, scope); };

        call.Relay(idx, sp);
    } else {
        CallData call(env, instance, mem, nullptr);
        call.RelayAsync(idx, sp);
    }
}

extern "C" void RelayDirect(CallData *call, Size idx, uint8_t *sp)
{
    TrampolineInfo *trampoline = &shared.trampolines[idx];

#if defined(_WIN32)
    TEB *teb = GetTEB();
    InstanceData *instance = trampoline->instance;

    // Restore previous stack limits at the end
    K_DEFER_C(base = teb->StackBase,
              limit = teb->StackLimit,
              dealloc = teb->DeallocationStack) {
        teb->StackBase = base;
        teb->StackLimit = limit;
        teb->DeallocationStack = dealloc;
    };

    // Adjust stack limits so SEH works correctly
    teb->StackBase = instance->main_stack_max;
    teb->StackLimit = instance->main_stack_min;
    teb->DeallocationStack = instance->main_stack_min;
#endif

    if (!CheckTrampolineStatus(trampoline))
        return;

    // Relay the call
    {
        napi_handle_scope scope;
        napi_open_handle_scope(call->env, &scope);
        K_DEFER { napi_close_handle_scope(call->env, scope); };

        call->Relay(idx, sp);
    }
}

static Napi::Value FindLibraryFunction(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();
    LibraryHolder *lib = (LibraryHolder *)info.Data();

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
        if (!ParsePrototype(env, proto.c_str(), true, func))
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

    if (!AnalyseFunction(env, instance, func))
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

    return WrapFunction(env, func);
}

static Napi::Value FindSymbol(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();
    LibraryHolder *lib = (LibraryHolder *)info.Data();

    if (info.Length() < 2) {
        ThrowError<Napi::TypeError>(env, "Expected 2, got %1", info.Length());
        return env.Null();
    }
     if (!info[0].IsString()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for name, expected string", GetValueType(instance, info[0]));
        return env.Null();
    }

    std::string name = info[0].As<Napi::String>();

    const TypeInfo *type = ResolveType(info[1]);
    if (!type)
        return env.Null();

#if defined(_WIN32)
    void *ptr = (void *)GetProcAddress((HMODULE)lib->module, name.c_str());
#else
    void *ptr = (void *)dlsym(lib->module, name.c_str());
#endif
    if (!ptr) {
        ThrowError<Napi::Error>(env, "Cannot find symbol '%1' in shared library", name.c_str());
        return env.Null();
    }

    return WrapPointer(env, type, ptr);
}

static Napi::Value UnloadLibrary(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    LibraryHolder *lib = (LibraryHolder *)info.Data();

    lib->Unload();

    return env.Undefined();
}

static Napi::Value LoadSharedLibrary(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 or 2 arguments, got %1", info.Length());
        return env.Null();
    }
    if (!info[0].IsString() && !IsNullOrUndefined(env, info[0])) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for filename, expected string or null", GetValueType(instance, info[0]));
        return env.Null();
    }
    if (info.Length() >= 2 && !IsObject(env, info[1])) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for options, expected object", GetValueType(instance, info[1]));
        return env.Null();
    }

#if !defined(_WIN32)
    int flags = 0;

    if (info.Length() >= 2) {
        Napi::Object options = info[1].As<Napi::Object>();

        flags |= options.Get("lazy").ToBoolean() ? RTLD_LAZY : RTLD_NOW;
        flags |= options.Get("global").ToBoolean() ? RTLD_GLOBAL : RTLD_LOCAL;
#if defined(RTLD_DEEPBIND)
        flags |= options.Get("deep").ToBoolean() ? RTLD_DEEPBIND : 0;
#endif
    } else {
        flags = RTLD_NOW | RTLD_LOCAL;
    }
#endif

    if (!instance->memories.len) {
        AllocateMemory(instance, instance->config.sync_stack_size, instance->config.sync_heap_size);
        K_ASSERT(instance->memories.len == 1);
    }

    // Load shared library
    void *module = nullptr;
#if defined(_WIN32)
    if (info[0].IsString()) {
        std::string filename = info[0].As<Napi::String>();
        module = LoadWindowsLibrary(env, filename.c_str());

        if (!module)
            return env.Null();
    } else {
        module = GetModuleHandle(nullptr);
        K_ASSERT(module);
    }
#else
    if (info[0].IsString()) {
        std::string filename = info[0].As<Napi::String>();
        module = dlopen(filename.c_str(), flags);

        if (!module) {
            const char *msg = dlerror();

            if (StartsWith(msg, filename.c_str())) {
                msg += filename.length();

                while (strchr(": ", msg[0]) && msg[0]) {
                    msg++;
                }
            }

            ThrowError<Napi::Error>(env, "Failed to load shared library: %1", msg);
            return env.Null();
        }
    } else {
        module = RTLD_DEFAULT;
    }
#endif

    LibraryHolder *lib = new LibraryHolder(module);
    K_DEFER { lib->Unref(); };

    Napi::Object obj = Napi::Object::New(env);

#define ADD_METHOD(Name, Call) \
        do { \
            const auto wrapper = [](const Napi::CallbackInfo &info) { return Call; }; \
            Napi::Function func = Napi::Function::New(env, wrapper, (Name), (void *)lib->Ref()); \
            func.AddFinalizer([](Napi::Env, LibraryHolder *lib) { lib->Unref(); }, lib); \
            obj.Set((Name), func); \
        } while (false)

    ADD_METHOD("func", FindLibraryFunction(info));
    ADD_METHOD("symbol", FindSymbol(info));

    // We can't unref the library after unload, obviously
    obj.Set("unload", Napi::Function::New(env, UnloadLibrary, "unload", (void *)lib->Ref()));

#undef ADD_METHOD

    return obj;
}

static Napi::Value RegisterCallback(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (!InitAsyncBroker(env, instance)) [[unlikely]]
        return env.Null();

    bool has_recv = (info.Length() >= 3 && info[1].IsFunction());

    if (info.Length() < 2u + has_recv) {
        ThrowError<Napi::TypeError>(env, "Expected 2 or 3 arguments, got %1", info.Length());
        return env.Null();
    }
    if (!info[0u + has_recv].IsFunction()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for func, expected function", GetValueType(instance, info[0 + has_recv]));
        return env.Null();
    }

    Napi::Value recv = has_recv ? info[0] : env.Undefined();
    Napi::Function func = info[0u + has_recv].As<Napi::Function>();

    const TypeInfo *type = ResolveType(info[1u + has_recv]);
    if (!type)
        return env.Null();
    if (type->primitive != PrimitiveKind::Callback) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 type, expected <callback> * type", type->name);
        return env.Null();
    }

    int16_t idx;
    {
        std::lock_guard<std::mutex> lock(shared.mutex);

        if (!shared.available.len) [[unlikely]] {
            ThrowError<Napi::Error>(env, "Too many callbacks are in use (max = %1)", MaxTrampolines);
            return env.Null();
        }

        idx = shared.available.data[--shared.available.len];
    }

    TrampolineInfo *trampoline = &shared.trampolines[idx];

    trampoline->state = 1;
    trampoline->instance = instance;
    trampoline->proto = type->ref.proto;
    trampoline->func.Reset(func, 1);
    if (!IsNullOrUndefined(env, recv)) {
        trampoline->recv.Reset(recv, 1);
    } else {
        trampoline->recv.Reset();
    }

    void *ptr = GetTrampoline(idx);

    // Cache index for fast unregistration
    instance->trampolines_map.Set(ptr, idx);

    return WrapCallback(env, type->ref.type, ptr);
}

static Napi::Value UnregisterCallback(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }

    void *ptr;
    if (!TryPointer(env, info[0], &ptr)) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for id, expected registered callback", GetValueType(instance, info[0]));
        return env.Null();
    }

    int16_t idx;
    {
        int16_t *it = instance->trampolines_map.Find(ptr);

        if (!it) [[unlikely]] {
            ThrowError<Napi::Error>(env, "Could not find matching registered callback");
            return env.Null();
        }

        idx = *it;
        instance->trampolines_map.Remove(it);
    }

    // Release shared trampoline safely
    {
        std::lock_guard<std::mutex> lock(shared.mutex);

        TrampolineInfo *trampoline = &shared.trampolines[idx];
        K_ASSERT(!trampoline->func.IsEmpty());

        trampoline->state = 0;
        trampoline->func.Reset();
        trampoline->recv.Reset();

        shared.available.Append(idx);
    }

    return env.Undefined();
}

static Napi::Value CastValue(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 2) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 2 arguments, got %1", info.Length());
        return env.Null();
    }

    Napi::Value value = info[0];

    const TypeInfo *type = ResolveType(info[1]);
    if (!type) [[unlikely]]
        return env.Null();
    if (type->primitive != PrimitiveKind::Pointer &&
            type->primitive != PrimitiveKind::Callback &&
            type->primitive != PrimitiveKind::String &&
            type->primitive != PrimitiveKind::String16 &&
            type->primitive != PrimitiveKind::String32) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Only pointer or string types can be used for casting");
        return env.Null();
    }

    ValueCast *cast = new ValueCast();

    cast->env = env;
    napi_create_reference(env, value, 1, &cast->ref);
    cast->type = type;

    Napi::External<ValueCast> external = Napi::External<ValueCast>::New(env, cast, [](Napi::Env, ValueCast *cast) { delete cast; });
    SetValueTag(env, external, &CastMarker);

    return external;
}

static Napi::Value DecodeValue(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    bool has_offset = (info.Length() >= 2 && info[1].IsNumber());
    bool has_len = (info.Length() >= 3u + has_offset && info[2 + has_offset].IsNumber());

    if (info.Length() < 2u + has_offset) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected %1 to 4 arguments, got %2", 2 + has_offset, info.Length());
        return env.Null();
    }

    const TypeInfo *type = ResolveType(info[1u + has_offset]);
    if (!type) [[unlikely]]
        return env.Null();

    Napi::Value value = info[0];
    int64_t offset = has_offset ? info[1].As<Napi::Number>().Int64Value() : 0;

    if (has_len) {
        Size len = info[2 + has_offset].As<Napi::Number>();

        Napi::Value ret = Decode(value, (Size)offset, type, &len);
        return ret;
    } else {
        Napi::Value ret = Decode(value, (Size)offset, type);
        return ret;
    }
}

static Napi::Value GetPointerAddress(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }

    void *ptr = nullptr;
    if (!GetExternalPointer(env, info[0], &ptr))
        return env.Null();

    uint64_t ptr64 = (uint64_t)(uintptr_t)ptr;
    Napi::BigInt bigint = Napi::BigInt::New(env, ptr64);

    return bigint;
}

static Napi::Value CallPointerSync(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 2) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 2 or more arguments, got %1", info.Length());
        return env.Null();
    }

    void *ptr = nullptr;
    if (!GetExternalPointer(env, info[0], &ptr)) [[unlikely]]
        return env.Null();

    const TypeInfo *type = ResolveType(info[1]);
    if (!type) [[unlikely]]
        return env.Null();
    if (type->primitive != PrimitiveKind::Prototype) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for type, expected function type", GetValueType(instance, info[1]));
        return env.Null();
    }

    const FunctionInfo *proto = type->ref.proto;

    napi_value ret = proto->variadic ? TranslateVariadicCall(env, proto, ptr, info.First() + 2, info.Length() - 2)
                                     : TranslateNormalCall(env, proto, ptr, info.First() + 2, info.Length() - 2);

    return Napi::Value(env, ret);
}

static Napi::Value EncodeValue(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    bool has_offset = (info.Length() >= 2 && info[1].IsNumber());
    bool has_len = (info.Length() >= 4u + has_offset && info[3 + has_offset].IsNumber());

    if (info.Length() < 3u + has_offset) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected %1 to 5 arguments, got %2", 3 + has_offset, info.Length());
        return env.Null();
    }

    const TypeInfo *type = ResolveType(info[1u + has_offset]);
    if (!type) [[unlikely]]
        return env.Null();

    Napi::Value ref = info[0];
    int64_t offset = has_offset ? info[1].As<Napi::Number>().Int64Value() : 0;
    Napi::Value value = info[2 + has_offset];

    if (has_len) {
        Size len = info[3 + has_offset].As<Napi::Number>();

        if (!Encode(ref, (Size)offset, value, type, &len))
            return env.Null();
    } else {
        if (!Encode(ref, (Size)offset, value, type))
            return env.Null();
    }

    return env.Undefined();
}

static Napi::Value CreateView(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 2 arguments, got %1", info.Length());
        return env.Null();
    }
    if (!info[1].IsNumber()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for length, expected integer", GetValueType(instance, info[1]));
        return env.Null();
    }

    void *ptr = nullptr;
    if (!GetExternalPointer(env, info[0], &ptr))
        return env.Null();
    Size len = (Size)info[1].As<Napi::Number>().Int64Value();

    if (len < 0) {
        ThrowError<Napi::TypeError>(env, "Array length must be positive and non-zero");
        return env.Null();
    }

    if (len) {
        Napi::ArrayBuffer view = Napi::ArrayBuffer::New(env, ptr, (size_t)len);

        if (!view.ByteLength()) {
            ThrowError<Napi::Error>(env, "This runtime does not support external buffers");
            return env.Null();
        }

        return view;
    } else {
        return Napi::ArrayBuffer::New(env, 0);
    }
}

static Napi::Value ResetKoffi(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (instance->broker) {
        napi_release_threadsafe_function(instance->broker, napi_tsfn_abort);
        instance->broker = nullptr;
    }

    instance->types.RemoveFrom(instance->base_types_count);

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

    return env.Undefined();
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

const LibraryHolder *LibraryHolder::Ref() const
{
    refcount++;
    return this;
}

void LibraryHolder::Unref() const
{
    if (!--refcount) {
        delete this;
    }
}

ValueCast::~ValueCast()
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

FunctionInfo::~FunctionInfo()
{
    if (lib) {
        lib->Unref();
    }
}

const FunctionInfo *FunctionInfo::Ref() const
{
    refcount++;
    return this;
}

void FunctionInfo::Unref() const
{
    if (!--refcount) {
        delete this;
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

bool InitAsyncBroker(Napi::Env env, InstanceData *instance)
{
    if (!instance->broker) {
        if (napi_create_threadsafe_function(env, nullptr, nullptr,
                                            Napi::String::New(env, "Koffi Async Callback Broker"),
                                            0, 1, nullptr, nullptr, nullptr,
                                            PerformAsyncRelay, &instance->broker) != napi_ok) {
            LogError("Failed to create async callback broker");
            return false;
        }
        napi_unref_threadsafe_function(env, instance->broker);
    }

    return true;
}

static void RegisterPrimitiveType(Napi::Env env, Napi::Object map, std::initializer_list<const char *> names,
                                  PrimitiveKind primitive, int32_t size, int16_t align, const char *ref = nullptr)
{
    K_ASSERT(names.size() > 0);
    K_ASSERT(align <= size);

    InstanceData *instance = env.GetInstanceData<InstanceData>();

    TypeInfo *type = instance->types.AppendDefault();

    type->name = *names.begin();

    type->primitive = primitive;
    type->size = size;
    type->align = align;

    if (IsInteger(type) || IsFloat(type)) {
        type->flags |= (int)TypeFlag::HasTypedArray;
    }
    if (TestStr(type->name, "char") ||
            TestStr(type->name, "char16") || TestStr(type->name, "char16_t") ||
            TestStr(type->name, "char32") || TestStr(type->name, "char32_t") ||
            TestStr(type->name, "wchar") || TestStr(type->name, "wchar_t")) {
        type->flags |= (int)TypeFlag::IsCharLike;
    }

    if (ref) {
        type->ref.type = instance->types_map.FindValue(ref, nullptr);
        K_ASSERT(type->ref.type);
    }

    Napi::Value wrapper = WrapType(env, type);

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

static bool DetectBun(Napi::Env env)
{
    Napi::Value ret = env.RunScript("process.isBun");
    Napi::Boolean b = ret.ToBoolean();

    return b.Value();
}

static void PickTranslateZeroCallVariant(Napi::Env env)
{
    static int BunTrap = 42;

    Napi::Object self = Napi::Object::New(env);

    napi_status status;
    napi_value func;
    napi_value ret;

    status = napi_create_function(env, nullptr, 0, [](napi_env env, napi_callback_info info) {
        napi_value self;
        napi_get_cb_info(env, info, nullptr, nullptr, &self, nullptr);

        void **ptr1 = (void **)info;
        napi_value *ptr2 = (napi_value *)info;

        if (ptr1[0] == &BunTrap || ptr1[1] == &BunTrap) {
            translate_zero_call = TranslateZeroCallBun;
        } else if (ptr2[0] != self && ptr2[1] != self) {
            translate_zero_call = TranslateZeroCallNode;
        } else {
            translate_zero_call = TranslateFastCall;
        }

        return self;
    }, &BunTrap, &func);
    K_ASSERT(status == napi_ok);

    status = napi_call_function(env, self, func, 0, nullptr, &ret);
    K_ASSERT(status == napi_ok);
}

static bool CanReferencePrimitiveValues(napi_env env)
{
    uint32_t version = 0;
    napi_get_version(env, &version);

    return version >= 10;
}

static bool CanDeleteReferenceInFinalizer(napi_env env)
{
    const napi_node_version *version = nullptr;
    napi_get_node_version(env, &version);

    if (version->major >= 24)
        return true;

    // Made by looking at the git history of each release branch
    if (version->major == 23 && version->minor >= 5)
        return true;
    if (version->major == 22 && version->minor >= 13)
        return true;
    if (version->major == 20 && version->minor >= 19)
        return true;
    if (version->major == 20 && version->minor == 18 && version->patch >= 3)
        return true;

    return false;
}

static Napi::Object InitModule(Napi::Env env, Napi::Object exports)
{
    // Load recent N-API functions (version >= 9) functions dynamically
    {
        static std::once_flag flag;

        std::call_once(flag, [&]() {
            PickTranslateZeroCallVariant(env);

            if (DetectBun(env))
                return;

            uv_lib_t lib = {};

#if defined(_WIN32)
            lib.handle = GetModuleHandle(nullptr);
#else
            uv_dlopen(nullptr, &lib);
            K_DEFER { uv_dlclose(&lib); };
#endif

            if (CanReferencePrimitiveValues(env)) {
                // We can't use optimized property keys in older versions because we need to create
                // references to them, but napi_create_reference() was not usable with primitive values.
                uv_dlsym(&lib, "node_api_create_property_key_utf8", (void **)&node_api_create_property_key_utf8);
            }

            if (!CanDeleteReferenceInFinalizer(env)) {
                // napi_delete_reference cannot be safely used in older Node versions because it
                // errors out (or even asserts) if it gets called in a finalizer. In this case,
                // use experimental API to try to run it later.
                uv_dlsym(&lib, "node_api_post_finalizer", (void **)&node_api_post_finalizer);
            }
        });
    }

    InstanceData *instance = new InstanceData();
    K_CRITICAL(instance, "Failed to initialize Koffi");

    instance->env = env;
    env.SetInstanceData(instance);

#if defined(__clang__)
    // First call to napi_create_double() does some weird stuff I can't explain
    // in clang-cl builds. I don't like this... but this fixes it, somehow ><
    Napi::Number::New(env, 0.0);
#endif


    exports.Set("config", Napi::Function::New(env, GetSetConfig));
    exports.Set("stats", Napi::Function::New(env, GetStats));

    exports.Set("config", Napi::Function::New(env, GetSetConfig, "config"));
    exports.Set("stats", Napi::Function::New(env, GetStats, "stats"));

    exports.Set("struct", Napi::Function::New(env, CreatePaddedStructType, "struct"));
    exports.Set("pack", Napi::Function::New(env, CreatePackedStructType, "pack"));
    exports.Set("union", Napi::Function::New(env, CreateUnionType, "union"));
    exports.Set("Union", Napi::Function::New(env, InstantiateUnion, "Union"));
    exports.Set("opaque", Napi::Function::New(env, CreateOpaqueType, "opaque"));
    exports.Set("pointer", Napi::Function::New(env, CreatePointerType, "pointer"));
    exports.Set("array", Napi::Function::New(env, CreateArrayType, "array"));
    exports.Set("proto", Napi::Function::New(env, CreateFunctionType, "proto"));
    exports.Set("alias", Napi::Function::New(env, CreateTypeAlias, "alias"));
    exports.Set("enumeration", Napi::Function::New(env, CreateEnumType, "enumeration"));

    exports.Set("sizeof", Napi::Function::New(env, GetTypeSize, "sizeof"));
    exports.Set("alignof", Napi::Function::New(env, GetTypeAlign, "alignof"));
    exports.Set("offsetof", Napi::Function::New(env, GetMemberOffset, "offsetof"));
    exports.Set("type", Napi::Function::New(env, GetResolvedType, "type"));

    exports.Set("load", Napi::Function::New(env, LoadSharedLibrary, "load"));

    exports.Set("in", Napi::Function::New(env, MarkIn, "in"));
    exports.Set("out", Napi::Function::New(env, MarkOut, "out"));
    exports.Set("inout", Napi::Function::New(env, MarkInOut, "inout"));

    exports.Set("disposable", Napi::Function::New(env, CreateDisposableType, "disposable"));
    exports.Set("alloc", Napi::Function::New(env, CallAlloc, "alloc"));
    exports.Set("free", Napi::Function::New(env, CallFree, "free"));

    exports.Set("register", Napi::Function::New(env, RegisterCallback, "register"));
    exports.Set("unregister", Napi::Function::New(env, UnregisterCallback, "unregister"));

    exports.Set("as", Napi::Function::New(env, CastValue, "as"));
    exports.Set("decode", Napi::Function::New(env, DecodeValue, "decode"));
    exports.Set("address", Napi::Function::New(env, GetPointerAddress, "address"));
    exports.Set("call", Napi::Function::New(env, CallPointerSync, "call"));
    exports.Set("encode", Napi::Function::New(env, EncodeValue, "encode"));
    exports.Set("view", Napi::Function::New(env, CreateView, "view"));

    exports.Set("reset", Napi::Function::New(env, ResetKoffi, "reset"));

    exports.Set("errno", Napi::Function::New(env, GetOrSetErrNo, "errno"));

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
    exports.Set("extension", Napi::String::New(env, ".dll"));
#elif defined(__APPLE__)
    exports.Set("extension", Napi::String::New(env, ".dylib"));
#else
    exports.Set("extension", Napi::String::New(env, ".so"));
#endif

    // Init various references
    {
        Napi::Function construct_type = TypeClass::InitClass(env);
        Napi::Symbol symbol = Napi::Symbol::New(env, "active");

        instance->construct_type.Reset(construct_type, 1);
        instance->active_symbol.Reset(symbol, 1);
    }

    // Init base types
    {
        Napi::Object types = Napi::Object::New(env);
        exports.Set("types", types);

        RegisterPrimitiveType(env, types, {"void"}, PrimitiveKind::Void, 0, 0);
        RegisterPrimitiveType(env, types, {"bool"}, PrimitiveKind::Bool, K_SIZE(bool), alignof(bool));
        RegisterPrimitiveType(env, types, {"int8_t", "int8"}, PrimitiveKind::Int8, 1, 1);
        RegisterPrimitiveType(env, types, {"uint8_t", "uint8"}, PrimitiveKind::UInt8, 1, 1);
        RegisterPrimitiveType(env, types, {"char"}, PrimitiveKind::Int8, 1, 1);
        RegisterPrimitiveType(env, types, {"unsigned char", "uchar"}, PrimitiveKind::UInt8, 1, 1);
        RegisterPrimitiveType(env, types, {"char16_t", "char16"}, PrimitiveKind::Int16, 2, 2);
        RegisterPrimitiveType(env, types, {"char32_t", "char32"}, PrimitiveKind::Int32, 4, 4);
        if (K_SIZE(wchar_t) == 2) {
            RegisterPrimitiveType(env, types, {"wchar_t", "wchar"}, PrimitiveKind::Int16, 2, 2);
        } else if (K_SIZE(wchar_t) == 4) {
            RegisterPrimitiveType(env, types, {"wchar_t", "wchar"}, PrimitiveKind::Int32, 4, 4);
        }
        RegisterPrimitiveType(env, types, {"int16_t", "int16"}, PrimitiveKind::Int16, 2, 2);
        RegisterPrimitiveType(env, types, {"int16_le_t", "int16_le"}, GetLittleEndianPrimitive(PrimitiveKind::Int16), 2, 2);
        RegisterPrimitiveType(env, types, {"int16_be_t", "int16_be"}, GetBigEndianPrimitive(PrimitiveKind::Int16), 2, 2);
        RegisterPrimitiveType(env, types, {"uint16_t", "uint16"}, PrimitiveKind::UInt16, 2, 2);
        RegisterPrimitiveType(env, types, {"uint16_le_t", "uint16_le"}, GetLittleEndianPrimitive(PrimitiveKind::UInt16), 2, 2);
        RegisterPrimitiveType(env, types, {"uint16_be_t", "uint16_be"}, GetBigEndianPrimitive(PrimitiveKind::UInt16), 2, 2);
        RegisterPrimitiveType(env, types, {"short"}, PrimitiveKind::Int16, 2, 2);
        RegisterPrimitiveType(env, types, {"unsigned short", "ushort"}, PrimitiveKind::UInt16, 2, 2);
        RegisterPrimitiveType(env, types, {"int32_t", "int32"}, PrimitiveKind::Int32, 4, 4);
        RegisterPrimitiveType(env, types, {"int32_le_t", "int32_le"}, GetLittleEndianPrimitive(PrimitiveKind::Int32), 4, 4);
        RegisterPrimitiveType(env, types, {"int32_be_t", "int32_be"}, GetBigEndianPrimitive(PrimitiveKind::Int32), 4, 4);
        RegisterPrimitiveType(env, types, {"uint32_t", "uint32"}, PrimitiveKind::UInt32, 4, 4);
        RegisterPrimitiveType(env, types, {"uint32_le_t", "uint32_le"}, GetLittleEndianPrimitive(PrimitiveKind::UInt32), 4, 4);
        RegisterPrimitiveType(env, types, {"uint32_be_t", "uint32_be"}, GetBigEndianPrimitive(PrimitiveKind::UInt32), 4, 4);
        RegisterPrimitiveType(env, types, {"int"}, PrimitiveKind::Int32, 4, 4);
        RegisterPrimitiveType(env, types, {"unsigned int", "uint"}, PrimitiveKind::UInt32, 4, 4);
        RegisterPrimitiveType(env, types, {"int64_t", "int64"}, PrimitiveKind::Int64, 8, alignof(int64_t));
        RegisterPrimitiveType(env, types, {"int64_le_t", "int64_le"}, GetLittleEndianPrimitive(PrimitiveKind::Int64), 8, alignof(int64_t));
        RegisterPrimitiveType(env, types, {"int64_be_t", "int64_be"}, GetBigEndianPrimitive(PrimitiveKind::Int64), 8, alignof(int64_t));
        RegisterPrimitiveType(env, types, {"uint64_t", "uint64"}, PrimitiveKind::UInt64, 8, alignof(int64_t));
        RegisterPrimitiveType(env, types, {"uint64_le_t", "uint64_le"}, GetLittleEndianPrimitive(PrimitiveKind::UInt64), 8, alignof(int64_t));
        RegisterPrimitiveType(env, types, {"uint64_be_t", "uint64_be"}, GetBigEndianPrimitive(PrimitiveKind::UInt64), 8, alignof(int64_t));
        RegisterPrimitiveType(env, types, {"intptr_t", "intptr"}, GetSignPrimitive(K_SIZE(intptr_t), true), K_SIZE(intptr_t), alignof(intptr_t));
        RegisterPrimitiveType(env, types, {"uintptr_t", "uintptr"}, GetSignPrimitive(K_SIZE(intptr_t), false), K_SIZE(intptr_t), alignof(intptr_t));
        RegisterPrimitiveType(env, types, {"size_t"}, GetSignPrimitive(K_SIZE(size_t), false), K_SIZE(size_t), alignof(size_t));
        RegisterPrimitiveType(env, types, {"long"}, GetSignPrimitive(K_SIZE(long), true), K_SIZE(long), alignof(long));
        RegisterPrimitiveType(env, types, {"unsigned long", "ulong"}, GetSignPrimitive(K_SIZE(long), false), K_SIZE(long), alignof(long));
        RegisterPrimitiveType(env, types, {"long long", "longlong"}, PrimitiveKind::Int64, K_SIZE(int64_t), alignof(int64_t));
        RegisterPrimitiveType(env, types, {"unsigned long long", "ulonglong"}, PrimitiveKind::UInt64, K_SIZE(uint64_t), alignof(uint64_t));
        RegisterPrimitiveType(env, types, {"float", "float32"}, PrimitiveKind::Float32, 4, alignof(float));
        RegisterPrimitiveType(env, types, {"double", "float64"}, PrimitiveKind::Float64, 8, alignof(double));
        RegisterPrimitiveType(env, types, {"char *", "str", "string"}, PrimitiveKind::String, K_SIZE(void *), alignof(void *), "char");
        RegisterPrimitiveType(env, types, {"char16_t *", "char16 *", "str16", "string16"}, PrimitiveKind::String16, K_SIZE(void *), alignof(void *), "char16_t");
        RegisterPrimitiveType(env, types, {"char32_t *", "char32 *", "str32", "string32"}, PrimitiveKind::String32, K_SIZE(void *), alignof(void *), "char32_t");
        if (K_SIZE(wchar_t) == 2) {
            RegisterPrimitiveType(env, types, {"wchar_t *", "wchar *"}, PrimitiveKind::String16, K_SIZE(void *), alignof(void *), "wchar_t");
        } else if (K_SIZE(wchar_t) == 4) {
            RegisterPrimitiveType(env, types, {"wchar_t *", "wchar *"}, PrimitiveKind::String32, K_SIZE(void *), alignof(void *), "wchar_t");
        }

        instance->void_type = instance->types_map.FindValue("void", nullptr);
        instance->char_type = instance->types_map.FindValue("char", nullptr);
        instance->char16_type = instance->types_map.FindValue("char16", nullptr);
        instance->char32_type = instance->types_map.FindValue("char32", nullptr);
        instance->str_type = instance->types_map.FindValue("char *", nullptr);
        instance->str16_type = instance->types_map.FindValue("char16_t *", nullptr);
        instance->str32_type = instance->types_map.FindValue("char32_t *", nullptr);

        instance->base_types_count = instance->types.count;
    }

    // Expose internal Node stuff
    {
        Napi::Object node = Napi::Object::New(env);
        exports.Set("node", node);

        node.Set("env", WrapPointer(env, instance->void_type, (napi_env)env));

        node.Set("poll", Napi::Function::New(env, &Poll, "poll"));
        node.Set("PollHandle", PollHandle::Define(env));
    }

    exports.Set("version", Napi::String::New(env, K_STRINGIFY(VERSION)));

#if defined(_WIN32)
    {
        TEB *teb = GetTEB();

        instance->main_stack_max = teb->StackBase;
        instance->main_stack_min = teb->DeallocationStack;
    }
#endif

    instance->translate_zero_call = translate_zero_call;
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
                trampoline->func.Reset();
                trampoline->recv.Reset();
                trampoline->state = 0;
            }
        }
    }
}

NODE_API_MODULE(koffi, InitModule);

}
