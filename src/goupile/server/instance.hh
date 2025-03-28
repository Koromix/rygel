// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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

namespace RG {

extern const int InstanceVersion;
extern const int LegacyVersion;

class InstanceHolder {
    mutable std::atomic_int refcount { 0 };

public:
    int64_t unique = -1;

    Span<const char> key = {};
    sq_Database *db = nullptr;
    const char *title = nullptr;

    bool legacy = false;
    bool demo = false;

    InstanceHolder *master = nullptr;
    HeapArray<InstanceHolder *> slaves;

    struct {
        const char *name = nullptr;
        bool use_offline = false;
        bool data_remote = true;
        int64_t max_file_size = Megabytes(16);
        const char *lock_key = nullptr;
        const char *token_key = nullptr;
        uint8_t token_skey[32];
        uint8_t token_pkey[32];
        const char *auto_key = nullptr;
        const char *shared_key = nullptr;
        bool allow_guests = false;
    } config;

    std::atomic_int64_t fs_version { 0 };
    uint8_t challenge_key[32];

    BlockAllocator str_alloc;

    RG_HASHTABLE_HANDLER(InstanceHolder, key);

    bool Checkpoint();

    void Ref() const { master->refcount++; }
    void Unref() const { master->refcount--; }

    bool SyncViews(const char *directory);

private:
    InstanceHolder() {};

    bool Open(int64_t unique, InstanceHolder *master, const char *key, sq_Database *db, bool migrate);

    friend class DomainHolder;
};

bool MigrateInstance(sq_Database *db, int target = 0);
bool MigrateInstance(const char *filename, int target = 0);

}
