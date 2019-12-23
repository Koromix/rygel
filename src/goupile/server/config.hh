// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "../../web/libserver/libserver.hh"

namespace RG {

struct Config {
    const char *app_key = nullptr;
    const char *app_name = nullptr;

    const char *files_directory = nullptr;
    const char *database_filename = nullptr;
    bool use_offline = false;

    http_Config http;
    int max_age = 3600;
    int sse_keep_alive = 60000;

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
