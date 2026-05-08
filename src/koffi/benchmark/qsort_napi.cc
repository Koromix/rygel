// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include <napi.h>

namespace K {

static thread_local napi_env cmp_env;
static thread_local napi_value cmp_func;
static thread_local napi_value cmp_recv;

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
    napi_env env = cmp_env;
    napi_value cmp = cmp_func;
    napi_value recv = cmp_recv;

    napi_value args[2];
    napi_create_bigint_uint64(env, (uint64_t)(uintptr_t)ptr1, &args[0]);
    napi_create_bigint_uint64(env, (uint64_t)(uintptr_t)ptr2, &args[1]);

    napi_value ret;
    napi_status status = napi_call_function(env, recv, cmp, K_LEN(args), args, &ret);
    K_ASSERT(status == napi_ok);

    int ret32;
    napi_get_value_int32(env, ret, &ret32);

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

    cmp_env = env;
    cmp_func = cmp;
    cmp_recv = env.Undefined();

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
