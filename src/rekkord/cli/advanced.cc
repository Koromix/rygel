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
    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 change_cid [-C filename] [option...]%!0
)", FelixTarget);
        PrintLn(st, CommonOptions);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!rekkord_config.Complete(true))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rekkord_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rekkord_config, true);
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
    bool list = true;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 reset_cache [-C filename] [option...]%!0
)", FelixTarget);
        PrintLn(st, CommonOptions);
        PrintLn(st, R"(
Cache options:

        %!..+--clear%!0                    Skip list of existing blobs)");
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("--clear")) {
                list = false;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!rekkord_config.Complete(true))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rekkord_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rekkord_config, true);
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
