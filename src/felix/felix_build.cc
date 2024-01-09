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
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/libcc/libcc.hh"
#include "build.hh"
#include "compiler.hh"
#include "git.hh"
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

struct BuildPreset {
    const char *name;

    HostSpecifier host_spec;
    bool changed_spec;

    BuildSettings build;
    uint32_t maybe_features = 0;
};

struct EnabledTarget {
    const TargetInfo *target;
    const char *version = nullptr;
};

static int RunTarget(const char *target_filename, Span<const char *const> arguments)
{
    LogInfo("Run '%1'", target_filename);
    LogInfo("%!D..--------------------------------------------------%!0");

#ifdef _WIN32
    HeapArray<char> cmd;

    Fmt(&cmd, "\"%1\"", target_filename);

    // Windows command line quoting rules are batshit crazy
    for (const char *arg: arguments) {
        bool quote = strchr(arg, ' ');

        cmd.Append(quote ? " \"" : " ");
        for (Size i = 0; arg[i]; i++) {
            if (arg[i] == '"') {
                cmd.Append('\\');
            }
            cmd.Append(arg[i]);
        }
        cmd.Append(quote ? "\"" : "");
    }
    cmd.Append(0);

    wchar_t target_filename_w[4096];
    HeapArray<wchar_t> cmd_w;
    cmd_w.AppendDefault(cmd.len + 1);
    if (ConvertUtf8ToWin32Wide(target_filename, target_filename_w) < 0)
        return 127;
    if (ConvertUtf8ToWin32Wide(cmd, cmd_w) < 0)
        return 127;

    // We could use ExecuteCommandLine, but for various reasons (detailed in its Win32
    // implementation) it does not handle Ctrl+C gently.
    STARTUPINFOW startup_info = {};
    PROCESS_INFORMATION process_info;
    if (!CreateProcessW(target_filename_w, cmd_w.ptr, nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &startup_info, &process_info)) {
        LogError("Failed to start process: %1", GetWin32ErrorString());
        return 127;
    }

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

static bool ParseHostString(Span<const char> str, Allocator *alloc, HostSpecifier *out_spec)
{
    Span<const char> platform = SplitStr(str, ':', &str);
    Span<const char> architecture = SplitStr(str, ':', &str);
    Span<const char> cc = SplitStr(str, ':', &str);
    Span<const char> ld = SplitStr(str, ':', &str);

    // Defaults
    out_spec->platform = NativePlatform;
    out_spec->architecture = NativeArchitecture;

    // Short form with architecture but native platform
    if (TestStrI(platform, "Native")) {
        out_spec->architecture = NativeArchitecture;
    } else if (OptionToEnumI(HostArchitectureNames, platform, &out_spec->architecture)) {
        out_spec->platform = NativePlatform;

        platform = "";
        ld = cc;
        cc = architecture;
        architecture = "";
    }

    if (architecture.len) {
        if (TestStrI(architecture, "Native")) {
            out_spec->architecture = NativeArchitecture;
        } else if (!OptionToEnumI(HostArchitectureNames, architecture, &out_spec->architecture)) {
            out_spec->architecture = NativeArchitecture;

            ld = cc;
            cc = architecture;
        }
    }

    if (platform.len) {
        if (TestStrI(platform, "Native")) {
            out_spec->platform = NativePlatform;
        } else {
            unsigned int platforms = ParseSupportedPlatforms(platform);

            if (!platforms) {
                return false;
            } else if (PopCount(platforms) > 1) {
                LogError("Ambiguous platform '%1' (multiple matches)", platform);
                return false;
            } else {
                int ctz = CountTrailingZeros(platforms);
                out_spec->platform = (HostPlatform)ctz;
            }
        }
    }

    out_spec->cc = cc.len ? NormalizePath(cc, alloc).ptr : nullptr;
    out_spec->ld = ld.len ? DuplicateString(ld, alloc).ptr : nullptr;

    return true;
}

static bool ParseFeatureString(Span<const char> str, uint32_t *out_features, uint32_t *out_maybe)
{
    while (str.len) {
        Span<const char> part = TrimStr(SplitStrAny(str, " ,", &str), " ");

        bool maybe = false;
        bool enable = true;

        if (part.len && part[0] == '-') {
            part = part.Take(1, part.len - 1);
            enable = false;
        } else if (part.len && part[0] == '+') {
            part = part.Take(1, part.len - 1);
            enable = true;
        } else if (part.len && part[0] == '?') {
            part = part.Take(1, part.len - 1);
            maybe = true;
        }

        if (TestStrI(part, "All") && !maybe) {
            *out_features = enable ? 0xFFFFFFFFul : 0;
        } else if (part.len && !OptionToFlagI(CompileFeatureOptions, part,
                                              maybe ? out_maybe : out_features, enable)) {
            LogError("Unknown target feature '%1'", part);
            return false;
        }
    }

    return true;
}

static bool LoadPresetFile(const char *basename, Allocator *alloc,
                           const char **out_preset_name, HostSpecifier *out_spec,
                           int *out_jobs, HeapArray<BuildPreset> *out_presets)
{
    // This function assumes the file is in the current working directory
    RG_ASSERT(!strpbrk(basename, RG_PATH_SEPARATORS));

    StreamReader st(basename);
    if (!st.IsValid())
        return false;

    IniParser ini(&st);
    ini.PushLogFilter();
    RG_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (!prop.section.len) {
                if (prop.key == "Preset") {
                    *out_preset_name = DuplicateString(prop.value, alloc).ptr;
                } else if (prop.key == "Host") {
                    valid &= ParseHostString(prop.value, alloc, out_spec);

                    for (BuildPreset &preset: *out_presets) {
                        if (!preset.changed_spec) {
                            preset.host_spec = *out_spec;
                        }
                    }
                } else if (prop.key == "Jobs") {
                    if (ParseInt(prop.value, out_jobs)) {
                        if (*out_jobs < 1) {
                            LogError("Jobs count cannot be < 1");
                            valid = false;
                        }
                    } else {
                        valid = false;
                    }
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else {
                BuildPreset *preset = std::find_if(out_presets->begin(), out_presets->end(),
                                                   [&](const BuildPreset &preset) { return TestStr(preset.name, prop.section); });

                if (preset == out_presets->end()) {
                    preset = out_presets->AppendDefault();
                    preset->name = DuplicateString(prop.section, alloc).ptr;
                    preset->host_spec = *out_spec;
                }

                if (prop.key == "Template") {
                    const BuildPreset *base = std::find_if(out_presets->begin(), preset,
                                                           [&](const BuildPreset &preset) { return TestStr(preset.name, prop.value); });

                    if (base < preset) {
                        const char *name = preset->name;

                        *preset = *base;
                        preset->name = name;
                    } else {
                        LogError("Preset '%1' does not exist", prop.value);
                        valid = false;
                    }

                    if (!ini.NextInSection(&prop))
                        continue;
                }

                do {
                    if (prop.key == "Template") {
                        LogError("Preset template cannot be changed");
                        valid = false;
                    } else if (prop.key == "Directory") {
                        preset->build.output_directory = NormalizePath(prop.value, GetWorkingDirectory(), alloc).ptr;
                    } else if (prop.key == "Host") {
                        valid &= ParseHostString(prop.value, alloc, &preset->host_spec);
                        preset->changed_spec = true;
                    } else if (prop.key == "Features") {
                        valid &= ParseFeatureString(prop.value.ptr, &preset->build.features,
                                                    &preset->maybe_features);
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            }
        }
    }
    if (!ini.IsValid() || !valid)
        return false;

    return true;
}

int RunBuild(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    HeapArray<const char *> selectors;
    const char *config_filename = nullptr;
    bool load_presets = true;
    bool load_user = true;
    const char *preset_name = nullptr;
    HostSpecifier host_spec = {};
    BuildSettings build = {};
    uint32_t maybe_features = 0;
    int jobs = std::min(GetCoreCount() + 1, RG_ASYNC_MAX_THREADS);
    int quiet = 0;
    bool verbose = false;
    const char *run_target_name = nullptr;
    Span<const char *> run_arguments = {};
    bool run_here = false;

    const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 build [options] [target...]
       %1 build [options] --run target [arguments...]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration filename
                                 %!D..(default: FelixBuild.ini)%!0
    %!..+-O, --output_dir <dir>%!0       Set output directory
                                 %!D..(default: bin/<preset>)%!0

        %!..+--no_presets%!0             Ignore all presets
                                 %!D..(FelixBuild.ini.presets, FelixBuild.ini.user)%!0
        %!..+--no_user%!0                Ignore user presets
                                 %!D..(FelixBuild.ini.user)%!0
    %!..+-p, --preset <preset>%!0        Select specific preset

    %!..+-h, --host <host>%!0            Override platform, compiler and/or linker
    %!..+-f, --features <features>%!0    Override compilation features
                                 %!D..(start with -All to reset and set only new flags)%!0

        %!..+--qmake_path <path>%!0      Set explicit path to QMake
        %!..+--esbuild_path <path>%!0    Set explicit path to esbuild binary

    %!..+-e, --environment%!0            Use compiler flags found in environment (CFLAGS, LDFLAGS, etc.)


    %!..+-j, --jobs <count>%!0           Set maximum number of parallel jobs
                                 %!D..(default: %2)%!0
    %!..+-s, --stop_after_error%!0       Continue build after errors
        %!..+--rebuild%!0                Force rebuild all files

    %!..+-q, --quiet%!0                  Reduce felix verbosity (use -qq for silence)
    %!..+-v, --verbose%!0                Show detailed build commands
    %!..+-n, --dry_run%!0                Fake command execution

        %!..+--run <target>%!0           Run target after successful build
                                 %!D..(all remaining arguments are passed as-is)%!0
        %!..+--run_here <target>%!0      Same thing, but run from current directory

Supported platforms:)", FelixTarget, jobs);

        for (const char *name: HostPlatformNames) {
            PrintLn(fp, "    %!..+%1%!0", name);
        }

        PrintLn(fp, R"(
Supported compilers:)");

        for (const SupportedCompiler &supported: SupportedCompilers) {
            if (supported.cc) {
                PrintLn(fp, "    %!..+%1%!0 Binary: %2", FmtArg(supported.name).Pad(28), supported.cc);
            } else {
                PrintLn(fp, "    %!..+%1%!0", supported.name);
            }
        }

        PrintLn(fp, R"(
Use %!..+--host=<host>%!0 to specify a custom platform, such as: %!..+felix --host=Teensy35%!0.
You can also use %!..+--host=:<binary>%!0 to specify a custom C compiler, such as: %!..+felix --host=:clang-11%!0.
Felix will use the matching C++ compiler automatically. Finally, you can also use this option to
change the linker: %!..+felix --host=:clang-11:lld-11%!0 or %!..+felix --host=::gold%!0.

Supported compiler features:)");

        for (const OptionDesc &desc: CompileFeatureOptions) {
            PrintLn(fp, "    %!..+%1%!0  %2", FmtArg(desc.name).Pad(27), desc.help);
        }

        Print(fp, R"(
Felix can also run the following special commands:
    %!..+build%!0                        Build C and C++ projects %!D..(default)%!0)");
#ifndef _WIN32
        Print(fp, R"(
    %!..+macify%!0                       Create macOS bundle app from binary)");
#endif
        PrintLn(fp, R"(
    %!..+pack%!0                         Pack assets to C source file and other formats

For help about those commands, type: %!..+%1 <command> --help%!0)", FelixTarget);
    };

    // Find config filename
    {
        OptionParser opt(arguments, OptionMode::Skip);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                if (IsDirectory(opt.current_value)) {
                    config_filename = Fmt(&temp_alloc, "%1%/FelixBuild.ini", TrimStrRight(opt.current_value, RG_PATH_SEPARATORS)).ptr;
                } else {
                    config_filename = opt.current_value;
                }
            } else if (opt.Test("--no_presets")) {
                load_presets = false;
                load_user = false;
            } else if (opt.Test("--no_user")) {
                load_user = false;
            } else if (opt.Test("-p", "--preset", OptionType::Value)) {
                preset_name = opt.current_value;
            } else if (opt.Test("--run") || opt.Test("--run_here")) {
                break;
            } else if (opt.TestHasFailed()) {
                return 1;
            }
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

    if (!TestFile(config_filename, FileType::File)) {
        LogError("Cannot find FelixBuild.ini");
        return 1;
    }

    // Load customized presets
    HeapArray<BuildPreset> presets;
    {
        const char *default_preset = nullptr;

        if (load_presets) {
            const char *filename = Fmt(&temp_alloc, "%1.presets", config_filename).ptr;

            if (TestFile(filename) && !LoadPresetFile(filename, &temp_alloc, &default_preset,
                                                      &host_spec, &jobs, &presets))
                return 1;
        }
        if (load_user) {
            const char *filename = Fmt(&temp_alloc, "%1.user", config_filename).ptr;

            if (TestFile(filename) && !LoadPresetFile(filename, &temp_alloc, &default_preset,
                                                      &host_spec, &jobs, &presets))
                return 1;
        }

        preset_name = preset_name ? preset_name : default_preset;
    }

    // Find selected preset
    {
        const BuildPreset *preset;

        if (preset_name) {
            if (!load_presets) {
                LogError("Option --preset cannot be used with --no_presets");
                return 1;
            }

            preset = std::find_if(presets.begin(), presets.end(),
                                  [&](const BuildPreset &preset) { return TestStr(preset.name, preset_name); });
            if (preset == presets.end()) {
                LogError("Preset '%1' does not exist", preset_name);
                return 1;
            }
        } else {
            preset = presets.len ? &presets[0] : nullptr;
        }

        if (preset) {
            preset_name = preset->name;
            host_spec = preset->host_spec;
            build = preset->build;
            maybe_features = preset->maybe_features;
        }
    }

    // Parse environment variables
    if (getenv("FELIX_HOST") && !ParseHostString(getenv("FELIX_HOST"), &temp_alloc, &host_spec))
        return 1;
    if (getenv("FELIX_FEATURES") && !ParseFeatureString(getenv("FELIX_FEATURES"), &build.features, &maybe_features))
        return 1;

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

            if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("--no_presets")) {
                // Already handled
            } else if (opt.Test("--no_user")) {
                // Already handled
            } else if (opt.Test("-p", "--preset", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-O", "--output_dir", OptionType::Value)) {
                build.output_directory = opt.current_value;
            } else if (opt.Test("-h", "--host", OptionType::Value)) {
                if (!ParseHostString(opt.current_value, &temp_alloc, &host_spec))
                    return 1;
            } else if (opt.Test("-f", "--features", OptionType::Value)) {
                if (!ParseFeatureString(opt.current_value, &build.features, &maybe_features))
                    return 1;
            } else if (opt.Test("--qmake_path", OptionType::Value)) {
                build.qmake_binary = opt.current_value;
            } else if (opt.Test("--esbuild_path", OptionType::Value)) {
                build.esbuild_binary = opt.current_value;
            } else if (opt.Test("-e", "--environment")) {
                build.env = true;
            } else if (opt.Test("-j", "--jobs", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &jobs))
                    return 1;
                if (jobs < 1) {
                    LogError("Jobs count cannot be < 1");
                    return 1;
                }
            } else if (opt.Test("-s", "--stop_after_error")) {
                build.stop_after_error = true;
            } else if (opt.Test("--rebuild")) {
                build.rebuild = true;
            } else if (opt.Test("-q", "--quiet")) {
                quiet++;
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
                opt.LogUnknownError();
                return 1;
            }
        }

        if (run_target_name) {
            selectors.Append(run_target_name);
            run_arguments = opt.GetRemainingArguments();
        }
    }

    if (quiet >= 2) {
        SetLogHandler([](LogLevel level, const char *ctx, const char *msg) {
            if (level != LogLevel::Info) {
                DefaultLogHandler(level, ctx, msg);
            }
        });
    }

    // Initialize and check compiler
    std::unique_ptr<const Compiler> compiler = PrepareCompiler(host_spec);
    if (!compiler)
        return 1;
    if (!compiler->CheckFeatures(build.features, maybe_features, &build.features))
        return 1;
    build.compiler = compiler.get();

    // Output directory
    if (build.output_directory) {
        build.output_directory = NormalizePath(build.output_directory, start_directory, &temp_alloc).ptr;
    } else {
        const char *basename = preset_name ? preset_name : build.compiler->name;
        build.output_directory = Fmt(&temp_alloc, "%1%/bin%/%2", GetWorkingDirectory(), basename).ptr;
    }

    // Load configuration file
    if (!quiet) {
        LogInfo("Loading targets...");
    }
    TargetSet target_set;
    if (!LoadTargetSet(config_filename, host_spec.platform, host_spec.architecture, &target_set))
        return 1;
    if (!target_set.targets.len) {
        LogError("Configuration file does not contain any target");
        return 1;
    }

    // Select targets
    HeapArray<EnabledTarget> enabled_targets;
    HeapArray<const SourceFileInfo *> enabled_sources;
    if (selectors.len) {
        bool valid = true;
        HashSet<const char *> handled_set;

        for (const char *selector: selectors) {
            bool match = false;

            // Match targets
            for (const TargetInfo &target: target_set.targets) {
                if (MatchPathSpec(target.name, selector)) {
                    bool inserted;
                    handled_set.TrySet(target.name, &inserted);

                    if (inserted) {
                        if (!target.TestPlatforms(host_spec.platform)) {
                            LogError("Cannot build '%1' for platform '%2'",
                                     target.name, HostPlatformNames[(int)host_spec.platform]);
                            valid = false;
                        }

                        enabled_targets.Append({ &target });
                        match = true;
                    }
                }
            }

            // Match source files
            for (const SourceFileInfo &src: target_set.sources) {
                if (MatchPathSpec(src.filename, selector)) {
                    bool inserted;
                    handled_set.TrySet(src.filename, &inserted);

                    if (inserted) {
                        if (src.target->TestPlatforms(host_spec.platform)) {
                            enabled_sources.Append(&src);
                            match = true;
                        } else {
                            LogError("Cannot build '%1' for platform '%2' (ignoring)",
                                     src.filename, HostPlatformNames[(int)host_spec.platform]);
                        }
                    }
                }
            }

            if (!match) {
                LogError("Selector '%1' does not match anything", selector);
                return 1;
            }
        }

        if (!valid)
            return 1;
    } else {
        for (const TargetInfo &target: target_set.targets) {
            if (target.enable_by_default && target.TestPlatforms(host_spec.platform)) {
                enabled_targets.Append({ &target });
            }
        }

        if (!enabled_targets.len) {
            LogError("No target to build by default for platform '%1'", HostPlatformNames[(int)host_spec.platform]);
            return 1;
        }
    }

    // Find and check target used with --run
    const TargetInfo *run_target = nullptr;
    if (run_target_name) {
        if (host_spec.platform != NativePlatform) {
            LogError("Cannot use --run when cross-compiling");
            return 1;
        }

        run_target = target_set.targets_map.FindValue(run_target_name, nullptr);

        if (!run_target) {
            LogError("Run target '%1' does not exist", run_target_name);
            return 1;
        } else if (run_target->type != TargetType::Executable) {
            LogError("Cannot run non-executable target '%1'", run_target->name);
            return 1;
        }
    }

    if (!quiet) {
        LogInfo("Computing versions...");
    }
    if (GitVersioneer::IsAvailable()) {
        GitVersioneer versioneer;

        if (!versioneer.Prepare("."))
            return 1;

        for (Size i = 0; i < enabled_targets.len; i++) {
            EnabledTarget *it = &enabled_targets[i];

            if (it->target->type != TargetType::Executable)
                continue;

            // Continue even if versioning fails
            const char *version = versioneer.Version(it->target->version_tag);
            it->version = version ? DuplicateString(version, &temp_alloc).ptr : nullptr;
        }
    } else {
        LogWarning("Built without git versioning support");
    }

    // We're ready to output stuff
    if (!quiet) {
        LogInfo("Root directory: %!..+%1%!0", GetWorkingDirectory());
        LogInfo("  Output directory: %!..+%1%!0", build.output_directory);
        LogInfo("  Host: %!..+%1 (%2)%!0", HostPlatformNames[(int)host_spec.platform],
                                           HostArchitectureNames[(int)host_spec.architecture]);
        LogInfo("  Compiler: %!..+%1%!0", build.compiler->name);
        LogInfo("  Features: %!..+%1%!0", FmtFlags(build.features, CompileFeatureOptions));
    }
    if (!build.fake && !MakeDirectoryRec(build.output_directory))
        return 1;

    // Prepare build
    Builder builder(build);
    for (const EnabledTarget &it: enabled_targets) {
        if (!builder.AddTarget(*it.target, it.version))
            return 1;
    }
    for (const SourceFileInfo *src: enabled_sources) {
        if (!builder.AddSource(*src))
            return 1;
    }

    // Build stuff!
    if (!builder.Build(jobs, verbose))
        return 1;

    // Run?
    if (run_target) {
        RG_ASSERT(run_target->type == TargetType::Executable);

        if (run_here && !SetWorkingDirectory(start_directory))
            return 1;

        const char *target_filename = builder.target_filenames.FindValue(run_target->name, nullptr);
        return RunTarget(target_filename, run_arguments);
    } else {
        return 0;
    }
}

}
