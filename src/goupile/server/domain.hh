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
#include "src/core/sqlite/sqlite.hh"

namespace K {

struct Config;

extern const int DomainVersion;
extern const int MaxInstances;

struct DomainSettings {
    const char *name = nullptr;
    const char *title = nullptr;
    const char *default_lang = "fr"; // Used to be french only, keep this value for compatibility

    const char *archive_key = nullptr;
    int archive_hour = 0;
    int archive_retention = 7;

    bool Validate() const;

    BlockAllocator str_alloc;
};

bool InitDomain();
void CloseDomain();
void PruneDomain();

void SyncDomain(bool wait, Span<InstanceHolder *> changes = {});

DomainHolder *RefDomain();
void UnrefDomain(DomainHolder *domain);
InstanceHolder *RefInstance(const char *key);

const char *MakeInstanceFileName(const char *directory, const char *key, Allocator *alloc);

class DomainHolder {
    std::atomic_int refcount { 1 };

    bool installed = false;
    HashTable<Span<const char>, InstanceHolder *> map;

public:
    int64_t unique = -1;

    HeapArray<InstanceHolder *> instances;
    DomainSettings settings;
    uint8_t archive_key[32] = {};

    bool Open();

    void Ref();
    void Unref();

    bool Checkpoint();

    bool IsInstalled() const { return installed; }
    InstanceHolder *Ref(Span<const char> key);

    friend bool InitDomain();
    friend void CloseDomain();
    friend void PruneDomain();
};

bool MigrateDomain(sq_Database *db, const char *instances_directory);
bool MigrateDomain(const Config &config);

}
