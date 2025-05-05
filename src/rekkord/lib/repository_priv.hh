// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "src/core/base/base.hh"
#include "disk.hh"

namespace RG {

#pragma pack(push, 1)
struct SnapshotHeader1 {
    char channel[512];
    int64_t time; // Little Endian
    int64_t size; // Little Endian
    int64_t storage; // Little Endian
};
#pragma pack(pop)
static_assert(RG_SIZE(SnapshotHeader1) == 536);

#pragma pack(push, 1)
struct SnapshotHeader2 {
    int64_t time; // Little Endian
    int64_t size; // Little Endian
    int64_t storage; // Little Endian
    char channel[512];
};
#pragma pack(pop)
static_assert(RG_SIZE(SnapshotHeader2) == 536);

#pragma pack(push, 1)
struct DirectoryHeader {
    int64_t size; // Little Endian
    int64_t entries; // Little Endian
};
#pragma pack(pop)
static_assert(RG_SIZE(DirectoryHeader) == 16);

#pragma pack(push, 1)
struct RawFile {
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

    rk_Hash hash;
    int16_t flags; // Little Endian
    int16_t kind; // Little Endian
    uint16_t name_len; // Little Endian
    uint16_t extended_len; // Little Endian
    int64_t mtime; // Little Endian
    int64_t ctime; // Little Endian
    int64_t btime; // Little Endian
    uint32_t uid; // Little Endian
    uint32_t gid; // Little Endian
    uint32_t mode; // Little Endian
    int64_t size; // Little Endian

    inline Size GetSize() const { return RG_SIZE(RawFile) + LittleEndian(name_len) + LittleEndian(extended_len); }

    inline Span<char> GetName() { return MakeSpan((char *)this + RG_SIZE(RawFile), LittleEndian(name_len)); }
    inline Span<const char> GetName() const { return MakeSpan((const char *)this + RG_SIZE(RawFile), LittleEndian(name_len)); }

    inline Span<uint8_t> GetExtended() { return MakeSpan((uint8_t *)this + RG_SIZE(RawFile) + LittleEndian(name_len), LittleEndian(extended_len)); }
    inline Span<const uint8_t> GetExtended() const { return MakeSpan((const uint8_t *)this + RG_SIZE(RawFile) + LittleEndian(name_len), LittleEndian(extended_len)); }
};
#pragma pack(pop)
static_assert(RG_SIZE(RawFile) == 84);

#pragma pack(push, 1)
struct RawChunk {
    int64_t offset; // Little Endian
    int32_t len;    // Little Endian
    rk_Hash hash;
};
#pragma pack(pop)
static_assert(RG_SIZE(RawChunk) == 44);

}
