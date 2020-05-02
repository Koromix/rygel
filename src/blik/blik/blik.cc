// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "../libblik/libblik.hh"

namespace RG {

static int RunCode(Span<const char> code, const char *filename)
{
    TokenizedFile file;
    Program program;

    if (!Tokenize(code, filename, &file))
        return 1;
    if (!Parse(file, &program))
        return 1;

    int exit_code;
    if (!Run(program, &exit_code))
        return 1;

    return exit_code;
}

static int RunInteractive()
{
    Program program;
    Parser parser(&program);
    VirtualMachine vm(&program);

    // XXX: We need to go back to the StreamReader/LineReader split design, because it
    // does not work correctly with stdin. The problem is that StreamReader uses fread()
    // which does block until the buffer is full or EOF. Until then, use fgets() here.
    char chunk[1024];
    while(fgets(chunk, sizeof(chunk), stdin) != nullptr) {
        TokenizedFile file;
        if (!Tokenize(chunk, "<inline>", &file))
            continue;
        if (!parser.Parse(file))
            continue;

        int code;
        if (!vm.Run(&code))
            return 1;

        // Delete the exit for the next iteration :)
        program.ir.RemoveLast(1);
    }

    return 0;
}

int Main(int argc, char **argv)
{
    enum class RunMode {
        File,
        Command,
        Interactive
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
        case RunMode::File: {
            if (!filename_or_code) {
                LogError("No filename provided");
                return 1;
            }

            HeapArray<char> code;
            if (ReadFile(filename_or_code, Megabytes(64), &code) < 0)
                return 1;

            return RunCode(code, filename_or_code);
        } break;

        case RunMode::Command: {
            if (!filename_or_code) {
                LogError("No command provided");
                return 1;
            }

            return RunCode(filename_or_code, "<inline>");
        } break;

        case RunMode::Interactive: { return RunInteractive(); } break;
    }

    RG_UNREACHABLE();
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
