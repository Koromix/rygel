// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "drdc.hh"

namespace RG {

struct Config {
    HeapArray<const char *> table_directories;
    const char *profile_directory = nullptr;

    drd_Sector sector = drd_Sector::Public;

    const char *mco_authorization_filename = nullptr;

    BlockAllocator str_alloc;
};

class ConfigBuilder {
    Config config;

public:
    bool LoadIni(StreamReader &st);
    bool LoadFiles(Span<const char *const> filenames);

    void Finish(Config *out_config);
};

bool LoadConfig(Span<const char *const> filenames, Config *out_config);

}
