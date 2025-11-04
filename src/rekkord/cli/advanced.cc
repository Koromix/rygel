// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/native/base/base.hh"
#include "rekkord.hh"

namespace K {

int RunChangeCID(Span<const char *> arguments)
{
    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 change_cid [-C filename] [option...]%!0)"), FelixTarget);
        PrintCommonOptions(st);
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

    if (!rk_config.Complete())
        return 1;
    if (!rk_config.Validate())
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rk_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rk_config, true);
    if (!repo)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
    if (!repo->HasMode(rk_AccessMode::Config)) {
        LogError("Cannot change cache ID with %1 key", repo->GetRole());
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
    bool list = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
T(R"(Usage: %!..+%1 reset_cache [-C filename] [option...]%!0)"), FelixTarget);
        PrintCommonOptions(st);
        PrintLn(st, T(R"(
Cache options:

        %!..+--list%!0                     List existing blobs)"));
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("--list")) {
                list = true;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!rk_config.Complete())
        return 1;
    if (!rk_config.Validate())
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rk_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rk_config, true);
    if (!repo)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
    LogInfo();

    rk_Cache cache;
    if (!cache.Open(repo.get(), false))
        return false;

    LogInfo("Resetting cache...");
    if (!cache.Reset(list))
        return 1;
    if (!cache.Close())
        return 1;
    LogInfo("Done");

    return 0;
}

}
