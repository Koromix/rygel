// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libdrd/libdrd.hh"

struct StructureEntity {
    const char *path;
    UnitCode unit;
};

struct Structure {
    const char *name;
    HeapArray<StructureEntity> entities;
};

struct StructureSet {
    mco_DispenseMode dispense_mode;
    HeapArray<Structure> structures;

    BlockAllocator str_alloc {Kibibytes(16)};
};

class StructureSetBuilder {
    StructureSet set;

    HashSet<const char *> structures_set;
    HashMap<UnitCode, Size> unit_reference_counts;

public:
    bool LoadIni(StreamReader &st);
    bool LoadFiles(Span<const char *const> filenames);

    void Finish(StructureSet *out_set);
};

bool LoadStructureSet(Span<const char *const> filenames, StructureSet *out_set);
