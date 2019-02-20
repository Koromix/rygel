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

bool RunCreateProfile(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupil_admin create_profile [options] profile_directory)");
    };

    const char *directory = nullptr;
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                PrintUsage(stdout);
                return true;
            } else {
                return false;
            }
        }

        directory = opt.ConsumeNonOption();
    }

    if (!directory) {
        LogError("Profile directory is missing");
        return false;
    }

    // Create profile directory
    if (!MakeDirectory(directory))
        return false;

    // Profile layout
    HeapArray<const char *> directories;
    HeapArray<const char *> files;
    const char *database_filename;
    directories.Append(Fmt(&temp_alloc, "%1%/pages", directory).ptr);
    directories.Append(Fmt(&temp_alloc, "%1%/templates", directory).ptr);
    database_filename = *files.Append(Fmt(&temp_alloc, "%1%/database.db", directory).ptr);

    // Drop profile directory if anything fails
    DEFER_N(out_guard) {
        for (const char *dir: directories) {
            rmdir(dir);
        }
        for (const char *filename: files) {
            unlink(filename);
        }

        if (rmdir(directory) < 0) {
            LogError("Failed to remove directory '%1': %2", directory, strerror(errno));
        }
    };

    // Create directory layout
    {
        bool valid = true;

        for (const char *dir: directories) {
            valid &= MakeDirectory(dir);
        }

        if (!valid)
            return false;
    }

    // Create database
    {
        SQLiteConnection db;

        if (!db.Open(database_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
            return false;
        if (!InitDatabase(db))
            return false;
    }

    out_guard.disable();
    return true;
}
