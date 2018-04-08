// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "mco_common.hh"

struct mco_GhmRootDesc {
    mco_GhmRootCode ghm_root;
    const char *ghm_root_desc;

    char da[4];
    const char *da_desc;

    char ga[5];
    const char *ga_desc;

    HASH_TABLE_HANDLER(mco_GhmRootDesc, ghm_root);
};

struct mco_CatalogSet {
    HeapArray<mco_GhmRootDesc> ghm_roots;

    HashTable<mco_GhmRootCode, mco_GhmRootDesc> ghm_roots_map;

    LinkedAllocator str_alloc;
};

bool mco_LoadGhmRootCatalog(const char *filename, Allocator *str_alloc,
                            HeapArray<mco_GhmRootDesc> *out_catalog,
                            HashTable<mco_GhmRootCode, mco_GhmRootDesc> *out_map = nullptr);
