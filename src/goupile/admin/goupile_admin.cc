// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "../../core/libwrap/sqlite.hh"
#include "../server/config.hh"
#include "../server/user.hh"

#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

#ifndef _WIN32
    #include <sys/types.h>
    #include <pwd.h>
    #include <unistd.h>
#endif

namespace RG {

extern "C" const Span<const AssetInfo> pack_assets;

static const char *const DefaultConfig =
R"([Application]
Key = %1
Name = %2

[Data]
FilesDirectory = files
DatabaseFile = database.db

[Sync]
# UseOffline = Off
# AllowGuests = Off

[HTTP]
# IPStack = Dual
# Port = 8889
# Threads = 4
# BaseUrl = /
)";

static std::function<bool(sq_Database &database)> MigrationRequests[] = {
    [](sq_Database &database) { return database.Run(R"(
        CREATE TABLE rec_entries (
            table_name TEXT NOT NULL,
            id TEXT NOT NULL,
            sequence INTEGER NOT NULL
        );
        CREATE UNIQUE INDEX rec_entries_ti ON rec_entries (table_name, id);

        CREATE TABLE rec_fragments (
            table_name TEXT NOT NULL,
            id TEXT NOT NULL,
            page TEXT,
            username TEXT NOT NULL,
            mtime TEXT NOT NULL,
            complete INTEGER CHECK(complete IN (0, 1)) NOT NULL,
            json TEXT
        );
        CREATE INDEX rec_fragments_tip ON rec_fragments(table_name, id, page);

        CREATE TABLE rec_columns (
            table_name TEXT NOT NULL,
            page TEXT NOT NULL,
            key TEXT NOT NULL,
            prop TEXT,
            before TEXT,
            after TEXT
        );
        CREATE UNIQUE INDEX rec_columns_tpkp ON rec_columns (table_name, page, key, prop);

        CREATE TABLE rec_sequences (
            table_name TEXT NOT NULL,
            sequence INTEGER NOT NULL
        );
        CREATE UNIQUE INDEX rec_sequences_t ON rec_sequences (table_name);

        CREATE TABLE sched_resources (
            schedule TEXT NOT NULL,
            date TEXT NOT NULL,
            time INTEGER NOT NULL,

            slots INTEGER NOT NULL,
            overbook INTEGER NOT NULL
        );
        CREATE UNIQUE INDEX sched_resources_sdt ON sched_resources (schedule, date, time);

        CREATE TABLE sched_meetings (
            schedule TEXT NOT NULL,
            date TEXT NOT NULL,
            time INTEGER NOT NULL,

            identity TEXT NOT NULL
        );
        CREATE INDEX sched_meetings_sd ON sched_meetings (schedule, date, time);

        CREATE TABLE usr_users (
            username TEXT NOT NULL,
            password_hash TEXT NOT NULL,

            permissions INTEGER NOT NULL
        );
        CREATE UNIQUE INDEX usr_users_u ON usr_users (username);
    )"); },

    [](sq_Database &database) { return database.Run(R"(
        ALTER TABLE rec_fragments RENAME TO rec_fragments_BAK;
        DROP INDEX rec_fragments_tip;

        CREATE TABLE rec_fragments (
            table_name TEXT NOT NULL,
            id TEXT NOT NULL,
            page TEXT,
            username TEXT NOT NULL,
            mtime TEXT NOT NULL,
            complete INTEGER CHECK(complete IN (0, 1)) NOT NULL,
            json TEXT,
            anchor INTEGER PRIMARY KEY AUTOINCREMENT
        );
        CREATE INDEX rec_fragments_tip ON rec_fragments(table_name, id, page);

        INSERT INTO rec_fragments (table_name, id, page, username, mtime, complete, json)
            SELECT table_name, id, page, username, mtime, complete, json FROM rec_fragments_BAK;
        DROP TABLE rec_fragments_BAK;
    )"); },

    [](sq_Database &database) { return database.Run(R"(
        DROP INDEX rec_entries_ti;
        DROP INDEX rec_fragments_tip;
        DROP INDEX rec_columns_tpkp;
        DROP INDEX rec_sequences_t;

        ALTER TABLE rec_entries RENAME COLUMN table_name TO store;
        ALTER TABLE rec_fragments RENAME COLUMN table_name TO store;
        ALTER TABLE rec_columns RENAME COLUMN table_name TO store;
        ALTER TABLE rec_sequences RENAME COLUMN table_name TO store;

        CREATE UNIQUE INDEX rec_entries_si ON rec_entries (store, id);
        CREATE INDEX rec_fragments_sip ON rec_fragments(store, id, page);
        CREATE UNIQUE INDEX rec_columns_spkp ON rec_columns (store, page, key, prop);
        CREATE UNIQUE INDEX rec_sequences_s ON rec_sequences (store);
    )"); }
};

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

static bool RunMigrations(sq_Database &database, int version)
{
    bool success = database.Transaction([&]() {
        for (Size i = version; i < RG_LEN(MigrationRequests); i++) {
            const std::function<bool(sq_Database &database)> &func = MigrationRequests[i];
            if (!func(database))
                return false;
        }

        char buf[128];
        Fmt(buf, "PRAGMA user_version = %1;", RG_LEN(MigrationRequests));

        return database.Run(buf);
    });
    if (!success)
        return false;

    LogInfo("Schema version: %1", RG_LEN(MigrationRequests));
    return true;
}

static int RunInit(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    Span<const char> app_key = {};
    Span<const char> app_name = {};
    bool empty = false;
    Span<const char> default_username = nullptr;
    Span<const char> default_password = nullptr;
#ifndef _WIN32
    bool change_owner = false;
    uid_t owner_uid = 0;
    gid_t owner_gid = 0;
#endif
    const char *profile_directory = nullptr;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+goupile_admin init [options] profile_directory%!0

Options:
    %!..+-k, --key <key>%!0              Change application key
                                 %!D..(default: directory name)%!0
        %!..+--name <name>%!0            Change application name
                                 %!D..(default: project key)%!0

    %!..+-u, --user <name>%!0            Name of default user
        %!..+--password <pwd>%!0         Password of default user

        %!..+--empty%!0                  Don't create default files)");

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
                default_username = opt.current_value;
            } else if (opt.Test("--password", OptionType::Value)) {
                default_password = opt.current_value;
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
        app_key = TrimStrRight((Span<const char>)profile_directory, RG_PATH_SEPARATORS);
        app_key = SplitStrReverseAny(app_key, RG_PATH_SEPARATORS);
    }
    if (!app_name.len) {
        app_name = app_key;
    }
    if (default_password.len && !default_username.len) {
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
    if (!default_username.len) {
        default_username = Prompt("User: ", &temp_alloc);
        if (!default_username.len)
            return 1;
    }
    if (!default_password.len) {
        default_password = Prompt("Password: ", "*", &temp_alloc);
        if (!default_password.len)
            return 1;
    }
    LogInfo();

    // Create base directories
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

        if (!make_profile_directory("files"))
            return 1;
        if (!make_profile_directory("files/pages"))
            return 1;
    }

    // Create default pages
    if (!empty) {
        for (const AssetInfo &asset: pack_assets) {
            RG_ASSERT(asset.compression_type == CompressionType::None);

            const char *filename = Fmt(&temp_alloc, "%1%/files/%2", profile_directory, asset.name).ptr;
            if (!WriteFile(asset.data, filename))
                return 1;
            files.Append(filename);

#ifndef _WIN32
            if (change_owner && !ChangeFileOwner(filename, owner_uid, owner_gid))
                return 1;
#endif
        }
    }

    // Create database
    sq_Database database;
    {
        const char *filename = Fmt(&temp_alloc, "%1%/database.db", profile_directory).ptr;
        files.Append(filename);

        if (!database.Open(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
            return 1;
        if (!RunMigrations(database, 0))
            return 1;

#ifndef _WIN32
        if (change_owner && !ChangeFileOwner(filename, owner_uid, owner_gid))
            return 1;
#endif
    }

    // Create default user (admin)
    {
        char hash[crypto_pwhash_STRBYTES];
        if (!HashPassword(default_password, hash))
            return 1;

        uint32_t permissions = (1u << RG_LEN(UserPermissionNames)) - 1;

        if (!database.Run("INSERT INTO usr_users (username, password_hash, permissions) VALUES (?, ?, ?)",
                          default_username, hash, permissions))
            return 1;
    }

    // Write configuration file
    {
        const char *filename = Fmt(&temp_alloc, "%1%/goupile.ini", profile_directory).ptr;
        files.Append(filename);

        StreamWriter st(filename);
        Print(&st, DefaultConfig, app_key, app_name);
        if (!st.Close())
            return 1;

         // This one keeps the default owner uid/gid even with --owner
    }

    // Make sure database is OK!
    if (!database.Close())
        return 1;

    out_guard.Disable();
    return 0;
}

static bool OpenProfileDatabase(const char *config_filename, sq_Database *out_database)
{
    if (!config_filename) {
        config_filename = "goupile.ini";

        if (!TestFile(config_filename, FileType::File)) {
            LogError("Configuration file must be specified");
            return false;
        }
    }

    Config config;
    if (!LoadConfig(config_filename, &config))
        return false;

    // Open database
    if (!config.database_filename) {
        LogError("Database file not specified");
        return false;
    }
    if (!out_database->Open(config.database_filename, SQLITE_OPEN_READWRITE))
        return false;

    return true;
}

static int RunMigrate(Span<const char *> arguments)
{
    // Options
    const char *config_filename = nullptr;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+goupile_admin migrate [options]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file)");
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

    // Open database
    sq_Database database;
    if (!OpenProfileDatabase(config_filename, &database))
        return 1;

    // Get schema version
    int version;
    {
        sq_Statement stmt;
        if (!database.Prepare("PRAGMA user_version;", &stmt))
            return 1;
        RG_ASSERT(stmt.Next());

        version = sqlite3_column_int(stmt, 0);
    }

    LogInfo("Previous version: %1", version);
    if (version == RG_LEN(MigrationRequests)) {
        LogInfo("Database is up to date");
        return 0;
    }
    if (!RunMigrations(database, version))
        return 1;

    return 0;
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
    Span<const char> password = nullptr;
    uint32_t permissions = UINT32_MAX;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+goupile_admin add_user [options] <username>%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

        %!..+--password <pwd>%!0         Password of user
    %!..+-p, --permissions <perms>%!0    User permissions

User permissions: %!..+%1%!0)", FmtSpan(UserPermissionNames));
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
    sq_Database database;
    if (!OpenProfileDatabase(config_filename, &database))
        return 1;

    // Find user first
    {
        sq_Statement stmt;
        if (!database.Prepare("SELECT permissions FROM usr_users WHERE username = ?", &stmt))
            return false;
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

        if (stmt.Next()) {
            LogError("User '%1' already exists", username);
            return 1;
        }
    }

    // Gather missing information
    if (!password.len) {
        password = Prompt("Password: ", "*", &temp_alloc);
        if (!password.len)
            return 1;
    }
    if (permissions == UINT32_MAX) {
        Span<const char> str = Prompt("Permissions: ", &temp_alloc);
        if (str.len && !ParsePermissionList(str, &permissions))
            return 1;
    }
    LogInfo();

    // Hash password
    char hash[crypto_pwhash_STRBYTES];
    if (!HashPassword(password, hash))
        return 1;

    // Create user
    if (!database.Run("INSERT INTO usr_users (username, password_hash, permissions) VALUES (?, ?, ?)",
                      username, hash, permissions))
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
    Span<const char> password = nullptr;
    uint32_t permissions = UINT32_MAX;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+goupile_admin edit_user [options] <username>%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

        %!..+--password <pwd>%!0         Password of user
    %!..+-p, --permissions <perms>%!0    User permissions

User permissions: %!..+%1%!0)", FmtSpan(UserPermissionNames));
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
    sq_Database database;
    if (!OpenProfileDatabase(config_filename, &database))
        return 1;

    // Find user first
    uint32_t prev_permissions;
    {
        sq_Statement stmt;
        if (!database.Prepare("SELECT permissions FROM usr_users WHERE username = ?", &stmt))
            return false;
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

        if (!stmt.Next()) {
            LogError("User '%1' does not exist", username);
            return 1;
        }

        prev_permissions = (uint32_t)sqlite3_column_int64(stmt, 0);
    }

    // Gather missing information
    if (!password.ptr) {
        password = Prompt("Password: ", "*", &temp_alloc);
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
    bool success = database.Transaction([&]() {
        if (password.len) {
            char hash[crypto_pwhash_STRBYTES];
            if (!HashPassword(password, hash))
                return false;

            if (!database.Run("UPDATE usr_users SET password_hash = ? WHERE username = ?",
                              hash, username))
                return false;
        }

        if (!database.Run("UPDATE usr_users SET permissions = ? WHERE username = ?",
                          permissions, username))
            return false;
        if (!sqlite3_changes(database)) {
            LogError("UPDATE request failed: no match");
            return false;
        }

        return true;
    });
    if (!success)
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
        PrintLn(fp, R"(Usage: %!..+goupile_admin remove_user [options] <username>%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file)");
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

    // Open database
    sq_Database database;
    if (!OpenProfileDatabase(config_filename, &database))
        return 1;

    // Delete user
    if (!database.Run("DELETE FROM usr_users WHERE username = ?", username))
        return 1;
    if (!sqlite3_changes(database)) {
        LogError("User '%1' does not exist", username);
        return 1;
    }

    LogInfo("Removed user");
    return 0;
}

int Main(int argc, char **argv)
{
    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+goupile_admin <command> [args]%!0

General commands:
    %!..+init%!0                         Create new profile
    %!..+migrate%!0                      Migrate existing profile

User commands:
    %!..+add_user%!0                     Add new user
    %!..+edit_user%!0                    Edit existing user
    %!..+remove_user%!0                  Remove existing user)");
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
        PrintLn("%!R..goupile_admin%!0 %1", FelixVersion);
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
