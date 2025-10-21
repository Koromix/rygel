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
#include "src/core/http/http.hh"
#include "src/core/sqlite/sqlite.hh"
#include "user.hh"

namespace K {

class DomainHolder;

extern const int InstanceVersion;
extern const int LegacyVersion;

class InstanceHolder {
    std::atomic_int refcount { 1 };

public:
    int64_t unique = -1;

    sq_Database *db = nullptr;
    Span<const char> key = {};

    const char *title = nullptr;
    bool legacy = false;
    bool demo = false;

    InstanceHolder *master = nullptr;
    HeapArray<InstanceHolder *> slaves;

    struct {
        const char *name = nullptr;
        const char *lang = nullptr;
        bool use_offline = false;
        bool data_remote = true;
        int64_t max_file_size = Megabytes(16);
        const char *lock_key = nullptr;
        const char *token_key = nullptr;
        uint8_t token_skey[32];
        uint8_t token_pkey[32];
        const char *shared_key = nullptr;
        bool allow_guests = false;
        int export_days = 0;
        int export_time = 0;
        bool export_all = false;
    } settings;

    std::atomic_int64_t fs_version { 0 };

    LocalDate last_export_day = {};
    int64_t last_export_sequence = -1;

    std::atomic_int64_t reserve_rnd { 0 };

    BlockAllocator str_alloc;

    K_HASHTABLE_HANDLER(InstanceHolder, key);

    ~InstanceHolder() { Close(); }

    bool Checkpoint();

    void Ref() { refcount++; }
    void Unref() { refcount--; }

    bool SyncViews(const char *directory);

private:
    bool Open(DomainHolder *domain, InstanceHolder *master, sq_Database *db, const char *key, bool demo);
    void Close();

    bool PerformScheduledExport();

    friend bool InitDomain();
    friend void CloseDomain();
    friend void PruneDomain();
};

bool MigrateInstance(sq_Database *db, int target = 0);
bool MigrateInstance(const char *filename, int target = 0);

}
