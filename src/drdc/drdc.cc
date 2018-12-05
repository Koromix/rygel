// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "drdc.hh"
#include "config.hh"

bool RunMcoClassify(Span<const char *> arguments);
bool RunMcoDump(Span<const char *> arguments);
bool RunMcoList(Span<const char *> arguments);
bool RunMcoMap(Span<const char *> arguments);
bool RunMcoPack(Span<const char *> arguments);
bool RunMcoShow(Span<const char *> arguments);

const char *const CommonOptions =
R"(Common options:
     -C, --config_file <file>     Set configuration file
                                  (default: <executable_dir>%/profile%/drdc.ini)

         --profile_dir <dir>      Set profile directory
         --table_dir <dir>        Add table directory
         --auth_file <file>       Set authorization file
                                  (default: <profile_dir>%/mco_authorizations.ini
                                            <profile_dir>%/mco_authorizations.txt))";

Config drdc_config;

bool HandleCommonOption(OptionParser &opt_parser)
{
    if (opt_parser.TestOption("-C", "--config_file")) {
        // Already handled
        opt_parser.ConsumeValue();
    } else if (opt_parser.TestOption("--profile_dir")) {
        drdc_config.profile_directory = opt_parser.RequireValue();
        if (!drdc_config.profile_directory)
            return false;
    } else if (opt_parser.TestOption("--table_dir")) {
        if (!opt_parser.RequireValue())
            return false;

        drdc_config.table_directories.Append(opt_parser.current_value);
    } else if (opt_parser.TestOption("--auth_file")) {
        drdc_config.authorization_filename = opt_parser.RequireValue();
        if (!drdc_config.authorization_filename)
            return false;
    } else {
        LogError("Unknown option '%1'", opt_parser.current_option);
        return false;
    }

    return true;
}

int main(int argc, char **argv)
{
    static const auto PrintUsage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: drdc <command> [<args>]
)");
        PrintLn(fp, CommonOptions);
        PrintLn(fp, R"(
Commands:
    mco_classify                 Classify MCO stays
    mco_dump                     Dump available MCO tables and lists
    mco_list                     Export MCO diagnosis and procedure lists
    mco_map                      Compute GHM accessibility constraints
    mco_pack                     Pack MCO stays for quicker loads
    mco_show                     Print information about individual MCO elements
                                 (diagnoses, procedures, GHM roots, etc.))");
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
        OptionParser opt_parser(arguments, (int)OptionParser::Flag::SkipNonOptions);

        while (opt_parser.Next()) {
            if (opt_parser.TestOption("--help")) {
                // Don't try to load anything in this case
                config_filename = nullptr;
                break;
            } else if (opt_parser.TestOption("-C", "--config_file")) {
                config_filename = opt_parser.RequireValue();
                if (!config_filename)
                    return 1;
            }
        }

        if (!config_filename) {
            const char *app_directory = GetApplicationDirectory();
            if (app_directory) {
                const char *test_filename = Fmt(&drdc_config.str_alloc, "%1%/profile/drdc.ini", app_directory).ptr;
                if (TestPath(test_filename, FileType::File)) {
                    config_filename = test_filename;
                }
            }
        }
    }

#define HANDLE_COMMAND(Cmd, Func) \
        do { \
            if (TestStr(cmd, STRINGIFY(Cmd))) { \
                if (config_filename && !LoadConfig(config_filename, &drdc_config)) \
                    return 1; \
                 \
                return !Func(arguments); \
            } \
        } while (false)

    HANDLE_COMMAND(mco_classify, RunMcoClassify);
    HANDLE_COMMAND(mco_dump, RunMcoDump);
    HANDLE_COMMAND(mco_list, RunMcoList);
    HANDLE_COMMAND(mco_map, RunMcoMap);
    HANDLE_COMMAND(mco_pack, RunMcoPack);
    HANDLE_COMMAND(mco_show, RunMcoShow);

#undef HANDLE_COMMAND

    LogError("Unknown command '%1'", cmd);
    return 1;
}
