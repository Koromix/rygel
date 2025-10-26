// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "tape.hh"

namespace K {

enum class BlobType {
    Chunk = 0,
    File = 1,
    Directory1 = 2,
    Snapshot1 = 3,
    Link = 4,
    Snapshot2 = 5,
    Directory2 = 6,
    Snapshot3 = 7,
    Directory3 = 8,
    Snapshot4 = 9,
    Directory = 10,
    Snapshot5 = 11,
    Snapshot = 12
};
static const char *const BlobTypeNames[] = {
    "Chunk",
    "File",
    "Directory1",
    "Snapshot1",
    "Link",
    "Snapshot2",
    "Directory2",
    "Snapshot3",
    "Directory3",
    "Snapshot4",
    "Directory",
    "Snapshot5",
    "Snapshot"
};

#pragma pack(push, 1)
struct SnapshotHeader1 {
    char channel[512];
    int64_t time; // Little Endian
    int64_t size; // Little Endian
    int64_t stored; // Little Endian
};
#pragma pack(pop)
static_assert(K_SIZE(SnapshotHeader1) == 536);

#pragma pack(push, 1)
struct SnapshotHeader2 {
    int64_t time; // Little Endian
    int64_t size; // Little Endian
    int64_t stored; // Little Endian
    char channel[512];
};
#pragma pack(pop)
static_assert(K_SIZE(SnapshotHeader2) == 536);

#pragma pack(push, 1)
struct SnapshotHeader3 {
    int64_t time; // Little Endian
    int64_t size; // Little Endian
    int64_t stored; // Little Endian
    int64_t added; // Little Endian
    char channel[512];
};
#pragma pack(pop)
static_assert(K_SIZE(SnapshotHeader3) == 544);

#pragma pack(push, 1)
struct DirectoryHeader {
    int64_t size; // Little Endian
    int64_t entries; // Little Endian
};
#pragma pack(pop)
static_assert(K_SIZE(DirectoryHeader) == 16);

#pragma pack(push, 1)
struct RawEntry {
    enum class Flags {
        Stated = 1 << 0,
        Readable = 1 << 1,
        AccessTime = 1 << 2
    };

    enum class Kind {
        Directory = 0,
        File = 1,
        Link = 2,
        Unknown = -1
    };

    rk_Hash hash;
    uint8_t flags;
    int8_t kind;
    uint16_t name_len; // Little Endian
    uint16_t extended_len; // Little Endian
    int64_t mtime; // Little Endian
    int64_t ctime; // Little Endian
    int64_t atime; // Little Endian
    int64_t btime; // Little Endian
    uint32_t uid; // Little Endian
    uint32_t gid; // Little Endian
    uint32_t mode; // Little Endian
    int64_t size; // Little Endian

    inline Size GetSize() const { return K_SIZE(RawEntry) + LittleEndian(name_len) + LittleEndian(extended_len); }

    inline Span<char> GetName() { return MakeSpan((char *)this + K_SIZE(RawEntry), LittleEndian(name_len)); }
    inline Span<const char> GetName() const { return MakeSpan((const char *)this + K_SIZE(RawEntry), LittleEndian(name_len)); }

    inline Span<uint8_t> GetExtended() { return MakeSpan((uint8_t *)this + K_SIZE(RawEntry) + LittleEndian(name_len), LittleEndian(extended_len)); }
    inline Span<const uint8_t> GetExtended() const { return MakeSpan((const uint8_t *)this + K_SIZE(RawEntry) + LittleEndian(name_len), LittleEndian(extended_len)); }
};
#pragma pack(pop)
static_assert(K_SIZE(RawEntry) == 90);

#pragma pack(push, 1)
struct RawChunk {
    int64_t offset; // Little Endian
    int32_t len;    // Little Endian
    rk_Hash hash;
};
#pragma pack(pop)
static_assert(K_SIZE(RawChunk) == 44);

}
