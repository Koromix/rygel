// SPDX-License-Identifier: MIT

#include "lib/native/base/base.hh"
#include "type.hh"
#include "util.hh"
#include "uv.hh"

#include <napi.h>

namespace K {

Napi::Function PollHandle::InitClass(InstanceData *instance)
{
    Napi::Env env = instance->env;

    // node-addon-api wants std::vector
    std::vector<Napi::ClassPropertyDescriptor<PollHandle>> properties = {
        InstanceMethod("start", &PollHandle::Start, napi_default, instance),
        InstanceMethod("stop", &PollHandle::Stop, napi_default, instance),
        InstanceMethod("close", &PollHandle::Close, napi_default, instance),
        InstanceMethod("unref", &PollHandle::Unref, napi_default, instance),
        InstanceMethod("ref", &PollHandle::Ref, napi_default, instance)
    };

    if (Napi::Value dispose = env.RunScript("Symbol.dispose"); !IsNullOrUndefined(env, dispose)) {
        Napi::ClassPropertyDescriptor<PollHandle> prop = InstanceMethod(dispose.As<Napi::Symbol>(), &PollHandle::Close);
        properties.push_back(prop);
    }

    Napi::Function constructor = DefineClass(env, "PollHandle", properties, instance);
    return constructor;
}

PollHandle::PollHandle(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<PollHandle>(info), env(info.Env())
{
    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 arguments, got %1", info.Length());
        return;
    }

    int fd = 0;
    uv_loop_t *loop = nullptr;

    // The descriptor is wrapped in an external object instead of an integer to prevent
    // JS code from trying to create PollHandle objects with new PollHandle.
    if (void *ptr = nullptr; napi_get_value_external(env, info[0], &ptr) == napi_ok) {
        fd = (int)(intptr_t)ptr;
    } else {
        ThrowError<Napi::Error>(env, "Poll handles cannot be constructed manually");
        return;
    }
    if (napi_get_uv_event_loop(env, &loop) != napi_ok || !loop) {
        ThrowError<Napi::Error>(env, "Failed to access Node event loop");
        return;
    }

    // We would store it inside the class, but the definition of uv_poll_t involves windows.h...
    // and we won't want that on Windows. Heap allocation it is!
    // Also, it may have to outlive the object, because uv_close() is asynchronous.
    {
        size_t size = uv_handle_size(UV_POLL);

        handle = (uv_poll_t *)AllocateRaw(nullptr, size);
        MemSet(handle, 0, size);
    }

    if (int ret = uv_poll_init_socket(loop, handle, (uv_os_sock_t)fd); ret != 0) {
        ThrowError<Napi::Error>(env, "Failed to init UV poll: %1", uv_strerror(ret));
        return;
    }

    handle->data = this;
}

void PollHandle::Start(const Napi::CallbackInfo &info)
{
    InstanceData *instance = (InstanceData *)info.Data();

    bool has_opts = (info.Length() >= 2 && info[0].IsObject());

    if (info.Length() < 1u + has_opts) {
        ThrowError<Napi::TypeError>(env, "Expected 1 to 2 arguments, got %1", info.Length());
        return;
    }
    if (!info[0u + has_opts].IsFunction()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for callback, expected function", GetValueType(instance, info[0u + has_opts]));
        return;
    }

    napi_value opts = has_opts ? info[0] : nullptr;
    napi_value func = info[0 + has_opts];

    Start(opts, func);
}

bool PollHandle::Start(napi_value opts, napi_value func)
{
    int events = 0;

    if (opts) {
        Napi::Object obj(env, opts);

        events |= obj.Get("readable").ToBoolean() ? UV_READABLE : 0;
        events |= obj.Get("writable").ToBoolean() ? UV_WRITABLE : 0;
        events |= obj.Get("disconnect").ToBoolean() ? UV_DISCONNECT : 0;
    } else {
        events = UV_READABLE;
    }

    callback = Napi::Persistent(Napi::Function(env, func));
    K_DEFER_N(err_guard) { callback.Reset(); };

    if (int ret = uv_poll_start(handle, events, &PollHandle::OnPoll); ret != 0) {
        ThrowError<Napi::Error>(env, "Failed to start UV poll: %1", uv_strerror(ret));
        return false;
    }

    err_guard.Disable();
    return true;
}

void PollHandle::Finalize(Napi::BasicEnv env)
{
    node_api_delete_reference(env, *this);
    SuppressDestruct();

    Close();
}

void PollHandle::Stop(const Napi::CallbackInfo &)
{
    uv_poll_stop(handle);
}

void PollHandle::Close(const Napi::CallbackInfo &)
{
    Close();
}

void PollHandle::Ref(const Napi::CallbackInfo &)
{
    uv_ref((uv_handle_t *)handle);
}

void PollHandle::Unref(const Napi::CallbackInfo &)
{
    uv_unref((uv_handle_t *)handle);
}

void PollHandle::Close()
{
    if (!handle)
        return;

    const auto release = [](uv_handle_t *ptr) {
        uv_poll_t *handle = (uv_poll_t *)ptr;
        ReleaseRaw(nullptr, handle, -1);
    };

    uv_poll_stop(handle);
    uv_close((uv_handle_t *)handle, release);

    callback.Reset();

    handle = nullptr;
}

void PollHandle::OnPoll(uv_poll_t *h, int status, int events)
{
    PollHandle *poll = (PollHandle *)h->data;

    if (poll->callback.IsEmpty()) [[unlikely]]
        return;

    Napi::Env env = poll->env;
    Napi::HandleScope scope(env);

    Napi::Object obj = Napi::Object::New(env);

    obj.Set("readable", !!(events & UV_READABLE));
    obj.Set("writable", !!(events & UV_WRITABLE));
    obj.Set("disconnect", !!(events & UV_DISCONNECT));

    napi_value args[] = { NewInt(env, (int32_t)status), obj };
    poll->callback.Call(poll->Value(), K_LEN(args), args);
}

napi_value Poll(napi_env env, napi_callback_info info)
{
    napi_value args[3];
    size_t count = 3;
    InstanceData *instance;

    NAPI_OK(napi_get_cb_info(env, info, &count, args, nullptr, (void **)&instance));

    bool has_opts = (count >= 3) && IsObject(env, args[1]);

    if (count < 2 + has_opts) {
        ThrowError<Napi::TypeError>(env, "Expected %1 arguments, got %2", 2 + has_opts, count);
        return GetNull(env);
    }

    int fd = 0;
    napi_value opts = has_opts ? args[1] : nullptr;
    napi_value func = args[1 + has_opts];

    if (!TryNumber(env, args[0], &fd)) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for descriptor, expected number", GetValueType(instance, args[0]));
        return GetNull(env);
    }
    if (GetKindOf(env, func) != napi_function) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for callback, expected function", GetValueType(instance, args[1 + has_opts]));
        return GetNull(env);
    }

    Napi::External<void> external = Napi::External<void>::New(env, (void *)(intptr_t)fd);
    Napi::Object inst = instance->construct_poll.New({ external });
    PollHandle *handle = PollHandle::Unwrap(inst);

    if (!handle->IsValid())
        return GetNull(env);
    if (!handle->Start(opts, func))
        return GetNull(env);

    return inst;
}

}
