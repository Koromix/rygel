// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"

static const char *const c_flags = "-std=gnu99 -O0 -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -Wno-unknown-warning-option";
static const char *const cxx_flags = "-std=gnu++17 -O0 -Xclang -flto-visibility-public-std -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -Wno-unknown-warning-option";

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

enum class SyncMode {
    None,
    Before,
    After
};

struct BuildCommand {
    const char *src_filename;
    const char *dest_filename;

    const char *cmd_text;
    std::function<bool()> func;

    SyncMode sync_mode;
};

int main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    HeapArray<const char *> src_directories;
    const char *pch_filename = nullptr;
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--pch", OptionType::Value)) {
                pch_filename = opt.current_value;
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

    // Deal with PCH
    if (pch_filename) {

        bool build_pch = false;
        if (TestFile("pch/stdafx.d", FileType::File)) {
            HeapArray<const char *> src_filenames;
            if (!ParseCompilerMakeRule("pch/stdafx.d", &temp_alloc, &src_filenames))
                return 1;

            build_pch = !IsFileUpToDate("pch/stdafx.h", src_filenames);
        } else {
            build_pch = true;
        }

        if (build_pch) {
            BuildCommand cmd = {};

            cmd.src_filename = pch_filename;
            cmd.dest_filename = "pch/stdafx.h";

            cmd.cmd_text = "Build PCH";
            cmd.func = [&]() {
                StreamWriter writer("pch/stdafx.h");
                if (PathIsAbsolute(pch_filename)) {
                    Print(&writer, "#include \"%1\"", pch_filename);
                } else {
                    Print(&writer, "#include \"%1%/%2\"", GetApplicationDirectory(), pch_filename);
                }
                if (!writer.Close())
                    return false;

                const char *cmd = Fmt(&temp_alloc, "clang++ %1 -x c++-header pch/stdafx.h -MMD -MF pch/stdafx.d", cxx_flags).ptr;
                return ExecuteCommand(cmd);
            };

            cmd.sync_mode = SyncMode::After;

            commands.Append(cmd);
        }
    }

    // Aggregate build commands
    {
        // Reuse for performance
        HeapArray<const char *> src_filenames;

        for (const char *src_directory: src_directories) {
            if (PathIsAbsolute(src_directory)) {
                LogError("Cannot use absolute directory '%1'", src_directory);
                return 1;
            }

            EnumStatus status = EnumerateDirectory(src_directory, nullptr, 32768,
                                                   [&](const char *name, FileType file_type) {
                if (file_type == FileType::File) {
                    Span<const char> extension = GetPathExtension(name);

                    if (extension == ".cc" || extension == ".cpp" || extension == ".c") {
                        BuildCommand cmd = {};

                        cmd.src_filename = Fmt(&temp_alloc, "%1%/%2", src_directory, name).ptr;
                        cmd.dest_filename = BuildObjectPath("objects", cmd.src_filename, &temp_alloc);

                        src_filenames.RemoveFrom(0);
                        src_filenames.Append(cmd.src_filename);

                        // Parse Make rule dependencies
                        const char *deps_filename = Fmt(&temp_alloc, "%1.d", cmd.dest_filename).ptr;
                        if (TestFile(deps_filename, FileType::File) &&
                                !ParseCompilerMakeRule(deps_filename, &temp_alloc, &src_filenames))
                            return false;

                        if (!IsFileUpToDate(cmd.dest_filename, src_filenames)) {
                            if (extension == ".c") {
                                cmd.cmd_text = Fmt(&temp_alloc, "clang %1 -c %2 -o %3",
                                                   c_flags, cmd.src_filename, cmd.dest_filename).ptr;
                            } else {
                                if (pch_filename) {
                                    cmd.cmd_text = Fmt(&temp_alloc, "clang++ -include pch/stdafx.h %1 -c %2 -MMD -MF %3 -o %4",
                                                       cxx_flags, cmd.src_filename, deps_filename, cmd.dest_filename).ptr;
                                } else {
                                    cmd.cmd_text = Fmt(&temp_alloc, "clang++ %1 -c %2 -MMD -MF %3 -o %4",
                                                       cxx_flags, cmd.src_filename, deps_filename, cmd.dest_filename).ptr;
                                }
                            }

                            commands.Append(cmd);
                        }
                    }
                }
                return true;
            });
            if (status != EnumStatus::Done)
                return 1;
        }
    }

    Async async;
    for (const BuildCommand &cmd: commands) {
        static std::atomic_int progress_counter = {};

        if (cmd.sync_mode == SyncMode::Before && !async.Sync())
            return 1;

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
                    success = cmd.func();
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
            return 1;
    }
    if (!async.Sync())
        return 1;

    return 0;
}
