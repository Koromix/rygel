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

#include "src/core/base/base.hh"
#include "config.hh"
#include "domain.hh"
#include "goupile.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace K {

const int DomainVersion = 112;
const int MaxInstances = 1024;

static std::mutex mutex;
static std::condition_variable cv;

static HeapArray<sq_Database *> databases;
static HeapArray<DomainHolder *> domains;
static HashSet<void *> reloads;

static DomainHolder *domain_ptr;
static int64_t inits = 0;

// Process-wide unique domain identifier
static std::atomic_int64_t next_unique { 0 };

bool InitDomain()
{
    BlockAllocator temp_alloc;

    // Wake up threads waiting in SyncDomain, even if we fail
    K_DEFER {
        std::lock_guard<std::mutex> lock(mutex);

        inits++;
        cv.notify_all();
    };

    struct LoadInfo {
        const char *instance_key;
        const char *master_key;
        bool demo;

        sq_Database *db;
        InstanceHolder *prev;
    };

    HeapArray<LoadInfo> loads;
    HashSet<void *> keeps;
    HashSet<void *> changes;

    DomainHolder *domain = RefDomain();
    if (!domain) {
        domain = new DomainHolder();

        domains.Append(domain);
        LogDebug("Add domain 0x%1", domain);
    }
    K_DEFER_N(domain_guard) { UnrefDomain(domain); };

    // Steal list of reloads
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::swap(changes, reloads);
    }

    // Step 1
    {
        sq_Statement stmt;
        if (!gp_db.Prepare(R"(WITH RECURSIVE rec (instance, master, demo) AS (
                                  SELECT instance, master, demo FROM dom_instances WHERE master IS NULL
                                  UNION ALL
                                  SELECT i.instance, i.master, i.demo FROM dom_instances i, rec WHERE i.master = rec.instance
                                  ORDER BY 2 DESC, 1
                              )
                              SELECT instance, master, demo FROM rec)", &stmt))
            return false;

        while (stmt.Step()) {
            Span<const char> instance_key = (const char *)sqlite3_column_text(stmt, 0);
            const char *master_key = (const char *)sqlite3_column_text(stmt, 1);
            bool demo = (sqlite3_column_type(stmt, 2) != SQLITE_NULL);

            InstanceHolder *instance = domain->map.FindValue(instance_key, nullptr);

            if (instance) {
                keeps.Set(instance);
            } else {
                LoadInfo load = {};

                load.instance_key = DuplicateString(instance_key, &temp_alloc).ptr;
                load.master_key = master_key ? DuplicateString(master_key, &temp_alloc).ptr : nullptr;
                load.demo = demo;

                loads.Append(load);
            }
        }
    }

    // Step 2
    for (const LoadInfo &load: loads) {
        InstanceHolder *master = load.master_key ? domain->map.FindValue(load.master_key, nullptr) : nullptr;
        changes.Set(master);
    }
    for (Size i = domain->instances.len - 1; i >= 0; i--) {
        InstanceHolder *instance = domain->instances[i];

        if (!keeps.Find(instance) || changes.Find(instance)) {
            changes.Set(instance->master);
        }
    }
    for (InstanceHolder *instance: domain->instances) {
        if (!keeps.Find(instance))
            continue;

        LoadInfo load = {};

        load.instance_key = DuplicateString(instance->key, &temp_alloc).ptr;
        load.master_key = (instance->master != instance) ? DuplicateString(instance->master->key, &temp_alloc).ptr : nullptr;
        load.demo = instance->demo;

        load.db = instance->db;

        if (changes.Find(instance)) {
            for (InstanceHolder *slave: instance->slaves) {
                changes.Set(slave);
            }
        } else {
            load.prev = instance;
        }

        loads.Append(load);
    }

    // Step 3
    std::sort(loads.begin(), loads.end(),
              [](const LoadInfo &load1, const LoadInfo &load2) {
        const char *master1 = load1.master_key ? load1.master_key : load1.instance_key;
        const char *master2 = load2.master_key ? load2.master_key : load2.instance_key;

        return MultiCmp(CmpStr(master1, master2),
                        CmpStr(load1.instance_key, load2.instance_key)) < 0;
    });

    if (domain) {
        domain->Unref();
        domain = nullptr;
    }

    domain = new DomainHolder();
    if (!domain->Open())
        return false;

    bool complete = true;

    // Start or reload instances
    for (const LoadInfo &load: loads) {
        InstanceHolder *instance = load.prev;
        sq_Database *db = load.db;

        if (!db) {
            db = new sq_Database;
            K_DEFER_N(db_guard) { delete db; };

            const char *filename = MakeInstanceFileName(gp_config.instances_directory, load.instance_key, &temp_alloc);

            LogDebug("Open database '%1'", filename);
            if (!db->Open(filename, SQLITE_OPEN_READWRITE)) {
                complete = false;
                continue;
            }
            if (!db->SetWAL(true)) {
                complete = false;
                continue;
            }
            if (gp_config.use_snapshots && !db->SetSnapshotDirectory(gp_config.snapshot_directory, FullSnapshotDelay)) {
                complete = false;
                continue;
            }

            db_guard.Disable();
            databases.Append(db);
        }

        if (instance) {
            instance->Ref();
        } else {
            instance = new InstanceHolder();
            K_DEFER_N(instance_guard) { delete instance; };

            InstanceHolder *master = load.master_key ? domain->map.FindValue(load.master_key, nullptr) : nullptr;

            if (!instance->Open(domain, master, db, load.instance_key, load.demo)) {
                complete = false;
                continue;
            }
            if (instance->master == instance && !instance->SyncViews(gp_config.view_directory)) {
                complete = false;
                continue;
            }

            instance_guard.Disable();
        }

        domain->instances.Append(instance);
        domain->map.Set(instance);

        if (instance->master != instance) {
            instance->master->slaves.Append(instance);
        }
    }

    // Commit domain
    domain_guard.Disable();
    domains.Append(domain);
    LogDebug("Add domain 0x%1", domain);

    // Replace current domain
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (domain_ptr) {
            domain_ptr->Unref();
        }

        domain_ptr = domain;
    }

    return complete;
}

void CloseDomain()
{
    // Prevent others from getting the current domain
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (domain_ptr) {
            domain_ptr->Unref();
            domain_ptr = nullptr;
        }
    }

    PruneDomain();

    while (domains.len) {
        WaitDelay(100);
        PruneDomain();
    }
}

void PruneDomain()
{
    HeapArray<InstanceHolder *> drops;
    HashSet<void *> keeps;

    // Clear unused domains
    {
        Size j = 0;
        for (Size i = 0; i < domains.len; i++) {
            DomainHolder *domain = domains[i];

            domains[j] = domain;

            for (InstanceHolder *instance: domain->instances) {
                if (instance->refcount) {
                    keeps.Set(instance);
                    keeps.Set(instance->db);
                } else {
                    drops.Append(instance);
                }
            }

            if (domain->refcount) {
                j++;
            } else {
                LogDebug("Delete domain 0x%1", domain);
                delete domain;
            }
        }
        domains.len = j;
    }

    // Delete unused instances
    for (InstanceHolder *instance: drops) {
        if (keeps.Find(instance))
            continue;

        keeps.Set(instance);
        delete instance;
    }

    // Close unused databases
    {
        Size j = 0;
        for (Size i = 0; i < databases.len; i++) {
            sq_Database *db = databases[i];

            databases[j] = db;

            if (keeps.Find(db)) {
                j++;
            } else {
                const char *filename = sqlite3_db_filename(*db, "main");
                LogDebug("Close database '%1'", filename);

                delete db;
            }
        }
        databases.len = j;
    }
}

void SyncDomain(bool wait, Span<InstanceHolder *> changes)
{
    std::unique_lock<std::mutex> lock(mutex);
    int64_t prev_inits = inits;

    for (InstanceHolder *instance: changes) {
        reloads.Set(instance);
    }

    // Signal main thread to reload domain
    InterruptWait();

    if (wait) {
        do {
            cv.wait(lock);
        } while (inits == prev_inits);
    }
}

DomainHolder *RefDomain()
{
    std::lock_guard<std::mutex> lock(mutex);

    if (!domain_ptr)
        return nullptr;
    if (!domain_ptr->IsInstalled())
        return nullptr;

    domain_ptr->Ref();
    return domain_ptr;
}

void UnrefDomain(DomainHolder *domain)
{
    if (!domain)
        return;

    domain->Unref();
}

InstanceHolder *RefInstance(const char *key)
{
    DomainHolder *domain = RefDomain();
    K_DEFER { UnrefDomain(domain); };

    if (!domain) [[unlikely]]
        return nullptr;

    return domain->Ref(key);
}

const char *MakeInstanceFileName(const char *directory, const char *key, Allocator *alloc)
{
    HeapArray<char> buf(alloc);

    Fmt(&buf, "%1%/", directory);
    for (Size i = 0; key[i]; i++) {
        char c = key[i];
        buf.Append(c != '/' ? c : '@');
    }
    buf.Append(".db");
    buf.Append(0);

    return buf.Leak().ptr;
}

bool ParseKeyString(Span<const char> str, uint8_t out_key[32])
{
    static_assert(crypto_box_curve25519xsalsa20poly1305_PUBLICKEYBYTES == 32);

    if (!str.len) {
        LogError("Empty or missing encryption key");
        return false;
    }

    uint8_t key[32];
    size_t key_len;
    int ret = sodium_base642bin(key, K_SIZE(key), str.ptr, (size_t)str.len,
                                nullptr, &key_len, nullptr, sodium_base64_VARIANT_ORIGINAL);
    if (ret || key_len != 32) {
        LogError("Malformed encryption key");
        return false;
    }

    if (out_key) {
        MemCpy(out_key, key, K_SIZE(key));
    }
    return true;
}

static bool CheckDomainName(Span<const char> name)
{
    const auto test_char = [](char c) { return IsAsciiAlphaOrDigit(c) || c == '_' || c == '.' || c == '-'; };

    if (!name.len) {
        LogError("Domain name cannot be empty");
        return false;
    }
    if (name.len > 64) {
        LogError("Domain name cannot be have more than 64 characters");
        return false;
    }
    if (!std::all_of(name.begin(), name.end(), test_char)) {
        LogError("Domain name must only contain alphanumeric, '_', '.' or '-' characters");
        return false;
    }

    return true;
}

static bool CheckDomainTitle(Span<const char> title)
{
    if (!title.len) {
        LogError("Domain title cannot be empty");
        return false;
    }
    if (title.len > 64) {
        LogError("Domain title cannot be have more than 64 characters");
        return false;
    }

    return true;
}

bool DomainSettings::Validate() const
{
    bool valid = true;

    valid &= CheckDomainName(name);
    valid &= CheckDomainTitle(title);
    valid &= ParseKeyString(archive_key);

    return valid;
}

bool DomainHolder::Open()
{
    unique = ++next_unique;

    // Load high-level settings
    {
        sq_Statement stmt;
        if (!gp_db.Prepare("SELECT key, value FROM dom_settings", &stmt))
            return false;

        bool valid = true;

        while (stmt.Step()) {
            const char *setting = (const char *)sqlite3_column_text(stmt, 0);
            const char *value = (const char *)sqlite3_column_text(stmt, 1);

            if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
                if (TestStr(setting, "Name")) {
                    settings.name = DuplicateString(value, &settings.str_alloc).ptr;
                } else if (TestStr(setting, "Title")) {
                    settings.title = DuplicateString(value, &settings.str_alloc).ptr;
                } else if (TestStr(setting, "DefaultLanguage")) {
                    settings.default_lang = DuplicateString(value, &settings.str_alloc).ptr;
                } else if (TestStr(setting, "ArchiveKey")) {
                    settings.archive_key = DuplicateString(value, &settings.str_alloc).ptr;
                }
            }
        }
        if (!stmt.IsValid() || !valid)
            return false;

        // Default values
        if (!settings.name) {
            LogWarning("Using default 'goupile' name for domain");
            settings.name = "goupile";
        }
        if (!settings.title) {
            settings.title = settings.name;
        }
    }

    // Detect valid installation (at least one user)
    {
        sq_Statement stmt;
        if (!gp_db.Prepare("SELECT userid FROM dom_users", &stmt))
            return false;
        if (!stmt.Run())
            return false;

        installed = stmt.IsRow();
    }

    if (installed && !settings.Validate())
        return false;

    return true;
}

void DomainHolder::Ref()
{
    refcount++;
}

void DomainHolder::Unref()
{
    if (--refcount)
        return;

    for (InstanceHolder *instance: instances) {
        instance->Unref();
    }
}

bool DomainHolder::Checkpoint()
{
    Async async;

    for (InstanceHolder *instance: instances) {
        async.Run([instance]() { return instance->Checkpoint(); });
    }

    return async.Sync();
}

InstanceHolder *DomainHolder::Ref(Span<const char> key)
{
    InstanceHolder *instance = map.FindValue(key, nullptr);

    if (instance) {
        instance->Ref();
        return instance;
    } else {
        return nullptr;
    }
}

bool MigrateDomain(sq_Database *db, const char *instances_directory)
{
    int version;
    if (!db->GetUserVersion(&version))
        return false;

    if (version > DomainVersion) {
        LogError("Domain schema is too recent (%1, expected %2)", version, DomainVersion);
        return false;
    } else if (version == DomainVersion) {
        return true;
    }

    LogInfo("Migrate domain database: %1 to %2", version, DomainVersion);

    bool success = db->Transaction([&]() {
        int64_t time = GetUnixTime();

        switch (version) {
            case 0: {
                bool success = db->RunMany(R"(
                    CREATE TABLE adm_events (
                        time INTEGER NOT NULL,
                        address TEXT,
                        type TEXT NOT NULL,
                        username TEXT NOT NULL,
                        details TEXT
                    );

                    CREATE TABLE adm_migrations (
                        version INTEGER NOT NULL,
                        build TEXT NOT NULL,
                        time INTEGER NOT NULL
                    );

                    CREATE TABLE dom_users (
                        username TEXT NOT NULL,
                        password_hash TEXT NOT NULL,
                        admin INTEGER CHECK (admin IN (0, 1)) NOT NULL
                    );
                    CREATE UNIQUE INDEX dom_users_u ON dom_users (username);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 1: {
                bool success = db->RunMany(R"(
                    CREATE TABLE dom_permissions (
                        username TEXT NOT NULL REFERENCES dom_users (username),
                        instance TEXT NOT NULL,
                        permissions INTEGER NOT NULL,
                        zone TEXT
                    );
                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (username, instance);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 2: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_permissions RENAME TO dom_permissions_BAK;
                    DROP INDEX dom_permissions_ui;

                    CREATE TABLE dom_instances (
                        instance TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX dom_instances_i ON dom_instances (instance);
                )");
                if (!success)
                    return false;

                // Insert existing instances
                if (version && instances_directory) {
                    sq_Statement stmt;
                    if (!db->Prepare("INSERT INTO dom_instances (instance) VALUES (?)", &stmt))
                        return false;

                    EnumResult ret = EnumerateDirectory(instances_directory, "*.db", -1,
                                                        [&](const char *filename, FileType) {
                        Span<const char> key = SplitStrReverseAny(filename, K_PATH_SEPARATORS);
                        key = SplitStr(key, '.');

                        stmt.Reset();
                        sqlite3_bind_text(stmt, 1, key.ptr, (int)key.len, SQLITE_STATIC);

                        return stmt.Run();
                    });
                    if (ret != EnumResult::Success)
                        return false;
                }

                success = db->RunMany(R"(
                    CREATE TABLE dom_permissions (
                        username TEXT NOT NULL REFERENCES dom_users (username),
                        instance TEXT NOT NULL REFERENCES dom_instances (instance),
                        permissions INTEGER NOT NULL,
                        zone TEXT
                    );

                    INSERT INTO dom_permissions (username, instance, permissions, zone)
                        SELECT username, instance, permissions, zone FROM dom_permissions_BAK;
                    DROP TABLE dom_permissions_BAK;

                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (username, instance);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 3: {
                if (!db->RunMany("UPDATE dom_permissions SET permissions = 127 WHERE permissions == 63"))
                    return false;
            } [[fallthrough]];

            case 4: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_users RENAME TO dom_users_BAK;
                    ALTER TABLE dom_permissions RENAME TO dom_permissions_BAK;
                    DROP INDEX dom_users_u;
                    DROP INDEX dom_permissions_ui;

                    CREATE TABLE dom_users (
                        username TEXT NOT NULL,
                        password_hash TEXT NOT NULL,
                        admin INTEGER CHECK (admin IN (0, 1)) NOT NULL,
                        passport TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX dom_users_u ON dom_users (username);

                    CREATE TABLE dom_permissions (
                        username TEXT NOT NULL REFERENCES dom_users (username),
                        instance TEXT NOT NULL REFERENCES dom_instances (instance),
                        permissions INTEGER NOT NULL,
                        zone TEXT
                    );
                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (username, instance);

                    INSERT INTO dom_users (username, password_hash, admin, passport)
                        SELECT username, password_hash, admin, '' FROM dom_users_BAK;
                    INSERT INTO dom_permissions (username, instance, permissions, zone)
                        SELECT username, instance, permissions, zone FROM dom_permissions_BAK;

                    DROP TABLE dom_users_BAK;
                    DROP TABLE dom_permissions_BAK;
                )");
                if (!success)
                    return false;

                sq_Statement stmt;
                if (!db->Prepare("SELECT rowid FROM dom_users", &stmt))
                    return false;

                while (stmt.Step()) {
                    int64_t rowid = sqlite3_column_int64(stmt, 0);

                    // Create passport key
                    char passport[45];
                    {
                        uint8_t buf[32];
                        FillRandomSafe(buf);
                        sodium_bin2base64(passport, K_SIZE(passport), buf, K_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
                    }

                    if (!db->Run("UPDATE dom_users SET passport = ?2 WHERE rowid = ?1", rowid, passport))
                        return false;
                }
                if (!stmt.IsValid())
                    return false;
            } [[fallthrough]];

            case 5: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_users RENAME TO dom_users_BAK;
                    ALTER TABLE dom_permissions RENAME TO dom_permissions_BAK;
                    DROP INDEX dom_users_u;
                    DROP INDEX dom_permissions_ui;

                    CREATE TABLE dom_users (
                        userid INTEGER PRIMARY KEY AUTOINCREMENT,
                        username TEXT NOT NULL,
                        password_hash TEXT NOT NULL,
                        admin INTEGER CHECK (admin IN (0, 1)) NOT NULL,
                        passport TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX dom_users_u ON dom_users (username);

                    CREATE TABLE dom_permissions (
                        userid INTEGER NOT NULL REFERENCES dom_users (userid),
                        instance TEXT NOT NULL REFERENCES dom_instances (instance),
                        permissions INTEGER NOT NULL,
                        zone TEXT
                    );
                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (userid, instance);

                    INSERT INTO dom_users (username, password_hash, admin, passport)
                        SELECT username, password_hash, admin, passport FROM dom_users_BAK;
                    INSERT INTO dom_permissions (userid, instance, permissions, zone)
                        SELECT u.userid, p.instance, p.permissions, p.zone FROM dom_permissions_BAK p
                        LEFT JOIN dom_users u ON (u.username = p.username);

                    DROP TABLE dom_users_BAK;
                    DROP TABLE dom_permissions_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 6: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_users RENAME COLUMN passport TO local_key;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 7: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_instances ADD COLUMN master TEXT REFERENCES dom_instances (instance);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 8: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_users RENAME TO dom_users_BAK;
                    ALTER TABLE dom_permissions RENAME TO dom_permissions_BAK;
                    DROP INDEX dom_users_u;
                    DROP INDEX dom_permissions_ui;

                    CREATE TABLE dom_users (
                        userid INTEGER PRIMARY KEY AUTOINCREMENT,
                        username TEXT NOT NULL,
                        password_hash TEXT NOT NULL,
                        admin INTEGER CHECK (admin IN (0, 1)) NOT NULL,
                        local_key TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX dom_users_u ON dom_users (username);

                    CREATE TABLE dom_permissions (
                        userid INTEGER NOT NULL REFERENCES dom_users (userid),
                        instance TEXT NOT NULL REFERENCES dom_instances (instance),
                        permissions INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (userid, instance);

                    INSERT INTO dom_users (userid, username, password_hash, admin, local_key)
                        SELECT userid, username, password_hash, admin, local_key FROM dom_users_BAK;
                    INSERT INTO dom_permissions (userid, instance, permissions)
                        SELECT userid, instance, permissions FROM dom_permissions_BAK;

                    DROP TABLE dom_permissions_BAK;
                    DROP TABLE dom_users_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 9: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_instances RENAME TO dom_instances_BAK;
                    ALTER TABLE dom_permissions RENAME TO dom_permissions_BAK;
                    DROP INDEX dom_instances_i;
                    DROP INDEX dom_permissions_ui;

                    CREATE TABLE dom_instances (
                        instance TEXT NOT NULL,
                        master TEXT REFERENCES dom_instances (instance) ON DELETE CASCADE
                    );
                    CREATE UNIQUE INDEX dom_instances_i ON dom_instances (instance);

                    CREATE TABLE dom_permissions (
                        userid INTEGER NOT NULL REFERENCES dom_users (userid) ON DELETE CASCADE,
                        instance TEXT NOT NULL REFERENCES dom_instances (instance) ON DELETE CASCADE,
                        permissions INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (userid, instance);

                    INSERT INTO dom_instances (instance, master)
                        SELECT instance, master FROM dom_instances_BAK;
                    INSERT INTO dom_permissions (userid, instance, permissions)
                        SELECT userid, instance, permissions FROM dom_permissions_BAK;

                    DROP TABLE dom_permissions_BAK;
                    DROP TABLE dom_instances_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 10: {
                // This migration is incomplete and does not rename slave instance database files

                bool success = db->RunMany(R"(
                    ALTER TABLE dom_instances RENAME TO dom_instances_BAK;
                    ALTER TABLE dom_permissions RENAME TO dom_permissions_BAK;
                    DROP INDEX dom_instances_i;
                    DROP INDEX dom_permissions_ui;

                    CREATE TABLE dom_instances (
                        instance TEXT NOT NULL,
                        master TEXT GENERATED ALWAYS AS (IIF(instr(instance, '@') > 0, substr(instance, 1, instr(instance, '@') - 1), NULL)) STORED
                                    REFERENCES dom_instances (instance) ON DELETE CASCADE
                    );
                    CREATE UNIQUE INDEX dom_instances_i ON dom_instances (instance);

                    CREATE TABLE dom_permissions (
                        userid INTEGER NOT NULL REFERENCES dom_users (userid) ON DELETE CASCADE,
                        instance TEXT NOT NULL REFERENCES dom_instances (instance) ON DELETE CASCADE,
                        permissions INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (userid, instance);

                    INSERT INTO dom_instances (instance)
                        SELECT IIF(master IS NULL, instance, master || '@' || instance) FROM dom_instances_BAK ORDER BY master ASC NULLS FIRST;
                    INSERT INTO dom_permissions (userid, instance, permissions)
                        SELECT p.userid, IIF(i.master IS NULL, i.instance, i.master || '@' || i.instance), p.permissions FROM dom_permissions_BAK p
                        LEFT JOIN dom_instances_BAK i ON (i.instance = p.instance);

                    DROP TABLE dom_permissions_BAK;
                    DROP TABLE dom_instances_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 11: {
                bool success = db->RunMany(R"(
                    CREATE INDEX dom_instances_m ON dom_instances (master);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 12: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_instances RENAME TO dom_instances_BAK;
                    ALTER TABLE dom_permissions RENAME TO dom_permissions_BAK;
                    DROP INDEX dom_instances_i;
                    DROP INDEX dom_permissions_ui;

                    CREATE TABLE dom_instances (
                        instance TEXT NOT NULL,
                        master TEXT GENERATED ALWAYS AS (IIF(instr(instance, '/') > 0, substr(instance, 1, instr(instance, '/') - 1), NULL)) STORED
                                    REFERENCES dom_instances (instance) ON DELETE CASCADE
                    );
                    CREATE UNIQUE INDEX dom_instances_i ON dom_instances (instance);

                    CREATE TABLE dom_permissions (
                        userid INTEGER NOT NULL REFERENCES dom_users (userid) ON DELETE CASCADE,
                        instance TEXT NOT NULL REFERENCES dom_instances (instance) ON DELETE CASCADE,
                        permissions INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (userid, instance);

                    INSERT INTO dom_instances (instance)
                        SELECT replace(instance, '@', '/') FROM dom_instances_BAK ORDER BY master ASC NULLS FIRST;
                    INSERT INTO dom_permissions (userid, instance, permissions)
                        SELECT userid, replace(instance, '@', '/'), permissions FROM dom_permissions_BAK;

                    DROP TABLE dom_permissions_BAK;
                    DROP TABLE dom_instances_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 13: {
                bool success = db->RunMany(R"(
                    UPDATE dom_permissions SET permissions = IIF(permissions & 1, 1, 0) |
                                                             IIF(permissions & 8, 2, 0) |
                                                             IIF(permissions & 1, 4, 0) |
                                                             IIF(permissions & 1, 8, 0) |
                                                             IIF(permissions & 4, 16, 0) |
                                                             IIF(permissions & 2, 32, 0) |
                                                             IIF(permissions & 4, 64, 0) |
                                                             IIF(permissions & 32, 128, 0) |
                                                             IIF(permissions & 64, 256, 0);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 14: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_users ADD COLUMN email TEXT;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 15: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_instances RENAME TO dom_instances_BAK;
                    ALTER TABLE dom_permissions RENAME TO dom_permissions_BAK;
                    DROP INDEX dom_instances_i;
                    DROP INDEX dom_permissions_ui;

                    CREATE TABLE dom_instances (
                        instance TEXT NOT NULL,
                        master TEXT GENERATED ALWAYS AS (IIF(instr(instance, '/') > 0, substr(instance, 1, instr(instance, '/') - 1), NULL)) STORED
                                    REFERENCES dom_instances (instance) ON DELETE CASCADE,
                        generation INTEGER NOT NULL DEFAULT 0
                    );
                    CREATE UNIQUE INDEX dom_instances_i ON dom_instances (instance);

                    CREATE TABLE dom_permissions (
                        userid INTEGER NOT NULL REFERENCES dom_users (userid) ON DELETE CASCADE,
                        instance TEXT NOT NULL REFERENCES dom_instances (instance) ON DELETE CASCADE,
                        permissions INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (userid, instance);

                    INSERT INTO dom_instances (instance)
                        SELECT instance FROM dom_instances_BAK ORDER BY master ASC NULLS FIRST;
                    INSERT INTO dom_permissions (userid, instance, permissions)
                        SELECT userid, instance, permissions FROM dom_permissions_BAK;

                    DROP TABLE dom_permissions_BAK;
                    DROP TABLE dom_instances_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 16: {
                bool success = db->RunMany(R"(
                    UPDATE dom_permissions SET permissions = IIF(permissions & 1, 1, 0) |
                                                             IIF(permissions & 2, 2, 0) |
                                                             IIF(permissions & 4, 4, 0) |
                                                             IIF(permissions & 8, 8, 0) |
                                                             IIF(permissions & 16, 16, 0) |
                                                             IIF(permissions & 16, 32, 0) |
                                                             IIF(permissions & 32, 64, 0) |
                                                             IIF(permissions & 64, 128, 0) |
                                                             IIF(permissions & 64, 256, 0) |
                                                             IIF(permissions & 128, 512, 0) |
                                                             IIF(permissions & 256, 1024, 0);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 17: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_instances RENAME TO dom_instances_BAK;
                    ALTER TABLE dom_permissions RENAME TO dom_permissions_BAK;
                    DROP INDEX dom_instances_i;
                    DROP INDEX dom_permissions_ui;

                    CREATE TABLE dom_instances (
                        instance TEXT NOT NULL,
                        master TEXT GENERATED ALWAYS AS (IIF(instr(instance, '/') > 0, substr(instance, 1, instr(instance, '/') - 1), NULL)) STORED
                                    REFERENCES dom_instances (instance),
                        generation INTEGER NOT NULL DEFAULT 0
                    );
                    CREATE UNIQUE INDEX dom_instances_i ON dom_instances (instance);

                    CREATE TABLE dom_permissions (
                        userid INTEGER NOT NULL REFERENCES dom_users (userid) ON DELETE CASCADE,
                        instance TEXT NOT NULL REFERENCES dom_instances (instance) ON DELETE CASCADE,
                        permissions INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (userid, instance);

                    INSERT INTO dom_instances (instance, generation)
                        SELECT instance, generation FROM dom_instances_BAK ORDER BY master ASC NULLS FIRST;
                    INSERT INTO dom_permissions (userid, instance, permissions)
                        SELECT userid, instance, permissions FROM dom_permissions_BAK;

                    DROP TABLE dom_permissions_BAK;
                    DROP TABLE dom_instances_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 18: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_users ADD COLUMN phone TEXT;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 19: {
                bool success = db->RunMany(R"(
                    UPDATE dom_permissions SET permissions = IIF(permissions & 1, 1, 0) |
                                                             IIF(permissions & 2, 2, 0) |
                                                             IIF(permissions & 4, 4, 0) |
                                                             IIF(permissions & 8, 8, 0) |
                                                             IIF(permissions & 16, 16, 0) |
                                                             IIF(permissions & 128, 32, 0) |
                                                             IIF(permissions & 512, 64, 0) |
                                                             IIF(permissions & 1024, 128, 0);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 20: {
                bool success = db->RunMany(R"(
                    UPDATE dom_permissions SET permissions = permissions |
                                                             IIF(permissions & 1, 256, 0);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 21: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_instances DROP COLUMN generation;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 22: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_users RENAME TO dom_users_BAK;
                    ALTER TABLE dom_permissions RENAME TO dom_permissions_BAK;
                    DROP INDEX dom_users_u;
                    DROP INDEX dom_permissions_ui;

                    CREATE TABLE dom_users (
                        userid INTEGER PRIMARY KEY AUTOINCREMENT,
                        username TEXT NOT NULL,
                        password_hash TEXT NOT NULL,
                        admin INTEGER CHECK (admin IN (0, 1)) NOT NULL,
                        local_key TEXT NOT NULL,
                        totp_required INTEGER CHECK (admin IN (0, 1)) NOT NULL,
                        totp_secret TEXT,
                        email TEXT,
                        phone TEXT
                    );
                    CREATE UNIQUE INDEX dom_users_u ON dom_users (username);

                    CREATE TABLE dom_permissions (
                        userid INTEGER NOT NULL REFERENCES dom_users (userid),
                        instance TEXT NOT NULL REFERENCES dom_instances (instance),
                        permissions INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (userid, instance);

                    INSERT INTO dom_users (userid, username, password_hash, admin, local_key, email, phone, totp_required)
                        SELECT userid, username, password_hash, admin, local_key, email, phone, 0 FROM dom_users_BAK;
                    INSERT INTO dom_permissions (userid, instance, permissions)
                        SELECT userid, instance, permissions FROM dom_permissions_BAK
                        WHERE userid IN (SELECT userid FROM dom_users) AND
                              instance IN (SELECT instance FROM dom_instances);

                    DROP TABLE dom_permissions_BAK;
                    DROP TABLE dom_users_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 23: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_permissions RENAME TO dom_permissions_BAK;
                    DROP INDEX dom_permissions_ui;

                    CREATE TABLE dom_permissions (
                        userid INTEGER NOT NULL REFERENCES dom_users (userid),
                        instance TEXT NOT NULL REFERENCES dom_instances (instance) ON DELETE CASCADE,
                        permissions INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (userid, instance);

                    INSERT INTO dom_permissions (userid, instance, permissions)
                        SELECT userid, instance, permissions FROM dom_permissions_BAK;

                    DROP TABLE dom_permissions_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 24: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_users RENAME TO dom_users_BAK;
                    ALTER TABLE dom_permissions RENAME TO dom_permissions_BAK;
                    DROP INDEX dom_users_u;
                    DROP INDEX dom_permissions_ui;

                    CREATE TABLE dom_users (
                        userid INTEGER PRIMARY KEY AUTOINCREMENT,
                        username TEXT NOT NULL,
                        password_hash TEXT NOT NULL,
                        admin INTEGER CHECK (admin IN (0, 1)) NOT NULL,
                        local_key TEXT NOT NULL,
                        confirm TEXT,
                        secret TEXT,
                        email TEXT,
                        phone TEXT
                    );
                    CREATE UNIQUE INDEX dom_users_u ON dom_users (username);

                    CREATE TABLE dom_permissions (
                        userid INTEGER NOT NULL REFERENCES dom_users (userid),
                        instance TEXT NOT NULL REFERENCES dom_instances (instance),
                        permissions INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (userid, instance);

                    INSERT INTO dom_users (userid, username, password_hash, admin, local_key, confirm, secret, email, phone)
                        SELECT userid, username, password_hash, admin, local_key,
                               IIF(totp_required == 1, 'TOTP', NULL), totp_secret, email, phone FROM dom_users_BAK;
                    INSERT INTO dom_permissions (userid, instance, permissions)
                        SELECT userid, instance, permissions FROM dom_permissions_BAK;

                    DROP TABLE dom_permissions_BAK;
                    DROP TABLE dom_users_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 25: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_users RENAME TO dom_users_BAK;
                    ALTER TABLE dom_permissions RENAME TO dom_permissions_BAK;
                    DROP INDEX dom_users_u;
                    DROP INDEX dom_permissions_ui;

                    CREATE TABLE dom_users (
                        userid INTEGER PRIMARY KEY AUTOINCREMENT,
                        username TEXT NOT NULL,
                        password_hash TEXT NOT NULL,
                        admin INTEGER CHECK (admin IN (0, 1)) NOT NULL,
                        local_key TEXT NOT NULL,
                        confirm TEXT,
                        secret TEXT,
                        email TEXT,
                        phone TEXT
                    );
                    CREATE UNIQUE INDEX dom_users_u ON dom_users (username);

                    CREATE TABLE dom_permissions (
                        userid INTEGER NOT NULL REFERENCES dom_users (userid),
                        instance TEXT NOT NULL REFERENCES dom_instances (instance) ON DELETE CASCADE,
                        permissions INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (userid, instance);

                    INSERT INTO dom_users (userid, username, password_hash, admin, local_key, confirm, secret, email, phone)
                        SELECT userid, username, password_hash, admin, local_key, confirm, secret, email, phone FROM dom_users_BAK;
                    INSERT INTO dom_permissions (userid, instance, permissions)
                        SELECT userid, instance, permissions FROM dom_permissions_BAK;

                    DROP TABLE dom_permissions_BAK;
                    DROP TABLE dom_users_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 26: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_users RENAME TO dom_users_BAK;
                    ALTER TABLE dom_permissions RENAME TO dom_permissions_BAK;
                    DROP INDEX dom_users_u;
                    DROP INDEX dom_permissions_ui;

                    CREATE TABLE dom_users (
                        userid INTEGER PRIMARY KEY AUTOINCREMENT,
                        username TEXT NOT NULL,
                        password_hash TEXT NOT NULL,
                        change_password INTEGER CHECK (change_password IN (0, 1)) NOT NULL,
                        admin INTEGER CHECK (admin IN (0, 1)) NOT NULL,
                        local_key TEXT NOT NULL,
                        confirm TEXT,
                        secret TEXT,
                        email TEXT,
                        phone TEXT
                    );
                    CREATE UNIQUE INDEX dom_users_u ON dom_users (username);

                    CREATE TABLE dom_permissions (
                        userid INTEGER NOT NULL REFERENCES dom_users (userid) ON DELETE CASCADE,
                        instance TEXT NOT NULL REFERENCES dom_instances (instance) ON DELETE CASCADE,
                        permissions INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (userid, instance);

                    INSERT INTO dom_users (userid, username, password_hash, change_password, admin, local_key, confirm, secret, email, phone)
                        SELECT userid, username, password_hash, 0, admin, local_key, confirm, secret, email, phone FROM dom_users_BAK;
                    INSERT INTO dom_permissions (userid, instance, permissions)
                        SELECT userid, instance, permissions FROM dom_permissions_BAK;

                    DROP TABLE dom_permissions_BAK;
                    DROP TABLE dom_users_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 27: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_permissions ADD COLUMN export_key TEXT;
                    CREATE UNIQUE INDEX dom_permissions_e ON dom_permissions (export_key);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 28: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_users RENAME TO dom_users_BAK;
                    ALTER TABLE dom_permissions RENAME TO dom_permissions_BAK;
                    DROP INDEX dom_users_u;
                    DROP INDEX dom_permissions_ui;
                    DROP INDEX dom_permissions_e;

                    CREATE TABLE dom_users (
                        userid INTEGER PRIMARY KEY AUTOINCREMENT,
                        username TEXT NOT NULL,
                        password_hash TEXT NOT NULL,
                        change_password INTEGER CHECK (change_password IN (0, 1)) NOT NULL,
                        admin INTEGER CHECK (admin IN (0, 1)) NOT NULL,
                        local_key TEXT NOT NULL,
                        confirm TEXT,
                        secret TEXT,
                        email TEXT,
                        phone TEXT
                    );
                    CREATE UNIQUE INDEX dom_users_u ON dom_users (username);

                    CREATE TABLE dom_permissions (
                        userid INTEGER NOT NULL REFERENCES dom_users (userid) ON DELETE CASCADE,
                        instance TEXT NOT NULL REFERENCES dom_instances (instance) ON DELETE CASCADE,
                        permissions INTEGER NOT NULL,
                        export_key TEXT
                    );
                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (userid, instance);
                    CREATE UNIQUE INDEX dom_permissions_e ON dom_permissions (export_key);

                    INSERT INTO dom_users (userid, username, password_hash, change_password, admin, local_key, confirm, secret, email, phone)
                        SELECT userid, username, password_hash, change_password, admin, local_key, confirm, secret, email, phone FROM dom_users_BAK;
                    INSERT INTO dom_permissions (userid, instance, permissions, export_key)
                        SELECT userid, instance, permissions, export_key FROM dom_permissions_BAK;

                    DROP TABLE dom_permissions_BAK;
                    DROP TABLE dom_users_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 29: {
                bool success = db->RunMany(R"(
                    UPDATE dom_permissions SET permissions = permissions |
                                                             IIF(permissions & 16, 512, 0);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 30: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_users RENAME COLUMN admin TO root;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 31: {
                bool success = db->RunMany(R"(
                    UPDATE dom_permissions SET permissions = permissions & ~8;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 32: {
                // Goupile v2 domain version
            } [[fallthrough]];

            case 100: {
                bool success = db->RunMany(R"(
                    UPDATE dom_permissions SET permissions = IIF(permissions & 1, 1, 0) |
                                                             IIF(permissions & 2, 2, 0) |
                                                             IIF(permissions & 4, 4, 0) |
                                                             IIF(permissions & 16, 16, 0) |
                                                             IIF(permissions & 32, 32, 0) |
                                                             IIF(permissions & 64, 128, 0) |
                                                             IIF(permissions & 128, 8, 0) |
                                                             IIF(permissions & 256, 256, 0) |
                                                             IIF(permissions & 512, 64, 0);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 101: {
                bool success = db->RunMany(R"(
                    UPDATE dom_permissions SET permissions = IIF(permissions & 1, 1, 0) |
                                                             IIF(permissions & 2, 2, 0) |
                                                             IIF(permissions & 4, 4, 0) |
                                                             IIF(permissions & 8, 8, 0) |
                                                             IIF(permissions & 16, 16, 0) |
                                                             IIF(permissions & 32, 32 | 64 | 128, 0) |
                                                             IIF(permissions & 64, 256, 0) |
                                                             IIF(permissions & 128, 512, 0) |
                                                             IIF(permissions & 256, 1024 | 2048, 0);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 102: {
                bool success = db->RunMany(R"(
                    UPDATE dom_users SET confirm = 'TOTP' WHERE confirm = 'totp';
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 103: {
                bool success = db->RunMany(R"(
                    UPDATE dom_permissions SET permissions = (permissions & 511) |
                                                             IIF(permissions & 256, 512, 0) |
                                                             IIF(permissions & 512, 1024, 0) |
                                                             IIF(permissions & 1024, 2048, 0) |
                                                             IIF(permissions & 2048, 4096, 0) |
                                                             IIF(permissions & 4096, 8192, 0);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 104: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_users RENAME TO dom_users_BAK;
                    ALTER TABLE dom_permissions RENAME TO dom_permissions_BAK;
                    DROP INDEX dom_users_u;
                    DROP INDEX dom_permissions_ui;
                    DROP INDEX dom_permissions_e;

                    CREATE TABLE dom_users (
                        userid INTEGER PRIMARY KEY AUTOINCREMENT,
                        username TEXT NOT NULL,
                        password_hash TEXT,
                        change_password INTEGER CHECK (change_password IN (0, 1)) NOT NULL,
                        root INTEGER CHECK (root IN (0, 1)) NOT NULL,
                        local_key TEXT NOT NULL,
                        confirm TEXT,
                        secret TEXT,
                        email TEXT,
                        phone TEXT
                    );
                    CREATE UNIQUE INDEX dom_users_u ON dom_users (username);

                    CREATE TABLE dom_permissions (
                        userid INTEGER NOT NULL REFERENCES dom_users (userid) ON DELETE CASCADE,
                        instance TEXT NOT NULL REFERENCES dom_instances (instance) ON DELETE CASCADE,
                        permissions INTEGER NOT NULL,
                        export_key TEXT
                    );
                    CREATE UNIQUE INDEX dom_permissions_ui ON dom_permissions (userid, instance);
                    CREATE UNIQUE INDEX dom_permissions_e ON dom_permissions (export_key);

                    INSERT INTO dom_users (userid, username, password_hash, change_password, root, local_key, confirm, secret, email, phone)
                        SELECT userid, username, password_hash, change_password, root, local_key, confirm, secret, email, phone FROM dom_users_BAK;
                    INSERT INTO dom_permissions (userid, instance, permissions, export_key)
                        SELECT userid, instance, permissions, export_key FROM dom_permissions_BAK;

                    DROP TABLE dom_permissions_BAK;
                    DROP TABLE dom_users_BAK;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 105: {
                if (version && version < 100) {
                    bool success = db->RunMany(R"(
                        UPDATE dom_permissions SET permissions = permissions | 8192;
                    )");
                    if (!success)
                        return false;
                }
            } [[fallthrough]];

            case 106: {
                bool success = db->RunMany(R"(
                    UPDATE dom_users SET phone = NULL WHERE phone = '';
                    UPDATE dom_users SET email = NULL WHERE email = '';
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 107: {
                bool success = db->RunMany(R"(
                    UPDATE dom_permissions SET permissions = IIF(permissions & 1, 1, 0) |
                                                             IIF(permissions & 2, 2, 0) |
                                                             IIF(permissions & 4, 4, 0) |
                                                             IIF(permissions & 8, 8, 0) |
                                                             IIF(permissions & 16, 16, 0) |
                                                             IIF(permissions & 64, 32, 0) |
                                                             IIF(permissions & 128, 64, 0) |
                                                             IIF(permissions & 256, 128, 0) |
                                                             IIF(permissions & 512, 256, 0) |
                                                             IIF(permissions & 1024, 512, 0) |
                                                             IIF(permissions & 2048, 1024, 0) |
                                                             IIF(permissions & 4096, 2048, 0) |
                                                             IIF(permissions & 8192, 4096, 0);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 108: {
                bool success = db->RunMany(R"(
                    ALTER TABLE dom_instances ADD COLUMN demo INTEGER;
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 109: {
                bool success = db->RunMany(R"(
                    UPDATE dom_permissions SET permissions = IIF(permissions & 1, 1, 0) |
                                                             IIF(permissions & 2, 2, 0) |
                                                             IIF(permissions & 4, 4, 0) |
                                                             IIF(permissions & 8, 8, 0) |
                                                             IIF(permissions & 16, 16, 0) |
                                                             IIF(permissions & 32, 32, 0) |
                                                             IIF(permissions & 64, 64, 0) |
                                                             IIF(permissions & 128, 128, 0) |
                                                             IIF(permissions & 256, 256, 0) |
                                                             IIF(permissions & 512, 512, 0) |
                                                             IIF(permissions & 512, 1024, 0) |
                                                             IIF(permissions & 1024, 2048, 0) |
                                                             IIF(permissions & 2048, 4096, 0) |
                                                             IIF(permissions & 4096, 8192, 0);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 110: {
                if (version == 110) {
                    // Give back DataSave permission to users with DataDelete to fix migration error

                    bool success = db->RunMany(R"(
                        UPDATE dom_permissions SET permissions = permissions | IIF(permissions & 80 = 80, 32, 0);
                    )");
                    if (!success)
                        return false;
                }
            } [[fallthrough]];

            case 111: {
                bool success = db->RunMany(R"(
                    CREATE TABLE dom_settings (
                        key TEXT NOT NULL,
                        value TEXT
                    );

                    CREATE UNIQUE INDEX dom_settings_k ON dom_settings (key);

                    INSERT INTO dom_settings (key, value) VALUES ('Name', NULL);
                    INSERT INTO dom_settings (key, value) VALUES ('Title', NULL);
                    INSERT INTO dom_settings (key, value) VALUES ('DefaultLang', 'fr');
                    INSERT INTO dom_settings (key, value) VALUES ('ArchiveKey', NULL);
                )");
                if (!success)
                    return false;
            } // [[fallthrough]];

            static_assert(DomainVersion == 112);
        }

        if (!db->Run("INSERT INTO adm_migrations (version, build, time) VALUES (?, ?, ?)",
                     DomainVersion, FelixVersion, time))
            return false;
        if (!db->SetUserVersion(DomainVersion))
            return false;

        return true;
    });

    return success;
}

bool MigrateDomain(const Config &config)
{
    sq_Database db;

    if (!db.Open(config.database_filename, SQLITE_OPEN_READWRITE))
        return false;
    if (!MigrateDomain(&db, config.instances_directory))
        return false;
    if (!db.Close())
        return false;

    return true;
}

}
