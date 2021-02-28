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

static void AddEnvironmentFlags(Span<const char *const> names, HeapArray<char> *out_buf)
{
    for (const char *name: names) {
        const char *flags = getenv(name);

        if (flags && flags[0]) {
            Fmt(out_buf, " %1", flags);
        }
    }
}

static void MakePackCommand(Span<const char *const> pack_filenames, CompileMode compile_mode,
                            bool use_arrays, const char *pack_options, const char *dest_filename,
                            Allocator *alloc, Command *out_cmd)
{
    RG_ASSERT(alloc);

    HeapArray<char> buf(alloc);

    Fmt(&buf, "\"%1\" pack -O \"%2\"", GetApplicationExecutable(), dest_filename);

    Fmt(&buf, use_arrays ? " -t Carray" : " -t C");
    switch (compile_mode) {
        case CompileMode::Debug:
        case CompileMode::DebugFast: { Fmt(&buf, " -m SourceMap"); } break;
        case CompileMode::Fast:
        case CompileMode::LTO: { Fmt(&buf, " -m RunTransform"); } break;
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

bool Compiler::Test() const
{
    if (!test_init) {
        test = FindExecutableInPath(binary);
        test_init = true;
    }

    return test;
}

void Compiler::LogUnsupportedFeatures(uint32_t unsupported) const
{
    LocalArray<const char *, RG_LEN(CompileFeatureNames)> list;
    for (int i = 0; i < RG_LEN(CompileFeatureNames); i++) {
        if (unsupported & (1u << i)) {
            list.Append(CompileFeatureNames[i]);
        }
    }

    LogError("Some features are not supported by %1: %2", name, FmtSpan((Span<const char *>)list));
}

class ClangCompiler final: public Compiler {
public:
    ClangCompiler(const char *name) : Compiler(name, "clang") {}

    bool CheckFeatures(CompileMode compile_mode, uint32_t features) const override
    {
        uint32_t supported = 0;

        supported |= (int)CompileFeature::NoDebug;
        supported |= (int)CompileFeature::Static;
        supported |= (int)CompileFeature::ASan;
#ifndef _WIN32
        supported |= (int)CompileFeature::TSan;
#endif
        supported |= (int)CompileFeature::UBSan;
        supported |= (int)CompileFeature::ProtectStack;
        supported |= (int)CompileFeature::CFI; // LTO only

        uint32_t unsupported = features & ~supported;
        if (unsupported) {
            LogUnsupportedFeatures(unsupported);
            return false;
        }

        if (compile_mode != CompileMode::LTO && (features & (int)CompileFeature::CFI)) {
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

    void MakePackCommand(Span<const char *const> pack_filenames, CompileMode compile_mode,
                         const char *pack_options, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        RG::MakePackCommand(pack_filenames, compile_mode, false, pack_options,
                            dest_filename, alloc, out_cmd);
    }

    void MakePchCommand(const char *pch_filename, SourceType src_type, CompileMode compile_mode,
                        bool warnings, Span<const char *const> definitions,
                        Span<const char *const> include_directories, uint32_t features, bool env_flags,
                        Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        MakeObjectCommand(pch_filename, src_type, compile_mode, warnings, nullptr, definitions,
                          include_directories, features, env_flags, nullptr, alloc, out_cmd);
    }

    const char *GetPchObject(const char *, Allocator *) const override { return nullptr; }

    void MakeObjectCommand(const char *src_filename, SourceType src_type, CompileMode compile_mode,
                           bool warnings, const char *pch_filename, Span<const char *const> definitions,
                           Span<const char *const> include_directories, uint32_t features, bool env_flags,
                           const char *dest_filename, Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Compiler
        switch (src_type) {
            case SourceType::C: { Fmt(&buf, "clang -std=gnu11"); } break;
            case SourceType::CXX: { Fmt(&buf, "clang++ -std=gnu++2a"); } break;
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
        switch (compile_mode) {
            case CompileMode::Debug: { Fmt(&buf, " -O0 -ftrapv"); } break;
            case CompileMode::DebugFast: { Fmt(&buf, " -Og -ftrapv -fno-omit-frame-pointer"); } break;
            case CompileMode::Fast: { Fmt(&buf, " -O2 -DNDEBUG"); } break;
            case CompileMode::LTO: { Fmt(&buf, " -O2 -flto -DNDEBUG"); } break;
        }
        Fmt(&buf, " -fvisibility=hidden");
        Fmt(&buf, warnings ? " -Wall" : " -Wno-everything");

        // Platform flags
#if defined(_WIN32)
        Fmt(&buf, " -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64"
                  " -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE"
                  " -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE"
                  " -D_MT -Xclang --dependent-lib=oldnames"
                  " -Wno-unknown-warning-option -Wno-unknown-pragmas -Wno-deprecated-declarations");

        if (src_type == SourceType::CXX) {
            Fmt(&buf, " -Xclang -flto-visibility-public-std -D_SILENCE_CLANG_CONCEPTS_MESSAGE");
        }
#elif defined(__APPLE__)
        Fmt(&buf, " -pthread -fPIC");
#else
        Fmt(&buf, " -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64"
                  " -pthread -fPIC");
        if (compile_mode == CompileMode::Fast || compile_mode == CompileMode::LTO) {
            Fmt(&buf, " -D_FORTIFY_SOURCE=2");
        }
#endif

        // Features
        if (!(features & (int)CompileFeature::NoDebug)) {
            Fmt(&buf, " -g");
        }
#ifdef _WIN32
        Fmt(&buf, (features & (int)CompileFeature::Static) ? " -Xclang --dependent-lib=libcmt"
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
        if (features & (int)CompileFeature::ProtectStack) {
            Fmt(&buf, " -fstack-protector-strong --param ssp-buffer-size=4");
// #ifndef _WIN32
//             Fmt(&buf, " -fstack-clash-protection");
// #endif
        }
        if (features & (int)CompileFeature::CFI) {
            RG_ASSERT(compile_mode == CompileMode::LTO);
            Fmt(&buf, " -fsanitize=cfi");
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

    void MakeLinkCommand(Span<const char *const> obj_filenames, CompileMode compile_mode,
                         Span<const char *const> libraries, LinkType link_type,
                         uint32_t features, bool env_flags, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
            case LinkType::Executable: { Fmt(&buf, "clang++"); } break;
            case LinkType::SharedLibrary: { Fmt(&buf, "clang++ -shared"); } break;
        }
        Fmt(&buf, " -o \"%1\"", dest_filename);
        out_cmd->rsp_offset = buf.len;

        // Build mode
        switch (compile_mode) {
            case CompileMode::Debug:
            case CompileMode::DebugFast: {} break;
#ifdef _WIN32
            case CompileMode::Fast: { Fmt(&buf, " %1", link_type == LinkType::Executable ? " -static" : ""); } break;
            case CompileMode::LTO: { Fmt(&buf, " -flto%1", link_type == LinkType::Executable ? " -static" : ""); } break;
#else
            case CompileMode::Fast: {} break;
            case CompileMode::LTO: { Fmt(&buf, " -flto -Wl,-O1"); } break;
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
#else
        Fmt(&buf, " -lrt -ldl -pthread -Wl,-z,relro,-z,now,-z,noexecstack,-z,separate-code");
    #if defined(__arm__) || defined(__thumb__)
        Fmt(&buf, " -latomic");
    #endif
#endif

        // Features
        if (!(features & (int)CompileFeature::NoDebug)) {
            Fmt(&buf, " -g");
        }
        if (features & (int)CompileFeature::Static) {
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
        if (features & (int)CompileFeature::CFI) {
            RG_ASSERT(compile_mode == CompileMode::LTO);
            Fmt(&buf, " -fsanitize=cfi");
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
public:
    GnuCompiler(const char *name) : Compiler(name, "gcc") {}

    bool CheckFeatures(CompileMode compile_mode, uint32_t features) const override
    {
        uint32_t supported = 0;

        supported |= (int)CompileFeature::NoDebug;
        supported |= (int)CompileFeature::Static;
#ifndef _WIN32
        supported |= (int)CompileFeature::ASan;
        supported |= (int)CompileFeature::TSan;
        supported |= (int)CompileFeature::UBSan;
#endif
        supported |= (int)CompileFeature::ProtectStack;

        uint32_t unsupported = features & ~supported;
        if (unsupported) {
            LogUnsupportedFeatures(unsupported);
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

    void MakePackCommand(Span<const char *const> pack_filenames, CompileMode compile_mode,
                         const char *pack_options, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        RG::MakePackCommand(pack_filenames, compile_mode, false, pack_options,
                            dest_filename, alloc, out_cmd);
    }

    void MakePchCommand(const char *pch_filename, SourceType src_type, CompileMode compile_mode,
                        bool warnings, Span<const char *const> definitions,
                        Span<const char *const> include_directories, uint32_t features, bool env_flags,
                        Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        MakeObjectCommand(pch_filename, src_type, compile_mode, warnings, nullptr,
                          definitions, include_directories, features, env_flags, nullptr, alloc, out_cmd);
    }

    const char *GetPchObject(const char *, Allocator *) const override { return nullptr; }

    void MakeObjectCommand(const char *src_filename, SourceType src_type, CompileMode compile_mode,
                           bool warnings, const char *pch_filename, Span<const char *const> definitions,
                           Span<const char *const> include_directories, uint32_t features, bool env_flags,
                           const char *dest_filename, Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Compiler
        switch (src_type) {
            case SourceType::C: { Fmt(&buf, "gcc -std=gnu11"); } break;
            case SourceType::CXX: { Fmt(&buf, "g++ -std=gnu++2a"); } break;
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
        switch (compile_mode) {
            case CompileMode::Debug: { Fmt(&buf, " -O0 -fsanitize=signed-integer-overflow -fsanitize-undefined-trap-on-error"); } break;
            case CompileMode::DebugFast: { Fmt(&buf, " -Og -fsanitize=signed-integer-overflow -fsanitize-undefined-trap-on-error"
                                                     " -fno-omit-frame-pointer"); } break;
            case CompileMode::Fast: { Fmt(&buf, " -O2 -DNDEBUG"); } break;
            case CompileMode::LTO: { Fmt(&buf, " -O2 -flto -DNDEBUG"); } break;
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
                  " -D__USE_MINGW_ANSI_STDIO=1 -DNOMINMAX");
#elif defined(__APPLE__)
        Fmt(&buf, " -pthread -fPIC");
#else
        Fmt(&buf, " -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64"
                  " -pthread -fPIC");
        if (compile_mode == CompileMode::Fast || compile_mode == CompileMode::LTO) {
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
        if (features & (int)CompileFeature::ProtectStack) {
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

    void MakeLinkCommand(Span<const char *const> obj_filenames, CompileMode compile_mode,
                         Span<const char *const> libraries, LinkType link_type,
                         uint32_t features, bool env_flags, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
            case LinkType::Executable: { Fmt(&buf, "g++"); } break;
            case LinkType::SharedLibrary: { Fmt(&buf, "g++ -shared"); } break;
        }
        Fmt(&buf, " -o \"%1\"", dest_filename);
        out_cmd->rsp_offset = buf.len;

        // Build mode
        switch (compile_mode) {
            case CompileMode::Debug:
            case CompileMode::DebugFast: {} break;
#ifdef _WIN32
            case CompileMode::Fast: { Fmt(&buf, " -s%1", link_type == LinkType::Executable ? " -static" : ""); } break;
            case CompileMode::LTO: { Fmt(&buf, " -flto -Wl,-O1 -s%1", link_type == LinkType::Executable ? " -static" : ""); } break;
#else
            case CompileMode::Fast: { Fmt(&buf, " -s"); } break;
            case CompileMode::LTO: { Fmt(&buf, " -flto -Wl,-O1 -s"); } break;
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
        if (features & (int)CompileFeature::Static) {
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
        if (features & (int)CompileFeature::ProtectStack) {
            Fmt(&buf, " -lssp");
        }
#endif

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
public:
    MsCompiler(const char *name) : Compiler(name, "cl") {}

    bool CheckFeatures(CompileMode compile_mode, uint32_t features) const override
    {
        uint32_t supported = 0;

        supported |= (int)CompileFeature::NoDebug;
        supported |= (int)CompileFeature::Static;
        supported |= (int)CompileFeature::ProtectStack;
        supported |= (int)CompileFeature::CFI;

        uint32_t unsupported = features & ~supported;
        if (unsupported) {
            LogUnsupportedFeatures(unsupported);
            return false;
        }

        return true;
    }

    const char *GetObjectExtension() const override { return ".obj"; }
    const char *GetExecutableExtension() const override { return ".exe"; }

    void MakePackCommand(Span<const char *const> pack_filenames, CompileMode compile_mode,
                         const char *pack_options, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        // Strings literals are limited in length in MSVC, even with concatenation (64kiB)
        RG::MakePackCommand(pack_filenames, compile_mode, true, pack_options,
                            dest_filename, alloc, out_cmd);
    }

    void MakePchCommand(const char *pch_filename, SourceType src_type, CompileMode compile_mode,
                        bool warnings, Span<const char *const> definitions,
                        Span<const char *const> include_directories, uint32_t features, bool env_flags,
                        Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        MakeObjectCommand(pch_filename, src_type, compile_mode, warnings, nullptr, definitions,
                          include_directories, features, env_flags, nullptr, alloc, out_cmd);
    }

    const char *GetPchObject(const char *pch_filename, Allocator *alloc) const override
    {
        RG_ASSERT(alloc);

        const char *obj_filename = Fmt(alloc, "%1.obj", pch_filename).ptr;
        return obj_filename;
    }

    void MakeObjectCommand(const char *src_filename, SourceType src_type, CompileMode compile_mode,
                           bool warnings, const char *pch_filename, Span<const char *const> definitions,
                           Span<const char *const> include_directories, uint32_t features, bool env_flags,
                           const char *dest_filename, Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Compiler
        switch (src_type) {
            case SourceType::C: { Fmt(&buf, "cl /nologo"); } break;
            case SourceType::CXX: { Fmt(&buf, "cl /nologo /std:c++latest"); } break;
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
        switch (compile_mode) {
            case CompileMode::Debug: { Fmt(&buf, " /Od"); } break;
            case CompileMode::DebugFast: { Fmt(&buf, " /O2"); } break;
            case CompileMode::Fast: { Fmt(&buf, " /O2 /DNDEBUG"); } break;
            case CompileMode::LTO: { Fmt(&buf, " /O2 /GL /DNDEBUG"); } break;
        }

        // Platform flags
        Fmt(&buf, " /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /DUNICODE /D_UNICODE"
                  " /D_LARGEFILE_SOURCE /D_LARGEFILE64_SOURCE /D_FILE_OFFSET_BITS=64"
                  " /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE");

        // Features
        if (!(features & (int)CompileFeature::NoDebug)) {
            Fmt(&buf, " /Z7 /Zo");
        }
        Fmt(&buf, (features & (int)CompileFeature::Static) ? " /MT" : " /MD");
        if (features & (int)CompileFeature::ProtectStack) {
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

    void MakeLinkCommand(Span<const char *const> obj_filenames, CompileMode compile_mode,
                         Span<const char *const> libraries, LinkType link_type,
                         uint32_t features, bool env_flags, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
            case LinkType::Executable: { Fmt(&buf, "link /nologo"); } break;
            case LinkType::SharedLibrary: { Fmt(&buf, "link /nologo /DLL"); } break;
        }
        Fmt(&buf, " \"/OUT:%1\"", dest_filename);
        out_cmd->rsp_offset = buf.len;

        // Build mode
        switch (compile_mode) {
            case CompileMode::Debug:
            case CompileMode::DebugFast:
            case CompileMode::Fast: {} break;
            case CompileMode::LTO: { Fmt(&buf, " /LTCG"); } break;
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

class EmsdkCompiler final: public Compiler {
public:
    EmsdkCompiler(const char *name) : Compiler(name, "emcc") {}

    bool CheckFeatures(CompileMode compile_mode, uint32_t features) const override
    {
        uint32_t supported = 0;

        supported |= (int)CompileFeature::NoDebug;
        supported |= (int)CompileFeature::Static;

        uint32_t unsupported = features & ~supported;
        if (unsupported) {
            LogUnsupportedFeatures(unsupported);
            return false;
        }

        return true;
    }

    const char *GetObjectExtension() const override { return ".o"; }
    const char *GetExecutableExtension() const override { return ".js"; }

    void MakePackCommand(Span<const char *const> pack_filenames, CompileMode compile_mode,
                         const char *pack_options, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        RG::MakePackCommand(pack_filenames, compile_mode, false, pack_options,
                            dest_filename, alloc, out_cmd);
    }

    void MakePchCommand(const char *pch_filename, SourceType src_type, CompileMode compile_mode,
                        bool warnings, Span<const char *const> definitions,
                        Span<const char *const> include_directories, uint32_t features, bool env_flags,
                        Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);
        MakeObjectCommand(pch_filename, src_type, compile_mode, warnings, nullptr,
                          definitions, include_directories, features, env_flags, nullptr, alloc, out_cmd);
    }

    const char *GetPchObject(const char *, Allocator *) const override { return nullptr; }

    void MakeObjectCommand(const char *src_filename, SourceType src_type, CompileMode compile_mode,
                           bool warnings, const char *pch_filename, Span<const char *const> definitions,
                           Span<const char *const> include_directories, uint32_t features, bool env_flags,
                           const char *dest_filename, Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Compiler
        switch (src_type) {
#ifdef _WIN32
            case SourceType::C: { Fmt(&buf, "emcc.bat -std=gnu11"); } break;
            case SourceType::CXX: { Fmt(&buf, "em++.bat -std=gnu++2a"); } break;
#else
            case SourceType::C: { Fmt(&buf, "emcc -std=gnu11"); } break;
            case SourceType::CXX: { Fmt(&buf, "em++ -std=gnu++2a"); } break;
#endif
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
        switch (compile_mode) {
            case CompileMode::Debug: { Fmt(&buf, " -O0 -ftrapv"); } break;
            case CompileMode::DebugFast: { Fmt(&buf, " -Og -ftrapv"); } break;
            case CompileMode::Fast: { Fmt(&buf, " -O2 -DNDEBUG"); } break;
            case CompileMode::LTO: { Fmt(&buf, " -O2 -flto -DNDEBUG"); } break;
        }
        Fmt(&buf, warnings ? " -Wall" : " -w");
        Fmt(&buf, " -fvisibility=hidden");

        // Platform flags
        Fmt(&buf, " -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64");

        // Features
        if (!(features & (int)CompileFeature::NoDebug)) {
            Fmt(&buf, " -g");
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

    void MakeLinkCommand(Span<const char *const> obj_filenames, CompileMode compile_mode,
                         Span<const char *const> libraries, LinkType link_type,
                         uint32_t features, bool env_flags, const char *dest_filename,
                         Allocator *alloc, Command *out_cmd) const override
    {
        RG_ASSERT(alloc);

        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
#ifdef _WIN32
            case LinkType::Executable: { Fmt(&buf, "emcc.bat"); } break;
            case LinkType::SharedLibrary: { Fmt(&buf, "emcc.bat -shared"); } break;
#else
            case LinkType::Executable: { Fmt(&buf, "emcc"); } break;
            case LinkType::SharedLibrary: { Fmt(&buf, "emcc -shared"); } break;
#endif
        }
        Fmt(&buf, " -o \"%1\"", dest_filename);
        out_cmd->rsp_offset = buf.len;

        // Build mode
        switch (compile_mode) {
            case CompileMode::Debug:
            case CompileMode::DebugFast: {} break;
            case CompileMode::Fast: { Fmt(&buf, " -s"); } break;
            case CompileMode::LTO: { Fmt(&buf, " -flto -s"); } break;
        }

        // Objects and libraries
        for (const char *obj_filename: obj_filenames) {
            Fmt(&buf, " \"%1\"", obj_filename);
        }
        for (const char *lib: libraries) {
            Fmt(&buf, " -l%1", lib);
        }
        Fmt(&buf, " -lnodefs.js");

        // Features
        Fmt(&buf, (features & (int)CompileFeature::NoDebug) ? " -s" : " -g");

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

static ClangCompiler ClangCompiler("Clang");
static GnuCompiler GnuCompiler("GCC");
#ifdef _WIN32
static MsCompiler MsCompiler("MSVC");
#endif
static EmsdkCompiler EmsdkCompiler("EmSDK");

static const Compiler *const CompilerTable[] = {
#if defined(_WIN32)
    &MsCompiler,
    &ClangCompiler,
    &GnuCompiler,
    &EmsdkCompiler
#elif defined(__APPLE__)
    &ClangCompiler,
    &GnuCompiler,
    &EmsdkCompiler
#else
    &GnuCompiler,
    &ClangCompiler,
    &EmsdkCompiler
#endif
};
const Span<const Compiler *const> Compilers = CompilerTable;

}
