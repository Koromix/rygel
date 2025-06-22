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
    rk_SaveSettings settings;
    bool raw = false;
    const char *channel = nullptr;
    HeapArray<const char *> filenames;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 save [-C filename] [option...] channel path...%!0
       %!..+%1 save [-C filename] [option...] --raw path...%!0
)", FelixTarget);
        PrintLn(st, CommonOptions);
        PrintLn(st, R"(
Save options:

        %!..+--raw%!0                      Skip snapshot object and report data OID

        %!..+--follow%!0                   Follow symbolic links (instead of storing them as-is)
        %!..+--noatime%!0                  Do not modify atime if possible (Linux-only)

    %!..+-m, --meta metadata%!0            Save additional directory/file metadata, see below

    %!..+-j, --threads threads%!0          Change number of threads
                                   %!D..(default: automatic)%!0

Available metadata save options:

    %!..+ATime%!0                          Store atime (access time) values
    %!..+XAttrs%!0                         Store extended attributes and ACLs (when supported))");
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
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
                raw = true;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        if (!raw) {
            channel = opt.ConsumeNonOption();
        }
        opt.ConsumeNonOptions(&filenames);

        opt.LogUnusedArguments();
    }

    if (!raw && !channel) {
        LogError("No channel provided");
        return 1;
    }
    if (!filenames.len) {
        LogError("No filename provided");
        return 1;
    }

    if (!rekkord_config.Complete(true))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rekkord_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rekkord_config, true);
    if (!repo)
        return 1;

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

    rk_SaveInfo info = {};
    if (!rk_Save(repo.get(), channel, filenames, settings, &info))
        return 1;

    double time = (double)(GetMonotonicTime() - now) / 1000.0;

    LogInfo();
    if (raw) {
        LogInfo("Data OID: %!..+%1%!0", info.oid);
    } else {
        LogInfo("Snapshot OID: %!..+%1%!0", info.oid);
        LogInfo("Snapshot channel: %!..+%1%!0", channel);
    }
    LogInfo("Source size: %!..+%1%!0", FmtDiskSize(info.size));
    LogInfo("Total stored: %!..+%1%!0 (added %2)", FmtDiskSize(info.stored), FmtDiskSize(info.added));
    LogInfo("Execution time: %!..+%1s%!0", FmtDouble(time, 1));

    return 0;
}

int RunRestore(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_RestoreSettings settings;
    const char *identifier = nullptr;
    const char *dest_filename = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 restore [-C filename] [option...] identifier destination%!0
)", FelixTarget);
        PrintLn(st, CommonOptions);
        PrintLn(st, R"(
Restore options:

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
If you use a snapshot channel, the most recent snapshot object that matches will be used.)");
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
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
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        identifier = opt.ConsumeNonOption();
        dest_filename = opt.ConsumeNonOption();

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

    if (!rekkord_config.Complete(true))
        return 1;

    std::unique_ptr<rk_Disk> disk = rk_OpenDisk(rekkord_config);
    std::unique_ptr<rk_Repository> repo = rk_OpenRepository(disk.get(), rekkord_config, true);
    if (!repo)
        return 1;

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

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 check [-C filename] [option...]%!0
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

