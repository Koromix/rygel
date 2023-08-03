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

namespace RG {

struct rk_ID {
    uint8_t hash[32];
    operator FmtArg() const { return FmtSpan(hash, FmtType::BigHex, "").Pad0(-2); }
};
static_assert(RG_SIZE(rk_ID) == 32);

#pragma pack(push, 1)
struct rk_SnapshotHeader {
    char name[512];
    int64_t time; // Little Endian
    int64_t len; // Little Endian
    int64_t stored; // Little Endian
};
#pragma pack(pop)
static_assert(RG_SIZE(rk_SnapshotHeader) == 536);

#pragma pack(push, 1)
struct rk_FileEntry {
    enum class Flags {
        Stated = 1 << 0,
        Readable = 1 << 1
    };

    enum class Kind {
        Directory = 0,
        File = 1,
        Link = 2,
        Unknown = -1
    };

    rk_ID id;
    int16_t flags; // Little Endian
    int16_t kind; // Little Endian
    int16_t name_len; // Little Endian
    int16_t extended_len; // Little Endian
    int64_t mtime; // Little Endian
    int64_t btime; // Little Endian
    uint32_t uid; // Little Endian
    uint32_t gid; // Little Endian
    uint32_t mode; // Little Endian
    int64_t size; // Little Endian

    inline Size GetSize() { return RG_SIZE(rk_FileEntry) + name_len + extended_len; }

    inline Span<char> GetName() { return MakeSpan((char *)this + RG_SIZE(rk_FileEntry), name_len); }
    inline Span<const char> GetName() const { return MakeSpan((const char *)this + RG_SIZE(rk_FileEntry), name_len); }
};
#pragma pack(pop)
static_assert(RG_SIZE(rk_FileEntry) == 76);

#pragma pack(push, 1)
struct rk_ChunkEntry {
    int64_t offset; // Little Endian
    int32_t len;    // Little Endian
    rk_ID id;
};
#pragma pack(pop)
static_assert(RG_SIZE(rk_ChunkEntry) == 44);

bool rk_ParseID(const char *str, rk_ID *out_id);

}
