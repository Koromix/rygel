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

namespace RG {

int RunSave(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_Config config;
    rk_PutSettings settings;
    HeapArray<const char *> filenames;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 save [-C filename] [option...] path...%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file

    %!..+-R, --repository URL%!0           Set repository URL
    %!..+-u, --user username%!0            Set repository username

    %!..+-n, --name name%!0                Set custom snapshot name
        %!..+--raw%!0                      Skip snapshot object and report data hash

        %!..+--follow_symlinks%!0          Follow symbolic links (instead of storing them as-is)
        %!..+--preserve_atime%!0           Do not modify atime if possible (Linux-only)

    %!..+-j, --threads threads%!0          Change number of threads
                                   %!D..(default: automatic)%!0)", FelixTarget);
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
            } else if (opt.Test("-n", "--name", OptionType::Value)) {
                settings.name = opt.current_value;
            } else if (opt.Test("--follow_symlinks")) {
                settings.follow_symlinks = true;
            } else if (opt.Test("--preserve_atime")) {
                settings.preserve_atime = true;
            } else if (opt.Test("--raw")) {
                settings.raw = true;
            } else if (opt.Test("-j", "--threads", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &config.threads))
                    return 1;
                if (config.threads < 1) {
                    LogError("Threads count cannot be < 1");
                    return 1;
                }
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.ConsumeNonOptions(&filenames);
        opt.LogUnusedArguments();
    }

    if (!filenames.len) {
        LogError("No filename provided");
        return 1;
    }

    if (!settings.name && !settings.raw && filenames.len > 1) {
        LogError("You must use an explicit name with %!..+-n name%!0 to save multiple paths");
        return 1;
    }

    if (!config.Complete(true))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_Open(config, true);
    if (!disk)
        return 1;

    if (!settings.raw) {
        const char *username = disk->GetUser();
        RG_ASSERT(username);

        if (!settings.name) {
            Span<char> path = NormalizePath(filenames[0], &temp_alloc);

            if (path.len > rk_MaxSnapshotNameLength) {
                path.len = rk_MaxSnapshotNameLength;
                path.ptr[path.len] = 0;

                LogWarning("Truncating snapshot name to '%1'", path);
            }

            settings.name = path.ptr;
        }
    }

    ZeroSafe((void *)config.password, strlen(config.password));
    config.password = nullptr;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), rk_DiskModeNames[(int)disk->GetMode()]);
    if (disk->GetMode() != rk_DiskMode::WriteOnly) {
        LogWarning("You should prefer write-only users for this command");
    }
    LogInfo();

    LogInfo("Backing up...");

    int64_t now = GetMonotonicTime();

    rk_Hash hash = {};
    int64_t total_size = 0;
    int64_t total_written = 0;
    if (!rk_Put(disk.get(), settings, filenames, &hash, &total_size, &total_written))
        return 1;

    double time = (double)(GetMonotonicTime() - now) / 1000.0;

    LogInfo();
    if (settings.raw) {
        LogInfo("Data hash: %!..+%1%!0", hash);
    } else {
        LogInfo("Snapshot hash: %!..+%1%!0", hash);
        LogInfo("Snapshot name: %!..+%1%!0", settings.name);
    }
    LogInfo("Source size: %!..+%1%!0", FmtDiskSize(total_size));
    LogInfo("Total written: %!..+%1%!0", FmtDiskSize(total_written));
    LogInfo("Execution time: %!..+%1s%!0", FmtDouble(time, 1));

    return 0;
}

int RunRestore(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_Config config;
    rk_GetSettings settings;
    const char *dest_filename = nullptr;
    const char *identifier = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 restore [-C filename] [option...] -O path identifier%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file

    %!..+-R, --repository URL%!0           Set repository URL
    %!..+-u, --user username%!0            Set repository username

    %!..+-O, --output path%!0              Restore file or directory to path

    %!..+-f, --force%!0                    Overwrite destination files
        %!..+--delete%!0                   Delete extraneous files from destination

        %!..+--chown%!0                    Restore original file UID and GID

    %!..+-v, --verbose%!0                  Show detailed actions
    %!..+-n, --dry_run%!0                  Fake file restoration

    %!..+-j, --threads threads%!0          Change number of threads
                                   %!D..(default: automatic)%!0

Use an object hash or a snapshot name as the identifier. You can append an optional path (separated by a colon), the full syntax for object identifiers is %!..+<hash|name>[:<path>]%!0.
Snapshot names are not unique, if you use a snapshot name, rekkord will use the most recent snapshot object that matches.)", FelixTarget);
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
            } else if (opt.Test("-O", "--output", OptionType::Value)) {
                dest_filename = opt.current_value;
            } else if (opt.Test("-f", "--force")) {
                settings.force = true;
            } else if (opt.Test("--delete")) {
                settings.unlink = true;
            } else if (opt.Test("--chown")) {
                settings.chown = true;
            } else if (opt.Test("-v", "--verbose")) {
                settings.verbose = true;
            } else if (opt.Test("-n", "--dry_run")) {
                settings.fake = true;
            } else if (opt.Test("-j", "--threads", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &config.threads))
                    return 1;
                if (config.threads < 1) {
                    LogError("Threads count cannot be < 1");
                    return 1;
                }
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        identifier = opt.ConsumeNonOption();
        opt.LogUnusedArguments();
    }

    if (!identifier) {
        LogError("No identifier provided");
        return 1;
    }
    if (!dest_filename) {
        LogError("Missing destination filename");
        return 1;
    }

    if (!config.Complete(true))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_Open(config, true);
    if (!disk)
        return 1;

    ZeroSafe((void *)config.password, strlen(config.password));
    config.password = nullptr;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), rk_DiskModeNames[(int)disk->GetMode()]);
    if (disk->GetMode() != rk_DiskMode::Full) {
        LogError("Cannot decrypt with write-only access");
        return 1;
    }
    LogInfo();

    LogInfo("Restoring...");

    int64_t now = GetMonotonicTime();

    rk_Hash hash = {};
    if (!rk_Locate(disk.get(), identifier, &hash))
        return 1;

    int64_t file_len = 0;
    if (!rk_Get(disk.get(), hash, settings, dest_filename, &file_len))
        return 1;

    double time = (double)(GetMonotonicTime() - now) / 1000.0;

    LogInfo();
    LogInfo("Restored: %!..+%1%!0 (%2)", dest_filename, FmtDiskSize(file_len));
    if (!settings.fake) {
        LogInfo("Execution time: %!..+%1s%!0", FmtDouble(time, 1));
    } else {
        LogInfo("Execution time: %!..+%1s%!0 %!D..[dry run]%!0", FmtDouble(time, 1));
    }

    return 0;
}

}

