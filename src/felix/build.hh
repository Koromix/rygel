// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"
#include "compiler.hh"
#include "target.hh"

namespace RG {

struct BuildSettings {
    // Mandatory
    const char *output_directory = nullptr;
    const Compiler *compiler = nullptr;

    // Optional
    CompileMode compile_mode = CompileMode::Debug;
    const char *version_str = "(unknown version)";
};

class Builder {
    struct Node {
        const char *text;

        const char *dest_filename;
        const char *deps_filename;

        BuildCommand cmd;
    };

    BuildSettings build;

    bool version_init = false;
    const char *version_obj_filename = nullptr;

    // Reuse for performance
    HeapArray<const char *> obj_filenames;

    HeapArray<Node> prep_nodes;
    HeapArray<Node> obj_nodes;
    HeapArray<Node> link_nodes;
    BlockAllocator str_alloc;

    HashMap<const char *, int64_t> mtime_map;
    HashSet<const char *> output_set;

public:
    HashMap<const char *, const char *> target_filenames;

    Builder(const BuildSettings &build);

    bool AddTarget(const Target &target);
    bool Build(int jobs, bool verbose);

private:
    bool NeedsRebuild(const char *src_filename, const char *pch_filename,
                      const char *dest_filename, const char *deps_filename);
    bool IsFileUpToDate(const char *dest_filename, Span<const char *const> src_filenames);
    int64_t GetFileModificationTime(const char *filename);

    bool RunNodes(Span<const Node> nodes, int jobs, bool verbose, Size progress, Size total);
};

}
