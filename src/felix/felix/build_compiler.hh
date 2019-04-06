// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"

namespace RG {

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

enum class LinkType {
    Executable,
    SharedLibrary
};

enum class CompilerFlag {
    PCH = 1 << 0,
    LTO = 1 << 1
};

class Compiler {
public:
    const char *name;
    unsigned int flags;

    Compiler(const char *name, unsigned int flags)
        : name(name), flags(flags) {}

    bool Supports(CompilerFlag flag) const { return flags & (int)flag; }

    virtual const char *MakeObjectCommand(const char *src_filename, SourceType src_type, BuildMode build_mode,
                                          const char *pch_filename, Span<const char *const> definitions,
                                          Span<const char *const> include_directories, const char *dest_filename,
                                          const char *deps_filename, Allocator *alloc) const = 0;
    virtual const char *MakePackCommand(Span<const char *const> pack_filenames, const char *pack_options,
                                        const char *dest_filename, Allocator *alloc) const = 0;
    virtual const char *MakeLinkCommand(Span<const char *const> obj_filenames, BuildMode build_mode,
                                        Span<const char *const> libraries, LinkType link_type,
                                        const char *dest_filename, Allocator *alloc) const = 0;
};

extern const Span<const Compiler *const> Compilers;

}
