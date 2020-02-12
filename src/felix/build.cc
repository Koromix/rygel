// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "build.hh"

#ifndef _WIN32
    #include <unistd.h>
#endif

namespace RG {

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

static bool ParseCompilerMakeRule(const char *filename, Allocator *alloc,
                                  HeapArray<const char *> *out_filenames)
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

Builder::Builder(const BuildSettings &settings)
    : output_directory(settings.output_directory), compiler(settings.compiler),
      compile_mode(settings.compile_mode), version_str(settings.version_str),
      async(settings.jobs - 1)
{
    RG_ASSERT(output_directory);
    RG_ASSERT(compiler);
    RG_ASSERT(settings.jobs >= 0);
}

bool Builder::AddTarget(const Target &target)
{
    const Size start_prep_len = prep_nodes.len;
    const Size start_obj_len = obj_nodes.len;
    const Size start_link_len = link_nodes.len;
    RG_DEFER_N(out_guard) {
        prep_nodes.RemoveFrom(start_prep_len);
        obj_nodes.RemoveFrom(start_obj_len);
        link_nodes.RemoveFrom(start_link_len);
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
            Node node = {};

            node.text = Fmt(&str_alloc, "Precompile %1", src_filename).ptr;
            node.dest_filename = DuplicateString(pch_filename, &str_alloc).ptr;
            if (!CreatePrecompileHeader(src_filename, pch_filename))
                return (const char *)nullptr;
            compiler->MakeObjectCommand(pch_filename, src_type, compile_mode, warnings,
                                        nullptr, definitions, target.include_directories,
                                        nullptr, deps_filename, &str_alloc, &node.cmd);

            prep_nodes.Append(node);
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
                Node node = {};

                node.text = "Build version file";
                node.dest_filename = version_obj_filename;
                compiler->MakeObjectCommand(src_filename, SourceType::C_Source, compile_mode,
                                            false, nullptr, {}, {}, version_obj_filename, nullptr,
                                            &str_alloc, &node.cmd);

                obj_nodes.Append(node);

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
            Node node = {};

            const char *pch_filename = nullptr;
            switch (src.type) {
                case SourceType::C_Source: { pch_filename = c_pch_filename; } break;
                case SourceType::CXX_Source: { pch_filename = cxx_pch_filename; } break;

                case SourceType::C_Header:
                case SourceType::CXX_Header: { RG_ASSERT(false); } break;
            }

            node.text = Fmt(&str_alloc, "Build %1", src.filename).ptr;
            node.dest_filename = DuplicateString(obj_filename, &str_alloc).ptr;
            if (!EnsureDirectoryExists(obj_filename))
                return false;
            compiler->MakeObjectCommand(src.filename, src.type, compile_mode, warnings,
                                        pch_filename, definitions, target.include_directories,
                                        obj_filename, deps_filename, &str_alloc, &node.cmd);

            obj_nodes.Append(node);

            // Pretend object file does not exist to force link step
            mtime_map.Set(obj_filename, -1);
        }

        obj_filenames.Append(obj_filename);
    }

    // Assets
    if (target.pack_filenames.len) {
        const char *src_filename = Fmt(&temp_alloc, "%1%/assets%/%2_assets.c",
                                       output_directory, target.name).ptr;
        const char *obj_filename = Fmt(&temp_alloc, "%1%/assets%/%2_assets.o",
                                       output_directory, target.name).ptr;

        // Make C file
        if (!IsFileUpToDate(src_filename, target.pack_filenames)) {
            Node node = {};

            node.text = Fmt(&str_alloc, "Pack %1 assets", target.name).ptr;
            node.dest_filename = DuplicateString(src_filename, &str_alloc).ptr;
            if (!EnsureDirectoryExists(src_filename))
                return false;
            MakePackCommand(target.pack_filenames, compile_mode,
                            target.pack_options, src_filename, &str_alloc, &node.cmd);

            prep_nodes.Append(node);

            // Pretend source file does not exist to force next step
            mtime_map.Set(src_filename, -1);
        }

        // Build object file
        if (!IsFileUpToDate(obj_filename, src_filename)) {
            Node node = {};

            node.text = Fmt(&str_alloc, "Build %1 assets", target.name).ptr;
            node.dest_filename = DuplicateString(obj_filename, &str_alloc).ptr;
            compiler->MakeObjectCommand(src_filename, SourceType::C_Source, CompileMode::Debug, false,
                                        nullptr, {}, {}, obj_filename, nullptr, &str_alloc, &node.cmd);

            obj_nodes.Append(node);

            // Pretend object file does not exist to force link step
            mtime_map.Set(obj_filename, -1);
        }

        bool module = false;
        switch (target.pack_link_type) {
            case PackLinkType::Static: { module = false; } break;
            case PackLinkType::Module: { module = true; } break;
            case PackLinkType::ModuleIfDebug: { module = (compile_mode == CompileMode::Debug); } break;
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
                Node node = {};

                // XXX: Check if this conflicts with a target destination file?
                node.text = Fmt(&str_alloc, "Link %1",
                                SplitStrReverseAny(module_filename, RG_PATH_SEPARATORS)).ptr;
                node.dest_filename = DuplicateString(module_filename, &str_alloc).ptr;
                compiler->MakeLinkCommand(obj_filename, CompileMode::Debug, {},
                                          LinkType::SharedLibrary, module_filename,
                                          &str_alloc, &node.cmd);

                link_nodes.Append(node);
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
            Node node = {};

            node.text = Fmt(&str_alloc, "Link %1",
                           SplitStrReverseAny(target_filename, RG_PATH_SEPARATORS)).ptr;
            node.dest_filename = DuplicateString(target_filename, &str_alloc).ptr;
            compiler->MakeLinkCommand(obj_filenames, compile_mode, target.libraries,
                                      LinkType::Executable, target_filename, &str_alloc, &node.cmd);

            link_nodes.Append(node);
        }

        target_filenames.Set(target.name, target_filename);
    }

    // Do this at the end because it's much harder to roll back changes in out_guard
    for (Size i = start_prep_len; i < prep_nodes.len; i++) {
        output_set.Append(prep_nodes[i].dest_filename);
    }
    for (Size i = start_obj_len; i < obj_nodes.len; i++) {
        output_set.Append(obj_nodes[i].dest_filename);
    }
    for (Size i = start_link_len; i < link_nodes.len; i++) {
        output_set.Append(link_nodes[i].dest_filename);
    }

    out_guard.Disable();
    return true;
}

// The caller needs to ignore (or do whetever) SIGINT for the clean up to work if
// the user interrupts felix. For this you can use libcc: call WaitForInterruption(0).
bool Builder::Build(bool verbose)
{
    Size total = prep_nodes.len + obj_nodes.len + link_nodes.len;

    if (!RunNodes(prep_nodes, 0, total, verbose))
        return false;
    if (!RunNodes(obj_nodes, prep_nodes.len, total, verbose))
        return false;
    if (!RunNodes(link_nodes, prep_nodes.len + obj_nodes.len, total, verbose))
        return false;

    LogInfo("(100%%) Done!");
    return true;
}

bool Builder::RunNodes(Span<const Node> nodes, Size progress, Size total, bool verbose)
{
#ifdef _WIN32
    BlockAllocator temp_alloc;

    HeapArray<const char *> rsp_filenames;
    HashMap<const void *, const char *> rsp_map;

    RG_DEFER {
        for (const char *filename: rsp_filenames) {
            UnlinkFile(filename);
        }
    };

    // Windows (especially cmd) does not like excessively long command lines,
    // so we need to use response files in this case.
    for (const Node &node: nodes) {
        if (node.cmd.line.len > 4096 && node.cmd.rsp_offset > 0) {
            RG_ASSERT(node.cmd.rsp_offset < node.cmd.line.len);

            const char *rsp_filename = Fmt(&temp_alloc, "%1.rsp", node.dest_filename).ptr;
            Span<const char> rsp = node.cmd.line.Take(node.cmd.rsp_offset + 1,
                                                      node.cmd.line.len - node.cmd.rsp_offset - 1);

            // Apparently backslash characters needs to be escaped in response files,
            // but it's easier to use '/' instead.
            StreamWriter st(rsp_filename);
            for (char c: rsp) {
                st.Write(c == '\\' ? '/' : c);
            }
            if (!st.Close())
                return false;

            const char *new_cmd = Fmt(&temp_alloc, "%1 \"@%2\"",
                                      node.cmd.line.Take(0, node.cmd.rsp_offset), rsp_filename).ptr;

            rsp_filenames.Append(rsp_filename);
            rsp_map.Append(&node, new_cmd);
        }
    }
#endif

    std::mutex out_mutex;
    bool interrupted = false;

    for (const Node &node: nodes) {
        async.Run([&]() {
#ifdef _WIN32
            const char *cmd_line = rsp_map.FindValue(&node, node.cmd.line.ptr);
#else
            const char *cmd_line = node.cmd.line.ptr;
#endif

            // The lock is needed to guarantee ordering of progress counter. Atomics
            // do not help much because the LogInfo() calls need to be protected too.
            {
                std::lock_guard<std::mutex> out_lock(out_mutex);

                Size progress_pct = 100 * progress++ / total;
                LogInfo("(%1%%) %2", FmtArg(progress_pct).Pad(-3), node.text);
                if (verbose) {
                    PrintLn(stderr, cmd_line);
                }
            }

            // Run command
            HeapArray<char> output;
            int exit_code;
            bool started = ExecuteCommandLine(cmd_line, {}, Megabytes(1), &output, &exit_code);

            // Deal with results
            if (started && !exit_code) {
                if (output.len) {
                    std::lock_guard<std::mutex> out_lock(out_mutex);
                    stdout_st.Write(output);
                }

                return true;
            } else {
                UnlinkFile(node.dest_filename);

                if (!started) {
                    // Error already issued by ExecuteCommandLine()
                } else if (exit_code == 130) {
                    interrupted = true; // SIGINT
                } else {
                    std::lock_guard<std::mutex> out_lock(out_mutex);

                    LogError("Failed: %1 (exit code %2)", node.text, exit_code);
                    stderr_st.Write(output);
                }

                return false;
            }
        });
    }

    if (async.Sync()) {
        return true;
    } else if (interrupted) {
        LogError("Build was interrupted");
        return false;
    } else {
        return false;
    }
}

bool Builder::NeedsRebuild(const char *src_filename, const char *dest_filename,
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

bool Builder::IsFileUpToDate(const char *dest_filename, Span<const char *const> src_filenames)
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

int64_t Builder::GetFileModificationTime(const char *filename)
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

}
