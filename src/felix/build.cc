// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef _WIN32
    #include <unistd.h>
#endif

#include "../libcc/libcc.hh"
#include "build.hh"

namespace RG {

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

static const char *BuildObjectPath(const char *src_filename, const char *output_directory,
                                   const char *suffix, Allocator *alloc)
{
    RG_ASSERT(!PathIsAbsolute(src_filename));

    HeapArray<char> buf;
    buf.allocator = alloc;

    Size offset = Fmt(&buf, "%1%/objects%/", output_directory).len;
    Fmt(&buf, "%1%2", src_filename, suffix);

    // Replace '..' components with '__'
    {
        char *ptr = buf.ptr + offset;

        while ((ptr = strstr(ptr, ".."))) {
            if (IsPathSeparator(ptr[-1]) && (IsPathSeparator(ptr[2]) || !ptr[2])) {
                ptr[0] = '_';
                ptr[1] = '_';
            }

            ptr += 2;
        }
    }

    return buf.Leak().ptr;
}

static bool UpdateVersionSource(const char *version_str, const char *dest_filename)
{
    if (!EnsureDirectoryExists(dest_filename))
        return false;

    char code[512];
    Fmt(code, "const char *BuildVersion = \"%1\";\n", version_str);

    bool new_version;
    if (TestFile(dest_filename, FileType::File)) {
        char old_code[512] = {};
        {
            StreamReader reader(dest_filename);
            reader.Read(RG_SIZE(old_code) - 1, old_code);
        }

        new_version = !TestStr(old_code, code);
    } else {
        new_version = true;
    }

    if (new_version) {
        return WriteFile(code, dest_filename);
    } else {
        return true;
    }
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

    obj_filenames.RemoveFrom(0);
    definitions.RemoveFrom(0);
    definitions.Append(target.definitions);

    bool warnings = (target.type != TargetType::ExternalLibrary);

    // Precompiled headers
    const char *c_pch_filename = nullptr;
    const char *cxx_pch_filename = nullptr;
    const auto add_pch_object = [&](const char *src_filename, SourceType src_type) {
        const char *pch_filename = BuildObjectPath(src_filename, output_directory,
                                                   ".pch.h", &temp_alloc);
        const char *deps_filename = Fmt(&temp_alloc, "%1.d", pch_filename).ptr;

        if (NeedsRebuild(src_filename, pch_filename, deps_filename)) {
            BuildCommand cmd = {};

            cmd.text = Fmt(&str_alloc, "Precompile %1", src_filename).ptr;
            cmd.dest_filename = DuplicateString(pch_filename, &str_alloc).ptr;
            if (!CreatePrecompileHeader(src_filename, pch_filename))
                return (const char *)nullptr;

            cmd.cmd = compiler->MakeObjectCommand(pch_filename, src_type, build_mode, warnings,
                                                  nullptr, definitions, target.include_directories,
                                                  nullptr, deps_filename, &str_alloc);
            if (!cmd.cmd)
                return (const char *)nullptr;

            pch_commands.Append(cmd);
        }

        return pch_filename;
    };
    if (target.c_pch_filename) {
        c_pch_filename = add_pch_object(target.c_pch_filename, SourceType::C_Header);
        if (!c_pch_filename)
            return false;
    }
    if (target.cxx_pch_filename) {
        cxx_pch_filename = add_pch_object(target.cxx_pch_filename, SourceType::CXX_Header);
        if (!cxx_pch_filename)
            return false;
    }

    // Build information
    if (!version_init && version_str) {
        const char *src_filename = Fmt(&temp_alloc, "%1%/resources%/version.c", output_directory).ptr;
        version_obj_filename = Fmt(&str_alloc, "%1%/resources%/version.c.o", output_directory).ptr;

        if (UpdateVersionSource(version_str, src_filename)) {
            if (!IsFileUpToDate(version_obj_filename, src_filename)) {
                BuildCommand cmd = {};

                cmd.text = "Build version file";
                cmd.dest_filename = version_obj_filename;
                cmd.cmd = compiler->MakeObjectCommand(src_filename, SourceType::C_Source, build_mode, false,
                                                      nullptr, {}, {}, version_obj_filename, nullptr, &str_alloc);

                obj_commands.Append(cmd);

                // Pretend object file does not exist to force link step
                mtime_map.Set(version_obj_filename, -1);
            }
        } else {
            LogError("Failed to build git version string");
            version_obj_filename = nullptr;
        }

        version_init = true;
    }
    if (version_obj_filename) {
        obj_filenames.Append(version_obj_filename);
        definitions.Append("FELIX_VERSION");
    }

    // Object commands
    for (const SourceFileInfo &src: target.sources) {
        const char *obj_filename = BuildObjectPath(src.filename, output_directory,
                                                   ".o", &temp_alloc);
        const char *deps_filename = Fmt(&temp_alloc, "%1.d", obj_filename).ptr;

        if (NeedsRebuild(src.filename, obj_filename, deps_filename)) {
            BuildCommand cmd = {};

            const char *pch_filename = nullptr;
            switch (src.type) {
                case SourceType::C_Source: { pch_filename = c_pch_filename; } break;
                case SourceType::CXX_Source: { pch_filename = cxx_pch_filename; } break;

                case SourceType::C_Header:
                case SourceType::CXX_Header: { RG_ASSERT(false); } break;
            }

            cmd.text = Fmt(&str_alloc, "Build %1", src.filename).ptr;
            cmd.dest_filename = DuplicateString(obj_filename, &str_alloc).ptr;
            if (!EnsureDirectoryExists(obj_filename))
                return false;
            cmd.cmd = compiler->MakeObjectCommand(src.filename, src.type, build_mode, warnings,
                                                  pch_filename, definitions, target.include_directories,
                                                  obj_filename, deps_filename, &str_alloc);
            if (!cmd.cmd)
                return false;

            obj_commands.Append(cmd);

            // Pretend object file does not exist to force link step
            mtime_map.Set(obj_filename, -1);
        }

        obj_filenames.Append(obj_filename);
    }

    // Assets
    if (target.pack_filenames.len) {
        const char *obj_filename = Fmt(&temp_alloc, "%1%/assets%/%2_assets.o",
                                       output_directory, target.name).ptr;

        if (!IsFileUpToDate(obj_filename, target.pack_filenames)) {
            BuildCommand cmd = {};

            cmd.text = Fmt(&str_alloc, "Pack %1 assets", target.name).ptr;
            cmd.dest_filename = DuplicateString(obj_filename, &str_alloc).ptr;
            if (!EnsureDirectoryExists(obj_filename))
                return false;
            cmd.cmd = compiler->MakePackCommand(target.pack_filenames, build_mode,
                                                target.pack_options, obj_filename, &str_alloc);
            if (!cmd.cmd)
                return false;

            obj_commands.Append(cmd);

            // Pretend object file does not exist to force link step
            mtime_map.Set(obj_filename, -1);
        }

        bool module = false;
        switch (target.pack_link_type) {
            case PackLinkType::Static: { module = false; } break;
            case PackLinkType::Module: { module = true; } break;
            case PackLinkType::ModuleIfDebug: { module = (build_mode == BuildMode::Debug ||
                                                          build_mode == BuildMode::DebugFast); } break;
        }

        if (module) {
#ifdef _WIN32
            const char *module_filename = Fmt(&temp_alloc, "%1%/%2_assets.dll",
                                              output_directory, target.name).ptr;
#else
            const char *module_filename = Fmt(&temp_alloc, "%1%/%2_assets.so",
                                              output_directory, target.name).ptr;
#endif

            if (!IsFileUpToDate(module_filename, obj_filename)) {
                BuildCommand cmd = {};

                // TODO: Check if this conflicts with a target destination file?
                cmd.text = Fmt(&str_alloc, "Link %1",
                               SplitStrReverseAny(module_filename, RG_PATH_SEPARATORS)).ptr;
                cmd.dest_filename = DuplicateString(module_filename, &str_alloc).ptr;
                cmd.cmd = compiler->MakeLinkCommand(obj_filename, BuildMode::Debug, {},
                                                    LinkType::SharedLibrary, module_filename,
                                                    &str_alloc);

                link_commands.Append(cmd);
            }
        } else {
            obj_filenames.Append(obj_filename);
        }
    }

    // Link commands
    if (target.type == TargetType::Executable) {
#ifdef _WIN32
        const char *target_filename = Fmt(&str_alloc, "%1%/%2.exe", output_directory, target.name).ptr;
#else
        const char *target_filename = Fmt(&str_alloc, "%1%/%2", output_directory, target.name).ptr;
#endif

        if (!IsFileUpToDate(target_filename, obj_filenames)) {
            BuildCommand cmd = {};

            cmd.text = Fmt(&str_alloc, "Link %1",
                           SplitStrReverseAny(target_filename, RG_PATH_SEPARATORS)).ptr;
            cmd.dest_filename = DuplicateString(target_filename, &str_alloc).ptr;
            cmd.cmd = compiler->MakeLinkCommand(obj_filenames, build_mode, target.libraries,
                                                LinkType::Executable, target_filename, &str_alloc);
            if (!cmd.cmd)
                return false;

            link_commands.Append(cmd);
        }

        target_filenames.Set(target.name, target_filename);
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
    RG_ASSERT(!out_set->commands.len);

    if (pch_commands.len) {
        pch_commands[pch_commands.len - 1].sync_after = true;
    }
    if (obj_commands.len) {
        obj_commands[obj_commands.len - 1].sync_after = true;
    }

    out_set->commands.Append(pch_commands);
    out_set->commands.Append(obj_commands);
    out_set->commands.Append(link_commands);
    std::swap(out_set->target_filenames, target_filenames);

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

// The caller needs to ignore (or do whetever) SIGINT for the clean up to work if
// the user interrupts felix. For this you can use libcc: call WaitForInterruption(0).
bool RunBuildCommands(Span<const BuildCommand> commands, int jobs, bool verbose)
{
    Async async(jobs - 1);

    std::mutex out_mutex;
    Size progress_counter = 0;
    bool interrupted = false;

    for (const BuildCommand &cmd: commands) {
        async.Run([&, cmd]() {
            // The lock is needed to guarantee ordering of progress counter. Atomics
            // do not help much because the LogInfo() calls need to be protected too.
            {
                std::lock_guard<std::mutex> out_lock(out_mutex);

                Size progress = 100 * progress_counter++ / commands.len;
                LogInfo("(%1%%) %2", FmtArg(progress).Pad(-3), verbose ? cmd.cmd : cmd.text);
            }

            // Run command
            HeapArray<char> output;
            int exit_code;
            bool started = ExecuteCommandLine(cmd.cmd, {}, Megabytes(1), &output, &exit_code);

            // Deal with results
            if (started && !exit_code) {
                if (output.len) {
                    std::lock_guard<std::mutex> out_lock(out_mutex);
                    stdout_st.Write(output);
                }

                return true;
            } else {
                unlink(cmd.dest_filename);

                if (!started) {
                    // Error already issued by ExecuteCommandLine()
                } else if (exit_code == 130) {
                    interrupted = true; // SIGINT
                } else {
                    LogError("Command '%1' failed (exit code %2)", cmd.cmd, exit_code);

                    std::lock_guard<std::mutex> out_lock(out_mutex);
                    stderr_st.Write(output);
                }

                return false;
            }
        });

        if (cmd.sync_after && !async.Sync())
            break;
    }

    if (async.Sync()) {
        LogInfo("(100%%) Done!");
        return true;
    } else if (interrupted) {
        LogError("Build was interrupted");
        return false;
    } else {
        return false;
    }
}

}
