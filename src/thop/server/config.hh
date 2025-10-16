// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
