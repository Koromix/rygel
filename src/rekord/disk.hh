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
#include "src/core/libnet/s3.hh"
#include "src/core/libsqlite/libsqlite.hh"

namespace RG {

enum class rk_DiskMode {
    WriteOnly,
    ReadWrite
};
static const char *const rk_DiskModeNames[] = {
    "WriteOnly",
    "ReadWrite"
};

enum class rk_ObjectType: int8_t {
    Chunk = 0,
    File = 1,
    Directory1 = 2,
    Directory2 = 5,
    Snapshot1 = 3,
    Snapshot2 = 6,
    Link = 4
};
static const char *const rk_ObjectTypeNames[] = {
    "Chunk",
    "File",
    "Directory1",
    "Snapshot1",
    "Link",
    "Directory2",
    "Snapshot2"
};

class rk_Disk {
protected:
    const char *url = nullptr;

    rk_DiskMode mode;
    uint8_t pkey[32];
    uint8_t skey[32];

    sq_Database cache_db;

    BlockAllocator str_alloc;

    rk_Disk() = default;

public:
    virtual ~rk_Disk() = default;

    bool InitCache();
    sq_Database *GetCache() { return &cache_db; }

    const char *GetURL() const { return url; }
    Span<const uint8_t> GetSalt() const { return pkey; }
    rk_DiskMode GetMode() const { return mode; }

    bool ReadObject(const rk_ID &id, rk_ObjectType *out_type, HeapArray<uint8_t> *out_obj);
    Size WriteObject(const rk_ID &id, rk_ObjectType type, Span<const uint8_t> obj);
    bool HasObject(const rk_ID &id);

    Size WriteTag(const rk_ID &id);
    bool ListTags(HeapArray<rk_ID> *out_ids);

protected:
    virtual bool ReadRaw(const char *path, HeapArray<uint8_t> *out_blob) = 0;
    virtual Size WriteRaw(const char *path, Size total_len, FunctionRef<bool(FunctionRef<bool(Span<const uint8_t>)>)> func) = 0;
    Size WriteRaw(const char *path, Span<const uint8_t> buf)
        { return WriteRaw(path, buf.len, [&](FunctionRef<bool(Span<const uint8_t>)> func) { return func(buf); }); }
    virtual bool ListRaw(const char *path, Allocator *alloc, HeapArray<const char *> *out_paths) = 0;
    virtual bool TestRaw(const char *path) = 0;
};

rk_Disk *rk_CreateLocalDisk(const char *path, const char *full_pwd, const char *write_pwd);
rk_Disk *rk_OpenLocalDisk(const char *path, const char *pwd);

rk_Disk *rk_CreateS3Disk(const s3_Config &config, const char *full_pwd, const char *write_pwd);
rk_Disk *rk_OpenS3Disk(const s3_Config &config, const char *pwd);

}
