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

#include "src/core/base/base.hh"
#include "config.hh"
#include "disk.hh"

namespace RG {

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
    RG_ASSERT(config.Validate(0));

    switch (config.type) {
        case rk_DiskType::Local: return rk_OpenLocalDisk(config.url);
        case rk_DiskType::SFTP: return rk_OpenSftpDisk(config.ssh);
        case rk_DiskType::S3: return rk_OpenS3Disk(config.s3);
    }

    RG_UNREACHABLE();
}

}
