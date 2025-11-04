// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/native/base/base.hh"
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

static Napi::Value RunAtoi(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }

    char str[64];
    {
        napi_status status = napi_get_value_string_utf8(env, info[0], str, K_SIZE(str), nullptr);

        if (status != napi_ok) [[unlikely]] {
            if (status == napi_string_expected) {
                ThrowError<Napi::TypeError>(env, "Unexpected value for str, expected string");
            } else {
                ThrowError<Napi::TypeError>(env, "Failed to read JS string");
            }
            return env.Null();
        }
    }

    int value = atoi(str);

    return Napi::Number::New(env, value);
}

}

static Napi::Object InitModule(Napi::Env env, Napi::Object exports)
{
    using namespace K;

    exports.Set("atoi", Napi::Function::New(env, RunAtoi));

    return exports;
}

NODE_API_MODULE(atoi_napi, InitModule);
