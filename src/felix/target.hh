// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "toolchain.hh"

enum class TargetType {
    Executable,
    Library
};

struct TargetConfig {
    const char *name;
    TargetType type;

    HeapArray<const char *> src_directories;
    HeapArray<const char *> src_filenames;
    HeapArray<const char *> exclusions;

    const char *c_pch_filename;
    const char *cxx_pch_filename;

    HeapArray<const char *> imports;
    HeapArray<const char *> include_directories;
    HeapArray<const char *> libraries;

    HASH_TABLE_HANDLER(TargetConfig, name);
};

struct TargetData {
    const char *name;
    TargetType type;

    Span<const char *const> include_directories;
    HeapArray<const char *> libraries;

    HeapArray<ObjectInfo> pch_objects;
    const char *c_pch_filename;
    const char *cxx_pch_filename;

    HeapArray<ObjectInfo> objects;
    const char *dest_filename;

    HASH_TABLE_HANDLER(TargetData, name);
};

struct TargetSet {
    HeapArray<TargetData> targets;
    HashTable<const char *, const TargetData *> targets_map;

    BlockAllocator str_alloc;
};

class TargetSetBuilder {
    const char *output_directory;

    TargetSet set;

    HashMap<const char *, Size> targets_map;

public:
    std::function<const TargetConfig *(const char *)> resolve_import;

    TargetSetBuilder(const char *output_directory)
        : output_directory(output_directory) {}

    const TargetData *CreateTarget(const TargetConfig &target_config);

    void Finish(TargetSet *out_set);
};
