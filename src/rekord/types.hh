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
RG_STATIC_ASSERT(RG_SIZE(rk_ID) == 32);

#pragma pack(push, 1)
struct rk_SnapshotHeader {
    char name[512];
    int64_t time; // Little Endian
    int64_t len; // Little Endian
    int64_t stored; // Little Endian
};
#pragma pack(pop)
RG_STATIC_ASSERT(RG_SIZE(rk_SnapshotHeader) == 536);

#pragma pack(push, 1)
struct rk_FileEntry {
    enum class Kind {
        Directory = 0,
        File = 1,
        Link = 2,
        Unknown = -1
    };

    rk_ID id;
    int8_t stated;
    int8_t readable;
    int8_t kind; // Kind
    int64_t mtime; // Little Endian
    int64_t btime; // Little Endian
    uint32_t uid; // Little Endian
    uint32_t gid; // Little Endian
    uint32_t mode; // Little Endian
    int64_t size; // Little Endian
    char name[];
};
#pragma pack(pop)
RG_STATIC_ASSERT(RG_SIZE(rk_FileEntry) == 71);

#pragma pack(push, 1)
struct rk_ChunkEntry {
    int64_t offset; // Little Endian
    int32_t len;    // Little Endian
    rk_ID id;
};
#pragma pack(pop)
RG_STATIC_ASSERT(RG_SIZE(rk_ChunkEntry) == 44);

bool rk_ParseID(const char *str, rk_ID *out_id);

}
