// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#ifndef _WIN32
    #include <time.h>
#endif

#include "../libcc/libcc.hh"

// Keep in sync with version in packer.cc
struct pack_Asset {
    const char *name;
    CompressionType compression_type;
    Span<const uint8_t> data;

    const char *source_map;
};

enum class pack_LoadStatus {
    Unchanged,
    Loaded,
    Error
};

struct pack_AssetSet {
    HeapArray<pack_Asset> assets;
    LinkedAllocator alloc;

    int64_t last_time = -1;

    pack_LoadStatus LoadFromLibrary(const char *filename, const char *var_name = "pack_assets");
};
