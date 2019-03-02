// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "compiler.hh"

Compiler ClangCompiler = {
    "Clang",

    /* BuildObjectCommand */
    [](SourceType source_type, const char *src_filename, const char *dest_filename,
       const char *deps_filename, Allocator *alloc) {
#ifdef _WIN32
        static const char *const flags = "-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE "
                                         "-Wno-unknown-warning-option";
#else
        static const char *const flags = "";
#endif

        HeapArray<char> buf;
        buf.allocator = alloc;

        switch (source_type) {
            case SourceType::C_Source: { Fmt(&buf, "clang -std=gnu99 -include pch/stdafx_c.h %1", flags); } break;
            case SourceType::C_Header: { Fmt(&buf, "clang -std=gnu99 -x c-header %1", flags); } break;
            case SourceType::CXX_Source: { Fmt(&buf, "clang++ -std=gnu++17 -Xclang -flto-visibility-public-std "
                                                     "-include pch/stdafx_cxx.h %1", flags); } break;
            case SourceType::CXX_Header: { Fmt(&buf, "clang++ -std=gnu++17 -Xclang -flto-visibility-public-std "
                                                     "-x c++-header %1", flags); } break;
        }

        Fmt(&buf, " -c %1", src_filename);
        if (deps_filename) {
            Fmt(&buf, " -MMD -MF %1", deps_filename);
        }
        if (dest_filename) {
            Fmt(&buf, " -o %1", dest_filename);
        }

        return (const char *)buf.Leak().ptr;
    }
};

Compiler GnuCompiler = {
    "GNU",

    /* BuildObjectCommand */
    [](SourceType source_type, const char *src_filename, const char *dest_filename,
       const char *deps_filename, Allocator *alloc) {
#ifdef _WIN32
        static const char *const flags = "-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE "
                                         "-Wno-unknown-warning-option";
#else
        static const char *const flags = "";
#endif

        HeapArray<char> buf;
        buf.allocator = alloc;

        switch (source_type) {
            case SourceType::C_Source: { Fmt(&buf, "clang -std=gnu99 -include pch/stdafx_c.h %1", flags); } break;
            case SourceType::C_Header: { Fmt(&buf, "clang -std=gnu99 -x c-header %1", flags); } break;
            case SourceType::CXX_Source: { Fmt(&buf, "clang++ -std=gnu++17 -Xclang -flto-visibility-public-std "
                                                     "-include pch/stdafx_cxx.h %1", flags); } break;
            case SourceType::CXX_Header: { Fmt(&buf, "clang++ -std=gnu++17 -Xclang -flto-visibility-public-std "
                                                     "-x c++-header %1", flags); } break;
        }

        Fmt(&buf, " -c %1", src_filename);
        if (deps_filename) {
            Fmt(&buf, " -MMD -MF %1", deps_filename);
        }
        if (dest_filename) {
            Fmt(&buf, " -o %1", dest_filename);
        }

        return (const char *)buf.Leak().ptr;
    }
};
