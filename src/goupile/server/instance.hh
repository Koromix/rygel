// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../core/libwrap/sqlite.hh"
#include "../../web/libhttp/libhttp.hh"

namespace RG {

extern const int InstanceVersion;

enum SyncMode {
    Offline,
    Mirror
};
static const char *const SyncModeNames[] = {
    "Offline",
    "Mirror"
};

class InstanceData {
public:
    const char *key = nullptr;
    const char *filename = nullptr;
    sq_Database db;

    struct {
        const char *base_url = nullptr;
        const char *app_key = nullptr;
        const char *app_name = nullptr;

        bool use_offline = false;
        int max_file_size = (int)Megabytes(8);
        SyncMode sync_mode = SyncMode::Offline;
    } config;

    HeapArray<AssetInfo> assets;
    HashTable<const char *, const AssetInfo *> assets_map;
    char etag[33];

    BlockAllocator str_alloc;
    BlockAllocator assets_alloc;

    bool Open(const char *key, const char *filename);
    bool Validate();

    // Can be restarted (for debug builds)
    void InitAssets();

    void Close();

private:
    Span<const uint8_t> PatchVariables(const AssetInfo &asset);
};

bool MigrateInstance(sq_Database *db);
bool MigrateInstance(const char *filename);

}
