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

static int RunTarget(const Target &target, const char *target_filename,
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
    BuildSettings settings = {};
    bool enable_pch = true;
    bool verbose = false;
    const char *run_target_name = nullptr;
    Span<const char *> run_arguments = {};
    bool run_here = false;

    // Find default settings
    settings.compiler = FindIf(Compilers, [](const Compiler *compiler) { return compiler->Test(); });
    settings.jobs = std::min(GetCoreCount() + 1, RG_ASYNC_MAX_WORKERS + 1);

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: felix build [options] [target...]
       felix build [options] target --run [arguments...]

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

    -v, --verbose                Show detailed build commands

        --run <target>           Run target after successful build
                                 (all remaining arguments are passed as-is)
        --run_here <target>      Same thing, but run from current directory

Supported compilers:)", settings.compiler ? settings.compiler->name : "?",
                        CompileModeNames[(int)settings.compile_mode], settings.jobs);

        for (const Compiler *compiler: Compilers) {
            PrintLn(fp, "    %1", compiler->name);
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
                settings.output_directory = opt.current_value;
            } else if (opt.Test("-c", "--compiler", OptionType::Value)) {
                settings.compiler =
                    FindIf(Compilers, [&](const Compiler *compiler) { return TestStr(compiler->name, opt.current_value); });

                if (!settings.compiler) {
                    LogError("Unknown compiler '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("-m", "--mode", OptionType::Value)) {
                const char *const *ptr =
                    FindIfPtr(CompileModeNames,
                              [&](const char *name) { return TestStr(name, opt.current_value); });

                if (ptr) {
                    settings.compile_mode = (CompileMode)(ptr - CompileModeNames);
                } else {
                    LogError("Unknown build mode '%1'", opt.current_value);
                    return 1;
                }
            } else if (opt.Test("--no_pch")) {
                enable_pch = false;
            } else if (opt.Test("-j", "--jobs", OptionType::Value)) {
                if (!ParseDec(opt.current_value, &settings.jobs))
                    return 1;
                if (settings.jobs < 1) {
                    LogError("Jobs count cannot be < 1");
                    return 1;
                }
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

    if (!settings.compiler) {
        LogError("No compiler is available");
        return 1;
    } else if (!settings.compiler->Test()) {
        LogError("Cannot find %1 compiler (binary = %2)",
                 settings.compiler->name, settings.compiler->binary);
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
    if (settings.output_directory) {
        settings.output_directory = NormalizePath(settings.output_directory, start_directory, &temp_alloc).ptr;
    } else {
        settings.output_directory = Fmt(&temp_alloc, "%1%/bin%/%2_%3", GetWorkingDirectory(),
                                        settings.compiler->name, CompileModeNames[(int)settings.compile_mode]).ptr;
    }

    // Load configuration file
    TargetSet target_set;
    if (!LoadTargetSet(config_filename, &target_set))
        return 1;

    // Default targets
    if (!target_names.len) {
        for (const Target &target: target_set.targets) {
            if (target.enable_by_default) {
                target_names.Append(target.name);
            }
        }

        if (!target_names.len) {
            LogError("There are no targets");
            return 1;
        }
    }

    // Select targets and their dependencies (imports)
    HeapArray<const Target *> enabled_targets;
    const Target *run_target = nullptr;
    {
        HashSet<const char *> handled_set;

        bool valid = true;
        for (const char *target_name: target_names) {
            if (handled_set.Append(target_name).second) {
                const Target *target = target_set.targets_map.FindValue(target_name, nullptr);
                if (!target) {
                    LogError("Target '%1' does not exist", target_name);
                    valid = false;
                    continue;
                }

                for (const char *import_name: target->imports) {
                    if (handled_set.Append(import_name).second) {
                        const Target *import = target_set.targets_map.FindValue(import_name, nullptr);
                        enabled_targets.Append(import);
                    }
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

    // Disable PCH?
    if (!enable_pch) {
        for (Target &target: target_set.targets) {
            target.c_pch_filename = nullptr;
            target.cxx_pch_filename = nullptr;
        }
    }

    // We're ready to output stuff
    LogInfo("Root directory: '%1'", GetWorkingDirectory());
    LogInfo("Output directory: '%1'", settings.output_directory);
    if (!MakeDirectoryRec(settings.output_directory))
        return 1;

    // Build version string from git commit (date, hash)
    settings.version_str = BuildGitVersionString(&temp_alloc);
    if (!settings.version_str) {
        LogError("Git version string will be null");
    }

    // The detection of SIGINT (or the Win32 equivalent) by WaitForInterruption()
    // remains after timing out, which will allow RunBuildNodes() to clean up files
    // produced by interrupted commands.
    WaitForInterruption(0);

    // Build stuff!
    Builder builder(settings);
    for (const Target *target: enabled_targets) {
        if (!builder.AddTarget(*target))
            return 1;
    }
    if (!builder.Build(verbose))
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
