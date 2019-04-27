// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <spawn.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>

    extern char **environ;
#endif

#include "../../libcc/libcc.hh"
#include "build_command.hh"

namespace RG {

static bool ExecuteCommandLine(const char *cmd_line, HeapArray<char> *out_buf, int *out_code)
{
#ifdef _WIN32
    STARTUPINFO startup_info = {};

    // Create read pipe
    HANDLE out_pipe[2];
    if (!CreatePipe(&out_pipe[0], &out_pipe[1], nullptr, 0)) {
        LogError("Failed to create pipe: %1", Win32ErrorString());
        return false;
    }
    RG_DEFER { CloseHandle(out_pipe[0]); };

    // Start process
    HANDLE process_handle;
    {
        RG_DEFER {
            CloseHandle(out_pipe[1]);

            if (startup_info.hStdOutput) {
                CloseHandle(startup_info.hStdOutput);
            }
            if (startup_info.hStdError) {
                CloseHandle(startup_info.hStdError);
            }
        };

        if (!DuplicateHandle(GetCurrentProcess(), out_pipe[1], GetCurrentProcess(),
                             &startup_info.hStdOutput, 0, TRUE, DUPLICATE_SAME_ACCESS) ||
            !DuplicateHandle(GetCurrentProcess(), out_pipe[1], GetCurrentProcess(),
                             &startup_info.hStdError, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
            LogError("Failed to duplicate handle: %1", Win32ErrorString());
            return false;
        }
        startup_info.dwFlags |= STARTF_USESTDHANDLES;

        PROCESS_INFORMATION process_info = {};
        if (!CreateProcessA(nullptr, const_cast<char *>(cmd_line), nullptr, nullptr, TRUE, 0,
                            nullptr, nullptr, &startup_info, &process_info)) {
            LogError("Failed to start process: %1", Win32ErrorString());
            return false;
        }

        process_handle = process_info.hProcess;
        CloseHandle(process_info.hThread);
    }
    RG_DEFER { CloseHandle(process_handle); };

    // Read process output
    for (;;) {
        out_buf->Grow(1024);

        DWORD read_len = 0;
        if (!::ReadFile(out_pipe[0], out_buf->end(), 1024, &read_len, nullptr)) {
            if (GetLastError() != ERROR_BROKEN_PIPE) {
                LogError("Failed to read process output: %1", Win32ErrorString());
            }
            break;
        }

        out_buf->len += read_len;
    }

    // Wait for process exit
    DWORD exit_code;
    if (WaitForSingleObject(process_handle, INFINITE) != WAIT_OBJECT_0) {
        LogError("WaitForSingleObject() failed: %1", Win32ErrorString());
        return false;
    }
    if (!GetExitCodeProcess(process_handle, &exit_code)) {
        LogError("GetExitCodeProcess() failed: %1", Win32ErrorString());
        return false;
    }

    *out_code = (int)exit_code;
    return true;
#else
    int out_pfd[2];
    if (pipe2(out_pfd, O_CLOEXEC) < 0) {
        LogError("Failed to create pipe: %1", strerror(errno));
        return false;
    }
    RG_DEFER { close(out_pfd[0]); };

    // Start process
    pid_t pid;
    {
        RG_DEFER { close(out_pfd[1]); };

        posix_spawn_file_actions_t file_actions;
        if ((errno = posix_spawn_file_actions_init(&file_actions)) ||
                (errno = posix_spawn_file_actions_adddup2(&file_actions, out_pfd[1], STDOUT_FILENO)) ||
                (errno = posix_spawn_file_actions_adddup2(&file_actions, out_pfd[1], STDERR_FILENO))) {
            LogError("Failed to set up standard process descriptors: %1", strerror(errno));
            return false;
        }

        const char *argv[] = {"sh", "-c", cmd_line, nullptr};
        if ((errno = posix_spawn(&pid, "/bin/sh", &file_actions, nullptr,
                                 const_cast<char **>(argv), environ))) {
            LogError("Failed to start process: %1", strerror(errno));
            return false;
        }
    }

    // Read process output
    for (;;) {
        out_buf->Grow(1024);

        ssize_t read_len = RG_POSIX_RESTART_EINTR(read(out_pfd[0], out_buf->end(), 1024));
        if (read_len < 0) {
            LogError("Failed to read process output: %1", strerror(errno));
            break;
        } else if (!read_len) {
            break;
        }

        out_buf->len += (Size)read_len;
    }

    // Wait for process exit
    int status;
    if (RG_POSIX_RESTART_EINTR(waitpid(pid, &status, 0)) < 0) {
        LogError("Failed to wait for process exit: %1", strerror(errno));
        return false;
    }

    if (WIFSIGNALED(status)) {
        *out_code = 128 + WTERMSIG(status);
    } else if (WIFEXITED(status)) {
        *out_code = WEXITSTATUS(status);
    } else {
        *out_code = -1;
    }
    return true;
#endif
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
            const char *dep_filename = NormalizePath(path, alloc).ptr;
            out_filenames->Append(dep_filename);
        }
    }

    return true;
}

static bool CreatePrecompileHeader(const char *pch_filename, const char *dest_filename)
{
    if (!EnsureDirectoryExists(dest_filename))
        return false;

    StreamWriter writer(dest_filename);
    Print(&writer, "#include \"%1%/%2\"", GetWorkingDirectory(), pch_filename);
    return writer.Close();
}

bool BuildSetBuilder::AppendTargetCommands(const Target &target)
{
    const Size start_pch_len = pch_commands.len;
    const Size start_obj_len = obj_commands.len;
    const Size start_link_len = link_commands.len;
    RG_DEFER_N(out_guard) {
        pch_commands.RemoveFrom(start_pch_len);
        obj_commands.RemoveFrom(start_obj_len);
        link_commands.RemoveFrom(start_link_len);
    };

    HeapArray<const char *> obj_filenames;

    bool warnings = (target.type != TargetType::ExternalLibrary);

    // Precompiled headers
    for (const ObjectInfo &obj: target.pch_objects) {
        const char *deps_filename = Fmt(&temp_alloc, "%1.d", obj.dest_filename).ptr;

        if (NeedsRebuild(obj.src_filename, obj.dest_filename, deps_filename)) {
            BuildCommand cmd = {};

            cmd.text = Fmt(&str_alloc, "Precompile %1", obj.src_filename).ptr;
            cmd.dest_filename = DuplicateString(obj.dest_filename, &str_alloc).ptr;
            if (!CreatePrecompileHeader(obj.src_filename, obj.dest_filename))
                return false;

            cmd.cmd = compiler->MakeObjectCommand(obj.dest_filename, obj.src_type, build_mode, warnings,
                                                  nullptr, target.definitions, target.include_directories,
                                                  nullptr, deps_filename, &str_alloc);
            if (!cmd.cmd)
                return false;

            pch_commands.Append(cmd);
        }
    }

    // Object commands
    for (const ObjectInfo &obj: target.objects) {
        const char *deps_filename = Fmt(&temp_alloc, "%1.d", obj.dest_filename).ptr;

        if (NeedsRebuild(obj.src_filename, obj.dest_filename, deps_filename)) {
            BuildCommand cmd = {};

            const char *pch_filename = nullptr;
            switch (obj.src_type) {
                case SourceType::C_Source: { pch_filename = target.c_pch_filename; } break;
                case SourceType::CXX_Source: { pch_filename = target.cxx_pch_filename; } break;

                case SourceType::C_Header:
                case SourceType::CXX_Header: { RG_ASSERT_DEBUG(false); } break;
            }

            cmd.text = Fmt(&str_alloc, "Build %1", obj.src_filename).ptr;
            cmd.dest_filename = DuplicateString(obj.dest_filename, &str_alloc).ptr;
            if (!EnsureDirectoryExists(obj.dest_filename))
                return false;
            cmd.cmd = compiler->MakeObjectCommand(obj.src_filename, obj.src_type, build_mode, warnings,
                                                  pch_filename, target.definitions, target.include_directories,
                                                  obj.dest_filename, deps_filename, &str_alloc);
            if (!cmd.cmd)
                return false;

            obj_commands.Append(cmd);

            // Pretend object file does not exist to force link step
            mtime_map.Set(obj.dest_filename, -1);
        }

        obj_filenames.Append(obj.dest_filename);
    }

    // Assets
    if (target.pack_obj_filename) {
        if (!IsFileUpToDate(target.pack_obj_filename, target.pack_filenames)) {
            BuildCommand cmd = {};

            cmd.text = Fmt(&str_alloc, "Pack %1 assets", target.name).ptr;
            cmd.dest_filename = DuplicateString(target.pack_obj_filename, &str_alloc).ptr;
            if (!EnsureDirectoryExists(target.pack_obj_filename))
                return false;
            cmd.cmd = compiler->MakePackCommand(target.pack_filenames, build_mode, target.pack_options,
                                                target.pack_obj_filename, &str_alloc);
            if (!cmd.cmd)
                return false;

            obj_commands.Append(cmd);

            // Pretend object file does not exist to force link step
            mtime_map.Set(target.pack_obj_filename, -1);
        }

        bool module = false;
        switch (target.pack_link_type) {
            case PackLinkType::Static: { module = false; } break;
            case PackLinkType::Module: { module = true; } break;
            case PackLinkType::ModuleIfDebug: { module = (build_mode == BuildMode::Debug); } break;
        }

        if (module) {
            if (!IsFileUpToDate(target.pack_module_filename, target.pack_obj_filename)) {
                BuildCommand cmd = {};

                // TODO: Check if this conflicts with a target destination file?
                cmd.text = Fmt(&str_alloc, "Link %1",
                               SplitStrReverseAny(target.pack_module_filename, RG_PATH_SEPARATORS)).ptr;
                cmd.dest_filename = DuplicateString(target.pack_module_filename, &str_alloc).ptr;
                cmd.cmd = compiler->MakeLinkCommand(target.pack_obj_filename, BuildMode::Debug, {},
                                                    LinkType::SharedLibrary, target.pack_module_filename,
                                                    &str_alloc);

                link_commands.Append(cmd);
            }
        } else {
            obj_filenames.Append(target.pack_obj_filename);
        }
    }

    // Link commands
    if (target.type == TargetType::Executable &&
            !IsFileUpToDate(target.dest_filename, obj_filenames)) {
        BuildCommand cmd = {};

        cmd.text = Fmt(&str_alloc, "Link %1",
                       SplitStrReverseAny(target.dest_filename, RG_PATH_SEPARATORS)).ptr;
        cmd.dest_filename = DuplicateString(target.dest_filename, &str_alloc).ptr;
        cmd.cmd = compiler->MakeLinkCommand(obj_filenames, build_mode, target.libraries,
                                            LinkType::Executable, target.dest_filename, &str_alloc);
        if (!cmd.cmd)
            return false;

        link_commands.Append(cmd);
    }

    // Do this at the end because it's much harder to roll back changes in out_guard
    for (Size i = start_pch_len; i < pch_commands.len; i++) {
        output_set.Append(pch_commands[i].dest_filename);
    }
    for (Size i = start_obj_len; i < obj_commands.len; i++) {
        output_set.Append(obj_commands[i].dest_filename);
    }
    for (Size i = start_link_len; i < link_commands.len; i++) {
        output_set.Append(link_commands[i].dest_filename);
    }

    out_guard.Disable();
    return true;
}

void BuildSetBuilder::Finish(BuildSet *out_set)
{
    RG_ASSERT_DEBUG(!out_set->commands.len);

    if (pch_commands.len) {
        pch_commands[pch_commands.len - 1].sync_after = true;
    }
    if (obj_commands.len) {
        obj_commands[obj_commands.len - 1].sync_after = true;
    }

    out_set->commands.Append(pch_commands);
    out_set->commands.Append(obj_commands);
    out_set->commands.Append(link_commands);

    SwapMemory(&out_set->str_alloc, &str_alloc, RG_SIZE(str_alloc));
}

bool BuildSetBuilder::NeedsRebuild(const char *src_filename, const char *dest_filename,
                                   const char *deps_filename)
{
    HeapArray<const char *> dep_filenames;
    dep_filenames.Append(src_filename);

    if (output_set.Find(dest_filename)) {
        return false;
    } else if (TestFile(deps_filename, FileType::File)) {
        // Parse Make rule dependencies
        if (!ParseCompilerMakeRule(deps_filename, &temp_alloc, &dep_filenames))
            return true;

        return !IsFileUpToDate(dest_filename, dep_filenames);
    } else {
        return true;
    }
}

bool BuildSetBuilder::IsFileUpToDate(const char *dest_filename,
                                     Span<const char *const> src_filenames)
{
    int64_t dest_time = GetFileModificationTime(dest_filename);
    if (dest_time < 0)
        return false;

    for (const char *src_filename: src_filenames) {
        int64_t src_time = GetFileModificationTime(src_filename);
        if (src_time < 0 || src_time > dest_time)
            return false;
    }

    return true;
}

int64_t BuildSetBuilder::GetFileModificationTime(const char *filename)
{
    std::pair<int64_t *, bool> ret = mtime_map.Append(filename, -1);

    if (ret.second) {
        FileInfo file_info;
        if (!StatFile(filename, false, &file_info))
            return -1;

        *ret.first = file_info.modification_time;
    }

    return *ret.first;
}

bool RunBuildCommands(Span<const BuildCommand> commands, bool verbose)
{
    Async async;

    std::mutex out_mutex;
    Size progress_counter = 0;

    for (const BuildCommand &cmd: commands) {
        async.AddTask([&, cmd]() {
            RG_DEFER_N(dest_guard) { unlink(cmd.dest_filename); };

            // The lock is needed to garantuee ordering of progress counter. Atomics
            // do not help much because the LogInfo() calls need to be protected too.
            {
                std::lock_guard<std::mutex> out_lock(out_mutex);

                Size progress = 100 * progress_counter++ / commands.len;
                LogInfo("(%1%%) %2", FmtArg(progress).Pad(-3), verbose ? cmd.cmd : cmd.text);
            }

            // Run command
            HeapArray<char> output;
            int exit_code;
            if (!ExecuteCommandLine(cmd.cmd, &output, &exit_code))
                return false;

            // Print command output
            if (exit_code) {
                LogError("Command '%1' failed", cmd.cmd);
            }
            if (output.len) {
                std::lock_guard<std::mutex> out_lock(out_mutex);
                stdout_st.Write(output);
            }

            if (!exit_code) {
                dest_guard.Disable();
                return true;
            } else {
                return false;
            }
        });

        if (cmd.sync_after && !async.Sync())
            return false;
    }

    if (!async.Sync())
        return false;

    LogInfo("(100%%) Done!");
    return true;
}

}
