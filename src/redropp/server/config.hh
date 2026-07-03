// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "lib/native/http/http.hh"
#include "lib/native/request/smtp.hh"
#include "lib/native/request/s3.hh"
#include "lib/native/sso/oidc.hh"

namespace K {

struct Config {
    const char *title = nullptr;
    const char *url = nullptr;

    const char *database_filename = nullptr;
    const char *tmp_directory = nullptr;

    int64_t quota = Megabytes(1000);
    bool explicit_quota = false;
    int64_t max_duration = 90 * 86400000ull; // 90 days
    bool explicit_duration = false;
    bool allow_infinite = false;

    s3_Config s3;

    http_Config http { 8894 };

    smtp_Config smtp;

    bool internal_auth = true;
    HeapArray<oidc_Provider> oidc_providers;
    HashMap<const char *, const oidc_Provider *> oidc_map;

    BlockAllocator str_alloc;

    bool Complete();
    bool Validate() const;
};

bool LoadConfig(StreamReader *st, Config *out_config);
bool LoadConfig(const char *filename, Config *out_config);

}
