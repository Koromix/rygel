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

static void AppendGccObjectArguments(const char *src_filename, BuildMode build_mode,
                                     const char *pch_filename, Span<const char *const> definitions,
                                     Span<const char *const> include_directories,
                                     const char *dest_filename, const char *deps_filename,
                                     HeapArray<char> *out_buf)
{
    switch (build_mode) {
        case BuildMode::Debug: { Fmt(out_buf, " -O0 -g"); } break;
        case BuildMode::Fast: { Fmt(out_buf, " -O2 -g -DNDEBUG"); } break;
        case BuildMode::LTO: { Fmt(out_buf, " -O2 -flto -g -DNDEBUG"); } break;
    }

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

static bool AppendGccLinkArguments(Span<const ObjectInfo> objects, BuildMode build_mode,
                                   Span<const char *const> libraries, const char *dest_filename,
                                   HeapArray<char> *out_buf)
{
    if (build_mode == BuildMode::LTO) {
        Fmt(out_buf, " -flto");
    }

#ifdef _WIN32
    Size rsp_offset = out_buf->len;
#endif
    for (const ObjectInfo &obj: objects) {
        switch (obj.src_type) {
            case SourceType::C_Source:
            case SourceType::CXX_Source: { Fmt(out_buf, " %1", obj.dest_filename); } break;

            case SourceType::C_Header:
            case SourceType::CXX_Header: { DebugAssert(false); } break;
        }
    }
#ifdef _WIN32
    if (out_buf->len - rsp_offset >= 4096) {
        char rsp_filename[4096];
        {
            // TODO: Maybe we should try to delete these temporary files when felix exits?
            char temp_directory[4096];
            if (!GetTempPath(SIZE(temp_directory), temp_directory) ||
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

#ifndef _WIN32
    Fmt(out_buf, " -lrt -ldl -pthread");
#endif
    for (const char *lib: libraries) {
        Fmt(out_buf, " -l%1", lib);
    }
    Fmt(out_buf, " -o %1", dest_filename);

    return true;
}

Compiler ClangCompiler = {
    "Clang",
#ifdef _WIN32
    (int)CompilerFlag::PCH,
#else
    (int)CompilerFlag::PCH | (int)CompilerFlag::LTO,
#endif

    // BuildObjectCommand
    [](const char *src_filename, SourceType src_type, BuildMode build_mode, const char *pch_filename,
       Span<const char *const> definitions, Span<const char *const> include_directories,
       const char *dest_filename, const char *deps_filename, Allocator *alloc) {
#ifdef _WIN32
        static const char *const flags = "-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE "
                                         "-Wall -Wno-unknown-warning-option";
#else
        static const char *const flags = "-pthread -Wall";
#endif

        HeapArray<char> buf;
        buf.allocator = alloc;

        switch (src_type) {
            case SourceType::C_Source: { Fmt(&buf, "clang -std=gnu11 %1", flags); } break;
            case SourceType::C_Header: { Fmt(&buf, "clang -std=gnu11 -x c-header %1", flags); } break;
            case SourceType::CXX_Source: { Fmt(&buf, "clang++ -std=gnu++17 -Xclang -flto-visibility-public-std "
                                                     "-fno-exceptions %1", flags); } break;
            case SourceType::CXX_Header: { Fmt(&buf, "clang++ -std=gnu++17 -Xclang -flto-visibility-public-std "
                                                     "-fno-exceptions -x c++-header %1", flags); } break;
        }
#ifndef _WIN32
        // -fno-rtti breaks <functional> on Windows
        Fmt(&buf, " -fno-rtti");
#endif

        AppendGccObjectArguments(src_filename, build_mode, pch_filename, definitions,
                                 include_directories, dest_filename, deps_filename, &buf);

        return (const char *)buf.Leak().ptr;
    },

    // BuildLinkCommand
    [](Span<const ObjectInfo> objects, BuildMode build_mode, Span<const char *const> libraries,
       const char *dest_filename, Allocator *alloc) {
        HeapArray<char> buf;
        buf.allocator = alloc;

        bool is_cxx = std::any_of(objects.begin(), objects.end(),
                                  [](const ObjectInfo &obj) { return obj.src_type == SourceType::CXX_Source; });
        Fmt(&buf, "%1", is_cxx ? "clang++" : "clang");

        if (!AppendGccLinkArguments(objects, build_mode, libraries, dest_filename, &buf))
            return (const char *)nullptr;

        return (const char *)buf.Leak().ptr;
    }
};

Compiler GnuCompiler = {
    "GNU",
#ifdef _WIN32
    (int)CompilerFlag::LTO,
#else
    (int)CompilerFlag::PCH | (int)CompilerFlag::LTO,
#endif

    // BuildObjectCommand
    [](const char *src_filename, SourceType src_type, BuildMode build_mode, const char *pch_filename,
       Span<const char *const> definitions, Span<const char *const> include_directories,
       const char *dest_filename, const char *deps_filename, Allocator *alloc) {
#ifdef _WIN32
        static const char *const flags = "-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE "
                                         "-Wno-unknown-warning-option";
#else
        static const char *const flags = "-pthread -Wall";
#endif

        HeapArray<char> buf;
        buf.allocator = alloc;

        switch (src_type) {
            case SourceType::C_Source: { Fmt(&buf, "gcc -std=gnu11 %1", flags); } break;
            case SourceType::C_Header: { Fmt(&buf, "gcc -std=gnu11 -x c-header %1", flags); } break;
            case SourceType::CXX_Source: { Fmt(&buf, "g++ -std=gnu++17 -fno-rtti -fno-exceptions "
                                                     "%1", flags); } break;
            case SourceType::CXX_Header: { Fmt(&buf, "g++ -std=gnu++17 -fno-rtti -fno-exceptions "
                                                     "-x c++-header %1", flags); } break;
        }

        AppendGccObjectArguments(src_filename, build_mode, pch_filename, definitions,
                                 include_directories, dest_filename, deps_filename, &buf);

        return (const char *)buf.Leak().ptr;
    },

    // BuildLinkCommand
    [](Span<const ObjectInfo> objects, BuildMode build_mode, Span<const char *const> libraries,
       const char *dest_filename, Allocator *alloc) {
        HeapArray<char> buf;
        buf.allocator = alloc;

        bool is_cxx = std::any_of(objects.begin(), objects.end(),
                                  [](const ObjectInfo &obj) { return obj.src_type == SourceType::CXX_Source; });
        Fmt(&buf, "%1", is_cxx ? "g++" : "gcc");

        if (!AppendGccLinkArguments(objects, build_mode, libraries, dest_filename, &buf))
            return (const char *)nullptr;

        return (const char *)buf.Leak().ptr;
    }
};
