// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "vendor/libcc/libcc.hh"
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

    // We want maximum performance here, this is a benchmark
#if 0
    if (info.Length() < 1) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }
    if (!info[0].IsString()) {
        ThrowError<Napi::TypeError>(env, "Unexpected value for str, expected string");
        return env.Null();
    }
#endif

    std::string name = info[0].As<Napi::String>();
    int value = atoi(name.c_str());

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
