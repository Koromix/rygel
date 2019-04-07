// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#endif

#include "../../libcc/libcc.hh"
#include "build_compiler.hh"

namespace RG {

static void AppendGccObjectArguments(const char *src_filename, BuildMode build_mode,
                                     const char *pch_filename, Span<const char *const> definitions,
                                     Span<const char *const> include_directories,
                                     const char *dest_filename, const char *deps_filename,
                                     HeapArray<char> *out_buf)
{
    Fmt(out_buf, " -fdiagnostics-color=always");

    switch (build_mode) {
        case BuildMode::Debug: { Fmt(out_buf, " -O0 -g"); } break;
        case BuildMode::Fast: { Fmt(out_buf, " -O2 -g -DNDEBUG"); } break;
        case BuildMode::LTO: { Fmt(out_buf, " -O2 -flto -g -DNDEBUG"); } break;
    }

    Fmt(out_buf, " -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -fvisibility=hidden");
#ifdef _WIN32
    Fmt(out_buf, " -DWINVER=0x0601 -D_WIN32_WINNT=0x0601");
#endif

    Fmt(out_buf, " -c %1", src_filename);
    if (pch_filename) {
        Fmt(out_buf, " -include %1", pch_filename);
    }
    for (const char *definition: definitions) {
        Fmt(out_buf, " -D%1", definition);
    }
    for (const char *include_directory: include_directories) {
        Fmt(out_buf, " -I%1", include_directory);
    }
    if (deps_filename) {
        Fmt(out_buf, " -MMD -MF %1", deps_filename);
    }
    if (dest_filename) {
        Fmt(out_buf, " -o %1", dest_filename);
    }
}

static bool AppendGccLinkArguments(Span<const char *const> obj_filenames, BuildMode build_mode,
                                   LinkType link_type, Span<const char *const> libraries,
                                   const char *dest_filename, HeapArray<char> *out_buf)
{
    Fmt(out_buf, " -fdiagnostics-color=always");
    if (build_mode == BuildMode::LTO) {
        Fmt(out_buf, " -flto");
    }

#ifdef _WIN32
    Size rsp_offset = out_buf->len;
#endif
    for (const char *obj_filename: obj_filenames) {
        Fmt(out_buf, " %1", obj_filename);
    }
#ifdef _WIN32
    if (out_buf->len - rsp_offset >= 4096) {
        char rsp_filename[4096];
        {
            // TODO: Maybe we should try to delete these temporary files when felix exits?
            char temp_directory[4096];
            if (!GetTempPath(RG_SIZE(temp_directory), temp_directory) ||
                    !GetTempFileName(temp_directory, "fxb", 0, rsp_filename)) {
                LogError("Failed to create temporary path");
                return false;
            }
        }

        // Apparently backslash characters needs to be escaped in response files,
        // but it's easier to use '/' instead.
        Span<char> arguments = out_buf->Take(rsp_offset + 1, out_buf->len - rsp_offset - 1);
        for (Size i = 0; i < arguments.len; i++) {
            arguments[i] = (arguments[i] == '\\' ? '/' : arguments[i]);
        }
        if (!WriteFile(arguments, rsp_filename))
            return false;

        out_buf->RemoveFrom(rsp_offset);
        Fmt(out_buf, " \"@%1\"", rsp_filename);
    }
#endif

    switch (link_type) {
        case LinkType::Executable: { /* Skip */ } break;
        case LinkType::SharedLibrary: { Fmt(out_buf, " -shared"); } break;
    }

#ifndef _WIN32
    Fmt(out_buf, " -lrt -ldl -pthread");
#endif
    for (const char *lib: libraries) {
        Fmt(out_buf, " -l%1", lib);
    }
    Fmt(out_buf, " -o %1", dest_filename);

    return true;
}

static void AppendPackCommandLine(Span<const char *const> pack_filenames, const char *pack_options,
                                  HeapArray<char> *out_buf)
{
#ifdef _WIN32
    Fmt(out_buf, "cmd /c \"%1\" pack", GetApplicationExecutable());
#else
    Fmt(out_buf, "\"%1\" pack", GetApplicationExecutable());
#endif

    if (pack_options) {
        Fmt(out_buf, " %1", pack_options);
    }
    for (const char *pack_filename: pack_filenames) {
        Fmt(out_buf, " %1", pack_filename);
    }
}

class ClangCompiler: public Compiler {
public:
    ClangCompiler(): Compiler("Clang", (int)CompilerFlag::PCH | (int)CompilerFlag::LTO) {}

    const char *MakeObjectCommand(const char *src_filename, SourceType src_type, BuildMode build_mode,
                                  const char *pch_filename, Span<const char *const> definitions,
                                  Span<const char *const> include_directories, const char *dest_filename,
                                  const char *deps_filename, Allocator *alloc) const override
    {
#ifdef _WIN32
        static const char *const flags = "-Wall -Wno-unknown-warning-option -Wno-unknown-pragmas "
                                         "-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE";
#else
        static const char *const flags = "-pthread -Wall";
#endif

        HeapArray<char> buf;
        buf.allocator = alloc;

        switch (src_type) {
            case SourceType::C_Source: { Fmt(&buf, "clang -std=gnu11 %1", flags); } break;
            case SourceType::C_Header: { Fmt(&buf, "clang -std=gnu11 -x c-header %1", flags); } break;
            case SourceType::CXX_Source: { Fmt(&buf, "clang++ -std=gnu++17 -fno-exceptions %1", flags); } break;
            case SourceType::CXX_Header: { Fmt(&buf, "clang++ -std=gnu++17 -fno-exceptions -x c++-header %1", flags); } break;
        }
#ifdef _WIN32
        Fmt(&buf, " -D_MT -Xclang --dependent-lib=libcmt -Xclang --dependent-lib=oldnames");
        if (src_type == SourceType::CXX_Source || src_type == SourceType::CXX_Header) {
            Fmt(&buf, " -Xclang -flto-visibility-public-std");
        }
#endif

        AppendGccObjectArguments(src_filename, build_mode, pch_filename, definitions,
                                 include_directories, dest_filename, deps_filename, &buf);

        return (const char *)buf.Leak().ptr;
    }

    const char *MakePackCommand(Span<const char *const> pack_filenames, const char *pack_options,
                                const char *dest_filename, Allocator *alloc) const override
    {
        HeapArray<char> buf;
        buf.allocator = alloc;

        AppendPackCommandLine(pack_filenames, pack_options, &buf);
        Fmt(&buf, " | clang -x c -c - -o %1", dest_filename);

        return (const char *)buf.Leak().ptr;
    }

    const char *MakeLinkCommand(Span<const char *const> obj_filenames, BuildMode build_mode,
                                Span<const char *const> libraries, LinkType link_type,
                                const char *dest_filename, Allocator *alloc) const override
    {
        HeapArray<char> buf;
        buf.allocator = alloc;

#ifdef _WIN32
        Fmt(&buf, "clang++ -g -fuse-ld=lld");
#else
        Fmt(&buf, "clang++ -g");
#endif
        if (!AppendGccLinkArguments(obj_filenames, build_mode, link_type, libraries,
                                    dest_filename, &buf))
            return (const char *)nullptr;

        return (const char *)buf.Leak().ptr;
    }
};

class GnuCompiler: public Compiler {
public:
    GnuCompiler()
        : Compiler("GNU", (int)CompilerFlag::PCH | (int)CompilerFlag::LTO)
    {
#ifdef _WIN32
        flags &= ~(int)CompilerFlag::PCH;
#endif
    }

    const char *MakeObjectCommand(const char *src_filename, SourceType src_type, BuildMode build_mode,
                                  const char *pch_filename, Span<const char *const> definitions,
                                  Span<const char *const> include_directories, const char *dest_filename,
                                  const char *deps_filename, Allocator *alloc) const override
    {
#ifdef _WIN32
        static const char *const flags = "-Wall -D__USE_MINGW_ANSI_STDIO=1";
#else
        static const char *const flags = "-pthread -Wall";
#endif

        HeapArray<char> buf;
        buf.allocator = alloc;

        switch (src_type) {
            case SourceType::C_Source: { Fmt(&buf, "gcc -std=gnu11 %1", flags); } break;
            case SourceType::C_Header: { Fmt(&buf, "gcc -std=gnu11 -x c-header %1", flags); } break;
            case SourceType::CXX_Source: { Fmt(&buf, "g++ -std=gnu++17 -fno-exceptions "
                                                     "%1", flags); } break;
            case SourceType::CXX_Header: { Fmt(&buf, "g++ -std=gnu++17 -fno-exceptions "
                                                     "-x c++-header %1", flags); } break;
        }

        AppendGccObjectArguments(src_filename, build_mode, pch_filename, definitions,
                                 include_directories, dest_filename, deps_filename, &buf);

        return (const char *)buf.Leak().ptr;
    }

    const char *MakePackCommand(Span<const char *const> pack_filenames, const char *pack_options,
                                const char *dest_filename, Allocator *alloc) const override
    {
        HeapArray<char> buf;
        buf.allocator = alloc;

        AppendPackCommandLine(pack_filenames, pack_options, &buf);
        Fmt(&buf, " | gcc -x c -c - -o %1", dest_filename);

        return (const char *)buf.Leak().ptr;
    }

    const char *MakeLinkCommand(Span<const char *const> obj_filenames, BuildMode build_mode,
                                Span<const char *const> libraries, LinkType link_type,
                                const char *dest_filename, Allocator *alloc) const override
    {
        HeapArray<char> buf;
        buf.allocator = alloc;

        Fmt(&buf, "g++ -g");
#ifdef _WIN32
        if (build_mode != BuildMode::Debug) {
            // Force static linking of libgcc, libstdc++ and winpthread
            Fmt(&buf, " -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic");
        }
#endif

        if (!AppendGccLinkArguments(obj_filenames, build_mode, link_type, libraries,
                                    dest_filename, &buf))
            return (const char *)nullptr;

        return (const char *)buf.Leak().ptr;
    }
};

static ClangCompiler ClangCompiler;
static GnuCompiler GnuCompiler;
static const Compiler *const CompilerTable[] = {
    &ClangCompiler,
    &GnuCompiler
};
const Span<const Compiler *const> Compilers = CompilerTable;

}
