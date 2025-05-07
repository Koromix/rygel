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
#include "src/drd/libdrd/libdrd.hh"

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
