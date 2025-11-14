// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include <napi.h>

namespace K {

template <typename T, typename... Args>
void ThrowError(Napi::Env env, const char *msg, Args... args)
{
    char buf[1024];
    Fmt(buf, msg, args...);

    auto err = T::New(env, buf);
    err.ThrowAsJavaScriptException();
}

static Napi::Value RunSrand(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }
    if (!info[0].IsNumber()) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Unexpected type for seed, expected number");
        return env.Null();
    }

    unsigned int seed = (info[0].As<Napi::Number>()).Uint32Value();
    srand(seed);

    return env.Null();
}

static Napi::Value RunRand(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    int rnd = rand();

    return Napi::Number::New(env, rnd);
}

}

static Napi::Object InitModule(Napi::Env env, Napi::Object exports)
{
    using namespace K;

    exports.Set("srand", Napi::Function::New(env, RunSrand));
    exports.Set("rand", Napi::Function::New(env, RunRand));

    return exports;
}

NODE_API_MODULE(rand_napi, InitModule);
