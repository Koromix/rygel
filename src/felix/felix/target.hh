// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "compiler.hh"

struct TargetConfig;

enum class TargetType {
    Executable,
    Library
};

struct Target {
    const char *name;
    TargetType type;

    HeapArray<const char *> imports;

    HeapArray<const char *> definitions;
    HeapArray<const char *> include_directories;
    HeapArray<const char *> libraries;

    HeapArray<ObjectInfo> pch_objects;
    const char *c_pch_filename;
    const char *cxx_pch_filename;

    HeapArray<ObjectInfo> objects;
    const char *dest_filename;

    HASH_TABLE_HANDLER(Target, name);
};

struct TargetSet {
    HeapArray<Target> targets;
    HashTable<const char *, const Target *> targets_map;

    BlockAllocator str_alloc;
};

class TargetSetBuilder {
    const char *output_directory;

    BlockAllocator temp_alloc;

    TargetSet set;

    HashMap<const char *, Size> targets_map;

public:
    TargetSetBuilder(const char *output_directory)
        : output_directory(output_directory) {}

    bool LoadIni(StreamReader &st);
    bool LoadFiles(Span<const char *const> filenames);

    void Finish(TargetSet *out_set);

private:
    const Target *CreateTarget(TargetConfig *target_config);
};

bool LoadTargetSet(Span<const char *const> filenames, const char *output_directory,
                   TargetSet *out_set);
