// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "admin.hh"
#include "domain.hh"
#include "goupile.hh"
#include "instance.hh"
#include "user.hh"
#include "../../core/libwrap/json.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

#ifdef _WIN32
    typedef unsigned int uid_t;
    typedef unsigned int gid_t;
#else
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <pwd.h>
    #include <unistd.h>
#endif

namespace RG {

static const char *DefaultConfig =
R"([Resources]
# DatabaseFile = goupile.db
# InstanceDirectory = instances
# TempDirectory = tmp
# BackupDirectory = backup

[Session]
# DemoUser =

[HTTP]
# SocketType = Dual
# Port = 8888
# Threads =
# AsyncThreads =
# TrustXRealIP = Off
)";

static bool CheckInstanceKey(Span<const char> key)
{
    const auto test_char = [](char c) { return (c >= 'a' && c <= 'z') || IsAsciiDigit(c) || c == '_'; };

    if (!key.len) {
        LogError("Instance key cannot be empty");
        return false;
    }
    if (key.len > 64) {
        LogError("Instance key cannot have more than 64 characters");
        return false;
    }
    if (!std::all_of(key.begin(), key.end(), test_char)) {
        LogError("Instance key must only contain lowercase alphanumeric or '_' characters");
        return false;
    }
    if (key == "admin") {
        LogError("The following instance keys are not allowed: admin");
        return false;
    }

    return true;
}

static bool CheckUserName(Span<const char> username)
{
    const auto test_char = [](char c) { return (c >= 'a' && c <= 'z') || IsAsciiDigit(c) || c == '_' || c == '.'; };

    if (!username.len) {
        LogError("Username cannot be empty");
        return false;
    }
    if (username.len > 64) {
        LogError("Username cannot be have more than 64 characters");
        return false;
    }
    if (!std::all_of(username.begin(), username.end(), test_char)) {
        LogError("Username must only contain lowercase alphanumeric, '_' or '.' characters");
        return false;
    }

    return true;
}

#ifndef _WIN32
static bool FindPosixUser(const char *username, uid_t *out_uid, gid_t *out_gid)
{
    struct passwd pwd_buf;
    HeapArray<char> buf;
    struct passwd *pwd;

again:
    buf.Grow(1024);
    buf.len += 1024;

    int ret = getpwnam_r(username, &pwd_buf, buf.ptr, buf.len, &pwd);
    if (ret != 0) {
        if (ret == ERANGE)
            goto again;

        LogError("getpwnam('%1') failed: %2", username, strerror(errno));
        return false;
    }
    if (!pwd) {
        LogError("Could not find system user '%1'", username);
        return false;
    }

    *out_uid = pwd->pw_uid;
    *out_gid = pwd->pw_gid;
    return true;
}
#endif

static bool HashPassword(Span<const char> password, char out_hash[crypto_pwhash_STRBYTES])
{
    if (crypto_pwhash_str(out_hash, password.ptr, password.len,
                          crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        LogError("Failed to hash password");
        return false;
    }

    return true;
}

static bool ChangeFileOwner(const char *filename, uid_t uid, gid_t gid)
{
#ifndef _WIN32
    if (chown(filename, uid, gid) < 0) {
        LogError("Failed to change '%1' owner: %2", filename, strerror(errno));
        return false;
    }
#endif

    return true;
}

static bool CreateInstance(DomainHolder *domain, const char *instance_key,
                           const char *title, const char *default_user,
                           bool *out_conflict = nullptr)
{
    BlockAllocator temp_alloc;

    if (out_conflict) {
        *out_conflict = false;
    }

    // Check for existing instance
    {
        sq_Statement stmt;
        if (!domain->db.Prepare("SELECT instance FROM dom_instances WHERE instance = ?1", &stmt))
            return false;
        sqlite3_bind_text(stmt, 1, instance_key, -1, SQLITE_STATIC);

        if (stmt.Next()) {
            LogError("Instance '%1' already exists", instance_key);
            if (out_conflict) {
                *out_conflict = true;
            }
            return false;
        } else if (!stmt.IsValid()) {
            return false;
        }
    }

    const char *database_filename = domain->config.GetInstanceFileName(instance_key, &temp_alloc);
    if (TestFile(database_filename)) {
        LogError("Database '%1' already exists (old deleted instance?)", database_filename);
        if (out_conflict) {
            *out_conflict = true;
        }
        return false;
    }
    RG_DEFER_N(db_guard) { UnlinkFile(database_filename); };

    uid_t owner_uid = 0;
    gid_t owner_gid = 0;
#ifndef _WIN32
    {
        struct stat sb;
        if (stat(domain->config.database_filename, &sb) < 0) {
            LogError("Failed to stat '%1': %2", database_filename, strerror(errno));
            return false;
        }

        owner_uid = sb.st_uid;
        owner_gid = sb.st_gid;
    }
#endif

    // Create instance database
    sq_Database db;
    if (!db.Open(database_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return false;
    if (!MigrateInstance(&db))
        return false;
    if (!ChangeFileOwner(database_filename, owner_uid, owner_gid))
        return false;

    // Set default settings
    {
        const char *sql = "UPDATE fs_settings SET value = ?2 WHERE key = ?1";
        bool success = true;

        success &= db.Run(sql, "Title", title);

        if (!success)
            return false;
    }

    // Create default files
    {
        sq_Statement stmt;
        if (!db.Prepare(R"(INSERT INTO fs_files (active, filename, mtime, blob, compression, sha256, size)
                           VALUES (1, ?1, ?2, ?3, ?4, ?5, ?6))", &stmt))
            return false;

        // Use same modification time for all files
        int64_t mtime = GetUnixTime();

        for (const AssetInfo &asset: GetPackedAssets()) {
            if (StartsWith(asset.name, "src/goupile/demo/")) {
                const char *filename = asset.name + 17;

                HeapArray<uint8_t> gzip;
                char sha256[65];
                Size total_len = 0;
                {
                    StreamReader reader(asset.data, "<asset>", asset.compression_type);
                    StreamWriter writer(&gzip, "<gzip>", CompressionType::Gzip);

                    crypto_hash_sha256_state state;
                    crypto_hash_sha256_init(&state);

                    while (!reader.IsEOF()) {
                        LocalArray<uint8_t, 16384> buf;
                        buf.len = reader.Read(buf.data);
                        if (buf.len < 0)
                            return false;
                        total_len += buf.len;

                        writer.Write(buf);
                        crypto_hash_sha256_update(&state, buf.data, buf.len);
                    }

                    bool success = writer.Close();
                    RG_ASSERT(success);

                    uint8_t hash[crypto_hash_sha256_BYTES];
                    crypto_hash_sha256_final(&state, hash);
                    FormatSha256(hash, sha256);
                }

                stmt.Reset();
                sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);
                sqlite3_bind_int64(stmt, 2, mtime);
                sqlite3_bind_blob64(stmt, 3, gzip.ptr, gzip.len, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 4, "Gzip", -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 5, sha256, -1, SQLITE_STATIC);
                sqlite3_bind_int64(stmt, 6, total_len);

                if (!stmt.Run())
                    return false;
            }
        }
    }

    if (!db.Close())
        return false;

    bool success = domain->db.Transaction([&]() {
        uint32_t permissions = (1u << RG_LEN(UserPermissionNames)) - 1;

        if (!domain->db.Run("INSERT INTO dom_instances (instance) VALUES (?1)", instance_key))
            return false;
        if (!domain->db.Run(R"(INSERT INTO dom_permissions (instance, username, permissions)
                               VALUES (?1, ?2, ?3))",
                            instance_key, default_user, permissions))
            return false;

        return true;
    });
    if (!success)
        return false;

    db_guard.Disable();
    return true;
}

int RunInit(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *username = nullptr;
    const char *password = nullptr;
    bool empty = false;
    bool change_owner = false;
    uid_t owner_uid = 0;
    gid_t owner_gid = 0;
    const char *root_directory = nullptr;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 init [options] [directory]%!0

Options:
    %!..+-u, --user <name>%!0            Name of default user
        %!..+--password <pwd>%!0         Password of default user

        %!..+--empty%!0                  Don't create default instance)", FelixTarget);

#ifndef _WIN32
        PrintLn(fp, R"(
    %!..+-o, --owner <owner>%!0          Change directory and file owner)");
#endif
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-u", "--user", OptionType::Value)) {
                username = opt.current_value;
            } else if (opt.Test("--password", OptionType::Value)) {
                password = opt.current_value;
            } else if (opt.Test("--empty")) {
                empty = true;
#ifndef _WIN32
            } else if (opt.Test("-o", "--owner", OptionType::Value)) {
                change_owner = true;

                if (!FindPosixUser(opt.current_value, &owner_uid, &owner_gid))
                    return 1;
#endif
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        root_directory = opt.ConsumeNonOption();
        root_directory = NormalizePath(root_directory ? root_directory : ".", GetWorkingDirectory(), &temp_alloc).ptr;
    }

    // Errors and defaults
    if (password && !username) {
        LogError("Option --password cannot be used without --user");
        return 1;
    }

    // Drop created files and directories if anything fails
    HeapArray<const char *> directories;
    HeapArray<const char *> files;
    RG_DEFER_N(root_guard) {
        for (const char *filename: files) {
            UnlinkFile(filename);
        }
        for (Size i = directories.len - 1; i >= 0; i--) {
            UnlinkDirectory(directories[i]);
        }
    };

    // Make or check instance directory
    if (TestFile(root_directory)) {
        if (!IsDirectoryEmpty(root_directory)) {
            LogError("Directory '%1' is not empty", root_directory);
            return 1;
        }
    } else {
        if (!MakeDirectory(root_directory, false))
            return 1;
        directories.Append(root_directory);
    }
    if (change_owner && !ChangeFileOwner(root_directory, owner_uid, owner_gid))
        return 1;

    // Gather missing information
    if (!username) {
        username = Prompt("Admin: ", &temp_alloc);
        if (!username)
            return 1;
    }
    if (!CheckUserName(username))
        return 1;
    if (!password) {
        password = Prompt("Password: ", "*", &temp_alloc);
        if (!password)
            return 1;
    }
    if (!password[0]) {
        LogError("Password cannot be empty");
        return 1;
    }
    LogInfo();

    // Create domain
    DomainHolder domain;
    {
        const char *filename = Fmt(&temp_alloc, "%1%/goupile.ini", root_directory).ptr;
        files.Append(filename);

        if (!WriteFile(DefaultConfig, filename))
            return 1;

        if (!LoadConfig(filename, &domain.config))
            return 1;
    }

    // Create directories
    {
        const auto make_directory = [&](const char *path) {
            if (!MakeDirectory(path))
                return false;
            directories.Append(path);
            if (change_owner && !ChangeFileOwner(path, owner_uid, owner_gid))
                return false;

            return true;
        };

        if (!make_directory(domain.config.instances_directory))
            return 1;
        if (!make_directory(domain.config.temp_directory))
            return 1;
        if (!make_directory(domain.config.backup_directory))
            return 1;
    }

    // Create database
    if (!domain.db.Open(domain.config.database_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return 1;
    files.Append(domain.config.database_filename);
    if (!MigrateDomain(&domain.db, domain.config.instances_directory))
        return 1;
    if (change_owner && !ChangeFileOwner(domain.config.database_filename, owner_uid, owner_gid))
        return 1;

    // Create default admin user
    {
        char hash[crypto_pwhash_STRBYTES];
        if (!HashPassword(password, hash))
            return 1;

        // Create passport key
        char passport[64];
        {
            uint8_t buf[32];
            randombytes_buf(&buf, RG_SIZE(buf));
            sodium_bin2base64(passport, RG_SIZE(passport), buf, RG_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
        }

        if (!domain.db.Run(R"(INSERT INTO dom_users (username, password_hash, admin, passport)
                              VALUES (?1, ?2, 1, ?3))", username, hash, passport))
            return 1;
    }

    // Create default instance
    if (!empty && !CreateInstance(&domain, "demo", "DEMO", username))
        return 1;

    if (!domain.db.Close())
        return 1;

    root_guard.Disable();
    return 0;
}

int RunMigrate(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *config_filename = "goupile.ini";

    const auto print_usage = [&](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 migrate <instance_file> ...%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file
                                 %!D..(default: %2)%!0)", FelixTarget, config_filename);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                config_filename = opt.current_value;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }
    }

    DomainConfig config;
    if (!LoadConfig(config_filename, &config))
        return 1;

    // Migrate and open main database
    sq_Database db;
    if (!db.Open(config.database_filename, SQLITE_OPEN_READWRITE))
        return 1;
    if (!MigrateDomain(&db, config.instances_directory))
        return 1;

    // Migrate instances
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT instance FROM dom_instances", &stmt))
            return 1;

        bool success = true;

        while (stmt.Next()) {
            const char *key = (const char *)sqlite3_column_text(stmt, 0);
            const char *filename = config.GetInstanceFileName(key, &temp_alloc);

            success &= MigrateInstance(filename);
        }
        if (!stmt.IsValid())
            return 1;

        if (!success)
            return 1;
    }

    if (!db.Close())
        return 1;

    return 0;
}

void HandleCreateInstance(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);
    if (!session || !session->admin) {
        LogError("Non-admin users are not allowed to create instances");
        io->AttachError(403);
        return;
    }

    if (gp_domain.CountInstances() >= MaxInstancesPerDomain) {
        LogError("This domain has too many instances");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Read POST values
        HashMap<const char *, const char *> values;
        if (!io->ReadPostValues(&io->allocator, &values)) {
            io->AttachError(422);
            return;
        }

        const char *instance_key = values.FindValue("key", nullptr);
        const char *title = values.FindValue("title", instance_key);
        if (!instance_key) {
            LogError("Missing parameters");
            io->AttachError(422);
            return;
        }
        if (!CheckInstanceKey(instance_key)) {
            io->AttachError(422);
            return;
        }
        if (!title[0]) {
            LogError("Empty parameters");
            io->AttachError(422);
            return;
        }

        bool success = gp_domain.db.Transaction([&]() {
            // Log action
            int64_t time = GetUnixTime();
            if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username, details)
                                     VALUES (?1, ?2, ?3, ?4, ?5))",
                                  time, request.client_addr, "create_instance", session->username,
                                  instance_key))
                return false;

            bool conflict;
            if (!CreateInstance(&gp_domain, instance_key, title, session->username, &conflict)) {
                if (conflict) {
                    io->AttachError(409);
                }
                return false;
            }

            return true;
        });
        if (!success)
            return;

        SignalWaitFor();
        io->AttachText(200, "Done!");
    });
}

void HandleDeleteInstance(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);
    if (!session || !session->admin) {
        LogError("Non-admin users are not allowed to delete instances");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Read POST values
        HashMap<const char *, const char *> values;
        if (!io->ReadPostValues(&io->allocator, &values)) {
            io->AttachError(422);
            return;
        }

        const char *instance_key = values.FindValue("key", nullptr);
        if (!instance_key) {
            LogError("Missing parameters");
            io->AttachError(422);
            return;
        }

        bool success = gp_domain.db.Transaction([&]() {
            // Log action
            int64_t time = GetUnixTime();
            if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username, details)
                                     VALUES (?1, ?2, ?3, ?4, ?5))",
                                  time, request.client_addr, "delete_instance", session->username,
                                  instance_key))
                return false;

            if (!gp_domain.db.Run("DELETE FROM dom_permissions WHERE instance = ?1", instance_key))
                return false;
            if (!gp_domain.db.Run("DELETE FROM dom_instances WHERE instance = ?1", instance_key))
                return false;

            if (!sqlite3_changes(gp_domain.db)) {
                LogError("Instance '%1' does not exist", instance_key);
                io->AttachError(404);
                return false;
            }

            return true;
        });
        if (!success)
            return;

        SignalWaitFor();
        io->AttachText(200, "Done!");
    });
}

void HandleConfigureInstance(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);
    if (!session || !session->admin) {
        LogError("Non-admin users are not allowed to configure instances");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Read POST values
        HashMap<const char *, const char *> values;
        if (!io->ReadPostValues(&io->allocator, &values)) {
            io->AttachError(422);
            return;
        }

        const char *instance_key = values.FindValue("key", nullptr);
        if (!instance_key) {
            LogError("Missing 'key' parameter");
            io->AttachError(422);
            return;
        }

        InstanceHolder *instance = gp_domain.Ref(instance_key);
        if (!instance) {
            LogError("Instance '%1' does not exist", instance_key);
            io->AttachError(404);
            return;
        }
        RG_DEFER_N(ref_guard) { instance->Unref(); };

        decltype(InstanceHolder::config) config = instance->config;

        // Parse new configuration values
        {
            bool valid = true;

            if (const char *str = values.FindValue("title", nullptr); str) {
                config.title = str;

                if (!str[0]) {
                    LogError("Application name cannot be empty");
                    valid = false;
                }
            }
            if (const char *str = values.FindValue("use_offline", nullptr); str) {
                char buf[32];
                ConvertFromJsonName(str, buf);

                valid &= ParseBool(buf, &config.use_offline);
            }
            if (const char *str = values.FindValue("sync_mode", nullptr); str) {
                char buf[32];
                ConvertFromJsonName(str, buf);

                if (!OptionToEnum(SyncModeNames, buf, &config.sync_mode)) {
                    LogError("Unknown sync mode '%1'", str);
                    valid = false;
                }
            }
            config.backup_key = values.FindValue("backup_key", config.backup_key);
            if (config.backup_key && !config.backup_key[0])
                config.backup_key = nullptr;

            if (!valid) {
                io->AttachError(422);
                return;
            }
        }

        // Write new configuration to database
        bool success = instance->db.Transaction([&]() {
            // Log action
            int64_t time = GetUnixTime();
            if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username, details)
                                     VALUES (?1, ?2, ?3, ?4, ?5))",
                                  time, request.client_addr, "edit_instance", session->username,
                                  instance_key))
                return false;

            const char *sql = "UPDATE fs_settings SET value = ?2 WHERE key = ?1";
            bool success = true;

            success &= instance->db.Run(sql, "Title", config.title);
            success &= instance->db.Run(sql, "UseOffline", 0 + config.use_offline);
            success &= instance->db.Run(sql, "SyncMode", SyncModeNames[(int)config.sync_mode]);
            success &= instance->db.Run(sql, "BackupKey", config.backup_key);

            return success;
        });
        if (!success)
            return;

        // Reload when you can
        instance->Reload();
        instance->Unref();
        ref_guard.Disable();

        SignalWaitFor();
        io->AttachText(200, "Done!");
    });
}

void HandleListInstances(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);
    if (!session || !session->admin) {
        LogError("Non-admin users are not allowed to list instances");
        io->AttachError(403);
        return;
    }

    sq_Statement stmt;
    if (!gp_domain.db.Prepare("SELECT instance FROM dom_instances ORDER BY instance", &stmt))
        return;

    // Export data
    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    while (stmt.Next()) {
        const char *key = (const char *)sqlite3_column_text(stmt, 0);
        char buf[512];

        InstanceHolder *instance = gp_domain.Ref(key);
        if (!instance)
            continue;
        RG_DEFER { instance->Unref(); };

        json.StartObject();

        json.Key("key"); json.String(instance->key.ptr);
        json.Key("config"); json.StartObject();
            json.Key("title"); json.String(instance->config.title);
            json.Key("use_offline"); json.Bool(instance->config.use_offline);
            ConvertToJsonName(SyncModeNames[(int)instance->config.sync_mode], buf);
            json.Key("sync_mode"); json.String(buf);
            if (instance->config.backup_key) {
                json.Key("backup_key"); json.String(instance->config.backup_key);
            }
        json.EndObject();

        json.EndObject();
    }
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish(io);
}

void HandleCreateUser(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);
    if (!session || !session->admin) {
        LogError("Non-admin users are not allowed to create users");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Read POST values
        HashMap<const char *, const char *> values;
        if (!io->ReadPostValues(&io->allocator, &values)) {
            io->AttachError(422);
            return;
        }

        // Username and password
        const char *username = values.FindValue("username", nullptr);
        const char *password = values.FindValue("password", nullptr);
        if (!username || !password) {
            LogError("Missing parameters");
            io->AttachError(422);
            return;
        }
        if (!CheckUserName(username)) {
            io->AttachError(422);
            return;
        }
        if (!password[0]) {
            LogError("Empty password is not allowed");
            io->AttachError(422);
            return;
        }

        // Create admin user?
        bool admin = false;
        {
            const char *str = values.FindValue("admin", nullptr);
            if (str && !ParseBool(str, &admin)) {
                io->AttachError(422);
                return;
            }
        }

        // Hash password
        char hash[crypto_pwhash_STRBYTES];
        if (!HashPassword(password, hash))
            return;

        // Create passport key
        char passport[64];
        {
            uint8_t buf[32];
            randombytes_buf(&buf, RG_SIZE(buf));
            sodium_bin2base64(passport, RG_SIZE(passport), buf, RG_SIZE(buf), sodium_base64_VARIANT_ORIGINAL);
        }

        gp_domain.db.Transaction([&]() {
            // Check for existing user
            {
                sq_Statement stmt;
                if (!gp_domain.db.Prepare("SELECT admin FROM dom_users WHERE username = ?1", &stmt))
                    return false;
                sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

                if (stmt.Next()) {
                    LogError("User '%1' already exists", username);
                    io->AttachError(409);
                    return false;
                } else if (!stmt.IsValid()) {
                    return false;
                }
            }

            // Log action
            int64_t time = GetUnixTime();
            if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username, details)
                                     VALUES (?1, ?2, ?3, ?4, ?5))",
                                  time, request.client_addr, "create_user", session->username,
                                  username))
                return false;

            // Create user
            if (!gp_domain.db.Run(R"(INSERT INTO dom_users (username, password_hash, admin, passport)
                                     VALUES (?1, ?2, ?3, ?4))",
                                  username, hash, 0 + admin, passport))
                return false;

            io->AttachText(200, "Done!");
            return true;
        });
    });
}

void HandleDeleteUser(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);
    if (!session || !session->admin) {
        LogError("Non-admin users are not allowed to delete users");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Read POST values
        HashMap<const char *, const char *> values;
        if (!io->ReadPostValues(&io->allocator, &values)) {
            io->AttachError(422);
            return;
        }

        // Username and password
        const char *username = values.FindValue("username", nullptr);
        if (!username) {
            LogError("Missing parameters");
            io->AttachError(422);
            return;
        }
        if (TestStr(username, session->username)) {
            LogError("You cannot delete yourself");
            io->AttachError(403);
            return;
        }

        gp_domain.db.Transaction([&]() {
            sq_Statement stmt;
            if (!gp_domain.db.Prepare("SELECT passport FROM dom_users", &stmt))
                return false;

            if (!stmt.Next()) {
                if (!stmt.IsValid()) {
                    LogError("User '%1' does not exist", username);
                    io->AttachError(404);
                }
                return false;
            }

            const char *passport = (const char *)sqlite3_column_text(stmt, 0);
            int64_t time = GetUnixTime();

            // Log action
            if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username, details)
                                     VALUES (?1, ?2, ?3, ?4, ?5 || ':' || ?6))",
                                  time, request.client_addr, "delete_user", session->username,
                                  username, passport))
                return false;

            if (!gp_domain.db.Run("DELETE FROM dom_permissions WHERE username = ?1", username))
                return false;
            if (!gp_domain.db.Run("DELETE FROM dom_users WHERE username = ?1", username))
                return false;

            io->AttachError(200, "Done!");
            return true;
        });
    });
}

static bool ParsePermissionList(Span<const char> remain, uint32_t *out_permissions)
{
    uint32_t permissions = 0;

    while (remain.len) {
        Span<const char> js_perm = TrimStr(SplitStr(remain, ',', &remain), " ");

        if (js_perm.len) {
            char perm_str[256];
            ConvertFromJsonName(js_perm, perm_str);

            UserPermission perm;
            if (!OptionToEnum(UserPermissionNames, perm_str, &perm)) {
                LogError("Unknown permission '%1'", js_perm);
                return false;
            }

            permissions |= 1 << (int)perm;
        }
    }

    *out_permissions = permissions;
    return true;
}

void HandleAssignUser(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);
    if (!session || !session->admin) {
        LogError("Non-admin users are not allowed to delete users");
        io->AttachError(403);
        return;
    }

    io->RunAsync([=]() {
        // Read POST values
        HashMap<const char *, const char *> values;
        if (!io->ReadPostValues(&io->allocator, &values)) {
            io->AttachError(422);
            return;
        }

        const char *instance = values.FindValue("instance", nullptr);
        const char *username = values.FindValue("user", nullptr);
        const char *zone = values.FindValue("zone", nullptr);
        if (!instance || !username) {
            LogError("Missing parameters");
            io->AttachError(422);
            return;
        }
        if (zone && !zone[0]) {
            LogError("Empty zone value is not allowed");
            io->AttachError(422);
            return;
        }

        // Parse permissions
        uint32_t permissions = 0;
        {
            const char *str = values.FindValue("permissions", nullptr);
            if (str && !ParsePermissionList(str, &permissions)) {
                io->AttachError(422);
                return;
            }
        }

        gp_domain.db.Transaction([&]() {
            // Does instance exist?
            {
                sq_Statement stmt;
                if (!gp_domain.db.Prepare("SELECT instance FROM dom_instances WHERE instance = ?1", &stmt))
                    return false;
                sqlite3_bind_text(stmt, 1, instance, -1, SQLITE_STATIC);

                if (!stmt.Next()) {
                    if (stmt.IsValid()) {
                        LogError("Instance '%1' does not exist", instance);
                        io->AttachError(404);
                    }
                    return false;
                }
            }

            // Does user exist?
            {
                sq_Statement stmt;
                if (!gp_domain.db.Prepare("SELECT username FROM dom_users WHERE username = ?1", &stmt))
                    return false;
                sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

                if (!stmt.Next()) {
                    if (stmt.IsValid()) {
                        LogError("User '%1' does not exist", instance);
                        io->AttachError(404);
                    }
                    return false;
                }
            }

            // Log action
            int64_t time = GetUnixTime();
            if (!gp_domain.db.Run(R"(INSERT INTO adm_events (time, address, type, username, details)
                                     VALUES (?1, ?2, ?3, ?4, ?5 || '+' || ?6 || ':' || ?7 || '@' || ?8))",
                                  time, request.client_addr, "assign_user", session->username,
                                  instance, username, permissions, zone))
                return false;

            // Adjust permissions
            if (permissions) {
                if (!gp_domain.db.Run(R"(INSERT INTO dom_permissions (instance, username, permissions, zone)
                                         VALUES (?1, ?2, ?3, ?4)
                                         ON CONFLICT(instance, username)
                                             DO UPDATE SET permissions = excluded.permissions)",
                                      instance, username, permissions, zone))
                    return false;
            } else {
                if (!gp_domain.db.Run("DELETE FROM dom_permissions WHERE instance = ?1 AND username = ?2",
                                      instance, username))
                    return false;
            }

            io->AttachText(200, "Done!");
            return true;
        });
    });
}

void HandleListUsers(const http_RequestInfo &request, http_IO *io)
{
    RetainPtr<const Session> session = GetCheckedSession(request, io);
    if (!session || !session->admin) {
        LogError("Non-admin users are not allowed to list users");
        io->AttachError(403);
        return;
    }

    sq_Statement stmt;
    if (!gp_domain.db.Prepare(R"(SELECT u.rowid, u.username, u.admin, p.instance, p.permissions FROM dom_users u
                                 LEFT JOIN dom_permissions p ON (p.username = u.username)
                                 ORDER BY u.username, p.instance)", &stmt))
        return;

    // Export data
    http_JsonPageBuilder json(request.compression_type);

    json.StartArray();
    if (stmt.Next()) {
        do {
            int64_t rowid = sqlite3_column_int64(stmt, 0);

            json.StartObject();

            json.Key("username"); json.String((const char *)sqlite3_column_text(stmt, 1));
            json.Key("admin"); json.Bool(sqlite3_column_int(stmt, 2));
            json.Key("instances"); json.StartObject();
            if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
                do {
                    const char *instance = (const char *)sqlite3_column_text(stmt, 3);
                    uint32_t permissions = (uint32_t)sqlite3_column_int64(stmt, 4);

                    json.Key(instance); json.StartArray();
                    for (Size i = 0; i < RG_LEN(UserPermissionNames); i++) {
                        if (permissions & (1 << i)) {
                            char js_name[64];
                            ConvertToJsonName(UserPermissionNames[i], js_name);

                            json.String(js_name);
                        }
                    }
                    json.EndArray();
                } while (stmt.Next() && sqlite3_column_int64(stmt, 0) == rowid);
            } else {
                stmt.Next();
            }
            json.EndObject();

            json.EndObject();
        } while (stmt.IsRow());
    }
    if (!stmt.IsValid())
        return;
    json.EndArray();

    json.Finish(io);
}

}
