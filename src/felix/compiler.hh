// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

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

enum class CompileFeature {
    Strip = 1 << 0,
    Static = 1 << 1,
    ASan = 1 << 2,
    TSan = 1 << 3
};
static const char *const CompileFeatureNames[] = {
    "Strip",
    "Static",
    "ASan",
    "TSan"
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
    uint32_t supported_features;

    Compiler(const char *name, const char *binary, uint32_t supported_features)
        : name(name), binary(binary), supported_features(supported_features) {}

    bool Test() const;

    virtual const char *GetObjectExtension() const = 0;
    virtual const char *GetExecutableExtension() const = 0;

    virtual void MakePackCommand(Span<const char *const> pack_filenames, CompileMode compile_mode,
                                 const char *pack_options, const char *dest_filename,
                                 Allocator *alloc, Command *out_cmd) const = 0;

    virtual void MakePchCommand(const char *pch_filename, SourceType src_type, CompileMode compile_mode,
                                bool warnings, Span<const char *const> definitions,
                                Span<const char *const> include_directories, uint32_t features, bool env_flags,
                                Allocator *alloc, Command *out_cmd) const = 0;
    virtual const char *GetPchObject(const char *pch_filename, Allocator *alloc) const = 0;

    virtual void MakeObjectCommand(const char *src_filename, SourceType src_type, CompileMode compile_mode,
                                   bool warnings, const char *pch_filename, Span<const char *const> definitions,
                                   Span<const char *const> include_directories, uint32_t features, bool env_flags,
                                   const char *dest_filename, Allocator *alloc, Command *out_cmd) const = 0;

    virtual void MakeLinkCommand(Span<const char *const> obj_filenames, CompileMode compile_mode,
                                 Span<const char *const> libraries, LinkType link_type,
                                 uint32_t features, bool env_flags, const char *dest_filename,
                                 Allocator *alloc, Command *out_cmd) const = 0;
};

extern const Span<const Compiler *const> Compilers;

}
