// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"

namespace RG {

struct Config {
    const char *database_filename = nullptr;

    IPStack ip_stack = IPStack::Dual;
    int port = 8888;
    int threads = 4;
    const char *base_url = "/";
    int max_age = 3600;
    int sse_keep_alive = 120000;

    const char *main_color = "#ff6600";

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
