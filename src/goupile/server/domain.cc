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

const int DomainVersion = 13;
const int MaxInstancesPerDomain = 4096;

bool DomainConfig::Validate() const
{
    bool valid = true;

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
            if (prop.section == "Resources") {
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
            } else if (prop.section == "SQLite") {
                do {
                    if (prop.key == "SynchronousFull") {
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

    for (InstanceHolder *instance: instances) {
        delete instance;
    }
    instances.Clear();
    instances_map.Clear();
}

// Can be called multiple times, from main thread only
bool DomainHolder::Sync()
{
    BlockAllocator temp_alloc;

    std::lock_guard<std::shared_mutex> lock_excl(mutex);

    HeapArray<InstanceHolder *> new_instances;
    HashTable<Span<const char>, InstanceHolder *> new_map;
    synced = true;

    // Start new instances (if any)
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(WITH RECURSIVE rec (instance, master) AS (
                               SELECT instance, master FROM dom_instances WHERE master IS NULL
                               UNION ALL
                               SELECT i.instance, i.master FROM dom_instances i, rec WHERE i.master = rec.instance
                               ORDER BY 2 DESC, 1
                           )
                           SELECT r.instance, r.master,
                                  (SELECT COUNT(*) FROM rec r2 WHERE r2.master = r.instance) AS slaves FROM rec r)", &stmt))
            return false;

        bool warned = false;

        while (stmt.Next()) {
            const char *key = (const char *)sqlite3_column_text(stmt, 0);
            const char *master_key = (const char *)sqlite3_column_text(stmt, 1);
            int slaves = sqlite3_column_int(stmt, 2);

            key = DuplicateString(key, &temp_alloc).ptr;
            InstanceHolder *instance = instances_map.FindValue(key, nullptr);

            // Should we reload this (happens when configuration changes)?
            if (instance && (instance->reload || instance->master->reload)) {
                instance->reload = true;

                if (!instance->refcount) {
                    instances_map.Remove(key);
                    instance->unload = true;

                    instance = nullptr;
                } else {
                    // We will try again later
                    synced = false;
                }
            }

            if (instance) {
                new_instances.Append(instance);
                new_map.Set(instance);
            } else if (new_instances.len < MaxInstancesPerDomain) {
                InstanceHolder *master;
                if (master_key) {
                    master = new_map.FindValue(master_key, nullptr);
                    if (!master) {
                        LogError("Cannot open instance '%1' because master instance is not available", key);
                        continue;
                    }
                } else {
                    master = nullptr;
                }

                const char *filename = config.GetInstanceFileName(key, &temp_alloc);
                instance = new InstanceHolder();

                if (instance->Open(key, filename, master, config.sync_full)) {
                    new_instances.Append(instance);
                    new_map.Set(instance);
                } else {
                    delete instance;
                    synced = false;
                }
            } else if (!warned) {
                LogError("Too many instances on this domain, maximum = %1", MaxInstancesPerDomain);
                warned = true;
            }

            instance->slaves.store(slaves, std::memory_order_relaxed);
        }
        synced &= stmt.IsValid();
    }

    // Drop removed instances (if any)
    for (InstanceHolder *instance: instances) {
        if (!instance->unload && !new_map.Find(instance->key)) {
            instances_map.Remove(instance->key);
            instance->unload = true;
        }

        if (instance->unload) {
            if (!instance->refcount) {
                delete instance;
            } else {
                // We will try again later
                synced = false;
            }
        }
    }

    std::swap(instances, new_instances);
    std::swap(instances_map, new_map);

    return synced;
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

InstanceHolder *DomainHolder::Ref(Span<const char> key, bool *out_reload)
{
    std::shared_lock<std::shared_mutex> lock_shr(mutex);

    InstanceHolder *instance = instances_map.FindValue(key, nullptr);

    if (!instance) {
        if (out_reload) {
            *out_reload = false;
        }
        return nullptr;
    } else if (instance->reload.load(std::memory_order_relaxed)) {
        if (out_reload) {
            *out_reload = true;
        }
        return nullptr;
    }

    if (instance->master != instance) {
        instance->master->refcount++;
    }
    instance->refcount++;

    return instance;
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

    LogInfo("Migrating domain: %1 to %2", version + 1, DomainVersion);

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
                // This migration is incomplete and does not rename slave instance database files

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
            } // [[fallthrough]];

            RG_STATIC_ASSERT(DomainVersion == 13);
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
