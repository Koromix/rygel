// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "lib/native/request/s3.hh"
#include "lib/native/request/ssh.hh"
#include "disk.hh"

namespace K {

enum class rk_DiskType {
    Local,
    S3,
    SFTP
};
static const char *const rk_DiskTypeNames[] = {
    "Local",
    "S3",
    "SFTP"
};

struct rk_S3Config {
    s3_Config remote;

    s3_LockMode lock = s3_LockMode::Governance;
    rk_ChecksumType checksum = rk_ChecksumType::CRC64nvme;
};

static const int64_t rk_MinimalRetention = 14 * 86400000ll; // 14 days
static const int64_t rk_MaximalRetention = 100 * 86400000ll; // 100 days

enum class rk_ConfigFlag {
    RequireAuth = 1 << 0,
    RequireAgent = 1 << 1
};

struct rk_Config {
    const char *url = nullptr;
    const char *key_filename = nullptr;

    rk_DiskType type = rk_DiskType::Local;
    rk_S3Config s3;
    ssh_Config ssh;

    int64_t retain = 0;
    bool safety = true;

    int threads = -1;
    int compression_level = 6;

    const char *agent_url = nullptr;
    const char *api_key = nullptr;
    int64_t agent_period = 20 * 60000; // 20 minutes

    BlockAllocator str_alloc;

    bool Complete(unsigned int flags = (int)rk_ConfigFlag::RequireAuth);
    bool Validate(unsigned int flags = (int)rk_ConfigFlag::RequireAuth) const;
};

bool rk_DecodeURL(Span<const char> url, rk_Config *out_config);
const char *rk_MakeURL(const rk_Config &config, Allocator *alloc);

bool rk_LoadConfig(StreamReader *st, rk_Config *out_config);
bool rk_LoadConfig(const char *filename, rk_Config *out_config);

}
