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

struct SaveRequest {
    const char *channel = nullptr;
    HeapArray<const char *> filenames;
};

static bool LoadFromFile(const char *filename, Allocator *alloc, HeapArray<SaveRequest> *out_saves)
{
    RG_DEFER_NC(out_guard, len = out_saves->len) { out_saves->RemoveFrom(len); };

    StreamReader st(filename);

    if (!st.IsValid())
        return false;

    Span<const char> root_directory = GetPathDirectory(filename);
    root_directory = NormalizePath(root_directory, GetWorkingDirectory(), alloc);

    IniParser ini(&st);
    ini.PushLogFilter();
    RG_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            SaveRequest save = {};

            if (!prop.section.len) {
                LogError("Property is outside section");
                return false;
            }

            save.channel = DuplicateString(prop.section, alloc).ptr;

            do {
                if (prop.key == "SourcePath") {
                    const char *path = NormalizePath(prop.value, root_directory, alloc).ptr;
                    save.filenames.Append(path);
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } while (ini.NextInSection(&prop));

            if (!save.filenames.len) {
                LogError("Missing source path");
                valid = false;
            }

            out_saves->Append(save);
        }
    }
    if (!ini.IsValid() || !valid)
        return false;

    out_guard.Disable();
    return true;
}

int RunSave(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    rk_SaveSettings settings;
    const char *from = nullptr;
    bool raw = false;
    HeapArray<SaveRequest> saves;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 save [-C filename] [option...] channel path...%!0
       %!..+%1 save [-C filename] [option...] --from file%!0
       %!..+%1 save [-C filename] [option...] --raw path...%!0
)", FelixTarget);
        PrintLn(st, CommonOptions);
        PrintLn(st, R"(
Save options:

    %!..+-F, --from file%!0                Use channel names and paths from file

    %!..+-f, --force%!0                    Check all files even if mtime/size match previous backup
        %!..+--follow%!0                   Follow symbolic links (instead of storing them as-is)
        %!..+--noatime%!0                  Do not modify atime if possible (Linux-only)

    %!..+-m, --meta metadata%!0            Save additional directory/file metadata, see below

        %!..+--raw%!0                      Skip snapshot object and report data OID

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
            } else if (opt.Test("-F", "--from", OptionType::Value)) {
                from = opt.current_value;
            } else if (opt.Test("-f", "--force")) {
                settings.skip = false;
            } else if (opt.Test("--follow")) {
                settings.follow = true;
            } else if (opt.Test("--noatime")) {
                settings.noatime = true;
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
            } else if (opt.Test("--raw")) {
                raw = true;
            } else if (!HandleCommonOption(opt)) {
                return 1;
            }
        }

        if (!from) {
            SaveRequest save = {};

            if (!raw) {
                save.channel = opt.ConsumeNonOption();
            }
            opt.ConsumeNonOptions(&save.filenames);

            saves.Append(save);
        }

        opt.LogUnusedArguments();
    }

    if (from) {
        if (raw) {
            LogError("Option --raw cannot be used with --from");
            return 1;
        }

        if (!LoadFromFile(from, &temp_alloc, &saves))
            return 1;

        if (!saves.len) {
            LogError("Missing save information in '%1'", from);
            return 1;
        }
    } else {
        RG_ASSERT(saves.len == 1);

        const SaveRequest &save = saves[0];

        if (!raw && !save.channel) {
            LogError("No channel provided");
            return 1;
        }
        if (!save.filenames.len) {
            LogError("No filename provided");
            return 1;
        }
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

    bool complete = true;

    for (const SaveRequest &save: saves) {
        int64_t now = GetMonotonicTime();

        rk_SaveInfo info = {};
        if (!rk_Save(repo.get(), save.channel, save.filenames, settings, &info)) {
            complete = false;
            continue;
        }

        double time = (double)(GetMonotonicTime() - now) / 1000.0;

        LogInfo();
        if (raw) {
            LogInfo("Data OID: %!..+%1%!0", info.oid);
        } else {
            LogInfo("Snapshot channel: %!..+%1%!0", save.channel);
            LogInfo("Snapshot OID: %!..+%1%!0", info.oid);
        }
        LogInfo("Source size: %!..+%1%!0", FmtDiskSize(info.size));
        LogInfo("Total stored: %!..+%1%!0 (added %2)", FmtDiskSize(info.stored), FmtDiskSize(info.added));
        LogInfo("Execution time: %!..+%1s%!0", FmtDouble(time, 1));
    }

    return !complete;
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

