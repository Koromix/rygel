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

public:
    kt_Disk(kt_DiskMode mode) : mode(mode) {}
    virtual ~kt_Disk() = default;

    kt_DiskMode GetMode() const { return mode; }

    virtual bool ListTags(Allocator *alloc, HeapArray<const char *> *out_tags) = 0;

    virtual bool ListChunks(const char *type, HeapArray<kt_ID> *out_ids) = 0;
    virtual bool ReadChunk(const kt_ID &id, HeapArray<uint8_t> *out_buf) = 0;
    virtual Size WriteChunk(const kt_ID &id, Span<const uint8_t> chunk) = 0;
};

bool kt_CreateLocalDisk(const char *path, const char *password);
kt_Disk *kt_OpenLocalDisk(const char *path, const char *password);

}
