// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "lib/native/http/http.hh"
#include "lib/native/request/smtp.hh"
#include "lib/native/sso/oidc.hh"

namespace K {

struct Config {
    const char *title = nullptr;
    const char *url = nullptr;

    const char *database_filename = nullptr;
    const char *tmp_directory = nullptr;

    int64_t stale_delay = 30 * 3600000;
    int64_t mail_delay = 1 * 3600000;
    int64_t repeat_delay = 24 * 3600000;

    http_Config http { 8891 };

    smtp_Config smtp;

    bool internal_auth = true;
    HeapArray<oidc_Provider> oidc_providers;
    HashMap<const char *, const oidc_Provider *> oidc_map;

    BlockAllocator str_alloc;

    bool Validate() const;
};

bool LoadConfig(StreamReader *st, Config *out_config);
bool LoadConfig(const char *filename, Config *out_config);

}
