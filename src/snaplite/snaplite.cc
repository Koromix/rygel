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
// along with this program. If not, see https://www.gnu.org/licenses/.

#include "src/core/libcc/libcc.hh"
#include "src/core/libsqlite/libsqlite.hh"

namespace RG {

int RunTorture(Span<const char *> arguments);

static bool ListSnapshotFiles(OptionParser *opt, bool recursive,
                              BlockAllocator *alloc, HeapArray<const char *> *out_filenames)
{
    if (recursive) {
        for (const char *filename; (filename = opt->ConsumeNonOption()); ) {
            FileInfo file_info;
            if (StatFile(filename, &file_info) != StatResult::Success)
                return false;

            if (file_info.type == FileType::Directory &&
                    !EnumerateFiles(filename, "*.dbsnap", -1, -1, alloc, out_filenames))
                return false;
        }
    } else {
        opt->ConsumeNonOptions(out_filenames);
    }

    return true;
}

static inline FmtArg FormatSha256(Span<const uint8_t> hash)
{
    RG_ASSERT(hash.len == 32);

    FmtArg arg = FmtSpan(hash, FmtType::BigHex, "").Pad0(-2);
    return arg;
}

static int RunRestore(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    HeapArray<const char *> src_filenames;
    const char *dest_directory = nullptr;
    bool recursive = false;
    bool force = false;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 restore [options] <snapshot...>%!0

Options:
    %!..+-O, --output_dir <dir>%!0       Restore inside this directory (instead of real path)

    %!..+-r, --recursive%!0              Collect all snapshots recursively
    %!..+-f, --force%!0                  Overwrite existing databases

As a precaution, you need to use %!..+--force%!0 if you don't use %!..+--output_dir%!0.)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-O", "--output_dir", OptionType::Value)) {
                dest_directory = opt.current_value;
            } else if (opt.Test("-r", "--recursive")) {
                recursive = true;
            } else if (opt.Test("-f", "--force")) {
                force = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        ListSnapshotFiles(&opt, recursive, &temp_alloc, &src_filenames);
    }

    if (!src_filenames.len) {
        LogError("No snapshot filename provided");
        return 1;
    }
    if (!dest_directory && !force) {
        LogError("No destination filename provided (and -f was not specified)");
        return 1;
    }

    sq_SnapshotSet snapshot_set;
    if (!sq_CollectSnapshots(src_filenames, &snapshot_set))
        return 1;

    bool complete = true;
    for (const sq_SnapshotInfo &snapshot: snapshot_set.snapshots) {
        const char *dest_filename;
        if (dest_directory) {
            HeapArray<char> buf(&temp_alloc);
            Span<const char> remain = snapshot.orig_filename;

            buf.Append(dest_directory);
            while (remain.len) {
                Span<const char> part = SplitStrAny(remain, "/\\", &remain);

                if (part == "..") {
                    Fmt(&buf, "%/__");
                } else if (part.len && part != ".") {
                    Fmt(&buf, "%/%1", part);
                }
            }
            buf.Append(0);

            dest_filename = buf.TrimAndLeak().ptr;
        } else {
            dest_filename = snapshot.orig_filename;
        }

        if (!EnsureDirectoryExists(dest_filename)) {
            complete = false;
            continue;
        }

        complete &= sq_RestoreSnapshot(snapshot, dest_filename, force);
    }

    return !complete;
}

static int RunList(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    HeapArray<const char *> src_filenames;
    int verbosity = 0;
    bool recursive = false;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 list [options] <snapshot...>%!0

Options:
    %!..+-r, --recursive%!0              Collect all snapshots recursively
    %!..+-v, --verbose%!0                List all available logs per snapshot)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-r", "--recursive")) {
                recursive = true;
            } else if (opt.Test("-v", "--verbose")) {
                verbosity++;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        ListSnapshotFiles(&opt, recursive, &temp_alloc, &src_filenames);
    }

    if (!src_filenames.len) {
        LogError("No snapshot filename provided");
        return 1;
    }

    sq_SnapshotSet snapshot_set;
    if (!sq_CollectSnapshots(src_filenames, &snapshot_set))
        return 1;

    for (Size i = 0; i < snapshot_set.snapshots.len; i++) {
        const sq_SnapshotInfo &snapshot = snapshot_set.snapshots[i];

        PrintLn("%1Database: %!..+%2%!0", verbosity && i ? "\n" : "", snapshot.orig_filename);

        if (verbosity) {
            for (const sq_SnapshotInfo::Version &version: snapshot.versions) {
                const char *basename = SplitStrReverseAny(version.base_filename, RG_PATH_SEPARATORS).ptr;

                if (verbosity >= 2) {
                    PrintLn("  - Generation %!y..'%1'%!0", basename);

                    for (Size j = 0; j < version.frames; j++) {
                        const sq_SnapshotInfo::Frame &frame = snapshot.frames[version.frame_idx + j];

                        if (verbosity >= 3) {
                            PrintLn("    %!D..+ Log:%!0 %1 (%2)", FmtTimeNice(DecomposeTime(frame.mtime)), FormatSha256(frame.sha256));
                        } else {
                            PrintLn("    %!D..+ Log:%!0 %1", FmtTimeNice(DecomposeTime(frame.mtime)));
                        }
                    }
                } else {
                    PrintLn("  - Generation %!y..'%1'%!0: %2", basename, FmtTimeNice(DecomposeTime(version.mtime)));
                }
            }
        } else {
            PrintLn("  - Time: %!y..%1%!0", FmtTimeNice(DecomposeTime(snapshot.mtime)));
        }
    }

    return 0;
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    // Options
    HeapArray<const char *> src_filenames;

    const auto print_usage = [](FILE *fp) {
        PrintLn(fp, R"(Usage: %!..+%1 <command> [args]%!0

Commands:
    %!..+restore%!0                      Restore databases from SQLite snapshots
    %!..+list%!0                         List available databases in snapshot files

    %!..+torture%!0                      Torture snapshot code (for testing)

Use %!..+%1 help <command>%!0 or %!..+%1 <command> --help%!0 for more specific help.)", FelixTarget);
    };

    if (argc < 2) {
        print_usage(stderr);
        PrintLn(stderr);
        LogError("No command provided");
        return 1;
    }

    const char *cmd = argv[1];
    Span<const char *> arguments((const char **)argv + 2, argc - 2);

    // Handle help and version arguments
    if (TestStr(cmd, "--help") || TestStr(cmd, "help")) {
        if (arguments.len && arguments[0][0] != '-') {
            cmd = arguments[0];
            arguments[0] = (cmd[0] == '-') ? cmd : "--help";
        } else {
            print_usage(stdout);
            return 0;
        }
    } else if (TestStr(cmd, "--version")) {
        PrintLn("%!R..%1%!0 %!..+%2%!0", FelixTarget, FelixVersion);
        PrintLn("Compiler: %1", FelixCompiler);
        return 0;
    }

    // Execute relevant command
    if (TestStr(cmd, "restore")) {
        return RunRestore(arguments);
    } else if (TestStr(cmd, "list")) {
        return RunList(arguments);
    } else if (TestStr(cmd, "torture")) {
        return RunTorture(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
