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
    bool pch = true;
    CompileMode compile_mode = CompileMode::Debug;
    const char *version_str = "(unknown version)";
    bool rebuild = false;
    bool env = false;
};

struct BuildNode {
    enum class DependencyMode {
        None,
        MakeLike,
        ShowIncludes
    };

    const char *text;
    const char *dest_filename;

    // Set by compiler methods
    Span<const char> cmd_line; // Must be C safe (NULL termination)
    Size cache_len;
    Size rsp_offset;
    bool skip_success;
    int skip_lines;
    DependencyMode deps_mode;
    const char *deps_filename; // Used by MakeLike mode
};

class Builder {
    struct CacheEntry {
        const char *filename;
        Span<const char> cmd_line;

        Size deps_offset;
        Size deps_len;

        RG_HASHTABLE_HANDLER(CacheEntry, filename);
    };

    struct WorkerState {
        HeapArray<CacheEntry> entries;
        HeapArray<const char *> dependencies;

        BlockAllocator str_alloc;
    };

    BuildSettings build;
    const char *cache_filename;

    bool version_init = false;
    const char *version_obj_filename = nullptr;

    // Reuse for performance
    HeapArray<const char *> obj_filenames;

    HeapArray<BuildNode> prep_nodes;
    HeapArray<BuildNode> obj_nodes;
    HeapArray<BuildNode> link_nodes;

    HashTable<const char *, CacheEntry> cache_map;
    HeapArray<const char *> cache_dependencies;

    HashMap<const char *, int64_t> mtime_map;
    HashMap<const char *, const char *> build_map;

    HeapArray<WorkerState> workers;

    BlockAllocator str_alloc;

public:
    HashMap<const char *, const char *> target_filenames;

    Builder(const BuildSettings &build);

    bool AddTarget(const TargetInfo &target);
    const char *AddSource(const SourceFileInfo &src);

    bool Build(int jobs, bool verbose);

private:
    void SaveCache();
    void LoadCache();

    bool NeedsRebuild(const char *dest_filename, const BuildNode &node,
                      Span<const char *const> src_filenames);
    bool IsFileUpToDate(const char *dest_filename, Span<const char *const> src_filenames);
    int64_t GetFileModificationTime(const char *filename);

    bool RunNodes(Async *async, Span<const BuildNode> nodes, bool verbose, Size progress, Size total);
};

}
