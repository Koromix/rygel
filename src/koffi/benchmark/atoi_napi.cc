// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "src/core/base/base.hh"
#include <napi.h>

namespace RG {

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
        napi_status status = napi_get_value_string_utf8(env, info[0], str, RG_SIZE(str), nullptr);

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
    using namespace RG;

    exports.Set("atoi", Napi::Function::New(env, RunAtoi));

    return exports;
}

NODE_API_MODULE(atoi_napi, InitModule);
