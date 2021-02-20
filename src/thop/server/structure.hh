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
// along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../drd/libdrd/libdrd.hh"

namespace RG {

struct StructureEntity {
    const char *path;
    drd_UnitCode unit;
};

struct Structure {
    const char *name;
    HeapArray<StructureEntity> entities;
};

struct StructureSet {
    HeapArray<Structure> structures;

    BlockAllocator str_alloc;
};

class StructureSetBuilder {
    RG_DELETE_COPY(StructureSetBuilder)

    StructureSet set;

    HashSet<const char *> structures_set;
    HashMap<drd_UnitCode, Size> unit_reference_counts;

public:
    StructureSetBuilder() = default;

    bool LoadIni(StreamReader *st);
    bool LoadFiles(Span<const char *const> filenames);

    void Finish(StructureSet *out_set);
};

bool LoadStructureSet(Span<const char *const> filenames, StructureSet *out_set);

}
