// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/base/base.hh"
#include "compiler.hh"
#include "locate.hh"
#include "target.hh"
#if defined(_WIN32)
    #if !defined(NOMINMAX)
        #define NOMINMAX
    #endif
    #if !defined(WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

namespace K {

static std::mutex mutex;
static BlockAllocator str_alloc;

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
        const char *basename1 = SplitStrReverseAny(path1, K_PATH_SEPARATORS).ptr;
        const char *basename2 = SplitStrReverseAny(path2, K_PATH_SEPARATORS).ptr;

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

                    case HostArchitecture::Unknown:
                    case HostArchitecture::ARM32:
                    case HostArchitecture::RISCV64:
                    case HostArchitecture::Loong64:
                    case HostArchitecture::Web32: { matches = false; } break;
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
        case HostArchitecture::Unknown: { K_UNREACHABLE(); } break;

        case HostArchitecture::x86: return "i386";
        case HostArchitecture::x86_64: return "x86_64";
        case HostArchitecture::ARM32: return "armv7";
        case HostArchitecture::ARM64: return "aarch64";
        case HostArchitecture::RISCV64: return "riscv64gc";
        case HostArchitecture::Loong64: return "loongarch64";
        case HostArchitecture::Web32: return "web";
    }

    K_UNREACHABLE();
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

const QtInfo *FindQtSdk(const Compiler *compiler)
{
    static QtInfo qt = {};
    static bool init = false;

    if (!init) {
        init = true;

        std::lock_guard<std::mutex> lock(mutex);

        if (const char *str = GetEnv("QMAKE_PATH"); str) {
            qt.qmake = NormalizePath(str, &str_alloc).ptr;
        } else {
            bool success = FindExecutableInPath("qmake6", &str_alloc, &qt.qmake) ||
                           FindExecutableInPath("qmake", &str_alloc, &qt.qmake) ||
                           LocateSdkQmake(compiler, &str_alloc, &qt.qmake);

            if (!success) {
                LogError("Cannot find QMake binary for Qt");
                return nullptr;
            }
        }

        HeapArray<char> specs;
        {
            const char *cmd_line = Fmt(&str_alloc, "\"%1\" -query", qt.qmake).ptr;

            if (!ReadCommandOutput(cmd_line, &specs)) {
                LogError("Failed to get qmake specs: %1", specs.As());
                return nullptr;
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
                    qt.binaries = DuplicateString(value, &str_alloc).ptr;

                    if (!qt.moc) {
                        const char *binary = Fmt(&str_alloc, "%1%/moc%2", value, K_EXECUTABLE_EXTENSION).ptr;
                        qt.moc = TestFile(binary, FileType::File) ? binary : nullptr;
                    }
                    if (!qt.rcc) {
                        const char *binary = Fmt(&str_alloc, "%1%/rcc%2", value, K_EXECUTABLE_EXTENSION).ptr;
                        qt.rcc = TestFile(binary, FileType::File) ? binary : nullptr;
                    }
                    if (!qt.uic) {
                        const char *binary = Fmt(&str_alloc, "%1%/uic%2", value, K_EXECUTABLE_EXTENSION).ptr;
                        qt.uic = TestFile(binary, FileType::File) ? binary : nullptr;
                    }

    #if defined(__APPLE__)
                    if (!qt.macdeployqt) {
                        const char *binary = Fmt(&str_alloc, "%1%/macdeployqt", value).ptr;
                        qt.macdeployqt = TestFile(binary, FileType::File) ? binary : nullptr;
                    }
    #endif
                } else if (key == "QT_INSTALL_HEADERS") {
                    qt.headers = DuplicateString(value, &str_alloc).ptr;
                } else if (key == "QT_INSTALL_LIBS") {
                    valid &= AdjustLibraryPath("libraries", compiler, value, &str_alloc, &qt.libraries);
                } else if (key == "QT_INSTALL_PLUGINS") {
                    valid &= AdjustLibraryPath("plugins", compiler, value, &str_alloc, &qt.plugins);
                } else if (key == "QT_VERSION") {
                    if (!ParseVersion(value, 3, 1000, &qt.version))
                        return nullptr;

                    if (qt.version < 5000000 || qt.version >= 7000000) {
                        LogError("Only Qt5 and Qt6 are supported");
                        return nullptr;
                    }
                    qt.version_major = (int)(qt.version / 1000000);
                }
            }

            valid &= qt.moc && qt.rcc && qt.uic && qt.binaries &&
                     qt.headers && qt.libraries && qt.plugins && qt.version_major;
        }

        if (!valid) {
            LogError("Cannot find required Qt tools");

            qt.version = 0;
            return nullptr;
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
    }

    return (qt.version > 0) ? &qt : nullptr;
}

static bool TestWasiSdk(const char *path, Allocator *alloc, WasiSdkInfo *out_sdk)
{
    char cc[4096];
    char ld[4096];
    char sysroot[4096];

    if (!TestFile(path))
        return false;

    Fmt(cc, "%1%/bin/clang%2", path, K_EXECUTABLE_EXTENSION);
    Fmt(ld, "%1%/bin/wasm-ld%2", path, K_EXECUTABLE_EXTENSION);
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

const WasiSdkInfo *FindWasiSdk()
{
    static WasiSdkInfo sdk = {};
    static bool init = false;
    static bool found = false;

    if (init)
        return found ? &sdk : nullptr;

    init = true;

    std::lock_guard<std::mutex> lock(mutex);

    struct TestPath {
        const char *env;
        const char *path;
    };

    static const TestPath test_paths[] = {
        { "WASI_SDK_PATH", "" },
#if !defined(_WIN32)
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
            prefix = TrimStrRight(prefix, K_PATH_SEPARATORS);

            if (!prefix.len)
                continue;

            Fmt(path, "%1%2", prefix, test.path);
        } else {
            CopyString(test.path, path);
        }

        if (TestWasiSdk(path, &str_alloc, &sdk)) {
            LogDebug("Found WASI-SDK: %1", path);

            found = true;
            return &sdk;
        }
    }

    return nullptr;
}

static bool TestArduinoSdk(const char *path)
{
    char arduino[4096];
    char hardware[4096];

    if (!TestFile(path))
        return false;

    Fmt(arduino, "%1%/arduino%2", path, K_EXECUTABLE_EXTENSION);
    Fmt(hardware, "%1%/hardware/arduino", path);

    if (!TestFile(arduino, FileType::File))
        return false;
    if (!TestFile(hardware, FileType::Directory))
        return false;

    return true;
}

const char *FindArduinoSdk()
{
    static const char *sdk = nullptr;
    static bool init = false;

    if (init)
        return sdk;

    init = true;

    std::lock_guard<std::mutex> lock(mutex);

#if defined(_WIN32)
    {
        wchar_t buf_w[2048];
        DWORD buf_w_len = K_LEN(buf_w);

        bool found = !RegGetValueW(HKEY_LOCAL_MACHINE, L"Software\\Arduino", L"Install_Dir",
                                   RRF_RT_REG_SZ, nullptr, buf_w, &buf_w_len) ||
                     !RegGetValueW(HKEY_LOCAL_MACHINE, L"Software\\WOW6432Node\\Arduino", L"Install_Dir",
                                   RRF_RT_REG_SZ, nullptr, buf_w, &buf_w_len) ||
                     !RegGetValueW(HKEY_CURRENT_USER, L"Software\\Arduino", L"Install_Dir",
                                   RRF_RT_REG_SZ, nullptr, buf_w, &buf_w_len) ||
                     !RegGetValueW(HKEY_CURRENT_USER, L"Software\\WOW6432Node\\Arduino", L"Install_Dir",
                                   RRF_RT_REG_SZ, nullptr, buf_w, &buf_w_len);

        if (found) {
            Span<char> path = AllocateSpan<char>(&str_alloc, 2 * (Size)buf_w_len + 1);

            path.len = ConvertWin32WideToUtf8(buf_w, path);
            if (path.len < 0)
                return nullptr;

            for (char &c: path) {
                c = (c == '/') ? '\\' : c;
            }

            if (TestArduinoSdk(path.ptr)) {
                LogDebug("Found Arduino SDK: %1", path);

                sdk = path.ptr;
                return sdk;
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
#if !defined(_WIN32)
        { nullptr, "/opt/arduino" },
        { nullptr, "/usr/share/arduino" },
        { nullptr, "/usr/local/share/arduino" },
        { "HOME",  "/.local/share/arduino" },
#if defined(__APPLE__)
        { nullptr, "/Applications/Arduino.app/Contents/Java" }
#endif
#endif
    };

    for (const TestPath &test: test_paths) {
        char path[4096];

        if (test.env) {
            Span<const char> prefix = GetEnv(test.env);
            prefix = TrimStrRight(prefix, K_PATH_SEPARATORS);

            if (!prefix.len)
                continue;

            Fmt(path, "%1%2", prefix, test.path);
        } else {
            CopyString(test.path, path);
        }

        if (TestArduinoSdk(path)) {
            LogDebug("Found Arduino SDK: %1", path);

            sdk = DuplicateString(path, &str_alloc).ptr;
            return sdk;
        }
    }

    return nullptr;
}

}
