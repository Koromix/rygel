// SPDX-License-Identifier: MIT

#pragma once

#include "lib/native/base/base.hh"

#include <napi.h>

struct uv_poll_s;
typedef struct uv_poll_s uv_poll_t;

namespace K {

class Poll: public Napi::ObjectWrap<Poll> {
    Napi::Env env = nullptr;

    uv_poll_t *handle = nullptr;
    Napi::FunctionReference callback;

public:
    static Napi::Function Define(Napi::Env env);

    Poll(const Napi::CallbackInfo &info);
    ~Poll() { Close(); }

    void Start(const Napi::CallbackInfo &info);
    void Stop(const Napi::CallbackInfo &info);
    void Close(const Napi::CallbackInfo &info);
    void Ref(const Napi::CallbackInfo &info);
    void Unref(const Napi::CallbackInfo &info);

private:
    void Close();

    static void OnPoll(uv_poll_t *handle, int status, int events);
};

Napi::Value Watch(const Napi::CallbackInfo &info);

}
