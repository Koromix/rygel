// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "instance.hh"
#include "../../core/libnet/libnet.hh"
#include "../../core/libsqlite/libsqlite.hh"

namespace RG {

extern const int DomainVersion;
extern const int MaxInstancesPerDomain;

struct DomainConfig {
    const char *config_filename = nullptr;
    const char *database_filename = nullptr;
    const char *database_directory = nullptr;
    const char *instances_directory = nullptr;
    const char *tmp_directory = nullptr;
    const char *archive_directory = nullptr;
    const char *snapshot_directory = nullptr;

    uint8_t archive_key[32] = {}; // crypto_box_curve25519xsalsa20poly1305_PUBLICKEYBYTES
    bool enable_archives = false;
    bool sync_full = false;
    int snapshot_hour = 0;
    TimeMode snapshot_zone = TimeMode::Local;

    // XXX: Restore http_Config designated initializers when MSVC ICE is fixed
    // https://developercommunity.visualstudio.com/content/problem/1238876/fatal-error-c1001-ice-with-ehsc.html
    http_Config http;
    int max_age = 900;
    const char *require_host = nullptr;

    smtp_Config smtp;
    sms_Config sms;

    bool Validate() const;

    BlockAllocator str_alloc;
};

const char *MakeInstanceFileName(const char *directory, const char *key, Allocator *alloc);

bool LoadConfig(StreamReader *st, DomainConfig *out_config);
bool LoadConfig(const char *filename, DomainConfig *out_config);

class DomainHolder {
    mutable std::shared_mutex mutex;

    HeapArray<InstanceHolder *> instances;
    HashTable<Span<const char>, InstanceHolder *> instances_map;

    HeapArray<sq_Database *> databases;

public:
    sq_Database db;

    DomainConfig config = {};

    ~DomainHolder() { Close(); }

    bool Open(const char *filename);
    void Close();

    bool SyncAll(bool thorough = false);
    bool SyncInstance(const char *key);

    bool Checkpoint();

    Span<InstanceHolder *> LockInstances();
    void UnlockInstances();
    Size CountInstances() const;

    InstanceHolder *Ref(Span<const char> key);

private:
    bool Sync(const char *key, bool thorough);
};

bool MigrateDomain(sq_Database *db, const char *instances_directory);
bool MigrateDomain(const DomainConfig &config);

}
