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

enum class CompileFeature {
    Optimize = 1 << 0,
    PCH = 1 << 1,
    DebugInfo = 1 << 2,
    StaticLink = 1 << 3,
    ASan = 1 << 4,
    TSan = 1 << 5,
    UBSan = 1 << 6,
    LTO = 1 << 7,
    SafeStack = 1 << 8,
    ZeroInit = 1 << 9,
    CFI = 1 << 10,
    ShuffleCode = 1 << 11
};
static const OptionDesc CompileFeatureOptions[] = {
    {"Optimize",    "Optimize generated builds"},
    {"PCH",         "Use precompiled headers for faster compilation"},
    {"DebugInfo",   "Add debug information to generated binaries"},
    {"StaticLink",  "Static link base system libraries (libc, etc.)"},
    {"ASan",        "Enable AdressSanitizer (ASan)"},
    {"TSan",        "Enable ThreadSanitizer (TSan)"},
    {"UBSan",       "Enable UndefinedBehaviorSanitizer (UBSan)"},
    {"LTO",         "Enable Link-Time Optimization"},
    {"SafeStack",   "Enable SafeStack protection (Clang)"},
    {"ZeroInit",    "Zero-init all undefined variables (Clang)"},
    {"CFI",         "Enable forward-edge CFI protection (Clang LTO)"},
    {"ShuffleCode", "Randomize ordering of data and functions (Clang)"}
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

public:
    const char *name;

    virtual ~Compiler() {}

    virtual bool CheckFeatures(uint32_t features) const = 0;

    virtual const char *GetObjectExtension() const = 0;
    virtual const char *GetExecutableExtension() const = 0;

    virtual void MakePackCommand(Span<const char *const> pack_filenames, bool optimize,
                                 const char *pack_options, const char *dest_filename,
                                 Allocator *alloc, Command *out_cmd) const = 0;

    virtual void MakePchCommand(const char *pch_filename, SourceType src_type,
                                bool warnings, Span<const char *const> definitions,
                                Span<const char *const> include_directories, uint32_t features, bool env_flags,
                                Allocator *alloc, Command *out_cmd) const = 0;
    virtual const char *GetPchObject(const char *pch_filename, Allocator *alloc) const = 0;

    virtual void MakeObjectCommand(const char *src_filename, SourceType src_type,
                                   bool warnings, const char *pch_filename, Span<const char *const> definitions,
                                   Span<const char *const> include_directories, uint32_t features, bool env_flags,
                                   const char *dest_filename, Allocator *alloc, Command *out_cmd) const = 0;

    virtual void MakeLinkCommand(Span<const char *const> obj_filenames,
                                 Span<const char *const> libraries, LinkType link_type,
                                 uint32_t features, bool env_flags, const char *dest_filename,
                                 Allocator *alloc, Command *out_cmd) const = 0;

protected:
    Compiler(const char *name) : name(name) {}
};

struct SupportedCompiler {
    const char *name;
    const char *cc;
};

struct CompilerInfo {
    const char *cc = nullptr;
    const char *ld = nullptr;
};

std::unique_ptr<const Compiler> PrepareCompiler(CompilerInfo info);

extern const Span<const SupportedCompiler> SupportedCompilers;

}
