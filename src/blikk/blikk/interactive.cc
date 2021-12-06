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
        str_alloc.ReleaseAll();
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
    if typeOf(__result) != Null do debug(__result)
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

    // Try to parse with fake print first...
    bool valid_with_fake_print;
    if (config.try_expression) {
        bk_TokenizedFile file;
        if (!TokenizeWithFakePrint(code, "<inline>", &file))
            return 1;

        // ... but don't tell the user if it fails!
        SetLogHandler([](LogLevel level, const char *ctx, const char *msg) {});
        RG_DEFER { SetLogHandler(DefaultLogHandler); };

        unsigned int flags = config.fold ? 0 : (int)bk_CompileFlag::NoFold;
        valid_with_fake_print = compiler.Compile(file, flags);
    } else {
        valid_with_fake_print = false;
    }

    // If the fake print has failed, reparse the code without the fake print
    if (!valid_with_fake_print) {
        bk_TokenizedFile file;
        bool success = bk_Tokenize(code, "<inline>", &file);
        RG_ASSERT(success);

        unsigned int flags = config.fold ? 0 : (int)bk_CompileFlag::NoFold;
        if (!compiler.Compile(file, flags))
            return 1;
    }

    unsigned int flags = config.debug ? (int)bk_RunFlag::DebugInstructions : 0;
    return config.execute ? !bk_Run(program, flags) : 0;
}

int RunInteractive(const Config &config)
{
    LogInfo("%!R..blikk%!0 %1", FelixVersion);

    bk_Program program;

    bk_Compiler compiler(&program);
    bk_ImportAll(&compiler);

    bk_VirtualMachine vm(&program);
    unsigned int flags = config.debug ? (int)bk_RunFlag::DebugInstructions : 0;
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

    // Make sure the prelude runs successfully
    {
        bool success = compiler.Compile("", "<inline>", 0) && vm.Run(flags);
        RG_ASSERT(success);
    }

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
        });
        RG_DEFER_N(try_guard) {
            prompter.Commit();
            trace.Dump();
        };

        Span<const char> code = TrimStrRight(prompter.str.Take());
        if (!code.len)
            continue;

        Size prev_variables_len = program.variables.len;
        Size prev_stack_len = vm.stack.len;

        bool valid_with_fake_print;
        if (config.try_expression) {
            bk_TokenizedFile file;
            if (!TokenizeWithFakePrint(code, "<inline>", &file))
                continue;

            unsigned int flags = config.fold ? 0 : (int)bk_CompileFlag::NoFold;
            valid_with_fake_print = compiler.Compile(file, flags);
        } else {
            valid_with_fake_print = false;
        }

        if (!valid_with_fake_print) {
            trace.Clear();

            bk_TokenizedFile file;
            bool success = bk_Tokenize(code, "<interactive>", &file);
            RG_ASSERT(success);

            unsigned int flags = config.fold ? 0 : (int)bk_CompileFlag::NoFold;
            bk_CompileReport report;

            if (!compiler.Compile(file, flags, &report)) {
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

        if (config.execute && !vm.Run(flags)) {
            // Destroying global variables should be enough, because we execute single statements.
            // Thus, if the user defines a function, pretty much no execution can occur, and
            // execution should not even be able to fail in this case.
            // Besides, since thore are global variables, no shadowing has occured and we don't
            // need to deal with this.
            for (Size i = prev_variables_len; i < program.variables.len; i++) {
                program.variables_map.Remove(program.variables[i].name);
            }
            program.variables.RemoveFrom(prev_variables_len);

            // XXX: We don't yet manage memory so this works for now
            vm.stack.RemoveFrom(prev_stack_len);

            vm.frames.RemoveFrom(1);
            vm.frames[0].pc = program.ir.len;
        }
    }

    return 0;
}

}
