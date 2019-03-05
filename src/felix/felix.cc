// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef _WIN32
    #include <unistd.h>
#endif

#include "../libcc/libcc.hh"
#include "compiler.hh"
#include "config.hh"

struct BuildCommand {
    const char *dest_filename;
    const char *cmd;
};

static int64_t GetFileModificationTime(const char *filename)
{
    FileInfo file_info;
    if (!StatFile(filename, false, &file_info))
        return -1;

    return file_info.modification_time;
}

// Do not use absolute filenames!
static const char *BuildObjectPath(const char *src_filename, Allocator *alloc)
{
    DebugAssert(!PathIsAbsolute(src_filename));

    Span<const char> output_directory = GetWorkingDirectory();
    char *path = Fmt(alloc, "%1%/objects%/%2.o", output_directory, src_filename).ptr;

    // Replace '..' components with '__'
    {
        char *ptr = path + output_directory.len + 1;

        while ((ptr = strstr(ptr, ".."))) {
            if ((ptr == path || IsPathSeparator(ptr[-1])) && (IsPathSeparator(ptr[2]) || !ptr[2])) {
                ptr[0] = '_';
                ptr[1] = '_';
            }

            ptr += 2;
        }
    }

    return path;
}

// TODO: Support Make escaping
static bool ParseCompilerMakeRule(const char *filename, Allocator *alloc,
                                  HeapArray<const char *> *out_filenames)
{
    HeapArray<char> rule_buf;
    if (ReadFile(filename, Megabytes(2), &rule_buf) < 0)
        return false;
    rule_buf.Append(0);

    // Skip output path
    Span<const char> rule;
    {
        const char *ptr = strstr(rule_buf.ptr, ": ");
        if (ptr) {
            rule = Span<const char>(ptr + 2);
        } else {
            rule = {};
        }
    }

    while (rule.len) {
        Span<const char> path = TrimStr(SplitStr(rule, ' ', &rule));

        if (path.len && path != "\\") {
            const char *dep_filename = CanonicalizePath(nullptr, path, alloc);
            if (!dep_filename)
                return false;
            out_filenames->Append(dep_filename);
        }
    }

    return true;
}

static bool IsFileUpToDate(const char *dest_filename, Span<const char *const> src_filenames)
{
    int64_t dest_time = GetFileModificationTime(dest_filename);

    for (const char *src_filename: src_filenames) {
        int64_t src_time = GetFileModificationTime(src_filename);
        if (src_time < 0 || src_time > dest_time)
            return false;
    }

    return true;
}

static bool EnsureDirectoryExists(const char *filename)
{
    Span<const char> directory;
    SplitStrReverseAny(filename, PATH_SEPARATORS, &directory);

    return MakeDirectoryRec(directory);
}

static bool AppendPCHCommands(const char *pch_filename, SourceType src_type,
                              const Compiler &compiler, BuildMode build_mode,
                              Allocator *alloc, HeapArray<BuildCommand> *out_commands)
{
    DEFER_NC(out_guard, len = out_commands->len) { out_commands->RemoveFrom(len); };

    BlockAllocator temp_alloc;

    const char *dest_filename = nullptr;
    const char *deps_filename = nullptr;
    switch (src_type) {
        case SourceType::C_Header: {
            dest_filename = "pch/stdafx_c.h";
            deps_filename = "pch/stdafx_c.d";
        } break;
        case SourceType::CXX_Header: {
            dest_filename = "pch/stdafx_cxx.h";
            deps_filename = "pch/stdafx_cxx.d";
        } break;

        case SourceType::C_Source:
        case SourceType::CXX_Source: {
            Assert(false);
        } break;
    }
    DebugAssert(dest_filename && deps_filename);

    bool build;
    if (TestFile(deps_filename, FileType::File)) {
        HeapArray<const char *> src_filenames;
        if (!ParseCompilerMakeRule(deps_filename, &temp_alloc, &src_filenames))
            return false;

        build = !IsFileUpToDate(dest_filename, src_filenames);
    } else {
        build = true;
    }

    if (build) {
        if (!EnsureDirectoryExists(dest_filename))
            return false;

        if (pch_filename) {
            BuildCommand cmd = {};
            cmd.dest_filename = dest_filename;

            StreamWriter writer(dest_filename);
            if (PathIsAbsolute(pch_filename)) {
                Print(&writer, "#include \"%1\"", pch_filename);
            } else {
                Print(&writer, "#include \"%1%/%2\"", GetWorkingDirectory(), pch_filename);
            }
            if (!writer.Close())
                return false;

            cmd.cmd = compiler.BuildObjectCommand(dest_filename, src_type, build_mode, nullptr,
                                                  deps_filename, alloc);

            out_commands->Append(cmd);
        } else {
            if (!WriteFile("", dest_filename))
                return false;
            if (!WriteFile("", deps_filename))
                return false;
        }
    }

    out_guard.disable();
    return true;
}

static bool GatherObjects(const Target &target, Allocator *alloc, HeapArray<ObjectInfo> *out_objects)
{
    BlockAllocator temp_alloc;

    HeapArray<const char *> src_filenames;
    for (const char *src_directory: target.src_directories) {
        if (!EnumerateDirectoryFiles(src_directory, nullptr, 1024, &temp_alloc, &src_filenames))
            return false;
    }
    src_filenames.Append(target.src_filenames);

    // Introduce out_guard if things can start to fail in there
    for (const char *src_filename: src_filenames) {
        const char *name = SplitStrReverseAny(src_filename, PATH_SEPARATORS).ptr;
        bool ignore = std::any_of(target.exclusions.begin(), target.exclusions.end(),
                                  [&](const char *excl) { return MatchPathName(name, excl); });

        if (!ignore) {
            ObjectInfo obj = {};

            Span<const char> extension = GetPathExtension(src_filename);
            if (extension == ".c") {
                obj.src_type = SourceType::C_Source;
            } else if (extension == ".cc" || extension == ".cpp") {
                obj.src_type = SourceType::CXX_Source;
            } else {
                continue;
            }

            obj.src_filename = DuplicateString(src_filename, alloc).ptr;
            obj.dest_filename = BuildObjectPath(src_filename, alloc);

            out_objects->Append(obj);
        }
    }

    return true;
}

static bool AppendTargetCommands(const Target &target, Span<const ObjectInfo> objects,
                                 const Compiler &compiler, BuildMode build_mode, Allocator *alloc,
                                 HeapArray<BuildCommand> *out_obj_commands,
                                 HeapArray<BuildCommand> *out_link_commands)
{
    DEFER_NC(out_guard, obj_len = out_obj_commands->len,
                        link_len = out_link_commands->len) {
        out_obj_commands->RemoveFrom(obj_len);
        out_link_commands->RemoveFrom(link_len);
    };

    BlockAllocator temp_alloc;

    // Reuse for performance
    HeapArray<const char *> src_filenames;

    bool relink = false;
    for (const ObjectInfo &obj: objects) {
        const char *deps_filename = Fmt(&temp_alloc, "%1.d", obj.dest_filename).ptr;

        src_filenames.RemoveFrom(0);
        src_filenames.Append(obj.src_filename);

        // Parse Make rule dependencies
        bool build = false;
        if (TestFile(deps_filename, FileType::File)) {
            if (!ParseCompilerMakeRule(deps_filename, &temp_alloc, &src_filenames))
                return false;

            build = !IsFileUpToDate(obj.dest_filename, src_filenames);
        } else {
            build = true;
        }
        relink |= build;

        if (build) {
            BuildCommand cmd = {};

            cmd.dest_filename = obj.dest_filename;
            if (!EnsureDirectoryExists(obj.dest_filename))
                return false;
            cmd.cmd = compiler.BuildObjectCommand(obj.src_filename, obj.src_type, build_mode,
                                                  obj.dest_filename, deps_filename, alloc);

            out_obj_commands->Append(cmd);
        }
    }

    if (target.type == TargetType::Executable) {
        const char *output_directory = GetWorkingDirectory();
#ifdef _WIN32
        const char *link_filename = Fmt(alloc, "%1%/%2.exe", output_directory, target.name).ptr;
#else
        const char *link_filename = Fmt(alloc, "%1%/%2", output_directory, target.name).ptr;
#endif

        if (relink || !TestFile(link_filename, FileType::File)) {
            BuildCommand cmd = {};

            cmd.dest_filename = link_filename;
            cmd.cmd = compiler.BuildLinkCommand(objects, target.libraries, link_filename, alloc);

            out_link_commands->Append(cmd);
        }
    }

    out_guard.disable();
    return true;
}

static bool RunBuildCommands(Span<const BuildCommand> commands)
{
    Async async;

    static std::atomic_int progress_counter;
    progress_counter = 0;

    for (const BuildCommand &cmd: commands) {
        async.AddTask([&, cmd]() {
            LogInfo("[%1/%2] %3", progress_counter.fetch_add(1) + 1, commands.len, cmd.cmd);

            // Run command
            errno = 0;
            if (system(cmd.cmd) || errno) {
                LogError("Command '%1' failed", cmd.cmd);
                unlink(cmd.dest_filename);
                return false;
            }

            return true;
        });
    }

    return async.Sync();
}

int main(int argc, char **argv)
{
    BlockAllocator commands_alloc;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: felix [options] [target]

Options:
    -C, --config <filename>      Set configuration filename
                                 (default: felix.ini)

    -c, --compiler <compiler>    Set compiler, see below
                                 (default: %1)
    -m, --mode     <mode>        Set build mode, see below
                                 (default: %2)

        --c_pch <filename>       Precompile C header <filename>
        --cxx_pch <filename>     Precompile C++ header <filename>

    -j, --jobs <count>           Set maximum number of parallel jobs
                                 (default: number of cores)

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
    const char *config_filename = "felix.ini";
    const Compiler *compiler = Compilers[0];
    BuildMode build_mode = (BuildMode)0;
    const char *c_pch_filename = nullptr;
    const char *cxx_pch_filename = nullptr;
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config", OptionType::Value)) {
                config_filename = opt.current_value;
            } else if (opt.Test("-c", "--compiler", OptionType::Value)) {
                const Compiler *const *ptr = FindIf(Compilers,
                                                    [&](const Compiler *compiler) { return TestStr(compiler->name, opt.current_value); });
                if (!ptr) {
                    LogError("Unknown toolchain '%1'", opt.current_value);
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
            } else if (opt.Test("--c_pch", OptionType::Value)) {
                c_pch_filename = opt.current_value;
            } else if (opt.Test("--cxx_pch", OptionType::Value)) {
                cxx_pch_filename = opt.current_value;
            } else if (opt.Test("-j", "--jobs", OptionType::Value)) {
                int max_threads;
                if (!ParseDec(opt.current_value, &max_threads))
                    return 1;
                if (max_threads < 1) {
                    LogError("Jobs count cannot be < 1");
                    return 1;
                }

                Async::SetThreadCount(max_threads);
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

#ifdef _WIN32
    if (compiler == &GnuCompiler && (c_pch_filename || cxx_pch_filename)) {
        LogError("PCH does not work correctly with MinGW (ignoring)");

        c_pch_filename = nullptr;
        cxx_pch_filename = nullptr;
    }
#endif

    HeapArray<const Target *> targets;
    if (target_names.len) {
        HashSet<const void *> handled_set;

        for (const char *target_name: target_names) {
            const Target *target = config.targets_map.FindValue(target_name, nullptr);
            if (!target) {
                LogError("Unknown target '%1'", target_name);
                return 1;
            }

            if (!handled_set.Append(target).second) {
                LogError("Target '%1' is specified multiple times");
                return 1;
            }

            targets.Append(target);
        }
    } else {
        for (const Target &target: config.targets) {
            targets.Append(&target);
        }
    }

    // We need to build PCH first (for dependency issues)
    HeapArray<BuildCommand> pch_commands;
    if (!AppendPCHCommands(c_pch_filename, SourceType::C_Header, *compiler, build_mode,
                           &commands_alloc, &pch_commands))
        return 1;
    if (!AppendPCHCommands(cxx_pch_filename, SourceType::CXX_Header, *compiler, build_mode,
                           &commands_alloc, &pch_commands))
        return 1;

    if (pch_commands.len) {
        LogInfo("Build PCH");

        if (!RunBuildCommands(pch_commands))
            return 1;
    }

    HeapArray<BuildCommand> obj_commands;
    HeapArray<BuildCommand> link_commands;
    {
        // Reuse for performance
        HeapArray<ObjectInfo> objects;

        for (const Target *target: targets) {
            objects.RemoveFrom(0);
            if (!GatherObjects(*target, &commands_alloc, &objects))
                return 1;

            if (!AppendTargetCommands(*target, objects, *compiler, build_mode,
                                      &commands_alloc, &obj_commands, &link_commands))
                return 1;
        }
    }

    if (obj_commands.len) {
        LogInfo("Build object files");
        if (!RunBuildCommands(obj_commands))
            return 1;
    }
    if (link_commands.len) {
        LogInfo("Link targets");
        if (!RunBuildCommands(link_commands))
            return 1;
    }

    LogInfo("Done!");
    return 0;
}
