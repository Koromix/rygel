// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "src/core/wrap/json.hh"
#include "src/core/wrap/xml.hh"
#include "build.hh"
#include "locate.hh"

namespace RG {

#if defined(_WIN32)
    #define MAX_COMMAND_LEN 4096
#else
    #define MAX_COMMAND_LEN 32768
#endif

template <typename T>
static bool AssembleResourceFile(const pugi::xml_document *doc, const char *icon_filename, T *out_buf)
{
    class StaticWriter: public pugi::xml_writer {
        T *out_buf;
        bool error = false;

    public:
        StaticWriter(T *out_buf) : out_buf(out_buf) {}

        bool IsValid() const { return !error; }

        void Append(Span<const char> str)
        {
            error |= (str.len > out_buf->Available());
            if (error) [[unlikely]]
                return;

            out_buf->Append(str);
        }

        void write(const void *buf, size_t len) override
        {
            for (Size i = 0; i < (Size)len; i++) {
                int c = ((const uint8_t *)buf)[i];

                switch (c) {
                    case '\"': { Append("\"\""); } break;
                    case '\t':
                    case '\r': {} break;
                    case '\n': { Append("\\n\",\n\t\""); } break;

                    default: {
                        if (c < 32 || c >= 128) {
                            error |= (out_buf->Available() < 4);
                            if (error) [[unlikely]]
                                return;

                            Fmt(out_buf->TakeAvailable(), "\\x%1", FmtHex(c).Pad0(-2));
                            out_buf->len += 4;
                        } else {
                            char ch = (char)c;
                            Append(ch);
                        }
                    } break;
                }
            }
        }
    };

    StaticWriter writer(out_buf);
    writer.Append("#include <winuser.h>\n\n");
    if (icon_filename) {
        writer.Append("1 ICON \"");
        writer.Append(icon_filename);
        writer.Append("\"\n");
    }
    writer.Append("1 24 {\n\t\"");
    doc->save(writer);
    writer.Append("\"\n}\n");

    return writer.IsValid();
}

static bool UpdateResourceFile(const char *target_name, const char *icon_filename, bool fake, const char *dest_filename)
{
    static const char *const manifest = R"(
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly manifestVersion="1.0" xmlns="urn:schemas-microsoft-com:asm.v1" xmlns:asmv3="urn:schemas-microsoft-com:asm.v3">
    <assemblyIdentity type="win32" name="" version="1.0.0.0"/>
    <application>
        <windowsSettings>
            <activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>
            <longPathAware xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">true</longPathAware>
            <heapType xmlns="http://schemas.microsoft.com/SMI/2020/WindowsSettings">SegmentHeap</heapType>
        </windowsSettings>
    </application>
    <asmv3:application>
        <asmv3:windowsSettings>
            <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true</dpiAware>
            <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>
        </asmv3:windowsSettings>
    </asmv3:application>
    <dependency>
        <dependentAssembly>
            <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0"
                              processorArchitecture="*" publicKeyToken="6595b64144ccf1df" language="*"/>
        </dependentAssembly>
    </dependency>
</assembly>
)";

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(manifest);
    RG_ASSERT(result);

    pugi::xml_node identity = doc.select_node("/assembly/assemblyIdentity").node();
    identity.attribute("name").set_value(target_name);

    LocalArray<char, 2048> res;
    if (!AssembleResourceFile(&doc, icon_filename, &res))
        return false;

    bool new_manifest;
    if (TestFile(dest_filename, FileType::File)) {
        char old_res[2048] = {};
        {
            StreamReader reader(dest_filename);
            reader.Read(RG_SIZE(old_res) - 1, old_res);
        }

        new_manifest = !TestStr(old_res, res);
    } else {
        new_manifest = true;
    }

    if (!fake && new_manifest) {
        return WriteFile(res, dest_filename);
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

    const char *platform = SplitStrReverse(HostPlatformNames[(int)build.compiler->platform], '/').ptr;
    const char *architecture = HostArchitectureNames[(int)build.compiler->architecture];

    cache_directory = Fmt(&str_alloc, "%1%/%2_%3@%4", build.output_directory, build.compiler->name, platform, architecture).ptr;
    shared_directory = Fmt(&str_alloc, "%1%/Shared", build.output_directory).ptr;
    cache_filename = Fmt(&str_alloc, "%1%/FelixCache.txt", shared_directory).ptr;
    compile_filename = Fmt(&str_alloc, "%1%/compile_commands.json", build.output_directory).ptr;

    LoadCache();
}

static const char *GetLastDirectoryAndName(const char *filename)
{
    Span<const char> remain = filename;
    const char *name;

    SplitStrReverseAny(remain, RG_PATH_SEPARATORS, &remain);
    name = SplitStrReverseAny(remain, RG_PATH_SEPARATORS, &remain).ptr;

    return name;
}

// Beware, failures can leave the Builder in a undefined state
bool Builder::AddTarget(const TargetInfo &target, const char *version_str)
{
    HeapArray<const char *> obj_filenames;
    HeapArray<const char *> embed_filenames;
    HeapArray<const char *> link_libraries;
    HeapArray<const char *> predep_filenames;
    HeapArray<const char *> qrc_filenames;

    // Should we link this target?
    bool link = false;
    switch (target.type) {
        case TargetType::Executable: { link = true; } break;
        case TargetType::Library: {
            uint32_t features = target.CombineFeatures(build.features);
            link = (features & (int)CompileFeature::LinkLibrary);
        } break;
    }

    // Core platform source files (e.g. Teensy core)
    TargetInfo *core = nullptr;
    {
        HeapArray<const char *> core_filenames;
        HeapArray<const char *> core_definitions;
        const char *name = nullptr;

        if (!build.compiler->GetCore(target.definitions, &str_alloc,
                                     &name, &core_filenames, &core_definitions))
            return false;

        if (name) {
            core = core_targets_map.FindValue(name, nullptr);

            if (!core) {
                core = core_targets.AppendDefault();

                core->name = name;
                core->type = TargetType::Library;
                core->platforms = 1u << (int)build.compiler->platform;
                std::swap(core->definitions, core_definitions);
                core->disable_features = (int)CompileFeature::Warnings;

                for (const char *core_filename: core_filenames) {
                    SourceFileInfo src = {};

                    src.target = core;
                    src.filename = core_filename;
                    DetermineSourceType(core_filename, &src.type);

                    const SourceFileInfo *ptr = core_sources.Append(src);
                    core->sources.Append(ptr);
                }

                core_targets_map.Set(name, core);
            }
        }
    }

    if (core) {
        RG_DEFER_C(prev_ns = current_ns,
                   prev_directory = cache_directory) {
            current_ns = prev_ns;
            cache_directory = prev_directory;
        };

        cache_directory = Fmt(&str_alloc, "%1%/%2", cache_directory, core->name).ptr;
        current_ns = core->name;

        for (const SourceFileInfo *src: core->sources) {
            RG_ASSERT(src->type == SourceType::C || src->type == SourceType::Cxx);

            if (!AddCppSource(*src, &obj_filenames))
                return false;
        }
    }

    Size prev_obj_filenames = obj_filenames.len;

    // Start with pregeneration steps (such as UI to header file)
    for (const SourceFileInfo *src: target.sources) {
        if (src->type == SourceType::QtUi) {
            const char *header_filename = AddQtUiSource(*src);
            predep_filenames.Append(header_filename);
        }
    }

    // Deal with user source files
    for (const SourceFileInfo *src: target.sources) {
        switch (src->type) {
            case SourceType::C:
            case SourceType::Cxx: {
                if (!AddCppSource(*src, &obj_filenames))
                    return false;
            } break;

            case SourceType::Object: { obj_filenames.Append(src->filename); } break;

            case SourceType::Esbuild: {
                const char *meta_filename = AddEsbuildSource(*src);
                if (!meta_filename)
                    return false;

                meta_filename = Fmt(&str_alloc, "@%1", meta_filename).ptr;
                embed_filenames.Append(meta_filename);
            } break;

            case SourceType::QtUi: { /* Already handled */ } break;

            case SourceType::QtResources: {
                qrc_filenames.Append(src->filename);
            } break;
        }
    }

    // Make sure C/C++ source files must depend on generated headers
    for (Size i = prev_obj_filenames; i < obj_filenames.len; i++) {
        const char *obj_filename = obj_filenames[i];
        Size node_idx = nodes_map.FindValue(obj_filename, -1);

        if (node_idx >= 0) {
            Node *node = &nodes[node_idx];

            for (const char *predep_filename: predep_filenames) {
                Size src_idx = nodes_map.FindValue(predep_filename, -1);

                if (src_idx >= 0) {
                    Node *src = &nodes[src_idx];

                    src->triggers.Append(node_idx);
                    node->semaphore++;
                }
            }
        }
    }

    // Build Qt resource file
    if (qrc_filenames.len) {
        const char *obj_filename = AddQtResource(target, qrc_filenames);
        obj_filenames.Append(obj_filename);
    }

    // User assets and libraries
    embed_filenames.Append(target.embed_filenames);
    link_libraries.Append(target.libraries);

    // Assets
    if (embed_filenames.len) {
        const char *src_filename = Fmt(&str_alloc, "%1%/Misc%/%2_embed.c", cache_directory, target.name).ptr;
        const char *obj_filename = Fmt(&str_alloc, "%1%2", src_filename, build.compiler->GetObjectExtension()).ptr;

        uint32_t features = target.CombineFeatures(build.features);
        bool module = (features & (int)CompileFeature::HotAssets);

        // Make C file
        {
            Command cmd = InitCommand();
            build.compiler->MakeEmbedCommand(embed_filenames, target.embed_options, src_filename, &str_alloc, &cmd);

            const char *text = Fmt(&str_alloc, StdErr->IsVt100(), "Embed %!..+%1%!0 assets", target.name).ptr;
            AppendNode(text, src_filename, cmd, embed_filenames);
        }

        // Build object file
        {
            Command cmd = InitCommand();

            const char *flags = GatherFlags(target, SourceType::C);

            if (module) {
                build.compiler->MakeObjectCommand(src_filename, SourceType::C,
                                                  nullptr, {"EXPORT"}, {}, {}, {}, flags, features,
                                                  obj_filename, &str_alloc, &cmd);
            } else {
                build.compiler->MakeObjectCommand(src_filename, SourceType::C,
                                                  nullptr, {}, {}, {}, {}, flags, features,
                                                  obj_filename,  &str_alloc, &cmd);
            }

            const char *text = Fmt(&str_alloc, StdErr->IsVt100(), "Compile %!..+%1%!0 assets", target.name).ptr;
            AppendNode(text, obj_filename, cmd, src_filename);
        }

        // Build module if needed
        if (module) {
            const char *module_filename = Fmt(&str_alloc, "%1%/%2_assets%3", build.output_directory,
                                              target.name, RG_SHARED_LIBRARY_EXTENSION).ptr;
            const char *flags = GatherFlags(target, SourceType::Object);

            Command cmd = InitCommand();
            build.compiler->MakeLinkCommand(obj_filename, {}, TargetType::Library,
                                            flags, features, module_filename, &str_alloc, &cmd);

            const char *text = Fmt(&str_alloc, StdErr->IsVt100(), "Link %!..+%1%!0", GetLastDirectoryAndName(module_filename)).ptr;
            AppendNode(text, module_filename, cmd, obj_filename);
        } else {
            obj_filenames.Append(obj_filename);
        }
    }

    // Some compilers (such as MSVC) also build PCH object files that need to be linked
    if (build.features & (int)CompileFeature::PCH) {
        for (const char *filename: target.pchs) {
            const char *pch_filename = build_map.FindValue({ current_ns, filename }, nullptr);

            if (pch_filename) {
                const char *obj_filename = build.compiler->GetPchObject(pch_filename, &str_alloc);

                if (obj_filename) {
                    obj_filenames.Append(obj_filename);
                }
            }
        }
    }

    // Version string
    if (target.type == TargetType::Executable) {
        const char *src_filename = Fmt(&str_alloc, "%1%/Misc%/%2_version.c", cache_directory, target.name).ptr;
        const char *obj_filename = Fmt(&str_alloc, "%1%2", src_filename, build.compiler->GetObjectExtension()).ptr;

        uint32_t features = target.CombineFeatures(build.features);
        const char *flags = GatherFlags(target, SourceType::C);

        if (!UpdateVersionSource(target.name, version_str, src_filename))
            return false;

        Command cmd = InitCommand();
        build.compiler->MakeObjectCommand(src_filename, SourceType::C,
                                          nullptr, {}, {}, {}, {}, flags, features,
                                          obj_filename, &str_alloc, &cmd);

        const char *text = Fmt(&str_alloc, StdErr->IsVt100(), "Compile %!..+%1%!0 version file", target.name).ptr;
        AppendNode(text, obj_filename, cmd, src_filename);

        obj_filenames.Append(obj_filename);
    }

    // Resource file (Windows only)
    if (build.compiler->platform == HostPlatform::Windows &&
            target.type == TargetType::Executable) {
        const char *rc_filename = Fmt(&str_alloc, "%1%/Misc%/%2_res.rc", cache_directory, target.name).ptr;
        const char *res_filename = Fmt(&str_alloc, "%1%/Misc%/%2_res.res", cache_directory, target.name).ptr;

        if (!UpdateResourceFile(target.name, target.icon_filename, build.fake, rc_filename))
            return false;

        Command cmd = InitCommand();
        build.compiler->MakeResourceCommand(rc_filename, res_filename, &str_alloc, &cmd);

        const char *text = Fmt(&str_alloc, StdErr->IsVt100(), "Build %!..+%1%!0 resource file", target.name).ptr;

        if (target.icon_filename) {
            AppendNode(text, res_filename, cmd, { rc_filename, target.icon_filename });
        } else {
            AppendNode(text, res_filename, cmd, rc_filename);
        }

        obj_filenames.Append(res_filename);
    }

    // Link with required Qt libraries
    if (target.qt_components.len && !AddQtLibraries(target, &obj_filenames, &link_libraries))
        return false;

    // Link commands
    if (link) {
        const char *link_ext = build.compiler->GetLinkExtension(target.type);
        const char *post_ext = build.compiler->GetPostExtension(target.type);

        // Generate linked output
        const char *link_filename;
        {
            link_filename = Fmt(&str_alloc, "%1%/%2%3", build.output_directory, target.title, link_ext).ptr;

            uint32_t features = target.CombineFeatures(build.features);
            const char *flags = GatherFlags(target, SourceType::Object);

            Command cmd = InitCommand();
            build.compiler->MakeLinkCommand(obj_filenames, link_libraries, target.type,
                                            flags, features, link_filename, &str_alloc, &cmd);

            const char *text = Fmt(&str_alloc, StdErr->IsVt100(), "Link %!..+%1%!0", GetLastDirectoryAndName(link_filename)).ptr;
            AppendNode(text, link_filename, cmd, obj_filenames);
        }

        const char *target_filename;
        if (post_ext) {
            target_filename = Fmt(&str_alloc, "%1%/%2%3", build.output_directory, target.title, post_ext).ptr;

            Command cmd = InitCommand();
            build.compiler->MakePostCommand(link_filename, target_filename, &str_alloc, &cmd);

            const char *text = Fmt(&str_alloc, StdErr->IsVt100(), "Convert %!..+%1%!0", GetLastDirectoryAndName(target_filename)).ptr;
            AppendNode(text, target_filename, cmd, link_filename);
        } else {
            target_filename = link_filename;
        }

#if defined(__APPLE__)
        // Bundle macOS GUI apps
        if (build.compiler->platform == HostPlatform::macOS && target.qt_components.len) {
            const char *bundle_filename = Fmt(&str_alloc, "%1.app", target_filename).ptr;

            Command cmd = InitCommand();

            {
                HeapArray<char> buf(&str_alloc);

                Fmt(&buf, "\"%1\" macify -f \"%2\" -O \"%3\"", GetApplicationExecutable(), target_filename, bundle_filename);
                if (target.icon_filename) {
                    Fmt(&buf, " --icon \"%1\"", target.icon_filename);
                }
                Fmt(&buf, " --title \"%1\"", target.title);

                cmd.cache_len = buf.len;
                cmd.cmd_line = buf.TrimAndLeak(1);
            }

            // Help command find qmake
            cmd.env_variables.Append({ "QMAKE_PATH", qt->qmake });

            const char *text = Fmt(&str_alloc, StdErr->IsVt100(), "Bundle %!..+%1%!0", GetLastDirectoryAndName(bundle_filename)).ptr;
            AppendNode(text, bundle_filename, cmd, target_filename);

            target_filename = bundle_filename;
        }
#endif

        target_filenames.Set(target.name, target_filename);
    }

    return true;
}

bool Builder::AddSource(const SourceFileInfo &src)
{
    switch (src.type) {
        case SourceType::C:
        case SourceType::Cxx: return AddCppSource(src, nullptr);
        case SourceType::Esbuild: return AddEsbuildSource(src);
        case SourceType::QtUi: return AddQtUiSource(src);

        case SourceType::Object: {
            LogWarning("Object file does not need to be built");
            return false;
        } break;
        case SourceType::QtResources: {
            LogError("You cannot build QRC files directly");
            return false;
        }
    }

    RG_UNREACHABLE();
}

bool Builder::AddCppSource(const SourceFileInfo &src, HeapArray<const char *> *out_objects)
{
    RG_ASSERT(src.type == SourceType::C || src.type == SourceType::Cxx);

    // Precompiled header (if any)
    const char *pch_filename = nullptr;
    if (build.features & (int)CompileFeature::PCH) {
        const SourceFileInfo *pch = nullptr;
        const char *pch_ext = nullptr;
        switch (src.type) {
            case SourceType::C: {
                pch = src.target->c_pch_src;
                pch_ext = ".c";
            } break;
            case SourceType::Cxx: {
                pch = src.target->cxx_pch_src;
                pch_ext = ".cc";
            } break;

            case SourceType::Object:
            case SourceType::Esbuild:
            case SourceType::QtUi:
            case SourceType::QtResources: { RG_UNREACHABLE(); } break;
        }

        if (pch) {
            pch_filename = build_map.FindValue({ current_ns, pch->filename }, nullptr);

            if (!pch_filename) {
                pch_filename = BuildObjectPath(pch->filename, cache_directory, "", pch_ext);

                const char *cache_filename = build.compiler->GetPchCache(pch_filename, &str_alloc);

                uint32_t features = pch->CombineFeatures(build.features);
                const char *flags = GatherFlags(*src.target, src.type);

                Command cmd = InitCommand();
                build.compiler->MakePchCommand(pch_filename, pch->type,
                                               pch->target->definitions, pch->target->include_directories,
                                               pch->target->include_files, flags, features, &str_alloc, &cmd);

                // Check the PCH cache file against main file dependencies
                if (!IsFileUpToDate(cache_filename, pch_filename)) {
                    mtime_map.Set(pch_filename, -1);
                } else {
                    const CacheEntry *entry = cache_map.Find(pch_filename);

                    if (!entry) {
                        mtime_map.Set(pch_filename, -1);
                    } else {
                        Span<const DependencyEntry> dependencies = MakeSpan(cache_dependencies.ptr + entry->deps_offset, entry->deps_len);

                        if (!IsFileUpToDate(cache_filename, dependencies)) {
                            mtime_map.Set(pch_filename, -1);
                        }
                    }
                }

                const char *text = Fmt(&str_alloc, StdErr->IsVt100(), "Precompile %!..+%1%!0", pch->filename).ptr;
                bool append = AppendNode(text, pch_filename, cmd, pch->filename);

                if (append) {
                    if (!build.fake && !CreatePrecompileHeader(pch->filename, pch_filename))
                        return false;
                }
            }
        }
    }

    const char *obj_filename = build_map.FindValue({ current_ns, src.filename }, nullptr);

    // Build main object
    if (!obj_filename) {
        obj_filename = BuildObjectPath(src.filename, cache_directory, "", build.compiler->GetObjectExtension());

        uint32_t features = src.CombineFeatures(build.features);
        const char *flags = GatherFlags(*src.target, src.type);

        HeapArray<const char *> system_directories;
        if (src.target->qt_components.len && !AddQtDirectories(src, &system_directories))
            return false;

        Command cmd = InitCommand();
        build.compiler->MakeObjectCommand(src.filename, src.type,
                                          pch_filename, src.target->definitions,
                                          src.target->include_directories, system_directories,
                                          src.target->include_files, flags, features,
                                          obj_filename, &str_alloc, &cmd);

        const char *text = Fmt(&str_alloc, StdErr->IsVt100(), "Compile %!..+%1%!0", src.filename).ptr;
        bool append = pch_filename ? AppendNode(text, obj_filename, cmd, { src.filename, pch_filename })
                                   : AppendNode(text, obj_filename, cmd, src.filename);

       if (append && !build.fake && !EnsureDirectoryExists(obj_filename))
            return false;

        if (src.target->qt_components.len && !CompileMocHelper(src, system_directories, features))
            return false;
    }

    if (out_objects) {
        out_objects->Append(obj_filename);

        if (src.target->qt_components.len) {
            const char *moc_obj = moc_map.FindValue(src.filename, nullptr);

            if (moc_obj) {
                out_objects->Append(moc_obj);
            }
        }
    }

    return true;
}

bool Builder::UpdateVersionSource(const char *target, const char *version, const char *dest_filename)
{
    if (!build.fake && !EnsureDirectoryExists(dest_filename))
        return false;

    char code[1024];
    Fmt(code, "// This file is auto-generated by felix\n\n"
              "const char *FelixTarget = \"%1\";\n"
              "const char *FelixVersion = \"%2\";\n"
              "const char *FelixCompiler = \"%3 (%4)\";\n",
        target, version ? version : "unknown",
        build.compiler->title, FmtFlags(build.features, CompileFeatureOptions));

    bool new_version;
    if (TestFile(dest_filename, FileType::File)) {
        char old_code[1024] = {};
        ReadFile(dest_filename, MakeSpan(old_code, RG_SIZE(old_code) - 1));

        new_version = !TestStr(old_code, code);
    } else {
        new_version = true;
    }

    if (!build.fake && new_version) {
        return WriteFile(code, dest_filename);
    } else {
        return true;
    }
}

bool Builder::Build(int jobs, bool verbose)
{
    RG_ASSERT(jobs > 0);

    // Reset build context
    clear_filenames.Clear();
    rsp_map.Clear();
    progress = total - nodes.len;
    workers.Clear();
    workers.AppendDefault(jobs);

    RG_DEFER {
        // Update cache even if some tasks fail
        if (nodes.len && !build.fake) {
            for (const WorkerState &worker: workers) {
                for (const CacheEntry &entry: worker.entries) {
                    cache_map.Set(entry)->deps_offset = cache_dependencies.len;

                    for (Size i = 0; i < entry.deps_len; i++) {
                        DependencyEntry dep = {};

                        dep.filename = DuplicateString(worker.dependencies[entry.deps_offset + i], &str_alloc).ptr;

                        FileInfo file_info;
                        if (StatFile(dep.filename, (int)StatFlag::SilentMissing, &file_info) == StatResult::Success) {
                            dep.mtime = file_info.mtime;
                        } else {
                            dep.mtime = -1;
                        }

                        cache_dependencies.Append(dep);
                    }
                }
            }
            workers.Clear();

            SaveCache();
        }

        // Update compilation database
        if (!build.fake) {
            SaveCompile();
        }

        // Clean up failed and temporary files
        // Windows has a tendency to hold file locks a bit longer than needed...
        // Try to delete files several times silently unless it's the last try.
#if defined(_WIN32)
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

                WaitDelay(150);
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
                const char *rsp_filename = Fmt(&str_alloc, "%1%/Misc%/%2.rsp", cache_directory, target_basename).ptr;

                if (!EnsureDirectoryExists(rsp_filename))
                    return false;

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

    LogInfo("Building with %1 %2...", jobs, jobs > 1 ? "threads" : "thread");
    int64_t now = GetMonotonicTime();

    Async async(jobs);

    // Run nodes
    bool busy = false;
    for (Size i = 0; i < nodes.len; i++) {
        Node *node = &nodes[i];

        if (!node->success && !node->semaphore) {
            async.Run([=, &async, this]() { return RunNode(&async, node, verbose); });
            busy = true;
        }
    }

    if (async.Sync()) {
        if (busy) {
            if (!build.fake) {
                double time = (double)(GetMonotonicTime() - now) / 1000.0;
                LogInfo("Done (%1s)", FmtDouble(time, 1));
            } else {
                LogInfo("Done %!D..[dry run]%!0");
            }
        } else {
            LogInfo("Nothing to do!%!D..%1%!0", build.fake ? " [dry run]" : "");
        }

        return true;
    } else if (WaitForInterrupt(0) == WaitForResult::Interrupt) {
        LogError("Build was interrupted");
        return false;
    } else {
        if (!build.stop_after_error) {
            LogError("Some errors have occured (use %!..+felix -s%!0 to stop after first error)");
        }
        return false;
    }
}

Command Builder::InitCommand()
{
    Command cmd = {};
    cmd.env_variables.allocator = &str_alloc;
    return cmd;
}

void Builder::SaveCache()
{
    if (!EnsureDirectoryExists(cache_filename))
        return;

    StreamWriter st(cache_filename, (int)StreamWriterFlag::Atomic);
    if (!st.IsValid())
        return;

    for (const CacheEntry &entry: cache_map) {
        PrintLn(&st, "%1>%2", entry.deps_len, entry.filename);
        PrintLn(&st, "%1", entry.cmd_line);
        for (Size i = 0; i < entry.deps_len; i++) {
            const DependencyEntry &dep = cache_dependencies[entry.deps_offset + i];
            PrintLn(&st, "%1|%2", dep.mtime, dep.filename);
        }
    }

    st.Close();
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
    {
        StreamReader st(cache_filename);
        if (!st.ReadAll(Megabytes(128), &cache))
            return;
        cache.len = TrimStrRight(cache.Take(), "\n").len;
        cache.Grow(1);
    }

    // Parse cache file
    {
        Span<char> remain = cache;

        while (remain.len) {
            CacheEntry entry = {};

            // Filename and dependency count
            {
                Span<char> part = SplitStr(remain, '>', &remain);
                if (!ParseInt(part, &entry.deps_len))
                    return;
                entry.deps_offset = cache_dependencies.len;

                part = SplitStr(remain, '\n', &remain);
                part.ptr[part.len] = 0;
                entry.filename = part.ptr;
            }

            // Command line
            {
                Span<char> part = SplitStr(remain, '\n', &remain);
                part.ptr[part.len] = 0;
                entry.cmd_line = part;
            }

            // Dependencies
            for (Size i = 0; i < entry.deps_len; i++) {
                DependencyEntry dep = {};

                Span<char> part = SplitStr(remain, '|', &remain);
                if (!ParseInt(part, &dep.mtime))
                    return;

                part = SplitStr(remain, '\n', &remain);
                part.ptr[part.len] = 0;
                dep.filename = part.ptr;

                cache_dependencies.Append(dep);
            }
            entry.deps_len = cache_dependencies.len - entry.deps_offset;

            cache_map.Set(entry);
        }
    }

    cache.Leak();
    clear_guard.Disable();
}

void Builder::SaveCompile()
{
    StreamWriter st(compile_filename, (int)StreamWriterFlag::Atomic);
    if (!st.IsValid())
        return;
    json_PrettyWriter json(&st);

    json.StartArray();

    for (const CacheEntry &entry: cache_map) {
        if (!entry.deps_len)
            continue;

        const char *directory = GetWorkingDirectory();
        const DependencyEntry &dep0 = cache_dependencies[entry.deps_offset];

        json.StartObject();

        json.Key("directory"); json.String(directory);
        json.Key("command"); json.String(entry.cmd_line.ptr, entry.cmd_line.len);
        json.Key("file"); json.String(dep0.filename);
        json.Key("output"); json.String(entry.filename);

        json.EndObject();
    }

    json.EndArray();

    st.Close();
}

const char *Builder::BuildObjectPath(Span<const char> src_filename, const char *output_directory,
                                     const char *prefix, const char *suffix)
{
    if (PathIsAbsolute(src_filename)) {
        SplitStrAny(src_filename, RG_PATH_SEPARATORS, &src_filename);
    }

    HeapArray<char> buf(&str_alloc);

    Span<const char> src_directory;
    Span<const char> src_name = SplitStrReverseAny(src_filename, RG_PATH_SEPARATORS, &src_directory);

    Size offset = Fmt(&buf, "%1%/Objects%/", output_directory).len;

    if (src_directory.len) {
        Fmt(&buf, "%1%/%2%3%4", src_directory, prefix, src_name, suffix);
    } else {
        Fmt(&buf, "%1%2%3", prefix, src_name, suffix);
    }

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

static void AppendFlags(const char *flags, HeapArray<char> *out_buf)
{
    if (!flags || !flags[0])
        return;

    if (out_buf->len) {
        out_buf->Append(' ');
    }
    out_buf->Append(flags);
}

const char *Builder::GatherFlags(const TargetInfo &target, SourceType type)
{
    RG_ASSERT((uintptr_t)&target % 8 == 0);
    RG_ASSERT((int)type < 8);

    const void *key = (const void *)((uint8_t *)&target + (int)type);

    bool inserted;
    const char **ptr = custom_flags.TrySet(key, nullptr, &inserted);

    if (inserted) {
        HeapArray<char> buf(&str_alloc);

        switch (type) {
            case SourceType::C: {
                if (build.env) {
                    AppendFlags(GetEnv("CFLAGS"), &buf);
                    AppendFlags(GetEnv("CPPFLAGS"), &buf);
                }
            } break;

            case SourceType::Cxx: {
                if (build.env) {
                    AppendFlags(GetEnv("CXXFLAGS"), &buf);
                    AppendFlags(GetEnv("CPPFLAGS"), &buf);
                }
            } break;

            // Not an object, we use this enum type as a hack for link flags
            case SourceType::Object: {
                if (build.env) {
                    AppendFlags(GetEnv("LDFLAGS"), &buf);
                }
            } break;

            case SourceType::Esbuild:
            case SourceType::QtUi:
            case SourceType::QtResources: {} break;
        }

        if (buf.len) {
            buf.Append(0);
            *ptr = buf.TrimAndLeak().ptr;
        }
    }

    return *ptr;
}

static inline const char *CleanFileName(const char *str)
{
    return str + (str[0] == '@');
}

bool Builder::AppendNode(const char *text, const char *dest_filename, const Command &cmd,
                         Span<const char *const> src_filenames)
{
    RG_ASSERT(src_filenames.len >= 1);

    build_map.Set({ current_ns, CleanFileName(src_filenames[0]) }, dest_filename);
    total++;

    if (NeedsRebuild(dest_filename, cmd, src_filenames)) {
        Size node_idx = nodes.len;
        Node *node = nodes.AppendDefault();

        node->text = text;
        node->dest_filename = dest_filename;
        node->cmd = cmd;

        // Add triggers to source file nodes
        for (const char *src_filename: src_filenames) {
            src_filename = CleanFileName(src_filename);

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

    Span<const DependencyEntry> dependencies = MakeSpan(cache_dependencies.ptr + entry->deps_offset, entry->deps_len);

    if (!IsFileUpToDate(dest_filename, dependencies))
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
        src_filename = CleanFileName(src_filename);

        int64_t src_time = GetFileModificationTime(src_filename);
        if (src_time < 0 || src_time > dest_time)
            return false;
    }

    return true;
}

bool Builder::IsFileUpToDate(const char *dest_filename, Span<const DependencyEntry> dependencies)
{
    if (build.rebuild)
        return false;

    int64_t dest_time = GetFileModificationTime(dest_filename);
    if (dest_time < 0)
        return false;

    for (const DependencyEntry &dep: dependencies) {
        int64_t dep_time = GetFileModificationTime(dep.filename);

        if (dep_time < 0 || dep_time > dest_time)
            return false;
        if (dep_time != dep.mtime)
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
        // filename might be temporary (e.g. dependency filenames in NeedsRebuild())
        filename = DuplicateString(filename, &str_alloc).ptr;

        FileInfo file_info;
        if (StatFile(filename, (int)StatFlag::SilentMissing, &file_info) != StatResult::Success) {
            mtime_map.Set(filename, -1);
            return -1;
        }

        mtime_map.Set(filename, file_info.mtime);
        return file_info.mtime;
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

            if (strchr(" $#:", c)) {
                if (remain[i - 1] == '\\') {
                    (*out_frag)[out_frag->len - 1] = c;
#if defined(_WIN32)
                } else if (c == ':' && i == 1) {
                    // Absolute Windows paths start with [A-Z]:
                    // Some MinGW builds escape the colon, some don't. Tolerate both cases.
                    out_frag->Append(c);
#endif
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
    RG_ASSERT(alloc);

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

        if (TestStr(frag, ":"))
            break;
    }

    // Get dependency filenames
    while (remain.len) {
        frag.RemoveFrom(0);
        remain = ParseMakeFragment(remain, &frag);

        if (frag.len && !TestStr(frag, "\\")) {
            const char *dep_filename = NormalizePath(frag, alloc).ptr;
            out_filenames->Append(dep_filename);
        }
    }

    return true;
}

static Size ExtractShowIncludes(Span<char> buf, Allocator *alloc, HeapArray<const char *> *out_filenames)
{
    RG_ASSERT(alloc || !out_filenames);

    // We need to strip include notes from the output
    Span<char> new_buf = MakeSpan(buf.ptr, 0);

    while (buf.len) {
        Span<const char> line = SplitStr(buf, '\n', &buf);

        // MS had the brilliant idea to localize inclusion notes.. In english it starts
        // with 'Note: including file: ' but it can basically be anything. We match
        // lines that start with a non-space character, with two pairs of ': ' not
        // preceeded by any digit. Meh.
        Span<const char> dep = {};
        if (line.len && !IsAsciiWhite(line[0])) {
            int counter = 0;

            for (Size i = 0; i < line.len - 2; i++) {
                if (IsAsciiDigit(line[i]))
                    break;

                counter += (line[i] == ':' && line[i + 1] == ' ');

                if (counter == 2) {
                    dep = TrimStr(line.Take(i + 2, line.len - i - 2));
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
            MemMove(new_buf.end(), line.ptr, copy_len);

            new_buf.len += copy_len;
        }
    }

    return new_buf.len;
}

static bool ParseEsbuildMeta(const char *filename, Allocator *alloc, HeapArray<const char *> *out_filenames)
{
    RG_ASSERT(alloc);

    RG_DEFER_NC(err_guard, len = out_filenames->len) {
        out_filenames->RemoveFrom(len);
        UnlinkFile(filename);
    };

    StreamReader reader(filename);
    if (!reader.IsValid())
        return false;
    json_Parser parser(&reader, alloc);

    HeapArray<Span<const char>> outputs;
    Span<const char> prefix = {};

    parser.ParseObject();
    while (parser.InObject()) {
        Span<const char> key = {};
        parser.ParseKey(&key);

        if (key == "inputs") {
            parser.ParseObject();
            while (parser.InObject()) {
                Span<const char> input = {};
                parser.ParseKey(&input);

                const char *dep_filename = NormalizePath(input, alloc).ptr;
                out_filenames->Append(dep_filename);

                parser.Skip();
            }
        } else if (key == "outputs") {
            parser.ParseObject();
            while (parser.InObject()) {
                Span<const char> output = {};
                parser.ParseKey(&output);

                const char *dep_filename = NormalizePath(output, alloc).ptr;
                out_filenames->Append(dep_filename);

                // Find entry with entryPoint, which we need to fix all paths
                if (!prefix.ptr) {
                    parser.ParseObject();
                    while (parser.InObject()) {
                        Span<const char> key = {};
                        parser.ParseKey(&key);

                        if (key == "entryPoint") {
                            Span<const char> entry_point = {};
                            parser.ParseString(&entry_point);

                            prefix = output.Take(0, output.len - entry_point.len);
                        } else {
                            parser.Skip();
                        }
                    }
                } else {
                    parser.Skip();
                }

                outputs.Append(output);
            }
        } else {
            parser.Skip();
        }
    }
    if (!parser.IsValid())
        return false;
    reader.Close();

    if (!prefix.ptr) {
        LogError("Failed to find entryPiont in esbuild meta file '%1'", filename);
        return false; 
    }

    // Replace with INI file
    {
        StreamWriter writer(filename, (int)StreamWriterFlag::Atomic);

        for (Span<const char> output: outputs) {
            if (!StartsWith(output, prefix)) [[unlikely]] {
                LogWarning("Ignoring esbuild output file '%1' (prefix mismatch)", output);
                continue;
            }

            PrintLn(&writer, "[%1]", output.Take(prefix.len, output.len - prefix.len));
            PrintLn(&writer, "File = %1", output);
        }

        if (!writer.Close())
            return true;
    }

    err_guard.Disable();
    return true;
}

bool Builder::RunNode(Async *async, Node *node, bool verbose)
{
    if (build.stop_after_error && !async->IsSuccess())
        return false;
    if (WaitForInterrupt(0) == WaitForResult::Interrupt)
        return false;

    const Command &cmd = node->cmd;

    WorkerState *worker = &workers[Async::GetWorkerIdx()];
    const char *cmd_line = rsp_map.FindValue(node, cmd.cmd_line.ptr);

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
        ExecuteInfo info = {};

        info.work_dir = nullptr;
        info.env_variables = cmd.env_variables;

        started = ExecuteCommandLine(cmd_line, info, {}, Megabytes(4), &output_buf, &exit_code);
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
                        started &= ParseMakeRule(cmd.deps_filename, &worker->str_alloc, &worker->dependencies);
                        UnlinkFile(cmd.deps_filename);
                    }
                } break;
                case Command::DependencyMode::ShowIncludes: {
                    output.len = ExtractShowIncludes(output, &worker->str_alloc, &worker->dependencies);
                } break;
                case Command::DependencyMode::EsbuildMeta: {
                    if (TestFile(cmd.deps_filename)) {
                        started &= ParseEsbuildMeta(cmd.deps_filename, &worker->str_alloc, &worker->dependencies);
                    }
                } break;
            }
            entry.deps_len = worker->dependencies.len - entry.deps_offset;

            worker->entries.Append(entry);
        }

        if (output.len) {
            std::lock_guard<std::mutex> out_lock(out_mutex);
            StdOut->Write(output);
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
            output.len = ExtractShowIncludes(output, nullptr, nullptr);
        }

        cache_map.Remove(node->dest_filename);
        clear_filenames.Append(node->dest_filename);

        if (!started) {
            // Error already issued by ExecuteCommandLine()
            StdErr->Write(output);
        } else if (WaitForInterrupt(0) != WaitForResult::Interrupt) {
            LogError("%1 %!..+(exit code %2)%!0", node->text, exit_code);
            StdErr->Write(output);
        }

        return false;
    }
}

}
