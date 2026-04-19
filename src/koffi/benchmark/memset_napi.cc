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

static napi_value RunMemset(napi_env env, napi_callback_info info)
{
    napi_value args[6];
    size_t count = 6;

    napi_status status = napi_get_cb_info(env, info, &count, args, nullptr, nullptr);
    K_ASSERT(status == napi_ok);

    if (count < 3) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 3 arguments, got %1", count);
        return Napi::Env(env).Null();
    }
    if (!Napi::Value(env, args[0]).IsTypedArray()) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected TypedArray pointer");
        return Napi::Env(env).Null();
    }
    if (!Napi::Value(env, args[1]).IsNumber()) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected number for value");
        return Napi::Env(env).Null();
    }
    if (!Napi::Value(env, args[2]).IsNumber()) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected number for length");
        return Napi::Env(env).Null();
    }

    Napi::TypedArray buf = Napi::TypedArray(env, args[0]);
    int value = Napi::Number(env, args[1]).Int32Value();
    size_t len = (size_t)Napi::Number(env, args[2]).Int64Value();

    void *ptr = (uint8_t *)buf.ArrayBuffer().Data() + buf.ByteOffset();
    void *ret = memset(ptr, value, len);

    return Napi::BigInt::New(env, (uint64_t)(uintptr_t)ret);
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

    exports.Set("memset", WrapFunction(env, "memset", RunMemset));

    return exports;
}

NODE_API_MODULE(atoi_napi, InitModule);
