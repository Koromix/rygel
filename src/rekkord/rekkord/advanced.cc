// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "rekkord.hh"
#include "../librekkord/priv_repository.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

int RunChangeID(Span<const char *> arguments)
{
    // Options
    rk_Config config;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 change_id [-C <config>]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

    %!..+-R, --repository <dir>%!0       Set repository directory
    %!..+-u, --user <user>%!0            Set repository username
        %!..+--password <pwd>%!0         Set repository password)", FelixTarget);
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
            } else if (opt.Test("--password", OptionType::Value)) {
                config.password = opt.current_value;
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

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), rk_DiskModeNames[(int)disk->GetMode()]);
    LogInfo();

    if (!disk->ChangeID())
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
R"(Usage: %!..+%1 rebuild_cache [-C <config>]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

    %!..+-R, --repository <dir>%!0       Set repository directory)", FelixTarget);
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
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (!config.Complete(false))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_Open(config, false);
    if (!disk)
        return 1;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), rk_DiskModeNames[(int)disk->GetMode()]);
    LogInfo();

    LogInfo("Rebuilding cache...");
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
R"(Usage: %!..+%1 migrate_tags [-C <config>]%!0

Options:
    %!..+-C, --config_file <file>%!0     Set configuration file

    %!..+-R, --repository <dir>%!0       Set repository directory
    %!..+-u, --user <user>%!0            Set repository username
        %!..+--password <pwd>%!0         Set repository password)", FelixTarget);
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
            } else if (opt.Test("--password", OptionType::Value)) {
                config.password = opt.current_value;
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

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), rk_DiskModeNames[(int)disk->GetMode()]);
    if (disk->GetMode() != rk_DiskMode::Full) {
        LogError("You must use the read-write password with this command");
        return 1;
    }
    LogInfo();

    LogInfo("Migrating tags...");

    HeapArray<const char *> filenames;
    {
        bool success = disk->ListRaw("tags", [&](const char *filename) {
            filename = DuplicateString(filename, &temp_alloc).ptr;
            filenames.Append(filename);

            return true;
        });
        if (!success)
            return 1;
    }

    Async async(disk->GetThreads(), false);
    int migrations = 0;

    for (const char *filename: filenames) {
        Span<const char> basename = SplitStrReverseAny(filename, RG_PATH_SEPARATORS);

        if (basename.len != 8)
            continue;
        migrations++;

        async.Run([=, &disk]() {
            rk_Hash hash = {};
            {
                uint8_t blob[crypto_box_SEALBYTES + 32];
                Size len = disk->ReadRaw(filename, blob);

                if (len != crypto_box_SEALBYTES + 32) {
                    if (len >= 0) {
                        LogError("Malformed tag file '%1' (ignoring)", filename);
                    }
                    return true;
                }

                if (crypto_box_seal_open(hash.hash, blob, RG_SIZE(blob), disk->GetWriteKey().ptr, disk->GetFullKey().ptr) != 0) {
                    LogError("Failed to unseal tag (ignoring)");
                    return true;
                }
            }

            HeapArray<uint8_t> blob;
            {
                rk_BlobType type;
                if (!disk->ReadBlob(hash, &type, &blob))
                    return false;

                if (type != rk_BlobType::Snapshot1) {
                    LogError("Blob '%1' is not a Snapshot (ignoring)", hash);
                    return true;
                }
                if (blob.len <= RG_SIZE(SnapshotHeader1)) {
                    LogError("Malformed snapshot blob '%1' (ignoring)", hash);
                    return true;
                }
            }

            // Migrate to new snapshot header
            SnapshotHeader2 header = {};
            {
                const SnapshotHeader1 *header1 = (const SnapshotHeader1 *)blob.ptr;

                header.time = header1->time;
                header.len = header1->len;
                header.stored = header1->stored;
                MemCpy(header.name, header1->name, RG_SIZE(header.name));
            }

            Size payload_len = RG_OFFSET_OF(SnapshotHeader2, name) + strlen(header.name) + 1;
            Span<const uint8_t> payload = MakeSpan((const uint8_t *)&header, payload_len);

            if (!disk->WriteTag(hash, payload))
                return false;
            disk->DeleteRaw(filename);

            return true;
        });
    }

    if (!async.Sync())
        return 1;

    LogInfo("Migrated %1 tags", migrations);
    return 0;
}

}
