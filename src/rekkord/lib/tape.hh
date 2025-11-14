// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "repository.hh"

namespace K {

class rk_Repository;

static const Size rk_MaxSnapshotChannelLength = 256;

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

struct rk_SaveSettings {
    bool skip = true;
    bool follow = false;
    bool noatime = false;
    bool atime = false;
    bool xattrs = false;
};

struct rk_SaveInfo {
    rk_ObjectID oid;
    int64_t time;
    int64_t size;
    int64_t stored;
    int64_t added;
    int64_t entries;
};

struct rk_SnapshotInfo {
    const char *tag;
    rk_ObjectID oid;

    const char *channel;
    int64_t time;
    int64_t size;
    int64_t stored;
    int64_t added;
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

enum class rk_ObjectFlag {
    Readable = 1 << 0,
    AccessTime = 1 << 1
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
    unsigned int flags;

    int64_t entries; // for snapshots and directories
    int64_t stored; // for snapshots
    int64_t added; // for snapshots

    Size children; // for snapshots and directories
};

class rk_FileHandle {
public:
    virtual ~rk_FileHandle() {}
    virtual Size Read(int64_t offset, Span<uint8_t> out_buf) = 0;
};

bool rk_Restore(rk_Repository *repo, const rk_ObjectID &oid, const rk_RestoreSettings &settings,
                const char *dest_path, int64_t *out_size = nullptr);

bool rk_ListSnapshots(rk_Repository *repo, Allocator *alloc, HeapArray<rk_SnapshotInfo> *out_snapshots);

void rk_ListChannels(Span<const rk_SnapshotInfo> snapshots, Allocator *alloc, HeapArray<rk_ChannelInfo> *out_channels);
bool rk_ListChannels(rk_Repository *repo, Allocator *alloc, HeapArray<rk_ChannelInfo> *out_channels);

bool rk_LocateObject(rk_Repository *repo, Span<const char> identifier, rk_ObjectID *out_pid);

bool rk_ListChildren(rk_Repository *repo, const rk_ObjectID &oid, const rk_ListSettings &settings,
                     Allocator *alloc, HeapArray<rk_ObjectInfo> *out_objects);

bool rk_CheckSnapshots(rk_Repository *repo, Span<const rk_SnapshotInfo> snapshots, HeapArray<Size> *out_errors = nullptr);

const char *rk_ReadLink(rk_Repository *repo, const rk_ObjectID &oid, Allocator *alloc);
std::unique_ptr<rk_FileHandle> rk_OpenFile(rk_Repository *repo, const rk_ObjectID &oid);

bool rk_Save(rk_Repository *repo, const char *channel, Span<const char *const> filenames,
             const rk_SaveSettings &settings = {}, rk_SaveInfo *out_info = nullptr);

}
