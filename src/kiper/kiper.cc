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
#include "src/core/sqlite/sqlite.hh"
#include "vendor/blake3/c/blake3.h"

#if defined(_WIN32)
    #include <io.h>
#endif

namespace RG {

static const int SchemaVersion = 8;

struct DiskData {
    int64_t id;

    char uuid[37];
    const char *name;
    const char *root;

    int64_t total;
    int64_t used;
    int64_t files;

    int64_t added;
    int64_t changed;
    int64_t removed;
};

struct SourceInfo {
    int64_t id;
    const char *root;
};

struct BackupSet {
    sq_Database db;

    HeapArray<DiskData> disks;
    HeapArray<SourceInfo> sources;

    BlockAllocator str_alloc;

    bool Open(const char *db_filename, bool create = false);
    bool Close();

    bool Refresh();

    DiskData *FindDisk(int64_t idx);
    DiskData *FindDisk(const char *selector);

    SourceInfo *FindSource(int64_t idx);
    SourceInfo *FindSource(const char *selector);
};

static const char *GetDefaultDatabasePath()
{
    const char *filename = GetEnv("KIPER_DATABASE");

    if (!filename || !filename[0]) {
        filename = "kiper.db";
    }

    return filename;
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

bool BackupSet::Open(const char *db_filename, bool create)
{
    RG_ASSERT(!db.IsValid());

    RG_DEFER_N(err_guard) { Close(); };

    int flags = SQLITE_OPEN_READWRITE | (create ? SQLITE_OPEN_CREATE : 0);
    int version;

    if (!db.Open(db_filename, flags))
        return false;
    if (!db.SetWAL(true))
        return false;
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
                } [[fallthrough]];

                case 2: {
                    bool success = db.RunMany(R"(
                        DROP INDEX disks_u;
                        DROP INDEX files_p;

                        ALTER TABLE disks RENAME TO disks_BAK;
                        ALTER TABLE files RENAME TO files_BAK;

                        CREATE TABLE sources (
                            id INTEGER PRIMARY KEY,
                            root TEXT NOT NULL
                        );

                        CREATE TABLE disks (
                            id INTEGER PRIMARY KEY,
                            uuid TEXT NOT NULL,
                            name TEXT NOT NULL,
                            root TEXT NOT NULL,
                            size INTEGER NOT NULL
                        );
                        CREATE UNIQUE INDEX disks_u ON disks (uuid);
                        CREATE UNIQUE INDEX disks_n ON disks (name);

                        CREATE TABLE files (
                            id INTEGER PRIMARY KEY,
                            path TEXT NOT NULL,
                            origin TEXT,
                            mtime INTEGER NOT NULL,
                            size INTEGER NOT NULL,
                            disk_id INTEGER REFERENCES disks (id),
                            changeset INTEGER
                        );
                        CREATE UNIQUE INDEX files_p ON files (path);

                        INSERT INTO disks (id, uuid, name, root, size)
                            SELECT id, uuid, 'Disk ' || id, root, size FROM disks_BAK;
                        INSERT INTO files (id, path, origin, mtime, size, disk_id)
                            SELECT id, path, origin, mtime, size, disk_id FROM files_BAK;
                    )");
                    if (!success)
                        return false;
                } [[fallthrough]];

                case 3: {
                    bool success = db.RunMany(R"(
                        DROP TABLE IF EXISTS files_BAK;
                        DROP TABLE IF EXISTS disks_BAK;

                        DROP INDEX files_p;

                        ALTER TABLE files RENAME TO files_BAK;

                        CREATE TABLE files (
                            id INTEGER PRIMARY KEY,
                            path TEXT NOT NULL,
                            origin TEXT,
                            mtime INTEGER NOT NULL,
                            size INTEGER NOT NULL,
                            disk_id INTEGER REFERENCES disks (id),
                            outdated INTEGER CHECK(outdated IN (0, 1)) NOT NULL,
                            changeset INTEGER
                        );
                        CREATE UNIQUE INDEX files_p ON files (path);

                        INSERT INTO files (id, path, origin, mtime, size, disk_id, outdated)
                            SELECT id, path, origin, mtime, size, disk_id, 0 FROM files_BAK;

                        DROP TABLE files_BAK;
                    )");
                    if (!success)
                        return false;
                } [[fallthrough]];

                case 4: {
                    bool success = db.RunMany(R"(
                        DROP INDEX files_p;

                        ALTER TABLE files RENAME TO files_BAK;

                        CREATE TABLE files (
                            id INTEGER PRIMARY KEY,
                            path TEXT NOT NULL,
                            mtime INTEGER NOT NULL,
                            size INTEGER NOT NULL,
                            disk_id INTEGER REFERENCES disks (id),
                            status TEXT CHECK(status IN ('ok', 'added', 'changed', 'removed')) NOT NULL,
                            changeset INTEGER
                        );
                        CREATE UNIQUE INDEX files_p ON files (path);

                        INSERT INTO files (id, path, mtime, size, disk_id, status)
                            SELECT id, origin, mtime, size, disk_id, IIF(outdated = 0, 'ok', 'changed')
                            FROM files_BAK
                            WHERE origin IS NOT NULL;

                        DROP TABLE files_BAK;
                    )");
                    if (!success)
                        return false;
                } [[fallthrough]];

                case 5: {
                    bool success = db.RunMany(R"(
                        UPDATE files SET path = replace(path, '\\', '/');
                    )");
                    if (!success)
                        return false;
                } [[fallthrough]];

                case 6: {
                    bool success = db.RunMany(R"(
                        DROP INDEX files_p;

                        ALTER TABLE files RENAME TO files_BAK;

                        CREATE TABLE files (
                            id INTEGER PRIMARY KEY,
                            path TEXT NOT NULL,
                            mtime INTEGER NOT NULL,
                            size INTEGER NOT NULL,
                            disk_id INTEGER REFERENCES disks (id) ON DELETE CASCADE,
                            status TEXT CHECK(status IN ('ok', 'added', 'changed', 'removed')) NOT NULL,
                            changeset INTEGER
                        );
                        CREATE UNIQUE INDEX files_p ON files (path);

                        INSERT INTO files (id, path, mtime, size, disk_id, status)
                            SELECT id, path, mtime, size, disk_id, status FROM files_BAK;

                        DROP TABLE files_BAK;
                    )");
                    if (!success)
                        return false;
                } [[fallthrough]];

                case 7: {
                    bool success = db.RunMany(R"(
                        CREATE UNIQUE INDEX sources_r ON sources (root);
                    )");
                    if (!success)
                        return false;
                } // [[fallthrough]];

                static_assert(SchemaVersion == 8);
            }

            if (!db.SetUserVersion(SchemaVersion))
                return false;

            return true;
        });

        if (!success)
            return false;
    }

    // Load sources
    {
        sq_Statement stmt;
        if (!db.Prepare("SELECT id, root FROM sources", &stmt))
            return 1;

        while (stmt.Step()) {
            SourceInfo src = {};

            const char *src_dir = (const char *)sqlite3_column_text(stmt, 1);

            if (!PathIsAbsolute(src_dir)) {
                LogError("Cannot backup from non-absolute source '%1'", src_dir);
                return 1;
            }

            src.id = sqlite3_column_int64(stmt, 0);
            src.root = NormalizePath(src_dir, (int)NormalizeFlag::EndWithSeparator | (int)NormalizeFlag::ForceSlash, &str_alloc).ptr;

            sources.Append(src);
        }
    }

    // Load disk information
    if (!Refresh())
        return false;

    err_guard.Disable();
    return true;
}

bool BackupSet::Close()
{
    bool success = db.Close();

    disks.Clear();
    sources.Clear();
    str_alloc.ReleaseAll();

    return success;
}

bool BackupSet::Refresh()
{
    HeapArray<DiskData> disks;

    sq_Statement stmt;
    if (!db.Prepare(R"(SELECT d.id, d.uuid, d.name, d.root, d.size, SUM(f.size), COUNT(f.id),
                              SUM(IIF(f.status = 'added', 1, 0)) AS added,
                              SUM(IIF(f.status = 'changed', 1, 0)) AS changed,
                              SUM(IIF(f.status = 'removed', 1, 0)) AS removed
                       FROM disks d
                       LEFT JOIN files f ON (f.disk_id = d.id)
                       GROUP BY d.id)", &stmt))
        return false;

    while (stmt.Step()) {
        DiskData disk = {};

        const char *name = (const char *)sqlite3_column_text(stmt, 2);
        const char *root = (const char *)sqlite3_column_text(stmt, 3);

        disk.id = sqlite3_column_int64(stmt, 0);
        CopyString((const char *)sqlite3_column_text(stmt, 1), disk.uuid);
        disk.name = DuplicateString(name, &str_alloc).ptr;
        disk.root = NormalizePath(root, (int)NormalizeFlag::EndWithSeparator | (int)NormalizeFlag::ForceSlash, &str_alloc).ptr;
        disk.total = sqlite3_column_int64(stmt, 4);
        disk.used = sqlite3_column_int64(stmt, 5);
        disk.files = sqlite3_column_int64(stmt, 6);
        disk.added = sqlite3_column_int64(stmt, 7);
        disk.changed = sqlite3_column_int64(stmt, 8);
        disk.removed = sqlite3_column_int64(stmt, 9);

        disks.Append(disk);
    }
    if (!stmt.IsValid())
        return false;

    std::swap(this->disks, disks);
    return true;
}

DiskData *BackupSet::FindDisk(int64_t idx)
{
    if (idx < 1 || --idx >= disks.len)
        return nullptr;

    return &disks[idx];
}

DiskData *BackupSet::FindDisk(const char *selector)
{
    int64_t idx = -1;
    ParseInt(selector, &idx, (int)ParseFlag::End);

    if (idx >= 1 && --idx < disks.len)
        return &disks[idx];

    for (DiskData &disk: disks) {
        if (TestStrI(disk.uuid, selector))
            return &disk;
        if (TestStrI(disk.name, selector))
            return &disk;
    }

    return nullptr;
}

SourceInfo *BackupSet::FindSource(int64_t idx)
{
    if (idx < 1 || --idx >= sources.len)
        return nullptr;

    return &sources[idx];
}

SourceInfo *BackupSet::FindSource(const char *selector)
{
    int64_t idx = -1;
    ParseInt(selector, &idx, (int)ParseFlag::End);

    if (idx >= 1 && --idx < sources.len)
        return &sources[idx];

    for (SourceInfo &src: sources) {
        if (TestStrI(src.root, selector))
            return &src;
    }

    return nullptr;
}

static int RunInit(Span<const char *> arguments)
{
    // Options
    const char *db_filename = GetDefaultDatabasePath();

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 init [options]

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
                db_filename = opt.current_value;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    if (TestFile(db_filename, FileType::File)) {
        LogError("File '%1' already exists", db_filename);
        return 1;
    }

    LogInfo("Initializing...");

    BackupSet set;
    if (!set.Open(db_filename, true))
        return 1;
    if (!set.Close())
        return 1;

    LogInfo("Done");

    return 0;
}

enum class DistributeResult {
    Complete,
    Partial,
    Error
};

class DistributeContext {
    struct UsageInfo {
        int64_t id;

        int64_t used;
        int64_t total;

        RG_HASHTABLE_HANDLER(UsageInfo, id);
    };

    BackupSet *set;

    int64_t changeset;

    HeapArray<UsageInfo> usages;
    HashTable<int64_t, UsageInfo *> usages_map;

public:
    DistributeContext(BackupSet *set);

    DistributeResult DistributeNew(const char *src_dir);
    bool DeleteOld();

    bool BackupNew(const DiskData &disk, bool checksum);
    bool DeleteExtra(const DiskData &disk);
    bool DeleteExtra(const DiskData &disk, const char *dest_dir, Size root_len);
};

DistributeContext::DistributeContext(BackupSet *set)
    : set(set), changeset(GetRandomInt64(0, INT64_MAX))
{
    for (const DiskData &disk: set->disks) {
        UsageInfo usage = {};

        usage.id = disk.id;
        usage.used = disk.used;
        usage.total = disk.total;

        usages.Append(usage);
    }

    for (UsageInfo &usage: usages) {
        usages_map.Set(&usage);
    }
}

DistributeResult DistributeContext::DistributeNew(const char *src_dir)
{
    BlockAllocator temp_alloc;

    if (!usages.len) {
        LogError("No backup disk is defined");
        return DistributeResult::Error;
    }

    bool complete = true;

    EnumResult ret = EnumerateDirectory(src_dir, nullptr, -1, [&](const char *basename, const FileInfo &file_info) {
        switch (file_info.type) {
            case FileType::Directory: {
                const char *dirname = Fmt(&temp_alloc, "%1%2/", src_dir, basename).ptr;

                switch (DistributeNew(dirname)) {
                    case DistributeResult::Complete: {} break;
                    case DistributeResult::Partial: { complete = false; } break;
                    case DistributeResult::Error: return false;
                }
            } break;

            case FileType::File: {
                const char *filename = Fmt(&temp_alloc, "%1%2", src_dir, basename).ptr;

                sq_Statement stmt;
                if (!set->db.Prepare("SELECT disk_id, size FROM files WHERE path = ?1", &stmt, filename))
                    return false;

                UsageInfo *usage = nullptr;

                if (stmt.Step()) {
                    int64_t disk_id = sqlite3_column_int64(stmt, 0);
                    int64_t size = sqlite3_column_int64(stmt, 1);

                    usage = usages_map.FindValue(disk_id, nullptr);

                    if (!usage) {
                        LogError("Unexplained disk info mismatch");
                        return false;
                    }
                    usage->used -= size;

                    if (file_info.size > usage->total - usage->used) {
                        usage = nullptr;
                    }
                } else if (!stmt.IsValid()) {
                    return false;
                }

                if (!usage) {
                    double min_ratio = 0;

                    for (UsageInfo &it: usages) {
                        int64_t available = it.total - it.used;

                        if (file_info.size <= available) {
                            double ratio = (double)(available - file_info.size) / it.total;

                            if (ratio > min_ratio) {
                                usage = &it;
                                min_ratio = ratio;
                            }
                        }
                    }

                    if (!usage) {
                        LogError("Not enough space for '%1'", filename);

                        complete = false;
                        return true;
                    }
                }

                usage->used += file_info.size;

                if (!set->db.Run(R"(INSERT INTO files (path, mtime, size, disk_id, status, changeset)
                                    VALUES (?1, ?2, ?3, ?4, 'added', ?5)
                                    ON CONFLICT (path) DO UPDATE SET mtime = excluded.mtime,
                                                                     size = excluded.size,
                                                                     disk_id = excluded.disk_id,
                                                                     status = IIF(mtime <> excluded.mtime OR
                                                                                  size <> excluded.size OR
                                                                                  disk_id <> excluded.disk_id, 'changed', status),
                                                                     changeset = excluded.changeset)",
                                 filename, file_info.mtime, file_info.size, usage->id, changeset))
                    return false;
            } break;

            case FileType::Link:
            case FileType::Device:
            case FileType::Pipe:
            case FileType::Socket: {
                const char *filename = Fmt(&temp_alloc, "%1%2", src_dir, basename).ptr;
                LogWarning("Ignoring special file '%1' (%2)", filename, FileTypeNames[(int)file_info.type]);
            } break;
        }

        return true;
    });

    if (ret != EnumResult::Success)
        return DistributeResult::Error;
    if (!complete)
        return DistributeResult::Partial;

    return DistributeResult::Complete;
}

bool DistributeContext::DeleteOld()
{
    if (!set->db.Run("DELETE FROM files WHERE status = 'added' AND changeset IS NOT ?1", changeset))
        return false;
    if (!set->db.Run("UPDATE files SET status = 'removed' WHERE changeset IS NOT ?1", changeset))
        return false;

    return true;
}

static const char *BuildDiskPath(const char *src_filename, const DiskData &disk, BlockAllocator *alloc)
{
    const char *dest_filename = nullptr;

#if defined(_WIN32)
    if (IsAsciiAlpha(src_filename[0]) && src_filename[1] == ':') {
        char drive = LowerAscii(src_filename[0]);
        const char *remain = TrimStrLeft(src_filename + 2, RG_PATH_SEPARATORS).ptr;

        dest_filename = Fmt(alloc, "%1%2/%3", disk.root, drive, remain).ptr;
    } else {
        const char *remain = TrimStrLeft(src_filename, RG_PATH_SEPARATORS).ptr;
        dest_filename = Fmt(alloc, "%1%2", disk.root, remain).ptr;
    }
#else
    {
        const char *remain = TrimStrLeft(src_filename, RG_PATH_SEPARATORS).ptr;
        dest_filename = Fmt(alloc, "%1%2", disk.root, remain).ptr;
    }
#endif

    return dest_filename;
}

static bool HashFile(int fd, const char *filename, Span<uint8_t> buf, uint8_t out_hash[32])
{
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);

    for (;;) {
#if defined(_WIN32)
        int bytes = _read(fd, buf.ptr, (unsigned int)buf.len);
#else
        ssize_t bytes = read(fd, buf.ptr, buf.len);
#endif

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

static bool IsTimeEquivalent(int64_t time1, int64_t time2)
{
    // Support FAT filesystems (precision is 2 seconds)
    int64_t delta = std::abs(time1 - time2);
    return delta < 2000;
}

bool DistributeContext::BackupNew(const DiskData &disk, bool checksum)
{
    BlockAllocator temp_alloc;

    sq_Statement stmt;
    if (!set->db.Prepare(R"(SELECT f.id, f.path, f.mtime, f.size
                            FROM disks d
                            INNER JOIN files f ON (f.disk_id = d.id)
                            WHERE d.uuid = ?1 AND f.status <> 'removed')",
                         &stmt, disk.uuid))
        return false;

    bool valid = true;

    Span<uint8_t> buf1 = AllocateSpan<uint8_t>(&temp_alloc, Mebibytes(4));
    Span<uint8_t> buf2 = AllocateSpan<uint8_t>(&temp_alloc, Mebibytes(4));
    RG_DEFER {
        ReleaseSpan(&temp_alloc, buf1);
        ReleaseSpan(&temp_alloc, buf2);
    };

    while (stmt.Step()) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        const char *src_filename = (const char *)sqlite3_column_text(stmt, 1);
        int64_t mtime = sqlite3_column_int64(stmt, 2);
        int64_t size = sqlite3_column_int64(stmt, 3);

        const char *dest_filename = BuildDiskPath(src_filename, disk, &temp_alloc);

        FileInfo dest_info;
        StatResult stat = StatFile(dest_filename, (int)StatFlag::SilentMissing, &dest_info);

        switch (stat) {
            case StatResult::Success: {
                bool changed = false;

                if (dest_info.size == size) {
                    if (checksum) {
                        int src_fd = OpenFile(src_filename, (int)OpenFlag::Read);
                        int dest_fd = OpenFile(dest_filename, (int)OpenFlag::Read);
                        RG_DEFER {
                            CloseDescriptor(src_fd);
                            CloseDescriptor(dest_fd);
                        };

                        if (src_fd < 0 || dest_fd < 0) {
                            valid = false;
                            break;
                        }

                        uint8_t src_hash[32] = {};
                        uint8_t dest_hash[32] = {};

                        Async async;

                        async.Run([&]() { return HashFile(src_fd, src_filename, buf1, src_hash); });
                        async.Run([&]() { return HashFile(dest_fd, dest_filename, buf2, dest_hash); });

                        if (!async.Sync()) {
                            valid = false;
                            continue;
                        }

                        changed = !!memcmp(src_hash, dest_hash, 32);
                    } else {
                        changed = !IsTimeEquivalent(dest_info.mtime, mtime);
                    }
                } else {
                    changed = true;
                }

                const char *status = changed ? "changed" : "ok";
                valid &= set->db.Run("UPDATE files SET status = ?2 WHERE id = ?1", id, status);
            } break;

            case StatResult::MissingPath: {
                valid &= set->db.Run("UPDATE files SET status = 'added' WHERE id = ?1", id);
            } break;

            case StatResult::AccessDenied:
            case StatResult::OtherError: {
                LogError("Failed to stat '%1': %1", strerror(errno));
                valid = false;
            } break;
        }
    }
    valid &= stmt.IsValid();

    return valid;
}

bool DistributeContext::DeleteExtra(const DiskData &disk)
{
    Size root_len = strlen(disk.root) - 1;
    bool success = DeleteExtra(disk, disk.root, root_len);

    if (!set->db.Run("DELETE FROM files WHERE disk_id = ?1 AND status = 'removed' AND changeset IS NOT ?2", disk.id, changeset))
        return false;

    return success;
}

// Return true if all children are deleted (directory is not empty)
bool DistributeContext::DeleteExtra(const DiskData &disk, const char *dest_dir, Size root_len)
{
    BlockAllocator temp_alloc;

    bool complete = true;

    EnumerateDirectory(dest_dir, nullptr, -1, [&](const char *basename, const FileInfo &file_info) {
        switch (file_info.type) {
            case FileType::Directory: {
                const char *dirname = Fmt(&temp_alloc, "%1%2/", dest_dir, basename).ptr;
                complete &= DeleteExtra(disk, dirname, root_len);
            } break;

            case FileType::File: {
                if (TestStr(basename, ".kiper"))
                    break;

                const char *filename = Fmt(&temp_alloc, "%1%2", dest_dir, basename).ptr;
                const char *origin = filename + root_len;

#if defined(_WIN32)
                if (origin[0] == '/' && IsAsciiAlpha(origin[1]) && origin[2] == '/') {
                    char drive = UpperAscii(origin[1]);
                    Span<const char> remain = TrimStrLeft(origin + 2, RG_PATH_SEPARATORS);

                    origin = Fmt(&temp_alloc, "%1:/%2", drive, remain).ptr;
                }
#endif

                sq_Statement stmt;
                if (!set->db.Prepare(R"(SELECT f.id
                                        FROM files f
                                        INNER JOIN disks d ON (d.id = f.disk_id)
                                        WHERE d.id = ?1 AND f.path = ?2)",
                                     &stmt, disk.id, origin))
                    return false;

                if (stmt.Step()) {
                    int64_t id = sqlite3_column_int64(stmt, 0);

                    if (!set->db.Run("UPDATE files SET changeset = ?2 WHERE id = ?1", id, changeset))
                        return false;
                } else if (stmt.IsValid()) {
                    if (!set->db.Run(R"(INSERT INTO files (path, mtime, size, disk_id, status, changeset)
                                        VALUES (?1, ?2, ?3, ?4, 'removed', ?5)
                                        ON CONFLICT (path) DO NOTHING)",
                                     origin, 0, 0, disk.id, changeset))
                        return false;
                } else {
                    return false;
                }
            } break;

            case FileType::Link:
            case FileType::Device:
            case FileType::Pipe:
            case FileType::Socket: {
                const char *filename = Fmt(&temp_alloc, "%1%2", dest_dir, basename).ptr;
                LogWarning("Ignoring special file '%1' (%2)", filename, FileTypeNames[(int)file_info.type]);

                complete = false;
            } break;
        }

        return true;
    });

    return complete;
}

static bool DistributeChanges(BackupSet *set, bool checksum)
{
    BlockAllocator temp_alloc;

    bool complete = true;

    DistributeContext ctx(set);

    bool success = set->db.Transaction([&]() {
        LogInfo("Detecting source changes...");

        for (const SourceInfo &src: set->sources) {
            switch (ctx.DistributeNew(src.root)) {
                case DistributeResult::Complete: {} break;
                case DistributeResult::Partial: { complete = false; } break;
                case DistributeResult::Error: return false;
            }
        }
        ctx.DeleteOld();

        LogInfo("Detecting backup changes...");

        for (const DiskData &disk: set->disks) {
            const char *uuid_filename = Fmt(&temp_alloc, "%1.kiper", disk.root).ptr;

            if (!TestFile(uuid_filename, FileType::File))
                continue;

            const char *uuid = ReadUUID(uuid_filename, &temp_alloc);

            if (!uuid) {
                LogError("Cannot find disk UUID from '%1", disk.root);
                return false;
            }
            if (!TestStr(uuid, disk.uuid))
                continue;

            complete &= ctx.BackupNew(disk, checksum);
            complete &= ctx.DeleteExtra(disk);
        }

        return true;
    });

    if (!success || !complete)
        return false;
    if (!set->Refresh())
        return false;

    return true;
}

static int RunStatus(Span<const char *> arguments)
{
    // Options
    const char *db_filename = GetDefaultDatabasePath();
    bool verbose = false;
    bool distribute = true;
    bool checksum = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 status [options]

Options:
    %!..+-D, --database_file <file>%!0   Set database file

    %!..+-v, --verbose%!0                Show detailed changes
        %!..+--no_detect%!0              Don't detect source changes

    %!..+-c, --checksum%!0               Use checksum (BLAKE3) to compare files)",
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
                db_filename = opt.current_value;
            } else if (opt.Test("-v", "--verbose")) {
                verbose = true;
            } else if (opt.Test("--no_detect")) {
                distribute = false;
            } else if (opt.Test("-c", "--checksum")) {
                checksum = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    BackupSet set;
    if (!set.Open(db_filename))
        return 1;

    distribute &= (set.disks.len > 0);
    if (distribute && !DistributeChanges(&set, checksum))
        return 1;

    bool blank = distribute;

#define BLANK(Cond) \
        do { \
            if (blank) { \
                PrintLn(); \
            } \
            blank = !!(Cond); \
        } while (false)

    BLANK(set.sources.len);
    if (set.sources.len) {
        PrintLn("Sources:");
        for (Size i = 0; i < set.sources.len; i++) {
            const SourceInfo &src = set.sources[i];
            PrintLn("  %!D..[%1]%!0 %!..+%2%!0", i + 1, src.root);
        }
        blank = true;
    } else {
        PrintLn("No source");
    }

    BLANK(set.disks.len);
    if (set.disks.len) {
        PrintLn("Disks:");
        for (Size i = 0; i < set.disks.len; i++) {
            const DiskData &disk = set.disks[i];
            double usage = (double)disk.used / (double)disk.total;

            PrintLn("  %!D..[%1]%!0 %!..+%2%!0 (%3)", i + 1, disk.name, disk.uuid);
            PrintLn("    Used: %!..+%1/%2%!0 (%3%%)", FmtDiskSize(disk.used), FmtDiskSize(disk.total), FmtDouble(usage * 100.0, 1));
            PrintLn("    Files: %!..+%1%!0", disk.files);

            if (disk.added || disk.changed || disk.removed) {
                int64_t changed = disk.added + disk.changed;
                int64_t removed = disk.removed + disk.changed;

                PrintLn("    Changes: %!G.++%1%!0 / %!R.+-%2%!0", changed, removed);
            } else {
                PrintLn("    Changes: none");
            }
        }
    } else {
        PrintLn("No disk");
    }

    if (verbose) {
        sq_Statement stmt;
        if (!set.db.Prepare("SELECT path, status, disk_id, size FROM files WHERE status <> 'ok'", &stmt))
            return 1;

        bool available = stmt.Step();
        if (!stmt.IsValid())
            return 1;
        BLANK(available);

        if (available) {
            PrintLn("Changes:");

            do {
                const char *filename = (const char *)sqlite3_column_text(stmt, 0);
                const char *status = (const char *)sqlite3_column_text(stmt, 1);
                int64_t disk_id = sqlite3_column_int64(stmt, 2);
                int64_t size = sqlite3_column_int64(stmt, 3);

                DiskData *disk = set.FindDisk(disk_id);

                if (!disk) {
                    LogError("Disk ID mismatch");
                    continue;
                }

                if (TestStr(status, "added") || TestStr(status, "changed")) {
                    PrintLn("  %!G..(+)%!0 %!..+%1%!0 %!D..(+%2 for %3)%!0", filename, FmtDiskSize(size), disk->name);
                } else if (TestStr(status, "removed")) {
                    PrintLn("  %!R..(-)%!0 %!..+%1%!0 %!D..(-%2 for %3)%!0", filename, FmtDiskSize(size), disk->name);
                } else {
                    RG_UNREACHABLE();
                }
            } while (stmt.Step());
        } else {
            PrintLn("No change");
        }
    }

#undef BLANK

    if (!set.Close())
        return 1;

    return 0;
}

static bool CopyFile(int src_fd, const char *src_filename, int dest_fd, const char *dest_filename, int64_t size, int64_t mtime)
{
    if (!SpliceFile(src_fd, src_filename, dest_fd, dest_filename, size))
        return false;
    if (!ResizeFile(dest_fd, dest_filename, size))
        return false;
    if (!FlushFile(dest_fd, dest_filename))
        return false;

    SetFileMetaData(dest_fd, dest_filename, mtime, 0, 0644);

    return true;
}

static bool PerformCopies(BackupSet *set, const DiskData &disk)
{
    BlockAllocator temp_alloc;

    sq_Statement stmt;
    if (!set->db.Prepare(R"(SELECT f.id, f.path, f.mtime, f.size
                            FROM disks d
                            INNER JOIN files f ON (f.disk_id = d.id)
                            WHERE d.uuid = ?1 AND f.status IN ('added', 'changed'))",
                         &stmt, disk.uuid))
        return false;

    bool valid = true;

    while (stmt.Step()) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        const char *src_filename = (const char *)sqlite3_column_text(stmt, 1);
        int64_t mtime = sqlite3_column_int64(stmt, 2);
        int64_t size = sqlite3_column_int64(stmt, 3);

        const char *dest_filename = BuildDiskPath(src_filename, disk, &temp_alloc);

        int src_fd = OpenFile(src_filename, (int)OpenFlag::Read);
        if (src_fd < 0) {
            valid = false;
            continue;
        }
        RG_DEFER { CloseDescriptor(src_fd); };

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
        RG_DEFER { CloseDescriptor(dest_fd); };

        LogInfo("Copy '%1' to %2 (%3)", src_filename, disk.name, disk.uuid);

        if (!CopyFile(src_fd, src_filename, dest_fd, dest_filename, size, mtime)) {
            valid = false;
            continue;
        }
        if (!set->db.Run("UPDATE files SET status = 'ok' WHERE id = ?1", id)) {
            valid = false;
            continue;
        }
    }

    return valid;
}

static bool PerformDeletions(BackupSet *set, const DiskData &disk)
{
    BlockAllocator temp_alloc;

    sq_Statement stmt;
    if (!set->db.Prepare(R"(SELECT f.id, f.path
                            FROM disks d
                            INNER JOIN files f ON (f.disk_id = d.id)
                            WHERE d.uuid = ?1 AND f.status = 'removed')",
                         &stmt, disk.uuid))
        return false;

    bool valid = true;

    while (stmt.Step()) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        const char *src_filename = (const char *)sqlite3_column_text(stmt, 1);

        const char *dest_filename = BuildDiskPath(src_filename, disk, &temp_alloc);

        LogInfo("Delete '%1'", dest_filename);

        if (!UnlinkFile(dest_filename)) {
            valid = false;
            continue;
        }
        if (!set->db.Run("DELETE FROM files WHERE id = ?1", id)) {
            valid = false;
            continue;
        }
    }

    return valid;
}

static int RunBackup(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *db_filename = GetDefaultDatabasePath();
    bool distribute = true;
    bool checksum = false;
    bool cleanup = false;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 backup [options]

Options:
    %!..+-D, --database_file <file>%!0   Set database file%!0

        %!..+--no_detect%!0              Don't detect source changes

    %!..+-c, --checksum%!0               Use checksum (BLAKE3) to compare files
        %!..+--cleanup%!0                Delete removed files from backup disks)",
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
                db_filename = opt.current_value;
            } else if (opt.Test("--no_detect")) {
                distribute = false;
            } else if (opt.Test("-c", "--checksum")) {
                checksum = true;
            } else if (opt.Test("--cleanup")) {
                cleanup = true;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        opt.LogUnusedArguments();
    }

    BackupSet set;
    if (!set.Open(db_filename))
        return 1;

    // Distribute changes
    if (distribute && !DistributeChanges(&set, checksum))
        return 1;

    Async async;
    int processed = 0;

    LogInfo("Backing up...");

    // Copy to backup disks
    for (const DiskData &disk: set.disks) {
        const char *uuid_filename = Fmt(&temp_alloc, "%1.kiper", disk.root).ptr;

        if (!TestFile(uuid_filename, FileType::File))
            continue;

        const char *uuid = ReadUUID(uuid_filename, &temp_alloc);

        if (!uuid) {
            LogError("Cannot find disk UUID from '%1", disk.root);
            return 1;
        }
        if (!TestStr(uuid, disk.uuid))
            continue;

        processed++;

        async.Run([&]() {
            if (!PerformCopies(&set, disk))
                return false;
            if (cleanup && !PerformDeletions(&set, disk))
                return false;

            return true;
        });
    }

    if (!async.Sync())
        return 1;
    if (!processed) {
        LogError("No backup disk found");
        return 1;
    }

    LogInfo("Done!");

    if (!set.Close())
        return 1;

    return 0;
}

static int RunAddSource(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *db_filename = GetDefaultDatabasePath();
    const char *src_dir = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 add_source [options] <directory>

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
                db_filename = opt.current_value;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        src_dir = opt.ConsumeNonOption();
        opt.LogUnusedArguments();
    }

    if (!src_dir) {
        LogError("Missing source path argument");
        return 1;
    }
    if (!PathIsAbsolute(src_dir)) {
        LogError("Source path must be absolute");
        return 1;
    }
    if (!TestFile(src_dir, FileType::Directory)) {
        LogError("Source directory '%1' does not exist", src_dir);
        return 1;
    }

    src_dir = NormalizePath(src_dir, (int)NormalizeFlag::EndWithSeparator | (int)NormalizeFlag::ForceSlash, &temp_alloc).ptr;

    BackupSet set;
    if (!set.Open(db_filename))
        return 1;

    if (!set.db.Run(R"(INSERT INTO sources (root)
                       VALUES (?1)
                       ON CONFLICT (root) DO NOTHING)", src_dir))
        return 1;

    if (!sqlite3_changes(set.db)) {
        LogError("Source '%1' already exists", src_dir);
        return 1;
    }

    LogInfo("Added source %!..+%1%!0", src_dir);

    if (!set.Close())
        return 1;

    return 0;
}

static int RunRemoveSource(Span<const char *> arguments)
{
    // Options
    const char *db_filename = GetDefaultDatabasePath();
    const char *identifier = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 remove_source [options] <ID | UUID | name>

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
                db_filename = opt.current_value;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        identifier = opt.ConsumeNonOption();
        opt.LogUnusedArguments();
    }

    if (!identifier) {
        LogError("Missing source identifier argument");
        return 1;
    }

    BackupSet set;
    if (!set.Open(db_filename))
        return 1;

    SourceInfo *src = set.FindSource(identifier);

    if (!src) {
        LogError("Cannot find source '%1'", identifier);
        return 1;
    }

    if (!set.db.Run("DELETE FROM sources WHERE id = ?1", src->id))
        return 1;

    LogInfo("Removed source %!..+%1%!0", src->root);

    if (!set.Close())
        return 1;

    return 0;
}

class IntegrateContext {
    BackupSet *set;

    int64_t changeset;

    int64_t disk_id;
    const char *disk_dir;

    BlockAllocator temp_alloc;

public:
    IntegrateContext(BackupSet *set, int64_t disk_id, const char *disk_dir);

    bool AddNew() { return AddNew(disk_dir); }
    bool DeleteOld();

private:
    bool AddNew(const char *src_dir);
};

IntegrateContext::IntegrateContext(BackupSet *set, int64_t disk_id, const char *disk_dir)
    : set(set), changeset(GetRandomInt64(0, INT64_MAX)), disk_id(disk_id)
{
    this->disk_dir = NormalizePath(disk_dir, (int)NormalizeFlag::EndWithSeparator | (int)NormalizeFlag::ForceSlash, &temp_alloc).ptr;
}

bool IntegrateContext::AddNew(const char *src_dir)
{
    EnumResult ret = EnumerateDirectory(src_dir, nullptr, -1, [&](const char *basename, const FileInfo &file_info) {
        switch (file_info.type) {
            case FileType::Directory: {
                const char *dirname = Fmt(&temp_alloc, "%1%2/", src_dir, basename).ptr;

                if (!AddNew(dirname))
                    return false;
            } break;

            case FileType::File: {
                if (TestStr(basename, ".kiper"))
                    break;

                const char *filename = Fmt(&temp_alloc, "%1%2", src_dir, basename).ptr;

                if (!set->db.Run(R"(INSERT INTO files (path, mtime, size, disk_id, status, changeset)
                                    VALUES (?1, ?2, ?3, ?4, 'added', ?5)
                                    ON CONFLICT (path) DO UPDATE SET mtime = excluded.mtime,
                                                                     size = excluded.size,
                                                                     disk_id = excluded.disk_id,
                                                                     status = 'changed',
                                                                     changeset = excluded.changeset)",
                                 filename, file_info.mtime, file_info.size, disk_id, changeset))
                    return false;
            } break;

            case FileType::Link:
            case FileType::Device:
            case FileType::Pipe:
            case FileType::Socket: {
                const char *filename = Fmt(&temp_alloc, "%1%2", src_dir, basename).ptr;
                LogWarning("Ignoring special file '%1' (%2)", filename, FileTypeNames[(int)file_info.type]);
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
    bool success = set->db.Run("DELETE FROM files WHERE disk_id = ?1 AND changeset IS NOT ?2", disk_id, changeset);
    return success;
}

static int64_t EsimateAvailableSpace(BackupSet *set, int64_t disk_id, const char *disk_dir)
{
    VolumeInfo volume;
    if (!GetVolumeInfo(disk_dir, &volume))
        return -1;

    sq_Statement stmt;
    if (!set->db.Prepare("SELECT SUM(size) * 1.02 FROM files WHERE disk_id = ?1 GROUP BY disk_id",
                         &stmt, disk_id))
        return -1;

    if (stmt.Step()) {
        volume.available += sqlite3_column_int64(stmt, 0);
    } else if (!stmt.IsValid()) {
        return -1;
    }

    // Max out at 98% of the total size to account for metadata (or at least, try to)
    volume.total -= volume.total / 50;
    volume.available = std::min(volume.total, volume.available);

    return volume.available;
}

static int RunAddDisk(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *db_filename = GetDefaultDatabasePath();
    const char *name = nullptr;
    int64_t size = -1;
    const char *disk_dir = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 add_disk [options] <directory>

Options:
    %!..+-D, --database_file <file>%!0   Set database file%!0

    %!..+-n, --name <name>%!0            Set disk name
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
                db_filename = opt.current_value;
            } else if (opt.Test("-n", "--name", OptionType::Value)) {
                name = opt.current_value;
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
        LogError("Missing disk path argument");
        return 1;
    }
    if (!PathIsAbsolute(disk_dir)) {
        LogError("Disk path must be absolute");
        return 1;
    }
    if (!TestFile(disk_dir, FileType::Directory)) {
        LogError("Disk directory '%1' does not exist", disk_dir);
        return 1;
    }

    disk_dir = NormalizePath(disk_dir, (int)NormalizeFlag::EndWithSeparator | (int)NormalizeFlag::ForceSlash, &temp_alloc).ptr;

    if (!name) {
        Span<const char> basename = SplitStrReverseAny(TrimStrRight(disk_dir, RG_PATH_SEPARATORS), RG_PATH_SEPARATORS);

        if (!basename.len) {
            LogError("Missing disk name (use -n option)");
            return 1;
        }

        name = DuplicateString(basename, &temp_alloc).ptr;
    }

    BackupSet set;
    if (!set.Open(db_filename))
        return 1;

    const char *uuid = nullptr;
    {
        const char *filename = Fmt(&temp_alloc, "%1.kiper", disk_dir).ptr;

        if (TestFile(filename, FileType::File)) {
            uuid = ReadUUID(filename, &temp_alloc);

            if (!uuid)
                return 1;
        } else {
            uuid = GenerateUUIDv4(&temp_alloc);

            if (!WriteFile(uuid, filename))
                return 1;
        }
    }

    bool success = set.db.Transaction([&]() {
        int64_t disk_id = -1;

        // Create new disk
        {
            sq_Statement stmt;
            if (!set.db.Prepare(R"(INSERT INTO disks (uuid, name, root, size) VALUES (?1, ?2, ?3, ?4)
                                   ON CONFLICT (uuid) DO NOTHING
                                   RETURNING id)",
                                &stmt, uuid, name, disk_dir, size))
                return false;

            if (!stmt.Step()) {
                if (stmt.IsValid()) {
                    LogError("Disk '%1' %!D..[%2]%!0 already exists", disk_dir, uuid);
                }
                return false;
            }

            disk_id = sqlite3_column_int64(stmt, 0);
        }

        // Run integration
        {
            IntegrateContext ctx(&set, disk_id, disk_dir);

            if (!ctx.AddNew())
                return false;
            ctx.DeleteOld();
        }

        if (size < 0) {
            int64_t available = EsimateAvailableSpace(&set, disk_id, disk_dir);

            if (available < 0)
                return false;
            if (!set.db.Run("UPDATE disks SET size = ?2 WHERE id = ?1", disk_id, available))
                return false;
        }

        return true;
    });
    if (!success)
        return 1;

    LogInfo("Added disk %!..+%1%!0 for '%2' %!D..[%3]%!0", name, disk_dir, uuid);

    if (!set.Close())
        return 1;

    return 0;
}

static int RunEditDisk(Span<const char *> arguments)
{
    BlockAllocator temp_alloc;

    // Options
    const char *db_filename = GetDefaultDatabasePath();
    const char *name = nullptr;
    int64_t size = -1;
    const char *identifier = nullptr;
    const char *disk_dir = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 edit_disk [options] <ID | UUID | name> [<directory>]

Options:
    %!..+-D, --database_file <file>%!0   Set database file%!0

    %!..+-n, --name <name>%!0            Set disk name
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
                db_filename = opt.current_value;
            } else if (opt.Test("-n", "--name", OptionType::Value)) {
                name = opt.current_value;
            } else if (opt.Test("-s", "--size", OptionType::Value)) {
                if (!ParseSize(opt.current_value, &size))
                    return 1;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        identifier = opt.ConsumeNonOption();
        disk_dir = opt.ConsumeNonOption();

        opt.LogUnusedArguments();
    }

    if (!identifier) {
        LogError("Missing disk identifier argument");
        return 1;
    }

    if (disk_dir) {
        if (!PathIsAbsolute(disk_dir)) {
            LogError("Disk path must be absolute");
            return 1;
        }
        if (!TestFile(disk_dir, FileType::Directory)) {
            LogError("Disk directory '%1' does not exist", disk_dir);
            return 1;
        }

        disk_dir = NormalizePath(disk_dir, (int)NormalizeFlag::EndWithSeparator | (int)NormalizeFlag::ForceSlash, &temp_alloc).ptr;
    }

    BackupSet set;
    if (!set.Open(db_filename))
        return 1;

    DiskData *disk = set.FindDisk(identifier);

    if (!disk) {
        LogError("Cannot find disk '%1'", identifier);
        return 1;
    }

    bool success = set.db.Transaction([&]() {
        disk_dir = disk_dir ? disk_dir : disk->root;
        name = name ? name : disk->name;

        if (!set.db.Run("UPDATE disks SET root = ?2, name = ?3, size = ?4 WHERE id = ?1",
                        disk->id, disk_dir, name, size))
                return false;

        // Run integration
        {
            IntegrateContext ctx(&set, disk->id, disk_dir);

            if (!ctx.AddNew())
                return false;
            ctx.DeleteOld();
        }

        if (size < 0) {
            int64_t available = EsimateAvailableSpace(&set, disk->id, disk_dir);

            if (available < 0)
                return false;
            if (!set.db.Run("UPDATE disks SET size = ?2 WHERE id = ?1", disk->id, available))
                return false;
        }

        return true;
    });
    if (!success)
        return 1;

    LogInfo("Updated disk %!..+%1%!0 for '%2' %!D..[%3]%!0", name, disk_dir, disk->uuid);

    if (!set.Close())
        return 1;

    return 0;
}

static int RunRemoveDisk(Span<const char *> arguments)
{
    // Options
    const char *db_filename = GetDefaultDatabasePath();
    const char *identifier = nullptr;

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 remove_disk [options] <ID | UUID | name>

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
                db_filename = opt.current_value;
            } else {
                opt.LogUnknownError();
                return 1;
            }
        }

        identifier = opt.ConsumeNonOption();
        opt.LogUnusedArguments();
    }

    if (!identifier) {
        LogError("Missing disk identifier argument");
        return 1;
    }

    BackupSet set;
    if (!set.Open(db_filename))
        return 1;

    DiskData *disk = set.FindDisk(identifier);

    if (!disk) {
        LogError("Cannot find disk '%1'", identifier);
        return 1;
    }
    if (!set.db.Run("DELETE FROM disks WHERE id = ?1", disk->id))
        return 1;

    LogInfo("Deleted disk %!..+%1%!0 for '%2' %!D..[%3]%!0", disk->name, disk->root, disk->uuid);

    if (!set.Close())
        return 1;

    return 0;
}

int Main(int argc, char **argv)
{
    RG_CRITICAL(argc >= 1, "First argument is missing");

    const auto print_usage = [=](StreamWriter *st) {
        PrintLn(st,
R"(Usage: %!..+%1 <command> [args]%!0

Commands:
    %!..+init%!0                         Init kiper database for backups
    %!..+status%!0                       Get backup status and recorded disk usage
    %!..+backup%!0                       Distribute changes and backup to plugged disks

    %!..+add_source%!0                   Add backup source directory
    %!..+remove_source%!0                Remove directory from known sources

    %!..+add_disk%!0                     Add disk for future backups
    %!..+edit_disk%!0                    Edit existing backup disk
    %!..+remove_disk%!0                  Remove disk from backups)",
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

    if (TestStr(cmd, "init")) {
        return RunInit(arguments);
    } else if (TestStr(cmd, "status")) {
        return RunStatus(arguments);
    } else if (TestStr(cmd, "backup")) {
        return RunBackup(arguments);
    } else if (TestStr(cmd, "add_disk")) {
        return RunAddDisk(arguments);
    } else if (TestStr(cmd, "edit_disk")) {
        return RunEditDisk(arguments);
    } else if (TestStr(cmd, "remove_disk")) {
        return RunRemoveDisk(arguments);
    } else if (TestStr(cmd, "add_source")) {
        return RunAddSource(arguments);
    } else if (TestStr(cmd, "remove_source")) {
        return RunRemoveSource(arguments);
    } else {
        LogError("Unknown command '%1'", cmd);
        return 1;
    }
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::RunApp(argc, argv); }
