// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "../../core/libwrap/sqlite.hh"
#include "../server/instance.hh"
#include "../server/user.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

#ifndef _WIN32
    #include <sys/types.h>
    #include <pwd.h>
    #include <unistd.h>
#endif

namespace RG {

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

static bool ChangeFileOwner(const char *filename, uid_t uid, gid_t gid)
{
    if (chown(filename, uid, gid) < 0) {
        LogError("Failed to change '%1' owner: %2", filename, strerror(errno));
        return false;
    }

    return true;
}
#endif

static int RunInit(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    Span<const char> app_key = {};
    Span<const char> app_name = {};
    bool empty = false;
    const char *username = nullptr;
    const char *password = nullptr;
#ifndef _WIN32
    bool change_owner = false;
    uid_t owner_uid = 0;
    gid_t owner_gid = 0;
#endif
    const char *profile_directory = nullptr;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 init [options] [profile_directory]%!0

Options:
    %!..+-k, --key <key>%!0              Change application key
                                 %!D..(default: directory name)%!0
        %!..+--name <name>%!0            Change application name
                                 %!D..(default: project key)%!0

    %!..+-u, --user <name>%!0            Name of default user
        %!..+--password <pwd>%!0         Password of default user

        %!..+--empty%!0                  Don't create default files)", FelixTarget);

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
            } else if (opt.Test("-k", "--key", OptionType::Value)) {
                app_key = opt.current_value;
            } else if (opt.Test("--name", OptionType::Value)) {
                app_name = opt.current_value;
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

        profile_directory = opt.ConsumeNonOption();
    }

    profile_directory = NormalizePath(profile_directory ? profile_directory : ".",
                                      GetWorkingDirectory(), &temp_alloc).ptr;

    // Errors and defaults
    if (!app_key.len) {
        app_key = TrimStrRight(profile_directory, RG_PATH_SEPARATORS);
        app_key = SplitStrReverseAny(app_key, RG_PATH_SEPARATORS);
    }
    if (!app_name.len) {
        app_name = app_key;
    }
    if (password && !username) {
        LogError("Option --password cannot be used without --user");
        return 1;
    }

    // Drop created files and directories if anything fails
    HeapArray<const char *> directories;
    HeapArray<const char *> files;
    RG_DEFER_N(out_guard) {
        for (const char *filename: files) {
            UnlinkFile(filename);
        }
        for (Size i = directories.len - 1; i >= 0; i--) {
            UnlinkDirectory(directories[i]);
        }
    };

    // Make or check project directory
    if (TestFile(profile_directory)) {
        if (!IsDirectoryEmpty(profile_directory)) {
            LogError("Directory '%1' is not empty", profile_directory);
            return 1;
        }
    } else {
        if (!MakeDirectory(profile_directory, false))
            return 1;

        directories.Append(profile_directory);
    }
#ifndef _WIN32
    if (change_owner && !ChangeFileOwner(profile_directory, owner_uid, owner_gid))
        return 1;
#endif

    // Gather missing information
    if (!username) {
        username = Prompt("User: ", &temp_alloc);
        if (!username)
            return 1;
    }
    if (!username[0]) {
        LogError("Username cannot be empty");
        return 1;
    }
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

    // Directories
    {
        const auto make_profile_directory = [&](const char *name) {
            const char *directory = Fmt(&temp_alloc, "%1%/%2", profile_directory, name).ptr;
            if (!MakeDirectory(directory))
                return false;

            directories.Append(directory);

#ifndef _WIN32
            if (change_owner && !ChangeFileOwner(directory, owner_uid, owner_gid))
                return false;
#endif

            return true;
        };

        if (!make_profile_directory("tmp"))
            return 1;
        if (!make_profile_directory("backup"))
            return 1;
    }

    const char *config_filename = Fmt(&temp_alloc, "%1%/goupile.ini", profile_directory).ptr;
    files.Append(config_filename);

    // Write configuration file
    {
        StreamWriter st(config_filename);
        Print(&st, DefaultConfig, app_key, app_name);
        if (!st.Close())
            return 1;

        // This one keeps the default owner uid/gid even with --owner
    }

    // Create database
    {
        const char *filename = Fmt(&temp_alloc, "%1%/database.db", profile_directory).ptr;
        files.Append(filename);

        sq_Database database;
        if (!database.Open(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
            return 1;
        if (!database.Close())
            return 1;
    }

    // Open and migrate database schema
    InstanceData instance;
    if (!instance.Open(config_filename))
        return 1;
    if (!instance.Migrate())
        return 1;
#ifndef _WIN32
    if (change_owner && !ChangeFileOwner(instance.config.database_filename, owner_uid, owner_gid))
        return 1;
#endif

    // Create default files
    if (!empty) {
        Span<const AssetInfo> assets = GetPackedAssets();

        sq_Statement stmt;
        if (!instance.db.Prepare("INSERT INTO fs_files (path, blob, compression, sha256) VALUES (?, ?, ?, ?)", &stmt))
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
                        return false;

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
            sqlite3_bind_blob64(stmt, 2, gzip.ptr, gzip.len, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, "Gzip", -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, sha256, -1, SQLITE_STATIC);

            if (!stmt.Run())
                return 1;
        }
    }

    // Create default user (admin)
    {
        char hash[crypto_pwhash_STRBYTES];
        if (!HashPassword(password, hash))
            return 1;

        uint32_t permissions = (1u << RG_LEN(UserPermissionNames)) - 1;

        if (!instance.db.Run("INSERT INTO usr_users (username, password_hash, permissions) VALUES (?, ?, ?)",
                             username, hash, permissions))
            return 1;
    }

    // Make sure instance.db is OK!
    if (!instance.db.Close())
        return 1;

    out_guard.Disable();
    return 0;
}

static int RunMigrate(Span<const char *> arguments)
{
    // Options
    const char *config_filename = nullptr;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 migrate [options]%!0

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
            } else if (opt.Test("-C", "--config_file", OptionType::OptionalValue)) {
                config_filename = opt.current_value;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }
    }

    // Open instance
    InstanceData instance;
    if (!instance.Open(config_filename))
        return 1;

    LogInfo("Profile version: %1", instance.version);
    if (instance.version > SchemaVersion) {
        LogError("Profile is too recent, expected version <= %1", SchemaVersion);
        return 1;
    } else if (instance.version == SchemaVersion) {
        LogInfo("Profile is up to date");
        return 0;
    }

    return !instance.Migrate();
}

static bool ParsePermissionList(Span<const char> remain, uint32_t *out_permissions)
{
    uint32_t permissions = 0;

    while (remain.len) {
        Span<const char> part = TrimStr(SplitStr(remain, ',', &remain), " ");

        if (part.len) {
            UserPermission perm;
            if (!OptionToEnum(UserPermissionNames, part, &perm)) {
                LogError("Unknown permission '%1'", part);
                return false;
            }

            permissions |= 1 << (int)perm;
        }
    }

    *out_permissions = permissions;
    return true;
}

static int RunAddUser(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *config_filename = nullptr;
    const char *username = nullptr;
    const char *password = nullptr;
    const char *zone = nullptr;
    uint32_t permissions = UINT32_MAX;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 add_user [options] <username>%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

        %!..+--password <pwd>%!0         Password of user
        %!..+--zone <zone>%!0            Zone of user
    %!..+-p, --permissions <perms>%!0    User permissions

User permissions: %!..+%2%!0)", FelixTarget, FmtSpan(UserPermissionNames));
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::OptionalValue)) {
                config_filename = opt.current_value;
            } else if (opt.Test("--password", OptionType::Value)) {
                password = opt.current_value;
            } else if (opt.Test("--zone", OptionType::Value)) {
                zone = opt.current_value;
            } else if (opt.Test("-p", "--permissions", OptionType::Value)) {
                if (!ParsePermissionList(opt.current_value, &permissions))
                    return 1;
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

    // Open database
    InstanceData instance;
    if (!instance.Open(config_filename))
        return 1;

    // Find user first
    {
        sq_Statement stmt;
        if (!instance.db.Prepare("SELECT permissions FROM usr_users WHERE username = ?", &stmt))
            return false;
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

        if (stmt.Next()) {
            if (stmt.IsValid()) {
                LogError("User '%1' already exists", username);
            }
            return 1;
        }
    }

    // Gather missing information
    if (!password) {
        password = Prompt("Password: ", "*", &temp_alloc);
        if (!password)
            return 1;
    }
    if (!password[0]) {
        LogError("Password cannot be empty");
        return 1;
    }
    if (!zone) {
        zone = Prompt("Zone: ", &temp_alloc);
        if (!zone)
            return 1;
    }
    zone = zone[0] ? zone : nullptr;
    if (permissions == UINT32_MAX) {
        Span<const char> str = Prompt("Permissions: ", &temp_alloc);
        if (str.len && !ParsePermissionList(str, &permissions)) {
            return 1;
        } else if (!str.ptr) {
            return 1;
        }
    }
    LogInfo();

    // Hash password
    char hash[crypto_pwhash_STRBYTES];
    if (!HashPassword(password, hash))
        return 1;

    // Create user
    if (!instance.db.Run(R"(INSERT INTO usr_users (username, password_hash, zone, permissions)
                            VALUES (?, ?, ?, ?))",
                      username, hash, zone ? sq_Binding(zone) : sq_Binding(), permissions))
        return 1;

    LogInfo("Added user");
    return 0;
}

static int RunEditUser(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *config_filename = nullptr;
    const char *username = nullptr;
    const char *password = nullptr;
    const char *zone = nullptr;
    uint32_t permissions = UINT32_MAX;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 edit_user [options] <username>%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

        %!..+--password <pwd>%!0         Password of user
        %!..+--zone <zone>%!0            Zone of user
    %!..+-p, --permissions <perms>%!0    User permissions

User permissions: %!..+%2%!0)", FelixTarget, FmtSpan(UserPermissionNames));
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::OptionalValue)) {
                config_filename = opt.current_value;
            } else if (opt.Test("--password", OptionType::Value)) {
                password = opt.current_value;
            } else if (opt.Test("--zone", OptionType::Value)) {
                zone = opt.current_value;
            } else if (opt.Test("-p", "--permissions", OptionType::Value)) {
                if (!ParsePermissionList(opt.current_value, &permissions))
                    return 1;
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

    // Open instance
    InstanceData instance;
    if (!instance.Open(config_filename))
        return 1;

    // Find user first
    const char *prev_zone;
    uint32_t prev_permissions;
    {
        sq_Statement stmt;
        if (!instance.db.Prepare("SELECT zone, permissions FROM usr_users WHERE username = ?", &stmt))
            return false;
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

        if (!stmt.Next()) {
            if (stmt.IsValid()) {
                LogError("User '%1' does not exist", username);
            }
            return 1;
        }

        if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
            const char *zone = (const char *)sqlite3_column_text(stmt, 0);
            prev_zone = DuplicateString(zone, &temp_alloc).ptr;
        } else {
            prev_zone = nullptr;
        }
        prev_permissions = (uint32_t)sqlite3_column_int64(stmt, 1);
    }

    // Gather missing information
    if (!password) {
        password = Prompt("Password: ", "*", &temp_alloc);
        if (!password)
            return 1;
    }
    password = password[0] ? password : nullptr;
    if (zone && !zone[0]) {
        zone = prev_zone;
    } else if (!zone) {
        ConsolePrompter prompter;

        prompter.prompt = "Zone: ";
        prompter.str.allocator = &temp_alloc;
        prompter.str.Append(prev_zone);

        if (!prompter.Read())
            return 1;

        zone = prompter.str.len ? prompter.str.ptr : nullptr;
    }
    if (permissions == UINT32_MAX) {
        ConsolePrompter prompter;

        prompter.prompt = "Permissions: ";
        for (Size i = 0; i < RG_LEN(UserPermissionNames); i++) {
            if (prev_permissions & (1u << i)) {
                Fmt(&prompter.str, "%1, ", UserPermissionNames[i]);
            }
        }
        prompter.str.len = std::max((Size)0, prompter.str.len - 2);

        if (!prompter.Read())
            return 1;
        if (!ParsePermissionList(prompter.str, &permissions))
            return 1;
    }

    // Update user
    sq_TransactionResult ret = instance.db.Transaction([&]() {
        if (password) {
            char hash[crypto_pwhash_STRBYTES];
            if (!HashPassword(password, hash))
                return sq_TransactionResult::Rollback;

            if (!instance.db.Run("UPDATE usr_users SET password_hash = ? WHERE username = ?",
                                 hash, username))
                return sq_TransactionResult::Error;
        }

        if (!instance.db.Run("UPDATE usr_users SET zone = ?, permissions = ? WHERE username = ?",
                             zone ? sq_Binding(zone) : sq_Binding(), permissions, username))
            return sq_TransactionResult::Error;
        if (!sqlite3_changes(instance.db)) {
            LogError("UPDATE request failed: no match");
            return sq_TransactionResult::Rollback;
        }

        return sq_TransactionResult::Commit;
    });
    if (ret != sq_TransactionResult::Commit)
        return 1;

    LogInfo("Changed user");
    return 0;
}

static int RunRemoveUser(Span<const char *> arguments)
{
    // Options
    const char *config_filename = nullptr;
    const char *username = nullptr;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 remove_user [options] <username>%!0

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
            } else if (opt.Test("-C", "--config_file", OptionType::OptionalValue)) {
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

    // Open instance
    InstanceData instance;
    if (!instance.Open(config_filename))
        return 1;

    // Delete user
    if (!instance.db.Run("DELETE FROM usr_users WHERE username = ?", username))
        return 1;
    if (!sqlite3_changes(instance.db)) {
        LogError("User '%1' does not exist", username);
        return 1;
    }

    LogInfo("Removed user");
    return 0;
}

int Main(int argc, char **argv)
{
    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 <command> [args]%!0

General commands:
    %!..+init%!0                         Create new profile
    %!..+migrate%!0                      Migrate existing profile

User commands:
    %!..+add_user%!0                     Add new user
    %!..+edit_user%!0                    Edit existing user
    %!..+remove_user%!0                  Remove existing user)", FelixTarget);
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
        PrintLn("%!R..%1%!0 %2", FelixTarget, FelixVersion);
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
    } else if (TestStr(cmd, "add_user")) {
        return RunAddUser(arguments);
    } else if (TestStr(cmd, "edit_user")) {
        return RunEditUser(arguments);
    } else if (TestStr(cmd, "remove_user")) {
        return RunRemoveUser(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
