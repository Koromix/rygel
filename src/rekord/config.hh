// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "src/core/libcc/libcc.hh"
#include "src/core/libnet/s3.hh"
#include "src/core/libnet/ssh.hh"

namespace RG {

enum class rk_DiskType {
    Local,
    S3,
    SFTP
};

struct rk_Config {
    const char *repository = nullptr;
    const char *password = nullptr;

    rk_DiskType type = rk_DiskType::Local;
    s3_Config s3;
    ssh_Config ssh;

    int threads = -1;

    BlockAllocator str_alloc;

    bool Complete(bool require_password);
    bool Validate(bool require_password) const;
};

bool rk_DecodeURL(Span<const char> url, rk_Config *out_config);

bool rk_LoadConfig(StreamReader *st, rk_Config *out_config);
bool rk_LoadConfig(const char *filename, rk_Config *out_config);

}
