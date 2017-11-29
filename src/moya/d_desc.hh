/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "kutil.hh"
#include "d_codes.hh"

struct GhmRootDesc {
    GhmRootCode ghm_root;
    const char *ghm_root_desc;

    char da[4];
    const char *da_desc;

    char ga[5];
    const char *ga_desc;
};

struct CatalogSet {
    HeapArray<GhmRootDesc> ghm_roots;

    Allocator str_alloc;
};

bool LoadGhmRootCatalog(const char *filename, Allocator *str_alloc,
                        HeapArray<GhmRootDesc> *out_catalog);
