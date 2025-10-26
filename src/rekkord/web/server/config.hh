// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "src/core/http/http.hh"
#include "src/core/request/smtp.hh"

namespace K {

struct Config {
    const char *title = nullptr;
    const char *url = nullptr;

    const char *database_filename = nullptr;
    const char *tmp_directory = nullptr;

    int64_t update_period = 6 * 3600000;
    int64_t retry_delay = 10 * 60000;

    int64_t stale_delay = 30 * 3600000;
    int64_t mail_delay = 1 * 3600000;
    int64_t repeat_delay = 24 * 3600000;

    http_Config http { 8891 };

    smtp_Config smtp;

    BlockAllocator str_alloc;

    bool Validate() const;
};

bool LoadConfig(StreamReader *st, Config *out_config);
bool LoadConfig(const char *filename, Config *out_config);

}
