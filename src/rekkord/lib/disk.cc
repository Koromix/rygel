// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/native/base/base.hh"
#include "config.hh"
#include "disk.hh"

namespace K {

rk_Disk::~rk_Disk()
{
}

bool rk_Disk::IsEmpty()
{
    bool empty = true;

    empty &= ListFiles([&](const char *, int64_t) {
        empty = false;
        return false;
    });

    return empty;
}

std::unique_ptr<rk_Disk> rk_OpenDisk(const rk_Config &config)
{
    K_ASSERT(config.Validate(0));

    switch (config.type) {
        case rk_DiskType::Local: return rk_OpenLocalDisk(config.url);
        case rk_DiskType::SFTP: return rk_OpenSftpDisk(config.ssh);
        case rk_DiskType::S3: return rk_OpenS3Disk(config.s3);
    }

    K_UNREACHABLE();
}

}
