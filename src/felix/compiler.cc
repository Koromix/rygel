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

static void MakePackCommand(Span<const char *const> pack_filenames, CompileOptimization compile_opt,
                            bool use_arrays, const char *pack_options, const char *dest_filename,
                            Allocator *alloc, Command *out_cmd)
{
    RG_ASSERT(alloc);

    HeapArray<char> buf(alloc);

    Fmt(&buf, "\"%1\" pack -O \"%2\"", GetApplicationExecutable(), dest_filename);

    Fmt(&buf, use_arrays ? "" : " -fUseLiterals");
    switch (compile_opt) {
        case CompileOptimization::None:
        case CompileOptimization::Debug: { Fmt(&buf, " -m SourceMap"); } break;
        case CompileOptimization::Fast:
        case CompileOptimization::LTO: { Fmt(&buf, " -m RunTransform"); } break;
    }

    if (pack_options) {
        Fmt(&buf, " %1", pack_options);
    }
    for (const char *pack_filename: pack_filenames) {
        Fmt(&buf, " \"%1\"", pack_filename);
    }

    out_cmd->cache_len = buf.len;
    out_cmd->cmd_line = buf.TrimAndLeak(1);
}

class ClangCompiler final: public Compiler {
    const char *cc;
    const char *cxx;
    const char *ld;

    int major_version;

    BlockAllocator str_alloc;

public:
    ClangCompiler() : Compiler("Clang") {}

    static std::unique_ptr<const Compiler> Create(const char *cc, const char *ld)
    {
        std::unique_ptr<ClangCompiler> compiler = std::make_unique<ClangCompiler>();

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

        // Determine Clang version
        {
            char cmd[1024];
            Fmt(cmd, "%1 --version", compiler->cc);

            HeapArray<char> output;
            int exit_code;
            if (!ExecuteCommandLine(cmd, {}, Kilobytes(1), &output, &exit_code))
                return nullptr;
            if (exit_code) {
                LogError("Command '%1 failed (exit code: %2)", cmd, exit_code);
                return nullptr;
            }

            Span<const char> remain = output;
            while (remain.len) {
                Span<const char> token = SplitStr(remain, ' ', &remain);

                if (token == "version") {
                    if (!ParseInt(remain, &compiler->major_version, 0, &remain)) {
                        LogError("Unexpected version format returned by '%1'", cmd);
                        return nullptr;
                    }
                    if (!remain.len || remain[0] != '.') {
                        LogError("Unexpected version format returned by '%1'", cmd);
                        return nullptr;
                    }

                    break;
                }
            }
        }

        return compiler;
    }

    bool CheckFeatures(CompileOptimization compile_opt, uint32_t features) const override
    {
        uint32_t supported = 0;

        supported |= (int)CompileFeature::PCH;
        supported |= (int)CompileFeature::NoDebug;
        supported |= (int)CompileFeature::StaticLink;
        supported |= (int)CompileFeature::ASan;
#ifndef _WIN32
        supported |= (int)CompileFeature::TSan;
#endif
        supported |= (int)CompileFeature::UBSan;
        supported |= (int)CompileFeature::StackProtect;
#ifndef _WIN32
        supported |= (int)CompileFeature::SafeStack;
#endif
        supported |= (int)CompileFeature::ZeroInit;
        supported |= (int)CompileFeature::CFI; // LTO only
#ifndef _WIN32
        supported |= (int)CompileFeature::ShuffleCode; // Requires LLD-11
#endif

        uint32_t unsupported = features & ~supported;
        if (unsupported) {
            LogError("Some features are not supported by %1: %2",
                     name, FmtFlags(unsupported, CompileFeatureNames));
            return false;
        }

        if ((features & (int)CompileFeature::ASan) && (features & (int)CompileFeature::TSan)) {
            LogError("Cannot use ASan and TSan at the same time");
            return false;
        }
        if (compile_opt != CompileOptimization::LTO && (features & (int)CompileFeature::CFI)) {
            LogError("Clang CFI feature requires LTO compilation");
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

    void MakePackCommand(Span<const char *const> pack_filenames, CompileOptimization compile_opt,
                         const char *pack_options, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        RG::MakePackCommand(pack_filenames, compile_opt, false, pack_options,
                            dest_filename, alloc, out_cmd);
    }

    void MakePchCommand(const char *pch_filename, SourceType src_type, CompileOptimization compile_opt,
                        bool warnings, Span<const char *const> definitions,
                        Span<const char *const> include_directories, uint32_t features, bool env_flags,
                        Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        MakeObjectCommand(pch_filename, src_type, compile_opt, warnings, nullptr, definitions,
                          include_directories, features, env_flags, nullptr, alloc, out_cmd);
    }

    const char *GetPchObject(const char *, Allocator *) const override { return nullptr; }

    void MakeObjectCommand(const char *src_filename, SourceType src_type, CompileOptimization compile_opt,
                           bool warnings, const char *pch_filename, Span<const char *const> definitions,
                           Span<const char *const> include_directories, uint32_t features, bool env_flags,
                           const char *dest_filename, Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(major_version >= 0);
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
        switch (compile_opt) {
            case CompileOptimization::None: { Fmt(&buf, " -O0 -ftrapv"); } break;
            case CompileOptimization::Debug: { Fmt(&buf, " -Og -ftrapv -fno-omit-frame-pointer"); } break;
            case CompileOptimization::Fast: { Fmt(&buf, " -O2 -DNDEBUG"); } break;
            case CompileOptimization::LTO: { Fmt(&buf, " -O2 -flto -DNDEBUG"); } break;
        }
        Fmt(&buf, " -fvisibility=hidden");
        Fmt(&buf, warnings ? " -Wall" : " -Wno-everything");

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
        if (major_version >= 11) {
            Fmt(&buf, " -fno-semantic-interposition");
        }
#else
        Fmt(&buf, " -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64"
                  " -pthread -fPIC");
        if (major_version >= 11) {
            Fmt(&buf, " -fno-semantic-interposition");
        }
        if (compile_opt == CompileOptimization::Fast || compile_opt == CompileOptimization::LTO) {
            Fmt(&buf, " -D_FORTIFY_SOURCE=2");
        }
#endif

        // Features
        if (!(features & (int)CompileFeature::NoDebug)) {
            Fmt(&buf, " -g");
        }
#ifdef _WIN32
        Fmt(&buf, (features & (int)CompileFeature::StaticLink) ? " -Xclang --dependent-lib=libcmt"
                                                               : " -Xclang --dependent-lib=msvcrt");
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
        if (features & (int)CompileFeature::StackProtect) {
            Fmt(&buf, " -fstack-protector-strong --param ssp-buffer-size=4");
#ifdef __linux__
            if (major_version >= 11) {
                Fmt(&buf, " -fstack-clash-protection");
            }
#endif
        }
        if (features & (int)CompileFeature::SafeStack) {
            Fmt(&buf, " -fsanitize=safe-stack");
        }
        if (features & (int)CompileFeature::ZeroInit) {
            Fmt(&buf, " -ftrivial-auto-var-init=zero -enable-trivial-auto-var-init-zero-knowing-it-will-be-removed-from-clang");
        }
        if (features & (int)CompileFeature::CFI) {
            RG_ASSERT(compile_opt == CompileOptimization::LTO);

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

    void MakeLinkCommand(Span<const char *const> obj_filenames, CompileOptimization compile_opt,
                         Span<const char *const> libraries, LinkType link_type,
                         uint32_t features, bool env_flags, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
            case LinkType::Executable: { Fmt(&buf, "%1", cxx); } break;
            case LinkType::SharedLibrary: { Fmt(&buf, "%1 -shared", cxx); } break;
        }
        Fmt(&buf, " -o \"%1\"", dest_filename);
        out_cmd->rsp_offset = buf.len;

        // Build mode
        switch (compile_opt) {
            case CompileOptimization::None:
            case CompileOptimization::Debug: {} break;
#ifdef _WIN32
            case CompileOptimization::Fast: { Fmt(&buf, " %1", link_type == LinkType::Executable ? " -static" : ""); } break;
            case CompileOptimization::LTO: { Fmt(&buf, " -flto%1", link_type == LinkType::Executable ? " -static" : ""); } break;
#else
            case CompileOptimization::Fast: {} break;
            case CompileOptimization::LTO: { Fmt(&buf, " -flto -Wl,-O1"); } break;
#endif
        }

        // Objects and libraries
        for (const char *obj_filename: obj_filenames) {
            Fmt(&buf, " \"%1\"", obj_filename);
        }
        for (const char *lib: libraries) {
            Fmt(&buf, " -l%1", lib);
        }

        // Platform flags
#if defined(_WIN32)
        Fmt(&buf, " -fuse-ld=lld --rtlib=compiler-rt");
#elif defined(__APPLE__)
        Fmt(&buf, " -ldl -pthread");
        if (link_type == LinkType::Executable) {
            Fmt(&buf, " -pie");
        }
#else
        Fmt(&buf, " -lrt -ldl -pthread -Wl,-z,relro,-z,now,-z,noexecstack,-z,separate-code");
        if (link_type == LinkType::Executable) {
            Fmt(&buf, " -pie");
        }
    #if defined(__arm__) || defined(__thumb__)
        Fmt(&buf, " -latomic");
    #endif
#endif

        // Features
        if (!(features & (int)CompileFeature::NoDebug)) {
            Fmt(&buf, " -g");
        }
        if (features & (int)CompileFeature::StaticLink) {
            Fmt(&buf, " -static");
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
        if (features & (int)CompileFeature::SafeStack) {
            Fmt(&buf, " -fsanitize=safe-stack");
        }
        if (features & (int)CompileFeature::CFI) {
            RG_ASSERT(compile_opt == CompileOptimization::LTO);
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

        return compiler;
    }

    bool CheckFeatures(CompileOptimization compile_opt, uint32_t features) const override
    {
        uint32_t supported = 0;

#ifndef _WIN32
        // Sometimes it works, somestimes not and the object files are
        // corrupt... just avoid PCH on MinGW
        supported |= (int)CompileFeature::PCH;
#endif
        supported |= (int)CompileFeature::NoDebug;
        supported |= (int)CompileFeature::StaticLink;
#ifndef _WIN32
        supported |= (int)CompileFeature::ASan;
        supported |= (int)CompileFeature::TSan;
        supported |= (int)CompileFeature::UBSan;
#endif
        supported |= (int)CompileFeature::StackProtect;

        uint32_t unsupported = features & ~supported;
        if (unsupported) {
            LogError("Some features are not supported by %1: %2",
                     name, FmtFlags(unsupported, CompileFeatureNames));
            return false;
        }

        if ((features & (int)CompileFeature::ASan) && (features & (int)CompileFeature::TSan)) {
            LogError("Cannot use ASan and TSan at the same time");
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

    void MakePackCommand(Span<const char *const> pack_filenames, CompileOptimization compile_opt,
                         const char *pack_options, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        RG::MakePackCommand(pack_filenames, compile_opt, false, pack_options,
                            dest_filename, alloc, out_cmd);
    }

    void MakePchCommand(const char *pch_filename, SourceType src_type, CompileOptimization compile_opt,
                        bool warnings, Span<const char *const> definitions,
                        Span<const char *const> include_directories, uint32_t features, bool env_flags,
                        Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        MakeObjectCommand(pch_filename, src_type, compile_opt, warnings, nullptr,
                          definitions, include_directories, features, env_flags, nullptr, alloc, out_cmd);
    }

    const char *GetPchObject(const char *, Allocator *) const override { return nullptr; }

    void MakeObjectCommand(const char *src_filename, SourceType src_type, CompileOptimization compile_opt,
                           bool warnings, const char *pch_filename, Span<const char *const> definitions,
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
        switch (compile_opt) {
            case CompileOptimization::None: { Fmt(&buf, " -O0 -fsanitize=signed-integer-overflow -fsanitize-undefined-trap-on-error"); } break;
            case CompileOptimization::Debug: { Fmt(&buf, " -Og -fsanitize=signed-integer-overflow -fsanitize-undefined-trap-on-error"
                                                     " -fno-omit-frame-pointer"); } break;
            case CompileOptimization::Fast: { Fmt(&buf, " -O2 -DNDEBUG"); } break;
            case CompileOptimization::LTO: { Fmt(&buf, " -O2 -flto -DNDEBUG"); } break;
        }
        if (warnings) {
            Fmt(&buf, " -Wall");
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
        if (compile_opt == CompileOptimization::Fast || compile_opt == CompileOptimization::LTO) {
            Fmt(&buf, " -D_FORTIFY_SOURCE=2");
        }
    #if defined(__arm__) || defined(__thumb__)
        Fmt(&buf, " -Wno-psabi");
    #endif
#endif

        // Features
        if (!(features & (int)CompileFeature::NoDebug)) {
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
        if (features & (int)CompileFeature::StackProtect) {
            Fmt(&buf, " -fstack-protector-strong --param ssp-buffer-size=4");
#ifndef _WIN32
            Fmt(&buf, " -fstack-clash-protection");
#endif
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

    void MakeLinkCommand(Span<const char *const> obj_filenames, CompileOptimization compile_opt,
                         Span<const char *const> libraries, LinkType link_type,
                         uint32_t features, bool env_flags, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
            case LinkType::Executable: { Fmt(&buf, "%1", cxx); } break;
            case LinkType::SharedLibrary: { Fmt(&buf, "%1 -shared", cxx); } break;
        }
        Fmt(&buf, " -o \"%1\"", dest_filename);
        out_cmd->rsp_offset = buf.len;

        // Build mode
        switch (compile_opt) {
            case CompileOptimization::None:
            case CompileOptimization::Debug: {} break;
#ifdef _WIN32
            case CompileOptimization::Fast: { Fmt(&buf, " -s%1", link_type == LinkType::Executable ? " -static" : ""); } break;
            case CompileOptimization::LTO: { Fmt(&buf, " -flto -Wl,-O1 -s%1", link_type == LinkType::Executable ? " -static" : ""); } break;
#else
            case CompileOptimization::Fast: { Fmt(&buf, " -s"); } break;
            case CompileOptimization::LTO: { Fmt(&buf, " -flto -Wl,-O1 -s"); } break;
#endif
        }

        // Objects and libraries
        for (const char *obj_filename: obj_filenames) {
            Fmt(&buf, " \"%1\"", obj_filename);
        }
        for (const char *lib: libraries) {
            Fmt(&buf, " -l%1", lib);
        }

        // Platform flags and libraries
#if defined(_WIN32)
        Fmt(&buf, " -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va");
#elif defined(__APPLE__)
        Fmt(&buf, " -ldl -pthread");
        if (link_type == LinkType::Executable) {
            Fmt(&buf, " -pie");
        }
#else
        Fmt(&buf, " -lrt -ldl -pthread -Wl,-z,relro,-z,now,-z,noexecstack,-z,separate-code");
        if (link_type == LinkType::Executable) {
            Fmt(&buf, " -pie");
        }
    #if defined(__arm__) || defined(__thumb__)
        Fmt(&buf, " -latomic");
    #endif
#endif

        // Features
        Fmt(&buf, (features & (int)CompileFeature::NoDebug) ? " -s" : " -g");
        if (features & (int)CompileFeature::StaticLink) {
            Fmt(&buf, " -static");
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
#ifdef _WIN32
        if (features & (int)CompileFeature::StackProtect) {
            Fmt(&buf, " -lssp");
        }
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

    bool CheckFeatures(CompileOptimization compile_opt, uint32_t features) const override
    {
        uint32_t supported = 0;

        supported |= (int)CompileFeature::PCH;
        supported |= (int)CompileFeature::NoDebug;
        supported |= (int)CompileFeature::StaticLink;
        supported |= (int)CompileFeature::ASan;
        supported |= (int)CompileFeature::StackProtect;
        supported |= (int)CompileFeature::CFI;

        uint32_t unsupported = features & ~supported;
        if (unsupported) {
            LogError("Some features are not supported by %1: %2",
                     name, FmtFlags(unsupported, CompileFeatureNames));
            return false;
        }

        return true;
    }

    const char *GetObjectExtension() const override { return ".obj"; }
    const char *GetExecutableExtension() const override { return ".exe"; }

    void MakePackCommand(Span<const char *const> pack_filenames, CompileOptimization compile_opt,
                         const char *pack_options, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        // Strings literals are limited in length in MSVC, even with concatenation (64kiB)
        RG::MakePackCommand(pack_filenames, compile_opt, true, pack_options,
                            dest_filename, alloc, out_cmd);
    }

    void MakePchCommand(const char *pch_filename, SourceType src_type, CompileOptimization compile_opt,
                        bool warnings, Span<const char *const> definitions,
                        Span<const char *const> include_directories, uint32_t features, bool env_flags,
                        Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        MakeObjectCommand(pch_filename, src_type, compile_opt, warnings, nullptr, definitions,
                          include_directories, features, env_flags, nullptr, alloc, out_cmd);
    }

    const char *GetPchObject(const char *pch_filename, Allocator *alloc) const override
    {
        RG_ASSERT(alloc);

        const char *obj_filename = Fmt(alloc, "%1.obj", pch_filename).ptr;
        return obj_filename;
    }

    void MakeObjectCommand(const char *src_filename, SourceType src_type, CompileOptimization compile_opt,
                           bool warnings, const char *pch_filename, Span<const char *const> definitions,
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
        Fmt(&buf, " /EHsc %1", warnings ? "/W3 /wd4200" : "/w");
        switch (compile_opt) {
            case CompileOptimization::None: { Fmt(&buf, " /Od"); } break;
            case CompileOptimization::Debug: { Fmt(&buf, " /O2"); } break;
            case CompileOptimization::Fast: { Fmt(&buf, " /O2 /DNDEBUG"); } break;
            case CompileOptimization::LTO: { Fmt(&buf, " /O2 /GL /DNDEBUG"); } break;
        }

        // Platform flags
        Fmt(&buf, " /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /DUNICODE /D_UNICODE"
                  " /D_LARGEFILE_SOURCE /D_LARGEFILE64_SOURCE /D_FILE_OFFSET_BITS=64"
                  " /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE");

        // Features
        if (!(features & (int)CompileFeature::NoDebug)) {
            Fmt(&buf, " /Z7 /Zo");
        }
        Fmt(&buf, (features & (int)CompileFeature::StaticLink) ? " /MT" : " /MD");
        if (features & (int)CompileFeature::ASan) {
            Fmt(&buf, " /fsanitize=address");
        }
        if (features & (int)CompileFeature::StackProtect) {
            Fmt(&buf, " /GS");
        }
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

    void MakeLinkCommand(Span<const char *const> obj_filenames, CompileOptimization compile_opt,
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
        switch (compile_opt) {
            case CompileOptimization::None:
            case CompileOptimization::Debug:
            case CompileOptimization::Fast: {} break;
            case CompileOptimization::LTO: { Fmt(&buf, " /LTCG"); } break;
        }
        Fmt(&buf, " /DYNAMICBASE /HIGHENTROPYVA");

        // Objects and libraries
        for (const char *obj_filename: obj_filenames) {
            Fmt(&buf, " \"%1\"", obj_filename);
        }
        for (const char *lib: libraries) {
            Fmt(&buf, " %1.lib", lib);
        }

        // Features
        Fmt(&buf, (features & (int)CompileFeature::NoDebug) ? " /DEBUG:NONE" : " /DEBUG:FULL");
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
