// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "src/core/base/base.hh"
#include "instance.hh"
#include "src/core/http/http.hh"
#include "src/core/request/sms.hh"
#include "src/core/request/smtp.hh"
#include "src/core/sqlite/sqlite.hh"

namespace RG {

extern const int DomainVersion;
extern const int MaxInstancesPerDomain;
extern const int DemoPruneDelay;

enum class PasswordComplexity {
    Easy,
    Moderate,
    Hard
};
static const char *const PasswordComplexityNames[] = {
    "Easy",
    "Moderate",
    "Hard"
};

struct DomainConfig {
    const char *config_filename = nullptr;
    const char *database_filename = nullptr;
    const char *database_directory = nullptr;
    const char *instances_directory = nullptr;
    const char *tmp_directory = nullptr;
    const char *archive_directory = nullptr;
    const char *snapshot_directory = nullptr;
    const char *view_directory = nullptr;
    const char *export_directory = nullptr;

    const char *title = nullptr;
    bool demo_mode = false;
    const char *default_lang = "fr";

    uint8_t archive_key[32] = {}; // crypto_box_curve25519xsalsa20poly1305_PUBLICKEYBYTES
    bool enable_archives = false;
    bool sync_full = false;
    bool use_snapshots = true;
    bool auto_create = true;
    bool auto_migrate = true;

    int archive_hour = 0;
    bool archive_utc = false;
    int archive_retention = 7;

    PasswordComplexity user_password = PasswordComplexity::Moderate;
    PasswordComplexity admin_password = PasswordComplexity::Moderate;
    PasswordComplexity root_password = PasswordComplexity::Hard;

    const char *default_username = nullptr;
    const char *default_password = nullptr;

    http_Config http { 8889 };
    const char *require_host = nullptr;

    smtp_Config smtp;
    sms_Config sms;

    bool Validate() const;

    BlockAllocator str_alloc;
};

bool CheckDomainTitle(Span<const char> title);

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
