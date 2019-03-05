// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "target.hh"

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
