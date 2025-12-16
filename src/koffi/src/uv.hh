// SPDX-License-Identifier: MIT

#pragma once

#include <napi.h>
#include <uv.h>

namespace K {

class Poll : public Napi::ObjectWrap<Poll> {
    static void OnPoll(uv_poll_t* handle, int status, int events);

    Napi::Env env{nullptr};
    uv_poll_t uv_poll{};
    Napi::FunctionReference cb;

public:
    static Napi::Function Define(Napi::Env env);

    explicit Poll(const Napi::CallbackInfo& info);

    void Start (const Napi::CallbackInfo& info);
    void Stop  (const Napi::CallbackInfo& info);
    void Close (const Napi::CallbackInfo& info);
    void Unref (const Napi::CallbackInfo& info);
    void Ref   (const Napi::CallbackInfo& info);
};

Napi::Value Watch(const Napi::CallbackInfo& info);

}
