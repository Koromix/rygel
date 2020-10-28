// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../web/libhttp/libhttp.hh"

namespace RG {

enum SyncMode {
    Offline,
    Mirror
};
static const char *const SyncModeNames[] = {
    "Offline",
    "Mirror"
};

struct Config {
    const char *app_key = nullptr;
    const char *app_name = nullptr;

    const char *live_directory = nullptr;
    const char *temp_directory = nullptr;
    const char *database_filename = nullptr;

    bool use_offline = false;
    int max_file_size = (int)Megabytes(8);
    SyncMode sync_mode = SyncMode::Offline;
    const char *demo_user = nullptr;

    http_Config http {.port = 8889};
    int max_age = 900;

    BlockAllocator str_alloc;
};

bool LoadConfig(StreamReader *st, Config *out_config);
bool LoadConfig(const char *filename, Config *out_config);

}
