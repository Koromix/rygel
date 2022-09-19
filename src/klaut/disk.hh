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

class kt_Disk {
public:
    virtual ~kt_Disk() = default;

    virtual bool ListTags(Allocator *alloc, HeapArray<const char *> *out_tags) = 0;

    virtual bool ListChunks(const char *type, HeapArray<kt_Hash> *out_ids) = 0;
    virtual bool ReadChunk(const kt_Hash &id, HeapArray<uint8_t> *out_buf) = 0;
    virtual bool WriteChunk(const kt_Hash &id, Span<const uint8_t> chunk) = 0;
};

kt_Disk *kt_OpenLocalDisk(const char *path, const char *encrypt_key);

}
