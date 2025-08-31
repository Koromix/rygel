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

K_PUSH_NO_WARNINGS
#include <jsapi.h>
#include <js/CompilationAndEvaluation.h>
#include <js/Conversions.h>
#include <js/ErrorReport.h>
#include <js/Initialization.h>
#include <js/SourceText.h>
K_POP_NO_WARNINGS

namespace K {

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
