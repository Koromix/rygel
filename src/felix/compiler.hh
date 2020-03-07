// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"

namespace RG {

struct BuildNode;

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

enum class SourceType {
    C,
    CXX
};

enum class LinkType {
    Executable,
    SharedLibrary
};

class Compiler {
    mutable bool test_init = false;
    mutable bool test;

public:
    const char *name;
    const char *binary;

    Compiler(const char *name, const char *binary)
        : name(name), binary(binary) {}

    bool Test() const;

    virtual void MakePchCommand(const char *pch_filename, SourceType src_type, CompileMode compile_mode,
                                bool warnings, Span<const char *const> definitions,
                                Span<const char *const> include_directories,
                                Allocator *alloc, BuildNode *out_node) const = 0;
    virtual const char *GetPchObject(const char *pch_filename, Allocator *alloc) const = 0;

    virtual void MakeObjectCommand(const char *src_filename, SourceType src_type, CompileMode compile_mode,
                                   bool warnings, const char *pch_filename, Span<const char *const> definitions,
                                   Span<const char *const> include_directories, const char *dest_filename,
                                   Allocator *alloc, BuildNode *out_node) const = 0;

    virtual void MakeLinkCommand(Span<const char *const> obj_filenames, CompileMode compile_mode,
                                 Span<const char *const> libraries, LinkType link_type,
                                 const char *dest_filename, Allocator *alloc, BuildNode *out_node) const = 0;
};

extern const Span<const Compiler *const> Compilers;

void MakePackCommand(Span<const char *const> pack_filenames, CompileMode compile_mode,
                     const char *pack_options, const char *dest_filename,
                     Allocator *alloc, BuildNode *out_node);

}
