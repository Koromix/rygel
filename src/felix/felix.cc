// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "build.hh"
#include "compiler.hh"

int main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: felix [options] [target]

Options:
    -C, --config <filename>      Set configuration filename
                                 (default: FelixBuild.ini)
    -O, --output <directory>     Set output directory
                                 (default: bin/<compiler>_<mode>)

    -c, --compiler <compiler>    Set compiler, see below
                                 (default: %1)
    -m, --mode <mode>            Set build mode, see below
                                 (default: %2)
       --disable_pch             Disable header precompilation (PCH)

    -j, --jobs <count>           Set maximum number of parallel jobs
                                 (default: number of cores)

    -v, --verbose                Show detailed build commands

Available compilers:)", Compilers[0]->name, BuildModeNames[0]);
        for (const Compiler *compiler: Compilers) {
            PrintLn(fp, "    %1", compiler->name);
        }
        PrintLn(fp, R"(
Available build modes:)");
        for (const char *mode_name: BuildModeNames) {
            PrintLn(fp, "    %1", mode_name);
        }
    };

    HeapArray<const char *> target_names;
    const char *config_filename = "FelixBuild.ini";
    const char *output_directory = nullptr;
    const Compiler *compiler = Compilers[0];
    BuildMode build_mode = (BuildMode)0;
    bool disable_pch = false;
    bool verbose = false;
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config", OptionType::Value)) {
                config_filename = opt.current_value;
            } else if (opt.Test("-O", "--output", OptionType::Value)) {
                output_directory = opt.current_value;
            } else if (opt.Test("-c", "--compiler", OptionType::Value)) {
                const Compiler *const *ptr = FindIf(Compilers,
                                                     [&](const Compiler *compiler) { return TestStr(compiler->name, opt.current_value); });
                if (!ptr) {
                    LogError("Unknown compiler '%1'", opt.current_value);
                    return 1;
                }

                compiler = *ptr;
            } else if (opt.Test("-m", "--mode", OptionType::Value)) {
                const char *const *name = FindIf(BuildModeNames,
                                                 [&](const char *name) { return TestStr(name, opt.current_value); });
                if (!name) {
                    LogError("Unknown build mode '%1'", opt.current_value);
                    return 1;
                }

                build_mode = (BuildMode)(name - BuildModeNames);
            } else if (opt.Test("--disable_pch")) {
                disable_pch = true;
            } else if (opt.Test("-j", "--jobs", OptionType::Value)) {
                int max_threads;
                if (!ParseDec(opt.current_value, &max_threads))
                    return 1;
                if (max_threads < 1) {
                    LogError("Jobs count cannot be < 1");
                    return 1;
                }

                Async::SetThreadCount(max_threads);
            } else if (opt.Test("-v", "--verbose")) {
                verbose = true;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        opt.ConsumeNonOptions(&target_names);
    }

    // Change to root directory
    const char *start_directory = DuplicateString(GetWorkingDirectory(), &temp_alloc).ptr;
    {
        Span<const char> root_directory;
        config_filename = SplitStrReverseAny(config_filename, PATH_SEPARATORS, &root_directory).ptr;

        if (root_directory.len) {
            const char *root_directory0 = DuplicateString(root_directory, &temp_alloc).ptr;
            if (!SetWorkingDirectory(root_directory0))
                return 1;
        }
    }
    if (output_directory) {
        output_directory = NormalizePath(output_directory, start_directory, &temp_alloc).ptr;
    } else {
        output_directory = Fmt(&temp_alloc, "%1%/bin%/%2_%3", start_directory,
                               compiler->name, BuildModeNames[(int)build_mode]).ptr;
    }

    // Load configuration file
    TargetSet target_set;
    {
        TargetSetBuilder target_set_builder(output_directory);

        if (!target_set_builder.LoadFiles(config_filename))
            return 1;
        target_set_builder.Finish(&target_set);
    }
    if (!target_set.targets.len) {
        LogError("There are no targets");
        return 1;
    }

    // Default targets
    if (!target_names.len) {
        for (const Target &target: target_set.targets) {
            if (target.type == TargetType::Executable) {
                target_names.Append(target.name);
            }
        }
    }

    // Select targets and their dependencies (imports)
    HeapArray<const Target *> enabled_targets;
    {
        HashSet<const char *> handled_set;

        for (const char *target_name: target_names) {
            const Target *target = target_set.targets_map.FindValue(target_name, nullptr);

            if (handled_set.Append(target->name).second) {
                for (const char *import_name: target->imports) {
                    if (handled_set.Append(import_name).second) {
                        const Target *import = target_set.targets_map.FindValue(import_name, nullptr);
                        enabled_targets.Append(import);
                    }
                }
                enabled_targets.Append(target);
            }
        }
    }

    // We're ready to output stuff
    if (!MakeDirectoryRec(output_directory))
        return 1;

    // Disable PCH?
#ifdef _WIN32
    if (!disable_pch && compiler == &GnuCompiler) {
        bool has_pch = std::any_of(target_set.targets.begin(), target_set.targets.end(),
                                   [](const Target &target) {
            return target.c_pch_filename || target.cxx_pch_filename;
        });

        if (has_pch) {
            LogError("PCH does not work correctly with MinGW (ignoring)");
            disable_pch = true;
        }
    }
#endif
    if (disable_pch) {
        for (Target &target: target_set.targets) {
            target.pch_objects.Clear();
            target.c_pch_filename = nullptr;
            target.cxx_pch_filename = nullptr;
        }
    }

    // Create build commands
    BuildSet build_set;
    {
        BuildSetBuilder build_set_builder(compiler, build_mode);

        for (const Target *target: enabled_targets) {
            if (!build_set_builder.AppendTargetCommands(*target))
                return 1;
        }

        build_set_builder.Finish(&build_set);
    }

    // Run build
    return !RunBuildCommands(build_set.commands, verbose);
}
