// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef _WIN32
    #include <unistd.h>
#endif

#include "../libcc/libcc.hh"
#include "compiler.hh"

struct BuildCommand {
    const char *dest_filename;
    const char *cmd;
};

struct Target {
    const char *name;
    HeapArray<const char> *src_filenames;
};

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
    if (ReadFile(filename, Megabytes(2), &rule_buf) < 0)
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

static bool EnsureDirectoryExists(const char *filename)
{
    Span<const char> directory;
    SplitStrReverseAny(filename, PATH_SEPARATORS, &directory);

    return MakeDirectoryRec(directory);
}

static bool AppendPCHCommands(const Compiler &compiler, SourceType source_type,
                              const char *pch_filename, Allocator *alloc,
                              HeapArray<BuildCommand> *out_commands)
{
    DEFER_NC(out_guard, len = out_commands->len) { out_commands->RemoveFrom(len); };

    BlockAllocator temp_alloc;

    const char *dest_filename = nullptr;
    const char *deps_filename = nullptr;
    switch (source_type) {
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

            cmd.cmd = compiler.BuildObjectCommand(source_type, dest_filename, nullptr,
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

static bool AppendObjectCommands(const Compiler &compiler, const char *src_directory,
                                 Allocator *alloc, HeapArray<BuildCommand> *out_commands)
{
    DEFER_NC(out_guard, len = out_commands->len) { out_commands->RemoveFrom(len); };

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
                const char *src_filename = Fmt(alloc, "%1%/%2", src_directory, name).ptr;
                const char *dest_filename = BuildObjectPath("objects", src_filename, alloc);
                const char *deps_filename = Fmt(&temp_alloc, "%1.d", dest_filename).ptr;

                src_filenames.RemoveFrom(0);
                src_filenames.Append(src_filename);

                // Parse Make rule dependencies
                bool build = false;
                if (TestFile(deps_filename, FileType::File)) {
                    if (!ParseCompilerMakeRule(deps_filename, &temp_alloc, &src_filenames))
                        return false;

                    build = !IsFileUpToDate(dest_filename, src_filenames);
                } else {
                    build = true;
                }

                if (build) {
                    BuildCommand cmd = {};
                    cmd.dest_filename = dest_filename;

                    if (!EnsureDirectoryExists(dest_filename))
                        return false;

                    if (extension == ".c") {
                        cmd.cmd = compiler.BuildObjectCommand(SourceType::C_Source, src_filename,
                                                              cmd.dest_filename, deps_filename, alloc);
                    } else {
                        cmd.cmd = compiler.BuildObjectCommand(SourceType::CXX_Source, src_filename,
                                                              cmd.dest_filename, deps_filename, alloc);
                    }

                    out_commands->Append(cmd);
                }
            }
        }
        return true;
    });
    if (status != EnumStatus::Done)
        return false;

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
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: builder [options] [target]

Options:
    -c, --compiler <compiler>    Set compiler
                                 (default: %1)

        --c_pch <filename>       Precompile C header <filename>
        --cxx_pch <filename>     Precompile C++ header <filename>

    -j, --jobs <count>           Set maximum number of parallel jobs
                                 (default: number of cores)

Available compilers:)", Compilers[0]->name);
        for (const Compiler *compiler: Compilers) {
            PrintLn(fp, "    %1", compiler->name);
        }
    };

    const Compiler *compiler = Compilers[0];
    HeapArray<const char *> src_directories;
    const char *c_pch_filename = nullptr;
    const char *cxx_pch_filename = nullptr;
    {
        OptionParser opt(argc, argv);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (opt.Test("-c", "--compiler", OptionType::Value)) {
                const Compiler *const *ptr = FindIf(Compilers,
                                                    [&](const Compiler *compiler) { return TestStr(compiler->name, opt.current_value); });
                if (!ptr) {
                    LogError("Unknown toolchain '%1'", opt.current_value);
                    return 1;
                }

                compiler = *ptr;
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

        opt.ConsumeNonOptions(&src_directories);
        if (!src_directories.len) {
            LogError("Source directory is missing");
            return 1;
        }
    }

#ifdef _WIN32
    if (compiler == &GnuCompiler && (c_pch_filename || cxx_pch_filename)) {
        LogError("PCH does not work correctly with MinGW (ignoring)");

        c_pch_filename = nullptr;
        cxx_pch_filename = nullptr;
    }
#endif

    // Build PCH
    {
        HeapArray<BuildCommand> commands;
        BlockAllocator commands_alloc;

        if (!AppendPCHCommands(*compiler, SourceType::C_Header, c_pch_filename, &commands_alloc, &commands))
            return 1;
        if (!AppendPCHCommands(*compiler, SourceType::CXX_Header, cxx_pch_filename, &commands_alloc, &commands))
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
            if (!AppendObjectCommands(*compiler, src_directory, &commands_alloc, &commands))
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
