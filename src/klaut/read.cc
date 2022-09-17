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

#include "src/core/libcc/libcc.hh"
#include "disk.hh"
#include "read.hh"

namespace RG {

class kt_Disk;

bool kt_ListSnapshots(kt_Disk *disk, Allocator *alloc, HeapArray<kt_SnapshotInfo> *out_snapshots)
{
    RG_UNREACHABLE();
}

bool kt_ReadSnapshot(kt_Disk *disk, const kt_Hash &id, Allocator *alloc, kt_SnapshotInfo *out_snapshot)
{
    RG_UNREACHABLE();
}

bool kt_ListDirectory(kt_Disk *disk, const kt_Hash &id, Allocator *alloc, HeapArray<kt_EntryInfo> *out_entries)
{
    RG_UNREACHABLE();
}

bool kt_ExtractFile(kt_Disk *disk, const kt_Hash &id, const char *dest_filename)
{
    RG_UNREACHABLE();
}

}
