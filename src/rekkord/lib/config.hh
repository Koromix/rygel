// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

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

    s3_StorageClass meta_storage = s3_StorageClass::STANDARD;
    s3_StorageClass data_storage = s3_StorageClass::STANDARD;

    rk_ChecksumType checksum = rk_ChecksumType::CRC64nvme;

    int64_t retain_duration = 0;
    bool retain_safety = true;
    s3_RetainMode retain_mode = s3_RetainMode::Governance;
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

    int threads = -1;
    int compression_level = 6;
    bool ocd = true;

    const char *link_url = nullptr;
    const char *link_key = nullptr;
    int64_t agent_period = 20 * 60000; // 20 minutes

    HeapArray<const char *> hooks_presave;
    HeapArray<const char *> hooks_postsave;
    HeapArray<const char *> hooks_prescan;
    HeapArray<const char *> hooks_postscan;

    BlockAllocator str_alloc;

    bool Complete();
    bool Validate(unsigned int flags = (int)rk_ConfigFlag::RequireAuth) const;
};

bool rk_DecodeURL(Span<const char> url, rk_Config *out_config);
const char *rk_MakeURL(const rk_Config &config, Allocator *alloc);

bool rk_LoadConfig(StreamReader *st, rk_Config *out_config);
bool rk_LoadConfig(const char *filename, rk_Config *out_config);

}
