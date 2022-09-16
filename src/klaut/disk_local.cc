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

#include "src/core/libcc/libcc.hh"
#include "disk.hh"

namespace RG {

class LocalDisk: public kt_Disk {
    const char *dirname;

    kt_OpenMode mode;
    const char *key;

public:
    LocalDisk(const char *dirname, kt_OpenMode mode, const char *key)
        : dirname(dirname), mode(mode), key(key) {}
    ~LocalDisk() override;

    bool ListTags(Allocator *alloc, HeapArray<const char *> *out_tags) override;

    bool ListChunks(const char *type, HeapArray<kt_ObjectID> *out_ids) override;
    bool ReadChunk(const kt_ObjectID &id, HeapArray<uint8_t> *out_buf) override;
    bool WriteChunk(const kt_ObjectID &id, Span<const uint8_t> buf) override;
};

LocalDisk::~LocalDisk()
{
}

bool LocalDisk::ListTags(Allocator *alloc, HeapArray<const char *> *out_tags)
{
    RG_UNREACHABLE();
}

bool LocalDisk::ListChunks(const char *type, HeapArray<kt_ObjectID> *out_ids)
{
    RG_UNREACHABLE();
}

bool LocalDisk::ReadChunk(const kt_ObjectID &id, HeapArray<uint8_t> *out_buf)
{
    RG_UNREACHABLE();
}

bool LocalDisk::WriteChunk(const kt_ObjectID &id, Span<const uint8_t> buf)
{
    RG_UNREACHABLE();
}

kt_Disk *kt_OpenLocalDisk(const char *dirname, kt_OpenMode mode, const char *key)
{
    kt_Disk *disk = new LocalDisk(dirname, mode, key);
    return disk;
}

}
