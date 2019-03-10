// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef _WIN32
    #include <unistd.h>
#endif

#include "../../libcc/libcc.hh"
#include "build_command.hh"

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

static bool EnsureDirectoryExists(const char *filename)
{
    Span<const char> directory;
    SplitStrReverseAny(filename, PATH_SEPARATORS, &directory);

    return MakeDirectoryRec(directory);
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
    DEFER_N(out_guard) {
        pch_commands.RemoveFrom(start_pch_len);
        obj_commands.RemoveFrom(start_obj_len);
        link_commands.RemoveFrom(start_link_len);
    };

    HeapArray<const char *> obj_filenames;

    // Precompiled headers
    for (const ObjectInfo &obj: target.pch_objects) {
        const char *deps_filename = Fmt(&temp_alloc, "%1.d", obj.dest_filename).ptr;

        if (NeedsRebuild(obj.src_filename, obj.dest_filename, deps_filename)) {
            BuildCommand cmd = {};

            cmd.text = Fmt(&str_alloc, "Precompile %1", obj.src_filename).ptr;
            cmd.dest_filename = DuplicateString(obj.dest_filename, &str_alloc).ptr;
            if (!CreatePrecompileHeader(obj.src_filename, obj.dest_filename))
                return false;

            cmd.cmd = compiler->BuildObjectCommand(obj.dest_filename, obj.src_type, build_mode, nullptr,
                                                   target.definitions, target.include_directories,
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
                case SourceType::CXX_Header: { DebugAssert(false); } break;
            }

            cmd.text = Fmt(&str_alloc, "Build %1", obj.src_filename).ptr;
            cmd.dest_filename = DuplicateString(obj.dest_filename, &str_alloc).ptr;
            if (!EnsureDirectoryExists(obj.dest_filename))
                return false;
            cmd.cmd = compiler->BuildObjectCommand(obj.src_filename, obj.src_type, build_mode, pch_filename,
                                                   target.definitions, target.include_directories,
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
    if (target.pack_filename) {
        if (!IsFileUpToDate(target.pack_filename, target.pack_filenames)) {
            BuildCommand cmd = {};

            cmd.text = Fmt(&str_alloc, "Pack assets for %1", target.name).ptr;
            cmd.dest_filename = DuplicateString(target.pack_filename, &str_alloc).ptr;
            if (!EnsureDirectoryExists(target.pack_filename))
                return false;
            cmd.cmd = compiler->BuildPackCommand(target.pack_filenames, target.pack_options,
                                                 target.pack_filename, &str_alloc);
            if (!cmd.cmd)
                return false;

            obj_commands.Append(cmd);

            // Pretend object file does not exist to force link step
            mtime_map.Set(target.pack_filename, -1);
        }

        obj_filenames.Append(target.pack_filename);
    }

    // Link commands
    if (target.type == TargetType::Executable &&
            !IsFileUpToDate(target.dest_filename, obj_filenames)) {
        BuildCommand cmd = {};

        cmd.text = Fmt(&str_alloc, "Link %1",
                       SplitStrReverseAny(target.dest_filename, PATH_SEPARATORS)).ptr;
        cmd.dest_filename = DuplicateString(target.dest_filename, &str_alloc).ptr;
        cmd.cmd = compiler->BuildLinkCommand(obj_filenames, build_mode, target.libraries,
                                             target.dest_filename, &str_alloc);
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

    out_guard.disable();
    return true;
}

void BuildSetBuilder::Finish(BuildSet *out_set)
{
    DebugAssert(!out_set->commands.len);

    if (pch_commands.len) {
        pch_commands[pch_commands.len - 1].sync_after = true;
    }
    if (obj_commands.len) {
        obj_commands[obj_commands.len - 1].sync_after = true;
    }

    out_set->commands.Append(pch_commands);
    out_set->commands.Append(obj_commands);
    out_set->commands.Append(link_commands);

    SwapMemory(&out_set->str_alloc, &str_alloc, SIZE(str_alloc));
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

    std::mutex log_mutex;
    int progress_counter = 0;

    for (const BuildCommand &cmd: commands) {
        async.AddTask([&, cmd]() {
            // The lock is needed to garantuee ordering of progress counter. Atomics
            // do not help much because the LogInfo() calls need to be protected too.
            {
                std::lock_guard lock(log_mutex);

                int progress = 100 * progress_counter++ / commands.len;
                LogInfo("(%1%%) %2", FmtArg(progress).Pad(-3), verbose ? cmd.cmd : cmd.text);
            }

            // Run command
            errno = 0;
            if (system(cmd.cmd) || errno) {
                LogError("Command '%1' failed", cmd.cmd);
#ifdef _WIN32
                _unlink(cmd.dest_filename);
#else
                unlink(cmd.dest_filename);
#endif
                return false;
            }

            return true;
        });

        if (cmd.sync_after && !async.Sync())
            return false;
    }

    if (!async.Sync())
        return false;

    LogInfo("(100%%) Done!");
    return true;
}
