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
#include "ffi.hh"
#include "call.hh"
#include "util.hh"

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <ntsecapi.h>
#else
    #include <dlfcn.h>
    #include <unistd.h>
#endif

#include <napi.h>
#if NODE_WANT_INTERNALS
    #include <env-inl.h>
    #include <js_native_api_v8.h>
#endif

namespace RG {

// Value does not matter, the tag system uses memory addresses
static const int TypeInfoMarker = 0xDEADBEEF;

static const TypeInfo *ResolveType(const InstanceData *instance, Napi::Value value, int *out_directions = nullptr)
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

static Napi::Value CreateStructType(const Napi::CallbackInfo &info)
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
    if (!info[1].IsObject()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for members, expected object", GetValueType(instance, info[1]));
        return env.Null();
    }

    TypeInfo *type = instance->types.AppendDefault();
    RG_DEFER_N(err_guard) { instance->types.RemoveLast(1); };

    std::string name = info[0].As<Napi::String>();
    Napi::Object obj = info[1].As<Napi::Object>();
    Napi::Array keys = obj.GetPropertyNames();

    type->name = DuplicateString(name.c_str(), &instance->str_alloc).ptr;

    type->primitive = PrimitiveKind::Record;
    type->align = 1;

    for (uint32_t i = 0; i < keys.Length(); i++) {
        RecordMember member = {};

        std::string key = ((Napi::Value)keys[i]).As<Napi::String>();
        Napi::Value value = obj[key];

        member.name = DuplicateString(key.c_str(), &instance->str_alloc).ptr;
        member.type = ResolveType(instance, value);
        if (!member.type)
            return env.Null();

        type->size += member.type->size;
        type->align = std::max(type->align, member.type->align);

        type->members.Append(member);
    }

    type->size = (int16_t)AlignLen(type->size, type->align);

    // If the insert succeeds, we cannot fail anymore
    if (!instance->types_map.TrySet(type).second) {
        ThrowError<Napi::Error>(env, "Duplicate type name '%1'", type->name);
        return env.Null();
    }
    err_guard.Disable();

    Napi::External<TypeInfo> external = Napi::External<TypeInfo>::New(env, type);
    SetValueTag(instance, external, &TypeInfoMarker);

    return external;
}

static Napi::Value CreateHandleType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }
    if (!info[0].IsString()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for name, expected string", GetValueType(instance, info[0]));
        return env.Null();
    }

    TypeInfo *type = instance->types.AppendDefault();
    RG_DEFER_N(err_guard) { instance->types.RemoveLast(1); };

    std::string name = info[0].As<Napi::String>();

    type->name = DuplicateString(name.c_str(), &instance->str_alloc).ptr;

    type->primitive = PrimitiveKind::Record;
    type->align = RG_SIZE(void *);
    type->size = RG_SIZE(void *);

    // Add single handle member
    {
        RecordMember member = {};

        member.name = "ptr";
        member.type = instance->types_map.FindValue("void *", nullptr);
        RG_ASSERT(member.type);

        type->members.Append(member);
    }

    // If the insert succeeds, we cannot fail anymore
    if (!instance->types_map.TrySet(type).second) {
        ThrowError<Napi::Error>(env, "Duplicate type name '%1'", type->name);
        return env.Null();
    }
    err_guard.Disable();

    Napi::External<TypeInfo> external = Napi::External<TypeInfo>::New(env, type);
    SetValueTag(instance, external, &TypeInfoMarker);

    return external;
}

static Napi::Value CreatePointerType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }

    const TypeInfo *ref = ResolveType(instance, info[0]);
    if (!ref)
        return env.Null();

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

    Napi::External<TypeInfo> external = Napi::External<TypeInfo>::New(env, type);
    SetValueTag(instance, external, &TypeInfoMarker);

    return external;
}

static Napi::Value EncodePointerDirection(const Napi::CallbackInfo &info, int directions)
{
    RG_ASSERT(directions >= 1 && directions <= 3);

    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }

    const TypeInfo *type = ResolveType(instance, info[0]);
    if (!type)
        return env.Null();

    if (type->primitive != PrimitiveKind::Pointer) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 type, expected pointer type", PrimitiveKindNames[(int)type->primitive]);
        return env.Null();
    }
    if ((directions & 2) && type->ref->primitive != PrimitiveKind::Record) {
        ThrowError<Napi::TypeError>(env, "Only objects can be used as out parameters (for now)");
        return env.Null();
    }

    // We need to lose the const for Napi::External to work
    TypeInfo *marked = (TypeInfo *)((uint8_t *)type + directions - 1);

    Napi::External<TypeInfo> external = Napi::External<TypeInfo>::New(env, marked);
    SetValueTag(instance, external, &TypeInfoMarker);

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

static Napi::Value LoadSharedLibrary(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 2) {
        ThrowError<Napi::TypeError>(env, "Expected 2 arguments, not %1", info.Length());
        return env.Null();
    }
    if (!info[0].IsString() && !info[0].IsNull()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for filename, expected string or null", GetValueType(instance, info[0]));
        return env.Null();
    }
    if (!info[1].IsObject()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for functions, expected object", GetValueType(instance, info[1]));
        return env.Null();
    }

    // Load shared library
    void *module = nullptr;
#ifdef _WIN32
    if (info[0].IsString()) {
        std::u16string filename = info[0].As<Napi::String>();
        module = LoadLibraryW((LPCWSTR)filename.c_str());

        if (!module) {
            ThrowError<Napi::Error>(env, "Failed to load shared library: %1", GetWin32ErrorString());
            return env.Null();
        }
    } else {
        module = GetModuleHandle(nullptr);
        RG_ASSERT(module);
    }
#else
    if (info[0].IsString()) {
        std::string filename = info[0].As<Napi::String>();
        module = dlopen(filename.c_str(), RTLD_NOW);

        if (!module) {
            const char *msg = dlerror();

            if (StartsWith(msg, filename.c_str())) {
                msg += filename.length();
            }
            while (strchr(": ", msg[0])) {
                msg++;
            }

            ThrowError<Napi::Error>(env, "Failed to load shared library: %1", msg);
            return env.Null();
        }
    } else {
        module = RTLD_DEFAULT;
    }
#endif

    std::shared_ptr<LibraryHolder> lib = std::make_shared<LibraryHolder>(module);
    Napi::Object obj = Napi::Object::New(env);
    Napi::Object functions = info[1].As<Napi::Array>();
    Napi::Array keys = functions.GetPropertyNames();

    for (uint32_t i = 0; i < keys.Length(); i++) {
        FunctionInfo *func = new FunctionInfo();
        RG_DEFER_N(func_guard) { delete func; };

        std::string key = ((Napi::Value)keys[i]).As<Napi::String>();
        Napi::Array value = ((Napi::Value)functions[key]).As<Napi::Array>();

        func->name = DuplicateString(key.c_str(), &instance->str_alloc).ptr;
        func->decorated_name = func->name;
        func->lib = lib;

        if (!value.IsArray()) {
            ThrowError<Napi::TypeError>(env, "Unexpexted %1 value for signature of '%2', expected an array", GetValueType(instance, value), func->name);
            return env.Null();
        }
        if (value.Length() < 2 || value.Length() > 3) {
            ThrowError<Napi::TypeError>(env, "Unexpected array of length %1 for '%2', expected 2 or 3 elements", value.Length(), func->name);
            return env.Null();
        }

        Napi::Array parameters;

        if (((Napi::Value)value[1u]).IsString()) {
            std::string conv = ((Napi::Value)value[1u]).As<Napi::String>();

            if (conv == "cdecl" || conv == "__cdecl") {
                func->convention = CallConvention::Default;
            } else if (conv == "stdcall" || conv == "__stdcall") {
                func->convention = CallConvention::Stdcall;
            } else {
                ThrowError<Napi::Error>(env, "Unknown calling convention '%1'", conv.c_str());
                return env.Null();
            }

            if (!((Napi::Value)value[2u]).IsArray()) {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for parameters of '%2', expected an array", GetValueType(instance, (Napi::Value)value[1u]), func->name);
                return env.Null();
            }

            parameters = ((Napi::Value)value[2u]).As<Napi::Array>();
        } else {
            if (!((Napi::Value)value[1u]).IsArray()) {
                ThrowError<Napi::TypeError>(env, "Unexpected %1 value for parameters of '%2', expected an array", GetValueType(instance, (Napi::Value)value[1u]), func->name);
                return env.Null();
            }

            parameters = ((Napi::Value)value[1u]).As<Napi::Array>();
        }

        func->ret.type = ResolveType(instance, value[0u]);
        if (!func->ret.type)
            return env.Null();

        Size out_counter = 0;

        for (uint32_t j = 0; j < parameters.Length(); j++) {
            ParameterInfo param = {};

            param.type = ResolveType(instance, parameters[j], &param.directions);
            if (!param.type)
                return env.Null();
            if (param.type->primitive == PrimitiveKind::Void) {
                ThrowError<Napi::TypeError>(env, "Type void cannot be used as a parameter");
                return env.Null();
            }

            if (func->parameters.len >= MaxParameters) {
                ThrowError<Napi::TypeError>(env, "Functions cannot have more than %1 parameters", MaxParameters);
                return env.Null();
            }
            if ((param.directions & 2) && ++out_counter >= MaxOutParameters) {
                ThrowError<Napi::TypeError>(env, "Functions cannot have more than out %1 parameters", MaxOutParameters);
                return env.Null();
            }

            func->parameters.Append(param);
        }

        if (!AnalyseFunction(instance, func))
            return env.Null();

#ifdef _WIN32
        func->func = (void *)GetProcAddress((HMODULE)module, func->decorated_name);
#else
        func->func = dlsym(module, func->decorated_name);
#endif
        if (!func->func) {
            ThrowError<Napi::Error>(env, "Cannot find function '%1' in shared library", key.c_str());
            return env.Null();
        }

        Napi::Function wrapper = Napi::Function::New(env, TranslateCall, key.c_str(), (void *)func);
        wrapper.AddFinalizer([](Napi::Env, FunctionInfo *func) { delete func; }, func);
        func_guard.Disable();

        obj.Set(key, wrapper);
    }

    return obj;
}

LibraryHolder::~LibraryHolder()
{
#ifdef _WIN32
    if (module && module != GetModuleHandle(nullptr)) {
        FreeLibrary((HMODULE)module);
    }
#else
    if (module && module != RTLD_DEFAULT) {
        dlclose(module);
    }
#endif
}

static void RegisterPrimitiveType(InstanceData *instance, const char *name, PrimitiveKind primitive, int16_t size)
{
    TypeInfo *type = instance->types.AppendDefault();

    type->name = name;

    type->primitive = primitive;
    type->size = size;
    type->align = size;

    RG_ASSERT(!instance->types_map.Find(name));
    instance->types_map.Set(type);
}

static Napi::Object InitBaseTypes(Napi::Env env)
{
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    RG_ASSERT(!instance->types.len);

    RegisterPrimitiveType(instance, "void", PrimitiveKind::Void, 0);
    RegisterPrimitiveType(instance, "void *", PrimitiveKind::Pointer, RG_SIZE(void *));
    RegisterPrimitiveType(instance, "bool", PrimitiveKind::Bool, 1);
    RegisterPrimitiveType(instance, "int8", PrimitiveKind::Int8, 1);
    RegisterPrimitiveType(instance, "int8_t", PrimitiveKind::Int8, 1);
    RegisterPrimitiveType(instance, "uint8", PrimitiveKind::UInt8, 1);
    RegisterPrimitiveType(instance, "uint8_t", PrimitiveKind::UInt8, 1);
    RegisterPrimitiveType(instance, "char", PrimitiveKind::Int8, 1);
    RegisterPrimitiveType(instance, "uchar", PrimitiveKind::UInt8, 1);
    RegisterPrimitiveType(instance, "unsigned char", PrimitiveKind::UInt8, 1);
    RegisterPrimitiveType(instance, "int16", PrimitiveKind::Int16, 2);
    RegisterPrimitiveType(instance, "int16_t", PrimitiveKind::Int16, 2);
    RegisterPrimitiveType(instance, "uint16", PrimitiveKind::UInt16, 2);
    RegisterPrimitiveType(instance, "uint16_t", PrimitiveKind::UInt16, 2);
    RegisterPrimitiveType(instance, "short", PrimitiveKind::Int16, 2);
    RegisterPrimitiveType(instance, "ushort", PrimitiveKind::UInt16, 2);
    RegisterPrimitiveType(instance, "unsigned short", PrimitiveKind::UInt16, 2);
    RegisterPrimitiveType(instance, "int32", PrimitiveKind::Int32, 4);
    RegisterPrimitiveType(instance, "int32_t", PrimitiveKind::Int32, 4);
    RegisterPrimitiveType(instance, "uint32", PrimitiveKind::UInt32, 4);
    RegisterPrimitiveType(instance, "uint32_t", PrimitiveKind::UInt32, 4);
    RegisterPrimitiveType(instance, "int", PrimitiveKind::Int32, 4);
    RegisterPrimitiveType(instance, "uint", PrimitiveKind::UInt32, 4);
    RegisterPrimitiveType(instance, "unsigned int", PrimitiveKind::UInt32, 4);
    RegisterPrimitiveType(instance, "int64", PrimitiveKind::Int64, 8);
    RegisterPrimitiveType(instance, "int64_t", PrimitiveKind::Int64, 8);
    RegisterPrimitiveType(instance, "uint64", PrimitiveKind::UInt64, 8);
    RegisterPrimitiveType(instance, "uint64_t", PrimitiveKind::UInt64, 8);
#if ULONG_MAX == UINT64_MAX
    RegisterPrimitiveType(instance, "long", PrimitiveKind::Int64, 8);
    RegisterPrimitiveType(instance, "ulong", PrimitiveKind::UInt64, 8);
    RegisterPrimitiveType(instance, "unsigned long", PrimitiveKind::UInt64, 8);
#else
    RegisterPrimitiveType(instance, "long", PrimitiveKind::Int32, 4);
    RegisterPrimitiveType(instance, "ulong", PrimitiveKind::UInt32, 4);
    RegisterPrimitiveType(instance, "unsigned long", PrimitiveKind::UInt64, 4);
#endif
    RegisterPrimitiveType(instance, "longlong", PrimitiveKind::Int64, 8);
    RegisterPrimitiveType(instance, "long long", PrimitiveKind::Int64, 8);
    RegisterPrimitiveType(instance, "ulonglong", PrimitiveKind::UInt64, 8);
    RegisterPrimitiveType(instance, "unsigned long long", PrimitiveKind::UInt64, 8);
    RegisterPrimitiveType(instance, "float32", PrimitiveKind::Float32, 4);
    RegisterPrimitiveType(instance, "float64", PrimitiveKind::Float64, 8);
    RegisterPrimitiveType(instance, "float", PrimitiveKind::Float32, 4);
    RegisterPrimitiveType(instance, "double", PrimitiveKind::Float64, 8);
    RegisterPrimitiveType(instance, "string", PrimitiveKind::String, RG_SIZE(void *));
    RegisterPrimitiveType(instance, "str", PrimitiveKind::String, RG_SIZE(void *));

    Napi::Object types = Napi::Object::New(env);
    for (TypeInfo &type: instance->types) {
        Napi::External<TypeInfo> external = Napi::External<TypeInfo>::New(env, &type);
        SetValueTag(instance, external, &TypeInfoMarker);

        types.Set(type.name, external);

        if (strchr(type.name, ' ')) {
            char name_buf[64] = {};
            for (Size i = 0; type.name[i]; i++) {
                RG_ASSERT(i < RG_SIZE(name_buf) - 1);
                name_buf[i] = type.name[i] != ' ' ? type.name[i] : '_';
            }
            types.Set(name_buf, external);
        }
    }
    types.Freeze();

    return types;
}

InstanceData::InstanceData()
{
    stack_mem.len = Mebibytes(2);
    stack_mem.ptr = (uint8_t *)Allocator::Allocate(&mem_alloc, stack_mem.len);

    heap_mem.len = Mebibytes(4);
    heap_mem.ptr = (uint8_t *)Allocator::Allocate(&mem_alloc, heap_mem.len);
}

}

#if NODE_WANT_INTERNALS

static void SetValue(node::Environment *env, v8::Local<v8::Object> target,
                     const char *name, Napi::Value value)
{
    v8::Isolate *isolate = env->isolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    v8::NewStringType str_type = v8::NewStringType::kInternalized;
    v8::Local<v8::String> str = v8::String::NewFromUtf8(isolate, name, str_type).ToLocalChecked();

    target->Set(context, str, v8impl::V8LocalValueFromJsValue(value)).Check();
}

static void InitInternal(v8::Local<v8::Object> target, v8::Local<v8::Value>,
                         v8::Local<v8::Context> context, void *)
{
    using namespace RG;

    node::Environment *env = node::Environment::GetCurrent(context);

    // Not very clean but I don't know enough about Node and V8 to do better...
    // ... and it seems to work okay.
    napi_env env_napi = new napi_env__(context);
    Napi::Env env_cxx(env_napi);
    env->AtExit([](void *udata) {
        napi_env env_napi = (napi_env)udata;
        delete env_napi;
    }, env_napi);

    InstanceData *instance = new InstanceData();
    env_cxx.SetInstanceData(instance);

    instance->debug = GetDebugFlag("DUMP_CALLS");
    FillRandomSafe(&instance->tag_lower, RG_SIZE(instance->tag_lower));

    SetValue(env, target, "struct", Napi::Function::New(env_napi, CreateStructType));
    SetValue(env, target, "handle", Napi::Function::New(env_napi, CreateHandleType));
    SetValue(env, target, "pointer", Napi::Function::New(env_napi, CreatePointerType));
    SetValue(env, target, "load", Napi::Function::New(env_napi, LoadSharedLibrary));
    SetValue(env, target, "in", Napi::Function::New(env_napi, MarkIn));
    SetValue(env, target, "out", Napi::Function::New(env_napi, MarkOut));
    SetValue(env, target, "inout", Napi::Function::New(env_napi, MarkInOut));
    SetValue(env, target, "internal", Napi::Boolean::New(env_napi, true));

    Napi::Object types = InitBaseTypes(env_cxx);
    SetValue(env, target, "types", types);
}

#else

static Napi::Object InitModule(Napi::Env env, Napi::Object exports)
{
    using namespace RG;

    InstanceData *instance = new InstanceData();
    env.SetInstanceData(instance);

    instance->debug = GetDebugFlag("DUMP_CALLS");
    FillRandomSafe(&instance->tag_lower, RG_SIZE(instance->tag_lower));

    exports.Set("struct", Napi::Function::New(env, CreateStructType));
    exports.Set("handle", Napi::Function::New(env, CreateHandleType));
    exports.Set("pointer", Napi::Function::New(env, CreatePointerType));
    exports.Set("load", Napi::Function::New(env, LoadSharedLibrary));
    exports.Set("in", Napi::Function::New(env, MarkIn));
    exports.Set("out", Napi::Function::New(env, MarkOut));
    exports.Set("inout", Napi::Function::New(env, MarkInOut));
    exports.Set("internal", Napi::Boolean::New(env, false));

    Napi::Object types = InitBaseTypes(env);
    exports.Set("types", types);

    return exports;
}

#endif

#if NODE_WANT_INTERNALS
    NODE_MODULE_CONTEXT_AWARE_INTERNAL(koffi, InitInternal);
#else
    NODE_API_MODULE(koffi, InitModule);
#endif
