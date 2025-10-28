// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "config.hh"
#include "instance.hh"
#include "src/core/request/smtp.hh"
#include "src/core/sqlite/sqlite.hh"

namespace K {

extern const int DomainVersion;
extern const int MaxInstances;

struct DomainSettings {
    const char *name = nullptr;
    const char *title = nullptr;
    const char *default_lang = nullptr;

    const char *archive_key = nullptr;
    int archive_hour = 0;
    int archive_retention = 7;

    PasswordComplexity user_password = PasswordComplexity::Moderate;
    PasswordComplexity admin_password = PasswordComplexity::Moderate;
    PasswordComplexity root_password = PasswordComplexity::Hard;
    bool security_provisioned = false;

    smtp_Config smtp;
    bool smtp_provisioned = false;

    bool Validate() const;

    BlockAllocator str_alloc;
};

bool InitDomain();
void CloseDomain();
void PruneDomain();

void SyncDomain(bool wait, Span<InstanceHolder *> changes = {});

DomainHolder *RefDomain(bool installed = true);
void UnrefDomain(DomainHolder *domain);
InstanceHolder *RefInstance(const char *key);

const char *MakeInstanceFileName(const char *directory, const char *key, Allocator *alloc);
bool ParseKeyString(Span<const char> str, uint8_t out_key[32] = nullptr);

class DomainHolder {
    std::atomic_int refcount { 1 };

    int upgrade = -1;
    HashTable<Span<const char>, InstanceHolder *> map;

public:
    int64_t unique = -1;

    HeapArray<InstanceHolder *> instances;
    DomainSettings settings;

    bool Open();

    void Ref();
    void Unref();

    bool Checkpoint();

    bool IsInstalled() { return upgrade < 0; }
    int GetUpgrade() { return upgrade; }

    InstanceHolder *Ref(Span<const char> key);

    friend bool InitDomain();
    friend void CloseDomain();
    friend void PruneDomain();
};

bool MigrateDomain(sq_Database *db, const char *instances_directory);
bool MigrateDomain(const Config &config);

}
