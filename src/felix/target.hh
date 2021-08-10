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

namespace RG {

struct TargetConfig;
struct TargetInfo;

enum class TargetType {
    Executable,
    Library,
    ExternalLibrary
};

enum class TargetPlatform {
    Windows = 1 << 0,
    Linux = 1 << 1,
    macOS = 1 << 2
};
static const char *const TargetPlatformNames[] = {
    "Windows",
    "Linux",
    "macOS"
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

    uint32_t enable_features;
    uint32_t disable_features;

    const SourceFileInfo *c_pch_src;
    const SourceFileInfo *cxx_pch_src;
    HeapArray<const char *> pchs;

    HeapArray<const SourceFileInfo *> sources;

    HeapArray<const char *> pack_filenames;
    const char *pack_options;

    uint32_t CombineFeatures(uint32_t defaults) const
    {
        defaults |= enable_features;
        defaults &= ~disable_features;

        return defaults;
    }

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
