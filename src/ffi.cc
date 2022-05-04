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
const int TypeInfoMarker = 0xDEADBEEF;

static Napi::Value CreateStructType(const Napi::CallbackInfo &info, bool pad)
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
    if (!IsObject(info[named])) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for members, expected object", GetValueType(instance, info[1]));
        return env.Null();
    }

    TypeInfo *type = instance->types.AppendDefault();
    RG_DEFER_N(err_guard) { instance->types.RemoveLast(1); };

    std::string name = named ? info[0].As<Napi::String>() : std::string("<anonymous>");
    Napi::Object obj = info[named].As<Napi::Object>();
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

        member.align = pad ? member.type->align : 1;

        type->size = (int16_t)(AlignLen(type->size, member.align) + member.type->size);
        type->align = std::max(type->align, member.align);

        type->members.Append(member);
    }

    type->size = (int16_t)AlignLen(type->size, type->align);

    // If the insert succeeds, we cannot fail anymore
    if (named && !instance->types_map.TrySet(type).second) {
        ThrowError<Napi::Error>(env, "Duplicate type name '%1'", type->name);
        return env.Null();
    }
    err_guard.Disable();

    Napi::External<TypeInfo> external = Napi::External<TypeInfo>::New(env, type);
    SetValueTag(instance, external, &TypeInfoMarker);

    return external;
}

static Napi::Value CreatePaddedStructType(const Napi::CallbackInfo &info)
{
    return CreateStructType(info, true);
}

static Napi::Value CreatePackedStructType(const Napi::CallbackInfo &info)
{
    return CreateStructType(info, false);
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
    type->align = alignof(void *);
    type->size = RG_SIZE(void *);

    // Add single handle member
    {
        RecordMember member = {};

        member.name = "value";
        member.type = instance->types_map.FindValue("void *", nullptr);
        RG_ASSERT(member.type);
        member.align = type->align;

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

static Napi::Value TranslateNormalCall(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();
    FunctionInfo *func = (FunctionInfo *)info.Data();

    return TranslateCall(instance, func, info);
}

static Napi::Value TranslateVariadicCall(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    FunctionInfo func;
    memcpy(&func, info.Data(), RG_SIZE(FunctionInfo));
    func.lib = nullptr;

    // This makes variadic calls non-reentrant
    RG_DEFER_C(len = func.parameters.len) {
        func.parameters.RemoveFrom(len);
        func.parameters.Leak();
    };

    if ((info.Length() - func.parameters.len) % 2) {
        ThrowError<Napi::Error>(env, "Missing value argument for variadic call");
        return env.Null();
    }

    for (Size i = func.parameters.len; i < info.Length(); i += 2) {
        ParameterInfo param = {};

        param.type = ResolveType(instance, info[i], &param.directions);
        if (!param.type)
            return env.Null();
        if (param.type->primitive == PrimitiveKind::Void) {
            ThrowError<Napi::TypeError>(env, "Type void cannot be used as a parameter");
            return env.Null();
        }

        if (func.parameters.len >= MaxParameters) {
            ThrowError<Napi::TypeError>(env, "Functions cannot have more than %1 parameters", MaxParameters);
            return env.Null();
        }
        if ((param.directions & 2) && ++func.out_parameters >= MaxOutParameters) {
            ThrowError<Napi::TypeError>(env, "Functions cannot have more than out %1 parameters", MaxOutParameters);
            return env.Null();
        }
        param.variadic = true;
        param.offset = i + 1;

        func.parameters.Append(param);
    }

    if (!AnalyseFunction(instance, &func))
        return env.Null();

    return TranslateCall(instance, &func, info);
}

static Napi::Value FindLibraryFunction(const Napi::CallbackInfo &info, CallConvention convention, bool variadic)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();
    LibraryHolder *lib = (LibraryHolder *)info.Data();

    if (info.Length() < 3) {
        ThrowError<Napi::TypeError>(env, "Expected 3 or 4 arguments, not %1", info.Length());
        return env.Null();
    }
    if (!info[0].IsString()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for filename, expected string", GetValueType(instance, info[0]));
        return env.Null();
    }

    FunctionInfo *func = new FunctionInfo();
    RG_DEFER_N(func_guard) { delete func; };

    std::string name = ((Napi::Value)info[0u]).As<Napi::String>();
    func->name = DuplicateString(name.c_str(), &instance->str_alloc).ptr;
    func->lib = lib->Ref();
    func->convention = convention;

    func->ret.type = ResolveType(instance, info[1u]);
    if (!func->ret.type)
        return env.Null();
    if (!((Napi::Value)info[2u]).IsArray()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for parameters of '%2', expected an array", GetValueType(instance, (Napi::Value)info[1u]), func->name);
        return env.Null();
    }

    Napi::Array parameters = ((Napi::Value)info[2u]).As<Napi::Array>();
    Size parameters_len = parameters.Length();

    if (parameters_len) {
        Napi::String str = ((Napi::Value)parameters[(uint32_t)(parameters_len - 1)]).As<Napi::String>();

        if (str.IsString() && str.Utf8Value() == "...") {
            func->variadic = true;
            parameters_len--;
        }

        if (!variadic && func->variadic) {
            LogError("Call convention '%1' does not support variadic functions, ignoring");
            func->convention = CallConvention::Default;
        }
    }

    for (uint32_t j = 0; j < parameters_len; j++) {
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
        if ((param.directions & 2) && ++func->out_parameters >= MaxOutParameters) {
            ThrowError<Napi::TypeError>(env, "Functions cannot have more than out %1 parameters", MaxOutParameters);
            return env.Null();
        }
        param.offset = j;

        func->parameters.Append(param);
    }

    if (!AnalyseFunction(instance, func))
        return env.Null();
    if (func->variadic) {
        // Minimize reallocations
        func->parameters.Grow(32);
    }

#ifdef _WIN32
    if (func->decorated_name) {
        func->func = (void *)GetProcAddress((HMODULE)lib->module, func->decorated_name);
    }
    if (!func->func) {
        func->func = (void *)GetProcAddress((HMODULE)lib->module, func->name);
    }
#else
    if (func->decorated_name) {
        func->func = dlsym(lib->module, func->decorated_name);
    }
    if (!func->func) {
        func->func = dlsym(lib->module, func->name);
    }
#endif
    if (!func->func) {
        ThrowError<Napi::Error>(env, "Cannot find function '%1' in shared library", func->name);
        return env.Null();
    }

    Napi::Function::Callback call = func->variadic ? TranslateVariadicCall : TranslateNormalCall;
    Napi::Function wrapper = Napi::Function::New(env, call, name.c_str(), (void *)func);
    wrapper.AddFinalizer([](Napi::Env, FunctionInfo *func) { delete func; }, func);
    func_guard.Disable();

    return wrapper;
}

static Napi::Value LoadSharedLibrary(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 2 arguments, not %1", info.Length());
        return env.Null();
    }
    if (!info[0].IsString() && !IsNullOrUndefined(info[0])) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for filename, expected string or null", GetValueType(instance, info[0]));
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

    LibraryHolder *lib = new LibraryHolder(module);
    RG_DEFER { lib->Unref(); };

    Napi::Object obj = Napi::Object::New(env);

#define ADD_CONVENTION(Name, Value, Variadic) \
        do { \
            const auto wrapper = [](const Napi::CallbackInfo &info) { return FindLibraryFunction(info, (Value), (Variadic)); }; \
            Napi::Function func = Napi::Function::New(env, wrapper, (Name), (void *)lib->Ref()); \
            func.AddFinalizer([](Napi::Env, LibraryHolder *lib) { lib->Unref(); }, lib); \
            obj.Set((Name), func); \
        } while (false)

    ADD_CONVENTION("cdecl", CallConvention::Default, true);
    ADD_CONVENTION("stdcall", CallConvention::Stdcall, false);
    ADD_CONVENTION("fastcall", CallConvention::Fastcall, false);

#undef ADD_CONVENTION

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

LibraryHolder *LibraryHolder::Ref()
{
    refcount++;
    return this;
}

void LibraryHolder::Unref()
{
    if (!--refcount) {
        delete this;
    }
}

static void RegisterPrimitiveType(InstanceData *instance, const char *name, PrimitiveKind primitive,
                                  int16_t size, int16_t align)
{
    TypeInfo *type = instance->types.AppendDefault();

    type->name = name;

    type->primitive = primitive;
    type->size = size;
    type->align = align;

    RG_ASSERT(!instance->types_map.Find(name));
    instance->types_map.Set(type);
}

static Napi::Object InitBaseTypes(Napi::Env env)
{
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    RG_ASSERT(!instance->types.len);

    RegisterPrimitiveType(instance, "void", PrimitiveKind::Void, 0, 0);
    RegisterPrimitiveType(instance, "void *", PrimitiveKind::Pointer, RG_SIZE(void *), alignof(void *));
    RegisterPrimitiveType(instance, "bool", PrimitiveKind::Bool, RG_SIZE(bool), alignof(bool));
    RegisterPrimitiveType(instance, "int8", PrimitiveKind::Int8, 1, 1);
    RegisterPrimitiveType(instance, "int8_t", PrimitiveKind::Int8, 1, 1);
    RegisterPrimitiveType(instance, "uint8", PrimitiveKind::UInt8, 1, 1);
    RegisterPrimitiveType(instance, "uint8_t", PrimitiveKind::UInt8, 1, 1);
    RegisterPrimitiveType(instance, "char", PrimitiveKind::Int8, 1, 1);
    RegisterPrimitiveType(instance, "uchar", PrimitiveKind::UInt8, 1, 1);
    RegisterPrimitiveType(instance, "unsigned char", PrimitiveKind::UInt8, 1, 1);
    RegisterPrimitiveType(instance, "int16", PrimitiveKind::Int16, 2, 2);
    RegisterPrimitiveType(instance, "int16_t", PrimitiveKind::Int16, 2, 2);
    RegisterPrimitiveType(instance, "uint16", PrimitiveKind::UInt16, 2, 2);
    RegisterPrimitiveType(instance, "uint16_t", PrimitiveKind::UInt16, 2, 2);
    RegisterPrimitiveType(instance, "short", PrimitiveKind::Int16, 2, 2);
    RegisterPrimitiveType(instance, "ushort", PrimitiveKind::UInt16, 2, 2);
    RegisterPrimitiveType(instance, "unsigned short", PrimitiveKind::UInt16, 2, 2);
    RegisterPrimitiveType(instance, "int32", PrimitiveKind::Int32, 4, 4);
    RegisterPrimitiveType(instance, "int32_t", PrimitiveKind::Int32, 4, 4);
    RegisterPrimitiveType(instance, "uint32", PrimitiveKind::UInt32, 4, 4);
    RegisterPrimitiveType(instance, "uint32_t", PrimitiveKind::UInt32, 4, 4);
    RegisterPrimitiveType(instance, "int", PrimitiveKind::Int32, 4, 4);
    RegisterPrimitiveType(instance, "uint", PrimitiveKind::UInt32, 4, 4);
    RegisterPrimitiveType(instance, "unsigned int", PrimitiveKind::UInt32, 4, 4);
    RegisterPrimitiveType(instance, "int64", PrimitiveKind::Int64, 8, alignof(int64_t));
    RegisterPrimitiveType(instance, "int64_t", PrimitiveKind::Int64, 8, alignof(int64_t));
    RegisterPrimitiveType(instance, "uint64", PrimitiveKind::UInt64, 8, alignof(int64_t));
    RegisterPrimitiveType(instance, "uint64_t", PrimitiveKind::UInt64, 8, alignof(int64_t));
    RegisterPrimitiveType(instance, "long", PrimitiveKind::Int64, RG_SIZE(long), alignof(long));
    RegisterPrimitiveType(instance, "ulong", PrimitiveKind::UInt64, RG_SIZE(long), alignof(long));
    RegisterPrimitiveType(instance, "unsigned long", PrimitiveKind::UInt64, RG_SIZE(long), alignof(long));
    RegisterPrimitiveType(instance, "longlong", PrimitiveKind::Int64, RG_SIZE(long long), alignof(long long));
    RegisterPrimitiveType(instance, "long long", PrimitiveKind::Int64, RG_SIZE(long long), alignof(long long));
    RegisterPrimitiveType(instance, "ulonglong", PrimitiveKind::UInt64, RG_SIZE(long long), alignof(long long));
    RegisterPrimitiveType(instance, "unsigned long long", PrimitiveKind::UInt64, RG_SIZE(long long), alignof(long long));
    RegisterPrimitiveType(instance, "float32", PrimitiveKind::Float32, 4, alignof(float));
    RegisterPrimitiveType(instance, "float64", PrimitiveKind::Float64, 8, alignof(double));
    RegisterPrimitiveType(instance, "float", PrimitiveKind::Float32, 4, alignof(float));
    RegisterPrimitiveType(instance, "double", PrimitiveKind::Float64, 8, alignof(double));
    RegisterPrimitiveType(instance, "string", PrimitiveKind::String, RG_SIZE(void *), alignof(void *));
    RegisterPrimitiveType(instance, "str", PrimitiveKind::String, RG_SIZE(void *), alignof(void *));

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

FunctionInfo::~FunctionInfo()
{
    if (lib) {
        lib->Unref();
    }
}

static Span<uint8_t> AllocateAndAlign16(Allocator *alloc, Size size)
{
    RG_ASSERT(AlignLen(size, 16) == size);

    uint8_t *ptr = (uint8_t *)Allocator::Allocate(alloc, size);
    uint8_t *aligned = AlignUp(ptr, 16);
    Size delta = AlignLen(aligned - ptr, 16);

    return MakeSpan(aligned, size - delta);
}

InstanceData::InstanceData()
{
    stack_mem = AllocateAndAlign16(&mem_alloc, Mebibytes(2));
    heap_mem = AllocateAndAlign16(&mem_alloc, Mebibytes(4));
}

template <typename Func>
static void SetExports(Napi::Env env, Func func)
{
    func("struct", Napi::Function::New(env, CreatePaddedStructType));
    func("pack", Napi::Function::New(env, CreatePackedStructType));
    func("handle", Napi::Function::New(env, CreateHandleType));
    func("pointer", Napi::Function::New(env, CreatePointerType));
    func("load", Napi::Function::New(env, LoadSharedLibrary));
    func("in", Napi::Function::New(env, MarkIn));
    func("out", Napi::Function::New(env, MarkOut));
    func("inout", Napi::Function::New(env, MarkInOut));

    func("internal", Napi::Boolean::New(env, true));
#if defined(_WIN32)
    func("extension", Napi::String::New(env, ".dll"));
#elif defined(__APPLE__)
    func("extension", Napi::String::New(env, ".dylib"));
#else
    func("extension", Napi::String::New(env, ".so"));
#endif

    Napi::Object types = InitBaseTypes(env);
    func("types", types);
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

    SetExports(env_napi, [&](const char *name, Napi::Value value) { SetValue(env, target, name, value); });
}

#else

static Napi::Object InitModule(Napi::Env env, Napi::Object exports)
{
    using namespace RG;

    InstanceData *instance = new InstanceData();
    env.SetInstanceData(instance);

    instance->debug = GetDebugFlag("DUMP_CALLS");
    FillRandomSafe(&instance->tag_lower, RG_SIZE(instance->tag_lower));

    SetExports(env, [&](const char *name, Napi::Value value) { exports.Set(name, value); });

    return exports;
}

#endif

#if NODE_WANT_INTERNALS
    NODE_MODULE_CONTEXT_AWARE_INTERNAL(koffi, InitInternal);
#else
    NODE_API_MODULE(koffi, InitModule);
#endif
