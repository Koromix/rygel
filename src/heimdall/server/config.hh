// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "lib/native/http/http.hh"
#include "lib/native/request/smtp.hh"

namespace K {

struct Config {
    const char *project_directory = nullptr;
    const char *tmp_directory = nullptr;

    http_Config http { 8892 };

    BlockAllocator str_alloc;

    bool Validate() const;
};

bool LoadConfig(StreamReader *st, Config *out_config);
bool LoadConfig(const char *filename, Config *out_config);

}
