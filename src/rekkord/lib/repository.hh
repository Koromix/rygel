// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

static const char *const rk_DefaultSnapshotChannel = "default";
static const Size rk_MaxSnapshotChannelLength = 256;

struct rk_SaveSettings {
    const char *channel = rk_DefaultSnapshotChannel;
    bool follow = false;
    bool noatime = false;
    bool atime = false;
    bool xattrs = false;
    bool raw = false;
};

struct rk_RestoreSettings {
    bool force = false;
    bool unlink = false;
    bool chown = false;
    bool xattrs = false;
    bool verbose = false;
    bool fake = false;
};

struct rk_ListSettings {
    bool recurse = false;
};

struct rk_SnapshotInfo {
    const char *tag;
    rk_ObjectID oid;

    const char *channel;
    int64_t time;
    int64_t size;
    int64_t storage;
};

struct rk_ChannelInfo {
    const char *name;

    rk_ObjectID oid;
    int64_t time;
    Size size;

    int count;
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
    rk_ObjectID oid;

    int depth;
    rk_ObjectType type;
    const char *name; // Can be NULL for snapshots

    int64_t mtime;
    int64_t ctime;
    int64_t atime;
    int64_t btime;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    int64_t size;
    bool readable;

    int64_t entries; // for snapshots and directories
    int64_t storage; // for snapshots

    Size children; // for snapshots and directories
};

class rk_FileHandle {
public:
    virtual ~rk_FileHandle() {}
    virtual Size Read(int64_t offset, Span<uint8_t> out_buf) = 0;
};

// Snapshot commands
bool rk_Save(rk_Disk *disk, const rk_SaveSettings &settings, Span<const char *const> filenames,
             rk_ObjectID *out_oid, int64_t *out_size = nullptr, int64_t *out_stored = nullptr);
bool rk_Restore(rk_Disk *disk, const rk_ObjectID &oid, const rk_RestoreSettings &settings,
                const char *dest_path, int64_t *out_size = nullptr);

// Snapshot exploration
bool rk_ListSnapshots(rk_Disk *disk, Allocator *alloc, HeapArray<rk_SnapshotInfo> *out_snapshots, HeapArray<rk_ChannelInfo> *out_channels);
static inline bool rk_ListSnapshots(rk_Disk *disk, Allocator *alloc, HeapArray<rk_SnapshotInfo> *out_snapshots)
    { return rk_ListSnapshots(disk, alloc, out_snapshots, nullptr); }
static inline bool rk_ListSnapshots(rk_Disk *disk, Allocator *alloc, HeapArray<rk_ChannelInfo> *out_channels)
    { return rk_ListSnapshots(disk, alloc, nullptr, out_channels); }

// List objects
bool rk_ListChildren(rk_Disk *disk, const rk_ObjectID &oid, const rk_ListSettings &settings,
                     Allocator *alloc, HeapArray<rk_ObjectInfo> *out_objects);
bool rk_LocateObject(rk_Disk *disk, Span<const char> identifier, rk_ObjectID *out_pid);

// Direct access
const char *rk_ReadLink(rk_Disk *disk, const rk_ObjectID &oid, Allocator *alloc);
std::unique_ptr<rk_FileHandle> rk_OpenFile(rk_Disk *disk, const rk_ObjectID &oid);

}
