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
#include "types.hh"

namespace RG {

enum class kt_DiskMode {
    WriteOnly,
    ReadWrite
};

class kt_Disk {
    kt_DiskMode mode;
    uint8_t pkey[32];
    uint8_t skey[32];

public:
    kt_Disk(kt_DiskMode mode, uint8_t skey[32], uint8_t pkey[32]);
    virtual ~kt_Disk() = default;

    kt_DiskMode GetMode() const { return mode; }

    bool ReadChunk(const kt_ID &id, HeapArray<uint8_t> *out_chunk);
    Size WriteChunk(const kt_ID &id, Span<const uint8_t> chunk);

protected:
    virtual bool ReadBlob(const char *path, HeapArray<uint8_t> *out_blob) = 0;
    virtual Size WriteBlob(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func) = 0;
};

bool kt_CreateLocalDisk(const char *path, const char *password);
kt_Disk *kt_OpenLocalDisk(const char *path, const char *password);

}
