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
#include "../../felix/libpack/libpack.hh"

extern "C" const Span<const pack_Asset> pack_assets;

int RunCreateProfile(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *profile_directory = nullptr;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupil_admin create_profile [options] profile_directory)");
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                PrintUsage(stdout);
                return 0;
            } else {
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
    DEFER_N(out_guard) {
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

    // Create profile directories
    {
        HashSet<Span<const char>> directories_map;

        for (const pack_Asset &asset: pack_assets) {
            DebugAssert(!PathIsAbsolute(asset.name));
            DebugAssert(!PathContainsDotDot(asset.name));

            const char *ptr = asset.name;
            while ((ptr = strchr(ptr, '/'))) {
                Span<const char> directory = MakeSpan(asset.name, ptr - asset.name);

                if (directories_map.Append(directory).second) {
                    const char *directory0 = Fmt(&temp_alloc, "%1%/%2", profile_directory,
                                                 MakeSpan(asset.name, ptr - asset.name)).ptr;
                    if (!MakeDirectory(directory0))
                        return 1;
                    directories.Append(directory0);
                }

                ptr++;
            }
        }
    }

    // Extract files
    for (const pack_Asset &asset: pack_assets) {
        const char *filename = Fmt(&temp_alloc, "%1%/%2", profile_directory, asset.name).ptr;
        files.Append(filename);

        StreamReader reader(asset.data, nullptr, asset.compression_type);
        StreamWriter writer(filename);
        if (!SpliceStream(&reader, Megabytes(8), &writer))
            return 1;
    }

    // Create database
    {
        const char *filename = Fmt(&temp_alloc, "%1%/database.db", profile_directory).ptr;
        files.Append(filename);

        SQLiteConnection db;
        if (!db.Open(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
            return 1;
        if (!InitDatabase(db))
            return 1;
    }

    out_guard.Disable();
    return 0;
}
