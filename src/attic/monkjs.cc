// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "src/core/wrap/spidermonkey.hh"

namespace RG {

static bool DoPrint(JSContext *ctx, unsigned argc, JS::Value *vp)
{
    js_Instance *instance = js_Instance::FromContext(ctx);
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    for (unsigned int i = 0; i < args.length(); i++) {
        JS::HandleValue arg = args[i];

        if (!instance->PrintValue(arg))
            return false;
    }
    PrintLn();

    args.rval().setUndefined();
    return true;
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    // Options
    const char *filename_or_code;
    bool is_code = false;

    const auto print_usage = [](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 [option...] filename
       %1 [option...] -c code%!0

Options:

    %!..+-c, --command%!0                  Run code directly from argument)", FelixTarget);
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

    std::unique_ptr<js_Instance> instance = js_CreateInstance();
    if (!instance)
        return 1;

    instance->AddFunction("print", DoPrint, 0, 0);

    JS::RootedValue ret(*instance);
    if (!instance->Evaluate(code, is_code ? "<inline>" : filename_or_code, 1, &ret))
        return 1;

    if (!ret.isNull() && !ret.isUndefined()) {
        instance->PrintValue(ret);
        PrintLn();
    }

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
