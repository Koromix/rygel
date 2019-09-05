// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "build.hh"
#include "compiler.hh"
#include "target.hh"

#ifndef _WIN32
    #include <unistd.h>
#endif

namespace RG {

struct Toolchain {
    const Compiler *compiler;
    BuildMode build_mode;

    operator FmtArg() const
    {
        FmtArg arg = {};
        arg.type = FmtArg::Type::Buffer;
        Fmt(arg.value.buf, "%1_%2", compiler->name, BuildModeNames[(int)build_mode]);
        return arg;
    }
};

static bool ParseToolchainSpec(Span<const char> str, Toolchain *out_toolchain)
{
    Span<const char> build_mode_str;
    Span<const char> compiler_str = SplitStr(str, '_', &build_mode_str);

    Toolchain toolchain = *out_toolchain;

    bool valid = true;
    if (compiler_str.len) {
        const Compiler *const *ptr = FindIf(Compilers,
                                            [&](const Compiler *compiler) { return TestStr(compiler->name, compiler_str); });

        if (ptr) {
            toolchain.compiler = *ptr;
        } else {
            LogError("Unknown compiler '%1'", compiler_str);
            valid = false;
        }
    }
    if (build_mode_str.ptr > compiler_str.end()) {
        const char *const *name = FindIf(BuildModeNames,
                                         [&](const char *name) { return TestStr(name, build_mode_str); });

        if (name) {
            toolchain.build_mode = (BuildMode)(name - BuildModeNames);
        } else {
            LogError("Unknown build mode '%1'", build_mode_str);
            valid = false;
        }
    }
    if (!valid)
        return false;

    *out_toolchain = toolchain;
    return true;
}

static int RunTarget(const Target &target, const char *target_filename,
                     Span<const char *const> arguments, bool verbose)
{
    if (target.type != TargetType::Executable) {
        LogError("Cannot run non-executable target '%1'", target.name);
        return 1;
    }

    HeapArray<char> cmd_buf;

    // FIXME: Just like the code in compiler.cc, command-line escaping is
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
    const char *output_directory = nullptr;
    Toolchain toolchain = {Compilers[0], BuildMode::Debug};
    bool enable_pch = true;
    int jobs = std::min(GetCoreCount() + 1, RG_ASYNC_MAX_WORKERS + 1);
    bool verbose = false;
    const char *run_target_name = nullptr;
    Span<const char *> run_arguments = {};

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: felix build [options] [target...]
       felix build [options] target --run [arguments...]

Options:
    -C, --config <filename>      Set configuration filename
                                 (default: FelixBuild.ini)
    -O, --output <directory>     Set output directory
                                 (default: bin/<toolchain>)

    -t, --toolchain <toolchain>  Set toolchain, see below
                                 (default: %1)
        --no_pch                 Disable header precompilation (PCH)

    -j, --jobs <count>           Set maximum number of parallel jobs
                                 (default: %2)

    -v, --verbose                Show detailed build commands

        --run <target>           Run target after successful build
                                 (all remaining arguments are passed as-is)

Available toolchains:)", toolchain, jobs);
        for (const Compiler *compiler: Compilers) {
            for (const char *mode_name: BuildModeNames) {
                PrintLn(fp, "    %1_%2", compiler->name, mode_name);
            }
        }
        PrintLn(fp, R"(
You can omit either part of the toolchain string (e.g. 'Clang' and '_Fast' are both valid).)");
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
                output_directory = opt.current_value;
            } else if (opt.Test("-t", "--toolchain", OptionType::Value)) {
                if (!ParseToolchainSpec(opt.current_value, &toolchain))
                    return 1;
            } else if (opt.Test("--no_pch")) {
                enable_pch = false;
            } else if (opt.Test("-j", "--jobs", OptionType::Value)) {
                if (!ParseDec(opt.current_value, &jobs))
                    return 1;
                if (jobs < 1) {
                    LogError("Jobs count cannot be < 1");
                    return 1;
                }
            } else if (opt.Test("-v", "--verbose")) {
                verbose = true;
            } else if (opt.Test("--run", OptionType::Value)) {
                run_target_name = opt.current_value;
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
    if (output_directory) {
        output_directory = NormalizePath(output_directory, start_directory, &temp_alloc).ptr;
    } else {
        output_directory = Fmt(&temp_alloc, "%1%/bin%/%2", GetWorkingDirectory(), toolchain).ptr;
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

    // We're ready to output stuff
    if (!MakeDirectoryRec(output_directory))
        return 1;
    LogInfo("Output directory: '%1'", output_directory);

    // Disable PCH?
    if (enable_pch && !toolchain.compiler->Supports(CompilerFlag::PCH)) {
        bool using_pch = std::any_of(enabled_targets.begin(), enabled_targets.end(),
                                     [](const Target *target) {
            return target->c_pch_filename || target->cxx_pch_filename;
        });

        if (using_pch) {
            LogError("PCH does not work correctly with %1 compiler (ignoring)",
                     toolchain.compiler->name);
            enable_pch = false;
        }
    }
    if (!enable_pch) {
        for (Target &target: target_set.targets) {
            target.c_pch_filename = nullptr;
            target.cxx_pch_filename = nullptr;
        }
    }

    // LTO?
    if ((toolchain.build_mode == BuildMode::LTO || toolchain.build_mode == BuildMode::StaticLTO) &&
            !toolchain.compiler->Supports(CompilerFlag::LTO)) {
        LogError("LTO does not work correctly with %1 compiler", toolchain.compiler->name);
        return 1;
    }

    // Create build commands
    BuildSet build_set;
    {
        BuildSetBuilder build_set_builder(output_directory, toolchain.compiler);
        build_set_builder.build_mode = toolchain.build_mode;
        build_set_builder.version_str = BuildGitVersionString(&temp_alloc);
        if (!build_set_builder.version_str) {
            LogError("Git version string will be null");
        }

        for (const Target *target: enabled_targets) {
            if (!build_set_builder.AppendTargetCommands(*target))
                return 1;
        }

        build_set_builder.Finish(&build_set);
    }

    // Build
    if (!RunBuildCommands(build_set.commands, jobs, verbose))
        return 1;

    // Run?
    if (run_target) {
        const char *target_filename = build_set.target_filenames.FindValue(run_target->name, nullptr);
        return RunTarget(*run_target, target_filename, run_arguments, verbose);
    } else {
        return 0;
    }
}

}
