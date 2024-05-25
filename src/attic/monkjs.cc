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

static bool PrintString(JSContext *ctx, JS::HandleString str)
{
    JS::UniqueChars chars = JS_EncodeStringToUTF8(ctx, str);
    if (!chars)
        return false;

    Span<const char> utf8 = chars.get();
    StdOut->Write(utf8);

    return true;
}

static bool PrintValue(JSContext *ctx, JS::HandleValue value)
{
    JS::RootedString str(ctx);

    if (value.isString()) {
        str = value.toString();
    } else {
        str = JS::ToString(ctx, value);
    }
    if (!str)
        return false;

    return PrintString(ctx, str);
}

static bool DoPrint(JSContext *ctx, unsigned argc, JS::Value *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    for (unsigned int i = 0; i < args.length(); i++) {
        JS::HandleValue arg = args[i];

        if (!PrintValue(ctx, arg))
            return false;
    }
    PrintLn();

    args.rval().setUndefined();
    return true;
}

static JSClass GlobalClass = {
    .name = "Global",
    .flags = JSCLASS_GLOBAL_FLAGS,
    .cOps = &JS::DefaultGlobalClassOps
};

static JSFunctionSpec functions[] = {
    JS_FN("print", DoPrint, 0, 0),
    JS_FS_END
};

static void ReportAndClearException(JSContext *ctx)
{
    JS::ExceptionStack stack(ctx);
    if (!JS::StealPendingExceptionStack(ctx, &stack)) {
        LogError("Uncatchable exception thrown, out of memory or something");
        exit(1);
    }

    JS::ErrorReportBuilder report(ctx);
    if (!report.init(ctx, stack, JS::ErrorReportBuilder::WithSideEffects)) {
        LogError("Failed to build error report");
        exit(1);
    }

    const char *str = report.toStringResult().c_str();
    LogError("Error: %1", str);
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    // Options
    const char *filename_or_code;
    bool is_code = false;

    const auto print_usage = [](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 [options] <file>
       %1 [options] -c <code>%!0

Options:
    %!..+-c, --command%!0                Run code directly from argument)", FelixTarget);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
        return 0;
    }

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-c", "--command")) {
                is_code = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        filename_or_code = opt.ConsumeNonOption();
        opt.LogUnusedArguments();
    }

    if (!filename_or_code) {
        LogError("No %1 provided", is_code ? "command" : "filename");
        return 1;
    }

    // Load code
    HeapArray<char> code;
    if (is_code) {
        code.Append(filename_or_code);
    } else {
        if (ReadFile(filename_or_code, Megabytes(8), &code) < 0)
            return 1;
    }

    if (!JS_Init()) {
        LogError("Failed to initialize JS engine");
        return 1;
    }
    RG_DEFER { JS_ShutDown(); };

    JSContext *ctx = JS_NewContext(JS::DefaultHeapMaxBytes);
    if (!ctx) {
        LogError("Failed to create JS context");
        return 1;
    }
    RG_DEFER { JS_DestroyContext(ctx); };

    if (!JS::InitSelfHostedCode(ctx)) {
        LogError("Failed to initialize JS self-hosted code");
        return 1;
    }

    JS::RealmOptions options;
    JS::RootedObject global { ctx, JS_NewGlobalObject(ctx, &GlobalClass, nullptr, JS::FireOnNewGlobalHook, options) };
    if (!global) {
        LogError("Failed to create JS global object");
        return 1;
    }

    JSAutoRealm ar(ctx, global);

    if (!JS_DefineFunctions(ctx, global, functions)) {
        LogError("Failed to define JS API");
        return 1;
    }

    // Execute code
    JS::RootedValue ret(ctx);
    {
        JS::CompileOptions options(ctx);
        options.setFileAndLine(is_code ? "<inline>" : filename_or_code, 1);

        JS::SourceText<mozilla::Utf8Unit> source;
        if (!source.init(ctx, code.ptr, code.len, JS::SourceOwnership::Borrowed)) {
            LogError("Failed to decode code buffer");
            return 1;
        }

        if (!JS::Evaluate(ctx, options, source, &ret)) {
            ReportAndClearException(ctx);
            return 1;
        }
    }

    if (!ret.isNull() && !ret.isUndefined()) {
        PrintValue(ctx, ret);
        PrintLn();
    }

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
