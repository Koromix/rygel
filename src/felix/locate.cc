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
#include "compiler.hh"
#include "locate.hh"
#include "target.hh"
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

namespace RG {

static bool LocateSdkQmake(const Compiler *compiler, Allocator *alloc, const char **out_qmake)
{
    BlockAllocator temp_alloc;

    struct TestPath {
        const char *env;
        const char *path;
    };

    static const TestPath test_paths[] = {
#if defined(_WIN32)
        { nullptr, "C:/Qt"},
        { "SystemDrive", "Qt" }
#else
        { "HOME",  "Qt" }
#endif
    };

    // Enumerate possible candidates
    HeapArray<Span<const char>> sdk_candidates;
    for (const TestPath &test: test_paths) {
        const char *directory = nullptr;

        if (test.env) {
            Span<const char> prefix = getenv(test.env);

            if (!prefix.len)
                continue;

            while (prefix.len && IsPathSeparator(prefix[prefix.len - 1])) {
                prefix.len--;
            }

            directory = Fmt(&temp_alloc, "%1%/%2", prefix, test.path).ptr;
        } else {
            directory = Fmt(&temp_alloc, "%1", test.path).ptr;
        }

        if (TestFile(directory, FileType::Directory)) {
            EnumerateDirectory(directory, nullptr, 128, [&](const char *basename, FileType file_type) {
                if (file_type != FileType::Directory)
                    return true;
                if (!StartsWith(basename, "5.") && !StartsWith(basename, "6."))
                    return true;

                Span<const char> candidate = NormalizePath(basename, directory, &temp_alloc);
                sdk_candidates.Append(candidate);

                return true;
            });
        }
    }

    // Sort by decreasing version
    std::sort(sdk_candidates.begin(), sdk_candidates.end(), [](Span<const char> path1, Span<const char> path2) {
        const char *basename1 = SplitStrReverseAny(path1, RG_PATH_SEPARATORS).ptr;
        const char *basename2 = SplitStrReverseAny(path2, RG_PATH_SEPARATORS).ptr;

        return CmpStr(basename1, basename2) > 0;
    });

    // Find first suitable candidate
    for (Span<const char> candidate: sdk_candidates) {
        const char *qmake_binary = nullptr;

        EnumerateDirectory(candidate.ptr, nullptr, 32, [&](const char *basename, FileType file_type) {
            if (file_type != FileType::Directory)
                return true;

            bool matches = true;

            if (compiler->platform == HostPlatform::macOS) {
                matches &= TestStr(basename, "macos");
            } else {
                // There are multiple ABIs on Windows (the official one, and MinGW stuff)
                if (compiler->platform == HostPlatform::Windows) {
                    const char *prefix = TestStr(compiler->name, "GCC") ? "mingw" : "msvc";
                    matches &= StartsWith(basename, prefix);
                } else if (compiler->platform == HostPlatform::Windows) {
                    matches &= StartsWith(basename, "gcc") || StartsWith(basename, "clang");
                }

                switch (compiler->architecture) {
                    case HostArchitecture::i386: { matches &= EndsWith(basename, "_32"); } break;
                    case HostArchitecture::x86_64: { matches &= EndsWith(basename, "_64"); } break;
                    case HostArchitecture::ARM64: { matches &= EndsWith(basename, "_arm32"); } break;

                    case HostArchitecture::ARM32:
                    case HostArchitecture::RISCV64:
                    case HostArchitecture::AVR:
                    case HostArchitecture::Web: { matches = false; } break;
                }
            }

            if (matches) {
                const char *ext = compiler->GetLinkExtension(TargetType::Executable);
                const char *binary = Fmt(alloc, "%1%/%2%/bin%/qmake%3", candidate, basename, ext).ptr;

                if (TestFile(binary, FileType::File)) {
                    // Interrupt enumeration, we're done!
                    qmake_binary = binary;
                    return false;
                }
            }

            return true;
        });

        if (qmake_binary) {
            *out_qmake = qmake_binary;
            return true;
        }
    }

    return false;
}

bool FindQtSdk(const Compiler *compiler, const char *qmake_binary, Allocator *alloc, QtInfo *out_qt)
{
    QtInfo qt = {};

    if (qmake_binary) {
        qt.qmake = NormalizePath(qmake_binary, alloc).ptr;
    } else if (getenv("QMAKE_PATH")) {
        const char *binary = getenv("QMAKE_PATH");
        qt.qmake = NormalizePath(binary, alloc).ptr;
    } else {
        bool success = FindExecutableInPath("qmake6", alloc, &qt.qmake) ||
                       FindExecutableInPath("qmake", alloc, &qt.qmake) ||
                       LocateSdkQmake(compiler, alloc, &qt.qmake);

        if (!success) {
            LogError("Cannot find QMake binary for Qt");
            return false;
        }
    }

    HeapArray<char> specs;
    {
        const char *cmd_line = Fmt(alloc, "\"%1\" -query", qt.qmake).ptr;

        if (!ReadCommandOutput(cmd_line, &specs)) {
            LogError("Failed to get qmake specs: %1", specs.As());
            return false;
        }
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
                qt.binaries = DuplicateString(value, alloc).ptr;

                if (!qt.moc) {
                    const char *binary = Fmt(alloc, "%1%/moc%2", value, RG_EXECUTABLE_EXTENSION).ptr;
                    qt.moc = TestFile(binary, FileType::File) ? binary : nullptr;
                }
                if (!qt.rcc) {
                    const char *binary = Fmt(alloc, "%1%/rcc%2", value, RG_EXECUTABLE_EXTENSION).ptr;
                    qt.rcc = TestFile(binary, FileType::File) ? binary : nullptr;
                }
                if (!qt.uic) {
                    const char *binary = Fmt(alloc, "%1%/uic%2", value, RG_EXECUTABLE_EXTENSION).ptr;
                    qt.uic = TestFile(binary, FileType::File) ? binary : nullptr;
                }
            } else if (key == "QT_INSTALL_HEADERS") {
                qt.headers = DuplicateString(value, alloc).ptr;
            } else if (key == "QT_INSTALL_LIBS") {
                qt.libraries = DuplicateString(value, alloc).ptr;
            } else if (key == "QT_INSTALL_PLUGINS") {
                qt.plugins = DuplicateString(value, alloc).ptr;
            } else if (key == "QT_VERSION") {
                qt.version = ParseVersionString(value, 3);

                if (qt.version < 5000000 || qt.version >= 7000000) {
                    LogError("Only Qt5 and Qt6 are supported");
                    return false;
                }
                qt.version_major = (int)(qt.version / 1000000);
            }

            valid = qt.moc && qt.rcc && qt.uic && qt.binaries &&
                    qt.headers && qt.libraries && qt.plugins && qt.version_major;
        }
    }

    if (!valid) {
        LogError("Cannot find required Qt tools");

        qt.version = -1;
        return false;
    }

    // Determine if Qt is built statically
    if (compiler->platform == HostPlatform::Windows) {
        char library0[4046];
        Fmt(library0, "%1%/Qt%2Core.dll", qt.binaries, qt.version_major);

        qt.shared = TestFile(library0);
    } else if (compiler->platform == HostPlatform::macOS) {
        char library0[4046];
        Fmt(library0, "%1%/libQt%2Core.a", qt.libraries, qt.version_major);

        qt.shared = !TestFile(library0);
    } else {
        char library0[4046];
        Fmt(library0, "%1%/libQt%2Core.so", qt.libraries, qt.version_major);

        qt.shared = TestFile(library0);
    }

    std::swap(qt, *out_qt);
    return true;
}

bool FindArduinoCompiler(const char *name, const char *compiler, Span<char> out_cc)
{
#ifdef _WIN32
    wchar_t buf[2048];
    DWORD buf_len = RG_LEN(buf);

    bool arduino = !RegGetValueW(HKEY_LOCAL_MACHINE, L"Software\\Arduino", L"Install_Dir",
                                 RRF_RT_REG_SZ, nullptr, buf, &buf_len) ||
                   !RegGetValueW(HKEY_LOCAL_MACHINE, L"Software\\WOW6432Node\\Arduino", L"Install_Dir",
                                 RRF_RT_REG_SZ, nullptr, buf, &buf_len) ||
                   !RegGetValueW(HKEY_CURRENT_USER, L"Software\\Arduino", L"Install_Dir",
                                 RRF_RT_REG_SZ, nullptr, buf, &buf_len) ||
                   !RegGetValueW(HKEY_CURRENT_USER, L"Software\\WOW6432Node\\Arduino", L"Install_Dir",
                                 RRF_RT_REG_SZ, nullptr, buf, &buf_len);
    if (!arduino)
        return false;

    Size pos = ConvertWin32WideToUtf8(buf, out_cc);
    if (pos < 0)
        return false;

    Span<char> remain = out_cc.Take(pos, out_cc.len - pos);
    Fmt(remain, "%/%1.exe", compiler);
    for (Size i = 0; remain.ptr[i]; i++) {
        char c = remain.ptr[i];
        remain.ptr[i] = (c == '/') ? '\\' : c;
    }

    if (TestFile(out_cc.ptr, FileType::File)) {
        LogDebug("Found %1 compiler for Teensy: '%2'", name, out_cc.ptr);
        return true;
    }
#else
    struct TestPath {
        const char *env;
        const char *path;
    };

    static const TestPath test_paths[] = {
        { nullptr, "/usr/share/arduino" },
        { nullptr, "/usr/local/share/arduino" },
        { "HOME",  ".local/share/arduino" },
#ifdef __APPLE__
        { nullptr, "/Applications/Arduino.app/Contents/Java" }
#endif
    };

    for (const TestPath &test: test_paths) {
        if (test.env) {
            Span<const char> prefix = getenv(test.env);

            if (!prefix.len)
                continue;

            while (prefix.len && IsPathSeparator(prefix[prefix.len - 1])) {
                prefix.len--;
            }

            Fmt(out_cc, "%1%/%2%/%3", prefix, test.path, compiler);
        } else {
            Fmt(out_cc, "%1%/%2", test.path, compiler);
        }

        if (TestFile(out_cc.ptr, FileType::File)) {
            LogDebug("Found %1 compiler for Teensy: '%2'", name, out_cc.ptr);
            return true;
        }
    }
#endif

    out_cc[0] = 0;
    return false;
}

}
