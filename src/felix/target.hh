// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "compiler.hh"

namespace RG {

struct TargetConfig;

enum class TargetType {
    Executable,
    Library,
    ExternalLibrary
};

struct SourceFile {
    const char *filename;
    SourceType type;
};

enum class PackLinkType {
    Static,
    Module,
    ModuleIfDebug
};

struct Target {
    const char *name;
    TargetType type;
    bool enable_by_default;

    HeapArray<const char *> imports;

    HeapArray<const char *> definitions;
    HeapArray<const char *> include_directories;
    HeapArray<const char *> libraries;

    const char *c_pch_filename;
    const char *cxx_pch_filename;
    HeapArray<SourceFile> sources;

    HeapArray<const char *> pack_filenames;
    const char *pack_options;
    PackLinkType pack_link_type;

    RG_HASH_TABLE_HANDLER(Target, name);
};

struct TargetSet {
    HeapArray<Target> targets;
    HashTable<const char *, const Target *> targets_map;

    BlockAllocator str_alloc;
};

class TargetSetBuilder {
    BlockAllocator temp_alloc;

    TargetSet set;

    HashMap<Span<const char>, Size> targets_map;

public:
    bool LoadIni(StreamReader &st);
    bool LoadFiles(Span<const char *const> filenames);

    void Finish(TargetSet *out_set);

private:
    const Target *CreateTarget(TargetConfig *target_config);
};

bool LoadTargetSet(Span<const char *const> filenames, TargetSet *out_set);

}
