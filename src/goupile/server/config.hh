// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "instance.hh"
#include "user.hh"
#include "src/core/http/http.hh"
#include "src/core/request/sms.hh"
#include "src/core/request/smtp.hh"

namespace K {

struct Config {
    const char *config_filename = nullptr;
    const char *database_filename = nullptr;
    const char *database_directory = nullptr;
    const char *instances_directory = nullptr;
    const char *tmp_directory = nullptr;
    const char *archive_directory = nullptr;
    const char *snapshot_directory = nullptr;
    const char *view_directory = nullptr;
    const char *export_directory = nullptr;
    bool use_snapshots = true;

    PasswordComplexity user_password = PasswordComplexity::Moderate;
    PasswordComplexity admin_password = PasswordComplexity::Moderate;
    PasswordComplexity root_password = PasswordComplexity::Hard;
    bool custom_security = false;

    bool demo_mode = false;

    http_Config http { 8889 };

    smtp_Config smtp;
    sms_Config sms;

    bool Validate() const;

    BlockAllocator str_alloc;
};

bool LoadConfig(StreamReader *st, Config *out_config);
bool LoadConfig(const char *filename, Config *out_config);

}
