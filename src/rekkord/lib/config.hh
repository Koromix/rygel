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
#include "src/core/request/s3.hh"
#include "src/core/request/ssh.hh"

namespace RG {

enum class rk_DiskType {
    Local,
    S3,
    SFTP
};

struct rk_S3Config {
    s3_Config remote;
    s3_LockMode lock = s3_LockMode::Governance;
};

static const int64_t rk_MinimalRetention = 14 * 86400000ll; // 14 days
static const int64_t rk_MaximalRetention = 100 * 86400000ll; // 100 days

struct rk_Config {
    const char *url = nullptr;

    const char *username = nullptr;
    const char *password = nullptr;
    const char *key_filename = nullptr;

    rk_DiskType type = rk_DiskType::Local;
    rk_S3Config s3;
    ssh_Config ssh;

    int64_t retain = 0;
    bool safety = true;

    int threads = -1;
    int compression_level = 6;

    BlockAllocator str_alloc;

    bool Complete(bool require_auth);
    bool Validate(bool require_auth) const;
};

bool rk_DecodeURL(Span<const char> url, rk_Config *out_config);

bool rk_LoadConfig(StreamReader *st, rk_Config *out_config);
bool rk_LoadConfig(const char *filename, rk_Config *out_config);

}
