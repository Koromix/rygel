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
#include "rekkord.hh"

namespace RG {

int RunChangeCID(Span<const char *> arguments)
{
    // Options
    rk_Config config;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 change_cid [-C filename] [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file

    %!..+-R, --repository URL%!0           Set repository URL
    %!..+-u, --user username%!0            Set repository username)", FelixTarget);
    };

    if (!FindAndLoadConfig(arguments, &config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-R", "--repository", OptionType::Value)) {
                if (!rk_DecodeURL(opt.current_value, &config))
                    return 1;
            } else if (opt.Test("-u", "--username", OptionType::Value)) {
                config.username = opt.current_value;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!config.Complete(true))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), config, true);
    if (!repo)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
    if (!repo->HasMode(rk_AccessMode::Config)) {
        LogError("Cannot change ID with %1 role", repo->GetRole());
        return 1;
    }
    LogInfo();

    if (!repo->ChangeCID())
        return 1;

    LogInfo("Changed cache ID");

    return 0;
}

int RunResetCache(Span<const char *> arguments)
{
    // Options
    rk_Config config;
    bool list = true;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 reset_cache [-C filename] [option...]%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file

    %!..+-R, --repository URL%!0           Set repository URL
    %!..+-u, --user username%!0            Set repository username

        %!..+--clear%!0                    Skip list of existing blobs)", FelixTarget);
    };

    if (!FindAndLoadConfig(arguments, &config))
        return 1;

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-C", "--config_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-R", "--repository", OptionType::Value)) {
                if (!rk_DecodeURL(opt.current_value, &config))
                    return 1;
            } else if (opt.Test("-u", "--username", OptionType::Value)) {
                config.username = opt.current_value;
            } else if (opt.Test("--clear")) {
                list = false;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!config.Complete(true))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), config, true);
    if (!repo)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
    LogInfo();

    LogInfo("Resetting cache...");
    if (!repo->OpenCache(false))
        return 1;
    if (!repo->ResetCache(list))
        return 1;
    LogInfo("Done");

    return 0;
}

}
