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
    void Store(LogLevel level, const char *ctx, const char *msg)
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

int RunInteractive()
{
    LogInfo("%!R.+blik%!0 %1", FelixVersion);

    WaitForInterruption(0);
    EnableAnsiOutput();

    Program program;
    Parser parser(&program);
    VirtualMachine vm(&program);
    bool run = true;

    parser.AddFunction("exit()", [&](VirtualMachine *vm, Span<const Value> args) {
        run = false;
        vm->SetInterrupt();
        return Value();
    });
    parser.AddFunction("quit()", [&](VirtualMachine *vm, Span<const Value> args) {
        run = false;
        vm->SetInterrupt();
        return Value();
    });

    ConsolePrompter prompter;
    ParseReport report;

    while (run && prompter.Read()) {
        // We need to intercept errors in order to hide them in some cases, such as
        // for unexpected EOF because we want to allow the user to add more lines!
        LogTrace trace;
        SetLogHandler([&](LogLevel level, const char *ctx, const char *msg) {
            if (level == LogLevel::Debug) {
                DefaultLogHandler(level, ctx, msg);
            } else {
                trace.Store(level, ctx, msg);
            }
        });
        RG_DEFER_N(try_guard) {
            prompter.Commit();
            trace.Dump();
        };

        TokenizedFile file;
        if (!Tokenize(prompter.str, "<interactive>", &file))
            continue;

        if (!parser.Parse(file, &report)) {
            if (report.unexpected_eof) {
                prompter.str.len = TrimStrRight((Span<const char>)prompter.str).len;
                Fmt(&prompter.str, "\n%1", FmtArg("    ").Repeat(report.depth + 1));

                try_guard.Disable();
            }

            continue;
        }

        if (!vm.Run())
            return 1;

        // Delete the exit for the next iteration :)
        program.ir.RemoveLast(1);
    }

    return 0;
}

}
