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
#include "compiler.hh"
#include "target.hh"

namespace RG {

struct BuildSettings {
    // Mandatory
    const char *output_directory = nullptr;
    const Compiler *compiler = nullptr;

    // Optional
    const char *version_str = nullptr;
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

    struct DependencyEntry {
        const char *filename;
        int64_t mtime;
    };

    struct WorkerState {
        HeapArray<CacheEntry> entries;
        HeapArray<const char *> dependencies;

        BlockAllocator str_alloc;
    };

    struct BuildKey {
        const char *ns;
        const char *filename;

        bool operator==(const BuildKey &other) const
        {
            return (ns ? (other.ns && TestStr(ns, other.ns)) : !other.ns) &&
                   TestStr(filename, other.filename);
        }
        bool operator !=(const BuildKey &other) const { return !(*this == other); }

        uint64_t Hash() const
        {
            uint64_t hash = (ns ? HashTraits<const char *>::Hash(ns) : 0ull) ^
                            HashTraits<const char *>::Hash(filename);
            return hash;
        }
    };

    BuildSettings build;
    const char *cache_directory;
    const char *cache_filename;

    // Core host targets (if any)
    BucketArray<TargetInfo> core_targets;
    HashMap<const char *, TargetInfo *> core_targets_map;
    BucketArray<SourceFileInfo> core_sources;

    // AddTarget, AddSource
    HeapArray<Node> nodes;
    Size total = 0;
    HashMap<const char *, Size> nodes_map;
    HashMap<BuildKey, const char *> build_map;
    HashMap<const char *, int64_t> mtime_map;

    // Build
    std::mutex out_mutex;
    HeapArray<const char *> clear_filenames;
    HashMap<const void *, const char *> rsp_map;
    Size progress;
    HeapArray<WorkerState> workers;

    HashTable<const char *, CacheEntry> cache_map;
    HeapArray<DependencyEntry> cache_dependencies;

    BlockAllocator str_alloc;

public:
    HashMap<const char *, const char *> target_filenames;

    Builder(const BuildSettings &build);

    bool AddTarget(const TargetInfo &target);
    const char *AddSource(const SourceFileInfo &src, const char *ns = nullptr);

    bool Build(int jobs, bool verbose);

private:
    void SaveCache();
    void LoadCache();

    bool AppendNode(const char *text, const char *dest_filename, const Command &cmd,
                    Span<const char *const> src_filenames, const char *ns);
    bool NeedsRebuild(const char *dest_filename, const Command &cmd,
                      Span<const char *const> src_filenames);
    bool IsFileUpToDate(const char *dest_filename, Span<const char *const> src_filenames);
    bool IsFileUpToDate(const char *dest_filename, Span<const DependencyEntry> dependencies);
    int64_t GetFileModificationTime(const char *filename, bool retry = false);

    bool RunNode(Async *async, Node *node, bool verbose);
};

}
