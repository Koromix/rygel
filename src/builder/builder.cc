// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"

enum class Toolchain {
    ClangCl_C,
    ClangCl_CXX
};

enum class SyncMode {
    None,
    Before,
    After
};

struct BuildCommand {
    const char *src_filename;
    const char *dest_filename;
    SyncMode sync_mode;

    const char *cmd_text;
    std::function<bool(BlockAllocator *alloc)> func;
};

static const char *const c_flags = "-std=gnu99 -O0 -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -Wno-unknown-warning-option";
static const char *const cxx_flags = "-std=gnu++17 -O0 -Xclang -flto-visibility-public-std -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -Wno-unknown-warning-option";

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
        if (src_time > dest_time)
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

static const char *CreateCompileCommand(Toolchain toolchain, const char *misc_flags,
                                        const char *src_filename, const char *dest_filename,
                                        const char *deps_filename, bool use_pch, Allocator *alloc)
{
    HeapArray<char> buf;
    buf.allocator = alloc;

    switch (toolchain) {
        case Toolchain::ClangCl_C: {
            Fmt(&buf, "clang %1 %2 -c %3", c_flags, misc_flags ? misc_flags : "", src_filename);
            if (use_pch) {
                Fmt(&buf, " -include pch/clangcl_c_stdafx.h");
            }
            if (deps_filename) {
                Fmt(&buf, " -MMD -MF %1", deps_filename);
            }
            if (dest_filename) {
                Fmt(&buf, " -o %1", dest_filename);
            }
        } break;

        case Toolchain::ClangCl_CXX: {
            Fmt(&buf, "clang++ %1 %2 -c %3", cxx_flags, misc_flags ? misc_flags : "", src_filename);
            if (use_pch) {
                Fmt(&buf, " -include pch/clangcl_cxx_stdafx.h");
            }
            if (deps_filename) {
                Fmt(&buf, " -MMD -MF %1", deps_filename);
            }
            if (dest_filename) {
                Fmt(&buf, " -o %1", dest_filename);
            }
        } break;
    }
    DebugAssert(buf.len);

    return buf.Leak().ptr;
}

static bool AppendPCHCommands(Toolchain toolchain, const char *pch_filename,
                              HeapArray<BuildCommand> *out_commands)
{
    BlockAllocator temp_alloc;

    const char *misc_flags = nullptr;
    const char *dest_filename = nullptr;
    const char *deps_filename = nullptr;
    switch (toolchain) {
        case Toolchain::ClangCl_C: {
            misc_flags = "-x c-header";
            dest_filename = "pch/clangcl_c_stdafx.h";
            deps_filename = "pch/clangcl_c_stdafx.d";
        } break;
        case Toolchain::ClangCl_CXX: {
            misc_flags = "-x c++-header";
            dest_filename = "pch/clangcl_cxx_stdafx.h";
            deps_filename = "pch/clangcl_cxx_stdafx.d";
        } break;
    }
    DebugAssert(misc_flags && dest_filename && deps_filename);

    if (TestFile(deps_filename, FileType::File)) {
        HeapArray<const char *> src_filenames;
        if (!ParseCompilerMakeRule(deps_filename, &temp_alloc, &src_filenames))
            return false;

        if (IsFileUpToDate(dest_filename, src_filenames))
            return true;
    }

    BuildCommand cmd = {};

    cmd.src_filename = pch_filename;
    cmd.dest_filename = dest_filename;
    cmd.sync_mode = SyncMode::After;

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

        const char *cmd = CreateCompileCommand(toolchain, misc_flags, dest_filename,
                                               nullptr, deps_filename, false, alloc);
        return ExecuteCommand(cmd);
    };

    out_commands->Append(cmd);
    return true;
}

static bool AppendObjectCommands(const char *src_directory, bool use_c_pch, bool use_cxx_pch,
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

                // Parse Make rule dependencies
                const char *deps_filename = Fmt(&temp_alloc, "%1.d", cmd.dest_filename).ptr;
                if (TestFile(deps_filename, FileType::File) &&
                        !ParseCompilerMakeRule(deps_filename, &temp_alloc, &src_filenames))
                    return false;

                if (!IsFileUpToDate(cmd.dest_filename, src_filenames)) {
                    if (extension == ".c") {
                        cmd.cmd_text =
                            CreateCompileCommand(Toolchain::ClangCl_C, nullptr, cmd.src_filename,
                                                 cmd.dest_filename, deps_filename,
                                                 use_c_pch, alloc);
                    } else {
                        cmd.cmd_text =
                            CreateCompileCommand(Toolchain::ClangCl_CXX, nullptr, cmd.src_filename,
                                                 cmd.dest_filename, deps_filename,
                                                 use_cxx_pch, alloc);
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
        static std::atomic_int progress_counter = {};

        if (cmd.sync_mode == SyncMode::Before && !async.Sync())
            return false;

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

        if (cmd.sync_mode == SyncMode::After && !async.Sync())
            return false;
    }

    return async.Sync();
}

int main(int argc, char **argv)
{
    HeapArray<const char *> src_directories;
    const char *c_pch_filename = nullptr;
    const char *cxx_pch_filename = nullptr;
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("-c_pch", OptionType::Value)) {
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

    uint64_t start_time = GetMonotonicTime();
    DEFER {
        uint64_t end_time = GetMonotonicTime();
        LogInfo("Elapsed time: %1 sec", FmtDouble((double)(end_time - start_time) / 1000.0, 2));
    };

    // List of build commands
    HeapArray<BuildCommand> commands;
    BlockAllocator commands_alloc;

    // Deal with PCH
    if (c_pch_filename && !AppendPCHCommands(Toolchain::ClangCl_C, c_pch_filename, &commands))
        return 1;
    if (cxx_pch_filename && !AppendPCHCommands(Toolchain::ClangCl_CXX, cxx_pch_filename, &commands))
        return 1;

    // Deal with source / object files
    for (const char *src_directory: src_directories) {
        if (!AppendObjectCommands(src_directory, c_pch_filename, cxx_pch_filename,
                                  &commands_alloc, &commands))
            return 1;
    }

    // Run build
    if (!RunBuildCommands(commands))
        return 1;

    return 0;
}
