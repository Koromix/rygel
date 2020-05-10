// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "../libblik/libblik.hh"

namespace RG {

struct LogEntry {
    LogLevel level;
    const char *ctx;
    const char *msg;
};

class LogTrace {
    HeapArray<LogEntry> entries;
    BlockAllocator str_alloc;

public:
    void HandleLog(LogLevel level, const char *ctx, const char *msg)
    {
        LogEntry entry = {};

        entry.level = level;
        entry.ctx = DuplicateString(ctx, &str_alloc).ptr;
        entry.msg = DuplicateString(msg, &str_alloc).ptr;

        entries.Append(entry);
    }

    void Dump()
    {
        for (const LogEntry &entry: entries) {
            DefaultLogHandler(entry.level, entry.ctx, entry.msg);
        }

        entries.Clear();
        str_alloc.ReleaseAll();
    }
};

static int RunInteractive()
{
    Program program;
    Parser parser(&program);
    VirtualMachine vm(&program);

    HeapArray<char> code;
    char line[1024];

    while (fgets(line, sizeof(line), stdin) != nullptr) {
        code.Append(line);

        // We need to intercept errors in order to hide them in some cases, such as
        // for unexpected EOF because we want to allow the user to add more lines!
        LogTrace trace;
        SetLogHandler([&](LogLevel level, const char *ctx, const char *msg) { trace.HandleLog(level, ctx, msg); });
        RG_DEFER_N(err_guard) {
            code.Clear();
            trace.Dump();
        };

        TokenizedFile file;
        if (!Tokenize(code, "<interactive>", &file))
            continue;

        ParseReport report;
        if (!parser.Parse(file, &report)) {
            if (report.unexpected_eof) {
                err_guard.Disable();
            }

            continue;
        }

        int exit_code;
        if (!vm.Run(&exit_code))
            return 1;

        // Delete the exit for the next iteration :)
        program.ir.RemoveLast(1);

        code.Clear();
    }

    return 0;
}

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
    }

    RG_UNREACHABLE();
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
