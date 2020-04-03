// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../core/libcc/libcc.hh"
#include "build.hh"
#include "compiler.hh"

namespace RG {

static bool TestBinary(const char *name)
{
    Span<const char> env = getenv("PATH");

    while (env.len) {
        Span<const char> path = SplitStr(env, RG_PATH_DELIMITER, &env);

        LocalArray<char, 4096> buf;
        buf.len = Fmt(buf.data, "%1%/%2", path, name).len;

#ifdef _WIN32
        static const Span<const char> extensions[] = {".com", ".exe", ".bat", ".cmd"};

        for (Span<const char> ext: extensions) {
            if (RG_LIKELY(ext.len < buf.Available() - 1)) {
                memcpy(buf.end(), ext.ptr, ext.len + 1);

                if (TestFile(buf.data))
                    return true;
            }
        }
#else
        if (RG_LIKELY(buf.len < RG_SIZE(buf.data) - 1) && TestFile(buf.data))
            return true;
#endif
    }

    return false;
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

bool Compiler::Test() const
{
    if (!test_init) {
        test = TestBinary(binary);
        test_init = true;
    }

    return test;
}

class ClangCompiler: public Compiler {
public:
    ClangCompiler(const char *name) : Compiler(name, "clang") {}

    void MakePchCommand(const char *pch_filename, SourceType src_type, CompileMode compile_mode,
                        bool warnings, Span<const char *const> definitions,
                        Span<const char *const> include_directories, bool env_flags,
                        Allocator *alloc, BuildNode *out_node) const override
    {
        MakeObjectCommand(pch_filename, src_type, compile_mode, warnings, nullptr, definitions,
                          include_directories, env_flags, nullptr, alloc, out_node);
    }

    const char *GetPchObject(const char *, Allocator *) const override { return nullptr; }

    void MakeObjectCommand(const char *src_filename, SourceType src_type, CompileMode compile_mode,
                           bool warnings, const char *pch_filename, Span<const char *const> definitions,
                           Span<const char *const> include_directories, bool env_flags,
                           const char *dest_filename, Allocator *alloc, BuildNode *out_node) const override
    {
        HeapArray<char> buf(alloc);

        // Compiler
        switch (src_type) {
            case SourceType::C: { Fmt(&buf, "clang -std=gnu11"); } break;
            case SourceType::CXX: { Fmt(&buf, "clang++ -std=gnu++17"); } break;
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
        out_node->rsp_offset = buf.len;

        // Build options
        switch (compile_mode) {
            case CompileMode::Debug: { Fmt(&buf, " -O0 -g -ftrapv"); } break;
            case CompileMode::Fast: { Fmt(&buf, " -O2 -g -DNDEBUG"); } break;
            case CompileMode::Release: { Fmt(&buf, " -O2 -flto -DNDEBUG"); } break;
        }
        Fmt(&buf, warnings ? " -Wall" : " -Wno-everything");

        // Platform flags
        Fmt(&buf, " -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64");
#if defined(_WIN32)
        Fmt(&buf, " -DWINVER=0x0601 -D_WIN32_WINNT=0x0601"
                  " -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE"
                  " -D_MT -Xclang --dependent-lib=libcmt -Xclang --dependent-lib=oldnames"
                  " -Wno-unknown-warning-option -Wno-unknown-pragmas -Wno-deprecated-declarations");

        if (src_type == SourceType::CXX) {
            Fmt(&buf, " -Xclang -flto-visibility-public-std");
        }
#elif defined(__APPLE__)
        Fmt(&buf, " -pthread -fPIC");
#else
        Fmt(&buf, " -pthread -fPIC -fstack-protector-strong --param ssp-buffer-size=4");
        if (compile_mode == CompileMode::Fast || compile_mode == CompileMode::Release) {
            Fmt(&buf, " -D_FORTIFY_SOURCE=2");
        }
#endif

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

        out_node->cache_len = buf.len;
        if (LogUsesTerminalOutput()) {
            Fmt(&buf, " -fdiagnostics-color=always");
        }
        out_node->cmd_line = buf.Leak();

        // Dependencies
        out_node->deps_mode = BuildNode::DependencyMode::MakeLike;
        out_node->deps_filename = Fmt(alloc, "%1.d", dest_filename ? dest_filename : src_filename).ptr;
    }

    void MakeLinkCommand(Span<const char *const> obj_filenames, CompileMode compile_mode,
                         Span<const char *const> libraries, LinkType link_type,
                         bool env_flags, const char *dest_filename,
                         Allocator *alloc, BuildNode *out_node) const override
    {
        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
            case LinkType::Executable: { Fmt(&buf, "clang++"); } break;
            case LinkType::SharedLibrary: { Fmt(&buf, "clang++ -shared"); } break;
        }
        Fmt(&buf, " -o \"%1\"", dest_filename);
        out_node->rsp_offset = buf.len;

        // Build mode
        if (compile_mode == CompileMode::Release) {
            Fmt(&buf, " -flto");
            if (link_type == LinkType::Executable) {
                Fmt(&buf, " -static");
            }
        } else {
            Fmt(&buf, " -g");
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
        Fmt(&buf, " -lrt -ldl -pthread -Wl,-z,relro,-z,now");
#endif

        if (env_flags) {
            AddEnvironmentFlags("LDFLAGS", &buf);
        }

        out_node->cache_len = buf.len;
        if (LogUsesTerminalOutput()) {
            Fmt(&buf, " -fdiagnostics-color=always");
        }
        out_node->cmd_line = buf.Leak();
    }
};

class GnuCompiler: public Compiler {
public:
    GnuCompiler(const char *name) : Compiler(name, "gcc") {}

    void MakePchCommand(const char *pch_filename, SourceType src_type, CompileMode compile_mode,
                        bool warnings, Span<const char *const> definitions,
                        Span<const char *const> include_directories, bool env_flags,
                        Allocator *alloc, BuildNode *out_node) const override
    {
        MakeObjectCommand(pch_filename, src_type, compile_mode, warnings, nullptr,
                          definitions, include_directories, env_flags, nullptr, alloc, out_node);
    }

    const char *GetPchObject(const char *, Allocator *) const override { return nullptr; }

    void MakeObjectCommand(const char *src_filename, SourceType src_type, CompileMode compile_mode,
                           bool warnings, const char *pch_filename, Span<const char *const> definitions,
                           Span<const char *const> include_directories, bool env_flags,
                           const char *dest_filename, Allocator *alloc, BuildNode *out_node) const override
    {
        HeapArray<char> buf(alloc);

        // Compiler
        switch (src_type) {
            case SourceType::C: { Fmt(&buf, "gcc -std=gnu11"); } break;
            case SourceType::CXX: { Fmt(&buf, "g++ -std=gnu++17 -fno-exceptions"); } break;
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
        out_node->rsp_offset = buf.len;

        // Build options
        switch (compile_mode) {
            case CompileMode::Debug: { Fmt(&buf, " -O0 -g -ftrapv"); } break;
            case CompileMode::Fast: { Fmt(&buf, " -O2 -g -DNDEBUG"); } break;
            case CompileMode::Release: { Fmt(&buf, " -O2 -flto -DNDEBUG"); } break;
        }
        if (warnings) {
            Fmt(&buf, " -Wall");
            if (src_type == SourceType::CXX) {
                Fmt(&buf, " -Wno-class-memaccess");
            }
        } else {
            Fmt(&buf, " -w");
        }

        // Platform flags
        Fmt(&buf, " -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64");
#if defined(_WIN32)
        Fmt(&buf, " -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -D__USE_MINGW_ANSI_STDIO=1 -DNOMINMAX");
#elif defined(__APPLE__)
        Fmt(&buf, " -pthread -fPIC");
#else
        Fmt(&buf, " -pthread -fPIC -fstack-protector-strong --param ssp-buffer-size=4");
        if (compile_mode == CompileMode::Fast || compile_mode == CompileMode::Release) {
            Fmt(&buf, " -D_FORTIFY_SOURCE=2");
        }
#endif

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

        out_node->cache_len = buf.len;
        if (LogUsesTerminalOutput()) {
            Fmt(&buf, " -fdiagnostics-color=always");
        }
        out_node->cmd_line = buf.Leak();

        // Dependencies
        out_node->deps_mode = BuildNode::DependencyMode::MakeLike;
        out_node->deps_filename = Fmt(alloc, "%1.d", dest_filename ? dest_filename : src_filename).ptr;
    }

    void MakeLinkCommand(Span<const char *const> obj_filenames, CompileMode compile_mode,
                         Span<const char *const> libraries, LinkType link_type,
                         bool env_flags, const char *dest_filename,
                         Allocator *alloc, BuildNode *out_node) const override
    {
        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
            case LinkType::Executable: { Fmt(&buf, "g++"); } break;
            case LinkType::SharedLibrary: { Fmt(&buf, "g++ -shared"); } break;
        }
        Fmt(&buf, " -o \"%1\"", dest_filename);
        out_node->rsp_offset = buf.len;

        // Build mode
        if (compile_mode == CompileMode::Release) {
            Fmt(&buf, " -flto -s");
            if (link_type == LinkType::Executable) {
                Fmt(&buf, " -static");
            }
        } else {
            Fmt(&buf, " -g");
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
        Fmt(&buf, " -lrt -ldl -pthread -Wl,-z,relro,-z,now");
        if (link_type == LinkType::Executable) {
            Fmt(&buf, " -pie");
        }
#endif

        if (env_flags) {
            AddEnvironmentFlags("LDFLAGS", &buf);
        }

        out_node->cache_len = buf.len;
        if (LogUsesTerminalOutput()) {
            Fmt(&buf, " -fdiagnostics-color=always");
        }
        out_node->cmd_line = buf.Leak();
    }
};

#ifdef _WIN32
class MsCompiler: public Compiler {
public:
    MsCompiler(const char *name) : Compiler(name, "cl") {}

    void MakePchCommand(const char *pch_filename, SourceType src_type, CompileMode compile_mode,
                        bool warnings, Span<const char *const> definitions,
                        Span<const char *const> include_directories, bool env_flags,
                        Allocator *alloc, BuildNode *out_node) const override
    {
        MakeObjectCommand(pch_filename, src_type, compile_mode, warnings, nullptr, definitions,
                          include_directories, env_flags, nullptr, alloc, out_node);
    }

    const char *GetPchObject(const char *pch_filename, Allocator *alloc) const override
    {
        const char *obj_filename = Fmt(alloc, "%1.obj", pch_filename).ptr;
        return obj_filename;
    }

    void MakeObjectCommand(const char *src_filename, SourceType src_type, CompileMode compile_mode,
                           bool warnings, const char *pch_filename, Span<const char *const> definitions,
                           Span<const char *const> include_directories, bool env_flags,
                           const char *dest_filename, Allocator *alloc, BuildNode *out_node) const override
    {
        HeapArray<char> buf(alloc);

        // Compiler
        switch (src_type) {
            case SourceType::C: { Fmt(&buf, "cl /nologo"); } break;
            case SourceType::CXX: { Fmt(&buf, "cl /nologo /std:c++17 "); } break;
        }
        if (dest_filename) {
            Fmt(&buf, " \"/Fo%1\"", dest_filename);
        } else {
            Fmt(&buf, " /Yc \"/Fp%1.pch\" \"/Fo%1.obj\"", src_filename);
        }
        Fmt(&buf, " /showIncludes");
        out_node->rsp_offset = buf.len;

        // Build options
        Fmt(&buf, " /MT /EHsc %1", warnings ? "/W3 /wd4200" : "/w");
        switch (compile_mode) {
            case CompileMode::Debug: { Fmt(&buf, " /Od /Z7"); } break;
            case CompileMode::Fast: { Fmt(&buf, " /O2 /DNDEBUG"); } break;
            case CompileMode::Release: { Fmt(&buf, " /O2 /GL /DNDEBUG"); } break;
        }

        // Platform flags
        Fmt(&buf, " /DWINVER=0x0601 /D_WIN32_WINNT=0x0601"
                  " /D_LARGEFILE_SOURCE /D_LARGEFILE64_SOURCE /D_FILE_OFFSET_BITS=64"
                  " /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE");

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

        out_node->cache_len = buf.len;
        out_node->cmd_line = buf.Leak();
        out_node->skip_lines = 1;

        // Dependencies
        out_node->deps_mode = BuildNode::DependencyMode::ShowIncludes;
    }

    void MakeLinkCommand(Span<const char *const> obj_filenames, CompileMode compile_mode,
                         Span<const char *const> libraries, LinkType link_type,
                         bool env_flags, const char *dest_filename,
                         Allocator *alloc, BuildNode *out_node) const override
    {
        HeapArray<char> buf(alloc);

        // Linker
        switch (link_type) {
            case LinkType::Executable: { Fmt(&buf, "link /nologo"); } break;
            case LinkType::SharedLibrary: { Fmt(&buf, "link /nologo /DLL"); } break;
        }
        Fmt(&buf, " \"/OUT:%1\"", dest_filename);
        out_node->rsp_offset = buf.len;

        // Build mode
        switch (compile_mode) {
            case CompileMode::Debug: { Fmt(&buf, " /DEBUG:FULL"); } break;
            case CompileMode::Fast: { Fmt(&buf, " /DEBUG:NONE"); } break;
            case CompileMode::Release: { Fmt(&buf, " /LTCG /DEBUG:NONE"); } break;
        }

        // Objects and libraries
        for (const char *obj_filename: obj_filenames) {
            Fmt(&buf, " \"%1\"", obj_filename);
        }
        for (const char *lib: libraries) {
            Fmt(&buf, " %1.lib", lib);
        }

        if (env_flags) {
            AddEnvironmentFlags("LDFLAGS", &buf);
        }

        out_node->cache_len = buf.len;
        out_node->cmd_line = buf.Leak();
        out_node->skip_success = true;
    }
};
#endif

static ClangCompiler ClangCompiler("Clang");
static GnuCompiler GnuCompiler("GCC");
#ifdef _WIN32
static MsCompiler MsCompiler("MSVC");
#endif

static const Compiler *const CompilerTable[] = {
#ifdef _WIN32
    &MsCompiler,
#endif
    &ClangCompiler,
    &GnuCompiler
};
const Span<const Compiler *const> Compilers = CompilerTable;

void MakePackCommand(Span<const char *const> pack_filenames, CompileMode compile_mode,
                     const char *pack_options, const char *dest_filename,
                     Allocator *alloc, BuildNode *out_node)
{
    HeapArray<char> buf(alloc);

    Fmt(&buf, "\"%1\" pack -O \"%2\"", GetApplicationExecutable(), dest_filename);

    switch (compile_mode) {
        case CompileMode::Debug: { Fmt(&buf, " -m SourceMap"); } break;
        case CompileMode::Fast:
        case CompileMode::Release: { Fmt(&buf, " -m RunTransform"); } break;
    }

    if (pack_options) {
        Fmt(&buf, " %1", pack_options);
    }
    for (const char *pack_filename: pack_filenames) {
        Fmt(&buf, " \"%1\"", pack_filename);
    }

    out_node->cache_len = buf.len;
    out_node->cmd_line = buf.Leak();
}

}
