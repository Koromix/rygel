// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#ifndef _WIN32
    #include <time.h>
#endif

#include "../libcc/libcc.hh"

// Keep in sync with version in packer.cc
struct asset_Asset {
    const char *name;
    CompressionType compression_type;
    Span<const uint8_t> data;

    const char *source_map;
};

enum class asset_LoadStatus {
    Unchanged,
    Loaded,
    Error
};

struct asset_AssetSet {
    HeapArray<asset_Asset> assets;
    LinkedAllocator alloc;

#ifdef _WIN32
    uint8_t last_time[8] = {}; // FILETIME
#else
    struct timespec last_time = {};
#endif

    asset_LoadStatus LoadFromLibrary(const char *filename, const char *var_name = "packer_assets");
};
