// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "build.hh"

namespace RG {

#ifdef _WIN32
    #define MAX_COMMAND_LEN 4096
#else
    #define MAX_COMMAND_LEN 32768
#endif

static const char *BuildObjectPath(const char *src_filename, const char *output_directory,
                                   const char *suffix, Allocator *alloc)
{
    RG_ASSERT(!PathIsAbsolute(src_filename));

    HeapArray<char> buf(alloc);

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

    return buf.TrimAndLeak(1).ptr;
}

static bool UpdateVersionSource(const char *target_name, const char *version_str,
                                bool fake, const char *dest_filename)
{
    if (!fake && !EnsureDirectoryExists(dest_filename))
        return false;

    char code[1024];
    Fmt(code, "const char *FelixTarget = \"%1\";\n"
              "const char *FelixVersion = \"%2\";\n", target_name, version_str);

    bool new_version;
    if (TestFile(dest_filename, FileType::File)) {
        char old_code[1024] = {};
        {
            StreamReader reader(dest_filename);
            reader.Read(RG_SIZE(old_code) - 1, old_code);
        }

        new_version = !TestStr(old_code, code);
    } else {
        new_version = true;
    }

    if (!fake && new_version) {
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

Builder::Builder(const BuildSettings &build)
    : build(build)
{
    RG_ASSERT(build.output_directory);
    RG_ASSERT(build.compiler);

    cache_filename = Fmt(&str_alloc, "%1%/cache%/FelixCache.bin", build.output_directory).ptr;
    LoadCache();
}

// Beware, failures can leave the Builder in a undefined state
bool Builder::AddTarget(const TargetInfo &target)
{
    HeapArray<const char *> obj_filenames;

    // Assets
    if (target.pack_filenames.len) {
        const char *src_filename = Fmt(&str_alloc, "%1%/cache%/%2_assets.c",
                                       build.output_directory, target.name).ptr;
        const char *obj_filename = Fmt(&str_alloc, "%1%2", src_filename,
                                       build.compiler->GetObjectExtension()).ptr;

        // Make C file
        {
            Command cmd = {};
            build.compiler->MakePackCommand(target.pack_filenames, build.compile_mode,
                                            target.pack_options, src_filename, &str_alloc, &cmd);

            const char *text = Fmt(&str_alloc, "Pack %1 assets", target.name).ptr;
            AppendNode(text, src_filename, cmd, target.pack_filenames);
        }

        bool module = (build.compile_mode == CompileMode::Debug ||
                       build.compile_mode == CompileMode::DebugFast);

        // Build object file
        {
            Command cmd = {};
            if (module) {
                build.compiler->MakeObjectCommand(src_filename, SourceType::C, CompileMode::Debug,
                                                  false, nullptr, {"EXPORT"}, {}, build.env,
                                                  obj_filename, &str_alloc, &cmd);
            } else {
                build.compiler->MakeObjectCommand(src_filename, SourceType::C, CompileMode::Debug,
                                                  false, nullptr, {}, {}, build.env,
                                                  obj_filename,  &str_alloc, &cmd);
            }

            const char *text = Fmt(&str_alloc, "Build %1 assets", target.name).ptr;
            AppendNode(text, obj_filename, cmd, src_filename);
        }

        // Build module if needed
        if (module) {
            const char *module_filename = Fmt(&str_alloc, "%1%/%2_assets%3", build.output_directory,
                                              target.name, RG_SHARED_LIBRARY_EXTENSION).ptr;

            Command cmd = {};
            build.compiler->MakeLinkCommand(obj_filename, CompileMode::Debug, {},
                                            LinkType::SharedLibrary, build.env,
                                            module_filename, &str_alloc, &cmd);

            const char *text = Fmt(&str_alloc, "Link %1",
                                   SplitStrReverseAny(module_filename, RG_PATH_SEPARATORS)).ptr;
            AppendNode(text, module_filename, cmd, obj_filename);
        } else {
            obj_filenames.Append(obj_filename);
        }
    }

    // Version string
    if (target.type == TargetType::Executable) {
        const char *src_filename = Fmt(&str_alloc, "%1%/cache%/%2.c", build.output_directory, target.name).ptr;
        const char *obj_filename = Fmt(&str_alloc, "%1%2", src_filename, build.compiler->GetObjectExtension()).ptr;

        if (!UpdateVersionSource(target.name, build.version_str, build.fake, src_filename))
            return false;

        Command cmd = {};
        build.compiler->MakeObjectCommand(src_filename, SourceType::C, build.compile_mode,
                                          false, nullptr, {}, {}, build.env,
                                          obj_filename, &str_alloc, &cmd);

        const char *text = Fmt(&str_alloc, "Build %1 version file", target.name).ptr;
        AppendNode(text, obj_filename, cmd, src_filename);

        obj_filenames.Append(obj_filename);
    }

    // Object commands
    for (const SourceFileInfo *src: target.sources) {
        const char *obj_filename = AddSource(*src);
        if (!obj_filename)
            return false;

        obj_filenames.Append(obj_filename);
    }

    // Some compilers (such as MSVC) also build PCH object files that need to be linked
    if (build.pch) {
        if (target.c_pch_src) {
            const char *pch_filename = build_map.FindValue(target.c_pch_src->filename, nullptr);
            const char *obj_filename = build.compiler->GetPchObject(pch_filename, &str_alloc);

            if (obj_filename) {
                obj_filenames.Append(obj_filename);
            }
        }
        if (target.cxx_pch_src) {
            const char *pch_filename = build_map.FindValue(target.cxx_pch_src->filename, nullptr);
            const char *obj_filename = build.compiler->GetPchObject(pch_filename, &str_alloc);

            if (obj_filename) {
                obj_filenames.Append(obj_filename);
            }
        }
    }

    // Link commands
    if (target.type == TargetType::Executable) {
        const char *target_filename = Fmt(&str_alloc, "%1%/%2%3", build.output_directory,
                                          target.name, build.compiler->GetExecutableExtension()).ptr;

        Command cmd = {};
        build.compiler->MakeLinkCommand(obj_filenames, build.compile_mode, target.libraries,
                                        LinkType::Executable, build.env, target_filename,
                                        &str_alloc, &cmd);

        const char *text = Fmt(&str_alloc, "Link %1", SplitStrReverseAny(target_filename, RG_PATH_SEPARATORS)).ptr;
        AppendNode(text, target_filename, cmd, obj_filenames);

        target_filenames.Set(target.name, target_filename);
    }

    return true;
}

const char *Builder::AddSource(const SourceFileInfo &src)
{
    // Precompiled header (if any)
    const char *pch_filename = nullptr;
    if (build.pch) {
        const SourceFileInfo *pch = nullptr;
        const char *pch_ext;
        switch (src.type) {
            case SourceType::C: {
                pch = src.target->c_pch_src;
                pch_ext = ".c";
            } break;
            case SourceType::CXX: {
                pch = src.target->cxx_pch_src;
                pch_ext = ".cc";
            } break;
        }

        if (pch) {
            pch_filename = build_map.FindValue(pch->filename, nullptr);

            if (!pch_filename) {
                pch_filename = BuildObjectPath(pch->filename, build.output_directory, pch_ext, &str_alloc);
                bool warnings = (pch->target->type != TargetType::ExternalLibrary);

                Command cmd = {};
                build.compiler->MakePchCommand(pch_filename, pch->type, build.compile_mode, warnings,
                                               pch->target->definitions, pch->target->include_directories,
                                               build.env, &str_alloc, &cmd);

                const char *text = Fmt(&str_alloc, "Precompile %1", pch->filename).ptr;
                if (AppendNode(text, pch_filename, cmd, pch->filename)) {
                    if (!build.fake && !CreatePrecompileHeader(pch->filename, pch_filename))
                        return (const char *)nullptr;
                }
            }
        }
    }

    const char *obj_filename = build_map.FindValue(src.filename, nullptr);

    // Build object
    if (!obj_filename) {
        obj_filename = BuildObjectPath(src.filename, build.output_directory,
                                       build.compiler->GetObjectExtension(), &str_alloc);
        bool warnings = (src.target->type != TargetType::ExternalLibrary);

        Command cmd = {};
        build.compiler->MakeObjectCommand(src.filename, src.type, build.compile_mode, warnings,
                                          pch_filename, src.target->definitions, src.target->include_directories,
                                          build.env, obj_filename, &str_alloc, &cmd);

        const char *text = Fmt(&str_alloc, "Build %1", src.filename).ptr;
        if (pch_filename ? AppendNode(text, obj_filename, cmd, {src.filename, pch_filename})
                         : AppendNode(text, obj_filename, cmd, src.filename)) {
            if (!build.fake && !EnsureDirectoryExists(obj_filename))
                return nullptr;
        }
    }

    return obj_filename;
}

// The caller needs to ignore (or do whetever) SIGINT for the clean up to work if
// the user interrupts felix. For this you can use libcc: call WaitForInterruption(0).
bool Builder::Build(int jobs, bool verbose)
{
    RG_ASSERT(jobs >= 0);

    // Reset build context
    clear_filenames.Clear();
    rsp_map.Clear();
    progress = total - nodes.len;
    interrupted = false;
    workers.Clear();
    workers.AppendDefault(jobs);

    RG_DEFER {
        // Update cache even if some tasks fail
        if (nodes.len && !build.fake) {
            for (const WorkerState &worker: workers) {
                for (const CacheEntry &entry: worker.entries) {
                    cache_map.Set(entry)->deps_offset = cache_dependencies.len;
                    for (Size i = 0; i < entry.deps_len; i++) {
                        const char *dep_filename = worker.dependencies[entry.deps_offset + i];
                        cache_dependencies.Append(DuplicateString(dep_filename, &str_alloc).ptr);
                    }
                }
            }
            workers.Clear();

            SaveCache();
        }

        // Clean up failed and temporary files
        // Windows has a tendency to hold file locks a bit longer than needed...
        // Try to delete files several times silently unless it's the last try.
#ifdef _WIN32
        if (clear_filenames.len) {
            PushLogFilter([](LogLevel, const char *, const char *, FunctionRef<LogFunc>) {});
            RG_DEFER { PopLogFilter(); };

            for (Size i = 0; i < 3; i++) {
                bool success = true;
                for (const char *filename: clear_filenames) {
                    success &= UnlinkFile(filename);
                }

                if (success)
                    return;

                WaitForDelay(150);
            }
        }
#endif
        for (const char *filename: clear_filenames) {
            UnlinkFile(filename);
        }
    };

    // Replace long command lines with response files if the command supports it
    if (!build.fake) {
        for (const Node &node: nodes) {
            const Command &cmd = node.cmd;

            if (cmd.cmd_line.len > MAX_COMMAND_LEN && cmd.rsp_offset > 0) {
                RG_ASSERT(cmd.rsp_offset < cmd.cmd_line.len);

                // In theory, there can be conflicts between RSP files. But it is unlikely
                // that response files will be generated for anything other than link commands,
                // so the risk is very low.
                const char *target_basename = SplitStrReverseAny(node.dest_filename, RG_PATH_SEPARATORS).ptr;
                const char *rsp_filename = Fmt(&str_alloc, "%1%/cache%/%2.rsp", build.output_directory, target_basename).ptr;

                Span<const char> rsp = cmd.cmd_line.Take(cmd.rsp_offset + 1,
                                                         cmd.cmd_line.len - cmd.rsp_offset - 1);

                // Apparently backslash characters needs to be escaped in response files,
                // but it's easier to use '/' instead.
                StreamWriter st(rsp_filename);
                for (char c: rsp) {
                    st.Write(c == '\\' ? '/' : c);
                }
                if (!st.Close())
                    return false;

                const char *new_cmd = Fmt(&str_alloc, "%1 \"@%2\"",
                                          cmd.cmd_line.Take(0, cmd.rsp_offset), rsp_filename).ptr;
                rsp_map.Set(&node, new_cmd);
            }
        }
    }

    LogInfo("Building...");
    int64_t now = GetMonotonicTime();

    Async async(jobs - 1);

    // Run nodes
    for (Size i = 0; i < nodes.len; i++) {
        Node *node = &nodes[i];

        if (!node->success && !node->semaphore) {
            async.Run([=, &async, this]() { return RunNode(&async, node, verbose); });
        }
    }

    if (async.Sync()) {
        LogInfo("Done (%1s)", FmtDouble((double)(GetMonotonicTime() - now) / 1000.0, 1));
        return true;
    } else if (interrupted) {
        LogError("Build was interrupted");
        return false;
    } else {
        return false;
    }
}

void Builder::SaveCache()
{
    if (!EnsureDirectoryExists(cache_filename))
        return;

    RG_DEFER_N(unlink_guard) {
        LogError("Purging cache file '%1'", cache_filename);
        UnlinkFile(cache_filename);
    };

    StreamWriter st(cache_filename, CompressionType::Gzip);
    if (!st.IsValid())
        return;

    for (const CacheEntry &entry: cache_map) {
        PrintLn(&st, "%1", entry.filename);
        PrintLn(&st, "!%1", entry.cmd_line);
        for (Size i = 0; i < entry.deps_len; i++) {
            PrintLn(&st, "+%1", cache_dependencies[entry.deps_offset + i]);
        }
    }

    if (!st.Close())
        return;

    unlink_guard.Disable();
}

void Builder::LoadCache()
{
    if (!TestFile(cache_filename))
        return;

    RG_DEFER_N(clear_guard) {
        cache_map.Clear();
        cache_dependencies.Clear();

        LogError("Purging cache file '%1'", cache_filename);
        UnlinkFile(cache_filename);
    };

    // Load whole file to memory
    HeapArray<char> cache(&str_alloc);
    if (ReadFile(cache_filename, Megabytes(64), CompressionType::Gzip, &cache) < 0)
        return;
    cache.len = TrimStrRight((Span<char>)cache, "\n").len;
    cache.SetCapacity(cache.len + 1);

    // Parse cache file
    {
        Span<char> remain = cache;

        Span<char> line = SplitStr(remain, '\n', &remain);
        line.ptr[line.len] = 0;

        while (remain.len) {
            CacheEntry entry = {};

            if (!line.len) {
                LogError("Malformed cache file");
                return;
            }
            entry.filename = line.ptr;

            entry.deps_offset = cache_dependencies.len;
            while (remain.len) {
                line = SplitStr(remain, '\n', &remain);
                if (line.len < 2) {
                    LogError("Malformed cache file");
                    return;
                }
                line.ptr[line.len] = 0;

                if (line[0] == '!') {
                    entry.cmd_line = line.Take(1, line.len - 1);
                } else if (line[0] == '+') {
                    cache_dependencies.Append(line.ptr + 1);
                } else {
                    break;
                }
            }
            entry.deps_len = cache_dependencies.len - entry.deps_offset;

            cache_map.Set(entry);
        }
    }

    cache.Leak();
    clear_guard.Disable();
}

bool Builder::AppendNode(const char *text, const char *dest_filename, const Command &cmd,
                         Span<const char *const> src_filenames)
{
    RG_ASSERT(src_filenames.len >= 1);

    build_map.Set(src_filenames[0], dest_filename);
    total++;

    if (NeedsRebuild(dest_filename, cmd, src_filenames)) {
        Size node_idx = nodes.len;
        Node *node = nodes.AppendDefault();

        node->text = text;
        node->dest_filename = dest_filename;
        node->cmd = cmd;

        // Add triggers to source file nodes
        for (const char *src_filename: src_filenames) {
            Size src_idx = nodes_map.FindValue(src_filename, -1);

            if (src_idx >= 0) {
                Node *src = &nodes[src_idx];

                src->triggers.Append(node_idx);
                node->semaphore++;
            }
        }

        nodes_map.Set(dest_filename, node_idx);
        mtime_map.Set(dest_filename, -1);

        return true;
    } else {
        return false;
    }
}

bool Builder::NeedsRebuild(const char *dest_filename, const Command &cmd,
                           Span<const char *const> src_filenames)
{
    const CacheEntry *entry = cache_map.Find(dest_filename);

    if (!entry)
        return true;
    if (!TestStr(cmd.cmd_line.Take(0, cmd.cache_len), entry->cmd_line))
        return true;

    if (!IsFileUpToDate(dest_filename, src_filenames))
        return true;

    Span<const char *> dep_filenames = MakeSpan(cache_dependencies.ptr + entry->deps_offset, entry->deps_len);
    if (!IsFileUpToDate(dest_filename, dep_filenames))
        return true;

    return false;
}

bool Builder::IsFileUpToDate(const char *dest_filename, Span<const char *const> src_filenames)
{
    if (build.rebuild)
        return false;

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

int64_t Builder::GetFileModificationTime(const char *filename)
{
    int64_t *ptr = mtime_map.Find(filename);

    if (ptr) {
        return *ptr;
    } else {
        FileInfo file_info;
        if (!StatFile(filename, false, &file_info))
            return -1;

        // filename might be temporary (e.g. dependency filenames in NeedsRebuild())
        const char *filename2 = DuplicateString(filename, &str_alloc).ptr;
        mtime_map.Set(filename2, file_info.modification_time);

        return file_info.modification_time;
    }
}

static Span<const char> ParseMakeFragment(Span<const char> remain, HeapArray<char> *out_frag)
{
    // Skip white spaces
    remain = TrimStrLeft(remain);

    if (remain.len) {
        out_frag->Append(remain[0]);

        Size i = 1;
        for (; i < remain.len && !strchr("\r\n", remain[i]); i++) {
            char c = remain[i];

            // The 'i > 1' check is for absolute Windows paths
            if (c == ':' && i > 1)
                break;

            if (strchr(" $#", c)) {
                if (remain[i - 1] == '\\') {
                    (*out_frag)[out_frag->len - 1] = c;
                } else {
                    break;
                }
            } else {
                out_frag->Append(c);
            }
        }

        remain = remain.Take(i, remain.len - i);
    }

    return remain;
}

static bool ParseMakeRule(const char *filename, Allocator *alloc, HeapArray<const char *> *out_filenames)
{
    HeapArray<char> rule_buf;
    if (ReadFile(filename, Megabytes(2), &rule_buf) < 0)
        return false;

    // Parser state
    Span<const char> remain = rule_buf;
    HeapArray<char> frag;

    // Skip outputs
    while (remain.len) {
        frag.RemoveFrom(0);
        remain = ParseMakeFragment(remain, &frag);

        if ((Span<const char>)frag == ":")
            break;
    }

    // Get dependency filenames
    while (remain.len) {
        frag.RemoveFrom(0);
        remain = ParseMakeFragment(remain, &frag);

        if (frag.len && (Span<const char>)frag != "\\") {
            const char *dep_filename = NormalizePath(frag, alloc).ptr;
            out_filenames->Append(dep_filename);
        }
    }

    return true;
}

static Size ExtractShowIncludes(Span<char> buf, Allocator *alloc, HeapArray<const char *> *out_filenames)
{
    // We need to strip include notes from the output
    Span<char> new_buf = MakeSpan(buf.ptr, 0);

    while (buf.len) {
        Span<const char> line = SplitStr(buf, '\n', &buf);

        // MS had the brilliant idea to localize inclusion notes.. In english it starts
        // with 'Note: including file:  ' but it can basically be anything. We match
        // lines that start with a non-space character, and which contain a colon
        // followed by two spaces we take the line. Not pretty, hopefully it is alright.
        Span<const char> dep = {};
        if (line.len && !IsAsciiWhite(line[0])) {
            for (Size i = 0; i < line.len - 3; i++) {
                if (line[i] == ':' && line[i + 1] == ' ' && line[i + 2] == ' ') {
                    dep = TrimStr(line.Take(i + 3, line.len - i - 3));
                    break;
                }
            }
        }

        if (dep.len) {
            if (out_filenames) {
                const char *dep_filename = DuplicateString(dep, alloc).ptr;
                out_filenames->Append(dep_filename);
            }
        } else {
            Size copy_len = line.len + (buf.ptr > line.end());

            memmove(new_buf.end(), line.ptr, copy_len);
            new_buf.len += copy_len;
        }
    }

    return new_buf.len;
}

bool Builder::RunNode(Async *async, Node *node, bool verbose)
{
    const Command &cmd = node->cmd;

    WorkerState *worker = &workers[Async::GetWorkerIdx()];
    const char *cmd_line = rsp_map.FindValue(&node, cmd.cmd_line.ptr);

    // The lock is needed to guarantee ordering of progress counter. Atomics
    // do not help much because the LogInfo() calls need to be protected too.
    {
        std::lock_guard<std::mutex> out_lock(out_mutex);

        int pad = (int)log10(total) + 3;
        progress++;

        LogInfo("%!C..%1/%2%!0 %3", FmtArg(progress).Pad(-pad), total, node->text);
        if (verbose) {
            PrintLn(cmd_line);
            fflush(stdout);
        }
    }

    // Run command
    HeapArray<char> output_buf;
    int exit_code;
    bool started;
    if (!build.fake) {
        started = ExecuteCommandLine(cmd_line, {}, Megabytes(4), &output_buf, &exit_code);
    } else {
        started = true;
        exit_code = 0;
    }

    // Skip first output lines (if needed)
    Span<char> output = output_buf;
    for (Size i = 0; i < cmd.skip_lines; i++) {
        SplitStr(output, '\n', &output);
    }

    // Deal with results
    if (started && !exit_code) {
        // Update cache entries
        {
            CacheEntry entry;

            entry.filename = node->dest_filename;
            entry.cmd_line = cmd.cmd_line.Take(0, cmd.cache_len);

            entry.deps_offset = worker->dependencies.len;
            switch (cmd.deps_mode) {
                case Command::DependencyMode::None: {} break;
                case Command::DependencyMode::MakeLike: {
                    if (TestFile(cmd.deps_filename)) {
                        started &= ParseMakeRule(cmd.deps_filename, &worker->str_alloc,
                                                 &worker->dependencies);
                        UnlinkFile(cmd.deps_filename);
                    }
                } break;
                case Command::DependencyMode::ShowIncludes: {
                    output.len = ExtractShowIncludes(output, &worker->str_alloc,
                                                     &worker->dependencies);
                } break;
            }
            entry.deps_len = worker->dependencies.len - entry.deps_offset;

            worker->entries.Append(entry);
        }

        if (output.len && !cmd.skip_success) {
            std::lock_guard<std::mutex> out_lock(out_mutex);
            stdout_st.Write(output);
        }

        // Trigger dependent nodes
        for (Size trigger_idx: node->triggers) {
            Node *trigger = &nodes[trigger_idx];

            if (!--trigger->semaphore) {
                RG_ASSERT(!trigger->success);
                async->Run([=, this]() { return RunNode(async, trigger, verbose); });
            }
        }

        node->success = true;
        return true;
    } else {
        std::lock_guard<std::mutex> out_lock(out_mutex);

        // Even through we don't care about dependencies, we still want to
        // remove include notes from the output buffer.
        if (cmd.deps_mode == Command::DependencyMode::ShowIncludes) {
            output.len = ExtractShowIncludes(output, &str_alloc, nullptr);
        }

        cache_map.Remove(node->dest_filename);
        clear_filenames.Append(node->dest_filename);

        if (!started) {
            // Error already issued by ExecuteCommandLine()
            stderr_st.Write(output);
        } else if (exit_code == 130) {
            interrupted = true; // SIGINT
        } else {
            LogError("%1 (exit code %2)", node->text, exit_code);
            stderr_st.Write(output);
        }

        return false;
    }
}

}
