// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"

struct Target {
    const char *name;

    HeapArray<const char *> src_directories;
    HeapArray<const char *> libraries;

    HASH_TABLE_HANDLER(Target, name);
};

struct Config {
    HeapArray<Target> targets;
    HashTable<const char *, const Target *> targets_map;

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
