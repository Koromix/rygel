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

namespace RG {

struct kt_ObjectID {
    uint32_t fast; // FastCDC
    uint8_t slow[64]; // BLAKE2b
};

struct kt_SnapshotInfo {
    const char *name;

    int64_t ctime;

    kt_ObjectID id;
};

enum class kt_EntryType {
    Directory,
    BigFile,
    SmallFile
};

struct kt_EntryInfo {
    const char *name;

    int64_t mtime;
    kt_EntryType type;

    kt_ObjectID id;
};

}
