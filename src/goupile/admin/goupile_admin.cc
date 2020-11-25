// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "../../core/libwrap/sqlite.hh"
#include "../server/domain.hh"
#include "../server/instance.hh"
#include "../server/user.hh"
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

static const char *DefaultConfig = R"(
[Resources]
# DatabaseFile = goupile.db
# InstanceDirectory = instances
# TempDirectory = tmp

[Session]
# DemoUser =

[HTTP]
# SocketType = Dual
# Port = 8889
# Threads =
# AsyncThreads =
)";

static bool CheckKeyName(Span<const char> key, const char *type)
{
    const auto test_key_char = [](char c) { return (c >= 'a' && c <= 'z') || IsAsciiDigit(c) || c == '_'; };

    if (!key.len) {
        LogError("%1 key cannot be empty", type);
        return false;
    }
    if (!std::all_of(key.begin(), key.end(), test_key_char)) {
        LogError("%1 key must only contain lowercase alphanumeric or '_' characters", type);
        return false;
    }

    return true;
}

static bool CheckUserName(Span<const char> username)
{
    const auto test_user_char = [](char c) { return IsAsciiAlphaOrDigit(c) || c == '_' || c == '.'; };

    if (!username.len) {
        LogError("Username cannot be empty");
        return false;
    }
    if (!std::all_of(username.begin(), username.end(), test_user_char)) {
        LogError("Username must only contain alphanumeric, '_' or '.' characters");
        return false;
    }

    return true;
}

static bool HashPassword(Span<const char> password, char out_hash[crypto_pwhash_STRBYTES])
{
    if (crypto_pwhash_str(out_hash, password.ptr, password.len,
                          crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        LogError("Failed to hash password");
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

static int RunInit(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *username = nullptr;
    const char *password = nullptr;
    bool change_owner = false;
    uid_t owner_uid = 0;
    gid_t owner_gid = 0;
    const char *root_directory = nullptr;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 init [options] [directory]%!0
Options:
    %!..+-u, --user <name>%!0            Name of default user
        %!..+--password <pwd>%!0         Password of default user)", FelixTarget);

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
        username = Prompt("Admin user: ", &temp_alloc);
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
    DomainConfig config;
    {
        const char *filename = Fmt(&temp_alloc, "%1%/goupile.ini", root_directory).ptr;
        files.Append(filename);

        if (!WriteFile(DefaultConfig, filename))
            return 1;

        if (!LoadConfig(filename, &config))
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

        if (!make_directory(config.instances_directory))
            return 1;
        if (!make_directory(config.temp_directory))
            return 1;
    }

    // Create database
    sq_Database db;
    if (!db.Open(config.database_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return 1;
    files.Append(config.database_filename);
    if (!MigrateDomain(&db, config.instances_directory))
        return 1;
    if (change_owner && !ChangeFileOwner(config.database_filename, owner_uid, owner_gid))
        return 1;

    // Create default admin user
    {
        char hash[crypto_pwhash_STRBYTES];
        if (!HashPassword(password, hash))
            return 1;

        if (!db.Run("INSERT INTO dom_users (username, password_hash, admin) VALUES (?, ?, 1)", username, hash))
            return 1;
    }

    if (!db.Close())
        return 1;

    root_guard.Disable();
    return 0;
}

static int RunMigrate(Span<const char *> arguments)
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
        if (!db.Prepare("SELECT instance FROM dom_instances;", &stmt))
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

static int RunAddInstance(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *config_filename = "goupile.ini";
    Span<const char> base_url = {};
    Span<const char> app_key = {};
    Span<const char> app_name = {};
    bool empty = false;
    bool force = false;
    const char *instance_key = nullptr;

    const auto print_usage = [&](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 add_instance [options] <instance>%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file
                                 %!D..(default: %2)%!0

        %!..+--base_url <url>%!0         Change base URL
                                 %!D..(default: directory name)%!0
        %!..+--app_key <key>%!0          Change application key
                                 %!D..(default: directory name)%!0
        %!..+--app_name <name>%!0        Change application name
                                 %!D..(default: project key)%!0

        %!..+--empty%!0                  Don't create default files
        %!..+--force%!0                  Force creation if database already exists)", FelixTarget, config_filename);
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
            } else if (opt.Test("--base_url", OptionType::Value)) {
                base_url = opt.current_value;
            } else if (opt.Test("--app_key", OptionType::Value)) {
                app_key = opt.current_value;
            } else if (opt.Test("--app_name", OptionType::Value)) {
                app_name = opt.current_value;
            } else if (opt.Test("--empty")) {
                empty = true;
            } else if (opt.Test("--force")) {
                force = true;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        instance_key = opt.ConsumeNonOption();
    }

    // Default values
    if (!CheckKeyName(instance_key, "Instance"))
        return 1;
    if (!base_url.len) {
        base_url = Fmt(&temp_alloc, "/%1/", instance_key);
    }
    if (!app_key.len) {
        app_key = instance_key;
    }
    if (!app_name.len) {
        app_name = instance_key;
    }

    DomainData domain;
    if (!domain.Open(config_filename))
        return 1;

    // Check for existing instance
    {
        sq_Statement stmt;
        if (!domain.db.Prepare("SELECT instance FROM dom_instances WHERE instance = ?;", &stmt))
            return 1;
        sqlite3_bind_text(stmt, 1, instance_key, -1, SQLITE_STATIC);

        if (stmt.Next()) {
            LogError("Instance '%1' already exists", instance_key);
            return 1;
        } else if (!stmt.IsValid()) {
            return 1;
        }
    }

    const char *database_filename = domain.config.GetInstanceFileName(instance_key, &temp_alloc);
    bool reuse_database = TestFile(database_filename);
    if (reuse_database && !force) {
        LogError("Database '%1' already exists (old deleted instance?)", database_filename);
        return 1;
    }
    RG_DEFER_N(db_guard) {
        if (!reuse_database) {
            UnlinkFile(database_filename);
        }
    };

    uid_t owner_uid = 0;
    gid_t owner_gid = 0;
#ifndef _WIN32
    {
        struct stat sb;
        if (stat(domain.config.database_filename, &sb) < 0) {
            LogError("Failed to stat '%1': %2", domain.config.database_filename, strerror(errno));
            return 1;
        }

        owner_uid = sb.st_uid;
        owner_gid = sb.st_gid;
    }
#endif

    // Create instance database
    sq_Database db;
    if (!db.Open(database_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return 1;
    if (!MigrateInstance(&db))
        return 1;
    if (!ChangeFileOwner(database_filename, owner_uid, owner_gid))
        return 1;

    // Set default settings
    {
        const char *sql = "UPDATE fs_settings SET value = ? WHERE key = ?";
        bool success = true;

        success &= db.Run(sql, base_url, "BaseUrl");
        success &= db.Run(sql, app_key, "AppKey");
        success &= db.Run(sql, app_name, "AppName");

        if (!success)
            return 1;
    }

    // Create default files
    if (!empty) {
        Span<const AssetInfo> assets = GetPackedAssets();
        int64_t mtime = GetUnixTime();

        sq_Statement stmt;
        if (!db.Prepare(R"(INSERT INTO fs_files (active, path, mtime, blob, compression, sha256, size)
                           VALUES (1, ?, ?, ?, ?, ?, ?);)", &stmt))
            return 1;

        for (const AssetInfo &asset: assets) {
            const char *path = Fmt(&temp_alloc, "/files/%1", asset.name).ptr;

            HeapArray<uint8_t> gzip;
            char sha256[65];
            {
                StreamReader reader(asset.data, "<asset>", asset.compression_type);
                StreamWriter writer(&gzip, "<gzip>", CompressionType::Gzip);

                crypto_hash_sha256_state state;
                crypto_hash_sha256_init(&state);

                while (!reader.IsEOF()) {
                    LocalArray<uint8_t, 16384> buf;
                    buf.len = reader.Read(buf.data);
                    if (buf.len < 0)
                        return 1;

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
            sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 2, mtime);
            sqlite3_bind_blob64(stmt, 3, gzip.ptr, gzip.len, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, "Gzip", -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 5, sha256, -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 6, asset.data.len);

            if (!stmt.Run())
                return 1;
        }
    }

    if (!db.Close())
        return 1;
    if (!domain.db.Run("INSERT INTO dom_instances (instance) VALUES (?);", instance_key))
        return 1;

    LogInfo("Added instance");
    db_guard.Disable();

    return 0;
}

static int RunDeleteInstance(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *config_filename = "goupile.ini";
    bool purge = false;
    const char *instance_key = nullptr;

    const auto print_usage = [&](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 delete_instance [options] <instance>%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file
                                 %!D..(default: %2)%!0

        %!..+--purge%!0                  Completely delete instance database)",
                FelixTarget, config_filename);
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
            } else if (opt.Test("--purge")) {
                purge = true;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        instance_key = opt.ConsumeNonOption();
        if (!instance_key) {
            LogError("Instance key must be provided");
            return 1;
        }
    }

    DomainData domain;
    if (!domain.Open(config_filename))
        return 1;

    if (!domain.db.Run("DELETE FROM dom_instances WHERE instance = ?;", instance_key))
        return 1;
    if (!sqlite3_changes(domain.db)) {
        LogError("Instance '%1' does not exist", instance_key);
        return 1;
    }

    if (purge) {
        const char *filename = domain.config.GetInstanceFileName(instance_key, &temp_alloc);
        if (!UnlinkFile(filename))
            return 1;
    }

    LogInfo("Deleted instance");
    return 0;
}

static int RunAddUser(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *config_filename = nullptr;
    const char *username = nullptr;
    const char *password = nullptr;
    bool admin = false;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 add_user [options] [username]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

        %!..+--password <pwd>%!0         Password of user
        %!..+--admin%!0                  Set user as administrator)", FelixTarget);
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
            } else if (opt.Test("--password", OptionType::Value)) {
                password = opt.current_value;
            } else if (opt.Test("--admin")) {
                admin = true;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        username = opt.ConsumeNonOption();
    }

    DomainData domain;
    if (!domain.Open(config_filename))
        return 1;

    // Get username and check it
    if (!username) {
        username = Prompt("User: ", nullptr, &temp_alloc);
        if (!username)
            return 1;
    }
    if (!CheckUserName(username))
        return 1;

    // Find user first
    {
        sq_Statement stmt;
        if (!domain.db.Prepare("SELECT admin FROM dom_users WHERE username = ?", &stmt))
            return 1;
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

        if (stmt.Next()) {
            if (stmt.IsValid()) {
                LogError("User '%1' already exists", username);
            }
            return 1;
        }
    }

    // Get password if needed
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

    // Hash password
    char hash[crypto_pwhash_STRBYTES];
    if (!HashPassword(password, hash))
        return 1;

    // Create user
    if (!domain.db.Run("INSERT INTO dom_users (username, password_hash, admin) VALUES (?, ?, ?);",
                       username, hash, 0 + admin))
        return 1;

    LogInfo("Added user");
    return 0;
}

static int RunDeleteUser(Span<const char *> arguments)
{
    // Options
    const char *config_filename = nullptr;
    const char *username = nullptr;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 delete_user [options] <username>%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file)", FelixTarget);
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

        username = opt.ConsumeNonOption();
        if (!username) {
            LogError("No username provided");
            return 1;
        }
    }

    DomainData domain;
    if (!domain.Open(config_filename))
        return 1;

    // Delete user
    if (!domain.db.Run("DELETE FROM dom_users WHERE username = ?", username))
        return 1;
    if (!sqlite3_changes(domain.db)) {
        LogError("User '%1' does not exist", username);
        return 1;
    }

    LogInfo("User deleted");
    return 0;
}

int Main(int argc, char **argv)
{
    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 <command> [args]%!0

Domain commands:
    %!..+init%!0                         Create new domain
    %!..+migrate%!0                      Migrate existing domain

Instance commands:
    %!..+add_instance%!0                 Add new instance
    %!..+delete_instance%!0              Delete existing instance

User commands:
    %!..+add_user%!0                     Add new user
    %!..+delete_user%!0                  Remove existing user)", FelixTarget);
    };

    if (argc < 2) {
        print_usage(stderr);
        return 1;
    }

    const char *cmd = argv[1];
    Span<const char *> arguments((const char **)argv + 2, argc - 2);

    // Handle help and version arguments
    if (TestStr(cmd, "--help") || TestStr(cmd, "help")) {
        if (arguments.len && arguments[0][0] != '-') {
            cmd = arguments[0];
            arguments[0] = "--help";
        } else {
            print_usage(stdout);
            return 0;
        }
    } else if (TestStr(cmd, "--version")) {
        PrintLn("%!R..%1%!0 %2 (domain: %!..+%3%!0, instance: %!..+%4%!0)",
                FelixTarget, FelixVersion, DomainVersion, InstanceVersion);
        return 0;
    }

    if (sodium_init() < 0) {
        LogError("Failed to initialize libsodium");
        return 1;
    }

    if (TestStr(cmd, "init")) {
        return RunInit(arguments);
    } else if (TestStr(cmd, "migrate")) {
        return RunMigrate(arguments);
    } else if (TestStr(cmd, "add_instance")) {
        return RunAddInstance(arguments);
    } else if (TestStr(cmd, "delete_instance")) {
        return RunDeleteInstance(arguments);
    } else if (TestStr(cmd, "add_user")) {
        return RunAddUser(arguments);
    } else if (TestStr(cmd, "delete_user")) {
        return RunDeleteUser(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
