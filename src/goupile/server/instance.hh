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
    Online,
    Mirror
};
static const char *const SyncModeNames[] = {
    "Offline",
    "Online",
    "Mirror"
};

class InstanceHolder {
    // Managed by DomainHolder
    std::atomic_int refcount {0};
    bool unload = false;
    std::atomic_bool reload {false};

public:
    int64_t unique = -1;

    Span<const char> key = {};
    const char *filename = nullptr;
    sq_Database db;

    struct {
        const char *title = nullptr;
        bool use_offline = false;
        SyncMode sync_mode = SyncMode::Offline;
        int max_file_size = (int)Megabytes(8);
        const char *backup_key = nullptr;
    } config;

    BlockAllocator str_alloc;

    ~InstanceHolder() { Close(); }

    bool Open(const char *key, const char *filename, bool sync_full);
    bool Validate();
    void Close();

    bool Checkpoint();

    void Reload() { reload = true; }
    void Unref() { refcount--; }

    RG_HASHTABLE_HANDLER(InstanceHolder, key);

    friend class DomainHolder;
};

bool MigrateInstance(sq_Database *db);
bool MigrateInstance(const char *filename);

}
