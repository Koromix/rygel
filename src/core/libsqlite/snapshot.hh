// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "src/core/libcc/libcc.hh"

namespace RG {

struct sq_SnapshotGeneration {
    const char *base_filename;
    Size frame_idx;
    Size frames;
    int64_t ctime;
    int64_t mtime;
};

struct sq_SnapshotFrame {
    int64_t mtime;
    Size generation_idx;
    uint8_t sha256[32];
};

struct sq_SnapshotInfo {
    const char *orig_filename;
    int64_t mtime;

    HeapArray<sq_SnapshotGeneration> generations;
    HeapArray<sq_SnapshotFrame> frames;

    Size FindFrame(int64_t mtime) const;
};

struct sq_SnapshotSet {
    HeapArray<sq_SnapshotInfo> snapshots;

    BlockAllocator str_alloc;
};

bool sq_CollectSnapshots(Span<const char *> filenames, sq_SnapshotSet *out_set);
bool sq_RestoreSnapshot(const sq_SnapshotInfo &snapshot, Size frame_idx, const char *dest_filename, bool overwrite);

}
