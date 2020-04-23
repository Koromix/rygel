// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "compiler.hh"
#include "debug.hh"
#include "lexer.hh"
#include "vm.hh"

namespace RG {

int Main(int argc, char **argv)
{
    // Options
    bool run_inline = false;
    bool generate_debug = true;
    const char *filename = nullptr;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: blik [options] <file>

Options:
    -i, --inline                 Run code directly from argument

        --no_debug               Disable generation of debug formation)");
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("blik %1", FelixVersion);
        return 0;
    }

    // Parse arguments
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-i", "--inline")) {
                run_inline = true;
            } else if (opt.Test("--no_debug")) {
                generate_debug = false;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        filename = opt.ConsumeNonOption();
        if (!filename) {
            LogError("No script provided");
            return 1;
        }
    }

    HeapArray<char> code;
    if (run_inline) {
        code.Append(filename);
        filename = "<inline>";
    } else {
        if (ReadFile(filename, Megabytes(64), &code) < 0)
            return 1;
    }

    TokenSet token_set;
    if (!Tokenize(code, filename, &token_set))
        return 1;

    Program program;
    DebugInfo debug;
    if (!Compile(token_set, filename, &program, generate_debug ? &debug : nullptr))
        return 1;

    return Run(program, generate_debug ? &debug : nullptr);
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
