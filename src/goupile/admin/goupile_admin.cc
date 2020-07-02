// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "../../core/libwrap/sqlite.hh"

#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

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
# OfflineRecords = Off
# AllowGuests = Off

# [HTTP]
# IPStack = Dual
# Port = 8889
# Threads = 4
# BaseUrl = /
)";

static const char *const SchemaSQL = R"(
CREATE TABLE users (
    username TEXT NOT NULL,
    password_hash TEXT NOT NULL,

    develop INTEGER CHECK(develop IN (0, 1)) NOT NULL,
    new INTEGER CHECK(new IN (0, 1)) NOT NULL,
    edit INTEGER CHECK(edit IN (0, 1)) NOT NULL,
    offline INTEGER CHECK(offline IN (0, 1)) NOT NULL
);
CREATE UNIQUE INDEX users_u ON users (username);

CREATE TABLE files (
    tag TEXT NOT NULL,
    path TEXT NOT NULL,
    size INTEGER NOT NULL,
    sha256 TEXT NOT NULL,
    data BLOB NOT NULL
);
CREATE UNIQUE INDEX files_tp ON files (tag, path);

CREATE TABLE records (
    id TEXT NOT NULL,
    form TEXT NOT NULL,
    sequence INTEGER NOT NULL,
    data TEXT NOT NULL
);
CREATE UNIQUE INDEX records_i ON records (id);
CREATE INDEX records_f ON records (form);

CREATE TABLE records_complete (
    form TEXT NOT NULL,
    page TEXT NOT NULL,
    complete INTEGER CHECK(complete IN (0, 1)) NOT NULL
);
CREATE UNIQUE INDEX records_complete_fp ON records_complete(form, page);

CREATE TABLE records_variables (
    form TEXT NOT NULL,
    key TEXT NOT NULL,
    page TEXT NOT NULL,
    before TEXT,
    after TEXT
);
CREATE UNIQUE INDEX records_variables_fk ON records_variables (form, key);

CREATE TABLE records_sequences (
    form TEXT NOT NULL,
    sequence INTEGER NOT NULL
);
CREATE UNIQUE INDEX records_sequences_f ON records_sequences (form);

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
)";

static int RunCreate(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    Span<const char> app_key = {};
    Span<const char> app_name = {};
    bool empty = false;
    Span<const char> default_username = nullptr;
    Span<const char> default_password = nullptr;
    const char *profile_directory = nullptr;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupile_admin create [options] profile_directory

Options:
    -k, --key <key>              Change application key
                                 (default: directory name)
        --name <name>            Change application name
                                 (default: project key)

    -u, --user <name>            Name of default user
    -p, --password <pwd>         Password of default user

        --empty                  Don't create default files)");
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
            } else if (opt.Test("-p", "--password", OptionType::Value)) {
                default_password = opt.current_value;
            } else if (opt.Test("--empty")) {
                empty = true;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        profile_directory = opt.ConsumeNonOption();
    }

    if (sodium_init() < 0) {
        LogError("Failed to initialize libsodium");
        return 1;
    }

    // Errors and defaults
    if (!profile_directory) {
        LogError("Profile directory is missing");
        return 1;
    }
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

    // If we can make it, it's a good start!
    if (!MakeDirectory(profile_directory))
        return 1;

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
        UnlinkDirectory(profile_directory);
    };

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

    // Create base directories
    {
        const auto make_profile_directory = [&](const char *name) {
            const char *directory = Fmt(&temp_alloc, "%1%/%2", profile_directory, name).ptr;
            if (!MakeDirectory(directory))
                return false;

            directories.Append(directory);
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
        }
    }

    // Create database
    sq_Database database;
    {
        const char *filename = Fmt(&temp_alloc, "%1%/database.db", profile_directory).ptr;
        files.Append(filename);

        if (!database.Open(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
            return 1;
        if (!database.Run(SchemaSQL))
            return 1;
    }

    // Create default user
    {
        char hash[crypto_pwhash_STRBYTES];
        if (crypto_pwhash_str(hash, default_password.ptr, default_password.len,
                              crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
            LogError("Failed to hash password");
            return 1;
        }

        if (!database.Run("INSERT INTO users (username, password_hash, develop, new, edit, offline) VALUES (?, ?, 1, 1, 1, 1)",
                          default_username, hash))
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
    }

    // Make sure database is OK!
    if (!database.Close())
        return 1;

    out_guard.Disable();
    return 0;
}

int Main(int argc, char **argv)
{
    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupile_admin <command> [<args>]

Commands:
    create                       Create new profile)");
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
        PrintLn("goupile_admin %1", FelixVersion);
        return 0;
    }

    if (TestStr(cmd, "create")) {
        return RunCreate(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
