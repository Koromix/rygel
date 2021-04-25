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

#include "../../core/libcc/libcc.hh"
#include "domain.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

const int DomainVersion = 18;
const int MaxInstancesPerDomain = 4096;

// Process-wide unique instance identifier 
static std::atomic_int64_t next_unique = 0;

bool DomainConfig::Validate() const
{
    bool valid = true;

    if (!enable_backups) {
        LogError("Domain backup key is not set");
        valid = false;
    }

    if (sms_sid) {
        if (!sms_token) {
            LogError("SMS token is not set");
            valid = false;
        }
        if (!sms_from) {
            LogError("SMS From setting is not set");
            valid = false;
        }
    }

    valid &= http.Validate();
    if (max_age < 0) {
        LogError("HTTP MaxAge must be >= 0");
        valid = false;
    }

    return valid;
}

const char *DomainConfig::GetInstanceFileName(const char *key, Allocator *alloc) const
{
    RG_ASSERT(instances_directory);

    HeapArray<char> buf(alloc);

    Fmt(&buf, "%1%/", instances_directory);
    for (Size i = 0; key[i]; i++) {
        char c = key[i];
        buf.Append(c != '/' ? c : '@');
    }
    buf.Append(".db");
    buf.Append(0);

    return buf.Leak().ptr;
}

bool LoadConfig(StreamReader *st, DomainConfig *out_config)
{
    DomainConfig config;

    Span<const char> root_directory = GetPathDirectory(st->GetFileName());

    IniParser ini(st);
    ini.PushLogFilter();
    RG_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section == "Paths" || prop.section == "Resources") {
                do {
                    if (prop.key == "DatabaseFile") {
                        config.database_filename = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
                    } else if (prop.key == "InstanceDirectory") {
                        config.instances_directory = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
                    } else if (prop.key == "TempDirectory") {
                        config.temp_directory = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
                    } else if (prop.key == "BackupDirectory") {
                        config.backup_directory = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "Data" || prop.section == "SQLite") {
                do {
                    if (prop.key == "BackupKey") {
                        RG_STATIC_ASSERT(crypto_box_curve25519xsalsa20poly1305_PUBLICKEYBYTES == 32);

                        size_t key_len;
                        int ret = sodium_base642bin(config.backup_key, RG_SIZE(config.backup_key),
                                                    prop.value.ptr, (size_t)prop.value.len, nullptr, &key_len,
                                                    nullptr, sodium_base64_VARIANT_ORIGINAL);
                        if (!ret && key_len == 32) {
                            config.enable_backups = true;
                        } else {
                            LogError("Malformed BackupKey value");
                            valid = false;
                        }
                    } else if (prop.key == "SynchronousFull") {
                        valid &= ParseBool(prop.value, &config.sync_full);
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "Session") {
                do {
                    if (prop.key == "DemoUser") {
                        config.demo_user = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "SMS") {
                do {
                    if (prop.key == "AuthSID") {
                        config.sms_sid = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else if (prop.key == "AuthToken") {
                        config.sms_token = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else if (prop.key == "From") {
                        config.sms_from = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "HTTP") {
                do {
                    if (prop.key == "SocketType" || prop.key == "IPStack") {
                        if (!OptionToEnum(SocketTypeNames, prop.value, &config.http.sock_type)) {
                            LogError("Unknown socket type '%1'", prop.value);
                            valid = false;
                        }
#ifndef _WIN32
                    } else if (prop.key == "UnixPath") {
                        config.http.unix_path = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
#endif
                    } else if (prop.key == "Port") {
                        valid &= ParseInt(prop.value, &config.http.port);
                    } else if (prop.key == "MaxConnections") {
                        valid &= ParseInt(prop.value, &config.http.max_connections);
                    } else if (prop.key == "IdleTimeout") {
                        valid &= ParseInt(prop.value, &config.http.idle_timeout);
                    } else if (prop.key == "Threads") {
                        valid &= ParseInt(prop.value, &config.http.threads);
                    } else if (prop.key == "AsyncThreads") {
                        valid &= ParseInt(prop.value, &config.http.async_threads);
                    } else if (prop.key == "MaxAge") {
                        valid &= ParseInt(prop.value, &config.max_age);
                    } else if (prop.key == "TrustXRealIP") {
                        valid &= ParseBool(prop.value, &config.http.use_xrealip);
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else {
                LogError("Unknown section '%1'", prop.section);
                while (ini.NextInSection(&prop));
                valid = false;
            }
        }
    }
    if (!ini.IsValid() || !valid)
        return false;

    // Default values
    if (!config.database_filename) {
        config.database_filename = NormalizePath("goupile.db", root_directory, &config.str_alloc).ptr;
    }
    if (!config.instances_directory) {
        config.instances_directory = NormalizePath("instances", root_directory, &config.str_alloc).ptr;
    }
    if (!config.temp_directory) {
        config.temp_directory = NormalizePath("tmp", root_directory, &config.str_alloc).ptr;
    }
    if (!config.backup_directory) {
        config.backup_directory = NormalizePath("backup", root_directory, &config.str_alloc).ptr;
    }

    std::swap(*out_config, config);
    return true;
}

bool LoadConfig(const char *filename, DomainConfig *out_config)
{
    StreamReader st(filename);
    return LoadConfig(&st, out_config);
}

bool DomainHolder::Open(const char *filename)
{
    RG_DEFER_N(err_guard) { Close(); };
    Close();

    // Load config and database
    if (!LoadConfig(filename, &config))
        return false;
    if (!db.Open(config.database_filename, SQLITE_OPEN_READWRITE))
        return false;
    if (!db.SetSynchronousFull(config.sync_full))
        return false;

    // Check schema version
    {
        int version;
        if (!db.GetUserVersion(&version))
            return false;

        if (version > DomainVersion) {
            LogError("Domain schema is too recent (%1, expected %2)", version, DomainVersion);
            return false;
        } else if (version < DomainVersion) {
            LogError("Domain schema is outdated");
            return false;
        }
    }

    // XXX: Check that temp_directory and backup_directory are one the same volume,
    // because we might want to rename from one to the other atomically.
    if (!MakeDirectory(config.temp_directory, false))
        return false;
    if (!MakeDirectory(config.backup_directory, false))
        return false;

    err_guard.Disable();
    return true;
}

void DomainHolder::Close()
{
    db.Close();
    config = {};

    // This is called when goupile exits and we don't really need the lock
    // at this point, but take if for consistency.
    std::lock_guard<std::shared_mutex> lock_excl(mutex);

    for (Size i = instances.len - 1; i >= 0; i--) {
        InstanceHolder *instance = instances[i];
        delete instance;
    }
    instances.Clear();
    instances_map.Clear();
}

bool DomainHolder::Sync()
{
    BlockAllocator temp_alloc;

    struct StartInfo {
        const char *instance_key;
        const char *master_key;
        int64_t generation;
        InstanceHolder *prev_instance;
    };

    int64_t prev_unique = next_unique;

    HeapArray<InstanceHolder *> new_instances;
    HashTable<Span<const char>, InstanceHolder *> new_map;
    HeapArray<StartInfo> registry_start;
    HeapArray<InstanceHolder *> registry_unload;
    {
        std::shared_lock<std::shared_mutex> lock_shr(mutex);
        Size offset = 0;

        sq_Statement stmt;
        if (!db.Prepare(R"(WITH RECURSIVE rec (instance, master, generation) AS (
                               SELECT instance, master, generation FROM dom_instances WHERE master IS NULL
                               UNION ALL
                               SELECT i.instance, i.master, i.generation FROM dom_instances i, rec WHERE i.master = rec.instance
                               ORDER BY 2 DESC, 1
                           )
                           SELECT instance, master, generation FROM rec)", &stmt))
            return false;

        while (stmt.Next()) {
            Span<const char> instance_key = (const char *)sqlite3_column_text(stmt, 0);
            const char *master_key = (const char *)sqlite3_column_text(stmt, 1);
            int64_t generation = sqlite3_column_int64(stmt, 2);

            for (;;) {
                InstanceHolder *instance = (offset < instances.len) ? instances[offset] : nullptr;
                int cmp = instance ? CmpStr(instance->key, instance_key) : 1;

                if (cmp < 0) {
                    registry_unload.Append(instance);
                    offset++;
                } else if (!cmp) {
                    if (instance->generation == generation) {
                        new_instances.Append(instance);
                        new_map.Set(instance);
                    } else {
                        StartInfo start = {};

                        start.instance_key = DuplicateString(instance_key, &temp_alloc).ptr;
                        start.master_key = master_key ? DuplicateString(master_key, &temp_alloc).ptr : nullptr;
                        start.generation = generation;
                        start.prev_instance = instance;

                        registry_start.Append(start);
                    }

                    offset++;
                    break;
                } else {
                    StartInfo start = {};

                    start.instance_key = DuplicateString(instance_key, &temp_alloc).ptr;
                    start.master_key = master_key ? DuplicateString(master_key, &temp_alloc).ptr : nullptr;
                    start.generation = generation;

                    registry_start.Append(start);

                    break;
                }
            }
        }
        if (!stmt.IsValid())
            return false;

        while (offset < instances.len) {
            InstanceHolder *instance = instances[offset];
            registry_unload.Append(instance);
            offset++;
        }
    }

    // Most calls should follow this path
    if (!registry_start.len && !registry_unload.len)
        return true;

    std::unique_lock<std::shared_mutex> lock_excl(mutex);
    bool complete = true;

    // Drop removed instances (if any)
    for (Size i = registry_unload.len - 1; i >= 0; i--) {
        InstanceHolder *instance = registry_unload[i];

        while (instance->master->refcount) {
            WaitDelay(100);
        }

        if (instance->master != instance) {
            InstanceHolder *master = instance->master;

            Size remove_idx = std::find(master->slaves.begin(), master->slaves.end(), instance) - master->slaves.ptr;
            RG_ASSERT(remove_idx < master->slaves.len);

            memmove(master->slaves.ptr + remove_idx, master->slaves.ptr + remove_idx + 1,
                    (master->slaves.len - remove_idx - 1) * RG_SIZE(*master->slaves.ptr));
            master->slaves.len--;

            if (master->unique < prev_unique) {
                master->unique = next_unique++;
            }
        }

        delete instance;
    }

    // Start new instances
    for (const StartInfo &start: registry_start) {
        if (new_instances.len >= MaxInstancesPerDomain) {
            LogError("Too many instances on this domain");
            complete = false;
            continue;
        }

        InstanceHolder *master;
        if (start.master_key) {
            master = new_map.FindValue(start.master_key, nullptr);

            if (!master) {
                LogError("Cannot open instance '%1' because master is not available", start.instance_key);
                complete = false;
                continue;
            }
        } else {
            master = nullptr;
        }

        const char *filename = config.GetInstanceFileName(start.instance_key, &temp_alloc);
        InstanceHolder *instance = new InstanceHolder();
        RG_DEFER_N(instance_guard) { delete instance; };

        if (!instance->Open(next_unique++, master, start.instance_key, filename)) {
            complete = false;
            continue;
        }
        if (!instance->db.SetSynchronousFull(config.sync_full)) {
            complete = false;
            continue;
        }
        instance->generation = start.generation;
        instance_guard.Disable();

        new_instances.Append(instance);
        new_map.Set(instance);

        if (start.prev_instance) {
            InstanceHolder *prev_instance = start.prev_instance;
            RG_ASSERT(prev_instance->key == instance->key);

            while (prev_instance->refcount) {
                WaitDelay(100);
            }

            // Fix pointers to previous instance
            if (prev_instance->master != prev_instance) {
                for (Size i = 0; i < prev_instance->master->slaves.len; i++) {
                    InstanceHolder *slave = prev_instance->master->slaves[i];

                    if (slave == prev_instance) {
                        prev_instance->master->slaves[i] = instance;
                        break;
                    }
                }
            }
            for (InstanceHolder *slave: prev_instance->slaves) {
                slave->master = instance;
                instance->slaves.Append(slave);
            }

            delete prev_instance;
        } else if (master) {
            while (master->refcount) {
                WaitDelay(100);
            }

            if (master->unique >= prev_unique) {
                // Fast path for new masters
                master->slaves.Append(instance);
            } else {
                Size insert_idx = std::find_if(master->slaves.begin(), master->slaves.end(),
                                               [&](InstanceHolder *slave) { return CmpStr(slave->key, instance->key) > 0; }) - master->slaves.ptr;

                // Add instance to parent list
                master->slaves.Grow(1);
                memmove(master->slaves.ptr + insert_idx + 1, master->slaves.ptr + insert_idx,
                        (master->slaves.len - insert_idx) * RG_SIZE(*master->slaves.ptr));
                master->slaves.ptr[insert_idx] = instance;
                master->slaves.len++;

                master->unique = next_unique++;
            }
        }
    }

    // Commit changes
    std::sort(new_instances.begin(), new_instances.end(),
              [](InstanceHolder *instance1, InstanceHolder *instance2) {
        return CmpStr(instance1->key, instance2->key) < 0;
    });
    std::swap(instances, new_instances);
    std::swap(instances_map, new_map);

    return complete;
}

bool DomainHolder::Checkpoint()
{
    std::shared_lock<std::shared_mutex> lock_shr(mutex);

    bool success = true;

    success &= db.Checkpoint();
    for (InstanceHolder *instance: instances) {
        success &= instance->Checkpoint();
    }

    return success;
}

Span<InstanceHolder *> DomainHolder::LockInstances()
{
    mutex.lock_shared();
    return instances;
}

void DomainHolder::UnlockInstances()
{
    mutex.unlock_shared();
}

Size DomainHolder::CountInstances() const
{
    std::shared_lock<std::shared_mutex> lock_shr(mutex);
    return instances.len;
}

InstanceHolder *DomainHolder::Ref(Span<const char> key)
{
    std::shared_lock<std::shared_mutex> lock_shr(mutex);

    InstanceHolder *instance = instances_map.FindValue(key, nullptr);

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
                        admin INTEGER CHECK(admin IN (0, 1)) NOT NULL
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
                if (version) {
                    sq_Statement stmt;
                    if (!db->Prepare("INSERT INTO dom_instances (instance) VALUES (?)", &stmt))
                        return false;

                    EnumStatus status = EnumerateDirectory(instances_directory, "*.db", -1,
                                                           [&](const char *filename, FileType) {
                        Span<const char> key = SplitStrReverseAny(filename, RG_PATH_SEPARATORS);
                        key = SplitStr(key, '.');

                        stmt.Reset();
                        sqlite3_bind_text(stmt, 1, key.ptr, (int)key.len, SQLITE_STATIC);

                        return stmt.Run();
                    });
                    if (status != EnumStatus::Done)
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
                        admin INTEGER CHECK(admin IN (0, 1)) NOT NULL,
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

                while (stmt.Next()) {
                    int64_t rowid = sqlite3_column_int64(stmt, 0);

                    // Create passport key
                    char passport[45];
                    {
                        uint8_t buf[32];
                        randombytes_buf(&buf, RG_SIZE(buf));
                        sodium_bin2base64(passport, RG_SIZE(passport), buf, RG_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
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
                        admin INTEGER CHECK(admin IN (0, 1)) NOT NULL,
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
                        admin INTEGER CHECK(admin IN (0, 1)) NOT NULL,
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
                        master TEXT GENERATED ALWAYS AS (iif(instr(instance, '@') > 0, substr(instance, 1, instr(instance, '@') - 1), NULL)) STORED
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
                        SELECT iif(master IS NULL, instance, master || '@' || instance) FROM dom_instances_BAK ORDER BY master ASC NULLS FIRST;
                    INSERT INTO dom_permissions (userid, instance, permissions)
                        SELECT p.userid, iif(i.master IS NULL, i.instance, i.master || '@' || i.instance), p.permissions FROM dom_permissions_BAK p
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
                        master TEXT GENERATED ALWAYS AS (iif(instr(instance, '/') > 0, substr(instance, 1, instr(instance, '/') - 1), NULL)) STORED
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
                    UPDATE dom_permissions SET permissions = iif(permissions & 1, 1, 0) |
                                                             iif(permissions & 8, 2, 0) |
                                                             iif(permissions & 1, 4, 0) |
                                                             iif(permissions & 1, 8, 0) |
                                                             iif(permissions & 4, 16, 0) |
                                                             iif(permissions & 2, 32, 0) |
                                                             iif(permissions & 4, 64, 0) |
                                                             iif(permissions & 32, 128, 0) |
                                                             iif(permissions & 64, 256, 0);
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
                        master TEXT GENERATED ALWAYS AS (iif(instr(instance, '/') > 0, substr(instance, 1, instr(instance, '/') - 1), NULL)) STORED
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
                    UPDATE dom_permissions SET permissions = iif(permissions & 1, 1, 0) |
                                                             iif(permissions & 2, 2, 0) |
                                                             iif(permissions & 4, 4, 0) |
                                                             iif(permissions & 8, 8, 0) |
                                                             iif(permissions & 16, 16, 0) |
                                                             iif(permissions & 16, 32, 0) |
                                                             iif(permissions & 32, 64, 0) |
                                                             iif(permissions & 64, 128, 0) |
                                                             iif(permissions & 64, 256, 0) |
                                                             iif(permissions & 128, 512, 0) |
                                                             iif(permissions & 256, 1024, 0);
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
                        master TEXT GENERATED ALWAYS AS (iif(instr(instance, '/') > 0, substr(instance, 1, instr(instance, '/') - 1), NULL)) STORED
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
            } // [[fallthrough]];

            RG_STATIC_ASSERT(DomainVersion == 18);
        }

        int64_t time = GetUnixTime();
        if (!db->Run("INSERT INTO adm_migrations (version, build, time) VALUES (?, ?, ?)",
                     DomainVersion, FelixVersion, time))
            return false;
        if (!db->SetUserVersion(DomainVersion))
            return false;

        return true;
    });

    return success;
}

bool MigrateDomain(const DomainConfig &config)
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
