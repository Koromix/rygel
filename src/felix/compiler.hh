// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"

namespace RG {

enum class SourceType {
    C_Source,
    C_Header,
    CXX_Source,
    CXX_Header
};

enum class CompileMode {
    Debug,
    Fast,
    Release
};
static const char *const CompileModeNames[] = {
    "Debug",
    "Fast",
    "Release"
};

enum class LinkType {
    Executable,
    SharedLibrary
};

struct BuildCommand {
    Span<const char> line;
    Size rsp_offset;

    int skip_lines;
    bool parse_cl_includes;
};

void MakePackCommand(Span<const char *const> pack_filenames, CompileMode compile_mode,
                     const char *pack_options, const char *dest_filename,
                     Allocator *alloc, BuildCommand *out_cmd);

class Compiler {
    mutable bool test_init = false;
    mutable bool test;

public:
    const char *name;
    const char *prefix;
    const char *binary;
    bool pch;

    Compiler(const char *name, const char *prefix, const char *binary, bool pch)
        : name(name), prefix(prefix), binary(binary), pch(pch) {}

    bool Test() const;

    virtual void MakeObjectCommand(const char *src_filename, SourceType src_type, CompileMode compile_mode,
                                   bool warnings, const char *pch_filename, Span<const char *const> definitions,
                                   Span<const char *const> include_directories, const char *dest_filename,
                                   const char *deps_filename, Allocator *alloc, BuildCommand *out_cmd) const = 0;
    virtual void MakeLinkCommand(Span<const char *const> obj_filenames, CompileMode compile_mode,
                                 Span<const char *const> libraries, LinkType link_type,
                                 const char *dest_filename, Allocator *alloc, BuildCommand *out_cmd) const = 0;
};

extern const Span<const Compiler *const> Compilers;

}
