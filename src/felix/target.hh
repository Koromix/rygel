// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../core/libcc/libcc.hh"
#include "compiler.hh"

namespace RG {

struct TargetConfig;

enum class TargetType {
    Executable,
    Library,
    ExternalLibrary
};

struct SourceFileInfo {
    const char *filename;
    SourceType type;
};

enum class PackLinkMode {
    Static,
    Module,
    ModuleIfDebug
};

struct TargetInfo {
    const char *name;
    TargetType type;
    bool enable_by_default;

    HeapArray<const char *> imports;

    HeapArray<const char *> definitions;
    HeapArray<const char *> export_definitions;
    HeapArray<const char *> include_directories;
    HeapArray<const char *> libraries;

    const char *c_pch_filename;
    const char *cxx_pch_filename;
    HeapArray<SourceFileInfo> sources;

    HeapArray<const char *> pack_filenames;
    const char *pack_options;
    PackLinkMode pack_link_mode;

    RG_HASHTABLE_HANDLER(TargetInfo, name);
};

struct TargetSet {
    HeapArray<TargetInfo> targets;
    HashTable<const char *, const TargetInfo *> targets_map;

    BlockAllocator str_alloc;
};

class TargetSetBuilder {
    BlockAllocator temp_alloc;

    TargetSet set;

    HashMap<Span<const char>, Size> targets_map;

public:
    bool LoadIni(StreamReader *st);
    bool LoadFiles(Span<const char *const> filenames);

    void Finish(TargetSet *out_set);

private:
    const TargetInfo *CreateTarget(TargetConfig *target_config);
};

bool LoadTargetSet(Span<const char *const> filenames, TargetSet *out_set);

}
