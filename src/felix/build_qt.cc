// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/base/base.hh"
#include "build.hh"
#include "locate.hh"

namespace RG {

static FmtArg FormatVersion(int64_t version, int components)
{
    RG_ASSERT(components > 0);

    FmtArg arg = {};

    arg.type = FmtType::Buffer;

    Size len = 0;
    while (components--) {
        Span<char> buf = MakeSpan(arg.u.buf + len, RG_SIZE(arg.u.buf) - len);

        int divisor = 1;
        for (int i = 0; i < components; i++) {
            divisor *= 1000;
        }
        int component = (version / divisor) % 1000;

        len += Fmt(buf, "%1.", component).len;
    }
    arg.u.buf[len - 1] = 0;

    return arg;
}

bool Builder::PrepareQtSdk(int64_t min_version)
{
    if (!qt) {
        qt = std::make_unique<QtInfo>();

        if (!FindQtSdk(build.compiler, &str_alloc, qt.get())) {
            qt.reset(nullptr);
            return false;
        }
    }
    if (!qt->qmake)
        return false;

    if (qt->version < min_version) {
        LogError("Found Qt %1 but %2 is required", FormatVersion(qt->version, 3), FormatVersion(min_version, 3));
        return false;
    }

    return true;
}

const char *Builder::AddQtUiSource(const SourceFileInfo &src)
{
    RG_ASSERT(src.type == SourceType::QtUi);

    const char *header_filename = build_map.FindValue({ current_ns, src.filename }, nullptr);

    // First, we need Qt!
    if (!header_filename && !PrepareQtSdk(src.target->qt_version))
        return nullptr;

    // Run Qt UI builder
    if (!header_filename) {
        Span<const char> filename_noext;
        SplitStrReverse(src.filename, '.', &filename_noext);

        header_filename = BuildObjectPath(filename_noext, cache_directory, "ui_", ".h");

        Command cmd = {};

        // Assemble uic command
        {
            HeapArray<char> buf(&str_alloc);

            Fmt(&buf, "\"%1\" -o \"%2\" \"%3\"", qt->uic, header_filename, src.filename);

            cmd.cache_len = buf.len;
            cmd.cmd_line = buf.TrimAndLeak(1);
        }

        const char *text = Fmt(&str_alloc, "Build UI %!..+%1%!0", src.filename).ptr;
        if (AppendNode(text, header_filename, cmd, { src.filename, qt->uic })) {
            if (!build.fake && !EnsureDirectoryExists(header_filename))
                return nullptr;
        }
    }

    return header_filename;
}

const char *Builder::AddQtResource(const TargetInfo &target, Span<const char *> qrc_filenames)
{
    const char *cpp_filename = build_map.FindValue({ current_ns, qrc_filenames[0] }, nullptr);

    // First, we need Qt!
    if (!cpp_filename && !PrepareQtSdk(target.qt_version))
        return nullptr;

    if (!cpp_filename) {
        cpp_filename = Fmt(&str_alloc, "%1%/Misc%/%2_qrc.cc", cache_directory, target.name).ptr;

        Command cmd = {};

        // Prepare QRC build command
        {
            HeapArray<char> buf(&str_alloc);

            Fmt(&buf, "\"%1\" -o \"%2\"", qt->rcc, cpp_filename);
            for (const char *qrc_filename: qrc_filenames) {
                Fmt(&buf, " \"%1\"", qrc_filename);
            }

            cmd.cache_len = buf.len;
            cmd.cmd_line = buf.TrimAndLeak(1);
        }

        const char *text = Fmt(&str_alloc, "Assemble %!..+%1%!0 resource file", target.name).ptr;
        AppendNode(text, cpp_filename, cmd, qrc_filenames);
    }

    const char *obj_filename = build_map.FindValue({ current_ns, cpp_filename }, nullptr);

    if (!obj_filename) {
        obj_filename = Fmt(&str_alloc, "%1%2", cpp_filename, build.compiler->GetObjectExtension()).ptr;

        uint32_t features = target.CombineFeatures(build.features);

        Command cmd = {};
        build.compiler->MakeObjectCommand(cpp_filename, SourceType::Cxx,
                                          nullptr, {}, {}, {}, {}, features, build.env,
                                          obj_filename, &str_alloc, &cmd);

        const char *text = Fmt(&str_alloc, "Compile %!..+%1%!0 QRC resources", target.name).ptr;
        AppendNode(text, obj_filename, cmd, cpp_filename);
    }

    return obj_filename;
}

bool Builder::AddQtDirectories(const SourceFileInfo &src, HeapArray<const char *> *out_list)
{
    if (!PrepareQtSdk(src.target->qt_version))
        return false;

    const char *src_directory = DuplicateString(GetPathDirectory(src.filename), &str_alloc).ptr;
    [[maybe_unused]] const char *misc_includes = nullptr;

    out_list->Append(src_directory);
    out_list->Append(qt->headers);

    for (const char *component: src.target->qt_components) {
#ifdef _WIN32
        // Probably never gonna be possible...
        RG_ASSERT(build.compiler->platform != HostPlatform::macOS);
#else
        if (build.compiler->platform == HostPlatform::macOS) {
            if (!misc_includes) {
                misc_includes = Fmt(&str_alloc, "%1%/Misc/%2", cache_directory, src.target->name).ptr;
                out_list->Append(misc_includes);
            }

            const char *dirname = Fmt(&str_alloc, "%1%/Qt%2.framework/Versions/Current/Headers", qt->libraries, component).ptr;

            if (TestFile(dirname, FileType::Directory)) {
                const char *linkname = Fmt(&str_alloc, "%1%/Qt%2", misc_includes, component).ptr;

                if (!build.fake && !TestFile(linkname, FileType::Link)) {
                    if (!MakeDirectoryRec(misc_includes))
                        return false;
                    if (symlink(dirname, linkname) < 0) {
                        LogError("Failed to create symbolic link '%1': %2", linkname, strerror(errno));
                        return false;
                    }
                }

                out_list->Append(dirname);
                continue;
            }
        }
#endif

        const char *dirname = Fmt(&str_alloc, "%1%/Qt%2", qt->headers, component).ptr;
        out_list->Append(dirname);
    }

    return true;
}

bool Builder::AddQtLibraries(const TargetInfo &target, HeapArray<const char *> *obj_filenames,
                                                       HeapArray<const char *> *link_libraries)
{
    if (!PrepareQtSdk(target.qt_version))
        return false;

    // Use marker to sort and deduplicate our libraries after
    Size prev_len = link_libraries->len;

    if (qt->shared) {
        for (const char *component: target.qt_components) {
            if (build.compiler->platform == HostPlatform::macOS) {
                Span<char> framework = Fmt(&str_alloc, "!%1%/Qt%2.framework", qt->libraries, component);
                const char *prl_filename = Fmt(&str_alloc, "%1%/Resources/Qt%2.prl", framework.ptr + 1, component).ptr;

                if (TestFile(framework.ptr + 1)) {
                    // Mask .framework extension
                    framework.ptr[framework.len - 10] = 0;

                    link_libraries->Append(framework.ptr);

                    if (TestFile(prl_filename)) {
                        ParsePrlFile(prl_filename, link_libraries);
                    }

                    continue;
                }
            }

            if (build.compiler->platform == HostPlatform::Windows) {
                const char *library = Fmt(&str_alloc, "%1%/%2Qt%3%4%5",
                                          qt->libraries, lib_prefix, qt->version_major, component, import_extension).ptr;
                obj_filenames->Append(library);
            } else {
                const char *library = Fmt(&str_alloc, "%1%/%2Qt%3%4%5.%6",
                                          qt->libraries, lib_prefix, qt->version_major, component, import_extension, qt->version_major).ptr;
                obj_filenames->Append(library);
            }
        }

        // Fix quirk: QtGui depends on QtDBus but it's not listed correctly
        // and macdeployqt does not handle it.
        for (Size i = prev_len; i < link_libraries->len; i++) {
            const char *library = (*link_libraries)[i];

            if (library[0] == '!') {
                Span<const char> name = SplitStrReverseAny(library + 1, RG_PATH_SEPARATORS);

                if (name == "QtGui") {
                    link_libraries->Append("!QtDBus");
                    break;
                }
            }
        }
    } else {
        const char *obj_filename = CompileStaticQtHelper(target);
        obj_filenames->Append(obj_filename);

        bool link_plugins =
            std::find_if(target.qt_components.begin(), target.qt_components.end(),
                         [](const char *component) { return !TestStr(component, "Core") && !TestStr(component, "Network"); });

        // Add all plugins for simplicity unless only Core or Network are used
        if (link_plugins) {
            EnumerateFiles(qt->plugins, archive_filter, 3, 512, &str_alloc, link_libraries);

            // Read plugin PRL files to add missing libraries
            for (Size i = prev_len, end = link_libraries->len; i < end; i++) {
                const char *library = (*link_libraries)[i];

                Span<const char> base = MakeSpan(library, strlen(library) - strlen(archive_filter) + 1);
                const char *prl_filename = Fmt(&str_alloc, "%1.prl", base).ptr;

                if (TestFile(prl_filename)) {
                    ParsePrlFile(prl_filename, link_libraries);
                }
            }
        }

        // Add explicit component libraries
        for (const char *component: target.qt_components) {
            const char *library_filename = Fmt(&str_alloc, "%1%/%2Qt%3%4%5", qt->libraries, lib_prefix, qt->version_major, component, archive_filter + 1).ptr;
            const char *prl_filename = Fmt(&str_alloc, "%1%/%2Qt%3%4.prl", qt->libraries, lib_prefix, qt->version_major, component).ptr;

            obj_filenames->Append(library_filename);

            if (!TestFile(prl_filename)) {
                LogError("Cannot find PRL file for Qt compoment '%1'", component);
                return false;
            }

            ParsePrlFile(prl_filename, link_libraries);
        }
    }

    // Remove pseudo-duplicate libraries (same base name)
    {
        HashSet<const char *> prev_libraries;

        Size j = prev_len;
        for (Size i = prev_len; i < link_libraries->len; i++) {
            const char *library = (*link_libraries)[i];
            const char *basename = SplitStrReverseAny(library, RG_PATH_SEPARATORS).ptr;

            (*link_libraries)[j] = library;

            if (build.compiler->platform == HostPlatform::Windows) {
                if (TestStr(basename, "qdirect2d.lib"))
                    continue;
            } else if (build.compiler->platform == HostPlatform::macOS) {
                basename += (basename[0] == '!');
            }

            bool inserted;
            prev_libraries.TrySet(basename, &inserted);

            j += inserted;
        }
        link_libraries->len = j;
    }

    return true;
}

bool Builder::CompileMocHelper(const SourceFileInfo &src, Span<const char *const> system_directories, uint32_t features)
{
    if (!PrepareQtSdk(src.target->qt_version))
        return false;

    static const char *const HeaderExtensions[] = { ".h", ".hh", ".hpp", ".hxx", ".H" };

    const char *moc_filename = build_map.FindValue({ "moc", src.filename }, nullptr);

    if (!moc_filename) {
        const char *end = GetPathExtension(src.filename).ptr;
        Span<const char> base = MakeSpan(src.filename, end - src.filename);

        for (const char *ext: HeaderExtensions) {
            const char *header_filename = Fmt(&str_alloc, "%1%2", base, ext).ptr;

            if (TestFile(header_filename, FileType::File)) {
                moc_filename = BuildObjectPath(header_filename, cache_directory, "moc_", ".cpp");

                Command cmd = {};

                cmd.cmd_line = Fmt(&str_alloc, "\"%1\" \"%2\" --no-notes -o \"%3\"", qt->moc, header_filename, moc_filename);
                cmd.cache_len = cmd.cmd_line.len;

                const char *text = Fmt(&str_alloc, "Run MOC on %!..+%1%!0", header_filename).ptr;
                if (AppendNode(text, moc_filename, cmd, { header_filename, qt->moc })) {
                    if (!build.fake && !EnsureDirectoryExists(moc_filename))
                        return false;
                }

                break;
            }
        }
    }

    if (moc_filename) {
        const char *obj_filename = build_map.FindValue({ current_ns, moc_filename }, nullptr);

        if (!obj_filename) {
            obj_filename = Fmt(&str_alloc, "%1%2", moc_filename, build.compiler->GetObjectExtension()).ptr;

            Command cmd = {};
            build.compiler->MakeObjectCommand(moc_filename, SourceType::Cxx,
                                              nullptr, src.target->definitions,
                                              src.target->include_directories, system_directories,
                                              {}, features, build.env,
                                              obj_filename, &str_alloc, &cmd);

            const char *text = Fmt(&str_alloc, "Build MOC for %!..+%1%!0", src.filename).ptr;
            AppendNode(text, obj_filename, cmd, moc_filename);
        }

        moc_map.Set(src.filename, obj_filename);
    }

    return true;
}

const char *Builder::CompileStaticQtHelper(const TargetInfo &target)
{
    static const char *const BaseCode =
R"(#include <QtCore/QtPlugin>

#if defined(_WIN32)
    Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
    Q_IMPORT_PLUGIN(QModernWindowsStylePlugin)
#elif defined(__APPLE__)
    Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
    Q_IMPORT_PLUGIN(QMacStylePlugin)
#else
    Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#endif
)";

    HeapArray<char> code;
    code.Append(BaseCode);

    for (const char *component: target.qt_components) {
        if (TestStr(component, "Svg")) {
            Fmt(&code, "Q_IMPORT_PLUGIN(QSvgPlugin)\n");
        }
    }

    const char *src_filename = Fmt(&str_alloc, "%1%/Misc%/%2_qt.cc", cache_directory, target.name).ptr;
    const char *obj_filename = Fmt(&str_alloc, "%1%2", src_filename, build.compiler->GetObjectExtension()).ptr;

    if (!TestFile(src_filename) && !WriteFile(code, src_filename))
        return nullptr;

    uint32_t features = target.CombineFeatures(build.features);

    // Build object file
    Command cmd = {};
    build.compiler->MakeObjectCommand(src_filename, SourceType::Cxx,
                                      nullptr, {}, {}, qt->headers, {}, features, build.env,
                                      obj_filename,  &str_alloc, &cmd);

    const char *text = Fmt(&str_alloc, "Compile %!..+%1%!0 static Qt helper", target.name).ptr;
    AppendNode(text, obj_filename, cmd, src_filename);

    return obj_filename;
}

void Builder::ParsePrlFile(const char *filename, HeapArray<const char *> *out_libraries)
{
    StreamReader st(filename);
    LineReader reader(&st);

    Span<const char> line = {};
    while (reader.Next(&line)) {
        Span<const char> value;
        Span<const char> key = TrimStr(SplitStr(line, '=', &value));
        value = TrimStr(value);

        if (key == "QMAKE_PRL_LIBS_FOR_CMAKE") {
            while (value.len) {
                Span<const char> part = TrimStr(SplitStr(value, ';', &value));

                if (StartsWith(part, "-l")) {
                    const char *library = DuplicateString(part.Take(2, part.len - 2), &str_alloc).ptr;
                    out_libraries->Append(library);
                } else if (StartsWith(part, "$$[QT_INSTALL_PREFIX]/lib/") || StartsWith(part, "$$[QT_INSTALL_PREFIX]\\lib\\")) {
                    const char *library = Fmt(&str_alloc, "%1%/%2", qt->libraries, part.Take(26, part.len - 26)).ptr;
                    out_libraries->Append(library);
                } else if (StartsWith(part, "$$[QT_INSTALL_LIBS]/") || StartsWith(part, "$$[QT_INSTALL_PREFIX]\\")) {
                    const char *library = Fmt(&str_alloc, "%1%/%2", qt->libraries, part.Take(20, part.len - 20)).ptr;
                    out_libraries->Append(library);
                } else if (StartsWith(part, "$$[QT_INSTALL_PREFIX]/plugins/") || StartsWith(part, "$$[QT_INSTALL_PREFIX]\\plugins\\")) {
                    const char *library = Fmt(&str_alloc, "%1%/%2", qt->libraries, part.Take(30, part.len - 30)).ptr;
                    out_libraries->Append(library);
                } else if (StartsWith(part, "$$[QT_INSTALL_PLUGINS]/") || StartsWith(part, "$$[QT_INSTALL_PLUGINS]\\")) {
                    const char *library = Fmt(&str_alloc, "%1%/%2", qt->libraries, part.Take(23, part.len - 23)).ptr;
                    out_libraries->Append(library);
                } else if (build.compiler->platform == HostPlatform::macOS && StartsWith(part, "-framework")) {
                    Span<const char> framework = TrimStr(part.Take(10, part.len - 10));

                    if (!framework.len) {
                        framework = TrimStr(SplitStr(value, ';', &value));
                    }

                    if (qt->shared || !StartsWith(framework, "Qt")) {
                        const char *copy = Fmt(&str_alloc, "!%1", framework).ptr;
                        out_libraries->Append(copy);
                    }
                }
            }

            break;
        }
    }
}

}
