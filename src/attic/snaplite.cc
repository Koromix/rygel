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
#include "src/core/sqlite/snapshot.hh"
#include "src/core/sqlite/sqlite.hh"

namespace RG {

static bool ListSnapshotFiles(const char *filename, BlockAllocator *alloc, HeapArray<const char *> *out_filenames)
{
    RG_ASSERT(!out_filenames->len);

    if (!filename) {
        LogError("Missing snapshot directory or filename");
        return false;
    }

    FileInfo file_info;
    if (StatFile(filename, &file_info) != StatResult::Success)
        return false;

    if (file_info.type == FileType::Directory) {
        if (!EnumerateFiles(filename, "*.dbsnap", -1, -1, alloc, out_filenames))
            return false;

        if (!out_filenames->len) {
            LogError("Could not find any snapshot file");
            return false;
        }
    } else {
        const char *copy = DuplicateString(filename, alloc).ptr;
        out_filenames->Append(copy);
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
    const char *src_filename = nullptr;
    const char *dest_directory = nullptr;
    bool dry_run = false;
    bool force = false;
    int64_t at = -1;

    const auto print_usage = [](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 restore [option...] directory%!0

Options:

    %!..+-O, --output_dir directory%!0     Restore inside this directory (instead of real path)

    %!..+-n, --dry_run%!0                  Pretend to restore without doing anything
    %!..+-f, --force%!0                    Overwrite existing databases

        %!..+--at unix_time%!0             Restore database as it was at specified time

As a precaution, you need to use %!..+--force%!0 if you don't use %!..+--output_dir%!0.)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-O", "--output_dir", OptionType::Value)) {
                dest_directory = opt.current_value;
            } else if (opt.Test("-n", "--dry_run")) {
                dry_run = true;
            } else if (opt.Test("-f", "--force")) {
                force = true;
            } else if (opt.Test("--at", OptionType::Value)) {
                if (TestStr(opt.current_value, "latest")) {
                    at = -1;
                } else if (ParseInt(opt.current_value, &at)) {
                    at = at * 1000 + 999;
                } else {
                    return 1;
                }
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        src_filename = opt.ConsumeNonOption();
        opt.LogUnusedArguments();
    }

    HeapArray<const char *> snapshot_filenames;
    if (!ListSnapshotFiles(src_filename, &temp_alloc, &snapshot_filenames))
        return 1;
    if (!snapshot_filenames.len) {
        LogError("Could not find any snapshot file");
        return 1;
    }

    if (!dest_directory && !force) {
        LogError("No destination filename provided (and -f was not specified)");
        return 1;
    }

    sq_SnapshotSet snapshot_set;
    if (!sq_CollectSnapshots(snapshot_filenames, &snapshot_set))
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

        Size frame_idx = (at >= 0) ? snapshot.FindFrame(at) : -1;
        int64_t mtime = (frame_idx >= 0) ? snapshot.frames[frame_idx].mtime : snapshot.mtime;

        TimeSpec spec = DecomposeTimeUTC(mtime);
        LogInfo("Restoring '%1' at %2%3", dest_filename, FmtTimeNice(spec), dry_run ? " [dry]" : "");

        if (!dry_run) {
            if (!EnsureDirectoryExists(dest_filename)) {
                complete = false;
                continue;
            }

            complete &= sq_RestoreSnapshot(snapshot, frame_idx, dest_filename, force);
        }
    }

    return !complete;
}

static int RunList(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *src_filename = nullptr;
    int verbosity = 0;

    const auto print_usage = [](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 list [option...] directory%!0

Options:

    %!..+-v, --verbose%!0                  List all available logs per snapshot)", FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-v", "--verbose")) {
                verbosity++;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        src_filename = opt.ConsumeNonOption();
        opt.LogUnusedArguments();
    }

    HeapArray<const char *> snapshot_filenames;
    if (!ListSnapshotFiles(src_filename, &temp_alloc, &snapshot_filenames))
        return 1;
    if (!snapshot_filenames.len) {
        LogError("Could not find any snapshot file");
        return 1;
    }

    sq_SnapshotSet snapshot_set;
    if (!sq_CollectSnapshots(snapshot_filenames, &snapshot_set))
        return 1;

    for (Size i = 0; i < snapshot_set.snapshots.len; i++) {
        const sq_SnapshotInfo &snapshot = snapshot_set.snapshots[i];

        PrintLn("%1Database: %!..+%2%!0", verbosity && i ? "\n" : "", snapshot.orig_filename);
        PrintLn("  - Creation time: %!y..%1%!0", FmtTimeNice(DecomposeTimeUTC(snapshot.ctime)));
        PrintLn("  - Last time:     %!y..%1%!0", FmtTimeNice(DecomposeTimeUTC(snapshot.mtime)));

        if (verbosity) {
            for (const sq_SnapshotGeneration &generation: snapshot.generations) {
                const char *basename = SplitStrReverseAny(generation.base_filename, RG_PATH_SEPARATORS).ptr;

                PrintLn("  - Generation '%1' (%2 %3)", basename, generation.frames, generation.frames == 1 ? "frame" : "frames");
                PrintLn("    + From:%!0 %1", FmtTimeNice(DecomposeTimeUTC(generation.ctime)));
                PrintLn("    + To: %1", FmtTimeNice(DecomposeTimeUTC(generation.mtime)));

                if (verbosity >= 2) {

                    for (Size j = 0; j < generation.frames; j++) {
                        const sq_SnapshotFrame &frame = snapshot.frames[generation.frame_idx + j];

                        if (verbosity >= 3) {
                            PrintLn("    + Frame %1: %2 %!D..(%3)%!0", j, FmtTimeNice(DecomposeTimeUTC(frame.mtime)), FormatSha256(frame.sha256));
                        } else {
                            PrintLn("    + Frame %1: %2", j, FmtTimeNice(DecomposeTimeUTC(frame.mtime)));
                        }
                    }
                }
            }
        }
    }

    return 0;
}

static inline bool InsertRandom(sq_Database *db)
{
    char buf[512];

    int i = GetRandomInt(0, 65536);
    const char *str = Fmt(buf, "%1", FmtRandom(i % 64)).ptr;

    if (GetRandomInt(0, 1000) < 20) {
        bool success = db->Transaction([&]() {
            if (!db->Run("INSERT INTO dummy VALUES (?1, ?2, 1)", i, str))
                return false;
            if (!db->Run("INSERT INTO dummy VALUES (?1, ?2, 1)", i + 1, str))
                return false;
            if (!db->Run("INSERT INTO dummy VALUES (?1, ?2, 1)", i + 2, str))
                return false;
            if (!db->Run("INSERT INTO dummy VALUES (?1, ?2, 1)", i + 3, str))
                return false;

            return true;
        });
        if (!success)
            return false;
    } else {
        if (!db->Run("INSERT INTO dummy VALUES (?1, ?2, 0)", i, str[0] ? str : nullptr))
            return false;
    }

    return true;
}

static bool TortureSnapshots(const char *database_filename, const char *snapshot_directory,
                             int64_t duration, int64_t full_delay)
{
    sq_Database db;

    if (!db.Open(database_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return false;
    if (!db.SetWAL(true))
        return false;

    // Init database
    {
        bool success = db.RunMany(R"(
            CREATE TABLE dummy (
                i INTEGER NOT NULL,
                s TEXT,
                t INTEGER CHECK(t IN (0, 1)) NOT NULL
            );

            CREATE INDEX dummy_s ON dummy (s);
        )");
        if (!success)
            return false;
    }

    // Add some random data before first snapshot
    for (Size i = 0; i < GetRandomInt(0, 65536); i++) {
        if (!InsertRandom(&db))
            return false;
    }
    if (!db.Checkpoint())
        return false;

    // Start snapshot
    if (!db.SetSnapshotDirectory(snapshot_directory, full_delay))
        return false;

    Async async;
    int64_t start = GetMonotonicTime();

    async.Run([&]() {
        while (GetMonotonicTime() - start < duration) {
            if (!db.Checkpoint())
                return false;

            int wait = GetRandomInt(500, 2000);
            WaitDelay(wait);
        }

        return true;
    });

    for (Size i = 0; i < 32; i++) {
        async.Run([&]() {
            while (GetMonotonicTime() - start < duration) {
                while (GetMonotonicTime() - start < duration) {
                    if (!InsertRandom(&db))
                        return false;
                }
            }

            return true;
        });

        async.Run([&]() {
            while (GetMonotonicTime() - start < duration) {
                sq_Statement stmt;
                if (!db.Prepare("SELECT * FROM dummy", &stmt))
                    return false;

                while (GetMonotonicTime() - start < duration) {
                    if (!stmt.Step())
                        break;
                }
            }

            return true;
        });
    }

    if (!async.Sync())
        return false;
    if (!db.Checkpoint())
        return false;

    return true;
}

static int RunTorture(Span<const char *> arguments)
{
    // Options
    const char *snapshot_directory = nullptr;
    int64_t duration = 60000;
    int64_t full_delay = 86400000;
    bool force = false;
    const char *database_filename = nullptr;

const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 torture [option...] database%!0

Options:

    %!..+-S, --snapshot_dir directory%!0   Create snapshots inside this directory

    %!..+-d, --duration sec%!0             Set torture duration in seconds
                                   %!D..(default: %2 sec)%!0
        %!..+--full_delay sec%!0           Set delay between full snapshots
                                   %!D..(default: %3 sec)%!0

    %!..+-f, --force%!0                    Overwrite existing database file)",
                FelixTarget, duration / 1000, full_delay / 1000);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-S", "--snapshot_dir", OptionType::Value)) {
                snapshot_directory = opt.current_value;
            } else if (opt.Test("-d", "--duration", OptionType::Value)) {
                if (!ParseDuration(opt.current_value, &duration))
                    return 1;
                if (duration < 0 || duration > INT64_MAX / 1000) {
                    LogError("Duration value cannot be negative or too big");
                    return 1;
                }

                duration *= 1000;
            } else if (opt.Test("--full_delay", OptionType::Value)) {
                if (!ParseDuration(opt.current_value, &full_delay))
                    return 1;
                if (full_delay < 0 || full_delay > INT64_MAX / 1000) {
                    LogError("Full snapshot delay cannot be negative or too big");
                    return 1;
                }

                full_delay *= 1000;
            } else if (opt.Test("-f", "--force")) {
                force = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        database_filename = opt.ConsumeNonOption();
        opt.LogUnusedArguments();
    }

    if (!database_filename) {
        LogError("Missing database filename");
        return 1;
    }
    if (!snapshot_directory) {
        LogError("Missing snapshot directory");
        return 1;
    }

    if (TestFile(database_filename) && !force) {
        LogError("File '%1' already exists", database_filename);
        return 1;
    }
    if (!UnlinkFile(database_filename))
        return 1;

    LogInfo("Running torture for %1 seconds...", duration / 1000);
    return !TortureSnapshots(database_filename, snapshot_directory, duration, full_delay);
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    const auto print_usage = [](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 command [arg...]%!0

Commands:

    %!..+restore%!0                        Restore databases from SQLite snapshots
    %!..+list%!0                           List available databases in snapshot files

    %!..+torture%!0                        Torture snapshot code (for testing)

Use %!..+%1 help command%!0 or %!..+%1 command --help%!0 for more specific help.)", FelixTarget);
    };

    if (argc < 2) {
        print_usage(StdErr);
        PrintLn(StdErr);
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
            print_usage(StdOut);
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
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
