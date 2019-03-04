// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"

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

    HeapArray<const char *> include_directories;
    HeapArray<const char *> libraries;

    HASH_TABLE_HANDLER(TargetConfig, name);
};

struct Config {
    HeapArray<TargetConfig> targets;
    HashTable<const char *, const TargetConfig *> targets_map;

    BlockAllocator str_alloc;
};

class ConfigBuilder {
    Config config;
    HashSet<const char *> targets_set;

public:
    bool LoadIni(StreamReader &st);
    bool LoadFiles(Span<const char *const> filenames);

    void Finish(Config *out_config);
};

bool LoadConfig(Span<const char *const> filenames, Config *out_config);
