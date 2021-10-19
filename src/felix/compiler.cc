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

#include "../core/libcc/libcc.hh"
#include "compiler.hh"

namespace RG {

static bool SplitPrefixSuffix(const char *binary, const char *needle,
                              Span<const char> *out_prefix, Span<const char> *out_suffix)
{
    const char *ptr = strstr(binary, needle);
    if (!ptr) {
        LogError("Compiler binary path must contain '%1'", needle);
        return false;
    }

    *out_prefix = MakeSpan(binary, ptr - binary);
    *out_suffix = ptr + strlen(needle);

    return true;
}

static void AddEnvironmentFlags(Span<const char *const> names, HeapArray<char> *out_buf)
{
    for (const char *name: names) {
        const char *flags = getenv(name);

        if (flags && flags[0]) {
            Fmt(out_buf, " %1", flags);
        }
    }
}

static void MakePackCommand(Span<const char *const> pack_filenames, bool optimize,
                            bool use_arrays, const char *pack_options, const char *dest_filename,
                            Allocator *alloc, Command *out_cmd)
{
    RG_ASSERT(alloc);

    HeapArray<char> buf(alloc);

    Fmt(&buf, "\"%1\" pack -O \"%2\"", GetApplicationExecutable(), dest_filename);

    Fmt(&buf, optimize ? " -mRunTransform" : " -mSourceMap");
    Fmt(&buf, use_arrays ? "" : " -fUseLiterals");

    if (pack_options) {
        Fmt(&buf, " %1", pack_options);
    }
    for (const char *pack_filename: pack_filenames) {
        Fmt(&buf, " \"%1\"", pack_filename);
    }

    out_cmd->cache_len = buf.len;
    out_cmd->cmd_line = buf.TrimAndLeak(1);
}

static int ParseMajorVersion(const char *cmd, const char *marker)
{
    HeapArray<char> output;
    int exit_code;
    if (!ExecuteCommandLine(cmd, {}, Kilobytes(4), &output, &exit_code))
        return -1;
    if (exit_code) {
        LogDebug("Command '%1 failed (exit code: %2)", cmd, exit_code);
        return -1;
    }

    Span<const char> remain = output;
    while (remain.len) {
        Span<const char> token = SplitStr(remain, ' ', &remain);

        if (token == marker) {
            int version;
            if (!ParseInt(remain, &version, 0, &remain)) {
                LogError("Unexpected version format returned by '%1'", cmd);
                return -1;
            }
            if (!remain.len || remain[0] != '.') {
                LogError("Unexpected version format returned by '%1'", cmd);
                return -1;
            }

            return version;
        }
    }

    // Fail graciously
    return -1;
}

class ClangCompiler final: public Compiler {
    const char *cc;
    const char *cxx;
    const char *ld;

    bool clang11;
    bool lld11;

    BlockAllocator str_alloc;

public:
    ClangCompiler() : Compiler("Clang") {}

    static std::unique_ptr<const Compiler> Create(const char *cc, const char *ld)
    {
        std::unique_ptr<ClangCompiler> compiler = std::make_unique<ClangCompiler>();

        // Prefer LLD
        if (!ld && FindExecutableInPath("lld")) {
            ld = "lld";
        }

        // Find executables
        {
            Span<const char> prefix;
            Span<const char> suffix;
            if (!SplitPrefixSuffix(cc, "clang", &prefix, &suffix))
                return nullptr;

            compiler->cc = DuplicateString(cc, &compiler->str_alloc).ptr;
            compiler->cxx = Fmt(&compiler->str_alloc, "%1clang++%2", prefix, suffix).ptr;
            compiler->ld = ld ? DuplicateString(ld, &compiler->str_alloc).ptr : nullptr;
        }

        Async async;

        // Determine Clang version
        async.Run([&]() {
            char cmd[2048];
            Fmt(cmd, "%1 --version", compiler->cc);

            compiler->clang11 = ParseMajorVersion(cmd, "version") >= 11;
            return true;
        });

        // Determine LLD version
        async.Run([&]() {
            char cmd[2048];
            if (compiler->ld) {
                Fmt(cmd, "%1 -fuse-ld=%2 -Wl,--version", compiler->cc, compiler->ld);
            } else {
                Fmt(cmd, "%1 -Wl,--version", compiler->cc);
            }

            compiler->lld11 = ParseMajorVersion(cmd, "LLD") >= 11;
            return true;
        });

        async.Sync();

        return compiler;
    }

    bool CheckFeatures(uint32_t features) const override
    {
        uint32_t supported = 0;

        supported |= (int)CompileFeature::Optimize;
        supported |= (int)CompileFeature::PCH;
        supported |= (int)CompileFeature::DebugInfo;
        supported |= (int)CompileFeature::StaticLink;
        supported |= (int)CompileFeature::ASan;
#ifndef _WIN32
        supported |= (int)CompileFeature::TSan;
#endif
        supported |= (int)CompileFeature::UBSan;
        supported |= (int)CompileFeature::LTO;
#ifndef _WIN32
        supported |= (int)CompileFeature::SafeStack;
#endif
        supported |= (int)CompileFeature::ZeroInit;
        supported |= (int)CompileFeature::CFI; // LTO only
#ifndef _WIN32
        supported |= (int)CompileFeature::ShuffleCode; // Requires lld version >= 11
#endif

        uint32_t unsupported = features & ~supported;
        if (unsupported) {
            LogError("Some features are not supported by %1: %2",
                     name, FmtFlags(unsupported, CompileFeatureOptions));
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
        if (!lld11 && (features & (int)CompileFeature::ShuffleCode)) {
            LogError("ShuffleCode requires LLD >= 11, try --linker option (e.g. --linker=lld-11)");
            return false;
        }

        return true;
    }

#ifdef _WIN32
    const char *GetObjectExtension() const override { return ".obj"; }
    const char *GetExecutableExtension() const override { return ".exe"; }
#else
    const char *GetObjectExtension() const override { return ".o"; }
    const char *GetExecutableExtension() const override { return ""; }
#endif

    void MakePackCommand(Span<const char *const> pack_filenames, bool optimize,
                         const char *pack_options, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        RG::MakePackCommand(pack_filenames, optimize, false, pack_options,
                            dest_filename, alloc, out_cmd);
    }

    void MakePchCommand(const char *pch_filename, SourceType src_type, bool warnings,
                        Span<const char *const> definitions, Span<const char *const> include_directories,
                        uint32_t features, bool env_flags, Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        MakeObjectCommand(pch_filename, src_type, warnings, nullptr, definitions,
                          include_directories, features, env_flags, nullptr, alloc, out_cmd);
    }

    const char *GetPchCache(const char *pch_filename, Allocator *alloc) const override
    {
        RG_ASSERT(alloc);

        const char *cache_filename = Fmt(alloc, "%1.gch", pch_filename).ptr;
        return cache_filename;
    }
    const char *GetPchObject(const char *, Allocator *) const override { return nullptr; }

    void MakeObjectCommand(const char *src_filename, SourceType src_type, bool warnings,
                           const char *pch_filename, Span<const char *const> definitions,
                           Span<const char *const> include_directories, uint32_t features, bool env_flags,
                           const char *dest_filename, Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Compiler
        switch (src_type) {
            case SourceType::C: { Fmt(&buf, "%1 -std=gnu11", cc); } break;
            case SourceType::CXX: { Fmt(&buf, "%1 -std=gnu++2a", cxx); } break;
        }
        if (dest_filename) {
            Fmt(&buf, " -o \"%1\"", dest_filename);
        } else {
            switch (src_type) {
                case SourceType::C: { Fmt(&buf, " -x c-header"); } break;
                case SourceType::CXX: { Fmt(&buf, " -x c++-header"); } break;
            }
        }
        Fmt(&buf, " -MD -MF \"%1.d\"", dest_filename ? dest_filename : src_filename);
        out_cmd->rsp_offset = buf.len;

        // Build options
        Fmt(&buf, " -fvisibility=hidden");
        if (features & (int)CompileFeature::Optimize) {
            Fmt(&buf, " -O2 -DNDEBUG");
        } else {
            Fmt(&buf, " -O0 -ftrapv -fno-omit-frame-pointer");
        }
        if (features & (int)CompileFeature::LTO) {
            Fmt(&buf, " -flto");
        }
        if (warnings) {
            Fmt(&buf, " -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter");
        } else {
            Fmt(&buf, " -Wno-everything");
        }

        // Platform flags
#if defined(_WIN32)
        Fmt(&buf, " -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64"
                  " -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE"
                  " -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE"
                  " -D_MT -Xclang --dependent-lib=oldnames"
                  " -Wno-unknown-warning-option -Wno-unknown-pragmas -Wno-deprecated-declarations");

        if (src_type == SourceType::CXX) {
            Fmt(&buf, " -Xclang -flto-visibility-public-std -D_SILENCE_CLANG_CONCEPTS_MESSAGE");
        }
#elif defined(__APPLE__)
        Fmt(&buf, " -pthread -fPIC");
        if (clang11) {
            Fmt(&buf, " -fno-semantic-interposition");
        }
#else
        Fmt(&buf, " -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64"
                  " -pthread -fPIC");
        if (clang11) {
            Fmt(&buf, " -fno-semantic-interposition");
        }
        if (features & (int)CompileFeature::Optimize) {
            Fmt(&buf, " -D_FORTIFY_SOURCE=2");
        }
#endif

        // Features
        if (features & (int)CompileFeature::DebugInfo) {
            Fmt(&buf, " -g");
        }
#ifdef _WIN32
        if (features & (int)CompileFeature::StaticLink) {
            Fmt(&buf, " -Xclang --dependent-lib=libcmt");
        } else {
            Fmt(&buf, " -Xclang --dependent-lib=msvcrt");
        }
#endif
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
#ifdef __linux__
        if (clang11) {
            Fmt(&buf, " -fstack-clash-protection");
        }
#endif
        if (features & (int)CompileFeature::SafeStack) {
            Fmt(&buf, " -fsanitize=safe-stack");
        }
        if (features & (int)CompileFeature::ZeroInit) {
            Fmt(&buf, " -ftrivial-auto-var-init=zero -enable-trivial-auto-var-init-zero-knowing-it-will-be-removed-from-clang");
        }
        if (features & (int)CompileFeature::CFI) {
            RG_ASSERT(features & (int)CompileFeature::LTO);

            Fmt(&buf, " -fsanitize=cfi");
            if (src_type == SourceType::C) {
                // There's too much pointer type fuckery going on in C
                // to not take this precaution. Without it, SQLite3 crashes.
                Fmt(&buf, " -fsanitize-cfi-icall-generalize-pointers");
            }
        }
        if (features & (int)CompileFeature::ShuffleCode) {
            Fmt(&buf, " -ffunction-sections -fdata-sections");
        }

        // Sources and definitions
        Fmt(&buf, " -DFELIX -c \"%1\"", src_filename);
        if (pch_filename) {
            Fmt(&buf, " -include \"%1\"", pch_filename);
        }
        for (const char *definition: definitions) {
            Fmt(&buf, " -D%1", definition);
        }
        for (const char *include_directory: include_directories) {
            Fmt(&buf, " \"-I%1\"", include_directory);
        }

        if (env_flags) {
            switch (src_type) {
                case SourceType::C: { AddEnvironmentFlags({"CPPFLAGS", "CFLAGS"}, &buf); } break;
                case SourceType::CXX: { AddEnvironmentFlags({"CPPFLAGS", "CXXFLAGS"}, &buf); } break;
            }
        }

        out_cmd->cache_len = buf.len;
        if (FileIsVt100(stdout)) {
            Fmt(&buf, " -fcolor-diagnostics -fansi-escape-codes");
        }
        out_cmd->cmd_line = buf.TrimAndLeak(1);

        // Dependencies
        out_cmd->deps_mode = Command::DependencyMode::MakeLike;
        out_cmd->deps_filename = Fmt(alloc, "%1.d", dest_filename ? dest_filename : src_filename).ptr;
    }

    void MakeLinkCommand(Span<const char *const> obj_filenames,
                         Span<const char *const> libraries, LinkType link_type,
                         uint32_t features, bool env_flags, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
            case LinkType::Executable: {
                bool link_static = features & (int)CompileFeature::StaticLink;
                Fmt(&buf, "%1%2", cxx, link_static ? " -static" : "");
            } break;
            case LinkType::SharedLibrary: { Fmt(&buf, "%1 -shared", cxx); } break;
        }
        Fmt(&buf, " -o \"%1\"", dest_filename);
        out_cmd->rsp_offset = buf.len;

        // Build mode
#ifdef _WIN32
        if (features & (int)CompileFeature::LTO) {
            Fmt(&buf, " -flto");
        }
#else
        if (features & (int)CompileFeature::LTO) {
            Fmt(&buf, " -flto -Wl,-O1");
        }
#endif

        // Objects and libraries
        for (const char *obj_filename: obj_filenames) {
            Fmt(&buf, " \"%1\"", obj_filename);
        }
        for (const char *lib: libraries) {
#ifdef __APPLE__
            if (lib[0] == '!') {
                Fmt(&buf, " -framework %1", lib);
            } else {
                Fmt(&buf, " -l%1", lib);
            }
#else
            Fmt(&buf, " -l%1", lib);
#endif
        }

        // Platform flags
#if defined(_WIN32)
        Fmt(&buf, " --rtlib=compiler-rt -Wl,setargv.obj");
#elif defined(__APPLE__)
        Fmt(&buf, " -ldl -pthread -framework CoreFoundation -framework SystemConfiguration");
#else
        Fmt(&buf, " -lrt -ldl -pthread -Wl,-z,relro,-z,now,-z,noexecstack,-z,separate-code,-z,stack-size=1048576");
        if (link_type == LinkType::Executable) {
            Fmt(&buf, " -pie");
        }
    #if defined(__arm__) || defined(__thumb__)
        Fmt(&buf, " -latomic");
    #endif
#endif

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
        if (features & (int)CompileFeature::SafeStack) {
            Fmt(&buf, " -fsanitize=safe-stack");
        }
        if (features & (int)CompileFeature::CFI) {
            RG_ASSERT(features & (int)CompileFeature::LTO);
            Fmt(&buf, " -fsanitize=cfi");
        }
        if (features & (int)CompileFeature::ShuffleCode) {
            Fmt(&buf, " -Wl,--shuffle-sections=0");
        }

        if (ld) {
            Fmt(&buf, " -fuse-ld=%1", ld);
        }
        if (env_flags) {
            AddEnvironmentFlags("LDFLAGS", &buf);
        }

        out_cmd->cache_len = buf.len;
        if (FileIsVt100(stdout)) {
            Fmt(&buf, " -fcolor-diagnostics -fansi-escape-codes");
        }
        out_cmd->cmd_line = buf.TrimAndLeak(1);
    }
};

class GnuCompiler final: public Compiler {
    const char *cc;
    const char *cxx;
    const char *ld;

    bool gcc12;

    BlockAllocator str_alloc;

public:
    GnuCompiler() : Compiler("GCC") {}

    static std::unique_ptr<const Compiler> Create(const char *cc, const char *ld)
    {
        std::unique_ptr<GnuCompiler> compiler = std::make_unique<GnuCompiler>();

        // Find executables
        {
            Span<const char> prefix;
            Span<const char> suffix;
            if (!SplitPrefixSuffix(cc, "gcc", &prefix, &suffix))
                return nullptr;

            compiler->cc = DuplicateString(cc, &compiler->str_alloc).ptr;
            compiler->cxx = Fmt(&compiler->str_alloc, "%1g++%2", prefix, suffix).ptr;
            compiler->ld = ld ? DuplicateString(ld, &compiler->str_alloc).ptr : nullptr;
        }

        // Determine GCC version
        {
            char cmd[2048];
            Fmt(cmd, "%1 -v", compiler->cc);

            compiler->gcc12 = ParseMajorVersion(cmd, "version") >= 12;
        };

        return compiler;
    }

    bool CheckFeatures(uint32_t features) const override
    {
        uint32_t supported = 0;

        supported |= (int)CompileFeature::Optimize;
#ifndef _WIN32
        // Sometimes it works, somestimes not and the object files are
        // corrupt... just avoid PCH on MinGW
        supported |= (int)CompileFeature::PCH;
#endif
        supported |= (int)CompileFeature::DebugInfo;
        supported |= (int)CompileFeature::StaticLink;
#ifndef _WIN32
        supported |= (int)CompileFeature::ASan;
        supported |= (int)CompileFeature::TSan;
        supported |= (int)CompileFeature::UBSan;
        supported |= (int)CompileFeature::LTO;
#endif
        supported |= (int)CompileFeature::ZeroInit;

        uint32_t unsupported = features & ~supported;
        if (unsupported) {
            LogError("Some features are not supported by %1: %2",
                     name, FmtFlags(unsupported, CompileFeatureOptions));
            return false;
        }

        if ((features & (int)CompileFeature::ASan) && (features & (int)CompileFeature::TSan)) {
            LogError("Cannot use ASan and TSan at the same time");
            return false;
        }
        if (!gcc12 && (features & (int)CompileFeature::ZeroInit)) {
            LogError("ZeroInit requires GCC >= 12, try --compiler option (e.g. --compiler=gcc-12)");
            return false;
        }

        return true;
    }

#ifdef _WIN32
    const char *GetObjectExtension() const override { return ".o"; }
    const char *GetExecutableExtension() const override { return ".exe"; }
#else
    const char *GetObjectExtension() const override { return ".o"; }
    const char *GetExecutableExtension() const override { return ""; }
#endif

    void MakePackCommand(Span<const char *const> pack_filenames, bool optimize,
                         const char *pack_options, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        RG::MakePackCommand(pack_filenames, optimize, false, pack_options,
                            dest_filename, alloc, out_cmd);
    }

    void MakePchCommand(const char *pch_filename, SourceType src_type, bool warnings,
                        Span<const char *const> definitions, Span<const char *const> include_directories,
                        uint32_t features, bool env_flags, Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        MakeObjectCommand(pch_filename, src_type, warnings, nullptr,
                          definitions, include_directories, features, env_flags, nullptr, alloc, out_cmd);
    }

    const char *GetPchCache(const char *pch_filename, Allocator *alloc) const override
    {
        RG_ASSERT(alloc);

        const char *cache_filename = Fmt(alloc, "%1.gch", pch_filename).ptr;
        return cache_filename;
    }
    const char *GetPchObject(const char *, Allocator *) const override { return nullptr; }

    void MakeObjectCommand(const char *src_filename, SourceType src_type, bool warnings,
                           const char *pch_filename, Span<const char *const> definitions,
                           Span<const char *const> include_directories, uint32_t features, bool env_flags,
                           const char *dest_filename, Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Compiler
        switch (src_type) {
            case SourceType::C: { Fmt(&buf, "%1 -std=gnu11", cc); } break;
            case SourceType::CXX: { Fmt(&buf, "%1 -std=gnu++2a", cxx); } break;
        }
        if (dest_filename) {
            Fmt(&buf, " -o \"%1\"", dest_filename);
        } else {
            switch (src_type) {
                case SourceType::C: { Fmt(&buf, " -x c-header"); } break;
                case SourceType::CXX: { Fmt(&buf, " -x c++-header"); } break;
            }
        }
        Fmt(&buf, " -MD -MF \"%1.d\"", dest_filename ? dest_filename : src_filename);
        out_cmd->rsp_offset = buf.len;

        // Build options
        if (features & (int)CompileFeature::Optimize) {
            Fmt(&buf, " -O2 -DNDEBUG");
        } else {
            Fmt(&buf, " -O0 -fsanitize=signed-integer-overflow -fsanitize-undefined-trap-on-error -fno-omit-frame-pointer");
        }
        if (features & (int)CompileFeature::LTO) {
            Fmt(&buf, " -flto");
        }
        if (warnings) {
            Fmt(&buf, " -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter -Wno-cast-function-type");
            if (src_type == SourceType::CXX) {
                Fmt(&buf, " -Wno-class-memaccess -Wno-init-list-lifetime");
            }
        } else {
            Fmt(&buf, " -w");
        }
        Fmt(&buf, " -fvisibility=hidden");

        // Platform flags
#if defined(_WIN32)
        Fmt(&buf, " -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64"
                  " -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE"
                  " -D__USE_MINGW_ANSI_STDIO=1");
#elif defined(__APPLE__)
        Fmt(&buf, " -pthread -fPIC -fno-semantic-interposition");
#else
        Fmt(&buf, " -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64"
                  " -pthread -fPIC -fno-semantic-interposition");
        if (features & (int)CompileFeature::Optimize) {
            Fmt(&buf, " -D_FORTIFY_SOURCE=2");
        }
    #if defined(__arm__) || defined(__thumb__)
        Fmt(&buf, " -Wno-psabi");
    #endif
#endif

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
#ifndef _WIN32
        Fmt(&buf, " -fstack-clash-protection");
#endif
        if (features & (int)CompileFeature::ZeroInit) {
            Fmt(&buf, " -ftrivial-auto-var-init=zero");
        }

        // Sources and definitions
        Fmt(&buf, " -DFELIX -c \"%1\"", src_filename);
        if (pch_filename) {
            Fmt(&buf, " -include \"%1\"", pch_filename);
        }
        for (const char *definition: definitions) {
            Fmt(&buf, " -D%1", definition);
        }
        for (const char *include_directory: include_directories) {
            Fmt(&buf, " \"-I%1\"", include_directory);
        }

        if (env_flags) {
            switch (src_type) {
                case SourceType::C: { AddEnvironmentFlags({"CPPFLAGS", "CFLAGS"}, &buf); } break;
                case SourceType::CXX: { AddEnvironmentFlags({"CPPFLAGS", "CXXFLAGS"}, &buf); } break;
            }
        }

        out_cmd->cache_len = buf.len;
        if (FileIsVt100(stdout)) {
            Fmt(&buf, " -fdiagnostics-color=always");
        }
        out_cmd->cmd_line = buf.TrimAndLeak(1);

        // Dependencies
        out_cmd->deps_mode = Command::DependencyMode::MakeLike;
        out_cmd->deps_filename = Fmt(alloc, "%1.d", dest_filename ? dest_filename : src_filename).ptr;
    }

    void MakeLinkCommand(Span<const char *const> obj_filenames,
                         Span<const char *const> libraries, LinkType link_type,
                         uint32_t features, bool env_flags, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
            case LinkType::Executable: {
                bool static_link = features & (int)CompileFeature::StaticLink;
                Fmt(&buf, "%1%2", cxx, static_link ? " -static" : "");
            } break;
            case LinkType::SharedLibrary: { Fmt(&buf, "%1 -shared", cxx); } break;
        }
        Fmt(&buf, " -o \"%1\"", dest_filename);
        out_cmd->rsp_offset = buf.len;

        // Build mode
#ifdef _WIN32
        if (!(features & (int)CompileFeature::DebugInfo)) {
            Fmt(&buf, " -s");
        }
        if (features & (int)CompileFeature::LTO) {
            Fmt(&buf, " -flto -Wl,-O1");
        }
#else
        if (!(features & (int)CompileFeature::DebugInfo)) {
            Fmt(&buf, " -s");
        }
        if (features & (int)CompileFeature::LTO) {
            Fmt(&buf, " -flto -Wl,-O1");
        }
#endif

        // Objects and libraries
        for (const char *obj_filename: obj_filenames) {
            Fmt(&buf, " \"%1\"", obj_filename);
        }
        for (const char *lib: libraries) {
#ifdef __APPLE__
            if (lib[0] == '!') {
                Fmt(&buf, " -framework %1", lib);
            } else {
                Fmt(&buf, " -l%1", lib);
            }
#else
            Fmt(&buf, " -l%1", lib);
#endif
        }

        // Platform flags and libraries
#if defined(_WIN32)
        Fmt(&buf, " -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va");
#elif defined(__APPLE__)
        Fmt(&buf, " -ldl -pthread -framework CoreFoundation -framework SystemConfiguration");
#else
        Fmt(&buf, " -lrt -ldl -pthread -Wl,-z,relro,-z,now,-z,noexecstack,-z,separate-code,-z,stack-size=1048576");
        if (link_type == LinkType::Executable) {
            Fmt(&buf, " -pie");
        }
    #if defined(__arm__) || defined(__thumb__)
        Fmt(&buf, " -latomic");
    #endif
#endif

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
#ifdef _WIN32
        Fmt(&buf, " -lssp");
#endif

        if (ld) {
            Fmt(&buf, " -fuse-ld=%1", ld);
        }
        if (env_flags) {
            AddEnvironmentFlags("LDFLAGS", &buf);
        }

        out_cmd->cache_len = buf.len;
        if (FileIsVt100(stdout)) {
            Fmt(&buf, " -fdiagnostics-color=always");
        }
        out_cmd->cmd_line = buf.TrimAndLeak(1);
    }
};

#ifdef _WIN32
class MsCompiler final: public Compiler {
    const char *cl;
    const char *link;

    BlockAllocator str_alloc;

public:
    MsCompiler() : Compiler("MSVC") {}

    static std::unique_ptr<const Compiler> Create(const char *cl, const char *link)
    {
        std::unique_ptr<MsCompiler> compiler = std::make_unique<MsCompiler>();

        // Find executables
        {
            Span<const char> prefix;
            Span<const char> suffix;
            if (!SplitPrefixSuffix(cl, "cl", &prefix, &suffix))
                return nullptr;

            compiler->cl = DuplicateString(cl, &compiler->str_alloc).ptr;
            compiler->link = link ? DuplicateString(link, &compiler->str_alloc).ptr
                                  : Fmt(&compiler->str_alloc, "%1link%2", prefix, suffix).ptr;
        }

        return compiler;
    }

    bool CheckFeatures(uint32_t features) const override
    {
        uint32_t supported = 0;

        supported |= (int)CompileFeature::Optimize;
        supported |= (int)CompileFeature::PCH;
        supported |= (int)CompileFeature::DebugInfo;
        supported |= (int)CompileFeature::StaticLink;
        supported |= (int)CompileFeature::ASan;
        supported |= (int)CompileFeature::LTO;
        supported |= (int)CompileFeature::CFI;

        uint32_t unsupported = features & ~supported;
        if (unsupported) {
            LogError("Some features are not supported by %1: %2",
                     name, FmtFlags(unsupported, CompileFeatureOptions));
            return false;
        }

        return true;
    }

    const char *GetObjectExtension() const override { return ".obj"; }
    const char *GetExecutableExtension() const override { return ".exe"; }

    void MakePackCommand(Span<const char *const> pack_filenames, bool optimize,
                         const char *pack_options, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        // Strings literals are limited in length in MSVC, even with concatenation (64kiB)
        RG::MakePackCommand(pack_filenames, optimize, true, pack_options,
                            dest_filename, alloc, out_cmd);
    }

    void MakePchCommand(const char *pch_filename, SourceType src_type, bool warnings,
                        Span<const char *const> definitions, Span<const char *const> include_directories,
                        uint32_t features, bool env_flags, Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        MakeObjectCommand(pch_filename, src_type, warnings, nullptr, definitions,
                          include_directories, features, env_flags, nullptr, alloc, out_cmd);
    }

    const char *GetPchCache(const char *pch_filename, Allocator *alloc) const override
    {
        RG_ASSERT(alloc);

        const char *cache_filename = Fmt(alloc, "%1.pch", pch_filename).ptr;
        return cache_filename;
    }
    const char *GetPchObject(const char *pch_filename, Allocator *alloc) const override
    {
        RG_ASSERT(alloc);

        const char *obj_filename = Fmt(alloc, "%1.obj", pch_filename).ptr;
        return obj_filename;
    }

    void MakeObjectCommand(const char *src_filename, SourceType src_type, bool warnings,
                           const char *pch_filename, Span<const char *const> definitions,
                           Span<const char *const> include_directories, uint32_t features, bool env_flags,
                           const char *dest_filename, Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Compiler
        switch (src_type) {
            case SourceType::C: { Fmt(&buf, "%1 /nologo", cl); } break;
            case SourceType::CXX: { Fmt(&buf, "%1 /nologo /std:c++latest", cl); } break;
        }
        if (dest_filename) {
            Fmt(&buf, " \"/Fo%1\"", dest_filename);
        } else {
            Fmt(&buf, " /Yc \"/Fp%1.pch\" \"/Fo%1.obj\"", src_filename);
        }
        Fmt(&buf, " /showIncludes");
        out_cmd->rsp_offset = buf.len;

        // Build options
        Fmt(&buf, " /EHsc");
        if (features & (int)CompileFeature::Optimize) {
            Fmt(&buf, " /O2 /DNDEBUG");
        } else {
            Fmt(&buf, " /Od /RTCsu");
        }
        if (features & (int)CompileFeature::LTO) {
            Fmt(&buf, " /GL");
        }
        if (warnings) {
            Fmt(&buf, " /W4 /wd4200 /wd4458 /wd4706 /wd4100 /wd4127 /wd4702");
        } else {
            Fmt(&buf, " /w");
        }

        // Platform flags
        Fmt(&buf, " /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /DUNICODE /D_UNICODE"
                  " /D_LARGEFILE_SOURCE /D_LARGEFILE64_SOURCE /D_FILE_OFFSET_BITS=64"
                  " /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE");

        // Features
        if (features & (int)CompileFeature::DebugInfo) {
            Fmt(&buf, " /Z7 /Zo");
        }
        if (features & (int)CompileFeature::StaticLink) {
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

        // Sources and definitions
        Fmt(&buf, " /DFELIX /c /utf-8 \"%1\"", src_filename);
        if (pch_filename) {
            Fmt(&buf, " \"/FI%1\" \"/Yu%1\" \"/Fp%1.pch\"", pch_filename);
        }
        for (const char *definition: definitions) {
            Fmt(&buf, " /D%1", definition);
        }
        for (const char *include_directory: include_directories) {
            Fmt(&buf, " \"/I%1\"", include_directory);
        }

        if (env_flags) {
            switch (src_type) {
                case SourceType::C: { AddEnvironmentFlags({"CPPFLAGS", "CFLAGS"}, &buf); } break;
                case SourceType::CXX: { AddEnvironmentFlags({"CPPFLAGS", "CXXFLAGS"}, &buf); } break;
            }
        }

        out_cmd->cache_len = buf.len;
        out_cmd->cmd_line = buf.TrimAndLeak(1);
        out_cmd->skip_lines = 1;

        // Dependencies
        out_cmd->deps_mode = Command::DependencyMode::ShowIncludes;
    }

    void MakeLinkCommand(Span<const char *const> obj_filenames,
                         Span<const char *const> libraries, LinkType link_type,
                         uint32_t features, bool env_flags, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
            case LinkType::Executable: { Fmt(&buf, "%1 /nologo", link); } break;
            case LinkType::SharedLibrary: { Fmt(&buf, "%1 /nologo /DLL", link); } break;
        }
        Fmt(&buf, " \"/OUT:%1\"", dest_filename);
        out_cmd->rsp_offset = buf.len;

        // Build mode
        if (features & (int)CompileFeature::LTO) {
            Fmt(&buf, " /LTCG");
        }
        Fmt(&buf, " /DYNAMICBASE /HIGHENTROPYVA");

        // Objects and libraries
        for (const char *obj_filename: obj_filenames) {
            Fmt(&buf, " \"%1\"", obj_filename);
        }
        for (const char *lib: libraries) {
            Fmt(&buf, " %1.lib", lib);
        }
        Fmt(&buf, " setargv.obj");

        // Features
        if (features & (int)CompileFeature::DebugInfo) {
            Fmt(&buf, " /DEBUG:FULL");
        } else {
            Fmt(&buf, " /DEBUG:NONE");
        }
        if (features & (int)CompileFeature::CFI) {
            Fmt(&buf, " /guard:cf /guard:ehcont");
        }

        if (env_flags) {
            AddEnvironmentFlags("LDFLAGS", &buf);
        }

        out_cmd->cache_len = buf.len;
        out_cmd->cmd_line = buf.TrimAndLeak(1);
        out_cmd->skip_success = true;
    }
};
#endif

std::unique_ptr<const Compiler> PrepareCompiler(CompilerInfo info)
{
    if (!info.cc) {
        for (const SupportedCompiler &supported: SupportedCompilers) {
            if (FindExecutableInPath(supported.cc)) {
                info.cc = supported.cc;
                break;
            }
        }

        if (!info.cc) {
            LogError("Could not find any supported compiler in PATH");
            return nullptr;
        }
    } else if (!FindExecutableInPath(info.cc)) {
        LogError("Cannot find compiler '%1' in PATH", info.cc);
        return nullptr;
    }

    if (info.ld) {
        if (TestStr(info.ld, "bfd") || TestStr(info.ld, "ld")) {
            if (!FindExecutableInPath("ld")) {
                LogError("Cannot find linker 'ld' in PATH");
                return nullptr;
            }

            info.ld = "bfd";
        } else if (!FindExecutableInPath(info.ld)) {
            LogError("Cannot find linker '%1' in PATH", info.ld);
            return nullptr;
        }
    }

    // Find appropriate driver
    {
        Span<const char> remain = SplitStrReverseAny(info.cc, RG_PATH_SEPARATORS).ptr;

        while (remain.len) {
            Span<const char> part = SplitStr(remain, '-', &remain);

            if (part == "clang") {
                return ClangCompiler::Create(info.cc, info.ld);
            } else if (part == "gcc") {
                return GnuCompiler::Create(info.cc, info.ld);
#ifdef _WIN32
            } else if (part == "cl") {
                return MsCompiler::Create(info.cc, info.ld);
#endif
            }
        }
    }

    LogError("Cannot find driver for compiler '%1'", info.cc);
    return nullptr;
}

static const SupportedCompiler CompilerTable[] = {
#if defined(_WIN32)
    {"MSVC", "cl"},
    {"Clang", "clang"},
    {"GCC", "gcc"}
#elif defined(__APPLE__)
    {"Clang", "clang"},
    {"GCC", "gcc"}
#else
    {"GCC", "gcc"},
    {"Clang", "clang"}
#endif
};
const Span<const SupportedCompiler> SupportedCompilers = CompilerTable;

}
