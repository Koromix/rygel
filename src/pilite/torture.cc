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

static inline bool InsertRandom(sq_Database *db)
{
    char buf[512];

    int i = GetRandomIntSafe(0, 65536);
    const char *str = Fmt(buf, "%1", FmtRandom(i % 64)).ptr;

    if (GetRandomIntSafe(0, 1000) < 20) {
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
    for (Size i = 0; i < GetRandomIntSafe(0, 65536); i++) {
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
            int wait = GetRandomIntSafe(200, 500);
            WaitDelay(wait);

            if (!db.Checkpoint())
                return false;
        }

        return true;
    });

    for (Size i = 0; i < 512; i++) {
        async.Run([&]() {
            while (GetMonotonicTime() - start < duration) {
                if (!InsertRandom(&db))
                    return false;
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

int RunTorture(Span<const char *> arguments)
{
    // Options
    const char *snapshot_directory = nullptr;
    int64_t duration = 60000;
    int64_t full_delay = 86400000;
    bool force = false;
    const char *database_filename = nullptr;

const auto print_usage = [=](FILE *fp) {
        PrintLn(fp,
R"(Usage: %!..+%1 torture [options] <database>%!0

Options:
    %!..+-S, --snapshot_dir <dir>%!0     Create snapshots inside this directory

    %!..+-d, --duration <sec>%!0         Set torture duration in seconds
                                         (default: %2 sec)
        %!..+--full_delay <sec>%!0       Set delay between full snapshots
                                         (default: %3 sec)

    %!..+-f, --force%!0                  Overwrite existing database file)",
                FelixTarget, duration / 1000, full_delay / 1000);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(stdout);
                return 0;
            } else if (opt.Test("-S", "--snapshot_dir", OptionType::Value)) {
                snapshot_directory = opt.current_value;
            } else if (opt.Test("-d", "--duration", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &duration))
                    return 1;
                if (duration < 0 || duration > INT64_MAX / 1000) {
                    LogError("Duration value cannot be negative or too big");
                    return 1;
                }

                duration *= 1000;
            } else if (opt.Test("--full_delay", OptionType::Value)) {
                if (!ParseInt(opt.current_value, &full_delay))
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

}
