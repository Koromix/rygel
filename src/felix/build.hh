// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "src/core/base/base.hh"
#include "compiler.hh"
#include "locate.hh"
#include "target.hh"

namespace RG {

struct BuildSettings {
    // Mandatory
    const char *output_directory = nullptr;
    const Compiler *compiler = nullptr;

    // Optional
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
        const char *ns = nullptr;
        const char *filename = nullptr;

        bool operator==(const BuildKey &other) const
        {
            bool test = TestStr(ns, other.ns) &&
                        TestStr(filename, other.filename);
            return test;
        }
        bool operator !=(const BuildKey &other) const { return !(*this == other); }

        uint64_t Hash() const
        {
            uint64_t hash = HashTraits<const char *>::Hash(ns) ^
                            HashTraits<const char *>::Hash(filename);
            return hash;
        }

        BuildKey() = default;
        BuildKey(const char *filename) : ns("default"), filename(filename) {}
        BuildKey(const char *ns, const char *filename) : ns(ns), filename(filename) {}
    };

    BuildSettings build;
    const char *cache_directory;
    const char *shared_directory;
    const char *cache_filename;
    const char *compile_filename;
    const char *current_ns = "default";

    // Qt stuff
    const QtInfo *qt = nullptr;
    bool missing_qt = false;

    // Javascript bundler
    const char *esbuild_binary = nullptr;

    // Core host targets (if any)
    BucketArray<TargetInfo> core_targets;
    HashMap<const char *, TargetInfo *> core_targets_map;
    BucketArray<SourceFileInfo> core_sources;

    // AddTarget, AddSource
    HeapArray<Node> nodes;
    Size total = 0;
    HashMap<const char *, Size> nodes_map;
    HashMap<BuildKey, const char *> build_map;
    HashMap<const char *, const char *> moc_map;
    HashMap<const char *, int64_t> mtime_map;
    HashMap<const void *, const char *> custom_flags;

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

    bool AddTarget(const TargetInfo &target, const char *version_str);
    bool AddSource(const SourceFileInfo &src);

    bool Build(int jobs, bool verbose);

private:
    Command InitCommand();

    void SaveCache();
    void LoadCache();

    void SaveCompile();

    bool PrepareQtSdk(int64_t version);
    bool PrepareEsbuild();

    bool AddCppSource(const SourceFileInfo &src, HeapArray<const char *> *obj_filenames = nullptr);
    bool AddAssemblySource(const SourceFileInfo &src, HeapArray<const char *> *obj_filenames = nullptr);
    const char *AddEsbuildSource(const SourceFileInfo &src);
    const char *AddQtUiSource(const SourceFileInfo &src);
    const char *AddQtResource(const TargetInfo &target, Span<const char *> qrc_filenames);

    bool AddQtDirectories(const SourceFileInfo &src, HeapArray<const char *> *out_list);
    bool AddQtLibraries(const TargetInfo &target, HeapArray<const char *> *obj_filenames,
                                                  HeapArray<const char *> *link_libraries);

    bool CompileMocHelper(const SourceFileInfo &src, Span<const char *const> system_directories, uint32_t features);
    const char *CompileStaticQtHelper(const TargetInfo &target);
    void ParsePrlFile(const char *filename, HeapArray<const char *> *out_libraries);

    bool UpdateVersionSource(const char *target, const char *version, const char *dest_filename);

    const char *BuildObjectPath(Span<const char> src_filename, const char *output_directory,
                                const char *prefix, const char *suffix);
    const char *GatherFlags(const TargetInfo& target, SourceType type);

    bool AppendNode(const char *text, const char *dest_filename, const Command &cmd,
                    Span<const char *const> src_filenames);
    bool NeedsRebuild(const char *dest_filename, const Command &cmd,
                      Span<const char *const> src_filenames);
    bool IsFileUpToDate(const char *dest_filename, Span<const char *const> src_filenames);
    bool IsFileUpToDate(const char *dest_filename, Span<const DependencyEntry> dependencies);
    int64_t GetFileModificationTime(const char *filename);

    bool RunNode(Async *async, Node *node, bool verbose);
};

}
