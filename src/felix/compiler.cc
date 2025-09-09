// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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
#include "compiler.hh"
#include "locate.hh"

namespace K {

enum class EmbedMode {
    Arrays,
    Literals,
    Embed
};

static bool SplitPrefixSuffix(const char *binary, const char *needle,
                              Span<const char> *out_prefix, Span<const char> *out_suffix,
                              Span<const char> *out_version)
{
    const char *ptr = strstr(binary, needle);
    if (!ptr) {
        LogError("Compiler binary path must contain '%1'", needle);
        return false;
    }

    *out_prefix = MakeSpan(binary, ptr - binary);
    *out_suffix = ptr + strlen(needle);

    if (out_suffix->ptr[0] == '-' && std::all_of(out_suffix->ptr + 1, out_suffix->end(), IsAsciiDigit)) {
        *out_version = *out_suffix;
    } else {
        *out_version = {};
    }

    return true;
}

static void MakeEmbedCommand(Span<const char *const> embed_filenames, EmbedMode mode,
                             const char *embed_options, const char *dest_filename,
                             Allocator *alloc, Command *out_cmd)
{
    K_ASSERT(alloc);

    HeapArray<char> buf(alloc);

    Fmt(&buf, "\"%1\" embed -O \"%2\"", GetApplicationExecutable(), dest_filename);

    switch (mode) {
        case EmbedMode::Arrays: {} break;
        case EmbedMode::Literals: { Fmt(&buf, " -fUseLiterals"); } break;
        case EmbedMode::Embed: { Fmt(&buf, " -fUseEmbed"); } break;
    }
    if (embed_options) {
        Fmt(&buf, " %1", embed_options);
    }

    for (const char *embed_filename: embed_filenames) {
        Fmt(&buf, " \"%1\"", embed_filename);
    }

    out_cmd->cache_len = buf.len;
    out_cmd->cmd_line = buf.TrimAndLeak(1);
}

static int ParseVersion(const char *cmd, Span<const char> output, const char *marker)
{
    Span<const char> remain = output;

    while (remain.len) {
        Span<const char> token = SplitStr(remain, ' ', &remain);

        if (token == marker) {
            int major = 0;
            int minor = 0;
            int patch = 0;

            if (!ParseInt(remain, &major, 0, &remain)) {
                LogError("Unexpected version format returned by '%1'", cmd);
                return -1;
            }
            if(remain[0] == '.') {
                remain = remain.Take(1, remain.len - 1);

                if (!ParseInt(remain, &minor, 0, &remain)) {
                    LogError("Unexpected version format returned by '%1'", cmd);
                    return -1;
                }
            }
            if(remain[0] == '.') {
                remain = remain.Take(1, remain.len - 1);

                if (!ParseInt(remain, &patch, 0, &remain)) {
                    LogError("Unexpected version format returned by '%1'", cmd);
                    return -1;
                }
            }

            int version = major * 10000 + minor * 100 + patch;
            return version;
        }
    }

    return -1;
}

static HostArchitecture ParseTarget(Span<const char> output)
{
    while (output.len) {
        Span<const char> line = SplitStrLine(output, &output);

        Span<const char> value;
        Span<const char> key = TrimStr(SplitStr(line, ':', &value));
        value = TrimStr(value);

        if (!value.len)
            continue;

        if (key == "Target") {
            if (StartsWith(value, "x86_64-") || StartsWith(value, "x86-64-")  || StartsWith(value, "amd64-")) {
                return HostArchitecture::x86_64;
            } else if (StartsWith(value, "i386-") || StartsWith(value, "i486-") || StartsWith(value, "i586-") ||
                       StartsWith(value, "i686-") || StartsWith(value, "x86-")) {
                return HostArchitecture::x86;
            } else if (StartsWith(value, "aarch64-") || StartsWith(value, "arm64-")) {
                return HostArchitecture::ARM64;
            } else if (StartsWith(value, "arm-")) {
                return HostArchitecture::ARM32;
            } else if (StartsWith(value, "riscv64-")) {
                return HostArchitecture::RISCV64;
            } else if (StartsWith(value, "loongarch64-")) {
                return HostArchitecture::Loong64;
            } else if (StartsWith(value, "wasm32-")) {
                return HostArchitecture::Web32;
            } else {
                break;
            }
        }
    }

    return HostArchitecture::Unknown;
}

static bool IdentifyCompiler(const char *bin, const char *needle)
{
    bin = SplitStrReverseAny(bin, K_PATH_SEPARATORS).ptr;

    const char *ptr = strstr(bin, needle);
    Size len = (Size)strlen(needle);

    if (!ptr)
        return false;
    if (ptr != bin && !strchr("_-.", ptr[-1]))
        return false;
    if (ptr[len] && !strchr("_-.", ptr[len]))
        return false;

    return true;
}

static bool DetectCcache() {
    static bool detected = FindExecutableInPath("ccache");
    return detected;
}

static bool DetectDistCC() {
    static bool detected = FindExecutableInPath("distcc") &&
                           (GetEnv("DISTCC_HOSTS") || GetEnv("DISTCC_POTENTIAL_HOSTS"));
    return detected;
}

class ClangCompiler final: public Compiler {
    const char *cc;
    const char *cxx;
    const char *rc;
    const char *ld;

    const char *target = nullptr;
    const char *sysroot = nullptr;

    int clang_ver = 0;
    int lld_ver = 0;

    BlockAllocator str_alloc;

public:
    ClangCompiler(HostPlatform platform, HostArchitecture architecture) : Compiler(platform, architecture, "Clang") {}

    static std::unique_ptr<const Compiler> Create(HostPlatform platform, HostArchitecture architecture,
                                                  const char *cc, const char *ld, const char *sysroot = nullptr)
    {
        std::unique_ptr<ClangCompiler> compiler = std::make_unique<ClangCompiler>(platform, architecture);

        // Prefer LLD
        if (!ld && FindExecutableInPath("ld.lld")) {
            ld = "lld";
        }

        // Find executables
        {
            Span<const char> prefix;
            Span<const char> suffix;
            Span<const char> version;
            if (!SplitPrefixSuffix(cc, "clang", &prefix, &suffix, &version))
                return nullptr;

            compiler->cc = DuplicateString(cc, &compiler->str_alloc).ptr;
            compiler->cxx = Fmt(&compiler->str_alloc, "%1clang++%2", prefix, suffix).ptr;
            compiler->rc = Fmt(&compiler->str_alloc, "%1llvm-rc%2", prefix, version).ptr;
            if (ld) {
                compiler->ld = ld;
            } else if (suffix.len) {
                compiler->ld = Fmt(&compiler->str_alloc, "%1lld%2", prefix, suffix).ptr;
            } else {
                compiler->ld = nullptr;
            }

            compiler->sysroot = sysroot ? DuplicateString(sysroot, &compiler->str_alloc).ptr : nullptr;
        }

        Async async;

        // Determine Clang version and architecture (if needed)
        async.Run([&]() {
            char cmd[2048];
            Fmt(cmd, "\"%1\" --version", compiler->cc);

            HeapArray<char> output;
            if (!ReadCommandOutput(cmd, &output))
                return false;

            compiler->clang_ver = ParseVersion(cmd, output, "version");

            HostArchitecture architecture = ParseTarget(output);

            if (architecture == HostArchitecture::Unknown) {
                LogError("Cannot determine default Clang architecture");
                return false;
            }

            if (compiler->architecture == HostArchitecture::Unknown) {
                compiler->architecture = architecture;
#if defined(_WIN32)
            } else {
                switch (compiler->architecture) {
                    case HostArchitecture::x86: { compiler->target = "-m32"; } break;
                    case HostArchitecture::x86_64: { compiler->target = "-m64"; } break;

                    case HostArchitecture::ARM64:
                    case HostArchitecture::RISCV64:
                    case HostArchitecture::Loong64:
                    case HostArchitecture::ARM32:
                    case HostArchitecture::Web32: {
                        LogError("Cannot use Clang (Windows) to build for '%1'", HostArchitectureNames[(int)compiler->architecture]);
                        return false;
                    } break;

                    case HostArchitecture::Unknown: { K_UNREACHABLE(); } break;
                }
#elif !defined(__APPLE_)
            } else {
                const char *prefix = nullptr;
                const char *suffix = nullptr;

                switch (compiler->architecture)  {
                    case HostArchitecture::x86: { prefix = "i386"; } break;
                    case HostArchitecture::x86_64: { prefix = "x86_64"; } break;
                    case HostArchitecture::ARM64: { prefix = "aarch64"; } break;
                    case HostArchitecture::RISCV64: { prefix = "riscv64"; } break;
                    case HostArchitecture::Loong64: { prefix = "loongarch64"; } break;
                    case HostArchitecture::Web32: { prefix = "wasm32"; } break;

                    case HostArchitecture::ARM32: {
                        LogError("Cannot use Clang to build for '%1'", HostArchitectureNames[(int)compiler->architecture]);
                        return false;
                    } break;

                    case HostArchitecture::Unknown: { K_UNREACHABLE(); } break;
                }

                switch (compiler->platform) {
                    case HostPlatform::Linux: { suffix = "pc-linux-gnu"; } break;
                    case HostPlatform::FreeBSD: { suffix = "freebsd-unknown"; } break;
                    case HostPlatform::OpenBSD: { suffix = "openbsd-unknown"; } break;

                    case HostPlatform::WasmWasi: {
                        K_ASSERT(sysroot);
                        suffix = "wasi";
                    } break;

                    default: {
                        LogError("Cannot use Clang to build for '%1'", HostPlatformNames[(int)compiler->platform]);
                        return false;
                    } break;
                }

                compiler->target = Fmt(&compiler->str_alloc, "--target=%1-%2", prefix, suffix).ptr;
#else
            } else if (compiler->architecture != architecture) {
                LogError("Cannot use Clang (%1) compiler to build for '%2'",
                         HostArchitectureNames[(int)architecture], HostArchitectureNames[(int)compiler->architecture]);
                return false;
#endif
            }

            return true;
        });

        // Determine LLD version
        async.Run([&]() {
            if (compiler->ld && IdentifyCompiler(compiler->ld, "lld")) {
                char cmd[4096];
                if (PathIsAbsolute(compiler->ld)) {
                    Fmt(cmd, "\"%1\" --version", compiler->ld);
                } else {
#if defined(_WIN32)
                    Fmt(cmd, "\"%1-link\" --version", compiler->ld);
#else
                    Fmt(cmd, "\"ld.%1\" --version", compiler->ld);
#endif
                }

                HeapArray<char> output;
                if (ReadCommandOutput(cmd, &output)) {
                    compiler->lld_ver = ParseVersion(cmd, output, "LLD");
                }
            }

            return true;
        });

        if (!async.Sync())
            return nullptr;

        Fmt(compiler->title, "%1 %2", compiler->name, FmtVersion(compiler->clang_ver, 3, 100));

        return compiler;
    }

    bool CheckFeatures(uint32_t features, uint32_t maybe_features, uint32_t *out_features) const override
    {
        uint32_t supported = 0;

        supported |= (int)CompileFeature::Optimize;
        supported |= (int)CompileFeature::MinimizeSize;
        if (DetectCcache()) {
            supported |= (int)CompileFeature::Ccache;
        }
        if (DetectDistCC()) {
            supported |= (int)CompileFeature::DistCC;
        }
        if (platform != HostPlatform::WasmWasi) {
            supported |= (int)CompileFeature::HotAssets;
        }
        supported |= (int)CompileFeature::PCH;
        supported |= (int)CompileFeature::Warnings;
        supported |= (int)CompileFeature::DebugInfo;
        if (platform != HostPlatform::WasmWasi) {
            supported |= (int)CompileFeature::ASan;
            supported |= (int)CompileFeature::UBSan;
            supported |= (int)CompileFeature::LTO;
        }
        supported |= (int)CompileFeature::ZeroInit;
        if (platform != HostPlatform::WasmWasi) {
            if (clang_ver >= 130000 && platform != HostPlatform::OpenBSD) {
                supported |= (int)CompileFeature::CFI; // LTO only
            }
            if (platform != HostPlatform::Windows) {
                supported |= (int)CompileFeature::TSan;
                supported |= (int)CompileFeature::ShuffleCode; // Requires lld version >= 11
            }
            if (platform == HostPlatform::Linux) {
                if (architecture == HostArchitecture::x86_64) {
                    supported |= (int)CompileFeature::SafeStack;
                } else if (architecture == HostArchitecture::ARM64) {
                    supported |= (int)CompileFeature::SafeStack;
                }
            }
            supported |= (int)CompileFeature::StaticRuntime;
            supported |= (int)CompileFeature::LinkLibrary;
            if (platform == HostPlatform::Windows) {
                supported |= (int)CompileFeature::NoConsole;
            }
        }

        supported |= (int)CompileFeature::AESNI;
        supported |= (int)CompileFeature::AVX2;
        supported |= (int)CompileFeature::AVX512;

        uint32_t unsupported = features & ~supported;
        if (unsupported) {
            LogError("Some features are not supported by %1: %2",
                     name, FmtFlags(unsupported, CompileFeatureOptions));
            return false;
        }

        features |= (supported & maybe_features);

        if ((features & (int)CompileFeature::MinimizeSize) && !(features & (int)CompileFeature::Optimize)) {
            LogError("Cannot use MinimizeSize without Optimize feature");
            return false;
        }
        if ((features & (int)CompileFeature::ASan) && (features & (int)CompileFeature::TSan)) {
            LogError("Cannot use ASan and TSan at the same time");
            return false;
        }
        if (!(features & (int)CompileFeature::LTO) && (features & (int)CompileFeature::CFI)) {
            LogError("Clang CFI feature requires LTO compilation");
            return false;
        }
        if (lld_ver < 110000 && (features & (int)CompileFeature::ShuffleCode)) {
            LogError("ShuffleCode requires LLD >= 11, try --host option (e.g. --host=:clang-11:lld-11)");
            return false;
        }

        *out_features = features;
        return true;
    }
    bool CanAssemble(SourceType type) const override { return (type == SourceType::GnuAssembly); }

    const char *GetObjectExtension() const override { return (platform == HostPlatform::Windows) ? ".obj" : ".o"; }
    const char *GetLinkExtension(TargetType type) const override
    {
        if (platform == HostPlatform::WasmWasi) {
            K_ASSERT(type != TargetType::Library);
            return ".wasm";
        }

        switch (type) {
            case TargetType::Executable: {
                const char *ext = (platform == HostPlatform::Windows) ? ".exe" : "";
                return ext;
            } break;
            case TargetType::Library: {
                const char *ext = (platform == HostPlatform::Windows) ? ".dll" : ".so";
                return ext;
            } break;
        }

        K_UNREACHABLE();
    }
    const char *GetImportExtension() const override { return (platform == HostPlatform::Windows) ? ".lib" : ".so"; }
    const char *GetLibPrefix() const override { return (platform == HostPlatform::Windows) ? "" : "lib"; }
    const char *GetArchiveExtension() const override { return (platform == HostPlatform::Windows) ? ".lib" : ".a"; }
    const char *GetPostExtension(TargetType) const override { return nullptr; }

    bool GetCore(Span<const char *const>, Allocator *, const char **out_name,
                 HeapArray<const char *> *, HeapArray<const char *> *) const override
    {
        *out_name = nullptr;
        return true;
    }

    void MakeEmbedCommand(Span<const char *const> embed_filenames,
                          const char *embed_options, const char *dest_filename,
                          Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        EmbedMode mode = (clang_ver >= 190000) ? EmbedMode::Embed : EmbedMode::Literals;
        K::MakeEmbedCommand(embed_filenames, mode, embed_options, dest_filename, alloc, out_cmd);
    }

    void MakePchCommand(const char *pch_filename, SourceType src_type,
                        Span<const char *const> definitions, Span<const char *const> include_directories,
                        Span<const char *const> include_files, const char *custom_flags, uint32_t features,
                        Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);
        MakeCppCommand(pch_filename, src_type, nullptr, definitions, include_directories, {},
                       include_files, custom_flags, features, nullptr, alloc, out_cmd);
    }

    const char *GetPchCache(const char *pch_filename, Allocator *alloc) const override
    {
        K_ASSERT(alloc);

        const char *cache_filename = Fmt(alloc, "%1.pch", pch_filename).ptr;
        return cache_filename;
    }
    const char *GetPchObject(const char *, Allocator *) const override { return nullptr; }

    void MakeCppCommand(const char *src_filename, SourceType src_type,
                        const char *pch_filename, Span<const char *const> definitions,
                        Span<const char *const> include_directories, Span<const char *const> system_directories,
                        Span<const char *const> include_files, const char *custom_flags, uint32_t features,
                        const char *dest_filename, Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        if (features & (int)CompileFeature::Ccache) {
            Fmt(&buf, "ccache ");

            out_cmd->env_variables.Append({ "CCACHE_DEPEND", "1" });
            out_cmd->env_variables.Append({ "CCACHE_SLOPPINESS", "pch_defines,time_macros,include_file_ctime,include_file_mtime" });
            if (dest_filename && (features & (int)CompileFeature::DistCC)) {
                out_cmd->env_variables.Append({ "CCACHE_PREFIX", "distcc" });
            }
        } else if (dest_filename && (features & (int)CompileFeature::DistCC)) {
            Fmt(&buf, "distcc ");
        }

        // Compiler
        switch (src_type) {
            case SourceType::C: { Fmt(&buf, "\"%1\" -std=gnu11", cc); } break;
            case SourceType::Cxx: {
                int std = (clang_ver >= 160000) ? 20 : 17;
                Fmt(&buf, "\"%1\" -std=gnu++%2", cxx, std);
            } break;

            case SourceType::GnuAssembly:
            case SourceType::MicrosoftAssembly:
            case SourceType::Object:
            case SourceType::Esbuild:
            case SourceType::QtUi:
            case SourceType::QtResources: { K_UNREACHABLE(); } break;
        }
        if (dest_filename) {
            Fmt(&buf, " -o \"%1\"", dest_filename);
        } else {
            switch (src_type) {
                case SourceType::C: { Fmt(&buf, " -x c-header -Xclang -fno-pch-timestamp -o \"%1.pch\"", src_filename); } break;
                case SourceType::Cxx: { Fmt(&buf, " -x c++-header -Xclang -fno-pch-timestamp -o \"%1.pch\"", src_filename); } break;

                case SourceType::GnuAssembly:
                case SourceType::MicrosoftAssembly:
                case SourceType::Object:
                case SourceType::Esbuild:
                case SourceType::QtUi:
                case SourceType::QtResources: { K_UNREACHABLE(); } break;
            }
        }
        Fmt(&buf, " -MD -MF \"%1.d\"", dest_filename ? dest_filename : src_filename);
        out_cmd->rsp_offset = buf.len;

        // Cross-compilation
        AddClangTarget(&buf);

        // Build options
        Fmt(&buf, " -I. -fvisibility=hidden -fno-strict-aliasing -fno-delete-null-pointer-checks -fno-omit-frame-pointer");
        Fmt(&buf, " -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free");
        if (clang_ver >= 130000) {
            Fmt(&buf, " -fno-finite-loops");
        }
        if (features & (int)CompileFeature::MinimizeSize) {
            Fmt(&buf, " -Os -fwrapv -DNDEBUG -ffunction-sections -fdata-sections");
        } else if (features & (int)CompileFeature::Optimize) {
            const char *level = (clang_ver >= 170000) ? "-O3" : "-O2";
            Fmt(&buf, " %1 -fwrapv -DNDEBUG", level);
        } else {
            Fmt(&buf, " -O0 -ftrapv");
        }
        if (features & (int)CompileFeature::LTO) {
            Fmt(&buf, " -flto");
        }
        if (features & (int)CompileFeature::Warnings) {
            Fmt(&buf, " -Wall -Wextra -Wswitch -Wuninitialized -Wno-unknown-warning-option");
            if (src_type == SourceType::Cxx) {
                Fmt(&buf, " -Wzero-as-null-pointer-constant");
            }
            Fmt(&buf, " -Wreturn-type -Werror=return-type");

            // Accept #embed without reserve
            if (clang_ver >= 190000) {
                Fmt(&buf, " -Wno-c23-extensions");
            }
        } else {
            Fmt(&buf, " -Wno-everything");
        }
        if (features & (int)CompileFeature::HotAssets) {
            Fmt(&buf, " -DFELIX_HOT_ASSETS");
        }

        // Include build directory (for generated files)
        Span<const char> dest_directory = GetPathDirectory(dest_filename);
        Fmt(&buf, " \"-I%1\"", dest_directory);

        switch (architecture) {
            case HostArchitecture::x86_64: {
                Fmt(&buf, " -mpopcnt -msse4.1 -msse4.2 -mssse3 -mcx16");

                if (features & (int)CompileFeature::AESNI) {
                    Fmt(&buf, " -maes -mpclmul");
                }
                if (features & (int)CompileFeature::AVX2) {
                    Fmt(&buf, " -mavx2");
                }
                if (features & (int)CompileFeature::AVX512) {
                    Fmt(&buf, " -mavx512f -mavx512vl");
                }
            } break;

            case HostArchitecture::x86: {
                Fmt(&buf, " -msse2");

                if (features & (int)CompileFeature::AESNI) {
                    Fmt(&buf, " -maes -mpclmul");
                }
            } break;

            case HostArchitecture::Web32: {
                Fmt(&buf, " -mbulk-memory");
            } break;

            case HostArchitecture::ARM32:
            case HostArchitecture::ARM64:
            case HostArchitecture::RISCV64:
            case HostArchitecture::Loong64: {} break;

            case HostArchitecture::Unknown: { K_UNREACHABLE(); } break;
        }

        // Platform flags
        switch (platform) {
            case HostPlatform::Windows: {
                Fmt(&buf, " -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE"
                          " -D_MT -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_VC_NODEFAULTLIB"
                          " -Wno-unknown-pragmas -Wno-deprecated-declarations");
            } break;

            case HostPlatform::macOS: {
                Fmt(&buf, " -pthread -fPIC");
            } break;

            case HostPlatform::Linux: {
                Fmt(&buf, " -pthread -fPIC -D_FILE_OFFSET_BITS=64 -D_GLIBCXX_ASSERTIONS");

                if (clang_ver >= 110000) {
                    Fmt(&buf, " -fno-semantic-interposition");
                }

                if (features & (int)CompileFeature::Optimize) {
                    Fmt(&buf, " -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=%1", clang_ver >= 170100 ? 3 : 2);
                } else {
                    Fmt(&buf, " -D_GLIBCXX_DEBUG -D_GLIBCXX_SANITIZE_VECTOR");
                }
            } break;

            case HostPlatform::WasmWasi: {
                Fmt(&buf, " -fno-exceptions");
                // --target is handled elsewhere
            } break;

            default: {
                Fmt(&buf, " -pthread -fPIC -D_FILE_OFFSET_BITS=64");

                if (clang_ver >= 110000) {
                    Fmt(&buf, " -fno-semantic-interposition");
                }

                if (features & (int)CompileFeature::Optimize) {
                    Fmt(&buf, " -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2");
                }
            } break;
        }

        // Features
        if (features & (int)CompileFeature::DebugInfo) {
            Fmt(&buf, " -g");
        }
        if (platform == HostPlatform::Windows) {
            if (features & (int)CompileFeature::StaticRuntime) {
                if (src_type == SourceType::Cxx) {
                    Fmt(&buf, " -Xclang -flto-visibility-public-std -D_SILENCE_CLANG_CONCEPTS_MESSAGE");
                }
            } else {
                Fmt(&buf, " -D_DLL");
            }
        }
        if (features & (int)CompileFeature::ASan) {
            Fmt(&buf, " -fsanitize=address");
        }
        if (features & (int)CompileFeature::TSan) {
            Fmt(&buf, " -fsanitize=thread");
        }
        if (features & (int)CompileFeature::UBSan) {
            Fmt(&buf, " -fsanitize=undefined");
        }
        Fmt(&buf, " -fstack-protector-strong --param ssp-buffer-size=4");
        if (platform == HostPlatform::Linux && clang_ver >= 110000) {
            Fmt(&buf, " -fstack-clash-protection");
        }
        if (features & (int)CompileFeature::SafeStack) {
            Fmt(&buf, " -fsanitize=safe-stack");
        }
        if (features & (int)CompileFeature::ZeroInit) {
            Fmt(&buf, " -ftrivial-auto-var-init=zero");

            if (clang_ver < 160000) {
                Fmt(&buf, " -enable-trivial-auto-var-init-zero-knowing-it-will-be-removed-from-clang");
            }
        }
        if (features & (int)CompileFeature::CFI) {
            K_ASSERT(features & (int)CompileFeature::LTO);

            if (clang_ver >= 160000) {
                if (architecture == HostArchitecture::x86_64) {
                    Fmt(&buf, " -fcf-protection=full");
                } else if (architecture == HostArchitecture::ARM64) {
                    Fmt(&buf, " -mbranch-protection=bti+pac-ret");
                }
            }

            // Fine-grained forward CFI
            Fmt(&buf, " -fsanitize=cfi");
        }
        if (features & (int)CompileFeature::ShuffleCode) {
            Fmt(&buf, " -ffunction-sections -fdata-sections");
        }

        // Sources and definitions
        Fmt(&buf, " -DFELIX -c \"%1\"", src_filename);
        if (pch_filename) {
            Fmt(&buf, " -include-pch \"%1.pch\"", pch_filename);
        }
        for (const char *definition: definitions) {
            Fmt(&buf, " \"-%1%2\"", definition[0] != '-' ? 'D' : 'U', definition);
        }
        for (const char *include_directory: include_directories) {
            Fmt(&buf, " \"-I%1\"", include_directory);
        }
        for (const char *system_directory: system_directories) {
            Fmt(&buf, " -isystem \"%1\"", system_directory);
        }
        for (const char *include_file: include_files) {
            Fmt(&buf, " -include \"%1\"", include_file);
        }

        if (custom_flags) {
            Fmt(&buf, " %1", custom_flags);
        }

        out_cmd->cache_len = buf.len;
        if (FileIsVt100(STDOUT_FILENO)) {
            Fmt(&buf, " -fcolor-diagnostics -fansi-escape-codes");
        } else {
            Fmt(&buf, " -fno-color-diagnostics");
        }
        out_cmd->cmd_line = buf.TrimAndLeak(1);

        // Dependencies
        out_cmd->deps_mode = Command::DependencyMode::MakeLike;
        out_cmd->deps_filename = Fmt(alloc, "%1.d", dest_filename ? dest_filename : src_filename).ptr;
    }

    void MakeAssemblyCommand(const char *src_filename, Span<const char *const> definitions,
                             Span<const char *const> include_directories, const char *custom_flags,
                             uint32_t features, const char *dest_filename,
                             Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Compiler
        Fmt(&buf, "\"%1\" -o \"%2\"", cc, dest_filename);
        out_cmd->rsp_offset = buf.len;

        // Cross-compilation
        AddClangTarget(&buf);

        // Build options
        Fmt(&buf, " -I.");
        if (features & ((int)CompileFeature::MinimizeSize | (int)CompileFeature::Optimize)) {
            Fmt(&buf, " -DNDEBUG");
        }

        // Include build directory (for generated files)
        Span<const char> dest_directory = GetPathDirectory(dest_filename);
        Fmt(&buf, " \"-I%1\"", dest_directory);

        // Sources and definitions
        Fmt(&buf, " -DFELIX -c \"%1\"", src_filename);
        for (const char *definition: definitions) {
            Fmt(&buf, " \"-%1%2\"", definition[0] != '-' ? 'D' : 'U', definition);
        }
        for (const char *include_directory: include_directories) {
            Fmt(&buf, " \"-I%1\"", include_directory);
        }

        if (custom_flags) {
            Fmt(&buf, " %1", custom_flags);
        }

        out_cmd->cache_len = buf.len;
        if (FileIsVt100(STDOUT_FILENO)) {
            Fmt(&buf, " -fcolor-diagnostics -fansi-escape-codes");
        } else {
            Fmt(&buf, " -fno-color-diagnostics");
        }
        out_cmd->cmd_line = buf.TrimAndLeak(1);

        // Dependencies
        out_cmd->deps_mode = Command::DependencyMode::MakeLike;
        out_cmd->deps_filename = Fmt(alloc, "%1.d", dest_filename ? dest_filename : src_filename).ptr;
    }

    void MakeResourceCommand(const char *rc_filename, const char *dest_filename,
                             Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        out_cmd->cmd_line = Fmt(alloc, "\"%1\" /FO\"%2\" \"%3\"", rc, dest_filename, rc_filename);
        out_cmd->cache_len = out_cmd->cmd_line.len;
    }

    void MakeLinkCommand(Span<const char *const> obj_filenames,
                         Span<const char *const> libraries, TargetType link_type,
                         const char *custom_flags, uint32_t features, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
            case TargetType::Executable: {
                bool link_static = (features & (int)CompileFeature::StaticRuntime);
                Fmt(&buf, "\"%1\"%2", cxx, link_static ? " -static" : "");
            } break;
            case TargetType::Library: { Fmt(&buf, "\"%1\" -shared", cxx); } break;
        }
        Fmt(&buf, " -o \"%1\"", dest_filename);
        out_cmd->rsp_offset = buf.len;

        // Cross-compilation
        AddClangTarget(&buf);

        // Build mode
        if (!(features & (int)CompileFeature::DebugInfo)) {
            Fmt(&buf, " -s");
        }
        if (features & (int)CompileFeature::LTO) {
            Fmt(&buf, " -flto");

            if (platform != HostPlatform::Windows) {
                Fmt(&buf, " -Wl,-O1");
            }
        }

        // Objects and libraries
        for (const char *obj_filename: obj_filenames) {
            Fmt(&buf, " \"%1\"", obj_filename);
        }
        if (libraries.len) {
            HashSet<Span<const char>> framework_paths;

            if (platform != HostPlatform::Windows && platform != HostPlatform::macOS &&
                    platform != HostPlatform::WasmWasi) {
                Fmt(&buf, " -Wl,--start-group");
            }
            for (const char *lib: libraries) {
                if (platform == HostPlatform::macOS && lib[0] == '@') {
                    Span<const char> directory = {};
                    Span<const char> basename = SplitStrReverse(lib + 1, '/', &directory);

                    if (EndsWith(basename, ".framework")) {
                        basename = basename.Take(0, basename.len - 10);
                    }

                    if (directory.len) {
                        bool inserted = false;
                        framework_paths.TrySet(directory, &inserted);

                        if (inserted) {
                            Fmt(&buf, " -F \"%1\"", directory);
                        }
                    }
                    Fmt(&buf, " -framework %1", basename);
                } else if (strpbrk(lib, K_PATH_SEPARATORS)) {
                    Fmt(&buf, " %1", lib);
                } else {
                    Fmt(&buf, " -l%1", lib);
                }
            }
            if (platform != HostPlatform::Windows && platform != HostPlatform::macOS &&
                    platform != HostPlatform::WasmWasi) {
                Fmt(&buf, " -Wl,--end-group");
            }
        }

        // Platform flags
        switch (platform) {
            case HostPlatform::Windows: {
                const char *suffix = (features & (int)CompileFeature::Optimize) ? "" : "d";

                Fmt(&buf, " -Wl,/NODEFAULTLIB:libcmt -Wl,/NODEFAULTLIB:msvcrt -Wl,setargv.obj -Wl,oldnames.lib");
                Fmt(&buf, " -Wl,/OPT:ref");

                if (features & (int)CompileFeature::StaticRuntime) {
                    Fmt(&buf, " -Wl,libcmt%1.lib", suffix);
                } else {
                    Fmt(&buf, " -Wl,msvcrt%1.lib", suffix);
                }

                if (features & (int)CompileFeature::DebugInfo) {
                    Fmt(&buf, " -g");
                }
            } break;

            case HostPlatform::macOS: {
                Fmt(&buf, " -ldl -pthread -framework CoreFoundation -framework SystemConfiguration ");
                Fmt(&buf, " -Wl,-dead_strip -rpath \"@executable_path/../Frameworks\"");
            } break;

            case HostPlatform::WasmWasi: { /* --target is handled elsewhere */ } break;

            default: {
                Fmt(&buf, " -pthread -Wl,-z,relro,-z,now,-z,noexecstack,-z,separate-code,-z,stack-size=1048576");

                if (lld_ver) {
                    if (lld_ver >= 130000) {
                        // The second flag is needed to fix undefined __start_/__stop_ symbols related to --gc-sections
                        Fmt(&buf, "  -Wl,--gc-sections -z nostart-stop-gc");
                    }
                } else {
                    Fmt(&buf, " -Wl,--gc-sections");
                }

                if (platform == HostPlatform::Linux) {
                    Fmt(&buf, "  -static-libgcc -static-libstdc++ -ldl -lrt");
                }
                if (link_type == TargetType::Executable) {
                    Fmt(&buf, " -pie");
                }
                if (architecture == HostArchitecture::ARM32) {
                    Fmt(&buf, " -latomic");
                }
            } break;
        }

        // Features
        if (features & (int)CompileFeature::ASan) {
            Fmt(&buf, " -fsanitize=address");
            if (platform == HostPlatform::Windows && !(features & (int)CompileFeature::StaticRuntime)) {
                Fmt(&buf, " -shared-libasan");
            }
        }
        if (features & (int)CompileFeature::TSan) {
            Fmt(&buf, " -fsanitize=thread");
        }
        if (features & (int)CompileFeature::UBSan) {
            Fmt(&buf, " -fsanitize=undefined");
        }
        if (features & (int)CompileFeature::SafeStack) {
            Fmt(&buf, " -fsanitize=safe-stack");
        }
        if (features & (int)CompileFeature::CFI) {
            K_ASSERT(features & (int)CompileFeature::LTO);
            Fmt(&buf, " -fsanitize=cfi");
        }
        if (features & (int)CompileFeature::ShuffleCode) {
            if (lld_ver >= 130000) {
                Fmt(&buf, " -Wl,--shuffle-sections=*=0");
            } else {
                Fmt(&buf, " -Wl,--shuffle-sections=0");
            }
        }
        if (features & (int)CompileFeature::NoConsole) {
            Fmt(&buf, " -Wl,/subsystem:windows, -Wl,/entry:mainCRTStartup");
        }

        if (ld) {
            Fmt(&buf, " -fuse-ld=%1", ld);
        }
        if (custom_flags) {
            Fmt(&buf, " %1", custom_flags);
        }

        out_cmd->cache_len = buf.len;
        if (FileIsVt100(STDOUT_FILENO)) {
            Fmt(&buf, " -fcolor-diagnostics -fansi-escape-codes");
        } else {
            Fmt(&buf, " -fno-color-diagnostics");
        }
        out_cmd->cmd_line = buf.TrimAndLeak(1);
    }

    void MakePostCommand(const char *, const char *, Allocator *, Command *) const override { K_UNREACHABLE(); }

private:
    void AddClangTarget(HeapArray<char> *out_buf) const
    {
        if (target) {
            Fmt(out_buf, " %1", target);
        }

        if (sysroot) {
            Fmt(out_buf, " --sysroot=%1", sysroot);
        }
    }
};

class GnuCompiler final: public Compiler {
    const char *cc;
    const char *cxx;
    const char *windres;
    const char *ld;

    int gcc_ver = 0;
    bool m32 = false;

    BlockAllocator str_alloc;

public:
    GnuCompiler(HostPlatform platform, HostArchitecture architecture) : Compiler(platform, architecture, "GCC") {}

    static std::unique_ptr<const Compiler> Create(HostPlatform platform, HostArchitecture architecture, const char *cc, const char *ld)
    {
        std::unique_ptr<GnuCompiler> compiler = std::make_unique<GnuCompiler>(platform, architecture);

        // Find executables
        {
            Span<const char> prefix;
            Span<const char> suffix;
            Span<const char> version;
            if (!SplitPrefixSuffix(cc, "gcc", &prefix, &suffix, &version))
                return nullptr;

            compiler->cc = DuplicateString(cc, &compiler->str_alloc).ptr;
            compiler->cxx = Fmt(&compiler->str_alloc, "%1g++%2", prefix, suffix).ptr;
            compiler->windres = Fmt(&compiler->str_alloc, "%1windres%2", prefix, version).ptr;
            compiler->ld = ld ? DuplicateString(ld, &compiler->str_alloc).ptr : nullptr;
        }

        // Determine GCC version
        {
            char cmd[2048];
            Fmt(cmd, "\"%1\" -v", compiler->cc);

            HeapArray<char> output;
            if (!ReadCommandOutput(cmd, &output))
                return nullptr;

            compiler->gcc_ver = ParseVersion(cmd, output, "version");

            HostArchitecture architecture = ParseTarget(output);

            if (architecture == HostArchitecture::Unknown) {
                LogError("Cannot determine default GCC architecture");
                return nullptr;
            }

            if (compiler->architecture == HostArchitecture::Unknown) {
                compiler->architecture = architecture;
#if defined(__x86_64__)
            } else if (architecture == HostArchitecture::x86_64 &&
                       compiler->architecture == HostArchitecture::x86) {
                compiler->m32 = true;
#endif
            } else if (compiler->architecture != architecture) {
                LogError("Cannot use GCC (%1) compiler to build for '%2'",
                         HostArchitectureNames[(int)architecture], HostArchitectureNames[(int)compiler->architecture]);
                return nullptr;
            }
        };

        Fmt(compiler->title, "%1 %2", compiler->name, FmtVersion(compiler->gcc_ver, 3, 100));

        return compiler;
    }

    bool CheckFeatures(uint32_t features, uint32_t maybe_features, uint32_t *out_features) const override
    {
        uint32_t supported = 0;

        supported |= (int)CompileFeature::Optimize;
        supported |= (int)CompileFeature::MinimizeSize;
        if (DetectCcache()) {
            supported |= (int)CompileFeature::Ccache;
        }
        if (DetectDistCC()) {
            supported |= (int)CompileFeature::DistCC;
        }
        supported |= (int)CompileFeature::HotAssets;
        supported |= (int)CompileFeature::Warnings;
        supported |= (int)CompileFeature::DebugInfo;
        if (platform != HostPlatform::Windows) {
            // Sometimes it works, somestimes not and the object files are
            // corrupt... just avoid PCH on MinGW
            supported |= (int)CompileFeature::PCH;
            supported |= (int)CompileFeature::ASan;
            supported |= (int)CompileFeature::TSan;
            supported |= (int)CompileFeature::UBSan;
            supported |= (int)CompileFeature::LTO;
        }
        supported |= (int)CompileFeature::ZeroInit;
        if (platform == HostPlatform::Linux) {
            if (architecture == HostArchitecture::x86_64) {
                supported |= (int)CompileFeature::CFI;
            } else if (architecture == HostArchitecture::ARM64 && gcc_ver >= 130000) {
                supported |= (int)CompileFeature::CFI;
            }
        }
        supported |= (int)CompileFeature::StaticRuntime;
        supported |= (int)CompileFeature::LinkLibrary;
        if (platform == HostPlatform::Windows) {
            supported |= (int)CompileFeature::NoConsole;
        }

        supported |= (int)CompileFeature::AESNI;
        supported |= (int)CompileFeature::AVX2;
        supported |= (int)CompileFeature::AVX512;

        uint32_t unsupported = features & ~supported;
        if (unsupported) {
            LogError("Some features are not supported by %1: %2",
                     name, FmtFlags(unsupported, CompileFeatureOptions));
            return false;
        }

        features |= (supported & maybe_features);

        if ((features & (int)CompileFeature::MinimizeSize) && !(features & (int)CompileFeature::Optimize)) {
            LogError("Cannot use MinimizeSize without Optimize feature");
            return false;
        }
        if ((features & (int)CompileFeature::ASan) && (features & (int)CompileFeature::TSan)) {
            LogError("Cannot use ASan and TSan at the same time");
            return false;
        }
        if (gcc_ver < 120100 && (features & (int)CompileFeature::ZeroInit)) {
            LogError("ZeroInit requires GCC >= 12.1, try --host option (e.g. --host=:gcc-12)");
            return false;
        }

        *out_features = features;
        return true;
    }
    bool CanAssemble(SourceType type) const override { return (type == SourceType::GnuAssembly); }

    const char *GetObjectExtension() const override { return ".o"; }
    const char *GetLinkExtension(TargetType type) const override
    {
        switch (type) {
            case TargetType::Executable: {
                const char *ext = (platform == HostPlatform::Windows) ? ".exe" : "";
                return ext;
            } break;
            case TargetType::Library: {
                const char *ext = (platform == HostPlatform::Windows) ? ".dll" : ".so";
                return ext;
            } break;
        }

        K_UNREACHABLE();
    }
    const char *GetImportExtension() const override { return ".so"; }
    const char *GetLibPrefix() const override { return "lib"; }
    const char *GetArchiveExtension() const override { return ".a"; }
    const char *GetPostExtension(TargetType) const override { return nullptr; }

    bool GetCore(Span<const char *const>, Allocator *, const char **out_name,
                 HeapArray<const char *> *, HeapArray<const char *> *) const override
    {
        *out_name = nullptr;
        return true;
    }

    void MakeEmbedCommand(Span<const char *const> embed_filenames,
                          const char *embed_options, const char *dest_filename,
                          Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        EmbedMode mode = (gcc_ver >= 150000) ? EmbedMode::Embed : EmbedMode::Literals;
        K::MakeEmbedCommand(embed_filenames, mode, embed_options, dest_filename, alloc, out_cmd);
    }

    void MakePchCommand(const char *pch_filename, SourceType src_type,
                        Span<const char *const> definitions, Span<const char *const> include_directories,
                        Span<const char *const> include_files, const char *custom_flags, uint32_t features,
                        Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);
        MakeCppCommand(pch_filename, src_type, nullptr, definitions, include_directories, {},
                       include_files, custom_flags, features, nullptr, alloc, out_cmd);
    }

    const char *GetPchCache(const char *pch_filename, Allocator *alloc) const override
    {
        K_ASSERT(alloc);

        const char *cache_filename = Fmt(alloc, "%1.gch", pch_filename).ptr;
        return cache_filename;
    }
    const char *GetPchObject(const char *, Allocator *) const override { return nullptr; }

    void MakeCppCommand(const char *src_filename, SourceType src_type,
                        const char *pch_filename, Span<const char *const> definitions,
                        Span<const char *const> include_directories, Span<const char *const> system_directories,
                        Span<const char *const> include_files, const char *custom_flags, uint32_t features,
                        const char *dest_filename, Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        if (features & (int)CompileFeature::Ccache) {
            Fmt(&buf, "ccache ");

            out_cmd->env_variables.Append({ "CCACHE_DEPEND", "1" });
            out_cmd->env_variables.Append({ "CCACHE_SLOPPINESS", "pch_defines,time_macros,include_file_ctime,include_file_mtime" });
            if (dest_filename && (features & (int)CompileFeature::DistCC)) {
                out_cmd->env_variables.Append({ "CCACHE_PREFIX", "distcc" });
            }
        } else if (dest_filename && (features & (int)CompileFeature::DistCC)) {
            Fmt(&buf, "distcc ");
        }

        // Compiler
        switch (src_type) {
            case SourceType::C: { Fmt(&buf, "\"%1\" -std=gnu11", cc); } break;
            case SourceType::Cxx: {
                int std = (gcc_ver >= 120000) ? 20 : 17;
                Fmt(&buf, "\"%1\" -std=gnu++%2", cxx, std);
            } break;

            case SourceType::GnuAssembly:
            case SourceType::MicrosoftAssembly:
            case SourceType::Object:
            case SourceType::Esbuild:
            case SourceType::QtUi:
            case SourceType::QtResources: { K_UNREACHABLE(); } break;
        }
        if (dest_filename) {
            Fmt(&buf, " -o \"%1\"", dest_filename);
        } else {
            switch (src_type) {
                case SourceType::C: { Fmt(&buf, " -x c-header"); } break;
                case SourceType::Cxx: { Fmt(&buf, " -x c++-header"); } break;

                case SourceType::GnuAssembly:
                case SourceType::MicrosoftAssembly:
                case SourceType::Object:
                case SourceType::Esbuild:
                case SourceType::QtUi:
                case SourceType::QtResources: { K_UNREACHABLE(); } break;
            }
        }
        Fmt(&buf, " -I. -MD -MF \"%1.d\"", dest_filename ? dest_filename : src_filename);
        out_cmd->rsp_offset = buf.len;

        // Build options
        Fmt(&buf, " -fvisibility=hidden -fno-strict-aliasing -fno-delete-null-pointer-checks -fno-omit-frame-pointer");
        Fmt(&buf, " -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free");
        if (gcc_ver >= 100000) {
            Fmt(&buf, " -fno-finite-loops");
        }
        if (features & (int)CompileFeature::MinimizeSize) {
            Fmt(&buf, " -Os -fwrapv -DNDEBUG -ffunction-sections -fdata-sections");
        } else if (features & (int)CompileFeature::Optimize) {
            Fmt(&buf, " -O2 -fwrapv -DNDEBUG");
        } else {
            Fmt(&buf, " -O0 -ftrapv -fsanitize-undefined-trap-on-error");
        }
        if (features & (int)CompileFeature::LTO) {
            Fmt(&buf, " -flto");
        }
        if (features & (int)CompileFeature::Warnings) {
            Fmt(&buf, " -Wall -Wextra -Wswitch -Wuninitialized -Wno-cast-function-type");
            if (src_type == SourceType::Cxx) {
                Fmt(&buf, " -Wno-init-list-lifetime -Wzero-as-null-pointer-constant");
            }
            Fmt(&buf, " -Wreturn-type -Werror=return-type");
        } else {
            Fmt(&buf, " -w");
        }
        if (features & (int)CompileFeature::HotAssets) {
            Fmt(&buf, " -DFELIX_HOT_ASSETS");
        }

        // Include build directory (for generated files)
        Span<const char> dest_directory = GetPathDirectory(dest_filename);
        Fmt(&buf, " \"-I%1\"", dest_directory);

        // Architecture flags
        switch (architecture)  {
            case HostArchitecture::x86_64: {
                Fmt(&buf, " -mpopcnt -msse4.1 -msse4.2 -mssse3 -mcx16");

                if (features & (int)CompileFeature::AESNI) {
                    Fmt(&buf, " -maes -mpclmul");
                }
                if (features & (int)CompileFeature::AVX2) {
                    Fmt(&buf, " -mavx2");
                }
                if (features & (int)CompileFeature::AVX512) {
                    Fmt(&buf, " -mavx512f -mavx512vl");
                }
            } break;

            case HostArchitecture::x86: {
                if (m32) {
                    Fmt(&buf, " -m32");
                }

                Fmt(&buf, " -msse2 -mfpmath=sse");

                if (features & (int)CompileFeature::AESNI) {
                    Fmt(&buf, " -maes -mpclmul");
                }
            } break;

            case HostArchitecture::ARM32:
            case HostArchitecture::ARM64:
            case HostArchitecture::RISCV64:
            case HostArchitecture::Loong64:
            case HostArchitecture::Web32: {} break;

            case HostArchitecture::Unknown: { K_UNREACHABLE(); } break;
        }

        // Platform flags
        switch (platform) {
            case HostPlatform::Windows: {
                Fmt(&buf, " -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE"
                          " -D__USE_MINGW_ANSI_STDIO=1");
            } break;

            case HostPlatform::macOS: {
                Fmt(&buf, " -pthread -fPIC");
            } break;

            case HostPlatform::Linux: {
                Fmt(&buf, " -pthread -fPIC -fno-semantic-interposition -D_FILE_OFFSET_BITS=64 -D_GLIBCXX_ASSERTIONS");

                if (features & (int)CompileFeature::Optimize) {
                    Fmt(&buf, " -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=%1", gcc_ver >= 120000 ? 3 : 2);
                } else {
                    Fmt(&buf, " -D_GLIBCXX_DEBUG -D_GLIBCXX_SANITIZE_VECTOR");
                }

                if (architecture == HostArchitecture::ARM32) {
                    Fmt(&buf, " -Wno-psabi");
                }
            } break;

            default: {
                Fmt(&buf, " -pthread -fPIC -fno-semantic-interposition -D_FILE_OFFSET_BITS=64");

                if (features & (int)CompileFeature::Optimize) {
                    Fmt(&buf, " -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2");
                }
            } break;
        }

        // Features
        if (features & (int)CompileFeature::DebugInfo) {
            Fmt(&buf, " -g");
        }
        if (features & (int)CompileFeature::ASan) {
            Fmt(&buf, " -fsanitize=address");
        }
        if (features & (int)CompileFeature::TSan) {
            Fmt(&buf, " -fsanitize=thread");
        }
        if (features & (int)CompileFeature::UBSan) {
            Fmt(&buf, " -fsanitize=undefined");
        }
        Fmt(&buf, " -fstack-protector-strong --param ssp-buffer-size=4");
        if (platform != HostPlatform::Windows) {
            Fmt(&buf, " -fstack-clash-protection");
        }
        if (features & (int)CompileFeature::ZeroInit) {
            Fmt(&buf, " -ftrivial-auto-var-init=zero");
        }
        if (features & (int)CompileFeature::CFI) {
            if (architecture == HostArchitecture::x86_64) {
                Fmt(&buf, " -fcf-protection=full");
            } else if (architecture == HostArchitecture::ARM64) {
                Fmt(&buf, " -mbranch-protection=standard");
            }
        }

        // Sources and definitions
        Fmt(&buf, " -DFELIX -c \"%1\"", src_filename);
        if (pch_filename) {
            Fmt(&buf, " -include \"%1\"", pch_filename);
        }
        for (const char *definition: definitions) {
            Fmt(&buf, " \"-%1%2\"", definition[0] != '-' ? 'D' : 'U', definition);
        }
        for (const char *include_directory: include_directories) {
            Fmt(&buf, " \"-I%1\"", include_directory);
        }
        for (const char *system_directory: system_directories) {
            Fmt(&buf, " -isystem \"%1\"", system_directory);
        }
        for (const char *include_file: include_files) {
            Fmt(&buf, " -include \"%1\"", include_file);
        }

        if (custom_flags) {
            Fmt(&buf, " %1", custom_flags);
        }

        out_cmd->cache_len = buf.len;
        if (FileIsVt100(STDOUT_FILENO)) {
            Fmt(&buf, " -fdiagnostics-color=always");
        } else {
            Fmt(&buf, " -fdiagnostics-color=never");
        }
        out_cmd->cmd_line = buf.TrimAndLeak(1);

        // Dependencies
        out_cmd->deps_mode = Command::DependencyMode::MakeLike;
        out_cmd->deps_filename = Fmt(alloc, "%1.d", dest_filename ? dest_filename : src_filename).ptr;
    }

    void MakeAssemblyCommand(const char *src_filename, Span<const char *const> definitions,
                             Span<const char *const> include_directories, const char *custom_flags,
                             uint32_t features, const char *dest_filename,
                             Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Compiler
        Fmt(&buf, "\"%1\" -o \"%2\"", cc, dest_filename);
        out_cmd->rsp_offset = buf.len;

        // Build options
        Fmt(&buf, " -I.");
        if (features & ((int)CompileFeature::MinimizeSize | (int)CompileFeature::Optimize)) {
            Fmt(&buf, " -DNDEBUG");
        }

        // Include build directory (for generated files)
        Span<const char> dest_directory = GetPathDirectory(dest_filename);
        Fmt(&buf, " \"-I%1\"", dest_directory);

        // Sources and definitions
        Fmt(&buf, " -DFELIX -c \"%1\"", src_filename);
        for (const char *definition: definitions) {
            Fmt(&buf, " \"-%1%2\"", definition[0] != '-' ? 'D' : 'U', definition);
        }
        for (const char *include_directory: include_directories) {
            Fmt(&buf, " \"-I%1\"", include_directory);
        }

        if (custom_flags) {
            Fmt(&buf, " %1", custom_flags);
        }

        out_cmd->cache_len = buf.len;
        if (FileIsVt100(STDOUT_FILENO)) {
            Fmt(&buf, " -fdiagnostics-color=always");
        } else {
            Fmt(&buf, " -fdiagnostics-color=never");
        }
        out_cmd->cmd_line = buf.TrimAndLeak(1);

        // Dependencies
        out_cmd->deps_mode = Command::DependencyMode::MakeLike;
        out_cmd->deps_filename = Fmt(alloc, "%1.d", dest_filename ? dest_filename : src_filename).ptr;
    }

    void MakeResourceCommand(const char *rc_filename, const char *dest_filename,
                             Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        out_cmd->cmd_line = Fmt(alloc, "\"%1\" -O coff \"%2\" \"%3\"", windres, rc_filename, dest_filename);
        out_cmd->cache_len = out_cmd->cmd_line.len;
    }

    void MakeLinkCommand(Span<const char *const> obj_filenames,
                         Span<const char *const> libraries, TargetType link_type,
                         const char *custom_flags, uint32_t features, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
            case TargetType::Executable: {
                bool static_link = (features & (int)CompileFeature::StaticRuntime);
                Fmt(&buf, "\"%1\"%2", cxx, static_link ? " -static" : "");
            } break;
            case TargetType::Library: { Fmt(&buf, "\"%1\" -shared", cxx); } break;
        }
        Fmt(&buf, " -o \"%1\"", dest_filename);
        out_cmd->rsp_offset = buf.len;

        // Build mode
        if (!(features & (int)CompileFeature::DebugInfo)) {
            Fmt(&buf, " -s");
        }
        if (features & (int)CompileFeature::LTO) {
            Fmt(&buf, " -flto -Wl,-O1");
        }

        // Objects and libraries
        for (const char *obj_filename: obj_filenames) {
            Fmt(&buf, " \"%1\"", obj_filename);
        }
        if (libraries.len) {
            HashSet<Span<const char>> framework_paths;

            if (platform != HostPlatform::Windows) {
                Fmt(&buf, " -Wl,--start-group");
            }
            for (const char *lib: libraries) {
                if (platform == HostPlatform::macOS && lib[0] == '@') {
                    Span<const char> directory = {};
                    Span<const char> basename = SplitStrReverse(lib + 1, '/', &directory);

                    if (EndsWith(basename, ".framework")) {
                        basename = basename.Take(0, basename.len - 10);
                    }

                    if (directory.len) {
                        bool inserted = false;
                        framework_paths.TrySet(directory, &inserted);

                        if (inserted) {
                            Fmt(&buf, " -F \"%1\"", directory);
                        }
                    }
                    Fmt(&buf, " -framework %1", basename);
                } else if (strpbrk(lib, K_PATH_SEPARATORS)) {
                    Fmt(&buf, " %1", lib);
                } else {
                    Fmt(&buf, " -l%1", lib);
                }
            }
            if (platform != HostPlatform::Windows) {
                Fmt(&buf, " -Wl,--end-group");
            }
        }

        // Platform flags and libraries
        Fmt(&buf, " -Wl,--gc-sections");
        switch (platform) {
            case HostPlatform::Windows: {
                Fmt(&buf, " -Wl,--dynamicbase -Wl,--nxcompat");
                if (architecture != HostArchitecture::x86) {
                    Fmt(&buf, " -Wl,--high-entropy-va");
                }

                Fmt(&buf, " -static-libgcc -static-libstdc++");
                if (!(features & (int)CompileFeature::StaticRuntime)) {
                    Fmt(&buf, " -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic");
                }
            } break;

            case HostPlatform::macOS: {
                Fmt(&buf, " -ldl -pthread -framework CoreFoundation -framework SystemConfiguration");
                Fmt(&buf, " -rpath \"@executable_path/../Frameworks\"");
            } break;

            default: {
                Fmt(&buf, " -pthread -Wl,-z,relro,-z,now,-z,noexecstack,-z,separate-code,-z,stack-size=1048576");
                Fmt(&buf, " -static-libgcc -static-libstdc++");

                if (platform == HostPlatform::Linux) {
                    Fmt(&buf, " -ldl -lrt");
                }
                if (link_type == TargetType::Executable) {
                    Fmt(&buf, " -pie");
                }
                if (architecture == HostArchitecture::ARM32) {
                    Fmt(&buf, " -latomic");
                }
            } break;
        }
        if (m32) {
            Fmt(&buf, " -m32");
        }

        // Features
        if (features & (int)CompileFeature::ASan) {
            Fmt(&buf, " -fsanitize=address");
        }
        if (features & (int)CompileFeature::TSan) {
            Fmt(&buf, " -fsanitize=thread");
        }
        if (features & (int)CompileFeature::UBSan) {
            Fmt(&buf, " -fsanitize=undefined");
        }
        if (features & (int)CompileFeature::NoConsole) {
            Fmt(&buf, " -mwindows");
        }

        if (ld) {
            Fmt(&buf, " -fuse-ld=%1", ld);
        }
        if (custom_flags) {
            Fmt(&buf, " %1", custom_flags);
        }

        out_cmd->cache_len = buf.len;
        if (FileIsVt100(STDOUT_FILENO)) {
            Fmt(&buf, " -fdiagnostics-color=always");
        } else {
            Fmt(&buf, " -fdiagnostics-color=never");
        }
        out_cmd->cmd_line = buf.TrimAndLeak(1);
    }

    void MakePostCommand(const char *, const char *, Allocator *, Command *) const override { K_UNREACHABLE(); }
};

#if defined(_WIN32)
class MsCompiler final: public Compiler {
    const char *cl;
    const char *assembler;
    const char *rc;
    const char *link;

    int cl_ver = 0;

    BlockAllocator str_alloc;

public:
    MsCompiler(HostArchitecture architecture) : Compiler(HostPlatform::Windows, architecture, "MSVC") {}

    static std::unique_ptr<const Compiler> Create(HostArchitecture architecture, const char *cl)
    {
        std::unique_ptr<MsCompiler> compiler = std::make_unique<MsCompiler>(architecture);

        // Determine CL version
        {
            char cmd[2048];
            Fmt(cmd, "\"%1\"", cl);

            HeapArray<char> output;
            if (!ReadCommandOutput(cmd, &output))
                return nullptr;

            compiler->cl_ver = ParseVersion(cmd, output, "Version");

            Span<const char> intro = SplitStrLine(output.As<const char>());
            HostArchitecture architecture = {};

            if (EndsWith(intro, " x86")) {
                architecture = HostArchitecture::x86;
            } else if (EndsWith(intro, " x64")) {
                architecture = HostArchitecture::x86_64;
            } else {
                LogError("Cannot determine MS compiler architecture");
                return nullptr;
            }

            if (compiler->architecture == HostArchitecture::Unknown) {
                compiler->architecture = architecture;
            } else if (compiler->architecture != architecture) {
                LogError("Mismatch between target architecture '%1' and compiler architecture '%2'",
                         HostArchitectureNames[(int)compiler->architecture], HostArchitectureNames[(int)architecture]);
                return nullptr;
            }
        }

        // Find main executables
        {
            Span<const char> prefix;
            Span<const char> suffix;
            Span<const char> version;
            if (!SplitPrefixSuffix(cl, "cl", &prefix, &suffix, &version))
                return nullptr;

            compiler->cl = DuplicateString(cl, &compiler->str_alloc).ptr;
            compiler->rc = Fmt(&compiler->str_alloc, "%1rc%2", prefix, version).ptr;
            switch (compiler->architecture) {
                case HostArchitecture::x86: { compiler->assembler = Fmt(&compiler->str_alloc, "%1ml%2", prefix, version).ptr; } break;
                case HostArchitecture::x86_64: { compiler->assembler = Fmt(&compiler->str_alloc, "%1ml64%2", prefix, version).ptr; } break;
                case HostArchitecture::ARM64: { compiler->assembler = Fmt(&compiler->str_alloc, "%1armasm64%2", prefix, version).ptr; } break;

                case HostArchitecture::ARM32:
                case HostArchitecture::RISCV64:
                case HostArchitecture::Loong64:
                case HostArchitecture::Web32:
                case HostArchitecture::Unknown: { K_UNREACHABLE(); } break;
            }
            compiler->link = Fmt(&compiler->str_alloc, "%1link%2", prefix, version).ptr;
        }

        Fmt(compiler->title, "%1 %2", compiler->name, FmtVersion(compiler->cl_ver, 3, 100));

        return compiler;
    }

    bool CheckFeatures(uint32_t features, uint32_t maybe_features, uint32_t *out_features) const override
    {
        uint32_t supported = 0;

        supported |= (int)CompileFeature::Optimize;
        supported |= (int)CompileFeature::MinimizeSize;
        supported |= (int)CompileFeature::HotAssets;
        supported |= (int)CompileFeature::PCH;
        supported |= (int)CompileFeature::Warnings;
        supported |= (int)CompileFeature::DebugInfo;
        supported |= (int)CompileFeature::ASan;
        supported |= (int)CompileFeature::LTO;
        supported |= (int)CompileFeature::CFI;
        supported |= (int)CompileFeature::LinkLibrary;
        supported |= (int)CompileFeature::StaticRuntime;
        supported |= (int)CompileFeature::NoConsole;

        supported |= (int)CompileFeature::AESNI;
        supported |= (int)CompileFeature::AVX2;
        supported |= (int)CompileFeature::AVX512;

        uint32_t unsupported = features & ~supported;
        if (unsupported) {
            LogError("Some features are not supported by %1: %2",
                     name, FmtFlags(unsupported, CompileFeatureOptions));
            return false;
        }

        features |= (supported & maybe_features);

        if ((features & (int)CompileFeature::MinimizeSize) && !(features & (int)CompileFeature::Optimize)) {
            LogError("Cannot use MinimizeSize without Optimize feature");
            return false;
        }

        *out_features = features;
        return true;
    }
    bool CanAssemble(SourceType type) const override { return (type == SourceType::MicrosoftAssembly); }

    const char *GetObjectExtension() const override { return ".obj"; }
    const char *GetLinkExtension(TargetType type) const override
    {
        switch (type) {
            case TargetType::Executable: return ".exe";
            case TargetType::Library: return ".dll";
        }

        K_UNREACHABLE();
    }
    const char *GetImportExtension() const override { return ".lib"; }
    const char *GetLibPrefix() const override { return ""; }
    const char *GetArchiveExtension() const override { return ".lib"; }
    const char *GetPostExtension(TargetType) const override { return nullptr; }

    bool GetCore(Span<const char *const>, Allocator *, const char **out_name,
                 HeapArray<const char *> *, HeapArray<const char *> *) const override
    {
        *out_name = nullptr;
        return true;
    }

    void MakeEmbedCommand(Span<const char *const> embed_filenames,
                          const char *embed_options, const char *dest_filename,
                          Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        // Strings literals were limited in length before MSVC 2022
        EmbedMode mode = (cl_ver >= 193000) ? EmbedMode::Literals : EmbedMode::Arrays;
        K::MakeEmbedCommand(embed_filenames, mode, embed_options, dest_filename, alloc, out_cmd);
    }

    void MakePchCommand(const char *pch_filename, SourceType src_type,
                        Span<const char *const> definitions, Span<const char *const> include_directories,
                        Span<const char *const> include_files, const char *custom_flags, uint32_t features,
                        Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);
        MakeCppCommand(pch_filename, src_type, nullptr, definitions, include_directories, {},
                       include_files, custom_flags, features, nullptr, alloc, out_cmd);
    }

    const char *GetPchCache(const char *pch_filename, Allocator *alloc) const override
    {
        K_ASSERT(alloc);

        const char *cache_filename = Fmt(alloc, "%1.pch", pch_filename).ptr;
        return cache_filename;
    }
    const char *GetPchObject(const char *pch_filename, Allocator *alloc) const override
    {
        K_ASSERT(alloc);

        const char *obj_filename = Fmt(alloc, "%1.obj", pch_filename).ptr;
        return obj_filename;
    }

    void MakeCppCommand(const char *src_filename, SourceType src_type,
                        const char *pch_filename, Span<const char *const> definitions,
                        Span<const char *const> include_directories, Span<const char *const> system_directories,
                        Span<const char *const> include_files, const char *custom_flags, uint32_t features,
                        const char *dest_filename, Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Compiler
        switch (src_type) {
            case SourceType::C: { Fmt(&buf, "\"%1\" /nologo", cl); } break;
            case SourceType::Cxx: { Fmt(&buf, "\"%1\" /nologo /std:c++20 /Zc:__cplusplus", cl); } break;

            case SourceType::GnuAssembly:
            case SourceType::MicrosoftAssembly:
            case SourceType::Object:
            case SourceType::Esbuild:
            case SourceType::QtUi:
            case SourceType::QtResources: { K_UNREACHABLE(); } break;
        }
        if (dest_filename) {
            Fmt(&buf, " \"/Fo%1\"", dest_filename);
        } else {
            Fmt(&buf, " /Yc \"/Fp%1.pch\" \"/Fo%1.obj\"", src_filename);
        }
        Fmt(&buf, " /Zc:preprocessor /permissive- /Zc:twoPhase- /showIncludes");
        out_cmd->rsp_offset = buf.len;

        // Build options
        Fmt(&buf, " /I. /EHsc /utf-8");
        if (features & (int)CompileFeature::MinimizeSize) {
            Fmt(&buf, " /O1 /DNDEBUG");
        } else if (features & (int)CompileFeature::Optimize) {
            Fmt(&buf, " /O2 /DNDEBUG");
        } else {
            Fmt(&buf, " /Od /RTCsu");
        }
        if (features & (int)CompileFeature::LTO) {
            Fmt(&buf, " /GL");
        }
        if (features & (int)CompileFeature::Warnings) {
            Fmt(&buf, " /W4 /wd4200 /wd4706 /wd4100 /wd4127 /wd4702 /wd4815 /wd4206 /wd4456 /wd4457 /wd4458 /wd4459");
        } else {
            Fmt(&buf, " /w");
        }
        if (features & (int)CompileFeature::HotAssets) {
            Fmt(&buf, " /DFELIX_HOT_ASSETS");
        }

        // Include build directory (for generated files)
        Span<const char> dest_directory = GetPathDirectory(dest_filename);
        Fmt(&buf, " \"/I%1\"", dest_directory);

        // Platform flags
        Fmt(&buf, " /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /DUNICODE /D_UNICODE"
                  " /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE");

        // Features
        if (features & (int)CompileFeature::DebugInfo) {
            Fmt(&buf, " /Z7 /Zo");
        }
        if (features & (int)CompileFeature::StaticRuntime) {
            Fmt(&buf, " /MT");
        } else {
            Fmt(&buf, " /MD");
        }
        if (features & (int)CompileFeature::ASan) {
            Fmt(&buf, " /fsanitize=address");
        }
        Fmt(&buf, " /GS");
        if (features & (int)CompileFeature::CFI) {
            Fmt(&buf, " /guard:cf /guard:ehcont");
        }

        if (architecture == HostArchitecture::x86_64) {
            if (features & (int)CompileFeature::AVX2) {
                Fmt(&buf, " /arch:AVX2");
            }
            if (features & (int)CompileFeature::AVX512) {
                Fmt(&buf, " /arch:AVX512");
            }
        } else if (architecture == HostArchitecture::x86) {
            Fmt(&buf, " /arch:SSE2");
        }

        // Sources and definitions
        Fmt(&buf, " /DFELIX /c /utf-8 \"%1\"", src_filename);
        if (pch_filename) {
            Fmt(&buf, " \"/FI%1\" \"/Yu%1\" \"/Fp%1.pch\"", pch_filename);
        }
        for (const char *definition: definitions) {
            Fmt(&buf, " \"/%1%2\"", definition[0] != '-' ? 'D' : 'U', definition);
        }
        for (const char *include_directory: include_directories) {
            Fmt(&buf, " \"/I%1\"", include_directory);
        }
        for (const char *system_directory: system_directories) {
            Fmt(&buf, " \"/I%1\"", system_directory);
        }
        for (const char *include_file: include_files) {
            if (PathIsAbsolute(include_file)) {
                Fmt(&buf, " \"/FI%1\"", include_file);
            } else {
                const char *cwd = GetWorkingDirectory();
                Fmt(&buf, " \"/FI%1%/%2\"", cwd, include_file);
            }
        }

        if (custom_flags) {
            Fmt(&buf, " %1", custom_flags);
        }

        out_cmd->cache_len = buf.len;
        out_cmd->cmd_line = buf.TrimAndLeak(1);
        out_cmd->skip_lines = 1;

        // Dependencies
        out_cmd->deps_mode = Command::DependencyMode::ShowIncludes;
    }

    void MakeAssemblyCommand(const char *src_filename, Span<const char *const> definitions,
                             Span<const char *const> include_directories, const char *custom_flags,
                             uint32_t features, const char *dest_filename,
                             Allocator *alloc, Command *out_cmd) const override
    {
        switch (architecture) {
            case HostArchitecture::x86:
            case HostArchitecture::x86_64: {
                MakeMasmCommand(src_filename, definitions, include_directories,
                                custom_flags, features, dest_filename, alloc, out_cmd);
            } break;

            case HostArchitecture::ARM64: {
                MakeArmAsmCommand(src_filename, definitions, include_directories,
                                  custom_flags, features, dest_filename, alloc, out_cmd);
            } break;

            case HostArchitecture::ARM32:
            case HostArchitecture::RISCV64:
            case HostArchitecture::Loong64:
            case HostArchitecture::Web32:
            case HostArchitecture::Unknown: { K_UNREACHABLE(); } break;
        }
    }

    void MakeResourceCommand(const char *rc_filename, const char *dest_filename,
                             Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        out_cmd->cmd_line = Fmt(alloc, "\"%1\" /nologo /FO\"%2\" \"%3\"", rc, dest_filename, rc_filename);
        out_cmd->cache_len = out_cmd->cmd_line.len;
    }

    void MakeLinkCommand(Span<const char *const> obj_filenames,
                         Span<const char *const> libraries, TargetType link_type,
                         const char *custom_flags, uint32_t features, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
            case TargetType::Executable: { Fmt(&buf, "\"%1\" /nologo", link); } break;
            case TargetType::Library: { Fmt(&buf, "\"%1\" /nologo /DLL", link); } break;
        }
        Fmt(&buf, " \"/OUT:%1\"", dest_filename);
        out_cmd->rsp_offset = buf.len;

        // Build mode
        if (features & (int)CompileFeature::LTO) {
            Fmt(&buf, " /LTCG");
        }
        Fmt(&buf, " /DYNAMICBASE /OPT:ref");
        if (architecture != HostArchitecture::x86) {
            Fmt(&buf, " /HIGHENTROPYVA");
        }

        // Objects and libraries
        for (const char *obj_filename: obj_filenames) {
            Fmt(&buf, " \"%1\"", obj_filename);
        }
        for (const char *lib: libraries) {
            if (GetPathExtension(lib).len) {
                Fmt(&buf, " %1", lib);
            } else {
                Fmt(&buf, " %1.lib", lib);
            }
        }
        Fmt(&buf, " setargv.obj");

        // Features
        if (features & (int)CompileFeature::DebugInfo) {
            Fmt(&buf, " /DEBUG:FULL");
        } else {
            Fmt(&buf, " /DEBUG:NONE");
        }
        if (features & (int)CompileFeature::CFI) {
            Fmt(&buf, " /GUARD:cf /GUARD:ehcont");
        }
        if (features & (int)CompileFeature::NoConsole) {
            Fmt(&buf, " /SUBSYSTEM:windows /ENTRY:mainCRTStartup");
        }

        if (custom_flags) {
            Fmt(&buf, " %1", custom_flags);
        }

        out_cmd->cache_len = buf.len;
        out_cmd->cmd_line = buf.TrimAndLeak(1);
        out_cmd->skip_lines = 1;
    }

    void MakePostCommand(const char *, const char *, Allocator *, Command *) const override { K_UNREACHABLE(); }

private:
    void MakeMasmCommand(const char *src_filename, Span<const char *const> definitions,
                         Span<const char *const> include_directories, const char *custom_flags,
                         uint32_t features, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const
    {
        K_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Compiler
        Fmt(&buf, "\"%1\" /c /nologo /Fo\"%2\"", assembler, dest_filename);

        // Build options
        Fmt(&buf, " -I.");
        if (features & (int)CompileFeature::Warnings) {
            Fmt(&buf, " /W3");
        } else {
            Fmt(&buf, " /w");
        }

        // Include build directory (for generated files)
        Span<const char> dest_directory = GetPathDirectory(dest_filename);
        Fmt(&buf, " \"/I%1\"", dest_directory);

        // Platform flags
        Fmt(&buf, " /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /DUNICODE /D_UNICODE");

        // Sources and definitions
        Fmt(&buf, " /DFELIX /c /utf-8 /Ta\"%1\"", src_filename);
        for (const char *definition: definitions) {
            Fmt(&buf, " \"/%1%2\"", definition[0] != '-' ? 'D' : 'U', definition);
        }
        for (const char *include_directory: include_directories) {
            Fmt(&buf, " \"/I%1\"", include_directory);
        }

        if (custom_flags) {
            Fmt(&buf, " %1", custom_flags);
        }

        out_cmd->cache_len = buf.len;
        out_cmd->cmd_line = buf.TrimAndLeak(1);
        out_cmd->skip_lines = 1;
    }

    void MakeArmAsmCommand(const char *src_filename, Span<const char *const>,
                           Span<const char *const> include_directories, const char *custom_flags,
                           uint32_t features, const char *dest_filename,
                           Allocator *alloc, Command *out_cmd) const
    {
        K_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Compiler
        Fmt(&buf, "\"%1\" -nologo -o \"%2\"", assembler, dest_filename);

        // Build options
        Fmt(&buf, " -i.");
        if (!(features & (int)CompileFeature::Warnings)) {
            Fmt(&buf, " -nowarn");
        }

        // Include build directory (for generated files)
        Span<const char> dest_directory = GetPathDirectory(dest_filename);
        Fmt(&buf, " \"-i%1\"", dest_directory);

        // Sources and definitions
        Fmt(&buf, " \"%1\"", src_filename);
        for (const char *include_directory: include_directories) {
            Fmt(&buf, " \"-i%1\"", include_directory);
        }

        if (custom_flags) {
            Fmt(&buf, " %1", custom_flags);
        }

        out_cmd->cache_len = buf.len;
        out_cmd->cmd_line = buf.TrimAndLeak(1);
    }
};
#endif

class EmCompiler final: public Compiler {
    const char *cc;
    const char *cxx;

    BlockAllocator str_alloc;

public:
    EmCompiler(HostPlatform platform) : Compiler(platform, HostArchitecture::Web32, "EmCC") {}

    static std::unique_ptr<const Compiler> Create(HostPlatform platform, const char *cc)
    {
        std::unique_ptr<EmCompiler> compiler = std::make_unique<EmCompiler>(platform);

        // Find executables
        {
            if (!FindExecutableInPath(cc, &compiler->str_alloc, &cc)) {
                LogError("Could not find '%1' in PATH", cc);
                return nullptr;
            }

            Span<const char> prefix;
            Span<const char> suffix;
            Span<const char> version;
            if (!SplitPrefixSuffix(cc, "emcc", &prefix, &suffix, &version))
                return nullptr;

            compiler->cc = cc;
            compiler->cxx = Fmt(&compiler->str_alloc, "%1em++%2", prefix, suffix).ptr;
        }

        return compiler;
    }

    bool CheckFeatures(uint32_t features, uint32_t maybe_features, uint32_t *out_features) const override
    {
        uint32_t supported = 0;

        supported |= (int)CompileFeature::Optimize;
        supported |= (int)CompileFeature::MinimizeSize;
        supported |= (int)CompileFeature::Warnings;
        supported |= (int)CompileFeature::DebugInfo;

        uint32_t unsupported = features & ~supported;
        if (unsupported) {
            LogError("Some features are not supported by %1: %2",
                     name, FmtFlags(unsupported, CompileFeatureOptions));
            return false;
        }

        features |= (supported & maybe_features);

        if ((features & (int)CompileFeature::MinimizeSize) && !(features & (int)CompileFeature::Optimize)) {
            LogError("Cannot use MinimizeSize without Optimize feature");
            return false;
        }

        *out_features = features;
        return true;
    }
    bool CanAssemble(SourceType) const override { return false; }

    const char *GetObjectExtension() const override { return ".o"; }
    const char *GetLinkExtension(TargetType) const override { return ".js"; }
    const char *GetImportExtension() const override { return ".so"; }
    const char *GetLibPrefix() const override { return "lib"; }
    const char *GetArchiveExtension() const override { return ".a"; }
    const char *GetPostExtension(TargetType) const override { return nullptr; }

    bool GetCore(Span<const char *const>, Allocator *, const char **out_name,
                 HeapArray<const char *> *, HeapArray<const char *> *) const override
    {
        *out_name = nullptr;
        return true;
    }

    void MakeEmbedCommand(Span<const char *const> embed_filenames,
                          const char *embed_options, const char *dest_filename,
                          Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);
        K::MakeEmbedCommand(embed_filenames, EmbedMode::Literals, embed_options, dest_filename, alloc, out_cmd);
    }

    void MakePchCommand(const char *, SourceType, Span<const char *const>, Span<const char *const>,
                        Span<const char *const>, const char *, uint32_t, Allocator *, Command *) const override { K_UNREACHABLE(); }

    const char *GetPchCache(const char *, Allocator *) const override { return nullptr; }
    const char *GetPchObject(const char *, Allocator *) const override { return nullptr; }

    void MakeCppCommand(const char *src_filename, SourceType src_type,
                        const char *pch_filename, Span<const char *const> definitions,
                        Span<const char *const> include_directories, Span<const char *const> system_directories,
                        Span<const char *const> include_files, const char *custom_flags, uint32_t features,
                        const char *dest_filename, Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        // Hide noisy EmCC messages
        out_cmd->env_variables.Append({ "EMCC_LOGGING", "0" });

        HeapArray<char> buf(alloc);

        // Compiler
        switch (src_type) {
            case SourceType::C: { Fmt(&buf, "\"%1\" -std=gnu11", cc); } break;
            case SourceType::Cxx: { Fmt(&buf, "\"%1\" -std=gnu++20", cxx); } break;

            case SourceType::GnuAssembly:
            case SourceType::MicrosoftAssembly:
            case SourceType::Object:
            case SourceType::Esbuild:
            case SourceType::QtUi:
            case SourceType::QtResources: { K_UNREACHABLE(); } break;
        }
        K_ASSERT(dest_filename); // No PCH
        Fmt(&buf, " -o \"%1\"", dest_filename);
        Fmt(&buf, " -MD -MF \"%1.d\"", dest_filename ? dest_filename : src_filename);
        out_cmd->rsp_offset = buf.len;

        // Build options
        Fmt(&buf, " -I. -fvisibility=hidden -fno-strict-aliasing -fno-delete-null-pointer-checks -fno-omit-frame-pointer");
        if (features & (int)CompileFeature::MinimizeSize) {
            Fmt(&buf, " -Os -fwrapv -DNDEBUG");
        } else if (features & (int)CompileFeature::Optimize) {
            Fmt(&buf, " -O1 -fwrapv -DNDEBUG");
        } else {
            Fmt(&buf, " -O0 -ftrapv");
        }
        if (features & (int)CompileFeature::Warnings) {
            Fmt(&buf, " -Wall -Wextra -Wswitch");
            if (src_type == SourceType::Cxx) {
                Fmt(&buf, " -Wzero-as-null-pointer-constant");
            }
            Fmt(&buf, " -Wreturn-type -Werror=return-type");
        } else {
            Fmt(&buf, " -Wno-everything");
        }
        Fmt(&buf, " -fPIC");

        // Include build directory (for generated files)
        Span<const char> dest_directory = GetPathDirectory(dest_filename);
        Fmt(&buf, " \"-I%1\"", dest_directory);

        // Features
        if (features & (int)CompileFeature::DebugInfo) {
            Fmt(&buf, " -g");
        }

        // Sources and definitions
        Fmt(&buf, " -DFELIX -c \"%1\"", src_filename);
        if (pch_filename) {
            Fmt(&buf, " -include \"%1\"", pch_filename);
        }
        for (const char *definition: definitions) {
            Fmt(&buf, " \"-%1%2\"", definition[0] != '-' ? 'D' : 'U', definition);
        }
        for (const char *include_directory: include_directories) {
            Fmt(&buf, " \"-I%1\"", include_directory);
        }
        for (const char *system_directory: system_directories) {
            Fmt(&buf, " -isystem \"%1\"", system_directory);
        }
        for (const char *include_file: include_files) {
            Fmt(&buf, " -include \"%1\"", include_file);
        }

        if (custom_flags) {
            Fmt(&buf, " %1", custom_flags);
        }

        out_cmd->cache_len = buf.len;
        if (FileIsVt100(STDOUT_FILENO)) {
            Fmt(&buf, " -fcolor-diagnostics -fansi-escape-codes");
        } else {
            Fmt(&buf, " -fno-color-diagnostics");
        }
        out_cmd->cmd_line = buf.TrimAndLeak(1);

        // Dependencies
        out_cmd->deps_mode = Command::DependencyMode::MakeLike;
        out_cmd->deps_filename = Fmt(alloc, "%1.d", dest_filename ? dest_filename : src_filename).ptr;
    }

    void MakeAssemblyCommand(const char *, Span<const char *const>, Span<const char *const>, const char *,
                             uint32_t, const char *, Allocator *, Command *) const override { K_UNREACHABLE(); }

    void MakeResourceCommand(const char *, const char *, Allocator *, Command *) const override { K_UNREACHABLE(); }

    void MakeLinkCommand(Span<const char *const> obj_filenames,
                         Span<const char *const> libraries, TargetType link_type,
                         const char *custom_flags, uint32_t, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        // Hide noisy EmCC messages
        out_cmd->env_variables.Append({ "EMCC_LOGGING", "0" });

        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
            case TargetType::Executable: { Fmt(&buf, "\"%1\"", cxx); } break;
            case TargetType::Library: { K_UNREACHABLE(); } break;
        }
        Fmt(&buf, " -o \"%1\"", dest_filename);
        out_cmd->rsp_offset = buf.len;

        // Objects and libraries
        for (const char *obj_filename: obj_filenames) {
            Fmt(&buf, " \"%1\"", obj_filename);
        }
        if (libraries.len) {
            for (const char *lib: libraries) {
                if (strpbrk(lib, K_PATH_SEPARATORS)) {
                    Fmt(&buf, " %1", lib);
                } else {
                    Fmt(&buf, " -l%1", lib);
                }
            }
        }

        // Platform flags
        Fmt(&buf, " -s MAXIMUM_MEMORY=%1 -s ALLOW_MEMORY_GROWTH=1", Mebibytes(256));
        if (platform == HostPlatform::EmscriptenNode) {
            Fmt(&buf, " -s NODERAWFS=1 -lnodefs.js");
        }
        if (link_type == TargetType::Library) {
            Fmt(&buf, " -s SIDE_MODULE=1");
        }

        if (custom_flags) {
            Fmt(&buf, " %1", custom_flags);
        }

        out_cmd->cache_len = buf.len;
        if (FileIsVt100(STDOUT_FILENO)) {
            Fmt(&buf, " -fcolor-diagnostics -fansi-escape-codes");
        } else {
            Fmt(&buf, " -fno-color-diagnostics");
        }
        out_cmd->cmd_line = buf.TrimAndLeak(1);
    }

    void MakePostCommand(const char *, const char *, Allocator *, Command *) const override { K_UNREACHABLE(); }
};

class TeensyCompiler final: public Compiler {
    enum class Model {
        TeensyLC,
        Teensy30,
        Teensy31,
        Teensy35,
        Teensy36,
        Teensy40,
        Teensy41,
        TeensyMM
    };

    const char *arduino;
    const char *cc;
    const char *cxx;
    const char *ld;
    const char *objcopy;
    Model model;

    BlockAllocator str_alloc;

public:
    TeensyCompiler(HostPlatform platform) : Compiler(platform, HostArchitecture::ARM32, "GCC") {}

    static std::unique_ptr<const Compiler> Create(HostPlatform platform, const char *arduino, const char *cc)
    {
        std::unique_ptr<TeensyCompiler> compiler = std::make_unique<TeensyCompiler>(platform);

        if (!cc) {
            cc = Fmt(&compiler->str_alloc, "%1%/hardware/tools/arm/bin/arm-none-eabi-gcc%2", arduino, K_EXECUTABLE_EXTENSION).ptr;

            if (!TestFile(cc)) {
                LogError("Cannot find Teensy compiler in Arduino SDK");
                return nullptr;
            }
        }

        // Decode model string
        switch (platform) {
            case HostPlatform::TeensyLC: { compiler->model = Model::TeensyLC; } break;
            case HostPlatform::Teensy30: { compiler->model = Model::Teensy30; } break;
            case HostPlatform::Teensy31: { compiler->model = Model::Teensy31; } break;
            case HostPlatform::Teensy35: { compiler->model = Model::Teensy35; } break;
            case HostPlatform::Teensy36: { compiler->model = Model::Teensy36; } break;
            case HostPlatform::Teensy40: { compiler->model = Model::Teensy40; } break;
            case HostPlatform::Teensy41: { compiler->model = Model::Teensy41; } break;
            case HostPlatform::TeensyMM: { compiler->model = Model::TeensyMM; } break;

            default: { K_UNREACHABLE(); } break;
        }

        // Find executables
        {
            Span<const char> prefix;
            Span<const char> suffix;
            Span<const char> version;
            if (!SplitPrefixSuffix(cc, "gcc", &prefix, &suffix, &version))
                return nullptr;

            compiler->arduino = DuplicateString(arduino, &compiler->str_alloc).ptr;
            compiler->cc = DuplicateString(cc, &compiler->str_alloc).ptr;
            compiler->cxx = Fmt(&compiler->str_alloc, "%1g++%2", prefix, suffix).ptr;
            compiler->ld = Fmt(&compiler->str_alloc, "%1ld%2", prefix, version).ptr;
            compiler->objcopy = Fmt(&compiler->str_alloc, "%1objcopy%2", prefix, version).ptr;
        }

        return compiler;
    }

    bool CheckFeatures(uint32_t features, uint32_t maybe_features, uint32_t *out_features) const override
    {
        uint32_t supported = 0;

        supported |= (int)CompileFeature::Optimize;
        supported |= (int)CompileFeature::MinimizeSize;
        supported |= (int)CompileFeature::Warnings;
        supported |= (int)CompileFeature::DebugInfo;
        supported |= (int)CompileFeature::LTO;

        uint32_t unsupported = features & ~supported;
        if (unsupported) {
            LogError("Some features are not supported by %1: %2",
                     name, FmtFlags(unsupported, CompileFeatureOptions));
            return false;
        }

        features |= (supported & maybe_features);

        if ((features & (int)CompileFeature::MinimizeSize) && !(features & (int)CompileFeature::Optimize)) {
            LogError("Cannot use MinimizeSize without Optimize feature");
            return false;
        }

        *out_features = features;
        return true;
    }
    bool CanAssemble(SourceType) const override { return false; }

    const char *GetObjectExtension() const override { return ".o"; }
    const char *GetLinkExtension(TargetType type) const override {
        K_ASSERT(type == TargetType::Executable);
        return ".elf";
    }
    const char *GetImportExtension() const override { return ".so"; }
    const char *GetLibPrefix() const override { return "lib"; }
    const char *GetArchiveExtension() const override { return ".a"; }
    const char *GetPostExtension(TargetType) const override { return ".hex"; }

    bool GetCore(Span<const char *const> definitions, Allocator *alloc, const char **out_name,
                 HeapArray<const char *> *out_filenames, HeapArray<const char *> *out_definitions) const override
    {
        const char *dirname = nullptr;
        switch (model) {
            case Model::TeensyLC:
            case Model::Teensy30:
            case Model::Teensy31:
            case Model::Teensy35:
            case Model::Teensy36: { dirname = Fmt(alloc, "%1%/hardware/teensy/avr/cores/teensy3", arduino).ptr; } break;
            case Model::Teensy40:
            case Model::Teensy41:
            case Model::TeensyMM: { dirname = Fmt(alloc, "%1%/hardware/teensy/avr/cores/teensy4", arduino).ptr; } break;
        }
        K_ASSERT(dirname);

        EnumResult ret = EnumerateDirectory(dirname, nullptr, 1024, [&](const char *basename, FileType) {
            if (TestStr(basename, "Blink.cc"))
                return true;

            SourceType src_type;
            if (!DetermineSourceType(basename, &src_type))
                return true;
            if (src_type != SourceType::C && src_type != SourceType::Cxx)
                return true;

            const char *src_filename = NormalizePath(basename, dirname, alloc).ptr;
            out_filenames->Append(src_filename);

            return true;
        });
        if (ret != EnumResult::Success)
            return false;

        uint64_t hash = 0;
        for (const char *definition: definitions) {
            if (StartsWith(definition, "F_CPU=") ||
                    StartsWith(definition, "USB_") ||
                    StartsWith(definition, "LAYOUT_")) {
                out_definitions->Append(definition);
                hash ^= HashTraits<const char *>::Hash(definition);
            }
        }
        *out_name = Fmt(alloc, "Teensy%/%1", FmtHex(hash).Pad0(-16)).ptr;

        return true;
    }

    void MakeEmbedCommand(Span<const char *const> embed_filenames,
                          const char *embed_options, const char *dest_filename,
                          Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);
        K::MakeEmbedCommand(embed_filenames, EmbedMode::Literals, embed_options, dest_filename, alloc, out_cmd);
    }

    void MakePchCommand(const char *, SourceType, Span<const char *const>, Span<const char *const>,
                        Span<const char *const>, const char *, uint32_t, Allocator *, Command *) const override { K_UNREACHABLE(); }

    const char *GetPchCache(const char *, Allocator *) const override { return nullptr; }
    const char *GetPchObject(const char *, Allocator *) const override { return nullptr; }

    void MakeCppCommand(const char *src_filename, SourceType src_type,
                        const char *pch_filename, Span<const char *const> definitions,
                        Span<const char *const> include_directories, Span<const char *const> system_directories,
                        Span<const char *const> include_files, const char *custom_flags, uint32_t features,
                        const char *dest_filename, Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Compiler
        switch (src_type) {
            case SourceType::C: { Fmt(&buf, "\"%1\" -std=gnu11", cc); } break;
            case SourceType::Cxx: { Fmt(&buf, "\"%1\" -std=gnu++20", cxx); } break;

            case SourceType::GnuAssembly:
            case SourceType::MicrosoftAssembly:
            case SourceType::Object:
            case SourceType::Esbuild:
            case SourceType::QtUi:
            case SourceType::QtResources: { K_UNREACHABLE(); } break;
        }
        K_ASSERT(dest_filename); // No PCH
        Fmt(&buf, " -o \"%1\"", dest_filename);
        Fmt(&buf, " -MD -MF \"%1.d\"", dest_filename ? dest_filename : src_filename);
        out_cmd->rsp_offset = buf.len;

        // Build options
        Fmt(&buf, " -I. -fvisibility=hidden -fno-strict-aliasing -fno-delete-null-pointer-checks -fno-omit-frame-pointer");
        if (features & (int)CompileFeature::MinimizeSize) {
            Fmt(&buf, " -Os -fwrapv -DNDEBUG");
        } else if (features & (int)CompileFeature::Optimize) {
            Fmt(&buf, " -O2 -fwrapv -DNDEBUG");
        } else {
            Fmt(&buf, " -O0 -ftrapv -fsanitize-undefined-trap-on-error");
        }
        if (features & (int)CompileFeature::LTO) {
            Fmt(&buf, " -flto");
        }
        if (features & (int)CompileFeature::Warnings) {
            Fmt(&buf, " -Wall -Wextra -Wswitch");
            if (src_type == SourceType::Cxx) {
                Fmt(&buf, " -Wzero-as-null-pointer-constant");
            }
            Fmt(&buf, " -Wreturn-type -Werror=return-type");
        } else {
            Fmt(&buf, " -w");
        }

        // Don't override explicit user defines
        bool set_fcpu = true;
        bool set_usb = true;
        bool set_layout = true;
        for (const char *definition: definitions) {
            set_fcpu &= !StartsWith(definition, "F_CPU=");
            set_usb &= !StartsWith(definition, "USB_");
            set_layout &= !StartsWith(definition, "LAYOUT_");
        }

        // Platform flags
        Fmt(&buf, " -ffunction-sections -fdata-sections -nostdlib");
        Fmt(&buf, " -DARDUINO=10819 -DTEENSYDUINO=159");
        switch (model) {
            case Model::TeensyLC: { Fmt(&buf, " -DARDUINO_TEENSYLC \"-I%1/hardware/teensy/avr/cores/teensy3\" -mcpu=cortex-m0plus -mthumb"
                                              " -fsingle-precision-constant -mno-unaligned-access -Wno-error=narrowing -D__MKL26Z64__%2", arduino, set_fcpu ? " -DF_CPU=48000000" : ""); } break;
            case Model::Teensy30: { Fmt(&buf, " -DARDUINO_TEENSY30 \"-I%1/hardware/teensy/avr/cores/teensy3\" -mcpu=cortex-m4 -mthumb"
                                              " -fsingle-precision-constant -mno-unaligned-access -Wno-error=narrowing -D__MK20DX128__%2", arduino, set_fcpu ? " -DF_CPU=96000000" : ""); } break;
            case Model::Teensy31: { Fmt(&buf, " -DARDUINO_TEENSY31 \"-I%1/hardware/teensy/avr/cores/teensy3\" -mcpu=cortex-m4 -mthumb"
                                              " -fsingle-precision-constant -mno-unaligned-access -Wno-error=narrowing -D__MK20DX256__%2", arduino, set_fcpu ? " -DF_CPU=96000000" : ""); } break;
            case Model::Teensy35: { Fmt(&buf, " -DARDUINO_TEENSY35 \"-I%1/hardware/teensy/avr/cores/teensy3\" -mcpu=cortex-m4 -mthumb -mfloat-abi=hard"
                                              " -mfpu=fpv4-sp-d16 -fsingle-precision-constant -mno-unaligned-access -Wno-error=narrowing -D__MK64FX512__%2", arduino, set_fcpu ? " -DF_CPU=120000000" : ""); } break;
            case Model::Teensy36: { Fmt(&buf, " -DARDUINO_TEENSY36 \"-I%1/hardware/teensy/avr/cores/teensy3\" -mcpu=cortex-m4 -mthumb -mfloat-abi=hard"
                                              " -mfpu=fpv4-sp-d16 -fsingle-precision-constant -mno-unaligned-access -Wno-error=narrowing -D__MK66FX1M0__%2", arduino, set_fcpu ? " -DF_CPU=180000000" : ""); } break;
            case Model::Teensy40: { Fmt(&buf, " -DARDUINO_TEENSY40 \"-I%1/hardware/teensy/avr/cores/teensy4\" -mcpu=cortex-m7 -mthumb -mfloat-abi=hard"
                                              " -mfpu=fpv5-d16 -mno-unaligned-access -D__IMXRT1062__%2", arduino, set_fcpu ? " -DF_CPU=600000000" : ""); } break;
            case Model::Teensy41: { Fmt(&buf, " -DARDUINO_TEENSY41 \"-I%1/hardware/teensy/avr/cores/teensy4\" -mcpu=cortex-m7 -mthumb -mfloat-abi=hard"
                                              " -mfpu=fpv5-d16 -mno-unaligned-access -D__IMXRT1062__%2", arduino, set_fcpu ? " -DF_CPU=600000000" : ""); } break;
            case Model::TeensyMM: { Fmt(&buf, " -DARDUINO_TEENSY_MICROMOD \"-I%1/hardware/teensy/avr/cores/teensy4\" -mcpu=cortex-m7 -mthumb -mfloat-abi=hard"
                                              " -mfpu=fpv5-d16 -mno-unaligned-access -D__IMXRT1062__%2", arduino, set_fcpu ? " -DF_CPU=600000000" : ""); } break;
        }
        if (src_type == SourceType::Cxx) {
            Fmt(&buf, " -felide-constructors -fno-exceptions -fno-rtti");
        }
        if (set_usb) {
            Fmt(&buf, " -DUSB_SERIAL");
        }
        if (set_layout) {
            Fmt(&buf, " -DLAYOUT_US_ENGLISH");
        }

        // Features
        if (features & (int)CompileFeature::DebugInfo) {
            Fmt(&buf, " -g");
        }
        if (features & (int)CompileFeature::ZeroInit) {
            Fmt(&buf, " -ftrivial-auto-var-init=zero");
        }

        // Sources and definitions
        Fmt(&buf, " -DFELIX -c \"%1\"", src_filename);
        if (pch_filename) {
            Fmt(&buf, " -include \"%1\"", pch_filename);
        }
        for (const char *definition: definitions) {
            Fmt(&buf, " \"-%1%2\"", definition[0] != '-' ? 'D' : 'U', definition);
        }
        for (const char *include_directory: include_directories) {
            Fmt(&buf, " \"-I%1\"", include_directory);
        }
        for (const char *system_directory: system_directories) {
            Fmt(&buf, " -isystem \"%1\"", system_directory);
        }
        for (const char *include_file: include_files) {
            Fmt(&buf, " -include \"%1\"", include_file);
        }

        if (custom_flags) {
            Fmt(&buf, " %1", custom_flags);
        }

        out_cmd->cache_len = buf.len;
        if (FileIsVt100(STDOUT_FILENO)) {
            Fmt(&buf, " -fdiagnostics-color=always");
        } else {
            Fmt(&buf, " -fdiagnostics-color=never");
        }
        out_cmd->cmd_line = buf.TrimAndLeak(1);

        // Dependencies
        out_cmd->deps_mode = Command::DependencyMode::MakeLike;
        out_cmd->deps_filename = Fmt(alloc, "%1.d", dest_filename ? dest_filename : src_filename).ptr;
    }

    void MakeAssemblyCommand(const char *, Span<const char *const>, Span<const char *const>, const char *,
                             uint32_t, const char *, Allocator *, Command *) const override { K_UNREACHABLE(); }

    void MakeResourceCommand(const char *, const char *, Allocator *, Command *) const override { K_UNREACHABLE(); }

    void MakeLinkCommand(Span<const char *const> obj_filenames,
                         Span<const char *const> libraries, TargetType link_type,
                         const char *custom_flags, uint32_t features, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
            case TargetType::Executable: { Fmt(&buf, "\"%1\"", cc); } break;
            case TargetType::Library: { K_UNREACHABLE(); } break;
        }
        Fmt(&buf, " -o \"%1\"", dest_filename);
        out_cmd->rsp_offset = buf.len;

        // Build mode
        if (!(features & (int)CompileFeature::DebugInfo)) {
            Fmt(&buf, " -s");
        }
        if (features & (int)CompileFeature::LTO) {
            Fmt(&buf, " -flto -Wl,-Os");
        }

        // Objects and libraries
        for (const char *obj_filename: obj_filenames) {
            Fmt(&buf, " \"%1\"", obj_filename);
        }
        if (libraries.len) {
            Fmt(&buf, " -Wl,--start-group");
            for (const char *lib: libraries) {
                if (strpbrk(lib, K_PATH_SEPARATORS)) {
                    Fmt(&buf, " %1", lib);
                } else {
                    Fmt(&buf, " -l%1", lib);
                }
            }
            Fmt(&buf, " -Wl,--end-group");
        }

        // Platform flags and libraries
        Fmt(&buf, " -Wl,--gc-sections,--defsym=__rtc_localtime=0 --specs=nano.specs");
        switch (model) {
            case Model::TeensyLC: { Fmt(&buf, " -mcpu=cortex-m0plus -mthumb -larm_cortexM0l_math -fsingle-precision-constant \"-T%1/hardware/teensy/avr/cores/teensy3/mkl26z64.ld\"", arduino); } break;
            case Model::Teensy30: { Fmt(&buf, " -mcpu=cortex-m4 -mthumb -larm_cortexM4l_math -fsingle-precision-constant \"-T%1/hardware/teensy/avr/cores/teensy3/mk20dx128.ld\"", arduino); } break;
            case Model::Teensy31: { Fmt(&buf, " -mcpu=cortex-m4 -mthumb -larm_cortexM4l_math -fsingle-precision-constant \"-T%1/hardware/teensy/avr/cores/teensy3/mk20dx256.ld\"", arduino); } break;
            case Model::Teensy35: { Fmt(&buf, " -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -larm_cortexM4lf_math"
                                              " -fsingle-precision-constant \"-T%1/hardware/teensy/avr/cores/teensy3/mk64fx512.ld\"", arduino); } break;
            case Model::Teensy36: { Fmt(&buf, " -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -larm_cortexM4lf_math"
                                              " -fsingle-precision-constant \"-T%1/hardware/teensy/avr/cores/teensy3/mk66fx1m0.ld\"", arduino); } break;
            case Model::Teensy40: { Fmt(&buf, " -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16 -larm_cortexM7lfsp_math \"-T%1/hardware/teensy/avr/cores/teensy4/imxrt1062.ld\"", arduino); } break;
            case Model::Teensy41: { Fmt(&buf, " -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16 -larm_cortexM7lfsp_math \"-T%1/hardware/teensy/avr/cores/teensy4/imxrt1062_t41.ld\"", arduino); } break;
            case Model::TeensyMM: { Fmt(&buf, " -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16 -larm_cortexM7lfsp_math \"-T%1/hardware/teensy/avr/cores/teensy4/imxrt1062_mm.ld\"", arduino); } break;
        }
        Fmt(&buf, " -lm -lstdc++");

        if (custom_flags) {
            Fmt(&buf, " %1", custom_flags);
        }

        out_cmd->cache_len = buf.len;
        if (FileIsVt100(STDOUT_FILENO)) {
            Fmt(&buf, " -fdiagnostics-color=always");
        } else {
            Fmt(&buf, " -fdiagnostics-color=never");
        }
        out_cmd->cmd_line = buf.TrimAndLeak(1);
    }

    void MakePostCommand(const char *src_filename, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        K_ASSERT(alloc);

        Span<const char> cmd = Fmt(alloc, "\"%1\" -O ihex -R .eeprom \"%2\" \"%3\"", objcopy, src_filename, dest_filename);
        out_cmd->cmd_line = cmd;
    }
};

std::unique_ptr<const Compiler> PrepareCompiler(HostSpecifier spec)
{
    static BlockAllocator str_alloc;

    if (spec.platform == NativePlatform) {
        if (!spec.cc) {
            if (spec.architecture == NativeArchitecture ||
                    spec.architecture == HostArchitecture::Unknown) {
                for (const KnownCompiler &known: KnownCompilers) {
                    if (!known.supported)
                        continue;

                    if (known.cc && FindExecutableInPath(known.cc)) {
                        spec.cc = known.cc;
                        break;
                    }
                }

                if (!spec.cc) {
                    LogError("Could not find any supported compiler in PATH");
                    return nullptr;
                }
            } else {
                LocalArray<const char *, 4> ccs;

#if defined(__linux__)
                switch (spec.architecture) {
                    case HostArchitecture::x86: { ccs.Append("i686-linux-gnu-gcc"); } break;
                    case HostArchitecture::x86_64: { ccs.Append("x86_64-linux-gnu-gcc"); } break;
                    case HostArchitecture::ARM64: { ccs.Append("aarch64-linux-gnu-gcc"); } break;
                    case HostArchitecture::RISCV64: { ccs.Append("riscv64-linux-gnu-gcc"); }  break;
                    case HostArchitecture::Loong64: { ccs.Append("loongarch64-linux-gnu-gcc"); } break;

                    case HostArchitecture::ARM32:
                    case HostArchitecture::Web32:
                    case HostArchitecture::Unknown: {} break;
                }
#endif

                ccs.Append("clang");

                for (const char *cc: ccs) {
                    if (FindExecutableInPath(cc)) {
                        spec.cc = cc;
                        break;
                    }
                }

                if (!spec.cc) {
                    LogError("Cannot find any compiler to build for '%1'", HostArchitectureNames[(int)spec.architecture]);
                    return nullptr;
                }
            }
        } else if (!FindExecutableInPath(spec.cc)) {
            LogError("Cannot find compiler '%1' in PATH", spec.cc);
            return nullptr;
        }

        if (spec.ld) {
            if (TestStr(spec.ld, "bfd") || TestStr(spec.ld, "ld")) {
                if (!FindExecutableInPath("ld.bfd")) {
                    LogError("Cannot find linker 'ld' in PATH");
                    return nullptr;
                }

                spec.ld = "bfd";
#if defined(_WIN32)
            } else if (TestStr(spec.ld, "link")) {
                if (!FindExecutableInPath("link")) {
                    LogError("Cannot find linker 'link.exe' in PATH");
                    return nullptr;
                }
#endif
            } else {
                char buf[512];
                Fmt(buf, "ld.%1", spec.ld);

                if (!FindExecutableInPath(buf)) {
                    LogError("Cannot find linker '%1' in PATH", buf);
                    return nullptr;
                }
            }
        }

        if (IdentifyCompiler(spec.cc, "clang")) {
            return ClangCompiler::Create(spec.platform, spec.architecture, spec.cc, spec.ld);
        } else if (IdentifyCompiler(spec.cc, "gcc")) {
            return GnuCompiler::Create(spec.platform, spec.architecture, spec.cc, spec.ld);
#if defined(_WIN32)
        } else if (IdentifyCompiler(spec.cc, "cl")) {
            if (spec.ld) {
                LogError("Cannot use custom linker with MSVC compiler");
                return nullptr;
            }

            return MsCompiler::Create(spec.architecture, spec.cc);
#endif
        } else {
            LogError("Cannot find driver for compiler '%1'", spec.cc);
            return nullptr;
        }
    } else if (StartsWith(HostPlatformNames[(int)spec.platform], "WASM/Emscripten/")) {
        if (!spec.cc) {
            spec.cc = "emcc";
        }

        if (spec.ld) {
            LogError("Cannot use custom linker for platform '%1'", HostPlatformNames[(int)spec.platform]);
            return nullptr;
        }

        return EmCompiler::Create(spec.platform, spec.cc);
    } else if (spec.platform == HostPlatform::WasmWasi) {
        static const WasiSdkInfo *sdk = FindWasiSdk();

        if (!sdk) {
            LogError("Cannot find WASI-SDK, set WASI_SDK_PATH manually");
            return nullptr;
        }

        spec.cc = spec.cc ? spec.cc : sdk->cc;

        if (spec.ld) {
            LogError("Cannot use custom linker for platform '%1'", HostPlatformNames[(int)spec.platform]);
            return nullptr;
        }

        return ClangCompiler::Create(spec.platform, HostArchitecture::Web32, spec.cc, nullptr, sdk->sysroot);
#if defined(__linux__)
    } else if (spec.platform == HostPlatform::Windows) {
        if (!spec.cc) {
            if (spec.architecture == HostArchitecture::Unknown) {
                spec.architecture = HostArchitecture::x86;
            }

            switch (spec.architecture) {
                case HostArchitecture::x86: {
                    if (FindExecutableInPath("i686-mingw-w64-gcc")) {
                        spec.cc = "i686-mingw-w64-gcc";
                    } else if (FindExecutableInPath("i686-w64-mingw32-gcc")) {
                        spec.cc = "i686-w64-mingw32-gcc";
                    }
                } break;

                case HostArchitecture::x86_64: {
                    if (FindExecutableInPath("x86_64-mingw-w64-gcc")) {
                        spec.cc = "x86_64-mingw-w64-gcc";
                    } else if (FindExecutableInPath("x86_64-w64-mingw32-gcc")) {
                        spec.cc = "x86_64-w64-mingw32-gcc";
                    }
                } break;

                case HostArchitecture::ARM64:
                case HostArchitecture::RISCV64:
                case HostArchitecture::Loong64:
                case HostArchitecture::ARM32:
                case HostArchitecture::Web32: {
                    LogError("Cannot use MinGW to cross-build for '%1'", HostArchitectureNames[(int)spec.architecture]);
                    return nullptr;
                } break;

                case HostArchitecture::Unknown: { K_UNREACHABLE(); } break;
            }

            if (!spec.cc) {
                LogError("Path to cross-platform MinGW must be explicitly specified");
                return nullptr;
            }
        }

        if (IdentifyCompiler(spec.cc, "mingw-w64") || IdentifyCompiler(spec.cc, "w64-mingw32")) {
            return GnuCompiler::Create(spec.platform, spec.architecture, spec.cc, spec.ld);
        } else {
            LogError("Only MinGW-w64 can be used for Windows cross-compilation at the moment");
            return nullptr;
        }
    } else if (spec.platform == HostPlatform::Linux) {
        // Go with GCC if not specified otherwise
        if (!spec.cc) {
            switch (spec.architecture) {
                case HostArchitecture::x86: { spec.cc = "i686-linux-gnu-gcc"; } break;
                case HostArchitecture::x86_64: { spec.cc = "x86_64-linux-gnu-gcc"; } break;
                case HostArchitecture::ARM64: { spec.cc = "aarch64-linux-gnu-gcc"; } break;
                case HostArchitecture::RISCV64: { spec.cc = "riscv64-linux-gnu-gcc"; }  break;
                case HostArchitecture::Loong64: { spec.cc = "loongarch64-linux-gnu-gcc"; }  break;

                case HostArchitecture::ARM32:
                case HostArchitecture::Web32: {
                    LogError("GCC cross-compilation for '%1' is not supported", HostArchitectureNames[(int)spec.architecture]);
                    return nullptr;
                } break;

                case HostArchitecture::Unknown: { K_UNREACHABLE(); } break;
            }
        }

        if (IdentifyCompiler(spec.cc, "gcc")) {
            return GnuCompiler::Create(spec.platform, spec.architecture, spec.cc, spec.ld);
        } else if (IdentifyCompiler(spec.cc, "clang")) {
            if (!spec.ld) {
                spec.ld = "lld";
            }
            if (!IdentifyCompiler(spec.ld, "lld")) {
                LogError("Use LLD for cross-compiling with Clang");
                return nullptr;
            }

            switch (spec.architecture)  {
                case HostArchitecture::x86:
                case HostArchitecture::x86_64:
                case HostArchitecture::ARM64:
                case HostArchitecture::RISCV64: {} break;
                case HostArchitecture::Loong64: {} break;

                case HostArchitecture::ARM32:
                case HostArchitecture::Web32: {
                    LogError("Clang cross-compilation for '%1' is not supported", HostArchitectureNames[(int)spec.architecture]);
                    return nullptr;
                } break;

                case HostArchitecture::Unknown: { K_UNREACHABLE(); } break;
            }

            return ClangCompiler::Create(spec.platform, spec.architecture, spec.cc, spec.ld);
        } else {
            LogError("Only GCC or Clang can be used for Linux cross-compilation at the moment");
            return nullptr;
        }
#endif
    } else if (StartsWith(HostPlatformNames[(int)spec.platform], "Embedded/Teensy/ARM/")) {
        static const char *arduino = FindArduinoSdk();

        if (!arduino) {
            LogError("Cannot find Arduino/Teensyduino, set ARDUINO_PATH manually");
            return nullptr;
        }

        if (spec.ld) {
            LogError("Cannot use custom linker for platform '%1'", HostPlatformNames[(int)spec.platform]);
            return nullptr;
        }

        return TeensyCompiler::Create(spec.platform, arduino, spec.cc);
    } else {
        LogError("Cross-compilation from platform '%1 (%2)' to '%3 (%4)' is not supported",
                 HostPlatformNames[(int)NativePlatform], HostArchitectureNames[(int)NativeArchitecture],
                 HostPlatformNames[(int)spec.platform], HostArchitectureNames[(int)spec.architecture]);
        return nullptr;
    }
}

bool DetermineSourceType(const char *filename, SourceType *out_type)
{
    Span<const char> extension = GetPathExtension(filename);

    SourceType type = SourceType::C;
    bool found = false;

    if (extension == ".c") {
        type = SourceType::C;
        found = true;
    } else if (extension == ".cc" || extension == ".cpp") {
        type = SourceType::Cxx;
        found = true;
    } else if (extension == ".S") {
        type = SourceType::GnuAssembly;
        found = true;
    } else if (extension == ".asm") {
        type = SourceType::MicrosoftAssembly;
        found = true;
    } else if (extension == ".o" || extension == ".obj") {
        type = SourceType::Object;
        found = true;
    } else if (extension == ".js" || extension == ".mjs" || extension == ".json" || extension == ".css") {
        type = SourceType::Esbuild;
        found = true;
    } else if (extension == ".ui") {
        type = SourceType::QtUi;
        found = true;
    } else if (extension == ".qrc") {
        type = SourceType::QtResources;
        found = true;
    }

    if (found && out_type) {
        *out_type = type;
    }
    return found;
}

static const KnownCompiler CompilerTable[] = {
#if defined(_WIN32)
    { "Clang", "clang", true },
    { "MSVC", "cl", true },
    { "GCC", "gcc", true },
#elif defined(__linux)
    { "GCC", "gcc", true },
    { "Clang", "clang", true },
    { "MSVC", "cl", false },
#else
    { "Clang", "clang", true },
    { "GCC", "gcc", true },
    { "MSVC", "cl", false },
#endif
    { "EmCC", "emcc", true }
};
const Span<const KnownCompiler> KnownCompilers = CompilerTable;

}
