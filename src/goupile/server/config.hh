// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../web/libhttp/libhttp.hh"

namespace RG {

struct Config {
    const char *app_key = nullptr;
    const char *app_name = nullptr;

    const char *files_directory = nullptr;
    const char *database_filename = nullptr;

    bool use_offline = false;
    bool offline_records = false;
    Size max_file_size = Megabytes(4);
    bool allow_guests = false;

    http_Config http {.port = 8889};
    int max_age = 3600;

    BlockAllocator str_alloc;
};

class ConfigBuilder {
    RG_DELETE_COPY(ConfigBuilder)

    Config config;

public:
    ConfigBuilder() = default;

    bool LoadIni(StreamReader *st);
    bool LoadFiles(Span<const char *const> filenames);

    void Finish(Config *out_config);
};

bool LoadConfig(Span<const char *const> filenames, Config *out_config);

}
