// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "drdc.hh"
#include "config.hh"

namespace RG {

int RunMcoClassify(Span<const char *> arguments);
int RunMcoDump(Span<const char *> arguments);
int RunMcoList(Span<const char *> arguments);
int RunMcoMap(Span<const char *> arguments);
int RunMcoPack(Span<const char *> arguments);
int RunMcoShow(Span<const char *> arguments);

const char *const CommonOptions =
R"(Common options:
    %!..+-C, --config_file <file>%!0     Set configuration file
                                 %!D..(default: <executable_dir>%/profile%/drdc.ini)%!0

        %!..+--profile_dir <dir>%!0      Set profile directory
        %!..+--table_dir <dir>%!0        Add table directory

        %!..+--mco_auth_file <file>%!0   Set MCO authorization file
                                 %!D..(default: <profile_dir>%/mco_authorizations.ini
                                           <profile_dir>%/mco_authorizations.txt)%!0

    %!..+-s, --sector <sector>%!0        Use Public or Private sector GHS and prices
                                 %!D..(default: Public)%!0)";

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
        if (!OptionToEnum(drd_SectorNames, opt.current_value, &drdc_config.sector)) {
            LogError("Unknown sector '%1'", opt.current_value);
            return false;
        }
    } else {
        LogError("Cannot handle option '%1'", opt.current_option);
        return false;
    }

    return true;
}

int Main(int argc, char **argv)
{
    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+drdc <command> [args]%!0
)");
        PrintLn(fp, CommonOptions);
        PrintLn(fp, R"(
Commands:
    %!..+mco_classify%!0                 Classify MCO stays
    %!..+mco_dump%!0                     Dump available MCO tables and lists
    %!..+mco_list%!0                     Export MCO diagnosis and procedure lists
    %!..+mco_map%!0                      Compute GHM accessibility constraints
    %!..+mco_pack%!0                     Pack MCO stays for quicker loads
    %!..+mco_show%!0                     Print information about individual MCO elements
                                 (diagnoses, procedures, GHM roots, etc.))");
    };

    if (argc < 2) {
        LogError("No command provided");
        return 1;
    }

    const char *cmd = argv[1];
    Span<const char *> arguments((const char **)argv + 2, argc - 2);

    // Handle help and version arguments
    if (TestStr(cmd, "--help") || TestStr(cmd, "help")) {
        if (arguments.len && arguments[0][0] != '-') {
            cmd = arguments[0];
            arguments[0] = (cmd[0] == '-') ? cmd : "--help";
        } else {
            print_usage(stdout);
            return 0;
        }
    } else if (TestStr(cmd, "--version")) {
        PrintLn("drdc %1", FelixVersion);
        return 0;
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
            if (TestStr(cmd, RG_STRINGIFY(Cmd))) { \
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

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
