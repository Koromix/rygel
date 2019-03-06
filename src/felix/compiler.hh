// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"

enum class SourceType {
    C_Source,
    C_Header,
    CXX_Source,
    CXX_Header
};

struct ObjectInfo {
    const char *src_filename;
    SourceType src_type;

    const char *dest_filename;
};

enum class BuildMode {
    Debug,
    Fast,
    LTO
};
static const char *const BuildModeNames[] = {
    "Debug",
    "Fast",
    "LTO"
};

struct Compiler {
    const char *name;

    const char *(*BuildObjectCommand)(const char *src_filename, SourceType src_type,
                                      BuildMode build_mode, const char *pch_filename,
                                      Span<const char *const> include_directories,
                                      const char *dest_filename, const char *deps_filename,
                                      Allocator *alloc);
    const char *(*BuildLinkCommand)(Span<const ObjectInfo> objects, BuildMode build_mode,
                                    Span<const char *const> libraries,
                                    const char *dest_filename, Allocator *alloc);
};

extern Compiler ClangCompiler;
extern Compiler GnuCompiler;

static const Compiler *const Compilers[] = {
    &ClangCompiler,
    &GnuCompiler
};
