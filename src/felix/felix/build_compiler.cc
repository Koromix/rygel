// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "build_compiler.hh"

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
            case SourceType::C_Source: { Fmt(&buf, "clang -std=gnu99 %1", flags); } break;
            case SourceType::C_Header: { Fmt(&buf, "clang -std=gnu99 -x c-header %1", flags); } break;
            case SourceType::CXX_Source: { Fmt(&buf, "clang++ -std=gnu++17 -Xclang -flto-visibility-public-std "
                                                     "-fno-exceptions %1", flags); } break;
            case SourceType::CXX_Header: { Fmt(&buf, "clang++ -std=gnu++17 -Xclang -flto-visibility-public-std "
                                                     "-fno-exceptions -x c++-header %1", flags); } break;
        }
#ifndef _WIN32
        // Breaks some <functional> stuff on Windows
        Fmt(&buf, " -fno-rtti");
#endif

        switch (build_mode) {
            case BuildMode::Debug: { Fmt(&buf, " -O0 -g"); } break;
            case BuildMode::Fast: { Fmt(&buf, " -O2 -g -DNDEBUG"); } break;
            case BuildMode::LTO: { Fmt(&buf, " -O2 -flto -g -DNDEBUG"); } break;
        }

        Fmt(&buf, " -c %1", src_filename);
        if (pch_filename) {
            Fmt(&buf, " -include %1", pch_filename);
        }
        for (const char *definition: definitions) {
            Fmt(&buf, " -D%1", definition);
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
    [](Span<const ObjectInfo> objects, BuildMode build_mode, Span<const char *const> libraries,
       const char *dest_filename, Allocator *alloc) {
#ifdef _WIN32
        static const char *const flags = "";
#else
        static const char *const flags = "-lrt -ldl -pthread";
#endif

        HeapArray<char> buf;
        buf.allocator = alloc;

        bool is_cxx = std::any_of(objects.begin(), objects.end(),
                                  [](const ObjectInfo &obj) { return obj.src_type == SourceType::CXX_Source; });
        Fmt(&buf, "%1 %2", is_cxx ? "clang++" : "clang", flags);

        for (const ObjectInfo &obj: objects) {
            switch (obj.src_type) {
                case SourceType::C_Source:
                case SourceType::CXX_Source: { Fmt(&buf, " %1", obj.dest_filename); } break;

                case SourceType::C_Header:
                case SourceType::CXX_Header: { DebugAssert(false); } break;
            }
        }

        if (build_mode == BuildMode::LTO) {
            Fmt(&buf, " -flto");
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
            case SourceType::C_Source: { Fmt(&buf, "gcc -std=gnu99 %1", flags); } break;
            case SourceType::C_Header: { Fmt(&buf, "gcc -std=gnu99 -x c-header %1", flags); } break;
            case SourceType::CXX_Source: { Fmt(&buf, "g++ -std=gnu++17 -fno-rtti -fno-exceptions "
                                                     "%1", flags); } break;
            case SourceType::CXX_Header: { Fmt(&buf, "g++ -std=gnu++17 -fno-rtti -fno-exceptions "
                                                     "-x c++-header %1", flags); } break;
        }

        switch (build_mode) {
            case BuildMode::Debug: { Fmt(&buf, " -O0 -g"); } break;
            case BuildMode::Fast: { Fmt(&buf, " -O2 -g -DNDEBUG"); } break;
            case BuildMode::LTO: { Fmt(&buf, " -O2 -flto -g -DNDEBUG"); } break;
        }

        Fmt(&buf, " -c %1", src_filename);
        if (pch_filename) {
            Fmt(&buf, " -include %1", pch_filename);
        }
        for (const char *definition: definitions) {
            Fmt(&buf, " -D%1", definition);
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
    [](Span<const ObjectInfo> objects, BuildMode build_mode, Span<const char *const> libraries,
       const char *dest_filename, Allocator *alloc) {
#ifdef _WIN32
        static const char *const flags = "";
#else
        static const char *const flags = "-lrt -ldl -pthread";
#endif

        HeapArray<char> buf;
        buf.allocator = alloc;

        bool is_cxx = std::any_of(objects.begin(), objects.end(),
                                  [](const ObjectInfo &obj) { return obj.src_type == SourceType::CXX_Source; });
        Fmt(&buf, "%1 %2", is_cxx ? "g++" : "gcc", flags);

        for (const ObjectInfo &obj: objects) {
            switch (obj.src_type) {
                case SourceType::C_Source:
                case SourceType::CXX_Source: { Fmt(&buf, " %1", obj.dest_filename); } break;

                case SourceType::C_Header:
                case SourceType::CXX_Header: { DebugAssert(false); } break;
            }
        }

        if (build_mode == BuildMode::LTO) {
            Fmt(&buf, " -flto");
        }

        for (const char *lib: libraries) {
            Fmt(&buf, " -l%1", lib);
        }

        Fmt(&buf, " -o %1", dest_filename);

        return (const char *)buf.Leak().ptr;
    }
};
