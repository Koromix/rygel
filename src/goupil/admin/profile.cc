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
R"([Resources]
DatabaseFile = %1

# [HTTP]
# IPStack = Dual
# Port = 8888
# Threads = 4
# BaseUrl = /
)";

static const char *const DemoSQL =
R"(BEGIN TRANSACTION;

INSERT INTO sc_resources VALUES ('pl', '2019-04-01', 730, 1, 1);
INSERT INTO sc_resources VALUES ('pl', '2019-04-01', 1130, 2, 0);
INSERT INTO sc_resources VALUES ('pl', '2019-04-02', 730, 1, 1);
INSERT INTO sc_resources VALUES ('pl', '2019-04-02', 1130, 2, 0);
INSERT INTO sc_resources VALUES ('pl', '2019-04-03', 730, 1, 1);
INSERT INTO sc_resources VALUES ('pl', '2019-04-03', 1130, 2, 0);
INSERT INTO sc_resources VALUES ('pl', '2019-04-04', 730, 1, 1);
INSERT INTO sc_resources VALUES ('pl', '2019-04-04', 1130, 2, 0);
INSERT INTO sc_resources VALUES ('pl', '2019-04-05', 730, 1, 1);
INSERT INTO sc_resources VALUES ('pl', '2019-04-05', 1130, 2, 0);

INSERT INTO sc_identities VALUES (1, 'Gwen', 'STACY', NULL, '1989-01-02');
INSERT INTO sc_identities VALUES (2, 'Peter', 'PARKER', NULL, '1990-04-08');
INSERT INTO sc_identities VALUES (3, 'Mary', 'JANE', 'PARKER', '1991-03-01');
INSERT INTO sc_identities VALUES (4, 'Clark', 'KENT', NULL, '1987-09-22');
INSERT INTO sc_identities VALUES (5, 'Lex', 'LUTHOR', NULL, '1985-07-17');

INSERT INTO sc_meetings VALUES ('pl', '2019-04-01', 730, 1);
INSERT INTO sc_meetings VALUES ('pl', '2019-04-01', 730, 2);
INSERT INTO sc_meetings VALUES ('pl', '2019-04-01', 730, 3);
INSERT INTO sc_meetings VALUES ('pl', '2019-04-02', 730, 4);
INSERT INTO sc_meetings VALUES ('pl', '2019-04-02', 1130, 5);

END TRANSACTION;
)";

int RunCreateProfile(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    bool dev = false;
    bool demo = false;
    const char *profile_directory = nullptr;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupil_admin create_profile [options] profile_directory

Options:
       --dev                     Create developer-oriented profile
       --demo                    Insert fake data in profile)");
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                PrintUsage(stdout);
                return 0;
            } else if (opt.Test("--dev")) {
                dev = true;
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
    const char *database_name;
    if (dev) {
        database_name = "database_dev.sql";

        const char *filename = Fmt(&temp_alloc, "%1%/%2", profile_directory, database_name).ptr;
        files.Append(filename);

        if (!WriteFile(demo ? DemoSQL : "", filename))
            return 1;
    } else {
        database_name = "database.db";

        const char *filename = Fmt(&temp_alloc, "%1%/%2", profile_directory, database_name).ptr;
        files.Append(filename);

        SQLiteDatabase database;
        if (!database.Open(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
            return 1;
        if (!database.CreateSchema())
            return 1;

        if (demo && !database.Execute(DemoSQL))
            return 1;
    }

    // Create configuration file
    {
        const char *filename = Fmt(&temp_alloc, "%1%/goupil.ini", profile_directory).ptr;
        files.Append(filename);

        StreamWriter st(filename);
        Print(&st, DefaultConfig, database_name);
        if (!st.Close())
            return 1;
    }

    out_guard.Disable();
    return 0;
}

}
