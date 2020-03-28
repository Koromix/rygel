// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "build.hh"
#include "compiler.hh"
#include "target.hh"

#ifndef _WIN32
    #include <unistd.h>
#endif

namespace RG {

static int RunTarget(const TargetInfo &target, const char *target_filename,
                     Span<const char *const> arguments, bool verbose)
{
    RG_ASSERT(target.type == TargetType::Executable);

    HeapArray<char> cmd_buf;

    // XXX: Just like the code in compiler.cc, command-line escaping is
    // either wrong or not done. Make something to deal with that uniformely.
    Fmt(&cmd_buf, "\"%1\"", target_filename);
    for (const char *arg: arguments) {
        bool escape = strchr(arg, ' ');
        Fmt(&cmd_buf, escape ? " \"%1\"" : " %1", arg);
    }

    if (verbose) {
        LogInfo("Run '%1'", cmd_buf);
    } else {
        LogInfo("Run target '%1'", target.name);
    }
    PrintLn(stderr);

#ifdef _WIN32
    return system(cmd_buf.ptr);
#else
    execl("/bin/sh", "sh", "-c", cmd_buf.ptr, nullptr);
    LogError("Failed to execute /bin/sh: %1", strerror(errno));
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

    return output.Leak().ptr;
}

int RunBuild(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    HeapArray<const char *> target_names;
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
R"(Usage: felix build [options] [target...]
       felix build [options] --run target [arguments...]

Options:
    -C, --config <filename>      Set configuration filename
                                 (default: FelixBuild.ini)
    -O, --output <directory>     Set output directory
                                 (default: bin/<toolchain>)

    -c, --compiler <compiler>    Set compiler, see below
                                 (default: %1)
    -m, --mode <mode>            Set build mode, see below
                                 (default: %2)
        --no_pch                 Disable header precompilation (PCH)

    -j, --jobs <count>           Set maximum number of parallel jobs
                                 (default: %3)
        --rebuild                Force rebuild all files

    -q, --quiet                  Hide felix progress statements
    -v, --verbose                Show detailed build commands

        --run <target>           Run target after successful build
                                 (all remaining arguments are passed as-is)
        --run_here <target>      Same thing, but run from current directory

Supported compilers:)", build.compiler ? build.compiler->name : "?",
                        CompileModeNames[(int)build.compile_mode], jobs);

        for (const Compiler *compiler: Compilers) {
            PrintLn(fp, "    %1 %2%3",
                    FmtArg(compiler->name).Pad(28), compiler->binary,
                    compiler->Test() ? "" : " [not available in PATH]");
        }

        PrintLn(fp, R"(
Supported compilation modes:)");
        for (const char *mode_name: CompileModeNames) {
            PrintLn(fp, "    %1", mode_name);
        }
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        for (;;) {
            // We need to consume values (target names) as we go because
            // the --run option will break the loop and all remaining
            // arguments will be passed as-is to the target.
            opt.ConsumeNonOptions(&target_names);
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
                const char *const *ptr =
                    FindIfPtr(CompileModeNames,
                              [&](const char *name) { return TestStr(name, opt.current_value); });

                if (ptr) {
                    build.compile_mode = (CompileMode)(ptr - CompileModeNames);
                } else {
                    LogError("Unknown build mode '%1'", opt.current_value);
                    return 1;
                }
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
            target_names.Append(run_target_name);
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

    // Default targets
    if (!target_names.len) {
        for (const TargetInfo &target: target_set.targets) {
            if (target.enable_by_default) {
                target_names.Append(target.name);
            }
        }

        if (!target_names.len) {
            LogError("There are no targets");
            return 1;
        }
    }

    // Select specified targets
    HeapArray<const TargetInfo *> enabled_targets;
    const TargetInfo *run_target = nullptr;
    {
        HashSet<const char *> handled_set;

        bool valid = true;
        for (const char *target_name: target_names) {
            if (handled_set.Append(target_name).second) {
                const TargetInfo *target = target_set.targets_map.FindValue(target_name, nullptr);
                if (!target) {
                    LogError("Target '%1' does not exist", target_name);
                    valid = false;
                    continue;
                }

                enabled_targets.Append(target);
                if (run_target_name && TestStr(target_name, run_target_name)) {
                    run_target = target;
                }
            }
        }
        if (!valid)
            return 1;
    }
    if (run_target && run_target->type != TargetType::Executable) {
        LogError("Cannot run non-executable target '%1'", run_target->name);
        return 1;
    }

    // We're ready to output stuff
    LogInfo("Root directory: '%1'", GetWorkingDirectory());
    LogInfo("Compiler: %1 (%2)", build.compiler->name, CompileModeNames[(int)build.compile_mode]);
    LogInfo("Output directory: '%1'", build.output_directory);
    if (!MakeDirectoryRec(build.output_directory))
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
    if (!builder.Build(jobs, verbose))
        return 1;

    // Run?
    if (run_target) {
        if (run_here && !SetWorkingDirectory(start_directory))
            return 1;

        const char *target_filename = builder.target_filenames.FindValue(run_target->name, nullptr);
        return RunTarget(*run_target, target_filename, run_arguments, verbose);
    } else {
        return 0;
    }
}

}
