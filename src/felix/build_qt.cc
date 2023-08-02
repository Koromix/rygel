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

#include "src/core/libcc/libcc.hh"
#include "build.hh"

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
    if (qt_version < 0)
        return false;

    if (!qmake_binary) {
        if (build.qmake_binary) {
            qmake_binary = build.qmake_binary;
        } else if (getenv("QMAKE_PATH")) {
            const char *str = getenv("QMAKE_PATH");

            if (str && str[0]) {
                qmake_binary = str;
            }
        } else if (FindExecutableInPath("qmake6", &str_alloc, &qmake_binary)) {
            // Done
        } else if (!FindExecutableInPath("qmake", &str_alloc, &qmake_binary)) {
            LogError("Cannot find QMake binary for Qt");
            return false;
        }

        HeapArray<char> specs;
        {
            const char *cmd_line = Fmt(&str_alloc, "\"%1\" -query", qmake_binary).ptr;

            if (!ReadCommandOutput(cmd_line, &specs))
                return false;
        }

        bool valid = false;

        // Parse specs to find moc, include paths, library path
        {
            StreamReader st(specs.As<const uint8_t>(), "<qmake>");
            LineReader reader(&st);

            Span<const char> line;
            while (!valid && reader.Next(&line)) {
                Span<const char> value = {};
                Span<const char> key = TrimStr(SplitStr(line, ':', &value));
                value = TrimStr(value);

                if (key == "QT_HOST_BINS" || key == "QT_HOST_LIBEXECS") {
                    if (!moc_binary) {
                        const char *binary = Fmt(&str_alloc, "%1%/moc%2", value, build.compiler->GetLinkExtension()).ptr;
                        moc_binary = TestFile(binary, FileType::File) ? binary : nullptr;
                    }
                    if (!rcc_binary) {
                        const char *binary = Fmt(&str_alloc, "%1%/rcc%2", value, build.compiler->GetLinkExtension()).ptr;
                        rcc_binary = TestFile(binary, FileType::File) ? binary : nullptr;
                    }
                    if (!uic_binary) {
                        const char *binary = Fmt(&str_alloc, "%1%/uic%2", value, build.compiler->GetLinkExtension()).ptr;
                        uic_binary = TestFile(binary, FileType::File) ? binary : nullptr;
                    }
                } else if (key == "QT_INSTALL_BINS") {
                    qt_binaries = DuplicateString(value, &str_alloc).ptr;
                } else if (key == "QT_INSTALL_HEADERS") {
                    qt_headers = DuplicateString(value, &str_alloc).ptr;
                } else if (key == "QT_INSTALL_LIBS") {
                    qt_libraries = DuplicateString(value, &str_alloc).ptr;
                } else if (key == "QT_INSTALL_PLUGINS") {
                    qt_plugins = DuplicateString(value, &str_alloc).ptr;
                } else if (key == "QT_VERSION") {
                    qt_version = ParseVersionString(value, 3);

                    if (qt_version < 5000000 || qt_version >= 7000000) {
                        LogError("Only Qt5 and Qt6 are supported");
                        return false;
                    }
                    qt_major = qt_version / 1000000;
                }

                valid = moc_binary && rcc_binary && uic_binary &&
                        qt_binaries && qt_headers && qt_libraries && qt_plugins && qt_major;
            }
        }

        if (!valid) {
            LogError("Cannot find required Qt tools");

            qt_version = -1;
            return false;
        }

        // Determine if Qt is built statically
        if (build.compiler->platform == HostPlatform::Windows) {
            char library0[4046];
            Fmt(library0, "%1%/Qt%2Core.dll", qt_binaries, qt_major);

            qt_static = !TestFile(library0);
        } else {
            char library0[4046];
            Fmt(library0, "%1%/libQt%2Core.so", qt_libraries, qt_major);

            qt_static = !TestFile(library0);
        }
    }

    if (qt_version < min_version) {
        LogError("Found Qt %1 but %2 is required", FormatVersion(qt_version, 3), FormatVersion(min_version, 3));
        return false;
    }

    return true;
}

const char *Builder::AddQtUiSource(const SourceFileInfo &src)
{
    RG_ASSERT(src.type == SourceType::QtUi);

    const char *header_filename = build_map.FindValue({ nullptr, src.filename }, nullptr);

    // First, we need Qt!
    if (!header_filename && !PrepareQtSdk(src.target->qt_version))
        return nullptr;

    // Run Qt UI builder
    if (!header_filename) {
        const char *target_includes = GetTargetIncludeDirectory(*src.target);

        // Get basename without extension
        Span<const char> basename = SplitStrReverseAny(src.filename, RG_PATH_SEPARATORS);
        SplitStrReverse(basename, '.', &basename);

        header_filename = Fmt(&str_alloc, "%1%/ui_%2.h", target_includes, basename).ptr;

        Command cmd = {};

        // Assemble uic command
        {
            HeapArray<char> buf(&str_alloc);

            Fmt(&buf, "\"%1\" -o \"%2\" \"%3\"", uic_binary, header_filename, src.filename);

            cmd.cache_len = buf.len;
            cmd.cmd_line = buf.TrimAndLeak(1);
        }

        const char *text = Fmt(&str_alloc, "Build UI %!..+%1%!0", src.filename).ptr;
        if (AppendNode(text, header_filename, cmd, { src.filename, uic_binary }, nullptr)) {
            if (!build.fake && !EnsureDirectoryExists(header_filename))
                return nullptr;
        }
    }

    return header_filename;
}

const char *Builder::AddQtResource(const TargetInfo &target, Span<const char *> qrc_filenames)
{
    const char *cpp_filename = build_map.FindValue({ nullptr, qrc_filenames[0] }, nullptr);

    if (!cpp_filename) {
        cpp_filename = Fmt(&str_alloc, "%1%/Misc%/%2_qrc.cc", cache_directory, target.name).ptr;

        Command cmd = {};

        // Prepare QRC build command
        {
            HeapArray<char> buf(&str_alloc);

            Fmt(&buf, "\"%1\" -o \"%2\"", rcc_binary, cpp_filename);
            for (const char *qrc_filename: qrc_filenames) {
                Fmt(&buf, " \"%1\"", qrc_filename);
            }

            cmd.cache_len = buf.len;
            cmd.cmd_line = buf.TrimAndLeak(1);
        }

        const char *text = Fmt(&str_alloc, "Assemble %!..+%1%!0 resource file", target.name).ptr;
        AppendNode(text, cpp_filename, cmd, qrc_filenames, nullptr);
    }

    const char *obj_filename = build_map.FindValue({ nullptr, cpp_filename }, nullptr);

    if (!obj_filename) {
        obj_filename = Fmt(&str_alloc, "%1%2", cpp_filename, build.compiler->GetObjectExtension()).ptr;

        uint32_t features = target.CombineFeatures(build.features);

        Command cmd = {};
        build.compiler->MakeObjectCommand(cpp_filename, SourceType::Cxx,
                                          nullptr, {}, {}, {}, {}, features, build.env,
                                          obj_filename, &str_alloc, &cmd);

        const char *text = Fmt(&str_alloc, "Compile %!..+%1%!0", cpp_filename).ptr;
        AppendNode(text, obj_filename, cmd, cpp_filename, nullptr);
    }

    return obj_filename;
}

bool Builder::AddQtDirectories(const SourceFileInfo &src, HeapArray<const char *> *out_list)
{
    if (!PrepareQtSdk(src.target->qt_version))
        return false;

    out_list->Append(qt_headers);

    const char *src_directory = DuplicateString(GetPathDirectory(src.filename), &str_alloc).ptr;
    out_list->Append(src_directory);

    for (const char *component: src.target->qt_components) {
        const char *dirname = Fmt(&str_alloc, "%1%/Qt%2", qt_headers, component).ptr;
        out_list->Append(dirname);
    }

    const char *target_includes = GetTargetIncludeDirectory(*src.target);
    out_list->Append(target_includes);

    return true;
}

bool Builder::AddQtLibraries(const TargetInfo &target, HeapArray<const char *> *obj_filenames,
                                                       HeapArray<const char *> *link_libraries)
{
    if (!PrepareQtSdk(target.qt_version))
        return false;

    if (qt_static) {
        const char *obj_filename = CompileStaticQtHelper(target);
        obj_filenames->Append(obj_filename);

        Span<const char *const> libraries = CacheList(&target, [&](HeapArray<const char *> *out_libraries) {
            EnumerateFiles(qt_plugins, archive_filter, 3, 512, &str_alloc, out_libraries);

            for (Size i = 0, end = out_libraries->len; i < end; i++) {
                const char *library = (*out_libraries)[i];

                Span<const char> base = MakeSpan(library, strlen(library) - strlen(archive_filter) + 1);
                const char *prl_filename = Fmt(&str_alloc, "%1.prl", base).ptr;

                if (TestFile(prl_filename)) {
                    ParsePrlFile(prl_filename, out_libraries);
                }
            }

            for (const char *component: target.qt_components) {
                const char *prl_filename = Fmt(&str_alloc, "%1%/%2Qt%3%4.prl", qt_libraries, lib_prefix, qt_major, component).ptr;

                if (!TestFile(prl_filename)) {
                    LogError("Cannot find PRL file for Qt compoment '%1'", component);
                    return false;
                }

                ParsePrlFile(prl_filename, out_libraries);
            }

            HashSet<const char *> prev_libraries;

            // Remove pseudo-duplicates (same base name)
            Size j = 0;
            for (Size i = 0; i < out_libraries->len; i++) {
                const char *library = (*out_libraries)[i];
                const char *basename = SplitStrReverseAny(library, RG_PATH_SEPARATORS).ptr;

                (*out_libraries)[j] = library;

#ifdef _WIN32
                if (TestStr(basename, "qdirect2d.lib"))
                    continue;
#endif

                bool inserted;
                prev_libraries.TrySet(basename, &inserted);

                j += inserted;
            }
            out_libraries->len = j;

            return true;
        });

        link_libraries->Append(libraries);
    } else {
        for (const char *component: target.qt_components) {
            const char *library = Fmt(&str_alloc, "%1%/%2Qt%3%4%5", qt_libraries, lib_prefix,
                                                                    qt_major, component, import_extension).ptr;
            obj_filenames->Append(library);
        }
    }

    return true;
}

bool Builder::CompileMocHelper(const SourceFileInfo &src, uint32_t features, HeapArray<const char *> *out_objects)
{
    if (!PrepareQtSdk(src.target->qt_version))
        return false;

    static const char *const HeaderExtensions[] = { ".h", ".hh", ".hpp", ".hxx", ".H" };

    const char *moc_filename = build_map.FindValue({ "moc", src.filename }, nullptr);

    if (!moc_filename) {
        RG_ASSERT(moc_binary);

        const char *end = GetPathExtension(src.filename).ptr;
        Span<const char> base = MakeSpan(src.filename, end - src.filename);

        HeapArray<char> buf(&str_alloc);

        for (const char *ext: HeaderExtensions) {
            buf.RemoveFrom(0);
            Fmt(&buf, "%1%2", base, ext);

            if (TestFile(buf.ptr, FileType::File)) {
                const char *header_filename = buf.TrimAndLeak(1).ptr;

                moc_filename = BuildObjectPath("moc", header_filename, cache_directory, ".cpp");

                Command cmd = {};

                // Assemble moc command
                {
                    Fmt(&buf, "\"%1\" \"%2\" --no-notes -o \"%3\"", moc_binary, header_filename, moc_filename);

                    cmd.cache_len = buf.len;
                    cmd.cmd_line = buf.TrimAndLeak(1);
                }

                const char *text = Fmt(&str_alloc, "Run MOC on %!..+%1%!0", header_filename).ptr;
                if (AppendNode(text, moc_filename, cmd, { src.filename, header_filename, moc_binary }, "moc")) {
                    if (!build.fake && !EnsureDirectoryExists(moc_filename))
                        return false;
                }

                break;
            }
        }
    }

    if (moc_filename) {
        const char *obj_filename = build_map.FindValue({ nullptr, moc_filename }, nullptr);

        if (!obj_filename) {
            obj_filename = Fmt(&str_alloc, "%1%2", moc_filename, build.compiler->GetObjectExtension()).ptr;

            HeapArray<const char *> *list = cache_lists.Find(&src);
            Span<const char *> system_directories = list ? list->As() : MakeSpan((const char **)nullptr, 0);

            Command cmd = {};
            build.compiler->MakeObjectCommand(moc_filename, SourceType::Cxx,
                                              nullptr, src.target->definitions,
                                              src.target->include_directories, system_directories,
                                              {}, features, build.env,
                                              obj_filename, &str_alloc, &cmd);

            const char *text = Fmt(&str_alloc, "Build MOC for %!..+%1%!0", src.filename).ptr;
            AppendNode(text, obj_filename, cmd, moc_filename, nullptr);
        }

        if (out_objects) {
            out_objects->Append(obj_filename);
        }
    }

    return true;
}

const char *Builder::CompileStaticQtHelper(const TargetInfo &target)
{
    static const char *const StaticCode =
R"(#include <QtCore/QtPlugin>

#if defined(_WIN32)
    Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
    Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)
#elif defined(__APPLE__)
    Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
#else
    Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#endif
)";

    const char *src_filename = Fmt(&str_alloc, "%1%/Misc%/%2_qt.cc", cache_directory, target.name).ptr;
    const char *obj_filename = Fmt(&str_alloc, "%1%2", src_filename, build.compiler->GetObjectExtension()).ptr;

    if (!TestFile(src_filename) && !WriteFile(StaticCode, src_filename))
        return nullptr;

    uint32_t features = target.CombineFeatures(build.features);

    // Build object file
    Command cmd = {};
    build.compiler->MakeObjectCommand(src_filename, SourceType::Cxx,
                                      nullptr, {}, {}, qt_headers, {}, features, build.env,
                                      obj_filename,  &str_alloc, &cmd);

    const char *text = Fmt(&str_alloc, "Compile %!..+%1%!0 static Qt helper", target.name).ptr;
    AppendNode(text, obj_filename, cmd, src_filename, nullptr);

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
                    const char *library = Fmt(&str_alloc, "%1%/%2", qt_libraries, part.Take(26, part.len - 26)).ptr;
                    out_libraries->Append(library);
                } else if (StartsWith(part, "$$[QT_INSTALL_LIBS]/") || StartsWith(part, "$$[QT_INSTALL_PREFIX]\\")) {
                    const char *library = Fmt(&str_alloc, "%1%/%2", qt_libraries, part.Take(20, part.len - 20)).ptr;
                    out_libraries->Append(library);
                } else if (StartsWith(part, "$$[QT_INSTALL_PREFIX]/plugins/") || StartsWith(part, "$$[QT_INSTALL_PREFIX]\\plugins\\")) {
                    const char *library = Fmt(&str_alloc, "%1%/%2", qt_libraries, part.Take(30, part.len - 30)).ptr;
                    out_libraries->Append(library);
                } else if (StartsWith(part, "$$[QT_INSTALL_PLUGINS]/") || StartsWith(part, "$$[QT_INSTALL_PLUGINS]\\")) {
                    const char *library = Fmt(&str_alloc, "%1%/%2", qt_libraries, part.Take(23, part.len - 23)).ptr;
                    out_libraries->Append(library);
                }
            }

            break;
        }
    }
}

}
