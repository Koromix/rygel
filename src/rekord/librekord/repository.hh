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

class rk_Disk;

struct rk_PutSettings {
    const char *name = nullptr;
    bool follow_symlinks = false;
    bool raw = false;
};

struct rk_GetSettings {
    bool flat = false;
    bool force = false;
    bool chown = false;
};

struct rk_TreeSettings {
    int max_depth = -1;
};

struct rk_SnapshotInfo {
    rk_ID id;

    const char *name; // Can be NULL
    int64_t time;
    int64_t len;
    int64_t stored;
};

enum class rk_ObjectType {
    File,
    Directory,
    Link,
    Unknown
};
static const char *const rk_ObjectTypeNames[] = {
    "File",
    "Directory",
    "Link",
    "Unknown"
};

struct rk_ObjectInfo {
    rk_ID id;

    int depth;
    rk_ObjectType type;
    const char *basename;

    int64_t mtime;
    int64_t btime;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    int64_t size;

    union {
        Size children; // for directories
        bool readable; // for files
        const char *target; // for symbolic links
    } u;
};

// Snapshot commands
bool rk_Put(rk_Disk *disk, const rk_PutSettings &settings, Span<const char *const> filenames,
            rk_ID *out_id, int64_t *out_len = nullptr, int64_t *out_written = nullptr);
bool rk_Get(rk_Disk *disk, const rk_ID &id, const rk_GetSettings &settings,
            const char *dest_path, int64_t *out_len = nullptr);

// Exploration commands
bool rk_List(rk_Disk *disk, Allocator *alloc, HeapArray<rk_SnapshotInfo> *out_snapshots);
bool rk_Tree(rk_Disk *disk, const rk_ID &id, const rk_TreeSettings &settings, Allocator *alloc,
             HeapArray<rk_ObjectInfo> *out_objects);

}
