// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "../libblik/libblik.hh"

namespace RG {

int RunCommand(Span<const char> code);
int RunInteractive();

static int RunFile(const char *filename)
{
    HeapArray<char> code;
    if (ReadFile(filename, Megabytes(64), &code) < 0)
        return 1;

    TokenizedFile file;
    Program program;
    if (!Tokenize(code, filename, &file))
        return 1;
    if (!Parse(file, &program))
        return 1;
    if (!Run(program))
        return 1;

    return 0;
}

int Main(int argc, char **argv)
{
    enum class RunMode {
        Interactive,
        File,
        Command
    };

    // Options
    RunMode mode = RunMode::File;
    const char *filename_or_code = nullptr;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: blik [options] <file>
       blik [options] -c <code>
       blik [options] -i

Options:
    -c, --command                Run code directly from argument
    -i, --interactive            Run code interactively (REPL))");
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
            } else if (opt.Test("-c", "--command")) {
                if (mode != RunMode::File) {
                    LogError("You cannot use --command and --interactive at the same time");
                    return 1;
                }

                mode = RunMode::Command;
            } else if (opt.Test("-i", "--interactive")) {
                if (mode != RunMode::File) {
                    LogError("You cannot use --command and --interactive at the same time");
                    return 1;
                }

                mode = RunMode::Interactive;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        filename_or_code = opt.ConsumeNonOption();
    }

    switch (mode) {
        case RunMode::Interactive: { return RunInteractive(); } break;

        case RunMode::File: {
            if (!filename_or_code) {
                LogError("No filename provided");
                return 1;
            }

            return RunFile(filename_or_code);
        } break;
        case RunMode::Command: {
            if (!filename_or_code) {
                LogError("No command provided");
                return 1;
            }

            return RunCommand(filename_or_code);
        } break;
    }

    RG_UNREACHABLE();
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
