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
#include "vendor/libssh/include/libssh/libssh.h"
#include "vendor/libssh/include/libssh/sftp.h"

namespace RG {

struct ssh_Config {
    const char *host = nullptr;
    const char *username = nullptr;
    const char *path = nullptr;

    bool known_hosts = true;
    const char *host_hash = nullptr;

    const char *password = nullptr;
    const char *keyfile = nullptr;

    BlockAllocator str_alloc;

    bool SetProperty(Span<const char> key, Span<const char> value, Span<const char> root_directory = {});

    bool Complete();
    bool Validate() const;
};

bool ssh_DecodeURL(Span<const char> url, ssh_Config *out_config);

}
