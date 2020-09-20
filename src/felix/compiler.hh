// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"

namespace RG {

enum class CompileMode {
    Debug,
    DebugFast,
    Fast,
    LTO
};
static const char *const CompileModeNames[] = {
    "Debug",
    "DebugFast",
    "Fast",
    "LTO"
};

enum class SourceType {
    C,
    CXX
};

enum class LinkType {
    Executable,
    SharedLibrary
};

struct Command {
    enum class DependencyMode {
        None,
        MakeLike,
        ShowIncludes
    };

    Span<const char> cmd_line; // Must be C safe (NULL termination)
    Size cache_len;
    Size rsp_offset;
    bool skip_success;
    int skip_lines;
    DependencyMode deps_mode;
    const char *deps_filename; // Used by MakeLike mode
};

class Compiler {
    RG_DELETE_COPY(Compiler)

    mutable bool test_init = false;
    mutable bool test;

public:
    const char *name;
    const char *binary;

    Compiler(const char *name, const char *binary)
        : name(name), binary(binary) {}

    bool Test() const;

    virtual void MakePackCommand(Span<const char *const> pack_filenames, CompileMode compile_mode,
                                 const char *pack_options, const char *dest_filename,
                                 Allocator *alloc, Command *out_cmd) const = 0;

    virtual void MakePchCommand(const char *pch_filename, SourceType src_type, CompileMode compile_mode,
                                bool warnings, Span<const char *const> definitions,
                                Span<const char *const> include_directories, bool env_flags,
                                Allocator *alloc, Command *out_cmd) const = 0;
    virtual const char *GetPchObject(const char *pch_filename, Allocator *alloc) const = 0;

    virtual void MakeObjectCommand(const char *src_filename, SourceType src_type, CompileMode compile_mode,
                                   bool warnings, const char *pch_filename, Span<const char *const> definitions,
                                   Span<const char *const> include_directories, bool env_flags,
                                   const char *dest_filename, Allocator *alloc, Command *out_cmd) const = 0;

    virtual void MakeLinkCommand(Span<const char *const> obj_filenames, CompileMode compile_mode,
                                 Span<const char *const> libraries, LinkType link_type,
                                 bool env_flags, const char *dest_filename,
                                 Allocator *alloc, Command *out_cmd) const = 0;
};

extern const Span<const Compiler *const> Compilers;

}
