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

static Napi::Value RunMemset(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 3) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 3 arguments, got %1", info.Length());
        return env.Null();
    }
    if (!info[0].IsTypedArray()) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected TypedArray pointer", info.Length());
        return env.Null();
    }
    if (!info[1].IsNumber()) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected number for value", info.Length());
        return env.Null();
    }
    if (!info[2].IsNumber()) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected number for length", info.Length());
        return env.Null();
    }

    Napi::TypedArray buf = info[0].As<Napi::TypedArray>();
    int value = (int)info[1].As<Napi::Number>().Int32Value();
    size_t len = (size_t)info[2].As<Napi::Number>().Int64Value();

    void *ptr = (uint8_t *)buf.ArrayBuffer().Data() + buf.ByteOffset();
    void *ret = memset(ptr, value, len);

    return Napi::BigInt::New(env, (uint64_t)(uintptr_t)ret);
}

}

static Napi::Object InitModule(Napi::Env env, Napi::Object exports)
{
    using namespace K;

    exports.Set("memset", Napi::Function::New(env, RunMemset));

    return exports;
}

NODE_API_MODULE(atoi_napi, InitModule);
