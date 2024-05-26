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

#include "src/core/base/base.hh"

RG_PUSH_NO_WARNINGS
#include <jsapi.h>
#include <js/CompilationAndEvaluation.h>
#include <js/Conversions.h>
#include <js/ErrorReport.h>
#include <js/Initialization.h>
#include <js/SourceText.h>
RG_POP_NO_WARNINGS

namespace RG {

class js_Instance {
    JSContext *ctx = nullptr;

    JS::RootedObject global;
    JSAutoRealm ar;

public:
    js_Instance(JSContext *ctx, JSObject *global) : ctx(ctx), global(ctx, global), ar(ctx, global) {}
    ~js_Instance() { JS_DestroyContext(ctx); }

    static js_Instance *FromContext(JSContext *ctx) { return (js_Instance *)JS_GetContextPrivate(ctx); }

    JSContext *GetContext() const { return ctx; }
    operator JSContext *() const { return ctx; }

    bool AddFunction(const char *name, JSNative call, int nargs, unsigned int attrs);

    bool Evaluate(Span<const char> code, const char *filename, int line, JS::RootedValue *out_ret);

    bool PrintString(JS::HandleString str);
    bool PrintValue(JS::HandleValue value);
};

std::unique_ptr<js_Instance> js_CreateInstance();

}
