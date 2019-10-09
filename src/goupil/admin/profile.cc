// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
    #include <direct.h>
    #include <io.h>
#else
    #include <unistd.h>
#endif

#include "../../libcc/libcc.hh"
#include "profile.hh"
#include "../server/data.hh"

namespace RG {

static const char *const DefaultConfig =
R"([Data]
ProjectKey = %1
DatabaseFile = %2

# [HTTP]
# IPStack = Dual
# Port = 8888
# Threads = 4
# BaseUrl = /
)";

int RunCreateProfile(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    Span<const char> project_key = {};
    bool demo = false;
    const char *profile_directory = nullptr;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupil_admin create_profile [options] profile_directory

Options:
    -k, --key <key>              Change project key
                                 (default: directory name)

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
                project_key = opt.current_value;
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
    if (!project_key.len) {
        project_key = TrimStrRight((Span<const char>)profile_directory, RG_PATH_SEPARATORS);
        project_key = SplitStrReverseAny(project_key, RG_PATH_SEPARATORS);
    }

    if (!MakeDirectory(profile_directory))
        return 1;

    // Drop created files and directories if anything fails
    HeapArray<const char *> directories;
    HeapArray<const char *> files;
    RG_DEFER_N(out_guard) {
        for (const char *filename: files) {
            unlink(filename);
        }
        for (Size i = directories.len - 1; i >= 0; i--) {
            rmdir(directories[i]);
        }
        if (rmdir(profile_directory) < 0) {
            LogError("Failed to remove directory '%1': %2", profile_directory, strerror(errno));
        }
    };

    // Create database
    const char *database_name = "database.db";
    {
        const char *filename = Fmt(&temp_alloc, "%1%/%2", profile_directory, database_name).ptr;
        files.Append(filename);

        SQLiteDatabase database;
        if (!database.Open(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
            return 1;
        if (!database.CreateSchema())
            return 1;

        if (demo && !database.InsertDemo())
            return 1;
    }

    // Create configuration file
    {
        const char *filename = Fmt(&temp_alloc, "%1%/goupil.ini", profile_directory).ptr;
        files.Append(filename);

        StreamWriter st(filename);
        Print(&st, DefaultConfig, project_key, database_name);
        if (!st.Close())
            return 1;
    }

    out_guard.Disable();
    return 0;
}

}
