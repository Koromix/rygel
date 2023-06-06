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
#include "vendor/webkit/include/JavaScriptCore/JavaScript.h"

namespace RG {

class AutoString {
    JSStringRef ref;

public:
    AutoString(const char *str) { ref = JSStringCreateWithUTF8CString(str); }
    ~AutoString() { JSStringRelease(ref); }

    operator JSStringRef() const { return ref; }
};

static bool PrintValue(JSContextRef ctx, JSValueRef value, JSValueRef *ex)
{
    JSStringRef str = nullptr;

    if (JSValueIsString(ctx, value)) {
        str = (JSStringRef)value;
        JSStringRetain(str);
    } else {
        str = JSValueToStringCopy(ctx, value, ex);
        if (!str)
            return false;
    }
    RG_DEFER { JSStringRelease(str); };

    Size max = JSStringGetMaximumUTF8CStringSize(str);
    Span<char> buf = AllocateSpan<char>(nullptr, max);
    RG_DEFER { ReleaseSpan(nullptr, buf); };

    Size len = JSStringGetUTF8CString(str, buf.ptr, buf.len) - 1;
    RG_ASSERT(len >= 0);

    stdout_st.Write(buf.Take(0, len));

    return true;
}

static inline bool IsNullOrUndefined(JSContextRef ctx, JSValueRef value)
{
    return JSValueIsNull(ctx, value) || JSValueIsUndefined(ctx, value);
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    // Options
    const char *filename_or_code;
    bool is_code = false;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 [options] <file>
       %1 [options] -c <code>

Options:
    %!..+-c, --command%!0                Run code directly from argument)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-c", "--command")) {
                is_code = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        filename_or_code = opt.ConsumeNonOption();
    }

    if (!filename_or_code) {
        LogError("No %1 provided", is_code ? "command" : "filename");
        return 1;
    }

    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    if (!ctx) {
        LogError("Failed to create JS context");
        return 1;
    }
    RG_DEFER { JSGlobalContextRelease(ctx); };

    // Expose utility functions
    {
        JSObjectRef global = JSContextGetGlobalObject(ctx);

        JSObjectRef func = JSObjectMakeFunctionWithCallback(ctx, AutoString("print"),
                                                            [](JSContextRef ctx, JSObjectRef, JSObjectRef,
                                                               size_t argc, const JSValueRef *argv, JSValueRef *ex) -> JSValueRef {
            for (Size i = 0; i < (Size)argc; i++) {
                if (!PrintValue(ctx, argv[i], ex))
                    return nullptr;
            }
            PrintLn();

            return JSValueMakeUndefined(ctx);
        });

        JSObjectSetProperty(ctx, global, AutoString("print"), func, kJSPropertyAttributeNone, nullptr);
    }

    // Load code
    HeapArray<char> code;
    if (is_code) {
        code.Append(filename_or_code);
    } else {
        if (ReadFile(filename_or_code, Megabytes(8), &code) < 0)
            return 1;
    }
    code.Grow(1);
    code.ptr[code.len] = 0;

    // Execute code
    JSValueRef ret = JSEvaluateScript(ctx, AutoString(code.ptr), nullptr, nullptr, 1, nullptr);
    if (!ret)
        return 1;

    if (!IsNullOrUndefined(ctx, ret)) {
        if (!PrintValue(ctx, ret, nullptr))
            return 1;
        PrintLn();
    }

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
