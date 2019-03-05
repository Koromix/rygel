// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "compiler.hh"

Compiler ClangCompiler = {
    "Clang",

    // BuildObjectCommand
    [](const char *src_filename, SourceType src_type, BuildMode build_mode,
       const char *pch_filename, Span<const char *const> include_directories,
       const char *dest_filename, const char *deps_filename, Allocator *alloc) {
#ifdef _WIN32
        static const char *const flags = "-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE "
                                         "-Wall -Wno-unknown-warning-option";
#else
        static const char *const flags = "-Wall";
#endif

        HeapArray<char> buf;
        buf.allocator = alloc;

        switch (src_type) {
            case SourceType::C_Source: { Fmt(&buf, "clang -std=gnu99 %1", flags); } break;
            case SourceType::C_Header: { Fmt(&buf, "clang -std=gnu99 -x c-header %1", flags); } break;
            case SourceType::CXX_Source: { Fmt(&buf, "clang++ -std=gnu++17 -Xclang -flto-visibility-public-std "
                                                     "%1", flags); } break;
            case SourceType::CXX_Header: { Fmt(&buf, "clang++ -std=gnu++17 -Xclang -flto-visibility-public-std "
                                                     "-x c++-header %1", flags); } break;
        }

        switch (build_mode) {
            case BuildMode::Debug: { Fmt(&buf, " -O0 -g"); } break;
            case BuildMode::FastDebug: { Fmt(&buf, " -O2 -g"); } break;
            case BuildMode::Release: { Fmt(&buf, " -O2 -flto"); } break;
        }

        Fmt(&buf, " -c %1", src_filename);
        if (pch_filename) {
            Fmt(&buf, " -include %1", pch_filename);
        }
        for (const char *include_directory: include_directories) {
            Fmt(&buf, " -I%1", include_directory);
        }
        if (deps_filename) {
            Fmt(&buf, " -MMD -MF %1", deps_filename);
        }
        if (dest_filename) {
            Fmt(&buf, " -o %1", dest_filename);
        }

        return (const char *)buf.Leak().ptr;
    },

    // BuildLinkCommand
    [](Span<const ObjectInfo> objects, Span<const char *const> libraries,
       const char *dest_filename, Allocator *alloc) {
        HeapArray<char> buf;
        buf.allocator = alloc;

        bool is_cxx = std::any_of(objects.begin(), objects.end(),
                                  [](const ObjectInfo &obj) { return obj.src_type == SourceType::CXX_Source; });
        buf.Append(is_cxx ? "clang++" : "clang");

        for (const ObjectInfo &obj: objects) {
            switch (obj.src_type) {
                case SourceType::C_Source:
                case SourceType::CXX_Source: { Fmt(&buf, " %1", obj.dest_filename); } break;

                case SourceType::C_Header:
                case SourceType::CXX_Header: { DebugAssert(false); } break;
            }
        }
        for (const char *lib: libraries) {
            Fmt(&buf, " -l%1", lib);
        }

        Fmt(&buf, " -o %1", dest_filename);

        return (const char *)buf.Leak().ptr;
    }
};

Compiler GnuCompiler = {
    "GNU",

    // BuildObjectCommand
    [](const char *src_filename, SourceType src_type, BuildMode build_mode,
       const char *pch_filename, Span<const char *const> include_directories,
       const char *dest_filename, const char *deps_filename, Allocator *alloc) {
#ifdef _WIN32
        static const char *const flags = "-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE "
                                         "-Wno-unknown-warning-option";
#else
        static const char *const flags = "-Wall";
#endif

        HeapArray<char> buf;
        buf.allocator = alloc;

        switch (src_type) {
            case SourceType::C_Source: { Fmt(&buf, "gcc -std=gnu99 %1", flags); } break;
            case SourceType::C_Header: { Fmt(&buf, "gcc -std=gnu99 -x c-header %1", flags); } break;
            case SourceType::CXX_Source: { Fmt(&buf, "g++ -std=gnu++17 %1", flags); } break;
            case SourceType::CXX_Header: { Fmt(&buf, "g++ -std=gnu++17 -x c++-header %1", flags); } break;
        }

        switch (build_mode) {
            case BuildMode::Debug: { Fmt(&buf, " -O0 -g"); } break;
            case BuildMode::FastDebug: { Fmt(&buf, " -O2 -g"); } break;
            case BuildMode::Release: {
#ifdef _WIN32
                LogError("LTO does not work correctly with MinGW (ignoring)");
                Fmt(&buf, " -O2");
#else
                Fmt(&buf, " -O2 -flto");
#endif
            } break;
        }

        Fmt(&buf, " -c %1", src_filename);
        if (pch_filename) {
            Fmt(&buf, " -include %1", pch_filename);
        }
        for (const char *include_directory: include_directories) {
            Fmt(&buf, " -I%1", include_directory);
        }
        if (deps_filename) {
            Fmt(&buf, " -MMD -MF %1", deps_filename);
        }
        if (dest_filename) {
            Fmt(&buf, " -o %1", dest_filename);
        }

        return (const char *)buf.Leak().ptr;
    },

    // BuildLinkCommand
    [](Span<const ObjectInfo> objects, Span<const char *const> libraries,
       const char *dest_filename, Allocator *alloc) {
        HeapArray<char> buf;
        buf.allocator = alloc;

        bool is_cxx = std::any_of(objects.begin(), objects.end(),
                                  [](const ObjectInfo &obj) { return obj.src_type == SourceType::CXX_Source; });
        buf.Append(is_cxx ? "g++" : "gcc");

        for (const ObjectInfo &obj: objects) {
            switch (obj.src_type) {
                case SourceType::C_Source:
                case SourceType::CXX_Source: { Fmt(&buf, " %1", obj.dest_filename); } break;

                case SourceType::C_Header:
                case SourceType::CXX_Header: { DebugAssert(false); } break;
            }
        }
        for (const char *lib: libraries) {
            Fmt(&buf, " -l%1", lib);
        }

        Fmt(&buf, " -o %1", dest_filename);

        return (const char *)buf.Leak().ptr;
    }
};
