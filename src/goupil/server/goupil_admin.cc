// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
    #include <direct.h>
#else
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

#include "../../libcc/libcc.hh"
#include "config.hh"
#include "sqlite.hh"

static Config goupil_config;

static const char *const CommonOptions =
R"(Common options:
     -C, --config_file <file>     Set configuration file
     -P, --profile_dir <dir>      Set profile directory)";

static bool HandleCommonOption(OptionParser &opt)
{
    if (opt.Test("-C", "--config_file", OptionType::Value)) {
        // Already handled
    } else if (opt.Test("-P", "--profile_dir", OptionType::Value)) {
        goupil_config.profile_directory = opt.current_value;
    } else {
        LogError("Cannot handle option '%1'", opt.current_option);
        return false;
    }

    return true;
}

static bool MakeDirectory(const char *dir)
{
#ifdef _WIN32
    int ret = _mkdir(dir);
#else
    int ret = mkdir(dir, 0755);
#endif
    if (ret < 0) {
        LogError("Cannot create directory '%1': %2", dir, strerror(errno));
        return false;
    }

    return true;
}

static bool RunCreate(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupil_admin create [options] profile_directory)");
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
        sqlite3 *db = OpenDatabase(database_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
        if (!db)
            return false;
        DEFER { sqlite3_close(db); };

        if (!InitDatabase(db))
            return false;
    }

    out_guard.disable();
    return true;
}

int main(int argc, char **argv)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupil_admin <command> [<args>]
)");
        PrintLn(fp, CommonOptions);
        PrintLn(fp, R"(
Commands:
    create                       Create new profile)");
    };

    if (argc < 2) {
        PrintUsage(stderr);
        return 1;
    }

    const char *cmd = argv[1];
    Span<const char *> arguments((const char **)argv + 2, argc - 2);

    // Handle 'goupil_admin help [command]' and 'goupil_admin --help [command]' invocations
    if (TestStr(cmd, "--help") || TestStr(cmd, "help")) {
        if (arguments.len && arguments[0][0] != '-') {
            cmd = arguments[0];
            arguments[0] = "--help";
        } else {
            PrintUsage(stdout);
            return 0;
        }
    }

    const char *config_filename = nullptr;
    {
        OptionParser opt(arguments, (int)OptionParser::Flag::SkipNonOptions);

        while (opt.Next()) {
            if (opt.Test("--help", OptionType::OptionalValue)) {
                // Don't try to load anything in this case
                config_filename = nullptr;
                break;
            } else if (opt.Test("-C", "--config_file", OptionType::OptionalValue)) {
                config_filename = opt.current_value;
            }
        }
    }

#define HANDLE_COMMAND(Cmd, Func) \
        do { \
            if (TestStr(cmd, STRINGIFY(Cmd))) { \
                if (config_filename && !LoadConfig(config_filename, &goupil_config)) \
                    return 1; \
                 \
                return !Func(arguments); \
            } \
        } while (false)

    HANDLE_COMMAND(create, RunCreate);

#undef HANDLE_COMMAND

    LogError("Unknown command '%1'", cmd);
    return 1;
}
