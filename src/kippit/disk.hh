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

enum class kt_ObjectType: int8_t {
    Chunk = 0,
    File = 1,
    Directory = 2,
    Snapshot = 3,
    Link = 4
};
static const char *const kt_ObjectTypeNames[] = {
    "Chunk",
    "File",
    "Directory",
    "Snapshot",
    "Link"
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

    bool ReadObject(const kt_ID &id, kt_ObjectType *out_type, HeapArray<uint8_t> *out_obj);
    Size WriteObject(const kt_ID &id, kt_ObjectType type, Span<const uint8_t> obj);

    Size WriteTag(const kt_ID &id);
    bool ListTags(HeapArray<kt_ID> *out_ids);

protected:
    virtual bool ReadRaw(const char *path, HeapArray<uint8_t> *out_blob) = 0;
    virtual Size WriteRaw(const char *path, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func) = 0;
    virtual bool ListRaw(const char *path, Allocator *alloc, HeapArray<const char *> *out_paths) = 0;
};

bool kt_CreateLocalDisk(const char *path, const char *full_pwd, const char *write_pwd);
kt_Disk *kt_OpenLocalDisk(const char *path, const char *pwd);

}
