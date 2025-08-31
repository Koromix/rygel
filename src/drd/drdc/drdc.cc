// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "drdc.hh"
#include "config.hh"

namespace K {

int RunMcoClassify(Span<const char *> arguments);
int RunMcoDump(Span<const char *> arguments);
int RunMcoList(Span<const char *> arguments);
int RunMcoMap(Span<const char *> arguments);
int RunMcoPack(Span<const char *> arguments);
int RunMcoShow(Span<const char *> arguments);

const char *const CommonOptions =
R"(Common options:

    %!..+-C, --config_file filename%!0     Set configuration file
                                   %!D..(default: drdc.ini)%!0

        %!..+--profile_dir directory%!0    Set profile directory
        %!..+--table_dir directory%!0      Add table directory

        %!..+--mco_auth_file filename%!0   Set MCO authorization file
                                   %!D..(default: <profile_dir>%/mco_authorizations.ini
                                             <profile_dir>%/mco_authorizations.txt)%!0

    %!..+-s, --sector sector%!0            Use Public or Private sector GHS and prices
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
        if (!OptionToEnumI(drd_SectorNames, opt.current_value, &drdc_config.sector)) {
            LogError("Unknown sector '%1'", opt.current_value);
            return false;
        }
    } else {
        opt.LogUnknownError();
        return false;
    }

    return true;
}

int Main(int argc, char **argv)
{
    BlockAllocator temp_alloc;

    // Global options
    const char *config_filename = "drdc.ini";

    const auto print_usage = [](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 command [arg...]%!0
)", FelixTarget);
        PrintLn(st, CommonOptions);
        PrintLn(st, R"(
Commands:

    %!..+mco_classify%!0                   Classify MCO stays
    %!..+mco_dump%!0                       Dump available MCO tables and lists
    %!..+mco_list%!0                       Export MCO diagnosis and procedure lists
    %!..+mco_map%!0                        Compute GHM accessibility constraints
    %!..+mco_pack%!0                       Pack MCO stays for quicker loads
    %!..+mco_show%!0                       Print information about individual MCO elements
                                   (diagnoses, procedures, GHM roots, etc.)

Use %!..+%1 help command%!0 or %!..+%1 command --help%!0 for more specific help.)", FelixTarget);
    };

    if (argc < 2) {
        print_usage(StdErr);
        PrintLn(StdErr);
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
            print_usage(StdOut);
            return 0;
        }
    } else if (TestStr(cmd, "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn(T("Compiler: %1"), FelixCompiler);
        return 0;
    }

    // Find config filename
    {
        OptionParser opt(arguments, OptionMode::Skip);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                // Don't try to load anything in this case
                config_filename = nullptr;
                break;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                if (IsDirectory(opt.current_value)) {
                    config_filename = Fmt(&temp_alloc, "%1%/drdc.ini", TrimStrRight(opt.current_value, K_PATH_SEPARATORS)).ptr;
                } else {
                    config_filename = opt.current_value;
                }
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

#define HANDLE_COMMAND(Cmd, Func, ReadConfig) \
        do { \
            if (TestStr(cmd, K_STRINGIFY(Cmd))) { \
                bool load = (ReadConfig) && TestFile(config_filename); \
                 \
                if (load && !LoadConfig(config_filename, &drdc_config)) \
                    return 1; \
                 \
                return Func(arguments); \
            } \
        } while (false)

    HANDLE_COMMAND(mco_classify, RunMcoClassify, true);
    HANDLE_COMMAND(mco_dump, RunMcoDump, true);
    HANDLE_COMMAND(mco_list, RunMcoList, true);
    HANDLE_COMMAND(mco_map, RunMcoMap, true);
    HANDLE_COMMAND(mco_pack, RunMcoPack, false);
    HANDLE_COMMAND(mco_show, RunMcoShow, true);

#undef HANDLE_COMMAND

    LogError("Unknown command '%1'", cmd);
    return 1;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return K::RunApp(argc, argv); }
