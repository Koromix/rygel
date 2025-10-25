// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "src/drd/libdrd/libdrd.hh"
#include "src/core/http/http.hh"

namespace K {

struct Config {
    HeapArray<const char *> table_directories;
    const char *profile_directory = nullptr;

    drd_Sector sector = drd_Sector::Public;

    const char *mco_authorization_filename = nullptr;
    mco_DispenseMode mco_dispense_mode = mco_DispenseMode::J;
    HeapArray<const char *> mco_stay_directories;
    HeapArray<const char *> mco_stay_filenames;

    http_Config http { 8888 };
    const char *base_url = "/";
    int64_t max_age = 3600000;

    BlockAllocator str_alloc;

    bool Validate() const;
};

bool LoadConfig(StreamReader *st, Config *out_config);
bool LoadConfig(const char *filename, Config *out_config);

}
