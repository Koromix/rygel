// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "drdc.hh"
#include "config.hh"

int RunMcoClassify(Span<const char *> arguments);
int RunMcoDump(Span<const char *> arguments);
int RunMcoList(Span<const char *> arguments);
int RunMcoMap(Span<const char *> arguments);
int RunMcoPack(Span<const char *> arguments);
int RunMcoShow(Span<const char *> arguments);

const char *const CommonOptions =
R"(Common options:
     -C, --config_file <file>     Set configuration file
                                  (default: <executable_dir>%/profile%/drdc.ini)

         --profile_dir <dir>      Set profile directory
         --table_dir <dir>        Add table directory

         --mco_auth_file <file>   Set MCO authorization file
                                  (default: <profile_dir>%/mco_authorizations.ini
                                            <profile_dir>%/mco_authorizations.txt)

     -s, --sector <sector>        Use Public or Private sector GHS and prices
                                  (default: Public))";

Config drdc_config;

bool HandleCommonOption(OptionParser &opt)
{
    if (opt.Test("-C", "--config_file", OptionType::Value)) {
        // Already handled
    } else if (opt.Test("--profile_dir", OptionType::Value)) {
        drdc_config.profile_directory = opt.current_value;
    } else if (opt.Test("--table_dir", OptionType::Value)) {
        drdc_config.table_directories.Append(opt.current_value);
    } else if (opt.Test("--mco_auth_file", OptionType::Value)) {
        drdc_config.mco_authorization_filename = opt.current_value;
    } else if (opt.Test("-s", "--sector", OptionType::Value)) {
        const char *const *ptr = FindIf(drd_SectorNames,
                                        [&](const char *name) { return TestStr(name, opt.current_value); });
        if (!ptr) {
            LogError("Unknown sector '%1'", opt.current_value);
            return false;
        }

        drdc_config.sector = (drd_Sector)(ptr - drd_SectorNames);
    } else {
        LogError("Cannot handle option '%1'", opt.current_option);
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

        if (!config_filename) {
            const char *app_directory = GetApplicationDirectory();
            if (app_directory) {
                const char *test_filename = Fmt(&drdc_config.str_alloc, "%1%/profile/drdc.ini", app_directory).ptr;
                if (TestFile(test_filename, FileType::File)) {
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
                return Func(arguments); \
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
