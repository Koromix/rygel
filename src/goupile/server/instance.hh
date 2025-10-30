// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

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
    int64_t last_export_thread = -1;

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
