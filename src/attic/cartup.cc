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

#include "src/core/base/base.hh"
#include "src/core/sqlite/sqlite.hh"
#include "vendor/blake3/c/blake3.h"

namespace RG {

static const int SchemaVersion = 2;

struct DiskData {
    int64_t id;
    char uuid[37];

    int64_t total;
    int64_t used;
    int64_t files;
};

struct DiskSet {
    HeapArray<DiskData> disks;
};

static const char *database_filename = nullptr;
static sq_Database db;

static bool OpenDatabase()
{
    RG_ASSERT(!db.IsValid());

    if (!database_filename) {
        LogError("Missing database filename");
        return false;
    }

    if (!db.Open(database_filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return false;
    if (!db.SetWAL(true))
        return false;

    int version;
    if (!db.GetUserVersion(&version))
        return false;

    if (version > SchemaVersion) {
        LogError("Database schema is too recent (%1, expected %2)", version, SchemaVersion);
        return false;
    } else if (version < SchemaVersion) {
        bool success = db.Transaction([&]() {
            switch (version) {
                case 0: {
                    bool success = db.RunMany(R"(
                        CREATE TABLE disks (
                            id INTEGER PRIMARY KEY,
                            uuid TEXT NOT NULL,
                            root TEXT NOT NULL,
                            size INTEGER NOT NULL
                        );
                        CREATE UNIQUE INDEX disks_u ON disks (uuid);

                        CREATE TABLE files (
                            id INTEGER PRIMARY KEY,
                            path TEXT NOT NULL,
                            origin TEXT,
                            mtime INTEGER NOT NULL,
                            size INTEGER NOT NULL,
                            disk_id INTEGER REFERENCES disks (id)
                        );
                        CREATE UNIQUE INDEX files_p ON files (path);
                    )");
                    if (!success)
                        return false;
                } [[fallthrough]];

                case 1: {
                    bool success = db.RunMany(R"(
                        ALTER TABLE files ADD COLUMN changeset INTEGER;
                    )");
                    if (!success)
                        return false;
                } // [[fallthrough]];

                static_assert(SchemaVersion == 2);
            }

            if (!db.SetUserVersion(SchemaVersion))
                return false;

            return true;
        });

        if (!success) {
            db.Close();
            return false;
        }
    }

    return true;
}

static const char *GenerateUUIDv4(Allocator *alloc)
{
    uint8_t bytes[16];
    FillRandomSafe(bytes);

    bytes[6] = (4 << 4) | (bytes[6] & 0x0F);
    bytes[8] = (2 << 6) | (bytes[8] & 0x3F);

    const char *uuid = Fmt(alloc, "%1-%2-%3-%4-%5",
                                  FmtSpan(MakeSpan(bytes + 0, 4), FmtType::BigHex, "").Pad0(-2),
                                  FmtSpan(MakeSpan(bytes + 4, 2), FmtType::BigHex, "").Pad0(-2),
                                  FmtSpan(MakeSpan(bytes + 6, 2), FmtType::BigHex, "").Pad0(-2),
                                  FmtSpan(MakeSpan(bytes + 8, 2), FmtType::BigHex, "").Pad0(-2),
                                  FmtSpan(MakeSpan(bytes + 10, 6), FmtType::BigHex, "").Pad0(-2)).ptr;
    return uuid;
}

static const char *ReadUUID(const char *filename, Allocator *alloc)
{
    LocalArray<char, 64> buf;
    buf.len = ReadFile(filename, buf.data);

    if (buf.len < 0)
        return nullptr;
    buf.len = TrimStrRight(buf.As<char>()).len;

    if (buf.len < 36) {
        LogError("Truncated disk UUID");
        return nullptr;
    } else if (buf.len > 36) {
        LogError("Excessive UUID size");
        return nullptr;
    }

    const char *uuid = DuplicateString(buf, alloc).ptr;
    return uuid;
}

static bool LoadDiskStats(DiskSet *out_set)
{
    RG_DEFER_NC(err_guard, len = out_set->disks.len) { out_set->disks.RemoveFrom(len); };

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT d.id, d.uuid, d.size, SUM(f.size), COUNT(f.id)
                       FROM disks d
                       LEFT JOIN files f ON (f.disk_id = d.id)
                       GROUP BY d.id)", &stmt))
        return false;

    while (stmt.Step()) {
        DiskData disk = {};

        disk.id = sqlite3_column_int64(stmt, 0);
        CopyString((const char *)sqlite3_column_text(stmt, 1), disk.uuid);
        disk.total = sqlite3_column_int64(stmt, 2);
        disk.used = sqlite3_column_int64(stmt, 3);
        disk.files = sqlite3_column_int64(stmt, 4);

        out_set->disks.Append(disk);
    }
    if (!stmt.IsValid())
        return false;

    err_guard.Disable();
    return true;
}

static DiskData *FindDisk(DiskSet *set, int64_t id)
{
    for (DiskData &disk: set->disks) {
        if (disk.id == id)
            return &disk;
    }
    return nullptr;
}

static DiskData *FindSpace(DiskSet *set, int64_t size)
{
    DiskData *target = nullptr;
    double min_ratio = INFINITY;

    for (DiskData &disk: set->disks) {
        int64_t available = disk.total - disk.used;

        if (size <= available) {
            double ratio = (double)(available + size) / disk.total;

            if (ratio < min_ratio) {
                target = &disk;
                min_ratio = ratio;
            }
        }
    }

    return target;
}

class IntegrateContext {
    int64_t disk_id;
    const char *disk_dir;
    int64_t changeset;

    BlockAllocator temp_alloc;

public:
    IntegrateContext(int64_t disk_id, const char *disk_dir)
        : disk_id(disk_id), disk_dir(disk_dir), changeset(GetRandomInt64(0, INT64_MAX)) {}

    bool AddNew() { return AddNew(""); }
    bool DeleteOld();

private:
    bool AddNew(const char *dirname);
};

bool IntegrateContext::AddNew(const char *dirname)
{
    const char *enum_dir = Fmt(&temp_alloc, "%1%2", disk_dir, dirname).ptr;

    EnumResult ret = EnumerateDirectory(enum_dir, nullptr, -1, [&](const char *basename, const FileInfo &file_info) {
        switch (file_info.type) {
            case FileType::Directory: {
                const char *path = Fmt(&temp_alloc, "%1%2%/", dirname, basename).ptr;

                if (!AddNew(path))
                    return false;
            } break;

            case FileType::File: {
                const char *path = Fmt(&temp_alloc, "%1%2", dirname, basename).ptr;

                if (TestStr(path, ".cartup"))
                    break;

                if (!db.Run(R"(INSERT INTO files (path, mtime, size, disk_id, changeset)
                               VALUES (?1, ?2, ?3, ?4, ?5)
                               ON CONFLICT DO UPDATE SET mtime = excluded.mtime,
                                                         size = excluded.size,
                                                         disk_id = excluded.disk_id,
                                                         changeset = excluded.changeset)",
                            path, file_info.mtime, file_info.size, disk_id, changeset))
                    return false;
            } break;

            case FileType::Link:
            case FileType::Device:
            case FileType::Pipe:
            case FileType::Socket: {
                const char *path = Fmt(&temp_alloc, "%1%2", dirname, basename).ptr;
                LogWarning("Ignoring special file '%1' (%2)", path, FileTypeNames[(int)file_info.type]);
            } break;
        }

        return true;
    });
    if (ret != EnumResult::Success)
        return false;

    return true;
}

bool IntegrateContext::DeleteOld()
{
    bool success = db.Run("DELETE FROM files WHERE disk_id = ?1 AND changeset IS NOT ?2", disk_id, changeset);
    return success;
}

static int RunIntegrate(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *disk_dir = nullptr;
    int64_t size = -1;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 integrate <disk> [...]

Options:
    %!..+-D, --database_file <file>%!0   Set database file%!0

    %!..+-s, --size <size>%!0            Set explicit disk size
                                 %!D..(default: auto-detect)%!0)",
                FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-D", "--database_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-s", "--size", OptionType::Value)) {
                if (!ParseSize(opt.current_value, &size))
                    return 1;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        disk_dir = opt.ConsumeNonOption();
        opt.LogUnusedArguments();
    }

    if (!disk_dir) {
        LogInfo("Missing disk path argument");
        return 1;
    }
    if (!PathIsAbsolute(disk_dir)) {
        LogInfo("Disk path must be absolute");
        return 1;
    }
    disk_dir = NormalizePath(disk_dir, (int)NormalizeFlag::EndWithSeparator, &temp_alloc).ptr;

    if (!OpenDatabase())
        return 1;

    const char *uuid = nullptr;
    {
        const char *filename = Fmt(&temp_alloc, "%1%/.cartup", disk_dir).ptr;

        if (TestFile(filename, FileType::File)) {
            uuid = ReadUUID(filename, &temp_alloc);

            if (!uuid)
                return false;
        } else {
            uuid = GenerateUUIDv4(&temp_alloc);

            if (!WriteFile(uuid, filename))
                return false;
        }
    }

    bool success = db.Transaction([&]() {
        int64_t disk_id;
        {
            sq_Statement stmt;
            if (!db.Prepare(R"(INSERT INTO disks (uuid, root, size) VALUES (?1, ?2, ?3)
                               ON CONFLICT DO UPDATE SET size = IIF(excluded.size > 0, excluded.size, size)
                               RETURNING id)",
                            &stmt, uuid, disk_dir, size))
                return false;
            if (!stmt.GetSingleValue(&disk_id))
                return false;
        }

        // Run integration
        {
            IntegrateContext ctx(disk_id, disk_dir);

            if (!ctx.AddNew())
                return false;
            ctx.DeleteOld();
        }

        if (size < 0) {
            VolumeInfo volume;
            if (!GetVolumeInfo(disk_dir, &volume))
                return false;

            sq_Statement stmt;
            if (!db.Prepare("SELECT SUM(size) * 1.02 FROM files WHERE disk_id = ?1 GROUP BY disk_id",
                            &stmt, disk_id))
                return false;

            if (stmt.Step()) {
                volume.available += sqlite3_column_int64(stmt, 0);
            } else if (!stmt.IsValid()) {
                return false;
            }

            // Max out at 98% of the total size to account for metadata (or at least, try to)
            volume.total -= volume.total / 50;
            volume.available = std::min(volume.total, volume.available);

            if (!db.Run("UPDATE disks SET size = ?2 WHERE id = ?1", disk_id, volume.available))
                return false;
        }

        return true;
    });
    if (!success)
        return 1;

    if (!db.Close())
        return 1;

    return 0;
}

enum class DistributeResult {
    Complete,
    Partial,
    Error
};

class DistributeContext {
    DiskSet *set;
    int64_t changeset;

    BlockAllocator temp_alloc;

public:
    DistributeContext(DiskSet *set)
        : set(set), changeset(GetRandomInt64(0, INT64_MAX)) {}

    DistributeResult DistributeNew(const char *src_dir) { return DistributeNew(src_dir, ""); }
    bool DeleteOld();

private:
    DistributeResult DistributeNew(const char *src_dir, const char *dirname);
};

DistributeResult DistributeContext::DistributeNew(const char *src_dir, const char *dirname)
{
    const char *enum_dir = Fmt(&temp_alloc, "%1%2", src_dir, dirname).ptr;
    bool complete = true;

    EnumResult ret = EnumerateDirectory(enum_dir, nullptr, -1, [&](const char *basename, const FileInfo &file_info) {
        switch (file_info.type) {
            case FileType::Directory: {
                const char *path = Fmt(&temp_alloc, "%1%2%/", dirname, basename).ptr;

                switch (DistributeNew(src_dir, path)) {
                    case DistributeResult::Complete: {} break;
                    case DistributeResult::Partial: { complete = false; } break;
                    case DistributeResult::Error: return false;
                }
            } break;

            case FileType::File: {
                const char *filename = Fmt(&temp_alloc, "%1%2", enum_dir, basename).ptr;
                const char *path = Fmt(&temp_alloc, "%1%2", dirname, basename).ptr;

                sq_Statement stmt;
                if (!db.Prepare("SELECT disk_id, size FROM files WHERE path = ?1", &stmt, path))
                    return false;

                DiskData *disk = nullptr;

                if (stmt.Step()) {
                    int64_t disk_id = sqlite3_column_int64(stmt, 0);
                    int64_t size = sqlite3_column_int64(stmt, 1);

                    disk = FindDisk(set, disk_id);

                    if (!disk) {
                        LogError("Unexplained disk info mismatch");
                        return false;
                    }
                    disk->used -= size;

                    if (file_info.size > disk->total - disk->used) {
                        disk = nullptr;
                    }
                } else if (!stmt.IsValid()) {
                    return false;
                }

                if (!disk) {
                    disk = FindSpace(set, file_info.size);

                    if (!disk) {
                        LogError("Not enough space for '%1'", path);

                        complete = false;
                        return true;
                    }
                }

                disk->used += file_info.size;

                if (!db.Run(R"(INSERT INTO files (disk_id, path, origin, mtime, size, changeset)
                               VALUES (?1, ?2, ?3, ?4, ?5, ?6)
                               ON CONFLICT (path) DO UPDATE SET disk_id = excluded.disk_id,
                                                                origin = excluded.origin,
                                                                mtime = excluded.mtime,
                                                                size = excluded.size,
                                                                changeset = excluded.changeset)",
                            disk->id, path, filename, file_info.mtime, file_info.size, changeset))
                    return false;

                LogInfo("Attribute '%1' to disk '%2'", filename, disk->uuid);
            } break;

            case FileType::Link:
            case FileType::Device:
            case FileType::Pipe:
            case FileType::Socket: {
                const char *path = Fmt(&temp_alloc, "%1%2", dirname, basename).ptr;
                LogWarning("Ignoring special file '%1' (%2)", path, FileTypeNames[(int)file_info.type]);
            } break;
        }

        return true;
    });
    if (ret != EnumResult::Success)
        return DistributeResult::Error;

    return complete ? DistributeResult::Complete : DistributeResult::Partial;
}

bool DistributeContext::DeleteOld()
{
    bool success = db.Run("UPDATE files SET origin = NULL WHERE changeset IS NOT ?1", changeset);
    return success;
}

static int RunDistribute(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    HeapArray<const char *> sources;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 distribute [options] <source> [...]

Options:
    %!..+-D, --database_file <file>%!0   Set database file%!0)",
                FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-D", "--database_file", OptionType::Value)) {
                // Already handled
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        const char *src_dir = opt.ConsumeNonOption();

        if (!src_dir) {
            LogError("Missing source directories");
            return 1;
        }

        do {
            src_dir = NormalizePath(src_dir, GetWorkingDirectory(), (int)NormalizeFlag::EndWithSeparator, &temp_alloc).ptr;

            if (!TestFile(src_dir, FileType::Directory)) {
                LogInfo("Directory '%1' does not exist", src_dir);
                return 1;
            }

            sources.Append(src_dir);
        } while ((src_dir = opt.ConsumeNonOption()));
    }

    DiskSet set;

    if (!OpenDatabase())
        return 1;
    if (!LoadDiskStats(&set))
        return 1;

    bool complete = true;
    bool success = db.Transaction([&]() {
        DistributeContext ctx(&set);

        for (const char *src_dir: sources) {
            switch (ctx.DistributeNew(src_dir)) {
                case DistributeResult::Complete: {} break;
                case DistributeResult::Partial: { complete = false; } break;
                case DistributeResult::Error: return false;
            }
        }

        ctx.DeleteOld();

        return true;
    });
    if (!success || !complete)
        return 1;

    return 0;
}

class BackupContext {
    const char *uuid;
    const char *disk_dir;

    bool checksum;
    bool fake;

    BlockAllocator temp_alloc;

public:
    BackupContext(const char *uuid, const char *disk_dir, bool checksum, bool fake);

    bool BackupNew();
    bool DeleteOld() { return DeleteOld(""); }

private:
    bool DeleteOld(const char *dirname);

    bool HashFile(int fd, const char *filename, Span<uint8_t> buf, uint8_t out_hash[32]);
    bool CopyFile(int src_fd, const char *src_filename,int dest_fd, const char *dest_filename, int64_t size, int64_t mtime);
};

BackupContext::BackupContext(const char *uuid, const char *disk_dir, bool checksum, bool fake)
    : uuid(uuid), disk_dir(disk_dir), checksum(checksum), fake(fake) {}

static bool IsTimeEquivalent(int64_t time1, int64_t time2)
{
    bool close = (time1 / 10) == (time2 / 10);
    return close;
}

bool BackupContext::BackupNew()
{
    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT f.origin, f.path, f.mtime, f.size
                       FROM disks d
                       INNER JOIN files f ON (f.disk_id = d.id)
                       WHERE d.uuid = ?1 AND f.origin IS NOT NULL)", &stmt, uuid))
        return false;

    bool valid = true;

    Span<uint8_t> buf1 = AllocateSpan<uint8_t>(&temp_alloc, Mebibytes(4));
    Span<uint8_t> buf2 = AllocateSpan<uint8_t>(&temp_alloc, Mebibytes(4));
    RG_DEFER {
        ReleaseSpan(&temp_alloc, buf1);
        ReleaseSpan(&temp_alloc, buf2);
    };

    while (stmt.Step()) {
        const char *src_filename = (const char *)sqlite3_column_text(stmt, 0);
        const char *path = (const char *)sqlite3_column_text(stmt, 1);
        int64_t mtime = sqlite3_column_int64(stmt, 2);
        int64_t size = sqlite3_column_int64(stmt, 3);

        const char *dest_filename = Fmt(&temp_alloc, "%1%2", disk_dir, path).ptr;

        int src_fd = OpenFile(src_filename, (int)OpenFlag::Read);
        if (src_fd < 0) {
            valid = false;
            continue;
        }
        RG_DEFER { close(src_fd); };

        // Check file information consistency
        {
            FileInfo src_info;
            StatResult stat = StatFile(src_fd, src_filename, 0, &src_info);

            if (stat != StatResult::Success) {
                valid = false;
                continue;
            }

            if (src_info.size != size || src_info.mtime != mtime) {
                LogError("Mismatched size or mtime for '%1' (skipping)", src_filename);

                valid = false;
                continue;
            }
        }

        if (!EnsureDirectoryExists(dest_filename)) {
            valid = false;
            continue;
        }

        int dest_fd = OpenFile(dest_filename, (int)OpenFlag::Read | (int)OpenFlag::Write | (int)OpenFlag::Keep);
        if (dest_fd < 0) {
            valid = false;
            continue;
        }
        RG_DEFER { close(dest_fd); };

        FileInfo dest_info;
        StatResult stat = StatFile(dest_fd, dest_filename, (int)StatFlag::SilentMissing, &dest_info);

        switch (stat) {
            case StatResult::Success: {
                if (dest_info.size == size) {
                    if (checksum) {
                        uint8_t src_hash[32];
                        uint8_t dest_hash[32];

                        Async async;

                        async.Run([&]() { return HashFile(src_fd, src_filename, buf1, src_hash); });
                        async.Run([&]() { return HashFile(dest_fd, dest_filename, buf2, dest_hash); });

                        if (!async.Sync()) {
                            valid = false;
                            continue;
                        }

                        if (!memcmp(src_hash, dest_hash, 32)) {
                            LogInfo("Skip '%1' (checksum match)", path);
                            if (!fake) {
                                SetFileMetaData(dest_fd, dest_filename, mtime, mtime, 0644);
                            }
                            continue;
                        }
                    } else {
                        if (IsTimeEquivalent(dest_info.mtime, mtime)) {
                            LogInfo("Skip '%1' (metadata match)", path);
                            continue;
                        }
                    }
                }

                // Go on!
            } break;

            case StatResult::MissingPath: { /* Go on */ } break;

            case StatResult::AccessDenied:
            case StatResult::OtherError: {
                LogError("Failed to stat '%1': %1", strerror(errno));

                valid = false;
                continue;
            } break;
        }

        LogInfo("Copy '%1' to disk '%2'", path, uuid);
        valid &= fake || CopyFile(src_fd, src_filename, dest_fd, dest_filename, size, mtime);
    }
    valid &= stmt.IsValid();

    return valid;
}

// Return true if all children are deleted (directory is not empty)
bool BackupContext::DeleteOld(const char *dirname)
{
    BlockAllocator temp_alloc;

    const char *enum_dir = Fmt(&temp_alloc, "%1%2", disk_dir, dirname).ptr;
    bool complete = true;

    EnumerateDirectory(enum_dir, nullptr, -1, [&](const char *basename, const FileInfo &file_info) {
        switch (file_info.type) {
            case FileType::Directory: {
                const char *path = Fmt(&temp_alloc, "%1%2%/", dirname, basename).ptr;
                bool empty = DeleteOld(path);

                if (empty && !fake) {
                    const char *filename = Fmt(&temp_alloc, "%1%2", enum_dir, basename).ptr;
                    complete &= UnlinkDirectory(filename);
                }

                complete &= empty;
            } break;

            case FileType::File: {
                const char *path = Fmt(&temp_alloc, "%1%2", dirname, basename).ptr;

                if (TestStr(path, ".cartup"))
                    break;

                sq_Statement stmt;
                if (!db.Prepare(R"(SELECT f.id
                                   FROM files f
                                   INNER JOIN disks d ON (d.id = f.disk_id)
                                   WHERE d.uuid = ?1 AND path = ?2 AND origin IS NOT NULL)", &stmt, uuid, path))
                    return false;

                bool exists = stmt.Step();

                if (!stmt.IsValid())
                    return false;

                if (!exists) {
                    LogInfo("Delete '%1'", path);

                    if (!fake) {
                        const char *filename = Fmt(&temp_alloc, "%1%2", enum_dir, basename).ptr;
                        complete &= UnlinkFile(filename);
                    }
                }

                complete &= !exists;
            } break;

            case FileType::Link:
            case FileType::Device:
            case FileType::Pipe:
            case FileType::Socket: {
                const char *path = Fmt(&temp_alloc, "%1%2", dirname, basename).ptr;
                LogWarning("Ignoring special file '%1' (%2)", path, FileTypeNames[(int)file_info.type]);

                complete = false;
            } break;
        }

        return true;
    });

    return complete;
}

bool BackupContext::HashFile(int fd, const char *filename, Span<uint8_t> buf, uint8_t out_hash[32])
{
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);

    for (;;) {
        ssize_t bytes = read(fd, buf.ptr, buf.len);

        if (bytes < 0) {
            if (errno == EINTR)
                continue;

            LogError("Failed to read '%1'", filename);
            return false;
        }
        if (!bytes)
            break;

        blake3_hasher_update(&hasher, buf.ptr, (size_t)bytes);
    }

    blake3_hasher_finalize(&hasher, out_hash, 32);

    return true;
}

bool BackupContext::CopyFile(int src_fd, const char *src_filename, int dest_fd, const char *dest_filename, int64_t size, int64_t mtime)
{
    if (!SpliceFile(src_fd, src_filename, dest_fd, dest_filename, size))
        return false;
    if (!FlushFile(dest_fd, dest_filename))
        return false;

    SetFileMetaData(dest_fd, dest_filename, mtime, 0, 0644);

    return true;
}

static int RunBackup(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    HeapArray<const char *> disks;
    bool checksum = false;
    bool fake = false;
    bool cleanup = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 backup [options] <disk> [...]

Options:
    %!..+-D, --database_file <file>%!0   Set database file%!0

    %!..+-c, --checksum%!0               Use checksum (BLAKE3) to compare files
        %!..+--delete%!0                 Delete unused files

    %!..+-n, --dry_run%!0                Fake backup actions)",
                FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-D", "--database_file", OptionType::Value)) {
                // Already handled
            } else if (opt.Test("-c", "--checksum")) {
                checksum = true;
            } else if (opt.Test("--delete")) {
                cleanup = true;
            } else if (opt.Test("-n", "--dry_run")) {
                fake = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        const char *disk_dir = opt.ConsumeNonOption();

        if (!disk_dir) {
            LogError("Missing disk directories");
            return 1;
        }

        do {
            disk_dir = NormalizePath(disk_dir, GetWorkingDirectory(), (int)NormalizeFlag::EndWithSeparator, &temp_alloc).ptr;

            if (!TestFile(disk_dir, FileType::Directory)) {
                LogInfo("Directory '%1' does not exist", disk_dir);
                return 1;
            }

            disks.Append(disk_dir);
        } while ((disk_dir = opt.ConsumeNonOption()));
    }

    if (!OpenDatabase())
        return 1;

    Async async;

    for (const char *disk_dir: disks) {
        const char *uuid_filename = Fmt(&temp_alloc, "%1.cartup", disk_dir).ptr;
        const char *uuid = ReadUUID(uuid_filename, &temp_alloc);

        if (!uuid) {
            LogError("Cannot find disk UUID from '%1", disk_dir);
            return 1;
        }

        // Check disk exists!
        {
            sq_Statement stmt;
            if (!db.Prepare("SELECT id FROM disks WHERE uuid = ?1", &stmt, uuid))
                return 1;

            if (!stmt.Step()) {
                if (stmt.IsValid()) {
                    LogError("Disk '%1' is not in database", uuid);
                }
                return 1;
            }
        }

        async.Run([&]() {
            BackupContext ctx(uuid, disk_dir, checksum, fake);

            if (!ctx.BackupNew())
                return false;
            if (cleanup) {
                ctx.DeleteOld();
            }

            return true;
        });
    }

    if (!async.Sync())
        return 1;

    return 0;
}

static int RunStats(Span<const char *> arguments)
{
    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 stats [options] <source> [...]

Options:
    %!..+-D, --database_file <file>%!0   Set database file%!0)",
                FelixTarget);
    };

    // Parse arguments
    {
        OptionParser opt(arguments);

        while (opt.Next()) {
            if (opt.Test("--help")) {
                print_usage(StdOut);
                return 0;
            } else if (opt.Test("-D", "--database_file", OptionType::Value)) {
                // Already handled
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }
    }

    DiskSet set;

    if (!OpenDatabase())
        return 1;
    if (!LoadDiskStats(&set))
        return 1;

    for (Size i = 0; i < set.disks.len; i++) {
        const DiskData &disk = set.disks[i];

        PrintLn("%1%!..+Disk %2%!0 [%3]:", i ? "\n" : "", i + 1, disk.uuid);
        PrintLn("  Total: %!..+%1%!0", FmtDiskSize(disk.total));
        PrintLn("  Used: %!..+%1 (%2%%)%!0", FmtDiskSize(disk.used), FmtDouble((double)disk.used * 100.0 / (double)disk.total, 1));
        PrintLn("  Files: %!..+%1%!0", disk.files);
    }

    return 0;
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 <command> [args]%!0

Backup commands:
    %!..+distribute%!0                   Distribute files to known disks
    %!..+backup%!0                       Backup files to plugged disks

Disk commands:
    %!..+integrate%!0                    Add disk for future backups
    %!..+stats%!0                        Get disk usage statistics

Options:
    %!..+-D, --database_file <file>%!0   Set database file%!0)",
                FelixTarget);
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

    // Find database filename
    {
        OptionParser opt(arguments, OptionMode::Skip);

        while (opt.Next()) {
            if (opt.Test("-D", "--database_file", OptionType::Value)) {
                database_filename = opt.current_value;
            } else if (opt.TestHasFailed()) {
                return 1;
            }
        }
    }

    if (TestStr(cmd, "integrate")) {
        return RunIntegrate(arguments);
    } else if (TestStr(cmd, "distribute")) {
        return RunDistribute(arguments);
    } else if (TestStr(cmd, "backup")) {
        return RunBackup(arguments);
    } else if (TestStr(cmd, "stats")) {
        return RunStats(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
