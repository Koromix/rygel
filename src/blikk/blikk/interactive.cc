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
#include "blikk.hh"

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
        entry.ctx = ctx ? DuplicateString(ctx, &str_alloc).ptr : nullptr;
        entry.msg = DuplicateString(msg, &str_alloc).ptr;

        entries.Append(entry);
    }

    void Dump()
    {
        for (const LogEntry &entry: entries) {
            DefaultLogHandler(entry.level, entry.ctx, entry.msg);
        }

        Clear();
    }

    void Clear()
    {
        entries.Clear();
        str_alloc.Reset();
    }
};

static bool TokenizeWithFakePrint(Span<const char> code, const char *filename, bk_TokenizedFile *out_file)
{
    static bk_TokenizedFile intro;
    static bk_TokenizedFile outro;

    // Tokenize must only be called once for each TokenizedFile, so we need to cheat a little
    if (!intro.tokens.len) {
        bool success = bk_Tokenize(R"(
begin
    let __result =
)", "<intro>", &intro);
        success &= bk_Tokenize(R"(
    if typeOf(__result) != Null do __log(__result)
end
)", "<outro>", &outro);

        RG_ASSERT(success);
    }

    out_file->tokens.Append(intro.tokens);
    if (!bk_Tokenize(code, filename, out_file))
        return false;
    out_file->tokens.Append(outro.tokens);

    return true;
}

int RunCommand(Span<const char> code, const Config &config)
{
    bk_Program program;

    bk_Compiler compiler(&program);
    bk_ImportAll(&compiler);

    BK_ADD_FUNCTION(compiler, "__log(...)", 0, { bk_DoPrint(vm, args, true); PrintLn(); });

    // Try to parse with fake print first...
    bool valid_with_fake_print;
    if (config.try_expression) {
        bk_TokenizedFile file;
        if (!TokenizeWithFakePrint(code, "<inline>", &file))
            return 1;

        // ... but don't tell the user if it fails!
        SetLogHandler([](LogLevel, const char *, const char *) {}, false);
        RG_DEFER { SetLogHandler(DefaultLogHandler, StdErr->IsVt100()); };

        valid_with_fake_print = compiler.Compile(file);
    } else {
        valid_with_fake_print = false;
    }

    // If the fake print has failed, reparse the code without the fake print
    if (!valid_with_fake_print) {
        bk_TokenizedFile file;
        bool success = bk_Tokenize(code, "<inline>", &file);
        RG_ASSERT(success);

        if (!compiler.Compile(file))
            return 1;
    }

    unsigned int flags = config.debug ? (int)bk_RunFlag::Debug : 0;
    return config.execute ? !bk_Run(program, flags) : 0;
}

int RunInteractive(const Config &config)
{
    LogInfo("%!R..blikk%!0 %!..+%1%!0", FelixVersion);

    bk_Program program;

    bk_Compiler compiler(&program);
    bk_ImportAll(&compiler);

    unsigned int flags = config.debug ? (int)bk_RunFlag::Debug : 0;
    bk_VirtualMachine vm(&program, flags);
    bool run = true;

    // Functions specific to interactive mode
    BK_ADD_FUNCTION(compiler, "exit()", 0, {
        run = false;
        vm->SetInterrupt();
    });
    BK_ADD_FUNCTION(compiler, "quit()", 0, {
        run = false;
        vm->SetInterrupt();
    });
    BK_ADD_FUNCTION(compiler, "__log(...)", 0, {
        bk_DoPrint(vm, args, true); PrintLn();

        if (args.len && args[0].type->primitive == bk_PrimitiveKind::Function &&
                        (TestStr(args[1].func->prototype, "quit()") ||
                         TestStr(args[1].func->prototype, "exit()"))) {
            PrintLn("%!D..Use quit() or exit() to exit%!0");
        }
    });

    ConsolePrompter prompter;

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
        }, false);
        RG_DEFER_N(try_guard) {
            prompter.Commit();
            trace.Dump();
        };

        Span<const char> code = TrimStrRight(prompter.str.Take());
        if (!code.len)
            continue;

        Size prev_variables_count = program.variables.count;
        Size prev_stack_len = vm.stack.len;

        bool valid_with_fake_print;
        if (config.try_expression) {
            bk_TokenizedFile file;
            if (!TokenizeWithFakePrint(code, "<inline>", &file))
                continue;

            valid_with_fake_print = compiler.Compile(file);
        } else {
            valid_with_fake_print = false;
        }

        if (!valid_with_fake_print) {
            trace.Clear();

            bk_TokenizedFile file;
            bool success = bk_Tokenize(code, "<interactive>", &file);
            RG_ASSERT(success);

            bk_CompileReport report;
            if (!compiler.Compile(file, &report)) {
                if (report.unexpected_eof) {
                    prompter.str.len = TrimStrRight(prompter.str.Take(), "\t ").len;
                    if (!prompter.str.len || prompter.str[prompter.str.len - 1] != '\n') {
                        prompter.str.Append('\n');
                    }
                    Fmt(&prompter.str, "%1", FmtArg("    ").Repeat(report.depth + 1));

                    try_guard.Disable();
                }

                continue;
            }
        }

        if (config.execute && !vm.Run()) {
            // Destroying global variables should be enough, because we execute single statements.
            // Thus, if the user defines a function, pretty much no execution can occur, and
            // execution should not even be able to fail in this case.
            // Besides, since thore are global variables, no shadowing has occured and we don't
            // need to deal with this.
            for (Size i = prev_variables_count; i < program.variables.count; i++) {
                program.variables_map.Remove(program.variables[i].name);
            }
            program.variables.RemoveFrom(prev_variables_count);

            // XXX: We don't yet manage memory so this works for now
            vm.stack.RemoveFrom(prev_stack_len);

            vm.frames.RemoveFrom(1);
            vm.frames[0].pc = program.main.len;
        }
    }

    return 0;
}

}
