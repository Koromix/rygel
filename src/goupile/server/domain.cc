// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "domain.hh"

namespace RG {

static const int DomainVersion = 2;

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
                    if (prop.key == "IPStack") {
                        if (prop.value == "Dual") {
                            config.http.ip_stack = IPStack::Dual;
                        } else if (prop.value == "IPv4") {
                            config.http.ip_stack = IPStack::IPv4;
                        } else if (prop.value == "IPv6") {
                            config.http.ip_stack = IPStack::IPv6;
                        } else {
                            LogError("Unknown IP version '%1'", prop.value);
                        }
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

bool MigrateDomain(sq_Database *db)
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
            } // [[fallthrough]];

            RG_STATIC_ASSERT(DomainVersion == 2);
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

bool MigrateDomain(const char *filename)
{
    sq_Database db;

    if (!db.Open(filename, SQLITE_OPEN_READWRITE))
        return false;
    if (!MigrateDomain(&db))
        return false;
    if (!db.Close())
        return false;

    return true;
}

}
