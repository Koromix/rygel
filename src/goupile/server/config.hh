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
#include "instance.hh"
#include "src/core/http/http.hh"
#include "src/core/request/sms.hh"
#include "src/core/request/smtp.hh"

namespace K {

enum class PasswordComplexity {
    Easy,
    Moderate,
    Hard
};
static const char *const PasswordComplexityNames[] = {
    "Easy",
    "Moderate",
    "Hard"
};

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
