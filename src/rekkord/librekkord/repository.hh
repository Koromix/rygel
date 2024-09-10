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

namespace RG {

class rk_Disk;

struct rk_PutSettings {
    const char *name = nullptr;
    bool follow_symlinks = false;
    bool raw = false;
};

struct rk_GetSettings {
    bool flat = false;
    bool force = false;
    bool unlink = false;
    bool chown = false;
    bool verbose = false;
    bool fake = false;
};

struct rk_ListSettings {
    int max_depth = 0;
};

struct rk_SnapshotInfo {
    rk_Hash hash;

    const char *name; // Can be NULL
    int64_t time;
    int64_t len;
    int64_t stored;
};

enum class rk_ObjectType {
    Snapshot,
    File,
    Directory,
    Link,
    Unknown
};
static const char *const rk_ObjectTypeNames[] = {
    "Snapshot",
    "File",
    "Directory",
    "Link",
    "Unknown"
};

struct rk_ObjectInfo {
    rk_Hash hash;

    int depth;
    rk_ObjectType type;
    const char *name; // Can be NULL for snapshots

    int64_t mtime;
    int64_t btime;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    int64_t size;
    bool readable;

    int64_t stored; // for snapshots
    Size children; // for snapshots and directories
};

class rk_FileReader {
public:
    virtual ~rk_FileReader() {}
    virtual Size Read(int64_t offset, Span<uint8_t> out_buf) = 0;
};

// Snapshot commands
bool rk_Put(rk_Disk *disk, const rk_PutSettings &settings, Span<const char *const> filenames,
            rk_Hash *out_hash, int64_t *out_len = nullptr, int64_t *out_written = nullptr);
bool rk_Get(rk_Disk *disk, const rk_Hash &hash, const rk_GetSettings &settings,
            const char *dest_path, int64_t *out_len = nullptr);

// Exploration commands
bool rk_Snapshots(rk_Disk *disk, Allocator *alloc, HeapArray<rk_SnapshotInfo> *out_snapshots);
bool rk_List(rk_Disk *disk, const rk_Hash &hash, const rk_ListSettings &settings,
             Allocator *alloc, HeapArray<rk_ObjectInfo> *out_objects);

// Symbolic links
const char *rk_ReadLink(rk_Disk *disk, const rk_Hash &hash, Allocator *alloc);

// Files
std::unique_ptr<rk_FileReader> rk_OpenFile(rk_Disk *disk, const rk_Hash &hash);

}
