// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "src/core/http/http.hh"
#include "src/core/request/smtp.hh"

namespace K {

struct Config {
    struct PageInfo {
        const char *title;
        const char *url;
    };

    const char *title = nullptr;
    const char *contact = nullptr;
    const char *url = nullptr;
    bool test_mode = false;

    HeapArray<PageInfo> pages;

    const char *database_filename = nullptr;
    const char *vault_directory = nullptr;
    const char *tmp_directory = nullptr;
    const char *static_directory = nullptr;

    http_Config http { 8890 };

    smtp_Config smtp;

    BlockAllocator str_alloc;

    bool Validate() const;
};

bool LoadConfig(StreamReader *st, Config *out_config);
bool LoadConfig(const char *filename, Config *out_config);

}
