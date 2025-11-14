// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "blikk.hh"

namespace K {

int RunFile(const char *filename, const Config &config)
{
    bk_Program program;
    {
        HeapArray<char> code;
        if (ReadFile(filename, Megabytes(256), &code) < 0)
            return 1;

        bk_Compiler compiler(&program);
        bk_ImportAll(&compiler);

        if (!compiler.Compile(code, filename))
            return 1;
    }

    unsigned int flags = config.debug ? (int)bk_RunFlag::Debug : 0;
    return config.execute ? !bk_Run(program, flags) : 0;
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
    Config config;

    const auto print_usage = [](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 [option...] file
       %1 [option...] -c code
       %1 [option...] -i%!0

Options:

    %!..+-c, --command%!0                  Run code directly from argument
    %!..+-i, --interactive%!0              Run code interactively (REPL)

        %!..+--no_execute%!0               Parse code but don't run it
        %!..+--no_expression%!0            Don't try to run code as expression
                                   %!D..(works only with -c or -i)%!0
        %!..+--debug%!0                    Dump executed VM instructions)", FelixTarget);
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
                if (mode == RunMode::Interactive) {
                    LogError("You cannot use --command and --interactive at the same time");
                    return 1;
                }

                mode = RunMode::Command;
            } else if (opt.Test("-i", "--interactive")) {
                if (mode == RunMode::Command) {
                    LogError("You cannot use --command and --interactive at the same time");
                    return 1;
                }

                mode = RunMode::Interactive;
            } else if (opt.Test("--no_execute")) {
                config.execute = false;
            } else if (opt.Test("--no_expression")) {
                config.try_expression = false;
            } else if (opt.Test("--debug")) {
                config.debug = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        filename_or_code = opt.ConsumeNonOption();
        opt.LogUnusedArguments();
    }

    switch (mode) {
        case RunMode::Interactive: return RunInteractive(config);

        case RunMode::File: {
            if (!filename_or_code) {
                LogError("No filename provided");
                return 1;
            }

            return RunFile(filename_or_code, config);
        } break;

        case RunMode::Command: {
            if (!filename_or_code) {
                LogError("No command provided");
                return 1;
            }

            return RunCommand(filename_or_code, config);
        } break;
    }

    K_UNREACHABLE();
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return K::RunApp(argc, argv); }
