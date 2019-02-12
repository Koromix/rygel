// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
    #include <direct.h>
#else

#endif

#include "../../libcc/libcc.hh"
#include "config.hh"

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

static bool RunInit(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: goupil_admin init [options] profile_directory)");
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
    init                         Init goupil profile)");
    };

    if (argc < 2) {
        PrintUsage(stderr);
        return 1;
    }

    const char *cmd = argv[1];
    Span<const char *> arguments((const char **)argv + 2, argc - 2);

    // Handle 'drdc help [command]' and 'drdc --help [command]' invocations
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

    HANDLE_COMMAND(init, RunInit);

#undef HANDLE_COMMAND

    LogError("Unknown command '%1'", cmd);
    return 1;
}
