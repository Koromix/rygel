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

#pragma once

#include "vendor/libcc/libcc.hh"
#include "util.hh"

#include <napi.h>

namespace RG {

struct InstanceData;
struct TypeInfo;
struct FunctionInfo;

class PrototypeParser {
    Napi::Env env;
    InstanceData *instance;

    // All these members are relevant to the current parse only, and get resetted each time
    HeapArray<Span<const char>> tokens;
    Size offset;
    bool valid;

public:
    PrototypeParser(Napi::Env env) : env(env), instance(env.GetInstanceData<InstanceData>()) {}

    bool Parse(Napi::String proto, FunctionInfo *func);

private:
    void Tokenize(const char *str);

    const TypeInfo *ParseType();
    const char *ParseIdentifier();

    bool Consume(const char *expect);
    bool Match(const char *expect);

    bool IsIdentifier(Span<const char> tok) const;

    template <typename... Args>
    void MarkError(const char *fmt, Args... args)
    {
        if (valid) {
            ThrowError<Napi::Error>(env, fmt, args...);
            valid = false;
        }
        valid = false;
    }
};

}
