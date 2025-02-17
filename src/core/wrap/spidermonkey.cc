// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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
#include "spidermonkey.hh"

namespace RG {

static JSClass GlobalClass = {
    .name = "Global",
    .flags = JSCLASS_GLOBAL_FLAGS,
    .cOps = &JS::DefaultGlobalClassOps
};

static bool InitEngine()
{
    static bool success = []() {
        if (!JS_Init()) {
            LogError("Failed to initialize JS engine");
            return false;
        }
        atexit(JS_ShutDown);

        return true;
    }();

    return success;
}

bool js_Instance::AddFunction(const char *name, JSNative call, int nargs, unsigned int attrs)
{
    if (!JS_DefineFunction(ctx, global, name, call, (unsigned int)nargs, attrs)) {
        LogError("Failed to add JS native function");
        return false;
    }

    return true;
}

static void ReportAndClearException(JSContext *ctx)
{
    JS::ExceptionStack stack(ctx);
    if (!JS::StealPendingExceptionStack(ctx, &stack)) {
        LogError("Uncatchable exception thrown, out of memory or something");
        return;
    }

    JS::ErrorReportBuilder builder(ctx);
    if (!builder.init(ctx, stack, JS::ErrorReportBuilder::WithSideEffects)) {
        LogError("Failed to build error report");
        return;
    }

    JSErrorReport *report = builder.report();
    const char *filename = report->filename.c_str();
    const char *message = report->message().c_str();

    RG::PushLogFilter([&](LogLevel level, const char *, const char *msg, FunctionRef<LogFunc> func) {
        char ctx[1024];
        Fmt(ctx, "%1(%2): ", filename, report->lineno);
        func(level, ctx, msg);
    });
    RG_DEFER { RG::PopLogFilter(); };

    LogError("%1", message);
}

bool js_Instance::Evaluate(Span<const char> code, const char *filename, int line, JS::RootedValue *out_ret)
{
    JS::CompileOptions options(ctx);
    options.setFileAndLine(filename, line);

    JS::SourceText<mozilla::Utf8Unit> source;
    if (!source.init(ctx, code.ptr, code.len, JS::SourceOwnership::Borrowed)) {
        LogError("Failed to decode code buffer");
        return false;
    }

    if (!JS::Evaluate(ctx, options, source, out_ret)) {
        ReportAndClearException(ctx);
        return false;
    }

    return true;
}

bool js_Instance::PrintString(JS::HandleString str)
{
    JS::UniqueChars chars = JS_EncodeStringToUTF8(ctx, str);
    if (!chars)
        return false;

    Span<const char> utf8 = chars.get();
    StdOut->Write(utf8);

    return true;
}

bool js_Instance::PrintValue(JS::HandleValue value)
{
    JS::RootedString str(ctx);

    if (value.isString()) {
        str = value.toString();
    } else {
        str = JS::ToString(ctx, value);
    }
    if (!str)
        return false;

    return PrintString(str);
}

std::unique_ptr<js_Instance> js_CreateInstance()
{
    if (!InitEngine())
        return nullptr;

    JSContext *ctx = JS_NewContext(JS::DefaultHeapMaxBytes);
    if (!ctx) {
        LogError("Failed to create JS context");
        return nullptr;
    }
    RG_DEFER_N(err_guard) { JS_DestroyContext(ctx); };

    if (!JS::InitSelfHostedCode(ctx)) {
        LogError("Failed to initialize JS self-hosted code");
        return nullptr;
    }

    JS::RealmOptions options;
    JSObject *global = JS_NewGlobalObject(ctx, &GlobalClass, nullptr, JS::FireOnNewGlobalHook, options);
    if (!global) {
        LogError("Failed to create JS global object");
        return nullptr;
    }

    std::unique_ptr<js_Instance> instance = std::make_unique<js_Instance>(ctx, global);
    JS_SetContextPrivate(ctx, instance.get());

    err_guard.Disable();

    return instance;
}

}
