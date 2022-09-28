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
static const char *const kt_DiskModeNames[] = {
    "WriteOnly",
    "ReadWrite"
};

class kt_Disk {
protected:
    const char *url = nullptr;

    kt_DiskMode mode;
    uint8_t pkey[32];
    uint8_t skey[32];

    BlockAllocator str_alloc;

    kt_Disk() = default;

public:
    virtual ~kt_Disk() = default;

    const char *GetURL() const { return url; }
    Span<const uint8_t> GetSalt() const { return pkey; }
    kt_DiskMode GetMode() const { return mode; }

    bool ReadObject(const kt_ID &id, int8_t *out_type, HeapArray<uint8_t> *out_obj);
    Size WriteObject(const kt_ID &id, int8_t type, Span<const uint8_t> obj);

protected:
    virtual bool ReadRaw(const char *path, HeapArray<uint8_t> *out_blob) = 0;
    virtual Size WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func) = 0;
};

bool kt_CreateLocalDisk(const char *path, const char *full_pwd, const char *write_pwd);
kt_Disk *kt_OpenLocalDisk(const char *path, const char *pwd);

}
