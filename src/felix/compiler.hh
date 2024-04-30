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

#pragma once

#include "src/core/base/base.hh"

namespace RG {

enum class HostPlatform {
    Windows,
    Linux,
    macOS,
    OpenBSD,
    FreeBSD,

    EmscriptenNode,
    EmscriptenWeb,
    EmscriptenBox,

    Teensy20,
    Teensy20pp,
    TeensyLC,
    Teensy30,
    Teensy31,
    Teensy35,
    Teensy36,
    Teensy40,
    Teensy41
};
static const char *const HostPlatformNames[] = {
    "Desktop/Windows",
    "Desktop/POSIX/Linux",
    "Desktop/POSIX/macOS",
    "Desktop/POSIX/OpenBSD",
    "Desktop/POSIX/FreeBSD",

    "WASM/Emscripten/Node",
    "WASM/Emscripten/Web",
    "WASM/Emscripten/Box",

    "Embedded/Teensy/AVR/Teensy20",
    "Embedded/Teensy/AVR/Teensy20++",
    "Embedded/Teensy/ARM/TeensyLC",
    "Embedded/Teensy/ARM/Teensy30",
    "Embedded/Teensy/ARM/Teensy31",
    "Embedded/Teensy/ARM/Teensy35",
    "Embedded/Teensy/ARM/Teensy36",
    "Embedded/Teensy/ARM/Teensy40",
    "Embedded/Teensy/ARM/Teensy41"
};

#if defined(_WIN32)
    static const HostPlatform NativePlatform = HostPlatform::Windows;
#elif defined(__APPLE__)
    static const HostPlatform NativePlatform = HostPlatform::macOS;
#elif defined(__linux__)
    static const HostPlatform NativePlatform = HostPlatform::Linux;
#elif defined(__OpenBSD__)
    static const HostPlatform NativePlatform = HostPlatform::OpenBSD;
#elif defined(__FreeBSD__)
    static const HostPlatform NativePlatform = HostPlatform::FreeBSD;
#elif defined(__EMSCRIPTEN__)
    static const HostPlatform NativePlatform = HostPlatform::EmscriptenNode;
#else
    #error Unsupported platform
#endif

enum class HostArchitecture {
    x86,
    x86_64,
    ARM32,
    ARM64,
    RISCV64,
    AVR,
    Web
};
static const char *const HostArchitectureNames[] = {
    "x86",
    "x86_64",
    "ARM32",
    "ARM64",
    "RISCV64",
    "AVR",
    "Web"
};

#if defined(__i386__) || defined(_M_IX86)
    static const HostArchitecture NativeArchitecture = HostArchitecture::x86;
#elif defined(__x86_64__) || defined(_M_AMD64)
    static const HostArchitecture NativeArchitecture = HostArchitecture::x86_64;
#elif defined(__arm__) || (defined(__M_ARM) && !defined(_M_ARM64))
    static const HostArchitecture NativeArchitecture = HostArchitecture::ARM32;
#elif defined(__aarch64__) || defined(_M_ARM64)
    static const HostArchitecture NativeArchitecture = HostArchitecture::ARM64;
#elif __riscv_xlen == 64
    static const HostArchitecture NativeArchitecture = HostArchitecture::RISCV64;
#else
    #error Unsupported architecture
#endif

enum class CompileFeature {
    PCH = 1 << 0,
    DistCC = 1 << 1,
    Ccache = 1 << 2,
    Warnings = 1 << 3,
    DebugInfo = 1 << 4,
    OptimizeSpeed = 1 << 5,
    OptimizeSize = 1 << 6,
    HotAssets = 1 << 7,
    ASan = 1 << 8,
    TSan = 1 << 9,
    UBSan = 1 << 10,
    LTO = 1 << 11,
    SafeStack = 1 << 12,
    ZeroInit = 1 << 13,
    CFI = 1 << 14,
    ShuffleCode = 1 << 15,
    StaticRuntime = 1 << 16,
    LinkLibrary = 1 << 17,
    NoConsole = 1 << 18,
    AESNI = 1 << 19,
    AVX2 = 1 << 20,
    AVX512 = 1 << 21,

    ESM = 1 << 22
};
static const OptionDesc CompileFeatureOptions[] = {
    {"PCH",            "Use precompiled headers for faster compilation"},
    {"DistCC",         "Use distcc for distributed compilation (must be in PATH)"},
    {"Ccache",         "Use ccache accelerator (must be in PATH)"},
    {"Warnings",       "Enable compiler warnings"},
    {"DebugInfo",      "Add debug information to generated binaries"},
    {"OptimizeSpeed",  "Optimize generated builds for speed"},
    {"OptimizeSize",   "Optimize generated builds for size"},
    {"HotAssets",      "Pack assets in reloadable shared library"},
    {"ASan",           "Enable AdressSanitizer (ASan)"},
    {"TSan",           "Enable ThreadSanitizer (TSan)"},
    {"UBSan",          "Enable UndefinedBehaviorSanitizer (UBSan)"},
    {"LTO",            "Enable Link-Time Optimization"},
    {"SafeStack",      "Enable SafeStack protection (Clang)"},
    {"ZeroInit",       "Zero-init all undefined variables (Clang)"},
    {"CFI",            "Enable forward-edge CFI protection (Clang LTO)"},
    {"ShuffleCode",    "Randomize ordering of data and functions (Clang)"},
    {"StaticRuntime",  "Use static runtime shared libraries (libc/msvcrt, etc.)"},
    {"LinkLibrary",    "Link library targets to .so/.dll files"},
    {"NoConsole",      "Link with /subsystem:windows (only for Windows)"},
    {"AESNI",          "Enable AES-NI generation and instrinsics (x86_64)"},
    {"AVX2",           "Enable AVX2 generation and instrinsics (x86_64)"},
    {"AVX512",         "Enable AVX512 generation and instrinsics (x86_64)"},

    // JS bundling
    {"ESM",            "Bundle JS in ESM format instead of IIFE"}
};

enum class SourceType {
    C,
    Cxx,
    Esbuild,
    QtUi,
    QtResources
};

enum class TargetType {
    Executable,
    Library
};
static const char *const TargetTypeNames[] = {
    "Executable",
    "Library"
};

struct Command {
    enum class DependencyMode {
        None,
        MakeLike,
        ShowIncludes,
        EsbuildMeta
    };

    Span<const char> cmd_line; // Must be C safe (NULL termination)
    Span<const ExecuteInfo::KeyValue> env_variables;

    Size cache_len;
    Size rsp_offset;
    int skip_lines;

    DependencyMode deps_mode;
    const char *deps_filename; // Used by MakeLike and EsbuildMeta modes
};

class Compiler {
    RG_DELETE_COPY(Compiler)

public:
    HostPlatform platform;
    HostArchitecture architecture;
    const char *name;

    virtual ~Compiler() {}

    virtual bool CheckFeatures(uint32_t features, uint32_t maybe_features, uint32_t *out_features) const = 0;

    virtual const char *GetObjectExtension() const = 0;
    virtual const char *GetLinkExtension(TargetType type) const = 0;
    virtual const char *GetPostExtension(TargetType type) const = 0;

    virtual void MakePackCommand(Span<const char *const> pack_filenames,
                                 const char *pack_options, const char *dest_filename,
                                 Allocator *alloc, Command *out_cmd) const = 0;

    virtual void MakePchCommand(const char *pch_filename, SourceType src_type,
                                Span<const char *const> definitions,
                                Span<const char *const> include_directories, Span<const char *const> include_files,
                                uint32_t features, bool env_flags, Allocator *alloc, Command *out_cmd) const = 0;
    virtual const char *GetPchCache(const char *pch_filename, Allocator *alloc) const = 0;
    virtual const char *GetPchObject(const char *pch_filename, Allocator *alloc) const = 0;

    virtual void MakeObjectCommand(const char *src_filename, SourceType src_type,
                                   const char *pch_filename, Span<const char *const> definitions,
                                   Span<const char *const> include_directories,
                                   Span<const char *const> system_directories,
                                   Span<const char *const> include_files,
                                   uint32_t features, bool env_flags, const char *dest_filename,
                                   Allocator *alloc, Command *out_cmd) const = 0;
    virtual void MakeResourceCommand(const char *rc_filename, const char *dest_filename,
                                     Allocator *alloc, Command *out_cmd) const = 0;

    virtual void MakeLinkCommand(Span<const char *const> obj_filenames,
                                 Span<const char *const> libraries, TargetType link_type,
                                 uint32_t features, bool env_flags, const char *dest_filename,
                                 Allocator *alloc, Command *out_cmd) const = 0;
    virtual void MakePostCommand(const char *src_filename, const char *dest_filename,
                                 Allocator *alloc, Command *out_cmd) const = 0;

protected:
    Compiler(HostPlatform platform, HostArchitecture architecture, const char *name)
        : platform(platform), architecture(architecture), name(name) {}
};

struct SupportedCompiler {
    const char *name;
    const char *cc;
};

struct HostSpecifier {
    HostPlatform platform = NativePlatform;
    HostArchitecture architecture = NativeArchitecture;
    const char *cc = nullptr;
    const char *ld = nullptr;
};

extern const Span<const SupportedCompiler> SupportedCompilers;

std::unique_ptr<const Compiler> PrepareCompiler(HostSpecifier spec);

bool DetermineSourceType(const char *filename, SourceType *out_type = nullptr);

}
