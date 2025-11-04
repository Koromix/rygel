// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"
#include "src/drd/libdrd/libdrd.hh"

namespace K {

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
    K_DELETE_COPY(StructureSetBuilder)

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
