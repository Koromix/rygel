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

#include "src/core/libcc/libcc.hh"

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
    static const HostPlatform NativeHost = HostPlatform::Windows;
#elif defined(__APPLE__)
    static const HostPlatform NativeHost = HostPlatform::macOS;
#elif defined(__linux__)
    static const HostPlatform NativeHost = HostPlatform::Linux;
#elif defined(__OpenBSD__)
    static const HostPlatform NativeHost = HostPlatform::OpenBSD;
#elif defined(__FreeBSD__)
    static const HostPlatform NativeHost = HostPlatform::FreeBSD;
#elif defined(__EMSCRIPTEN__)
    static const HostPlatform NativeHost = HostPlatform::EmscriptenNode;
#else
    #error Unsupported platform
#endif

enum class CompileFeature {
    PCH = 1 << 0,
    Ccache = 1 << 1,
    DebugInfo = 1 << 2,
    StaticLink = 1 << 3,
    OptimizeSpeed = 1 << 4,
    OptimizeSize = 1 << 5,
    HotAssets = 1 << 6,
    ASan = 1 << 7,
    TSan = 1 << 8,
    UBSan = 1 << 9,
    LTO = 1 << 10,
    SafeStack = 1 << 11,
    ZeroInit = 1 << 12,
    CFI = 1 << 13,
    ShuffleCode = 1 << 14,
    Cxx17 = 1 << 15,
    NoConsole = 1 << 16,

    SSE41 = 1 << 17,
    SSE42 = 1 << 18,
    AVX2 = 1 << 19,
    AVX512 = 1 << 20
};
static const OptionDesc CompileFeatureOptions[] = {
    {"PCH",           "Use precompiled headers for faster compilation"},
    {"Ccache",        "Use Ccache accelerator (must be in PATH)"},
    {"DebugInfo",     "Add debug information to generated binaries"},
    {"StaticLink",    "Static link base system libraries (libc, etc.)"},
    {"OptimizeSpeed", "Optimize generated builds for speed"},
    {"OptimizeSize",  "Optimize generated builds for size"},
    {"HotAssets",     "Pack assets in reloadable shared library"},
    {"ASan",          "Enable AdressSanitizer (ASan)"},
    {"TSan",          "Enable ThreadSanitizer (TSan)"},
    {"UBSan",         "Enable UndefinedBehaviorSanitizer (UBSan)"},
    {"LTO",           "Enable Link-Time Optimization"},
    {"SafeStack",     "Enable SafeStack protection (Clang)"},
    {"ZeroInit",      "Zero-init all undefined variables (Clang)"},
    {"CFI",           "Enable forward-edge CFI protection (Clang LTO)"},
    {"ShuffleCode",   "Randomize ordering of data and functions (Clang)"},
    {"C++17",         "Use C++17 standard instead of C++20"},
    {"NoConsole",     "Link with /subsystem:windows (only for Windows)"},

    {"SSE41",         "Enable SSE4.1 generation and instrinsics (x86_64)"},
    {"SSE42",         "Enable SSE4.2 generation and instrinsics (x86_64)"},
    {"AVX2",          "Enable AVX2 generation and instrinsics (x86_64)"},
    {"AVX512",        "Enable AVX512 generation and instrinsics (x86_64)"}
};

enum class SourceType {
    C,
    CXX
};

enum class LinkType {
    Executable,
    SharedLibrary
};

struct Command {
    enum class DependencyMode {
        None,
        MakeLike,
        ShowIncludes
    };

    Span<const char> cmd_line; // Must be C safe (NULL termination)
    Size cache_len;
    Size rsp_offset;
    int skip_lines;
    DependencyMode deps_mode;
    const char *deps_filename; // Used by MakeLike mode
};

class Compiler {
    RG_DELETE_COPY(Compiler)

public:
    HostPlatform host;
    const char *name;

    virtual ~Compiler() {}

    virtual bool CheckFeatures(uint32_t features, uint32_t maybe_features, uint32_t *out_features) const = 0;

    virtual const char *GetObjectExtension() const = 0;
    virtual const char *GetLinkExtension() const = 0;
    virtual const char *GetPostExtension() const = 0;

    virtual bool GetCore(Span<const char *const> definitions, Allocator *alloc,
                         HeapArray<const char *> *out_filenames,
                         HeapArray<const char *> *out_definitions, const char **out_ns) const = 0;

    virtual void MakePackCommand(Span<const char *const> pack_filenames, bool optimize,
                                 const char *pack_options, const char *dest_filename,
                                 Allocator *alloc, Command *out_cmd) const = 0;

    virtual void MakePchCommand(const char *pch_filename, SourceType src_type,
                                bool warnings, Span<const char *const> definitions,
                                Span<const char *const> include_directories, Span<const char *const> include_files,
                                uint32_t features, bool env_flags, Allocator *alloc, Command *out_cmd) const = 0;
    virtual const char *GetPchCache(const char *pch_filename, Allocator *alloc) const = 0;
    virtual const char *GetPchObject(const char *pch_filename, Allocator *alloc) const = 0;

    virtual void MakeObjectCommand(const char *src_filename, SourceType src_type,
                                   bool warnings, const char *pch_filename, Span<const char *const> definitions,
                                   Span<const char *const> include_directories, Span<const char *const> include_files,
                                   uint32_t features, bool env_flags, const char *dest_filename,
                                   Allocator *alloc, Command *out_cmd) const = 0;
    virtual void MakeResourceCommand(const char *rc_filename, const char *dest_filename,
                                     Allocator *alloc, Command *out_cmd) const = 0;

    virtual void MakeLinkCommand(Span<const char *const> obj_filenames,
                                 Span<const char *const> libraries, LinkType link_type,
                                 uint32_t features, bool env_flags, const char *dest_filename,
                                 Allocator *alloc, Command *out_cmd) const = 0;
    virtual void MakePostCommand(const char *src_filename, const char *dest_filename,
                                 Allocator *alloc, Command *out_cmd) const = 0;

protected:
    Compiler(HostPlatform host, const char *name) : host(host), name(name) {}
};

struct SupportedCompiler {
    const char *name;
    const char *cc;
};

struct PlatformSpecifier {
    HostPlatform host = NativeHost;
    const char *cc = nullptr;
    const char *ld = nullptr;
};

extern const Span<const SupportedCompiler> SupportedCompilers;

std::unique_ptr<const Compiler> PrepareCompiler(PlatformSpecifier spec);

RG_EXPORT bool DetermineSourceType(const char *filename, SourceType *out_type = nullptr);

}
