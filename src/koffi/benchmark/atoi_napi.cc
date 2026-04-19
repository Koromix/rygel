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

static napi_value RunAtoi(napi_env env, napi_callback_info info)
{
    napi_value args[6];
    size_t count = 6;

    napi_status status = napi_get_cb_info(env, info, &count, args, nullptr, nullptr);
    K_ASSERT(status == napi_ok);

    if (count < 1) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", count);
        return Napi::Env(env).Null();
    }

    char str[64];
    {
        napi_status status = napi_get_value_string_utf8(env, args[0], str, K_SIZE(str), nullptr);

        if (status != napi_ok) [[unlikely]] {
            if (status == napi_string_expected) {
                ThrowError<Napi::TypeError>(env, "Unexpected value for str, expected string");
            } else {
                ThrowError<Napi::TypeError>(env, "Failed to read JS string");
            }
            return Napi::Env(env).Null();
        }
    }

    int value = atoi(str);

    return Napi::Number::New(env, value);
}

static napi_value WrapFunction(napi_env env, const char *name, napi_callback func)
{
    napi_value value;

    napi_status status = napi_create_function(env, name, NAPI_AUTO_LENGTH, func, nullptr, &value);
    K_ASSERT(status == napi_ok);

    return value;
}

}

static Napi::Object InitModule(Napi::Env env, Napi::Object exports)
{
    using namespace K;

    exports.Set("atoi", WrapFunction(env, "atoi", RunAtoi));

    return exports;
}

NODE_API_MODULE(atoi_napi, InitModule);
