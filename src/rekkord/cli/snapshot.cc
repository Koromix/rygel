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

int RunSave(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_Config config;
    rk_SaveSettings settings;
    HeapArray<const char *> filenames;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 save [-C filename] [option...] path...%!0

Options:

    %!..+-C, --config_file filename%!0     Set configuration file

    %!..+-R, --repository URL%!0           Set repository URL
    %!..+-u, --user username%!0            Set repository username

    %!..+-c, --channel channel%!0          Set custom snapshot channel
                                   %!D..(default: %2)%!0
        %!..+--raw%!0                      Skip snapshot object and report data OID

        %!..+--follow%!0                   Follow symbolic links (instead of storing them as-is)
        %!..+--noatime%!0                  Do not modify atime if possible (Linux-only)

    %!..+-m, --meta metadata%!0            Save additional directory/file metadata, see below

    %!..+-j, --threads threads%!0          Change number of threads
                                   %!D..(default: automatic)%!0

Available metadata save options:

    %!..+ATime%!0                          Store atime (access time) values
    %!..+XAttrs%!0                         Store extended attributes and ACLs (when supported))",
                FelixTarget, rk_DefaultSnapshotChannel);
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
            } else if (opt.Test("-c", "--channel", OptionType::Value)) {
                settings.channel = opt.current_value;
            } else if (opt.Test("--follow")) {
                settings.follow = true;
            } else if (opt.Test("-m", "--meta", OptionType::Value)) {
                Span<const char> remain = opt.current_value;

                while (remain.len) {
                    Span<const char> part = TrimStr(SplitStrAny(remain, " ,", &remain));

                    if (part.len) {
                        if (TestStrI(part, "ATime")) {
                            settings.atime = true;
                        } else if (TestStrI(part, "XAttrs")) {
                            settings.xattrs = true;
                        } else {
                            LogError("Unknown option specified for --meta");
                            return 1;
                        }
                    }
                }
            } else if (opt.Test("--noatime")) {
                settings.noatime = true;
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

    if (!config.Complete(true))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), config, true);
    if (!repo)
        return 1;

    ZeroSafe((void *)config.password, strlen(config.password));
    config.password = nullptr;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
    if (!repo->HasMode(rk_AccessMode::Write)) {
        LogError("Cannot save data with %1 role", repo->GetRole());
        return 1;
    }
    if (repo->HasMode(rk_AccessMode::Read)) {
        LogWarning("You should prefer write-only users for this command");
    }
    LogInfo();

    LogInfo("Backing up...");

    int64_t now = GetMonotonicTime();

    rk_ObjectID oid = {};
    int64_t total_size = 0;
    int64_t total_stored = 0;
    if (!rk_Save(repo.get(), settings, filenames, &oid, &total_size, &total_stored))
        return 1;

    double time = (double)(GetMonotonicTime() - now) / 1000.0;

    LogInfo();
    if (settings.raw) {
        LogInfo("Data OID: %!..+%1%!0", oid);
    } else {
        LogInfo("Snapshot OID: %!..+%1%!0", oid);
        LogInfo("Snapshot channel: %!..+%1%!0", settings.channel);
    }
    LogInfo("Source size: %!..+%1%!0", FmtDiskSize(total_size));
    LogInfo("Total stored: %!..+%1%!0", FmtDiskSize(total_stored));
    LogInfo("Execution time: %!..+%1s%!0", FmtDouble(time, 1));

    return 0;
}

int RunRestore(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_Config config;
    rk_RestoreSettings settings;
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

    %!..+-m, --meta metadata%!0            Restore additional directory/file metadata, see below

    %!..+-v, --verbose%!0                  Show detailed actions
    %!..+-n, --dry_run%!0                  Fake file restoration

    %!..+-j, --threads threads%!0          Change number of threads
                                   %!D..(default: automatic)%!0

Available metadata restoration options:

    %!..+Owner%!0                          Restore original file owner and group (UID and GID)
    %!..+XAttrs%!0                         Restore extended attributes and ACLs (when supported)

Use an object ID (OID) or a snapshot channel as the identifier. You can append an optional path (separated by a colon), the full syntax for object identifiers is %!..+<OID|channel>[:<path>]%!0.
If you use a snapshot channel, rekkord will use the most recent snapshot object that matches.)", FelixTarget);
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
            } else if (opt.Test("-m", "--meta", OptionType::Value)) {
                Span<const char> remain = opt.current_value;

                while (remain.len) {
                    Span<const char> part = TrimStr(SplitStrAny(remain, " ,", &remain));

                    if (part.len) {
                        if (TestStrI(part, "Owner")) {
                            settings.chown = true;
                        } else if (TestStrI(part, "XAttrs")) {
                            settings.xattrs = true;
                        } else {
                            LogError("Unknown option specified for --meta");
                            return 1;
                        }
                    }
                }
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

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), config, true);
    if (!repo)
        return 1;

    ZeroSafe((void *)config.password, strlen(config.password));
    config.password = nullptr;

    LogInfo("Repository: %!..+%1%!0 (%2)", disk->GetURL(), repo->GetRole());
    if (!repo->HasMode(rk_AccessMode::Read)) {
        LogError("Cannot restore data with %1 role", repo->GetRole());
        return 1;
    }
    LogInfo();

    LogInfo("Restoring...");

    int64_t now = GetMonotonicTime();

    rk_ObjectID oid = {};
    if (!rk_LocateObject(repo.get(), identifier, &oid))
        return 1;

    int64_t file_len = 0;
    if (!rk_Restore(repo.get(), oid, settings, dest_filename, &file_len))
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

int RunCheck(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_Config config;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 check [-C filename] [option...]%!0

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
    if (!repo->HasMode(rk_AccessMode::Read)) {
        LogError("Cannot check repository with %1 role", repo->GetRole());
        return 1;
    }
    LogInfo();

    HeapArray<rk_SnapshotInfo> snapshots;
    if (!rk_ListSnapshots(repo.get(), &temp_alloc, &snapshots))
        return 1;

    LogInfo("Checking repository...");
    if (!rk_CheckSnapshots(repo.get(), snapshots))
        return 1;
    LogInfo("Done");

    return 0;
}

}

