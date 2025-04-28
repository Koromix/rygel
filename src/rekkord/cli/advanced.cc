// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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
#include "../lib/disk_priv.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

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

int RunRebuildCache(Span<const char *> arguments)
{
    // Options
    rk_Config config;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 rebuild_cache [-C filename] [option...]%!0

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
    LogInfo();

    if (!disk->OpenCache(false))
        return 1;
    if (!disk->RebuildCache())
        return 1;
    LogInfo("Done");

    return 0;
}

int RunMigrateTags(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_Config config;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 migrate_tags [-C filename] [option...]%!0

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
    rk_KeySet *keyset = disk->GetKeySet();

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), disk->GetRole());
    if (!disk->HasMode(rk_AccessMode::Write)) {
        LogError("Cannot migrate tags with %1 role", disk->GetRole());
        return 1;
    }
    LogInfo();

    HeapArray<const char *> filenames;
    {
        bool success = disk->ListRaw("tags", [&](const char *path) {
            const char *filename = DuplicateString(path, &temp_alloc).ptr;
            filenames.Append(filename);

            return true;
        });
        if (!success)
            return false;
    }

    for (Size i = 0; i < filenames.len; i++) {
        const char *filename = filenames[i];
        Span<const char> basename = SplitStrReverseAny(filename, RG_PATH_SEPARATORS);

        HeapArray<uint8_t> cypher;
        {
            cypher.Reserve(basename.len);

            size_t len = 0;
            if (sodium_base642bin(cypher.ptr, cypher.capacity, basename.ptr, basename.len, nullptr,
                                  &len, nullptr, sodium_base64_VARIANT_URLSAFE_NO_PADDING) < 0) {
                LogError("Invalid base64 string in tag '%1'", filename);
                continue;
            }
            cypher.len = (Size)len;

            if (cypher.len < (Size)crypto_box_SEALBYTES + RG_SIZE(TagIntro)) {
                LogError("Truncated cypher in tag");
                continue;
            }
        }

        Size data_len = cypher.len - crypto_box_SEALBYTES;
        Span<uint8_t> data = AllocateSpan<uint8_t>(&temp_alloc, data_len);

        if (crypto_box_seal_open(data.ptr, cypher.ptr, cypher.len, keyset->tkey, keyset->lkey) != 0) {
            LogError("Failed to unseal tag data from '%1'", basename);
            continue;
        }

        TagIntro intro = {};
        MemCpy(&intro, data.ptr, RG_SIZE(intro));

        if (intro.version != 1) {
            LogError("Unexpected tag version %1 (expected %2) in '%3'", intro.version, TagVersion, basename);
            continue;
        }

        rk_Hash hash = intro.hash;
        Span<const uint8_t> payload = data.Take(RG_SIZE(intro), data.len - RG_SIZE(intro));

        if (!disk->WriteTag(hash, payload))
            continue;
        disk->DeleteRaw(filename);

        LogInfo("Migrated tag '%1'", filename);
    }

    return 0;
}

}
