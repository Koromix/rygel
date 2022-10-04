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

class kt_Disk;

struct kt_PutSettings {
    const char *name = nullptr;
    bool follow_symlinks = false;
    bool raw = false;
};

struct kt_SnapshotInfo {
    kt_ID id;

    const char *name; // Can be NULL
    int64_t time;
    int64_t len;
    int64_t stored;
};

struct kt_GetSettings {
    bool flat = false;
};

bool kt_Put(kt_Disk *disk, const kt_PutSettings &settings, Span<const char *const> filenames,
            kt_ID *out_id, int64_t *out_len = nullptr, int64_t *out_written = nullptr);

bool kt_List(kt_Disk *disk, Allocator *str_alloc, HeapArray<kt_SnapshotInfo> *out_snapshots);
bool kt_Get(kt_Disk *disk, const kt_ID &id, const kt_GetSettings &settings, const char *dest_path, int64_t *out_len = nullptr);

}
