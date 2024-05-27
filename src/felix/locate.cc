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
            Span<const char> prefix = GetEnv(test.env);

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
                    case HostArchitecture::x86: { matches &= EndsWith(basename, "_32"); } break;
                    case HostArchitecture::x86_64: { matches &= EndsWith(basename, "_64"); } break;
                    case HostArchitecture::ARM64: { matches &= EndsWith(basename, "_arm32"); } break;

                    case HostArchitecture::ARM32:
                    case HostArchitecture::RISCV64:
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

static const char *GetGnuArchitectureString(HostArchitecture architecture)
{
    switch (architecture) {
        case HostArchitecture::x86: return "i386";
        case HostArchitecture::x86_64: return "x86_64";
        case HostArchitecture::ARM32: return "armv7";
        case HostArchitecture::ARM64: return "aarch64";
        case HostArchitecture::RISCV64: return "riscv64gc";
        case HostArchitecture::Web: return "web";
    }

    RG_UNREACHABLE();
}

static bool AdjustLibraryPath(const char *name, const Compiler *compiler,
                              Span<const char> path, Allocator *alloc, const char **out_path)
{
    *out_path = nullptr;

    if (compiler->platform != NativePlatform) {
        LogError("Cross-compilation is not supported with Qt (as of now)");
        return false;
    }

    if (compiler->architecture != NativeArchitecture) {
        Span<const char> from = GetGnuArchitectureString(NativeArchitecture);
        Span<const char> to = GetGnuArchitectureString(compiler->architecture);

        HeapArray<char> buf(alloc);

        for (Size i = 0; i < path.len;) {
            Span<const char> remain = path.Take(i, path.len - i);

            if (StartsWith(remain, from) && (!i || strchr("_-./", path[i - 1]))
                                         && (i + from.len >= path.len || strchr("_-./", path[i + from.len]))) {
                buf.Append(to);
                i += from.len;
            } else {
                buf.Append(path[i]);
                i++;
            }
        }

        buf.Grow(1);
        buf.ptr[buf.len] = 0;

        if (!TestFile(buf.ptr, FileType::Directory)) {
            LogError("Missing Qt %1 for %2", name, HostArchitectureNames[(int)compiler->architecture]);
            return false;
        }

        *out_path = buf.TrimAndLeak(1).ptr;
    } else {
        *out_path = DuplicateString(path, alloc).ptr;
    }

    return true;
}

bool FindQtSdk(const Compiler *compiler, const char *qmake_binary, Allocator *alloc, QtInfo *out_qt)
{
    QtInfo qt = {};

    if (qmake_binary) {
        qt.qmake = NormalizePath(qmake_binary, alloc).ptr;
    } else if (const char *binary = GetEnv("QMAKE_PATH"); binary) {
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

    bool valid = true;

    // Parse specs to find moc, include paths, library path
    {
        StreamReader st(specs.As<const uint8_t>(), "<qmake>");
        LineReader reader(&st);

        Span<const char> line;
        while (reader.Next(&line)) {
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

#ifdef __APPLE__
                if (!qt.macdeployqt) {
                    const char *binary = Fmt(alloc, "%1%/macdeployqt", value).ptr;
                    qt.macdeployqt = TestFile(binary, FileType::File) ? binary : nullptr;
                }
#endif
            } else if (key == "QT_INSTALL_HEADERS") {
                qt.headers = DuplicateString(value, alloc).ptr;
            } else if (key == "QT_INSTALL_LIBS") {
                valid &= AdjustLibraryPath("libraries", compiler, value, alloc, &qt.libraries);
            } else if (key == "QT_INSTALL_PLUGINS") {
                valid &= AdjustLibraryPath("plugins", compiler, value, alloc, &qt.plugins);
            } else if (key == "QT_VERSION") {
                qt.version = ParseVersionString(value, 3);

                if (qt.version < 5000000 || qt.version >= 7000000) {
                    LogError("Only Qt5 and Qt6 are supported");
                    return false;
                }
                qt.version_major = (int)(qt.version / 1000000);
            }
        }

        valid &= qt.moc && qt.rcc && qt.uic && qt.binaries &&
                 qt.headers && qt.libraries && qt.plugins && qt.version_major;
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

static bool TestWasiSdk(const char *path, Allocator *alloc, WasiSdkInfo *out_sdk)
{
    char cc[4096];
    char ld[4096];
    char sysroot[4096];

    if (!TestFile(path))
        return false;

    Fmt(cc, "%1%/bin/clang%2", path, RG_EXECUTABLE_EXTENSION);
    Fmt(ld, "%1%/bin/wasm-ld%2", path, RG_EXECUTABLE_EXTENSION);
    Fmt(sysroot, "%1%/share/wasi-sysroot", path);

    if (!TestFile(cc, FileType::File))
        return false;
    if (!TestFile(ld, FileType::File))
        return false;
    if (!TestFile(sysroot, FileType::Directory))
        return false;

    out_sdk->path = DuplicateString(path, alloc).ptr;
    out_sdk->cc = DuplicateString(cc, alloc).ptr;
    out_sdk->sysroot = DuplicateString(sysroot, alloc).ptr;

    return true;
}

bool FindWasiSdk(Allocator *alloc, WasiSdkInfo *out_sdk)
{
    struct TestPath {
        const char *env;
        const char *path;
    };

    static const TestPath test_paths[] = {
        { "WASI_SDK_PATH", "" },
#ifndef _WIN32
        { nullptr, "/opt/wasi-sdk" },
        { nullptr, "/usr/share/wasi-sdk" },
        { nullptr, "/usr/local/share/wasi-sdk" },
        { "HOME",  "/.local/share/wasi-sdk" },
        { "HOME",  "/wasi-sdk" }
#endif
    };

    for (const TestPath &test: test_paths) {
        char path[4096];

        if (test.env) {
            Span<const char> prefix = GetEnv(test.env);
            prefix = TrimStrRight(prefix, RG_PATH_SEPARATORS);

            if (!prefix.len)
                continue;

            Fmt(path, "%1%2", prefix, test.path);
        } else {
            CopyString(test.path, path);
        }

        if (TestWasiSdk(path, alloc, out_sdk)) {
            LogDebug("Found WASI-SDK: %1", path);
            return true;
        }
    }

    return false;
}

bool FindArduinoSdk(const char *compiler, Allocator *alloc, const char **out_arduino, const char **out_cc)
{
#ifdef _WIN32
    {
        wchar_t buf_w[2048];
        DWORD buf_w_len = RG_LEN(buf);

        bool found = !RegGetValueW(HKEY_LOCAL_MACHINE, L"Software\\Arduino", L"Install_Dir",
                                   RRF_RT_REG_SZ, nullptr, buf_w, &buf_w_len) ||
                     !RegGetValueW(HKEY_LOCAL_MACHINE, L"Software\\WOW6432Node\\Arduino", L"Install_Dir",
                                   RRF_RT_REG_SZ, nullptr, buf_w, &buf_w_len) ||
                     !RegGetValueW(HKEY_CURRENT_USER, L"Software\\Arduino", L"Install_Dir",
                                   RRF_RT_REG_SZ, nullptr, buf_w, &buf_w_len) ||
                     !RegGetValueW(HKEY_CURRENT_USER, L"Software\\WOW6432Node\\Arduino", L"Install_Dir",
                                   RRF_RT_REG_SZ, nullptr, buf_w, &buf_w_len);

        if (found) {
            Span<char> arduino = AllocateSpan<char>(alloc, 2 * (Size)buf_w_len + 1);

            arduino.len = ConvertWin32WideToUtf8(buf_w, arduino);
            if (arduino.len < 0)
                return false;

            for (char &c: arduino) {
                c = (c == '/') ? '\\' : c;
            }

            Span<char> cc = Fmt(alloc, "%1%/%2.exe", arduino, compiler);

            if (!TestFile(cc.ptr, FileType::File)) {
                LogDebug("Found compiler for Teensy: %1", cc);

                *out_arduino = arduino.ptr;
                *out_cc = cc.ptr;

                return true;
            }
        }
    }
#endif

    struct TestPath {
        const char *env;
        const char *path;
    };

    static const TestPath test_paths[] = {
        { "ARDUINO_PATH", "" },
#ifndef _WIN32
        { nullptr, "/opt/arduino" },
        { nullptr, "/usr/share/arduino" },
        { nullptr, "/usr/local/share/arduino" },
        { "HOME",  "/.local/share/arduino" },
#ifdef __APPLE__
        { nullptr, "/Applications/Arduino.app/Contents/Java" }
#endif
#endif
    };

    for (const TestPath &test: test_paths) {
        char arduino[4096];
        char cc[4096];

        if (test.env) {
            Span<const char> prefix = GetEnv(test.env);
            prefix = TrimStrRight(prefix, RG_PATH_SEPARATORS);

            if (!prefix.len)
                continue;

            Fmt(arduino, "%1%%2", prefix, test.path);
            Fmt(cc, "%1%/%2", arduino, compiler);
        } else {
            CopyString(test.path, arduino);
            Fmt(cc, "%1%/%2", arduino, compiler);
        }

        if (TestFile(cc, FileType::File)) {
            LogDebug("Found compiler for Teensy: %1", cc);

            *out_arduino = DuplicateString(arduino, alloc).ptr;
            *out_cc = DuplicateString(cc, alloc).ptr;

            return true;
        }
    }

    return false;
}

}
