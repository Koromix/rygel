// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"

struct Config {
    enum class IPVersion {
        Dual,
        IPv4,
        IPv6
    };

    const char *profile_directory = nullptr;

    IPVersion ip_version = IPVersion::Dual;
    int port = 8888;
    int threads = 4;
    const char *base_url = "/";

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
