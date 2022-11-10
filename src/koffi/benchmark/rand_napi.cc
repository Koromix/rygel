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

static Napi::Value RunSrand(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (RG_UNLIKELY(info.Length() < 1)) {
        ThrowError<Napi::TypeError>(env, "Expected 1 argument, got %1", info.Length());
        return env.Null();
    }
    if (RG_UNLIKELY(!info[0].IsNumber())) {
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
    using namespace RG;

    exports.Set("srand", Napi::Function::New(env, RunSrand));
    exports.Set("rand", Napi::Function::New(env, RunRand));

    return exports;
}

NODE_API_MODULE(koffi, InitModule);
