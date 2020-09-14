// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "build.hh"
#include "compiler.hh"
#include "target.hh"

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <unistd.h>
#endif

namespace RG {

static int RunTarget(const TargetInfo &target, const char *target_filename,
                     Span<const char *const> arguments)
{
    RG_ASSERT(target.type == TargetType::Executable);

    LogInfo("Run target '%1'\n%!D..--------------------------------------------------%!0", target.name);

#ifdef _WIN32
    HeapArray<char> cmd;

    Fmt(&cmd, "\"%1\"", target_filename);

    // Windows command line quoting rules are batshit crazy
    for (const char *arg: arguments) {
        bool quote = strchr(arg, ' ');

        cmd.Append(quote ? " \"" : " ");
        for (Size i = 0; arg[i]; i++) {
            if (strchr("\"", arg[i])) {
                cmd.Append('\\');
            }
            cmd.Append(arg[i]);
        }
        cmd.Append(quote ? "\"" : "");
    }
    cmd.Append(0);

    // XXX: Use ExecuteCommandLine() but it needs to be improved first, because right
    // now it always wants to capture output. And we don't want to do that here.
    STARTUPINFOA startup_info = {};
    PROCESS_INFORMATION process_info;
    if (!CreateProcessA(target_filename, cmd.ptr, nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &startup_info, &process_info)) {
        LogError("Failed to start process: %1", GetWin32ErrorString());
        return 127;
    }
    RG_DEFER {
        CloseHandle(process_info.hProcess);
        CloseHandle(process_info.hThread);
    };

    DWORD exit_code = 0;

    bool success = (WaitForSingleObject(process_info.hProcess, INFINITE) == WAIT_OBJECT_0) &&
                   GetExitCodeProcess(process_info.hProcess, &exit_code);
    RG_ASSERT(success);

    return (int)exit_code;
#else
    HeapArray<const char *> argv;

    argv.Append(target_filename);
    argv.Append(arguments);
    argv.Append(nullptr);

    execv(target_filename, (char **)argv.ptr);
    LogError("Failed to execute '%1': %2", target_filename, strerror(errno));
    return 127;
#endif
}

static const char *BuildGitVersionString(Allocator *alloc)
{
    HeapArray<char> output(alloc);
    int exit_code;
    if (!ExecuteCommandLine("git log -n1 --pretty=format:%ad_%h --date=format:%Y%m%d",
                            {}, Kilobytes(1), &output, &exit_code))
        return nullptr;
    if (exit_code) {
        LogError("Command 'git log' failed");
        return nullptr;
    }

    output.len = TrimStrRight((Span<const char>)output).len;
    output.Append(0);

    return output.TrimAndLeak().ptr;
}

int RunBuild(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    HeapArray<const char *> selectors;
    const char *config_filename = nullptr;
    BuildSettings build = {};
    int jobs = std::min(GetCoreCount() + 1, RG_ASYNC_MAX_WORKERS + 1);
    bool quiet = false;
    bool verbose = false;
    const char *run_target_name = nullptr;
    Span<const char *> run_arguments = {};
    bool run_here = false;

    // Find default compiler
    build.compiler = FindIf(Compilers, [](const Compiler *compiler) { return compiler->Test(); });

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+felix build [options] [target...]
       felix build [options] --run target [arguments...]%!0

Options:
    %!..+-C, --config <filename>%!0      Set configuration filename
                                 %!D..(default: FelixBuild.ini)%!0
    %!..+-O, --output <directory>%!0     Set output directory
                                 %!D..(default: bin/<toolchain>)%!0

    %!..+-c, --compiler <compiler>%!0    Set compiler, see below
                                 %!D..(default: %1)%!0
    %!..+-m, --mode <mode>%!0            Set build mode, see below
                                 %!D..(default: %2)%!0
    %!..+-e, --environment%!0            Use compiler flags found in environment (CFLAGS, LDFLAGS, etc.)
        %!..+--no_pch%!0                 Disable header precompilation (PCH)

    %!..+-j, --jobs <count>%!0           Set maximum number of parallel jobs
                                 %!D..(default: %3)%!0
        %!..+--rebuild%!0                Force rebuild all files

    %!..+-q, --quiet%!0                  Hide felix progress statements
    %!..+-v, --verbose%!0                Show detailed build commands
    %!..+-n, --dry_run%!0                Fake command execution

        %!..+--run <target>%!0           Run target after successful build
                                 %!D..(all remaining arguments are passed as-is)%!0
        %!..+--run_here <target>%!0      Same thing, but run from current directory

Supported compilers:)", build.compiler ? build.compiler->name : "?",
                        CompileModeNames[(int)build.compile_mode], jobs);

        for (const Compiler *compiler: Compilers) {
            PrintLn(fp, "    %!..+%1%!0 %2%!D..%3%!0",
                    FmtArg(compiler->name).Pad(28), compiler->binary,
                    compiler->Test() ? "" : " [not available in PATH]");
        }

        PrintLn(fp, R"(
Supported compilation modes: %!..+%1%!0)", FmtSpan(CompileModeNames));

        PrintLn(fp, R"(
Felix can also run the following special commands:
    %!..+build%!0                        Build C and C++ projects %!D..(default)%!0
    %!..+pack%!0                         Pack assets to C source file and other formats

For help about those commands, type: %!..+felix <command> --help%!0)");
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        for (;;) {
            // We need to consume values (target names) as we go because
            // the --run option will break the loop and all remaining
            // arguments will be passed as-is to the target.
            opt.ConsumeNonOptions(&selectors);
            if (!opt.Next())
                break;

            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config", OptionType::Value)) {
                config_filename = opt.current_value;
            } else if (opt.Test("-O", "--output", OptionType::Value)) {
                build.output_directory = opt.current_value;
            } else if (opt.Test("-c", "--compiler", OptionType::Value)) {
                build.compiler =
                    FindIf(Compilers, [&](const Compiler *compiler) { return TestStr(compiler->name, opt.current_value); });

                if (!build.compiler) {
                    LogError("Unknown compiler '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("-m", "--mode", OptionType::Value)) {
                if (!OptionToEnum(CompileModeNames, opt.current_value, &build.compile_mode)) {
                    LogError("Unknown build mode '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("-e", "--environment")) {
                build.env = true;
            } else if (opt.Test("--no_pch")) {
                build.pch = false;
            } else if (opt.Test("-j", "--jobs", OptionType::Value)) {
                if (!ParseDec(opt.current_value, &jobs))
                    return 1;
                if (jobs < 1) {
                    LogError("Jobs count cannot be < 1");
                    return 1;
                }
            } else if (opt.Test("--rebuild")) {
                build.rebuild = true;
            } else if (opt.Test("-q", "--quiet")) {
                quiet = true;
            } else if (opt.Test("-v", "--verbose")) {
                verbose = true;
            } else if (opt.Test("-n", "--dry_run")) {
                build.fake = true;
            } else if (opt.Test("--run", OptionType::Value)) {
                run_target_name = opt.current_value;
                break;
            } else if (opt.Test("--run_here", OptionType::Value)) {
                run_target_name = opt.current_value;
                run_here = true;
                break;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        if (run_target_name) {
            selectors.Append(run_target_name);
            run_arguments = opt.GetRemainingArguments();
        }
    }

    if (quiet) {
        SetLogHandler([](LogLevel level, const char *ctx, const char *msg) {
            if (level != LogLevel::Info) {
                DefaultLogHandler(level, ctx, msg);
            }
        });
    }

    // Check compiler
    if (!build.compiler) {
        LogError("Could not find any supported compiler in PATH");
        return 1;
    } else if (!build.compiler->Test()) {
        LogError("Cannot find %1 compiler in PATH [%2]", build.compiler->name, build.compiler->binary);
        return 1;
    }

    // Root directory
    const char *start_directory = DuplicateString(GetWorkingDirectory(), &temp_alloc).ptr;
    if (config_filename) {
        Span<const char> root_directory;
        config_filename = SplitStrReverseAny(config_filename, RG_PATH_SEPARATORS, &root_directory).ptr;

        if (root_directory.len) {
            const char *root_directory0 = DuplicateString(root_directory, &temp_alloc).ptr;
            if (!SetWorkingDirectory(root_directory0))
                return 1;
        }
    } else {
        config_filename = "FelixBuild.ini";

        // Try to find FelixBuild.ini in current directory and all parent directories. We
        // don't need to handle not finding it anywhere, because in this case the config load
        // will fail with a simple "Cannot open 'FelixBuild.ini'" message.
        for (Size i = 0; start_directory[i]; i++) {
            if (IsPathSeparator(start_directory[i])) {
                if (TestFile(config_filename))
                    break;

                SetWorkingDirectory("..");
            }
        }
    }

    // Output directory
    if (build.output_directory) {
        build.output_directory = NormalizePath(build.output_directory, start_directory, &temp_alloc).ptr;
    } else {
        build.output_directory = Fmt(&temp_alloc, "%1%/bin%/%2_%3", GetWorkingDirectory(),
                                     build.compiler->name, CompileModeNames[(int)build.compile_mode]).ptr;
    }

    // Load configuration file
    TargetSet target_set;
    if (!LoadTargetSet(config_filename, &target_set))
        return 1;
    if (!target_set.targets.len) {
        LogError("Configuration file does not contain any target");
        return 1;
    }

    // Select targets
    HeapArray<const TargetInfo *> enabled_targets;
    HeapArray<const SourceFileInfo *> enabled_sources;
    if (selectors.len) {
        HashSet<const char *> handled_set;

        for (const char *selector: selectors) {
            bool match = false;
            for (const TargetInfo &target: target_set.targets) {
                if (MatchPathSpec(target.name, selector)) {
                    if (handled_set.TrySet(target.name).second) {
                        enabled_targets.Append(&target);
                    }

                    match = true;
                }
            }
            for (const SourceFileInfo &src: target_set.sources) {
                if (MatchPathSpec(src.filename, selector)) {
                    if (handled_set.TrySet(src.filename).second) {
                        enabled_sources.Append(&src);
                    }

                    match = true;
                }
            }

            if (!match) {
                LogError("Selector '%1' does not match anything", selector);
                return 1;
            }
        }
    } else {
        for (const TargetInfo &target: target_set.targets) {
            if (target.enable_by_default) {
                enabled_targets.Append(&target);
            }
        }

        if (!enabled_targets.len) {
            LogError("No target to build by default");
            return 1;
        }
    }

    // Find and check target used with --run
    const TargetInfo *run_target = nullptr;
    if (run_target_name) {
        run_target = target_set.targets_map.FindValue(run_target_name, nullptr);

        if (!run_target) {
            LogError("Run target '%1' does not exist", run_target_name);
            return 1;
        } else if (run_target->type != TargetType::Executable) {
            LogError("Cannot run non-executable target '%1'", run_target->name);
            return 1;
        }
    }

    // We're ready to output stuff
    LogInfo("Root directory: '%1'", GetWorkingDirectory());
    LogInfo("  Compiler: %1 (%2)", build.compiler->name, CompileModeNames[(int)build.compile_mode]);
    LogInfo("  Output directory: '%1'", build.output_directory);
    if (!build.fake && !MakeDirectoryRec(build.output_directory))
        return 1;

    // Build version string from git commit (date, hash)
    build.version_str = BuildGitVersionString(&temp_alloc);
    if (!build.version_str) {
        LogError("Git version string will be null");
    }

    // The detection of SIGINT (or the Win32 equivalent) by WaitForInterruption()
    // remains after timing out, which will allow RunBuildNodes() to clean up files
    // produced by interrupted commands.
    WaitForInterruption(0);

    // Build stuff!
    Builder builder(build);
    for (const TargetInfo *target: enabled_targets) {
        if (!builder.AddTarget(*target))
            return 1;
    }
    for (const SourceFileInfo *src: enabled_sources) {
        if (!builder.AddSource(*src))
            return 1;
    }
    if (!builder.Build(jobs, verbose))
        return 1;

    // Run?
    if (run_target) {
        if (run_here && !SetWorkingDirectory(start_directory))
            return 1;

        const char *target_filename = builder.target_filenames.FindValue(run_target->name, nullptr);
        return RunTarget(*run_target, target_filename, run_arguments);
    } else {
        return 0;
    }
}

}
