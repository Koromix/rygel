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

const int DomainVersion = 23;
const int MaxInstancesPerDomain = 1024;
const int64_t FullSnapshotDelay = 86400 * 1000;

// Process-wide unique instance identifier 
static std::atomic_int64_t next_unique = 0;

bool DomainConfig::Validate() const
{
    bool valid = true;

    if (!title || !title[0]) {
        LogError("Missing domain title");
        valid = false;
    }
    if (!enable_archives) {
        LogError("Domain archive key is not set");
        valid = false;
    }
    valid &= http.Validate();
    valid &= !smtp.url || smtp.Validate();
    valid &= (sms.provider == sms_Provider::None) || sms.Validate();

    return valid;
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

bool LoadConfig(StreamReader *st, DomainConfig *out_config)
{
    DomainConfig config;

    config.config_filename = NormalizePath(st->GetFileName(), GetWorkingDirectory(), &config.str_alloc).ptr;

    Span<const char> root_directory = GetPathDirectory(config.config_filename);

    IniParser ini(st);
    ini.PushLogFilter();
    RG_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (prop.section == "Domain") {
                do {
                    if (prop.key == "Title") {
                        config.title = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "Paths" || prop.section == "Resources") {
                do {
                    if (prop.key == "DatabaseFile") {
                        config.database_filename = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
                    } else if (prop.key == "ArchiveDirectory" || prop.key == "BackupDirectory") {
                        config.archive_directory = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
                    } else if (prop.key == "SnapshotDirectory") {
                        config.snapshot_directory = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "Data" || prop.section == "SQLite") {
                do {
                    if (prop.key == "ArchiveKey" || prop.key == "BackupKey") {
                        RG_STATIC_ASSERT(crypto_box_curve25519xsalsa20poly1305_PUBLICKEYBYTES == 32);

                        size_t key_len;
                        int ret = sodium_base642bin(config.archive_key, RG_SIZE(config.archive_key),
                                                    prop.value.ptr, (size_t)prop.value.len, nullptr, &key_len,
                                                    nullptr, sodium_base64_VARIANT_ORIGINAL);
                        if (!ret && key_len == 32) {
                            config.enable_archives = true;
                        } else {
                            LogError("Malformed BackupKey value");
                            valid = false;
                        }
                    } else if (prop.key == "SynchronousFull") {
                        valid &= ParseBool(prop.value, &config.sync_full);
                    } else if (prop.key == "SnapshotHour") {
                        valid &= ParseInt(prop.value, &config.snapshot_hour);

                        if (ParseInt(prop.value, &config.snapshot_hour)) {
                            if (config.snapshot_hour < 0 || config.snapshot_hour > 23) {
                                LogError("SnapshotHour is outside of 0-23 (inclusive) range");
                                valid = false;
                            }
                        } else {
                            valid = false;
                        }
                    } else if (prop.key == "SnapshotZone") {
                        if (!OptionToEnum(TimeModeNames, prop.value, &config.snapshot_zone)) {
                            LogError("Unknown time mode '%1'", prop.value);
                            valid = false;
                        }
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
                    } else if (prop.key == "UnixPath") {
                        config.http.unix_path = NormalizePath(prop.value, root_directory, &config.str_alloc).ptr;
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
                    } else if (prop.key == "ClientAddress") {
                        if (!OptionToEnum(http_ClientAddressModeNames, prop.value, &config.http.client_addr_mode)) {
                            LogError("Unknown client address mode '%1'", prop.value);
                            valid = false;
                        }
                    } else if (prop.key == "RequireHost") {
                        config.require_host = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "SMTP") {
                do {
                    if (prop.key == "URL") {
                        config.smtp.url = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else if (prop.key == "Username") {
                        config.smtp.username = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else if (prop.key == "Password") {
                        config.smtp.password = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else if (prop.key == "From") {
                        config.smtp.from = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else {
                        LogError("Unknown attribute '%1'", prop.key);
                        valid = false;
                    }
                } while (ini.NextInSection(&prop));
            } else if (prop.section == "SMS") {
                do {
                    if (prop.key == "Provider") {
                        if (!OptionToEnum(sms_ProviderNames, prop.value, &config.sms.provider)) {
                            LogError("Unknown SMS provider '%1'", prop.value);
                            valid = false;
                        }
                    } else if (prop.key == "AuthID") {
                        config.sms.authid = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else if (prop.key == "AuthToken") {
                        config.sms.token = DuplicateString(prop.value, &config.str_alloc).ptr;
                    } else if (prop.key == "From") {
                        config.sms.from = DuplicateString(prop.value, &config.str_alloc).ptr;
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
    config.database_directory = DuplicateString(GetPathDirectory(config.database_filename), &config.str_alloc).ptr;
    config.instances_directory = NormalizePath("instances", root_directory, &config.str_alloc).ptr;
    config.tmp_directory = NormalizePath("tmp", root_directory, &config.str_alloc).ptr;
    if (!config.archive_directory) {
        config.archive_directory = NormalizePath("archives", root_directory, &config.str_alloc).ptr;
    }
    if (!config.snapshot_directory) {
        config.snapshot_directory = NormalizePath("snapshots", root_directory, &config.str_alloc).ptr;
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

    // XXX: Check that these directories are one the same volume,
    // because we might want to rename from one to the other atomically.
    if (!MakeDirectory(config.tmp_directory, false))
        return false;
    if (!MakeDirectory(config.archive_directory, false))
        return false;
    if (!MakeDirectory(config.snapshot_directory, false))
        return false;

    // Properly configure database
    if (!db.SetWAL(true))
        return false;
    if (!db.SetSynchronousFull(config.sync_full))
        return false;
    if (!db.SetSnapshotDirectory(config.snapshot_directory, FullSnapshotDelay))
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

    for (sq_Database *db: databases) {
        delete db;
    }
    databases.Clear();
}

bool DomainHolder::SyncAll(bool thorough)
{
    return Sync(nullptr, thorough);
}

bool DomainHolder::SyncInstance(const char *key)
{
    return Sync(key, true);
}

bool DomainHolder::Checkpoint()
{
    std::shared_lock<std::shared_mutex> lock_shr(mutex);

    Async async(-1, false);

    async.Run([&]() { return db.Checkpoint(); });
    for (InstanceHolder *instance: instances) {
        async.Run([instance]() { return instance->Checkpoint(); });
    }

    return async.Sync();
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

bool DomainHolder::Sync(const char *filter_key, bool thorough)
{
    BlockAllocator temp_alloc;

    struct StartInfo {
        const char *instance_key;
        const char *master_key;
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
        if (!db.Prepare(R"(WITH RECURSIVE rec (instance, master) AS (
                               SELECT instance, master FROM dom_instances WHERE master IS NULL
                               UNION ALL
                               SELECT i.instance, i.master FROM dom_instances i, rec WHERE i.master = rec.instance
                               ORDER BY 2 DESC, 1
                           )
                           SELECT instance, master FROM rec)", &stmt))
            return false;

        while (stmt.Step()) {
            Span<const char> instance_key = (const char *)sqlite3_column_text(stmt, 0);
            const char *master_key = (const char *)sqlite3_column_text(stmt, 1);

            for (;;) {
                InstanceHolder *instance = (offset < instances.len) ? instances[offset] : nullptr;

                int cmp = instance ? CmpStr(instance->key, instance_key) : 1;
                bool match = !filter_key || TestStr(filter_key, instance_key) ||
                                            (master_key && TestStr(filter_key, master_key));

                if (cmp < 0) {
                    if (match) {
                        registry_unload.Append(instance);
                    } else {
                        new_instances.Append(instance);
                        new_map.Set(instance);
                    }

                    offset++;
                } else if (!cmp) {
                    // Reload instance for thorough syncs or if the master instance is being
                    // reconfigured itself for some reason.
                    match &= thorough | (master_key && !new_map.Find(master_key));

                    if (match) {
                        StartInfo start = {};

                        start.instance_key = DuplicateString(instance_key, &temp_alloc).ptr;
                        start.master_key = master_key ? DuplicateString(master_key, &temp_alloc).ptr : nullptr;
                        start.prev_instance = instance;

                        registry_start.Append(start);
                    } else {
                        new_instances.Append(instance);
                        new_map.Set(instance);
                    }

                    offset++;
                    break;
                } else {
                    if (match) {
                        StartInfo start = {};

                        start.instance_key = DuplicateString(instance_key, &temp_alloc).ptr;
                        start.master_key = master_key ? DuplicateString(master_key, &temp_alloc).ptr : nullptr;

                        registry_start.Append(start);
                    } else if (instance) {
                        new_instances.Append(instance);
                        new_map.Set(instance);
                    }

                    break;
                }
            }
        }
        if (!stmt.IsValid())
            return false;

        while (offset < instances.len) {
            InstanceHolder *instance = instances[offset];
            bool match = !filter_key || TestStr(filter_key, instance->key) ||
                                        TestStr(filter_key, instance->master->key);

            if (match) {
                registry_unload.Append(instance);
            } else {
                new_instances.Append(instance);
                new_map.Set(instance);
            }

            offset++;
        }
    }

    // Most (non-thorough) calls should follow this path
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

            memmove_safe(master->slaves.ptr + remove_idx, master->slaves.ptr + remove_idx + 1,
                         (master->slaves.len - remove_idx - 1) * RG_SIZE(*master->slaves.ptr));
            master->slaves.len--;

            if (master->unique < prev_unique) {
                master->unique = next_unique++;
            }
        }

        LogDebug("Close instance '%1' @%2", instance->key, instance->unique);
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

        InstanceHolder *instance = new InstanceHolder();
        int64_t unique = next_unique++;
        RG_DEFER_N(instance_guard) { delete instance; };

        if (start.prev_instance) {
            InstanceHolder *prev_instance = start.prev_instance;

            while (prev_instance->master->refcount) {
                WaitDelay(100);
            }

            LogDebug("Reconfigure instance '%1' @%2", start.instance_key, unique);

            if (!instance->Open(unique, master, start.instance_key, prev_instance->db)) {
                complete = false;
                continue;
            }
        } else {
            sq_Database *db = new sq_Database;
            RG_DEFER_N(db_guard) { delete db; };

            const char *db_filename = MakeInstanceFileName(config.instances_directory, start.instance_key, &temp_alloc);

            LogDebug("Open database '%1'", db_filename);
            if (!db->Open(db_filename, SQLITE_OPEN_READWRITE)) {
                complete = false;
                continue;
            }
            if (!db->SetWAL(true)) {
                complete = false;
                continue;
            }
            if (!db->SetSynchronousFull(config.sync_full)) {
                complete = false;
                continue;
            }
            if (!db->SetSnapshotDirectory(config.snapshot_directory, FullSnapshotDelay)) {
                complete = false;
                continue;
            }

            LogDebug("Open instance '%1' @%2", start.instance_key, unique);

            if (!instance->Open(unique, master, start.instance_key, db)) {
                complete = false;
                continue;
            }

            db_guard.Disable();
            databases.Append(db);
        }

        instance_guard.Disable();
        new_instances.Append(instance);
        new_map.Set(instance);

        if (start.prev_instance) {
            InstanceHolder *prev_instance = start.prev_instance;
            RG_ASSERT(prev_instance->key == instance->key);

            while (prev_instance->master->refcount) {
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
                memmove_safe(master->slaves.ptr + insert_idx + 1, master->slaves.ptr + insert_idx,
                             (master->slaves.len - insert_idx) * RG_SIZE(*master->slaves.ptr));
                master->slaves.ptr[insert_idx] = instance;
                master->slaves.len++;

                master->unique = next_unique++;
            }
        }
    }

    // Close unused databases
    {
        HashSet<const void *> used_databases;

        for (const InstanceHolder *instance: new_instances) {
            used_databases.Set(instance->db);
        }

        Size j = 0;
        for (Size i = 0; i < databases.len; i++) {
            sq_Database *db = databases[i];
            databases[j] = db;

            if (used_databases.Find(db)) {
                j++;
            } else {
                const char *filename = sqlite3_db_filename(*db, "main");
                LogDebug("Close unused database '%1'", filename);

                complete &= db->Close();
                delete db;
            }
        }
        databases.RemoveFrom(j);
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
                if (version && instances_directory) {
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

                while (stmt.Step()) {
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
                    UPDATE dom_permissions SET permissions = iif(permissions & 1, 1, 0) |
                                                             iif(permissions & 2, 2, 0) |
                                                             iif(permissions & 4, 4, 0) |
                                                             iif(permissions & 8, 8, 0) |
                                                             iif(permissions & 16, 16, 0) |
                                                             iif(permissions & 128, 32, 0) |
                                                             iif(permissions & 512, 64, 0) |
                                                             iif(permissions & 1024, 128, 0);
                )");
                if (!success)
                    return false;
            } [[fallthrough]];

            case 20: {
                bool success = db->RunMany(R"(
                    UPDATE dom_permissions SET permissions = permissions |
                                                             iif(permissions & 1, 256, 0);
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
                        admin INTEGER CHECK(admin IN (0, 1)) NOT NULL,
                        local_key TEXT NOT NULL,
                        totp_required INTEGER CHECK(admin IN (0, 1)) NOT NULL,
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
                        SELECT userid, instance, permissions FROM dom_permissions_BAK;

                    DROP TABLE dom_permissions_BAK;
                    DROP TABLE dom_users_BAK;
                )");
                if (!success)
                    return false;
            } // [[fallthrough]];

            RG_STATIC_ASSERT(DomainVersion == 23);
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
