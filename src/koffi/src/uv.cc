// SPDX-License-Identifier: MIT

#include "uv.hh"

namespace K {

Napi::Function Poll::Define(Napi::Env env)
{
    return DefineClass(env, "Poll", {
        InstanceMethod("start", &Poll::Start),
        InstanceMethod("stop",  &Poll::Stop),
        InstanceMethod("close", &Poll::Close),
        InstanceMethod("unref", &Poll::Unref),
        InstanceMethod("ref",   &Poll::Ref),
    });
}

Poll::Poll(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<Poll>(info), env(info.Env())
{
    if (info.Length() != 1 || !info[0].IsNumber()) {
        ThrowError<Napi::Error>(env, "Poll only accepts the file descripter as number");
        return;
    }

    const int fd = info[0].As<Napi::Number>().Int32Value();

    // get the main event loop object
    uv_loop_t* loop = nullptr;
    napi_status st = napi_get_uv_event_loop(env, &loop);
    if (st != napi_ok || !loop) {
        ThrowError<Napi::Error>(env, "napi_get_uv_event_loop failed");
        return;
    }

    int r = uv_poll_init(loop, &uv_poll, fd);
    if (r != 0) {
        Napi::Error::New(env, uv_strerror(r)).ThrowAsJavaScriptException();
        return;
    }

    uv_poll.data = this;
}

void Poll::Start(const Napi::CallbackInfo& info)
{
    Napi::Object opts = info.Length() >= 1 && info[0].IsObject()
        ? info[0].As<Napi::Object>()
        : Napi::Object::New(env);

    if (info.Length() != 2) {
        ThrowError<Napi::Error>(env, "start expects two parameters: opts, cb");
        return;
    }
    if (!info[1].IsFunction()) {
        ThrowError<Napi::Error>(env, "start expects a function as second parameter");
        return;
    }

    int events = 0;
    if (opts.Has("readable") && opts.Get("readable").ToBoolean()) {
        events |= UV_READABLE;
    }
    if (opts.Has("writable") && opts.Get("writable").ToBoolean()) {
        events |= UV_WRITABLE;
    }
    if (opts.Has("disconnect") && opts.Get("disconnect").ToBoolean()) {
        events |= UV_DISCONNECT;
    }

    cb.Reset(info[1].As<Napi::Function>(), 1);

    int r = uv_poll_start(&uv_poll, events, &Poll::OnPoll);
    if (r != 0) {
        Napi::Error::New(env, uv_strerror(r)).ThrowAsJavaScriptException();
        cb.Reset();
    }
}

void Poll::Stop(const Napi::CallbackInfo&)
{
    if (uv_is_active(reinterpret_cast<uv_handle_t*>(&uv_poll))) {
        uv_poll_stop(&uv_poll);
    }
    cb.Reset();
}

void Poll::Close(const Napi::CallbackInfo& info)
{
    Stop(info);
    if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(&uv_poll))) {
        uv_close(reinterpret_cast<uv_handle_t*>(&uv_poll), nullptr);
    }
}

void Poll::Unref(const Napi::CallbackInfo&)
{
    uv_unref(reinterpret_cast<uv_handle_t*>(&uv_poll));
}

void Poll::Ref(const Napi::CallbackInfo&)
{
    uv_ref(reinterpret_cast<uv_handle_t*>(&uv_poll));
}

void Poll::OnPoll(uv_poll_t* h, int status, int events)
{
    auto* self = static_cast<Poll*>(h->data);
    if (!self)
        return;

    Napi::Env env = self->env;
    Napi::HandleScope scope(env);

    Napi::Object ev = Napi::Object::New(env);

    ev.Set("status", status);
    ev.Set("readable", (events & UV_READABLE) != 0);
    ev.Set("writable", (events & UV_WRITABLE) != 0);
    ev.Set("disconnect", (events & UV_DISCONNECT) != 0);

    if (!self->cb.IsEmpty()) {
        self->cb.Call(self->Value(), { ev });
    }
}

Napi::Value Watch(const Napi::CallbackInfo& info)
{
    Napi::Env env = info.Env();
    if (info.Length() != 3) {
        ThrowError<Napi::Error>(env, "watch expects three parameters: fd, opts, cb");
        return env.Null();
    }
    if (!info[0].IsNumber()) {
        ThrowError<Napi::Error>(env, "watch expects a number as second parameter");
        return env.Null();
    }
    if (!info[2].IsFunction()) {
        ThrowError<Napi::Error>(env, "watch expects a function as third parameter");
        return env.Null();
    }

    int fd = info[0].As<Napi::Number>().Int32Value();
    Napi::Object opts = info[1].IsObject() ? info[1].As<Napi::Object>() : Napi::Object::New(env);
    Napi::Function cb = info[2].As<Napi::Function>();

    Napi::Function ctor = Poll::Define(env);
    Napi::Object inst = ctor.New({ Napi::Number::New(env, fd) });
    inst.Get("start").As<Napi::Function>().Call(inst, { opts, cb });

    return inst;
}

}
