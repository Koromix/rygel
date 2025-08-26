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
#include "src/core/wrap/jscore.hh"

namespace RG {

int Main(int argc, char **argv)
{
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
        PrintLn(T("Compiler: %1"), FelixCompiler);
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

    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    RG_DEFER { JSGlobalContextRelease(ctx); };

    // Expose utility functions
    {
        JSObjectRef global = JSContextGetGlobalObject(ctx);

        js_ExposeFunction(ctx, global, "print", [](JSContextRef ctx, JSObjectRef, JSObjectRef,
                                                   size_t argc, const JSValueRef *argv, JSValueRef *ex) {
            for (Size i = 0; i < (Size)argc; i++) {
                if (!js_PrintValue(ctx, argv[i], ex, StdOut))
                    return (JSValueRef)nullptr;
            }
            PrintLn();

            return JSValueMakeUndefined(ctx);
        });
    }

    // Load code
    HeapArray<char> code;
    if (is_code) {
        code.Append(filename_or_code);
    } else {
        if (ReadFile(filename_or_code, Megabytes(8), &code) < 0)
            return 1;
    }

    // Execute code
    JSValueRef ret;
    {
        JSValueRef ex = nullptr;
        ret = JSEvaluateScript(ctx, js_AutoString(code), nullptr, nullptr, 1, &ex);

        if (!ret) {
            RG_ASSERT(ex);

            js_PrintValue(ctx, ex, nullptr, StdErr);
            PrintLn(StdErr);

            return 1;
        }
    }

    if (!js_IsNullOrUndefined(ctx, ret)) {
        js_PrintValue(ctx, ret, nullptr, StdOut);
        PrintLn();
    }

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
