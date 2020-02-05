// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"
#include "compiler.hh"
#include "target.hh"

namespace RG {

struct BuildNode {
    const char *text;

    const char *dest_filename;
    BuildCommand cmd;

    bool sync_after;
};

struct BuildSet {
    HeapArray<BuildNode> nodes;

    HashMap<const char *, const char *> target_filenames;

    BlockAllocator str_alloc;
};

class BuildSetBuilder {
    BlockAllocator temp_alloc;

    bool version_init = false;
    const char *version_obj_filename = nullptr;

    // Reuse for performance
    HeapArray<const char *> obj_filenames;
    HeapArray<const char *> definitions;

    HeapArray<BuildNode> pch_nodes;
    HeapArray<BuildNode> obj_nodes;
    HeapArray<BuildNode> link_nodes;
    BlockAllocator str_alloc;

    HashMap<const char *, int64_t> mtime_map;
    HashSet<const char *> output_set;

    HashMap<const char *, const char *> target_filenames;

public:
    const char *output_directory;
    const Compiler *compiler;
    CompileMode compile_mode = CompileMode::Debug;
    const char *version_str = nullptr;

    BuildSetBuilder(const char *output_directory, const Compiler *compiler)
        : output_directory(output_directory), compiler(compiler) {}

    bool AppendTargetCommands(const Target &target);

    void Finish(BuildSet *out_set);

private:
    bool NeedsRebuild(const char *src_filename, const char *dest_filename,
                      const char *deps_filename);
    bool IsFileUpToDate(const char *dest_filename, Span<const char *const> src_filenames);
    int64_t GetFileModificationTime(const char *filename);
};

bool RunBuildNodes(Span<const BuildNode> nodes, int jobs, bool verbose);

}
