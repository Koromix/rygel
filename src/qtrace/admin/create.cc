// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "create.hh"
#include "../../core/libwrap/sqlite.hh"

namespace RG {

static const char *const DefaultConfig =
R"([Data]
DatabaseFile = database.db

# [HTTP]
# IPStack = Dual
# Port = 8888
# Threads = 4
# BaseUrl = /
)";

static const char *const SchemaSQL = R"(
)";

static const char *const DemoSQL = R"(
BEGIN TRANSACTION;

END TRANSACTION;
)";

int RunCreate(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    bool demo = false;
    const char *profile_directory = nullptr;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: qtrace_admin create_profile [options] profile_directory

Options:
        --demo                   Insert fake data in profile)");
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
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
        const char *filename = Fmt(&temp_alloc, "%1%/qtrace.ini", profile_directory).ptr;
        files.Append(filename);

        StreamWriter st(filename);
        Print(&st, DefaultConfig);
        if (!st.Close())
            return 1;
    }

    out_guard.Disable();
    return 0;
}

}
