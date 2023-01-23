// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/libcc/libcc.hh"
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

    if (RG_UNLIKELY(info.Length() < 1)) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }

    char str[64];
    {
        napi_status status = napi_get_value_string_utf8(env, info[0], str, RG_SIZE(str), nullptr);

        if (RG_UNLIKELY(status != napi_ok)) {
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

NODE_API_MODULE(koffi, InitModule);
