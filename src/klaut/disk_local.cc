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

static const int MaxPathSize = 4096 - 128;

class LocalDisk: public kt_Disk {
    LocalArray<char, MaxPathSize + 128> directory;

public:
    LocalDisk(Span<const char> directory);
    ~LocalDisk() override;

    bool ListTags(Allocator *alloc, HeapArray<const char *> *out_tags) override;

    bool ListChunks(const char *type, HeapArray<kt_Hash> *out_ids) override;
    bool ReadChunk(const kt_Hash &id, HeapArray<uint8_t> *out_buf) override;
    bool WriteChunk(const kt_Hash &id, Span<const uint8_t> buf) override;
};

LocalDisk::LocalDisk(Span<const char> directory)
{
    RG_ASSERT(directory.len <= MaxPathSize);

    this->directory.Append(directory);
    this->directory.data[this->directory.len] = 0;
}

LocalDisk::~LocalDisk()
{
}

bool LocalDisk::ListTags(Allocator *alloc, HeapArray<const char *> *out_tags)
{
    RG_UNREACHABLE();
}

bool LocalDisk::ListChunks(const char *type, HeapArray<kt_Hash> *out_ids)
{
    RG_UNREACHABLE();
}

bool LocalDisk::ReadChunk(const kt_Hash &id, HeapArray<uint8_t> *out_buf)
{
    RG_UNREACHABLE();
}

bool LocalDisk::WriteChunk(const kt_Hash &id, Span<const uint8_t> buf)
{
    LocalArray<char, 4096> path;

    path.len += Fmt(path.TakeAvailable(), "%1%/%2", directory, FmtHex(id.hash[0]).Pad0(-2)).len;
    if (!MakeDirectory(path.data, false))
        return false;
    path.len += Fmt(path.TakeAvailable(), "%/%1", id).len;
    if (!WriteFile(buf, path.data))
        return false;

    return true;
}

kt_Disk *kt_OpenLocalDisk(const char *path)
{
    Span<const char> directory = TrimStrRight(path, RG_PATH_SEPARATORS);

    if (!TestFile(path, FileType::Directory)) {
        LogError("Directory '%1' does not exist", directory);
        return nullptr;
    }
    if (directory.len > MaxPathSize) {
        LogError("Directory path '%1' is too long", directory);
        return nullptr;
    }

    kt_Disk *disk = new LocalDisk(directory);
    return disk;
}

}
