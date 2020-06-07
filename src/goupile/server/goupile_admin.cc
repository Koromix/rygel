// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "../../core/libwrap/sqlite.hh"

namespace RG {

static const char *const DefaultConfig =
R"([Application]
Key = %1
Name = %2

[Data]
FilesDirectory = files
DatabaseFile = database.db

[Sync]
# UseOffline = Off
AllowGuests = On

# [HTTP]
# IPStack = Dual
# Port = 8888
# Threads = 4
# BaseUrl = /
)";

static const char *const SchemaSQL = R"(
CREATE TABLE users (
    username TEXT NOT NULL,
    password_hash TEXT NOT NULL,

    admin INTEGER CHECK(admin IN (0, 1)) NOT NULL
);
CREATE UNIQUE INDEX users_u ON users (username);

CREATE TABLE permissions (
    username TEXT NOT NULL,

    read INTEGER CHECK(read IN (0, 1)) NOT NULL,
    query INTEGER CHECK(query IN (0, 1)) NOT NULL,
    new INTEGER CHECK(new IN (0, 1)) NOT NULL,
    remove INTEGER CHECK(remove IN (0, 1)) NOT NULL,
    edit INTEGER CHECK(edit IN (0, 1)) NOT NULL,
    validate INTEGER CHECK(validate IN (0, 1)) NOT NULL
);
CREATE INDEX permissions_u ON permissions (username);

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
    data TEXT NOT NULL,
    complete TEXT NOT NULL
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

static const char *const DemoSQL = R"(
BEGIN TRANSACTION;

INSERT INTO users VALUES ('goupile', '$argon2id$v=19$m=65536,t=2,p=1$zsVerrO6LpOnY46D2B532A$dXWo9OKKutuZZzN49HD+oGtjCp6vfIoINfmbsjq5ttI', 1);
INSERT INTO permissions VALUES ('goupile', 1, 1, 1, 1, 1, 1);

INSERT INTO sched_resources VALUES ('pl', '2019-08-01', 730, 1, 1);
INSERT INTO sched_resources VALUES ('pl', '2019-08-01', 1130, 2, 0);
INSERT INTO sched_resources VALUES ('pl', '2019-08-02', 730, 1, 1);
INSERT INTO sched_resources VALUES ('pl', '2019-08-02', 1130, 2, 0);
INSERT INTO sched_resources VALUES ('pl', '2019-08-05', 730, 1, 1);
INSERT INTO sched_resources VALUES ('pl', '2019-08-05', 1130, 2, 0);
INSERT INTO sched_resources VALUES ('pl', '2019-08-06', 730, 1, 1);
INSERT INTO sched_resources VALUES ('pl', '2019-08-06', 1130, 2, 0);
INSERT INTO sched_resources VALUES ('pl', '2019-08-07', 730, 1, 1);
INSERT INTO sched_resources VALUES ('pl', '2019-08-07', 1130, 2, 0);

INSERT INTO sched_meetings VALUES ('pl', '2019-08-01', 730, 'Gwen STACY');
INSERT INTO sched_meetings VALUES ('pl', '2019-08-01', 730, 'Peter PARKER');
INSERT INTO sched_meetings VALUES ('pl', '2019-08-01', 730, 'Mary JANE PARKER');
INSERT INTO sched_meetings VALUES ('pl', '2019-08-02', 730, 'Clark KENT');
INSERT INTO sched_meetings VALUES ('pl', '2019-08-02', 1130, 'Lex LUTHOR');

END TRANSACTION;
)";

static int RunCreate(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    Span<const char> app_key = {};
    Span<const char> app_name = {};
    bool demo = false;
    const char *profile_directory = nullptr;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupile_admin create_profile [options] profile_directory

Options:
    -k, --key <key>              Change application key
                                 (default: directory name)
        --name <name>            Change application name
                                 (default: project key)

        --demo                   Insert fake data in profile)");
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
            } else if (opt.Test("--demo")) {
                demo = true;
            } else {
                LogError("Cannot handle option '%1'", opt.current_option);
                return 1;
            }
        }

        profile_directory = opt.ConsumeNonOption();
    }

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

    // Create files directory
    {
        const char *directory = Fmt(&temp_alloc, "%1%/files", profile_directory).ptr;
        if (!MakeDirectory(directory))
            return 1;
        directories.Append(directory);
    }

    // Create database
    {
        const char *filename = Fmt(&temp_alloc, "%1%/database.db", profile_directory).ptr;
        files.Append(filename);

        sq_Database database;
        if (!database.Open(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
            return 1;

        if (!database.Run(SchemaSQL))
            return 1;
        if (demo && !database.Run(DemoSQL))
            return 1;
    }

    // Create configuration file
    {
        const char *filename = Fmt(&temp_alloc, "%1%/goupile.ini", profile_directory).ptr;
        files.Append(filename);

        StreamWriter st(filename);
        Print(&st, DefaultConfig, app_key, app_name);
        if (!st.Close())
            return 1;
    }

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
