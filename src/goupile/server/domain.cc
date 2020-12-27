// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "domain.hh"

namespace RG {

const int DomainVersion = 4;

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

    const char *filename = Fmt(alloc, "%1%/%2.db", instances_directory, key).ptr;
    return filename;
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

    if (!Sync())
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
    std::lock_guard<std::shared_mutex> lock(mutex);

    for (InstanceHolder *instance: instances) {
        delete instance;
    }
    instances.Clear();
    instances_map.Clear();
}

void DomainHolder::InitAssets()
{
    std::shared_lock<std::shared_mutex> lock(mutex);

    for (InstanceHolder *instance: instances) {
        instance->InitAssets();
    }
}

// Can be called multiple times, from main thread only
bool DomainHolder::Sync()
{
    BlockAllocator temp_alloc;

    std::lock_guard<std::shared_mutex> lock(mutex);

    HeapArray<InstanceHolder *> new_instances;
    HashTable<Span<const char>, InstanceHolder *> new_map;
    synced = true;

    // Start new instances (if any)
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT instance FROM dom_instances ORDER BY instance;", &stmt))
            return false;

        while (stmt.Next()) {
            const char *key = (const char *)sqlite3_column_text(stmt, 0);
            key = DuplicateString(key, &temp_alloc).ptr;

            InstanceHolder *instance = instances_map.FindValue(key, nullptr);

            // Should we reload this (happens when configuration changes)?
            if (instance && instance->reload) {
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
            } else {
                const char *filename = config.GetInstanceFileName(key, &temp_alloc);
                instance = new InstanceHolder();

                if (instance->Open(key, filename)) {
                    new_instances.Append(instance);
                    new_map.Set(instance);
                } else {
                    delete instance;
                    synced = false;
                }
            }
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

InstanceHolder *DomainHolder::Ref(Span<const char> key, bool *out_reload)
{
    std::shared_lock<std::shared_mutex> lock(mutex);

    InstanceHolder *instance = instances_map.FindValue(key, nullptr);

    if (!instance) {
        if (out_reload) {
            *out_reload = false;
        }
        return nullptr;
    } else if (instance->reload) {
        if (out_reload) {
            *out_reload = true;
        }
        return nullptr;
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
                bool success = db->Run(R"(
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
                bool success = db->Run(R"(
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
                bool success = db->Run(R"(
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
                    if (!db->Prepare("INSERT INTO dom_instances (instance) VALUES (?);", &stmt))
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

                success = db->Run(R"(
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
                if (!db->Run("UPDATE dom_permissions SET permissions = 127 WHERE permissions == 63;"))
                    return false;
            } // [[fallthrough]];

            RG_STATIC_ASSERT(DomainVersion == 4);
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
