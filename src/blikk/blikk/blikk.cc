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
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "../../core/libcc/libcc.hh"
#include "../libblikk/libblikk.hh"
#include "../../core/libsecurity/libsecurity.hh"

namespace RG {

int RunCommand(Span<const char> code, bool execute, bool try_expr, bool debug);
int RunInteractive(bool execute, bool try_expr, bool debug);

static bool ApplySandbox()
{
    if (!sec_IsSandboxSupported()) {
        LogError("Sandbox mode is not supported on this platform");
        return false;
    }

    sec_SandboxBuilder sb;

#ifdef __linux__
    sb.FilterSyscalls(sec_FilterAction::Kill, {
        {"exit", sec_FilterAction::Allow},
        {"exit_group", sec_FilterAction::Allow},
        {"brk", sec_FilterAction::Allow},
        {"mmap/anon", sec_FilterAction::Allow},
        {"munmap", sec_FilterAction::Allow},
        {"read", sec_FilterAction::Allow},
        {"readv", sec_FilterAction::Allow},
        {"write", sec_FilterAction::Allow},
        {"writev", sec_FilterAction::Allow},
        {"fstat", sec_FilterAction::Allow},
        {"ioctl/tty", sec_FilterAction::Allow},
        {"rt_sigaction", sec_FilterAction::Allow},
        {"close", sec_FilterAction::Allow},
        {"fsync", sec_FilterAction::Allow}
    });
#endif

    return sb.Apply();
}

static int RunFile(const char *filename, bool sandbox, bool execute, bool debug)
{
    bk_Program program;
    {
        HeapArray<char> code;
        if (ReadFile(filename, Megabytes(256), &code) < 0)
            return 1;

        if (sandbox && !ApplySandbox())
            return 1;

        bk_Compiler compiler(&program);
        bk_ImportAll(&compiler);

        if (!compiler.Compile(code, filename))
            return 1;
    }

    return execute ? !bk_Run(program, debug) : 0;
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
    bool sandbox = false;
    bool execute = true;
    bool try_expr = true;
    bool debug = false;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 [options] <file>
       %1 [options] -c <code>
       %1 [options] -i%!0

Options:
    %!..+-c, --command%!0                Run code directly from argument
    %!..+-i, --interactive%!0            Run code interactively (REPL)

        %!..+--sandbox%!0                Run in strict OS sandbox (if supported)

        %!..+--no_execute%!0             Parse code but don't run it
        %!..+--no_expression%!0          Don't try to run code as expression
                                 %!D..(works only with -c or -i)%!0
        %!..+--debug%!0                  Dump executed VM instructions)", FelixTarget);
    };

    // Handle version
    if (argc >= 2 && TestStr(argv[1], "--version")) {
        PrintLn("%!R..%1%!0 %2", FelixTarget, FelixVersion);
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
            } else if (opt.Test("--sandbox")) {
                sandbox = true;
            } else if (opt.Test("--no_execute")) {
                execute = false;
            } else if (opt.Test("--no_expression")) {
                try_expr = false;
            } else if (opt.Test("--debug")) {
                debug = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        filename_or_code = opt.ConsumeNonOption();
    }

    switch (mode) {
        case RunMode::Interactive: {
            if (sandbox && !ApplySandbox())
                return 1;

            return RunInteractive(execute, try_expr, debug);
        } break;

        case RunMode::File: {
            if (!filename_or_code) {
                LogError("No filename provided");
                return 1;
            }

            return RunFile(filename_or_code, sandbox, execute, debug);
        } break;
        case RunMode::Command: {
            if (!filename_or_code) {
                LogError("No command provided");
                return 1;
            }

            if (sandbox && !ApplySandbox())
                return 1;

            return RunCommand(filename_or_code, execute, try_expr, debug);
        } break;
    }

    RG_UNREACHABLE();
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
