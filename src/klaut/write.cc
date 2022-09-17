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
#include "write.hh"

namespace RG {

bool kt_CreateSnapshot(kt_Disk *disk, const char *dir_id, kt_Hash *out_id)
{
    RG_UNREACHABLE();
}

bool kt_CreateDirectory(kt_Disk *disk, Span<const kt_EntryInfo> entries, kt_Hash *out_id)
{
    RG_UNREACHABLE();
}

bool kt_BackupFile(kt_Disk *disk, const char *src_filename, kt_Hash *out_id)
{
    RG_UNREACHABLE();
}

}
