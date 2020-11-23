// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "domain.hh"

namespace RG {

const int DomainVersion = 3;

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
                        if (prop.value == "Dual") {
                            config.http.sock_type = SocketType::Dual;
                        } else if (prop.value == "IPv4") {
                            config.http.sock_type = SocketType::IPv4;
                        } else if (prop.value == "IPv6") {
                            config.http.sock_type = SocketType::IPv6;
                        } else {
                            LogError("Unknown IP version '%1'", prop.value);
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

bool DomainData::Open(const char *filename)
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

    err_guard.Disable();
    return true;
}

void DomainData::Close()
{
    db.Close();
    config = {};
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

    sq_TransactionResult ret = db->Transaction([&]() {
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
                    return sq_TransactionResult::Error;
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
                    return sq_TransactionResult::Error;
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
                    return sq_TransactionResult::Error;

                // Insert existing instances
                if (version) {
                    sq_Statement stmt;
                    if (!db->Prepare("INSERT INTO dom_instances (instance) VALUES (?);", &stmt))
                        return sq_TransactionResult::Error;

                    EnumStatus status = EnumerateDirectory(instances_directory, "*.db", -1,
                                                           [&](const char *filename, FileType) {
                        Span<const char> key = SplitStrReverseAny(filename, RG_PATH_SEPARATORS);
                        key = SplitStr(key, '.');

                        stmt.Reset();
                        sqlite3_bind_text(stmt, 1, key.ptr, (int)key.len, SQLITE_STATIC);

                        return stmt.Run();
                    });
                    if (status != EnumStatus::Done)
                        return sq_TransactionResult::Error;
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
                    return sq_TransactionResult::Error;
            } // [[fallthrough]];

            RG_STATIC_ASSERT(DomainVersion == 3);
        }

        int64_t time = GetUnixTime();
        if (!db->Run("INSERT INTO adm_migrations (version, build, time) VALUES (?, ?, ?)",
                     DomainVersion, FelixVersion, time))
            return sq_TransactionResult::Error;
        if (!db->SetUserVersion(DomainVersion))
            return sq_TransactionResult::Error;

        return sq_TransactionResult::Commit;
    });
    if (ret != sq_TransactionResult::Commit)
        return false;

    return true;
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
