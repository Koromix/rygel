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
#include "src/core/http/http.hh"
#include "src/core/request/smtp.hh"

namespace RG {

struct Config {
    const char *title = nullptr;
    const char *url = nullptr;

    const char *database_filename = nullptr;
    const char *tmp_directory = nullptr;
    bool sync_full = false;

    int64_t update_delay = 6 * 3600000;
    int64_t retry_delay = 10 * 60000;

    http_Config http { 8891 };
    const char *require_host = nullptr;

    smtp_Config smtp;

    BlockAllocator str_alloc;

    bool Validate() const;
};

bool LoadConfig(StreamReader *st, Config *out_config);
bool LoadConfig(const char *filename, Config *out_config);

}
