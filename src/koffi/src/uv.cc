// SPDX-License-Identifier: MIT

#include "uv.hh"
#include "util.hh"

#include <uv.h>

namespace K {

Napi::Function PollHandle::Define(Napi::Env env)
{
    return DefineClass(env, "PollHandle", {
        InstanceMethod("start", &PollHandle::Start),
        InstanceMethod("stop", &PollHandle::Stop),
        InstanceMethod("close", &PollHandle::Close),
        InstanceMethod("unref", &PollHandle::Unref),
        InstanceMethod("ref", &PollHandle::Ref)
    });
}

PollHandle::PollHandle(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<PollHandle>(info), env(info.Env())
{
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    if (info.Length() < 1 || !info[0].IsNumber()) {
        ThrowError<Napi::Error>(env, "Expected 1 argument, got %1", info.Length());
        return;
    }
    if (!info[0].IsNumber()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for descriptor, expected number", GetValueType(instance, info[0]));
        return;
    }

    int fd = info[0].As<Napi::Number>().Int32Value();

    uv_loop_t *loop = nullptr;
    if (napi_get_uv_event_loop(env, &loop) != napi_ok || !loop) {
        ThrowError<Napi::Error>(env, "napi_get_uv_event_loop() failed");
        return;
    }

    // We would store it inside the class, but the definition of uv_poll_t involves windows.h...
    // and we won't want that on Windows. Heap allocation it is!
    // Also, it may have to outlive the object, because uv_close() is asynchronous.
    handle = new uv_poll_t();

    if (int ret = uv_poll_init_socket(loop, handle, (uv_os_sock_t)fd); ret != 0) {
        ThrowError<Napi::Error>(env, "Failed to init UV poll: %1", uv_strerror(ret));
        return;
    }

    handle->data = this;
}

void PollHandle::Start(const Napi::CallbackInfo &info)
{
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    bool has_opts = (info.Length() >= 2 && info[0].IsObject());

    if (info.Length() < 1u + has_opts) {
        ThrowError<Napi::TypeError>(env, "Expected 1 to 2 arguments, got %1", info.Length());
        return;
    }
    if (!info[0u + has_opts].IsFunction()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for callback, expected function", GetValueType(instance, info[0u + has_opts]));
        return;
    }

    int events = 0;
    Napi::Function cb = info[0 + has_opts].As<Napi::Function>();

    if (has_opts) {
        Napi::Object opts = has_opts ? info[0].As<Napi::Object>() : Napi::Object::New(env);

        events |= opts.Get("readable").ToBoolean() ? UV_READABLE : 0;
        events |= opts.Get("writable").ToBoolean() ? UV_WRITABLE : 0;
        events |= opts.Get("disconnect").ToBoolean() ? UV_DISCONNECT : 0;
    } else {
        events = UV_READABLE;
    }

    callback.Reset(cb, 1);

    if (int ret = uv_poll_start(handle, events, &PollHandle::OnPoll); ret != 0) {
        callback.Reset();
        ThrowError<Napi::Error>(env, "Failed to start UV poll: %1", uv_strerror(ret));
    }
}

void PollHandle::Stop(const Napi::CallbackInfo &)
{
    uv_poll_stop(handle);
    callback.Reset();
}

void PollHandle::Close(const Napi::CallbackInfo &)
{
    Close();
    callback.Reset();
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
        delete handle;
    };

    uv_poll_stop(handle);
    uv_close((uv_handle_t *)handle, release);

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

    napi_value args[] = { Napi::Number::New(env, status), obj };
    poll->callback.Call(poll->Value(), K_LEN(args), args);
}

Napi::Value Poll(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    InstanceData *instance = env.GetInstanceData<InstanceData>();

    bool has_opts = (info.Length() >= 3 && info[1].IsObject());

    if (info.Length() < 2u + has_opts) {
        ThrowError<Napi::TypeError>(env, "Expected 2 to 3 arguments, got %1", info.Length());
        return env.Null();
    }

    if (!info[0].IsNumber()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for descriptor, expected number", GetValueType(instance, info[0]));
        return env.Null();
    }
    if (!info[1 + has_opts].IsFunction()) {
        ThrowError<Napi::TypeError>(env, "Unexpected %1 value for callback, expected function", GetValueType(instance, info[1 + has_opts]));
        return env.Null();
    }

    int fd = info[0].As<Napi::Number>().Int32Value();

    Napi::Function ctor = PollHandle::Define(env);
    Napi::Object inst = ctor.New({ Napi::Number::New(env, fd) });
    Napi::Function start = inst.Get("start").As<Napi::Function>();

    if (env.IsExceptionPending()) [[unlikely]]
        return env.Null();

    if (has_opts) {
        Napi::Value opts = info[1];
        Napi::Function cb = info[2].As<Napi::Function>();

        start.Call(inst, { opts, cb });
    } else {
        Napi::Function cb = info[1].As<Napi::Function>();
        start.Call(inst, { cb });
    }

    return inst;
}

}
