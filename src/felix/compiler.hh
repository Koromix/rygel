// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

enum class HostPlatform {
    Windows,
    Linux,
    macOS,
    OpenBSD,
    FreeBSD,

    EmscriptenNode,
    EmscriptenWeb,
    WasmWasi,

    TeensyLC,
    Teensy30,
    Teensy31,
    Teensy35,
    Teensy36,
    Teensy40,
    Teensy41,
    TeensyMM
};
static const char *const HostPlatformNames[] = {
    "Desktop/Windows",
    "Desktop/POSIX/Linux",
    "Desktop/POSIX/macOS",
    "Desktop/POSIX/OpenBSD",
    "Desktop/POSIX/FreeBSD",

    "WASM/Emscripten/Node",
    "WASM/Emscripten/Web",
    "WASM/WASI",

    "Embedded/Teensy/ARM/TeensyLC",
    "Embedded/Teensy/ARM/Teensy30",
    "Embedded/Teensy/ARM/Teensy31",
    "Embedded/Teensy/ARM/Teensy35",
    "Embedded/Teensy/ARM/Teensy36",
    "Embedded/Teensy/ARM/Teensy40",
    "Embedded/Teensy/ARM/Teensy41",
    "Embedded/Teensy/ARM/TeensyMM"
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
    static const HostPlatform NativePlatform = HostPlatform::WasmEmscripten;
#elif defined(__wasi__)
    static const HostPlatform NativePlatform = HostPlatform::WasmWasi;
#else
    #error Unsupported platform
#endif

enum class HostArchitecture {
    x86,
    x86_64,
    ARM32,
    ARM64,
    RISCV64,
    Loong64,
    Web32,
    Unknown
};
static const char *const HostArchitectureNames[] = {
    "x86",
    "x86_64",
    "ARM32",
    "ARM64",
    "RISCV64",
    "Loong64",
    "Web32",
    "Unknown"
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
#elif defined(__loongarch64)
    static const HostArchitecture NativeArchitecture = HostArchitecture::Loong64;
#else
    #error Unsupported architecture
#endif

enum class CompileFeature {
    PCH = 1 << 0,
    DistCC = 1 << 1,
    Ccache = 1 << 2,
    Warnings = 1 << 3,
    DebugInfo = 1 << 4,
    Optimize = 1 << 5,
    MinimizeSize = 1 << 6,
    HotAssets = 1 << 7,
    MaxCompression = 1 << 8,
    ASan = 1 << 9,
    TSan = 1 << 10,
    UBSan = 1 << 11,
    LTO = 1 << 12,
    SafeStack = 1 << 13,
    ZeroInit = 1 << 14,
    CFI = 1 << 15,
    ShuffleCode = 1 << 16,
    StaticRuntime = 1 << 17,
    LinkLibrary = 1 << 18,
    NoConsole = 1 << 19,
    AESNI = 1 << 20,
    AVX2 = 1 << 21,
    AVX512 = 1 << 22,
    ESM = 1 << 23
};
static const OptionDesc CompileFeatureOptions[] = {
    {"PCH",            "Use precompiled headers for faster compilation"},
    {"DistCC",         "Use distcc for distributed compilation (must be in PATH)"},
    {"Ccache",         "Use ccache accelerator (must be in PATH)"},
    {"Warnings",       "Enable compiler warnings"},
    {"DebugInfo",      "Add debug information to generated binaries"},
    {"Optimize",       "Optimize generated builds"},
    {"MinimizeSize",   "Prefer small size over bigger code"},
    {"HotAssets",      "Embed assets in reloadable shared library"},
    {"MaxCompression", "Maximize compression of assets (slow)"},
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
    {"AESNI",          "Enable AES-NI generation and instrinsics (x86 and x86_64)"},
    {"AVX2",           "Enable AVX2 generation and instrinsics (x86_64)"},
    {"AVX512",         "Enable AVX512 generation and instrinsics (x86_64)"},
    {"ESM",            "Bundle JS in ESM format instead of IIFE"}
};

enum class SourceType {
    C,
    Cxx,
    GnuAssembly,
    MicrosoftAssembly,
    Object,
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
    HeapArray<ExecuteInfo::KeyValue> env_variables;

    Size cache_len;
    Size rsp_offset;
    int skip_lines;

    DependencyMode deps_mode;
    const char *deps_filename; // Used by MakeLike and EsbuildMeta modes
};

class Compiler {
    K_DELETE_COPY(Compiler)

public:
    HostPlatform platform;
    HostArchitecture architecture;
    const char *name;
    char title[64];

    virtual ~Compiler() {}

    virtual bool CheckFeatures(uint32_t features, uint32_t maybe_features, uint32_t *out_features) const = 0;
    virtual bool CanAssemble(SourceType type) const = 0;

    virtual const char *GetObjectExtension() const = 0;
    virtual const char *GetLinkExtension(TargetType type) const = 0;
    virtual const char *GetImportExtension() const = 0;
    virtual const char *GetLibPrefix() const = 0;
    virtual const char *GetArchiveExtension() const = 0;
    virtual const char *GetPostExtension(TargetType type) const = 0;

    virtual bool GetCore(Span<const char *const> definitions, Allocator *alloc, const char **out_ns,
                         HeapArray<const char *> *out_filenames, HeapArray<const char *> *out_definitions) const = 0;

    virtual void MakeEmbedCommand(Span<const char *const> embed_filenames, const char *embed_options,
                                  uint32_t features, const char *dest_filename,
                                  Allocator *alloc, Command *out_cmd) const = 0;

    virtual void MakePchCommand(const char *pch_filename, SourceType src_type,
                                Span<const char *const> definitions,
                                Span<const char *const> include_directories, Span<const char *const> include_files,
                                const char *custom_flags, uint32_t features, Allocator *alloc, Command *out_cmd) const = 0;
    virtual const char *GetPchCache(const char *pch_filename, Allocator *alloc) const = 0;
    virtual const char *GetPchObject(const char *pch_filename, Allocator *alloc) const = 0;

    virtual void MakeCppCommand(const char *src_filename, SourceType src_type,
                                const char *pch_filename, Span<const char *const> definitions,
                                Span<const char *const> include_directories,
                                Span<const char *const> system_directories,
                                Span<const char *const> include_files,
                                const char *custom_flags, uint32_t features, const char *dest_filename,
                                Allocator *alloc, Command *out_cmd) const = 0;
    virtual void MakeAssemblyCommand(const char *src_filename, Span<const char *const> definitions,
                                     Span<const char *const> include_directories, const char *custom_flags,
                                     uint32_t features, const char *dest_filename,
                                     Allocator *alloc, Command *out_cmd) const = 0;
    virtual void MakeResourceCommand(const char *rc_filename, const char *dest_filename,
                                     Allocator *alloc, Command *out_cmd) const = 0;

    virtual void MakeLinkCommand(Span<const char *const> obj_filenames,
                                 Span<const char *const> libraries, TargetType link_type,
                                 const char *custom_flags, uint32_t features, const char *dest_filename,
                                 Allocator *alloc, Command *out_cmd) const = 0;
    virtual void MakePostCommand(const char *src_filename, const char *dest_filename,
                                 Allocator *alloc, Command *out_cmd) const = 0;

protected:
    Compiler(HostPlatform platform, HostArchitecture architecture, const char *name)
        : platform(platform), architecture(architecture), name(name)
    {
        CopyString(name, title);
    }
};

struct KnownCompiler {
    const char *name;
    const char *cc;
    bool supported;
};

struct HostSpecifier {
    HostPlatform platform = NativePlatform;
    HostArchitecture architecture = HostArchitecture::Unknown;
    const char *cc = nullptr;
    const char *ld = nullptr;
};

extern const Span<const KnownCompiler> KnownCompilers;

std::unique_ptr<const Compiler> PrepareCompiler(HostSpecifier spec);

bool DetermineSourceType(const char *filename, SourceType *out_type = nullptr);

}
