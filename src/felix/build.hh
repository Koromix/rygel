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
    bool stop_after_error = false;
    uint32_t features = 0;
    bool env = false;
    bool fake = false;
};

class Builder {
    RG_DELETE_COPY(Builder)

    struct Node {
        const char *text;
        const char *dest_filename;
        HeapArray<Size> triggers;

        // Set by compiler methods
        Command cmd;

        // Managed by Build method
        std::atomic_int semaphore;
        bool success;
    };

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

    // AddTarget, AddSource
    HeapArray<Node> nodes;
    Size total = 0;
    HashMap<const char *, Size> nodes_map;
    HashMap<const char *, const char *> build_map;
    HashMap<const char *, int64_t> mtime_map;

    // Build
    std::mutex out_mutex;
    HeapArray<const char *> clear_filenames;
    HashMap<const void *, const char *> rsp_map;
    Size progress;
    bool interrupted;
    HeapArray<WorkerState> workers;

    HashTable<const char *, CacheEntry> cache_map;
    HeapArray<const char *> cache_dependencies;

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

    bool AppendNode(const char *text, const char *dest_filename, const Command &cmd,
                    Span<const char *const> src_filenames);
    bool NeedsRebuild(const char *dest_filename, const Command &cmd,
                      Span<const char *const> src_filenames);
    bool IsFileUpToDate(const char *dest_filename, Span<const char *const> src_filenames);
    int64_t GetFileModificationTime(const char *filename);

    bool RunNode(Async *async, Node *node, bool verbose);
};

}
