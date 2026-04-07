// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include <napi.h>

namespace K {

static thread_local Napi::ArrayBuffer cmp_buffer;
static thread_local int cmp_size;
static thread_local Napi::Function cmp_func;

template <typename T, typename... Args>
void ThrowError(Napi::Env env, const char *msg, Args... args)
{
    char buf[1024];
    Fmt(buf, msg, args...);

    auto err = T::New(env, buf);
    err.ThrowAsJavaScriptException();
}

static int RunCompareFunction(const void *ptr1, const void *ptr2)
{
    Napi::ArrayBuffer buffer = cmp_buffer;
    int size = cmp_size;
    Napi::Function cmp = cmp_func;

    Napi::Env env = buffer.Env();

    napi_value args[] = {
        Napi::DataView::New(env, cmp_buffer, (uint8_t *)ptr1 - (uint8_t *)cmp_buffer.Data(), size),
        Napi::DataView::New(env, cmp_buffer, (uint8_t *)ptr2 - (uint8_t *)cmp_buffer.Data(), size)
    };

    Napi::Value ret = cmp.Call(K_LEN(args), args);
    int ret32 = ret.As<Napi::Number>().Int32Value();

    return ret32;
}

static Napi::Value RunQsort(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 4) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Expected 4 arguments, got %1", info.Length());
        return env.Null();
    }
    if (!info[0].IsTypedArray()) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Unexpected type for base, expected Buffer or TypedArray");
        return env.Null();
    }
    if (!info[1].IsNumber()) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Unexpected type for nmemb, expected number");
        return env.Null();
    }
    if (!info[2].IsNumber()) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Unexpected type for size, expected number");
        return env.Null();
    }
    if (!info[3].IsFunction()) [[unlikely]] {
        ThrowError<Napi::TypeError>(env, "Unexpected type for comparator, expected function");
        return env.Null();
    }

    Napi::TypedArray array = info[0].As<Napi::TypedArray>();
    int nmemb = info[1].As<Napi:: Number>().Int32Value();
    int size = info[2].As<Napi:: Number>().Int32Value();
    Napi::Function cmp = info[3].As<Napi::Function>();

    uint8_t *base = (uint8_t *)array.ArrayBuffer().Data() + array.ByteOffset();

    cmp_buffer = array.ArrayBuffer();
    cmp_size = size;
    cmp_func = cmp;

    qsort(base, nmemb, size, RunCompareFunction);

    return env.Null();
}

}

static Napi::Object InitModule(Napi::Env env, Napi::Object exports)
{
    using namespace K;

    exports.Set("qsort", Napi::Function::New(env, RunQsort));

    return exports;
}

NODE_API_MODULE(rand_napi, InitModule);
