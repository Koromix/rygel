// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "build.hh"
#include "config.hh"
#include "toolchain.hh"

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
                                 (default: working directory)

    -t, --toolchain <toolchain>  Set toolchain, see below
                                 (default: %1)
    -m, --mode     <mode>        Set build mode, see below
                                 (default: %2)
       --disable_pch             Disable header precompilation (PCH)

    -j, --jobs <count>           Set maximum number of parallel jobs
                                 (default: number of cores)

    -v, --verbose                Show detailed build commands

Available toolchains:)", Toolchains[0]->name, BuildModeNames[0]);
        for (const Toolchain *toolchain: Toolchains) {
            PrintLn(fp, "    %1", toolchain->name);
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
    const Toolchain *toolchain = Toolchains[0];
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
            } else if (opt.Test("-t", "--toolchain", OptionType::Value)) {
                const Toolchain *const *ptr = FindIf(Toolchains,
                                                     [&](const Toolchain *toolchain) { return TestStr(toolchain->name, opt.current_value); });
                if (!ptr) {
                    LogError("Unknown toolchain '%1'", opt.current_value);
                    return 1;
                }

                toolchain = *ptr;
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

    Config config;
    if (!LoadConfig(config_filename, &config))
        return 1;

    // Default targets
    if (!target_names.len) {
        for (const TargetConfig &target_config: config.targets) {
            if (target_config.type == TargetType::Executable) {
                target_names.Append(target_config.name);
            }
        }
    }

    // Disable PCH?
#ifdef _WIN32
    if (!disable_pch && toolchain == &GnuToolchain) {
        bool has_pch = std::any_of(config.targets.begin(), config.targets.end(),
                                   [](const TargetConfig &target_config) {
            return target_config.c_pch_filename || target_config.cxx_pch_filename;
        });

        if (has_pch) {
            LogError("PCH does not work correctly with MinGW (ignoring)");
            disable_pch = true;
        }
    }
#endif
    if (disable_pch) {
        for (TargetConfig &target_config: config.targets) {
            target_config.c_pch_filename = nullptr;
            target_config.cxx_pch_filename = nullptr;
        }
    }

    // Directories
    const char *start_directory = DuplicateString(GetWorkingDirectory(), &temp_alloc).ptr;
    if (output_directory) {
        output_directory = NormalizePath(output_directory, start_directory, &temp_alloc).ptr;
    } else {
        output_directory = start_directory;
    }

    // Change to root directory
    {
        Span<const char> root_directory;
        SplitStrReverseAny(config_filename, PATH_SEPARATORS, &root_directory);

        if (root_directory.len) {
            const char *root_directory0 = DuplicateString(root_directory, &temp_alloc).ptr;
            if (!SetWorkingDirectory(root_directory0))
                return 1;
        }
    }

    // Gather target information
    TargetSet target_set;
    {
        TargetSetBuilder target_set_builder(output_directory);
        target_set_builder.resolve_import = [&](const char *import_name) {
            const TargetConfig *import_config = config.targets_map.FindValue(import_name, nullptr);
            return import_config;
        };

        HashSet<const char *> handled_set;
        for (const char *target_name: target_names) {
            if (!handled_set.Append(target_name).second) {
                LogError("Target '%1' is specified multiple times", target_name);
                return 1;
            }

            const TargetConfig *target_config = config.targets_map.FindValue(target_name, nullptr);
            if (!target_config) {
                LogError("Target '%1' does not exist", target_name);
                return 1;
            }

            if (!target_set_builder.CreateTarget(*target_config))
                return 1;
        }

        target_set_builder.Finish(&target_set);
    }

    // Create build commands
    BuildSet build_set;
    {
        BuildSetBuilder build_set_builder(toolchain, build_mode, output_directory);

        for (const TargetData &target: target_set.targets) {
            if (!build_set_builder.AppendTargetCommands(target))
                return 1;
        }

        build_set_builder.Finish(&build_set);
    }

    // Run build
    if (!RunBuildCommands(build_set.commands, verbose))
        return 1;

    LogInfo("Done!");
    return 0;
}
