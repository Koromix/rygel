// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "instance.hh"
#include "../../core/libwrap/sqlite.hh"
#include "../../web/libhttp/libhttp.hh"

namespace RG {

extern const int DomainVersion;
extern const int MaxInstancesPerDomain;

struct DomainConfig {
    const char *database_filename = nullptr;
    const char *instances_directory = nullptr;
    const char *temp_directory = nullptr;
    const char *backup_directory = nullptr;

    const char *demo_user = nullptr;

    // XXX: Restore http_Config designated initializers when MSVC ICE is fixed
    // https://developercommunity.visualstudio.com/content/problem/1238876/fatal-error-c1001-ice-with-ehsc.html
    http_Config http;
    int max_age = 900;

    bool Validate() const;

    const char *GetInstanceFileName(const char *key, Allocator *alloc) const;

    BlockAllocator str_alloc;
};

bool LoadConfig(StreamReader *st, DomainConfig *out_config);
bool LoadConfig(const char *filename, DomainConfig *out_config);

class DomainHolder {
    bool synced = true;

    mutable std::shared_mutex mutex;
    HeapArray<InstanceHolder *> instances;
    HashTable<Span<const char>, InstanceHolder *> instances_map;

public:
    DomainConfig config = {};
    sq_Database db;

    ~DomainHolder() { Close(); }

    bool Open(const char *filename);
    void Close();

    bool IsSynced() const { return synced; }
    bool Sync();
    bool Checkpoint();

    Size CountInstances() const
    {
        std::shared_lock<std::shared_mutex> lock_shr(mutex);
        return instances_map.count;
    }

    InstanceHolder *Ref(Span<const char> key, bool *out_reload = nullptr);
};

bool MigrateDomain(sq_Database *db, const char *instances_directory);
bool MigrateDomain(const DomainConfig &config);

}
