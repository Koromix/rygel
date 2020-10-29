// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../drd/libdrd/libdrd.hh"
#include "../../web/libhttp/libhttp.hh"

namespace RG {

struct Config {
    HeapArray<const char *> table_directories;
    const char *profile_directory = nullptr;

    drd_Sector sector = drd_Sector::Public;

    const char *mco_authorization_filename = nullptr;
    mco_DispenseMode mco_dispense_mode = mco_DispenseMode::J;
    HeapArray<const char *> mco_stay_directories;
    HeapArray<const char *> mco_stay_filenames;

    http_Config http {.port = 8888};
    int max_age = 3600;

    BlockAllocator str_alloc;
};

bool LoadConfig(StreamReader *st, Config *out_config);
bool LoadConfig(const char *filename, Config *out_config);

}
