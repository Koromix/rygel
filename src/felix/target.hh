// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"
#include "compiler.hh"

namespace RG {

struct TargetConfig;
struct TargetInfo;

enum class TargetType {
    Executable,
    Library,
    ExternalLibrary
};

struct SourceFileInfo {
    // In order to build source files with the correct definitions (and include directories, etc.),
    // we need to use the options from the target that first found this source file!
    const TargetInfo *target;

    const char *filename;
    SourceType type;

    RG_HASHTABLE_HANDLER(SourceFileInfo, filename);
};

struct TargetInfo {
    const char *name;
    TargetType type;
    bool enable_by_default;

    HeapArray<const TargetInfo *> imports;

    HeapArray<const char *> definitions;
    HeapArray<const char *> export_definitions;
    HeapArray<const char *> include_directories;
    HeapArray<const char *> libraries;
    uint32_t features;

    const SourceFileInfo *c_pch_src;
    const SourceFileInfo *cxx_pch_src;
    HeapArray<const SourceFileInfo *> sources;

    HeapArray<const char *> pack_filenames;
    const char *pack_options;

    RG_HASHTABLE_HANDLER(TargetInfo, name);
};

struct TargetSet {
    BucketArray<TargetInfo> targets;
    HashTable<const char *, TargetInfo *> targets_map;

    BucketArray<SourceFileInfo> sources;
    HashTable<const char *, SourceFileInfo *> sources_map;

    BlockAllocator str_alloc;
};

class TargetSetBuilder {
    RG_DELETE_COPY(TargetSetBuilder)

    BlockAllocator temp_alloc;

    TargetSet set;

public:
    TargetSetBuilder() = default;

    bool LoadIni(StreamReader *st);
    bool LoadFiles(Span<const char *const> filenames);

    void Finish(TargetSet *out_set);

private:
    const TargetInfo *CreateTarget(TargetConfig *target_config);
    const SourceFileInfo *CreateSource(const TargetInfo *target, const char *filename, SourceType type);
};

bool LoadTargetSet(Span<const char *const> filenames, TargetSet *out_set);

}
