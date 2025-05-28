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
#include "../lib/repository_priv.hh"

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

    std::unique_ptr<rk_Disk> disk = rk_Open(config, true);
    if (!disk)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), disk->GetRole());
    if (!disk->HasMode(rk_AccessMode::Config)) {
        LogError("Cannot change ID with %1 role", disk->GetRole());
        return 1;
    }
    LogInfo();

    if (!disk->ChangeCID())
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

    std::unique_ptr<rk_Disk> disk = rk_Open(config, true);
    if (!disk)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), disk->GetRole());
    LogInfo();

    LogInfo("Resetting cache...");

    if (!disk->OpenCache(false))
        return 1;
    if (!disk->ResetCache(list))
        return 1;
    LogInfo("Done");

    return 0;
}

static inline bool IsHexadecimalString(Span<const char> str)
{
    for (char c: str) {
        if (c >= '0' && c <= '9')
            continue;
        if (c >= 'A' && c <= 'F')
            continue;
        if (c >= 'a' && c <= 'f')
            continue;

        return false;
    }

    return true;
}

static inline char TypeToCatalog(int type)
{
    switch (type) {
        case (int)BlobType::Chunk:
        case (int)BlobType::File:
        case (int)BlobType::Link: return rk_BlobCatalogNames[(int)rk_BlobCatalog::Raw];

        case (int)BlobType::Directory1:
        case (int)BlobType::Snapshot1:
        case (int)BlobType::Snapshot2:
        case (int)BlobType::Directory2:
        case (int)BlobType::Snapshot3:
        case (int)BlobType::Directory:
        case (int)BlobType::Snapshot: return rk_BlobCatalogNames[(int)rk_BlobCatalog::Meta];
    }

    return 0;
}

int RunMigrateBlobs(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_Config config;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 migrate_blobs [-C filename] [option...]%!0

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

    std::unique_ptr<rk_Disk> disk = rk_Open(config, true);
    if (!disk)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), disk->GetRole());
    if (!disk->HasMode(rk_AccessMode::Config) || !disk->HasMode(rk_AccessMode::Read)) {
        LogError("Cannot migrate blobs with %1 role", disk->GetRole());
        return 1;
    }
    LogInfo();

    LogInfo("Creating blob catalogs...");
    for (char catalog: rk_BlobCatalogNames) {
        char parent[128];
        Fmt(parent, "blobs/%1", catalog);

        if (!disk->CreateDirectory(parent))
            return 1;

        for (int i = 0; i < 256; i++) {
            char name[128];
            Fmt(name, "%1/%2", parent, FmtHex(i).Pad0(-2));

            if (!disk->CreateDirectory(name))
                return 1;
        }
    }

    LogInfo("Moving blobs...");
    {
        ProgressHandle progress("Moving");
        std::atomic_int64_t processed { 0 };

        Async async(disk->GetAsync());

        bool success = disk->ListRaw("blobs", [&](const char *path, int64_t) {
            if (!StartsWith(path, "blobs/"))
                return true;

            Span<const char> name;
            Span<const char> prefix = SplitStr(path + 6, '/', &name);

            // Already processed
            if (prefix.len == 2 && prefix[0] >= 'A' && prefix[0] <= 'Z')
                return true;

            if (prefix.len != 2 || !IsHexadecimalString(prefix)) {
                LogWarning("Skipping blob '%1'", path);
                return true;
            }
            if (name.len != 64 || !IsHexadecimalString(name)) {
                LogWarning("Skipping blob '%1'", path);
                return true;
            }

            char *src = DuplicateString(path, &temp_alloc).ptr;
            char *dest = Fmt(&temp_alloc, "blobs/?/%1/%2", prefix, name).ptr;

            async.Run([&, src, dest] {
                int type;
                HeapArray<uint8_t> blob;

                if (!disk->ReadBlob(src, &type, &blob))
                    return false;

                char catalog = TypeToCatalog(type);
                if (!catalog)
                    return false;
                dest[6] = catalog;

                if (!disk->WriteBlob(dest, type, blob))
                    return false;
                if (!disk->DeleteRaw(src))
                    return false;

                int64_t done = processed.fetch_add(1) + 1;
                progress.SetFmt("%1 moved", done);

                return true;
            });

            return true;
        });
        if (!success)
            return 1;

        if (!async.Sync())
            return 1;
    }

    LogInfo("Deleting old blob directories...");
    for (int i = 0; i < 256; i++) {
        char name[128];
        Fmt(name, "blobs/%1", FmtHex(i).Pad0(-2));

        if (!disk->DeleteDirectory(name))
            return 1;
    }

    LogInfo("Changing cache ID...");
    if (!disk->ChangeCID())
        return 1;

    LogInfo("Done");

    return 0;
}

}
