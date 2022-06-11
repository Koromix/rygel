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
#include "parser.hh"
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
    #include <sys/mman.h>
#endif

#include <napi.h>
#if NODE_WANT_INTERNALS
    #include <env-inl.h>
    #include <js_native_api_v8.h>
#endif

namespace RG {

// Value does not matter, the tag system uses memory addresses
const int TypeInfoMarker = 0xDEADBEEF;

static bool ChangeMemorySize(Napi::Value value, Size *out_size)
{
    const Size MinSize = Kibibytes(1);
    const Size MaxSize = Mebibytes(16);

    Napi::Env env = value.Env();

    if (!value.IsNumber()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for memory size, expected number");
        return env.Null();
    }

    int64_t size = value.As<Napi::Number>().Int64Value();

    if (size < MinSize || size > MaxSize) {
        ThrowError<Napi::Error>(env, "Memory size must be between %1 and %2", FmtMemSize(MinSize), FmtMemSize(MaxSize));
        return false;
    }

    *out_size = (Size)size;
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

        if (!info[0].IsObject()) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for config, expected object", GetValueType(instance, info[0]));
            return env.Null();
        }

        Napi::Object obj = info[0].As<Napi::Object>();
        Napi::Array keys = obj.GetPropertyNames();

        for (uint32_t i = 0; i < keys.Length(); i++) {
            std::string key = ((Napi::Value)keys[i]).As<Napi::String>();
            Napi::Value value = obj[key];

            if (key == "sync_stack_size") {
                if (!ChangeMemorySize(value, &instance->sync_stack_size))
                    return env.Null();
            } else if (key == "sync_heap_size") {
                if (!ChangeMemorySize(value, &instance->sync_heap_size))
                    return env.Null();
            } else if (key == "async_stack_size") {
                if (!ChangeMemorySize(value, &instance->async_stack_size))
                    return env.Null();
            } else if (key == "async_heap_size") {
                if (!ChangeMemorySize(value, &instance->async_heap_size))
                    return env.Null();
            } else if (key == "resident_async_pools") {
                if (!value.IsNumber()) {
                    ThrowError<Napi::TypeError>(env, "Unexpected %1 value for resident_async_pools, expected number");
                    return env.Null();
                }

                int64_t n = value.As<Napi::Number>().Int64Value();

                if (n < 0 || n > RG_LEN(instance->memories.data)) {
                    ThrowError<Napi::Error>(env, "Parameter resident_async_pools must be between 0 and %1", RG_LEN(instance->memories.data));
                    return env.Null();
                }

                RG_STATIC_ASSERT(DefaultResidentAsyncPools <= RG_LEN(instance->memories.data));
            } else {
                ThrowError<Napi::Error>(env, "Unexpected config member '%1'", key.c_str());
                return env.Null();
            }
        }
    }

    Napi::Object obj = Napi::Object::New(env);

    obj.Set("sync_stack_size", instance->sync_stack_size);
    obj.Set("sync_heap_size", instance->sync_heap_size);
    obj.Set("async_stack_size", instance->async_stack_size);
    obj.Set("async_heap_size", instance->async_heap_size);
    obj.Set("resident_async_pools", instance->resident_async_pools);

    return obj;
}

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

    type->defn.Reset(obj, 1);

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
        if (member.type->primitive == PrimitiveKind::Void) {
            ThrowError<Napi::TypeError>(env, "Type Void cannot be used as a member");
            return env.Null();
        }

        member.align = pad ? member.type->align : 1;

        type->size = (int16_t)(AlignLen(type->size, member.align) + member.type->size);
        type->align = std::max(type->align, member.align);

        type->members.Append(member);
    }

    if (!type->size) {
        ThrowError<Napi::TypeError>(env, "Empty struct '%1' is not allowed in C", type->name);
        return env.Null();
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

    TypeInfo *type = (TypeInfo *)GetPointerType(instance, ref);
    RG_ASSERT(type);

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

static Napi::Value CreateArrayType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 2) {
        ThrowError<Napi::TypeError>(env, "Expected 2 arguments, got %1", info.Length());
        return env.Null();
    }
    if (!info[1].IsNumber()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for length, expected integer", GetValueType(instance, info[1]));
        return env.Null();
    }

    TypeInfo::ArrayHint hint;
    if (info.Length() >= 3 && !IsNullOrUndefined(info[2])) {
        if (!info[2].IsString()) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for hint, expected string", GetValueType(instance, info[2]));
            return env.Null();
        }

        std::string to = info[2].As<Napi::String>();

        if (to == "typed") {
            hint = TypeInfo::ArrayHint::TypedArray;
        } else if (to == "array") {
            hint = TypeInfo::ArrayHint::Array;
        } else if (to == "string") {
            hint = TypeInfo::ArrayHint::String;
        } else {
            ThrowError<Napi::Error>(env, "Array conversion hint must be 'typed' or 'array'");
            return env.Null();
        }
    } else {
        hint = TypeInfo::ArrayHint::TypedArray;
    }

    const TypeInfo *ref = ResolveType(instance, info[0]);
    int64_t len = (uint16_t)info[1].As<Napi::Number>().Int64Value();

    if (!ref)
        return env.Null();
    if (len <= 0) {
        ThrowError<Napi::TypeError>(env, "Array length must be positive and non-zero");
        return env.Null();
    }
    if (len > INT16_MAX / ref->size) {
        ThrowError<Napi::TypeError>(env, "Array length is too high (max = %1)", INT16_MAX / ref->size);
        return env.Null();
    }

    TypeInfo *type = instance->types.AppendDefault();

    type->name = Fmt(&instance->str_alloc, "%1[%2]", ref->name, len).ptr;

    type->primitive = PrimitiveKind::Array;
    type->align = ref->align;
    type->size = (int16_t)(len * ref->size);
    type->ref = ref;
    type->hint = hint;

    Napi::External<TypeInfo> external = Napi::External<TypeInfo>::New(env, type);
    SetValueTag(instance, external, &TypeInfoMarker);

    return external;
}

static bool ParseClassicFunction(Napi::Env env, Napi::String name, Napi::Value ret,
                                 Napi::Array parameters, FunctionInfo *func)
{
    InstanceData *instance = env.GetInstanceData<InstanceData>();

#ifdef _WIN32
    if (!name.IsString() && !name.IsNumber()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for name, expected string or integer", GetValueType(instance, name));
        return false;
    }
#else
    if (!name.IsString()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for name, expected string", GetValueType(instance, name));
        return false;
    }
#endif

    func->name = DuplicateString(name.ToString().Utf8Value().c_str(), &instance->str_alloc).ptr;

    func->ret.type = ResolveType(instance, ret);
    if (!func->ret.type)
        return false;
    if (func->ret.type->primitive == PrimitiveKind::Array) {
        ThrowError<Napi::Error>(env, "You are not allowed to directly return fixed-size arrays");
        return false;
    }

    if (!parameters.IsArray()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for parameters of '%2', expected an array", GetValueType(instance, parameters), func->name);
        return false;
    }

    uint32_t parameters_len = parameters.Length();

    if (parameters_len) {
        Napi::String str = ((Napi::Value)parameters[parameters_len - 1]).As<Napi::String>();

        if (str.IsString() && str.Utf8Value() == "...") {
            func->variadic = true;
            parameters_len--;
        }
    }

    for (uint32_t j = 0; j < parameters_len; j++) {
        ParameterInfo param = {};

        param.type = ResolveType(instance, parameters[j], &param.directions);
        if (!param.type)
            return false;
        if (param.type->primitive == PrimitiveKind::Void ||
                param.type->primitive == PrimitiveKind::Array) {
            ThrowError<Napi::TypeError>(env, "Type %1 cannot be used as a parameter", param.type->name);
            return false;
        }

        if (func->parameters.len >= MaxParameters) {
            ThrowError<Napi::TypeError>(env, "Functions cannot have more than %1 parameters", MaxParameters);
            return false;
        }
        if ((param.directions & 2) && ++func->out_parameters >= MaxOutParameters) {
            ThrowError<Napi::TypeError>(env, "Functions cannot have more than out %1 parameters", MaxOutParameters);
            return false;
        }

        param.offset = (int8_t)j;

        func->parameters.Append(param);
    }

    return true;
}

static Napi::Value CreateCallbackType(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    FunctionInfo *func = instance->callbacks.AppendDefault();
    RG_DEFER_N(err_guard) { instance->callbacks.RemoveLast(1); };

    if (info.Length() >= 3) {
        if (!ParseClassicFunction(env, info[0u].As<Napi::String>(), info[1u], info[2u].As<Napi::Array>(), func))
            return env.Null();
    } else if (info.Length() >= 1) {
        if (!info[0].IsString()) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for prototype, expected string", GetValueType(instance, info[0]));
            return env.Null();
        }

        std::string proto = info[0u].As<Napi::String>();
        if (!ParsePrototype(env, proto.c_str(), func))
            return env.Null();
    } else {
        ThrowError<Napi::TypeError>(env, "Expected 1 or 3 arguments, not %1", info.Length());
        return env.Null();
    }

    if (func->variadic) {
        LogError("Variadic callbacks are not supported");
        return env.Null();
    }

    if (!AnalyseFunction(env, instance, func))
        return env.Null();

    // We cannot fail after this check
    if (instance->types_map.Find(func->name)) {
        ThrowError<Napi::Error>(env, "Duplicate type name '%1'", func->name);
        return env.Null();
    }
    err_guard.Disable();

    TypeInfo *type = instance->types.AppendDefault();

    type->name = func->name;

    type->primitive = PrimitiveKind::Callback;
    type->align = alignof(void *);
    type->size = RG_SIZE(void *);
    type->proto = func;

    instance->types_map.Set(type);

    Napi::External<TypeInfo> external = Napi::External<TypeInfo>::New(env, type);
    SetValueTag(instance, external, &TypeInfoMarker);

    return external;
}

static Napi::Value GetTypeSize(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }

    const TypeInfo *type = ResolveType(instance, info[0]);
    if (!type)
        return env.Null();

    return Napi::Number::New(env, type->size);
}

static Napi::Value GetTypeAlign(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }

    const TypeInfo *type = ResolveType(instance, info[0]);
    if (!type)
        return env.Null();

    return Napi::Number::New(env, type->align);
}

static Napi::Value GetTypeDefinition(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }

    const TypeInfo *type = ResolveType(instance, info[0]);
    if (!type)
        return env.Null();
    if (type->defn.IsEmpty()) {
        ThrowError<Napi::TypeError>(env, "Definition of type %1 is not available", type->name);
        return env.Null();
    }

    return type->defn.Value();
}

static InstanceMemory *AllocateMemory(InstanceData *instance, Size stack_size, Size heap_size)
{
    for (Size i = 1; i < instance->memories.len; i++) {
        InstanceMemory *mem = instance->memories[i];

        if (!mem->depth)
            return mem;
    }

    InstanceMemory *mem = new InstanceMemory();

    mem->stack.len = stack_size;
#if defined(_WIN32)
    mem->stack.ptr = (uint8_t *)VirtualAlloc(nullptr, mem->stack.len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#elif defined(__APPLE__)
    mem->stack.ptr = (uint8_t *)mmap(nullptr, mem->stack.len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
#else
    mem->stack.ptr = (uint8_t *)mmap(nullptr, mem->stack.len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_STACK, -1, 0);
#endif
    RG_CRITICAL(mem->stack.ptr, "Failed to allocate %1 of memory", mem->stack.len);

    mem->heap.len = heap_size;
#ifdef _WIN32
    mem->heap.ptr = (uint8_t *)VirtualAlloc(nullptr, mem->heap.len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    mem->heap.ptr = (uint8_t *)mmap(nullptr, mem->heap.len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
#endif
    RG_CRITICAL(mem->heap.ptr, "Failed to allocate %1 of memory", mem->heap.len);

    if (instance->memories.len <= instance->resident_async_pools) {
        instance->memories.Append(mem);
    } else {
        mem->temporary = true;
    }

    return mem;
}

static Napi::Value TranslateNormalCall(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();
    FunctionInfo *func = (FunctionInfo *)info.Data();

    if (RG_UNLIKELY(info.Length() < (uint32_t)func->parameters.len)) {
        ThrowError<Napi::TypeError>(env, "Expected %1 arguments, got %2", func->parameters.len, info.Length());
        return env.Null();
    }

    InstanceMemory *mem = instance->memories[0];
    CallData call(env, instance, func, mem);

    if (!RG_UNLIKELY(call.Prepare(info)))
        return env.Null();

    if (instance->debug) {
        call.DumpForward();
    }
    call.Execute();

    return call.Complete();
}

static Napi::Value TranslateVariadicCall(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    FunctionInfo func;
    memcpy((void *)&func, info.Data(), RG_SIZE(FunctionInfo));
    func.lib = nullptr;

    // This makes variadic calls non-reentrant
    RG_DEFER_C(len = func.parameters.len) {
        func.parameters.RemoveFrom(len);
        func.parameters.Leak();
    };

    if (RG_UNLIKELY(info.Length() < (uint32_t)func.parameters.len)) {
        ThrowError<Napi::TypeError>(env, "Expected %1 arguments or more, got %2", func.parameters.len, info.Length());
        return env.Null();
    }
    if (RG_UNLIKELY((info.Length() - func.parameters.len) % 2)) {
        ThrowError<Napi::Error>(env, "Missing value argument for variadic call");
        return env.Null();
    }

    for (Size i = func.parameters.len; i < (Size)info.Length(); i += 2) {
        ParameterInfo param = {};

        param.type = ResolveType(instance, info[i], &param.directions);
        if (RG_UNLIKELY(!param.type))
            return env.Null();
        if (RG_UNLIKELY(param.type->primitive == PrimitiveKind::Void ||
                        param.type->primitive == PrimitiveKind::Array)) {
            ThrowError<Napi::TypeError>(env, "Type %1 cannot be used as a parameter", PrimitiveKindNames[(int)param.type->primitive]);
            return env.Null();
        }

        if (RG_UNLIKELY(func.parameters.len >= MaxParameters)) {
            ThrowError<Napi::TypeError>(env, "Functions cannot have more than %1 parameters", MaxParameters);
            return env.Null();
        }
        if (RG_UNLIKELY((param.directions & 2) && ++func.out_parameters >= MaxOutParameters)) {
            ThrowError<Napi::TypeError>(env, "Functions cannot have more than out %1 parameters", MaxOutParameters);
            return env.Null();
        }

        param.variadic = true;
        param.offset = (int8_t)(i + 1);

        func.parameters.Append(param);
    }

    if (RG_UNLIKELY(!AnalyseFunction(env, instance, &func)))
        return env.Null();

    InstanceMemory *mem = instance->memories[0];
    CallData call(env, instance, &func, mem);

    if (!RG_UNLIKELY(call.Prepare(info)))
        return env.Null();

    if (instance->debug) {
        call.DumpForward();
    }
    call.Execute();

    return call.Complete();
}

class AsyncCall: public Napi::AsyncWorker {
    Napi::Env env;
    const FunctionInfo *func;

    CallData call;
    bool prepared = false;

public:
    AsyncCall(Napi::Env env, InstanceData *instance, const FunctionInfo *func,
              InstanceMemory *mem, Napi::Function &callback)
        : Napi::AsyncWorker(callback), env(env), func(func->Ref()),
          call(env, instance, func, mem) {}
    ~AsyncCall() { func->Unref(); }

    bool Prepare(const Napi::CallbackInfo &info) {
        prepared = call.Prepare(info);

        if (!prepared) {
            Napi::Error err = env.GetAndClearPendingException();
            SetError(err.Message());
        }

        return prepared;
    }
    void DumpForward() { call.DumpForward(); }

    void Execute() override;
    void OnOK() override;
};

void AsyncCall::Execute()
{
    if (prepared) {
        call.Execute();
    }
}

void AsyncCall::OnOK()
{
    RG_ASSERT(prepared);

    Napi::FunctionReference &callback = Callback();

    Napi::Value self = env.Null();
    napi_value args[] = {
        env.Null(),
        call.Complete()
    };

    callback.Call(self, RG_LEN(args), args);
}

static Napi::Value TranslateAsyncCall(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();
    FunctionInfo *func = (FunctionInfo *)info.Data();

    if (info.Length() <= (uint32_t)func->parameters.len) {
        ThrowError<Napi::TypeError>(env, "Expected %1 arguments, got %2", func->parameters.len + 1, info.Length());
        return env.Null();
    }

    Napi::Function callback = info[(uint32_t)func->parameters.len].As<Napi::Function>();

    if (!callback.IsFunction()) {
        ThrowError<Napi::TypeError>(env, "Expected callback function as last arguments, got %1", GetValueType(instance, callback));
        return env.Null();
    }

    InstanceMemory *mem = AllocateMemory(instance, instance->async_stack_size, instance->async_heap_size);
    AsyncCall *async = new AsyncCall(env, instance, func, mem, callback);

    if (async->Prepare(info) && instance->debug) {
        async->DumpForward();
    }
    async->Queue();

    return env.Null();
}

static Napi::Value FindLibraryFunction(const Napi::CallbackInfo &info, CallConvention convention)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();
    LibraryHolder *lib = (LibraryHolder *)info.Data();

    FunctionInfo *func = new FunctionInfo();
    RG_DEFER { func->Unref(); };

    func->lib = lib->Ref();
    func->convention = convention;

    if (info.Length() >= 3) {
        if (!ParseClassicFunction(env, info[0u].As<Napi::String>(), info[1u], info[2u].As<Napi::Array>(), func))
            return env.Null();
    } else if (info.Length() >= 1) {
        if (!info[0].IsString()) {
            ThrowError<Napi::TypeError>(env, "Unexpected %1 value for prototype, expected string", GetValueType(instance, info[0]));
            return env.Null();
        }

        std::string proto = info[0u].As<Napi::String>();
        if (!ParsePrototype(env, proto.c_str(), func))
            return env.Null();
    } else {
        ThrowError<Napi::TypeError>(env, "Expected 1 or 3 arguments, not %1", info.Length());
        return env.Null();
    }

    if (func->convention != CallConvention::Cdecl && func->variadic) {
        LogError("Call convention '%1' does not support variadic functions, ignoring",
                 CallConventionNames[(int)func->convention]);
        func->convention = CallConvention::Cdecl;
    }

    if (!AnalyseFunction(env, instance, func))
        return env.Null();
    if (func->variadic) {
        // Minimize reallocations
        func->parameters.Grow(32);
    }

#ifdef _WIN32
    if (info[0].IsString()) {
        if (func->decorated_name) {
            func->func = (void *)GetProcAddress((HMODULE)lib->module, func->decorated_name);
        }
        if (!func->func) {
            func->func = (void *)GetProcAddress((HMODULE)lib->module, func->name);
        }
    } else {
        uint16_t ordinal = (uint16_t)info[0].As<Napi::Number>().Uint32Value();

        func->decorated_name = nullptr;
        func->func = (void *)GetProcAddress((HMODULE)lib->module, (LPCSTR)ordinal);
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
    Napi::Function wrapper = Napi::Function::New(env, call, func->name, (void *)func->Ref());
    wrapper.AddFinalizer([](Napi::Env, FunctionInfo *func) { func->Unref(); }, func);

    if (!func->variadic) {
        Napi::Function async = Napi::Function::New(env, TranslateAsyncCall, func->name, (void *)func->Ref());
        async.AddFinalizer([](Napi::Env, FunctionInfo *func) { func->Unref(); }, func);
        wrapper.Set("async", async);
    }

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

    if (!instance->memories.len) {
        AllocateMemory(instance, instance->sync_stack_size, instance->sync_heap_size);
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

#define ADD_CONVENTION(Name, Value) \
        do { \
            const auto wrapper = [](const Napi::CallbackInfo &info) { return FindLibraryFunction(info, (Value)); }; \
            Napi::Function func = Napi::Function::New(env, wrapper, (Name), (void *)lib->Ref()); \
            func.AddFinalizer([](Napi::Env, LibraryHolder *lib) { lib->Unref(); }, lib); \
            obj.Set((Name), func); \
        } while (false)

    ADD_CONVENTION("func", CallConvention::Cdecl);
    ADD_CONVENTION("cdecl", CallConvention::Cdecl);
    ADD_CONVENTION("stdcall", CallConvention::Stdcall);
    ADD_CONVENTION("fastcall", CallConvention::Fastcall);
    ADD_CONVENTION("thiscall", CallConvention::Thiscall);

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

static void RegisterPrimitiveType(InstanceData *instance, const char *name, PrimitiveKind primitive,
                                  int16_t size, int16_t align)
{
    RG_ASSERT(align <= size);

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
    RegisterPrimitiveType(instance, "char16", PrimitiveKind::Int16, 2, 2);
    RegisterPrimitiveType(instance, "char16_t", PrimitiveKind::Int16, 2, 2);
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
    RegisterPrimitiveType(instance, "string16", PrimitiveKind::String16, RG_SIZE(void *), alignof(void *));

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
#ifdef _WIN32
    if (stack.ptr) {
        VirtualFree(stack.ptr, 0, MEM_RELEASE);
    }
    if (heap.ptr) {
        VirtualFree(heap.ptr, 0, MEM_RELEASE);
    }
#else
    if (stack.ptr) {
        munmap(stack.ptr, stack.len);
    }
    if (heap.ptr) {
        munmap(heap.ptr, heap.len);
    }
#endif
}

InstanceData::~InstanceData()
{
    for (InstanceMemory *mem: memories) {
        delete mem;
    }
}

template <typename Func>
static void SetExports(Napi::Env env, Func func)
{
    func("config", Napi::Function::New(env, GetSetConfig));

    func("struct", Napi::Function::New(env, CreatePaddedStructType));
    func("pack", Napi::Function::New(env, CreatePackedStructType));
    func("handle", Napi::Function::New(env, CreateHandleType));
    func("pointer", Napi::Function::New(env, CreatePointerType));
    func("array", Napi::Function::New(env, CreateArrayType));
    func("callback", Napi::Function::New(env, CreateCallbackType));

    func("sizeof", Napi::Function::New(env, GetTypeSize));
    func("alignof", Napi::Function::New(env, GetTypeAlign));
    func("introspect", Napi::Function::New(env, GetTypeDefinition));

    func("load", Napi::Function::New(env, LoadSharedLibrary));

    func("in", Napi::Function::New(env, MarkIn));
    func("out", Napi::Function::New(env, MarkOut));
    func("inout", Napi::Function::New(env, MarkInOut));

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
    SetValue(env, target, "internal", Napi::Boolean::New(env_cxx, true));
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
    exports.Set("internal", Napi::Boolean::New(env, false));

    return exports;
}

#endif

#if NODE_WANT_INTERNALS
    NODE_MODULE_CONTEXT_AWARE_INTERNAL(koffi, InitInternal);
#else
    NODE_API_MODULE(koffi, InitModule);
#endif
