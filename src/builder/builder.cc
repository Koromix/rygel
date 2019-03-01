// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"

enum class Toolchain {
    ClangCl
};
static const char *const ToolchainNames[] = {
    "ClangCl"
};

enum class Language {
    C,
    CXX
};

struct BuildCommand {
    const char *src_filename;
    const char *dest_filename;

    const char *cmd_text;
    std::function<bool(BlockAllocator *alloc)> func;
};

static const char *const CFlags = "-std=gnu99 -O0 -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -Wno-unknown-warning-option";
static const char *const CXXFlags = "-std=gnu++17 -O0 -Xclang -flto-visibility-public-std -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -Wno-unknown-warning-option";

static int64_t GetFileModificationTime(const char *filename)
{
    FileInfo file_info;
    if (!StatFile(filename, false, &file_info))
        return -1;

    return file_info.modification_time;
}

// Do not use absolute filenames!
static const char *BuildObjectPath(Span<const char> output_dir, Span<const char> filename,
                                   Allocator *alloc)
{
    char *path = Fmt(alloc, "%1%/%2.o", output_dir, filename).ptr;

    // Replace '..' components with '__'
    {
        char *ptr = path + output_dir.len + 1;

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
    Span<const char> rule;
    HeapArray<char> rule_buf;
    if (!ReadFile(filename, Megabytes(2), &rule_buf))
        return false;
    rule = rule_buf;

    // Skip target path
    SplitStr(rule, ':', &rule);

    while (rule.len) {
        Span<const char> path = TrimStr(SplitStr(rule, ' ', &rule));

        if (path.len && path != "\\") {
            const char *dep_filename = CanonicalizePath(nullptr, path, alloc);
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

static bool ExecuteCommand(const char *cmd)
{
    errno = 0;
    if (system(cmd) || errno) {
        LogError("Command '%1' failed", cmd);
        return false;
    }

    return true;
}

static const char *CreateCompileCommand(Toolchain toolchain, Language language, const char *misc_flags,
                                        const char *src_filename, const char *dest_filename,
                                        const char *deps_filename, bool use_pch, Allocator *alloc)
{
    HeapArray<char> buf;
    buf.allocator = alloc;

    switch (toolchain) {
        case Toolchain::ClangCl: {
            switch (language) {
                case Language::C: {
                    Fmt(&buf, "clang %1 %2 -c %3", CFlags, misc_flags ? misc_flags : "", src_filename);
                    if (use_pch) {
                        Fmt(&buf, " -include pch/stdafx_c.h");
                    }
                    if (deps_filename) {
                        Fmt(&buf, " -MMD -MF %1", deps_filename);
                    }
                    if (dest_filename) {
                        Fmt(&buf, " -o %1", dest_filename);
                    }
                } break;

                case Language::CXX: {
                    Fmt(&buf, "clang++ %1 %2 -c %3", CXXFlags, misc_flags ? misc_flags : "", src_filename);
                    if (use_pch) {
                        Fmt(&buf, " -include pch/stdafx_cxx.h");
                    }
                    if (deps_filename) {
                        Fmt(&buf, " -MMD -MF %1", deps_filename);
                    }
                    if (dest_filename) {
                        Fmt(&buf, " -o %1", dest_filename);
                    }
                } break;
            }
        } break;
    }
    DebugAssert(buf.len);

    return buf.Leak().ptr;
}

static bool AppendPCHCommands(Toolchain toolchain, Language language, const char *pch_filename,
                              HeapArray<BuildCommand> *out_commands)
{
    BlockAllocator temp_alloc;

    const char *dest_filename = nullptr;
    const char *deps_filename = nullptr;
    switch (language) {
        case Language::C: {
            dest_filename = "pch/stdafx_c.h";
            deps_filename = "pch/stdafx_c.d";
        } break;
        case Language::CXX: {
            dest_filename = "pch/stdafx_cxx.h";
            deps_filename = "pch/stdafx_cxx.d";
        } break;
    }
    DebugAssert(dest_filename && deps_filename);

    const char *misc_flags = nullptr;
    switch (toolchain) {
        case Toolchain::ClangCl: {
            switch (language) {
                case Language::C: { misc_flags = "-x c-header"; } break;
                case Language::CXX: { misc_flags = "-x c++-header"; } break;
            }
        } break;
    }
    DebugAssert(misc_flags);

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
        BuildCommand cmd = {};

        cmd.src_filename = pch_filename;
        cmd.dest_filename = dest_filename;

        cmd.cmd_text = "Build PCH";
        cmd.func = [=](BlockAllocator *alloc) {
            StreamWriter writer(dest_filename);
            if (PathIsAbsolute(pch_filename)) {
                Print(&writer, "#include \"%1\"", pch_filename);
            } else {
                Print(&writer, "#include \"%1%/%2\"", GetApplicationDirectory(), pch_filename);
            }
            if (!writer.Close())
                return false;

            const char *cmd = CreateCompileCommand(toolchain, language, misc_flags, dest_filename,
                                                   nullptr, deps_filename, false, alloc);
            return ExecuteCommand(cmd);
        };

        out_commands->Append(cmd);
    }

    return true;
}

static bool AppendObjectCommands(Toolchain toolchain, const char *src_directory,
                                 bool use_c_pch, bool use_cxx_pch,
                                 Allocator *alloc, HeapArray<BuildCommand> *out_commands)
{
    BlockAllocator temp_alloc;

    if (PathIsAbsolute(src_directory)) {
        LogError("Cannot use absolute directory '%1'", src_directory);
        return false;
    }

    // Reuse for performance
    HeapArray<const char *> src_filenames;

    EnumStatus status = EnumerateDirectory(src_directory, nullptr, 32768,
                                           [&](const char *name, FileType file_type) {
        if (file_type == FileType::File) {
            Span<const char> extension = GetPathExtension(name);

            if (extension == ".cc" || extension == ".cpp" || extension == ".c") {
                BuildCommand cmd = {};

                cmd.src_filename = Fmt(alloc, "%1%/%2", src_directory, name).ptr;
                cmd.dest_filename = BuildObjectPath("objects", cmd.src_filename, alloc);

                src_filenames.RemoveFrom(0);
                src_filenames.Append(cmd.src_filename);

                const char *deps_filename = Fmt(&temp_alloc, "%1.d", cmd.dest_filename).ptr;

                // Parse Make rule dependencies
                bool build = false;
                if (TestFile(deps_filename, FileType::File)) {
                    if (!ParseCompilerMakeRule(deps_filename, &temp_alloc, &src_filenames))
                        return false;

                    build = !IsFileUpToDate(cmd.dest_filename, src_filenames);
                } else {
                    build = true;
                }

                if (build) {
                    if (extension == ".c") {
                        cmd.cmd_text =
                            CreateCompileCommand(toolchain, Language::C, nullptr, cmd.src_filename,
                                                 cmd.dest_filename, deps_filename, use_c_pch, alloc);
                    } else {
                        cmd.cmd_text =
                            CreateCompileCommand(toolchain, Language::CXX, nullptr, cmd.src_filename,
                                                 cmd.dest_filename, deps_filename, use_cxx_pch, alloc);
                    }

                    out_commands->Append(cmd);
                }
            }
        }
        return true;
    });

    return status == EnumStatus::Done;
}

static bool RunBuildCommands(Span<const BuildCommand> commands)
{
    BlockAllocator temp_alloc;

    Async async;

    for (const BuildCommand &cmd: commands) {
        static std::atomic_int progress_counter;
        progress_counter = 0;

        async.AddTask([&, cmd]() {
            LogInfo("[%1/%2] %3", progress_counter.fetch_add(1) + 1, commands.len, cmd.cmd_text);

            // Create output directory
            {
                Span<const char> dest_dir;
                SplitStrReverseAny(cmd.dest_filename, PATH_SEPARATORS, &dest_dir);

                if (!MakeDirectoryRec(dest_dir))
                    return false;
            }

            // Run command or function
            {
                bool success;
                if (cmd.func) {
                    success = cmd.func(&temp_alloc);
                } else {
                    success = ExecuteCommand(cmd.cmd_text);
                }
                if (!success) {
                    unlink(cmd.dest_filename);
                    return false;
                }
            }

            return true;
        });
    }

    return async.Sync();
}

int main(int argc, char **argv)
{
    Toolchain toolchain = (Toolchain)0;
    HeapArray<const char *> src_directories;
    const char *c_pch_filename = nullptr;
    const char *cxx_pch_filename = nullptr;
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("-s", "--serial")) {
                // FIXME: Provide knobs in Async instead of this hack
                putenv("LIBCC_THREADS=1");
            } else if (opt.Test("-t", "--toolchain", OptionType::Value)) {
                const char *const *name = FindIf(ToolchainNames,
                                                 [&](const char *name) { return TestStr(name, opt.current_value); });
                if (!name) {
                    LogError("Unknown toolchain '%1'", opt.current_value);
                    return 1;
                }

                toolchain = (Toolchain)(name - ToolchainNames);
            } else if (opt.Test("-c_pch", OptionType::Value)) {
                c_pch_filename = opt.current_value;
            } else if (opt.Test("--cxx_pch", OptionType::Value)) {
                cxx_pch_filename = opt.current_value;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        opt.ConsumeNonOptions(&src_directories);
        if (!src_directories.len) {
            LogError("Source directory is missing");
            return 1;
        }
    }

    // Build PCH
    {
        HeapArray<BuildCommand> commands;
        BlockAllocator commands_alloc;

        if (c_pch_filename && !AppendPCHCommands(toolchain, Language::C, c_pch_filename, &commands))
            return 1;
        if (cxx_pch_filename && !AppendPCHCommands(toolchain, Language::CXX, cxx_pch_filename, &commands))
            return 1;

        if (commands.len) {
            LogInfo("Build PCH");

            if (!RunBuildCommands(commands))
                return 1;
        }
    }

    // Build object files
    {
        HeapArray<BuildCommand> commands;
        BlockAllocator commands_alloc;

        for (const char *src_directory: src_directories) {
            if (!AppendObjectCommands(toolchain, src_directory, c_pch_filename, cxx_pch_filename,
                                      &commands_alloc, &commands))
                return 1;
        }

        if (commands.len) {
            LogInfo("Build object files");

            if (!RunBuildCommands(commands))
                return 1;
        }
    }

    LogInfo("Done!");
    return 0;
}
