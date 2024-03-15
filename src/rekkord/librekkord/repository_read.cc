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
#include "disk.hh"
#include "repository.hh"

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <io.h>
#else
    #include <sys/stat.h>
    #include <sys/time.h>
    #include <unistd.h>
#endif

namespace RG {

// Fix mess caused by windows.h
#ifdef CreateSymbolicLink
    #undef CreateSymbolicLink
#endif

enum class ExtractFlag {
    SkipMeta = 1 << 0,
    AllowSeparators = 1 << 1,
    FlattenName = 1 << 2
};

struct EntryInfo {
    rk_Hash hash;

    int kind;
    unsigned int flags;

    const char *basename;

    int64_t mtime;
    int64_t btime;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    int64_t size;

    const char *filename;
};

class GetContext {
    rk_Disk *disk;
    bool chown;

    Async tasks;

    std::atomic<int64_t> stat_len { 0 };

public:
    GetContext(rk_Disk *disk, bool chown);

    bool ExtractEntries(Span<const uint8_t> entries, unsigned int flags, const char *dest_dirname);
    bool ExtractEntries(Span<const uint8_t> entries, unsigned int flags, const EntryInfo &dest);

    int GetFile(const rk_Hash &hash, rk_BlobType type, Span<const uint8_t> file_blob, const char *dest_filename);

    bool Sync() { return tasks.Sync(); }

    int64_t GetLen() const { return stat_len; }
};

GetContext::GetContext(rk_Disk *disk, bool chown)
    : disk(disk), chown(chown), tasks(disk->GetThreads())
{
}

#ifdef _WIN32

static bool ReserveFile(int fd, const char *filename, int64_t len)
{
    HANDLE h = (HANDLE)_get_osfhandle(fd);

    LARGE_INTEGER prev_pos = {};
    if (!SetFilePointerEx(h, prev_pos, &prev_pos, FILE_CURRENT)) {
        LogError("Failed to resize file '%1': %2", filename, GetWin32ErrorString());
        return false;
    }
    RG_DEFER { SetFilePointerEx(h, prev_pos, nullptr, FILE_BEGIN); };

    if (!SetFilePointerEx(h, { .QuadPart = len }, nullptr, FILE_BEGIN)) {
        LogError("Failed to resize file '%1': %2", filename, GetWin32ErrorString());
        return false;
    }
    if (!SetEndOfFile(h)) {
        LogError("Failed to resize file '%1': %2", filename, GetWin32ErrorString());
        return false;
    }

    return true;
}

static bool WriteAt(int fd, const char *filename, int64_t offset, Span<const uint8_t> buf)
{
    RG_ASSERT(buf.len < UINT32_MAX);

    HANDLE h = (HANDLE)_get_osfhandle(fd);

    while (buf.len) {
        OVERLAPPED ov = {};
        DWORD written = 0;

        ov.OffsetHigh = (uint32_t)((offset & 0xFFFFFFFF00000000ll) >> 32);
        ov.Offset = (uint32_t)(offset & 0xFFFFFFFFll);

        if (!WriteFile(h, buf.ptr, (DWORD)buf.len, &written, &ov)) {
            LogError("Failed to write to '%1': %2", filename, GetWin32ErrorString());
            return false;
        }

        offset += (Size)written;
        buf.ptr += (Size)written;
        buf.len -= (Size)written;
    }

    return true;
}

static bool CreateSymbolicLink(const char *filename, const char *target, bool)
{
    LogWarning("Ignoring symbolic link '%1' to '%2'", filename, target);
    return true;
}

static FILETIME UnixTimeToFileTime(int64_t time)
{
    time = (time + 11644473600000ll) * 10000;

    FILETIME ft;
    ft.dwHighDateTime = (DWORD)(time >> 32);
    ft.dwLowDateTime = (DWORD)time;

    return ft;
}

static void SetFileOwner(int, const char *, uint32_t, uint32_t)
{
}

static void SetFileMetaData(int fd, const char *filename, int64_t mtime, int64_t btime, uint32_t)
{
    HANDLE h = (HANDLE)_get_osfhandle(fd);

    FILETIME mft = UnixTimeToFileTime(mtime);
    FILETIME bft = UnixTimeToFileTime(btime);

    if (!SetFileTime(h, &bft, nullptr, &mft)) {
        LogError("Failed to set modification time of '%1': %2", filename, GetWin32ErrorString());
    }
}

#else

static bool ReserveFile(int fd, const char *filename, int64_t len)
{
    if (ftruncate(fd, len) < 0) {
        if (errno == EINVAL) {
            // Only write() calls seem to return ENOSPC, ftruncate() seems to fail with EINVAL
            LogError("Failed to reserve file '%1': not enough space", filename);
        } else {
            LogError("Failed to reserve file '%1': %2", filename, strerror(errno));
        }
        return false;
    }

    return true;
}

static bool WriteAt(int fd, const char *filename, int64_t offset, Span<const uint8_t> buf)
{
    while (buf.len) {
        Size written = RG_POSIX_RESTART_EINTR(pwrite(fd, buf.ptr, buf.len, (off_t)offset), < 0);

        if (written < 0) {
            LogError("Failed to write to '%1': %2", filename, strerror(errno));
            return false;
        }

        offset += written;
        buf.ptr += written;
        buf.len -= written;
    }

    return true;
}

static bool CreateSymbolicLink(const char *filename, const char *target, bool overwrite)
{
retry:
    if (symlink(target, filename) < 0) {
        if (errno == EEXIST && overwrite) {
            struct stat sb;
            if (!lstat(filename, &sb) && S_ISLNK(sb.st_mode)) {
                unlink(filename);
            }

            overwrite = false;
            goto retry;
        }

        LogError("Failed to create symbolic link '%1': %2", filename, strerror(errno));
        return false;
    }

    return true;
}

static void SetFileOwner(int fd, const char *filename, uint32_t uid, uint32_t gid)
{
    if (fchown(fd, (uid_t)uid, (gid_t)gid) < 0) {
        LogError("Failed to change owner of '%1' (ignoring)", filename);
    }
}

static void SetFileMetaData(int fd, const char *filename, int64_t mtime, int64_t, uint32_t mode)
{
    struct timespec times[2] = {};

    times[0].tv_nsec = UTIME_OMIT;
    times[1].tv_sec = mtime / 1000;
    times[1].tv_nsec = (mtime % 1000) * 1000;

    if (futimens(fd, times) < 0) {
        LogError("Failed to set mtime of '%1' (ignoring)", filename);
    }

    if (fchmod(fd, (mode_t)mode) < 0) {
        LogError("Failed to set permissions of '%1' (ignoring)", filename);
    }
}

#endif

// Does not fill EntryInfo::filename
static Size DecodeEntry(Span<const uint8_t> entries, Size offset, bool allow_separators,
                        Allocator *alloc, EntryInfo *out_entry)
{
    rk_RawFile *ptr = (rk_RawFile *)(entries.ptr + offset);

    if (entries.len - offset < RG_SIZE(*ptr)) {
        LogError("Malformed entry in directory blob");
        return -1;
    }

    EntryInfo entry = {};

    entry.hash = ptr->hash;
    entry.kind = LittleEndian(ptr->kind);
    entry.flags = LittleEndian(ptr->flags);
    entry.basename = DuplicateString(ptr->GetName(), alloc).ptr;

    entry.mtime = LittleEndian(ptr->mtime);
    entry.btime = LittleEndian(ptr->btime);
    entry.mode = LittleEndian(ptr->mode);
    entry.uid = LittleEndian(ptr->uid);
    entry.gid = LittleEndian(ptr->gid);
    entry.size = LittleEndian(ptr->size);

    // Sanity checks
    if (entry.kind != (int8_t)rk_RawFile::Kind::Directory &&
            entry.kind != (int8_t)rk_RawFile::Kind::File &&
            entry.kind != (int8_t)rk_RawFile::Kind::Link &&
            entry.kind != (int8_t)rk_RawFile::Kind::Unknown) {
        LogError("Unknown object kind 0x%1", FmtHex((unsigned int)entry.kind));
        return -1;
    }
    if (!entry.basename[0] || PathContainsDotDot(entry.basename)) {
        LogError("Unsafe object name '%1'", entry.basename);
        return -1;
    }
    if (PathIsAbsolute(entry.basename)) {
        LogError("Unsafe object name '%1'", entry.basename);
        return -1;
    }
    if (!allow_separators && strpbrk(entry.basename, RG_PATH_SEPARATORS)) {
        LogError("Unsafe object name '%1'", entry.basename);
        return -1;
    }

    *out_entry = entry;
    return ptr->GetSize();
}

bool GetContext::ExtractEntries(Span<const uint8_t> entries, unsigned int flags, const char *dest_dirname)
{
    flags |= (int)ExtractFlag::SkipMeta;

    EntryInfo dest = {};
    dest.filename = dest_dirname;

    return ExtractEntries(entries, flags, dest);
}

bool GetContext::ExtractEntries(Span<const uint8_t> entries, unsigned int flags, const EntryInfo &dest)
{
    // XXX: Make sure each path does not clobber a previous one

    if (entries.len < RG_SIZE(int64_t)) [[unlikely]] {
        LogError("Malformed directory blob");
        return false;
    }
    entries.len -= RG_SIZE(int64_t);

    // Get total length from end of stream
    int64_t dir_len = 0;
    memcpy(&dir_len, entries.end(), RG_SIZE(dir_len));
    dir_len = LittleEndian(dir_len);

    struct SharedContext {
        BlockAllocator temp_alloc;

        EntryInfo meta = {};
        bool chown = false;

        ~SharedContext() {
            if (meta.filename) {
                int fd = OpenDescriptor(meta.filename, (int)OpenFlag::Write | (int)OpenFlag::Directory);
                RG_DEFER { close(fd); };

                // Set directory metadata
                if (chown) {
                    SetFileOwner(fd, meta.filename, meta.uid, meta.gid);
                }
                SetFileMetaData(fd, meta.filename, meta.mtime, meta.btime, meta.mode);
            }
        }
    };

    std::shared_ptr<SharedContext> ctx = std::make_shared<SharedContext>();

    if (!(flags & (int)ExtractFlag::SkipMeta)) {
        RG_ASSERT(dest.basename);

        ctx->meta = dest;
        ctx->meta.filename = DuplicateString(dest.filename, &ctx->temp_alloc).ptr;
        ctx->chown = chown;
    }

    for (Size offset = 0; offset < entries.len;) {
        EntryInfo entry = {};

        Size skip = DecodeEntry(entries, offset, flags & (int)ExtractFlag::AllowSeparators, &ctx->temp_alloc, &entry);
        if (skip < 0)
            return false;
        offset += skip;

        if (entry.kind == (int)rk_RawFile::Kind::Unknown)
            continue;
        if (!(entry.flags & (int)rk_RawFile::Flags::Readable))
            continue;

        if (flags & (int)ExtractFlag::FlattenName) {
            entry.filename = Fmt(&ctx->temp_alloc, "%1%/%2", dest.filename, SplitStrReverse(entry.basename, '/')).ptr;
        } else {
            entry.filename = Fmt(&ctx->temp_alloc, "%1%/%2", dest.filename, entry.basename).ptr;

            if ((flags & (int)ExtractFlag::AllowSeparators) && !EnsureDirectoryExists(entry.filename))
                return false;
        }

        tasks.Run([=, ctx = ctx, this] () {
            rk_BlobType entry_type;
            HeapArray<uint8_t> entry_blob;
            if (!disk->ReadBlob(entry.hash, &entry_type, &entry_blob))
                return false;

            switch (entry.kind) {
                case (int)rk_RawFile::Kind::Directory: {
                    if (entry_type != rk_BlobType::Directory) {
                        LogError("Blob '%1' is not a Directory", entry.hash);
                        return false;
                    }

                    if (!MakeDirectory(entry.filename, false))
                        return false;
                    if (!ExtractEntries(entry_blob, 0, entry))
                        return false;
                } break;

                case (int)rk_RawFile::Kind::File: {
                    if (entry_type != rk_BlobType::File && entry_type != rk_BlobType::Chunk) {
                        LogError("Blob '%1' is not a File", entry.hash);
                        return false;
                    }

                    int fd = GetFile(entry.hash, entry_type, entry_blob, entry.filename);
                    if (fd < 0)
                        return false;
                    RG_DEFER { close(fd); };

                    // Set file metadata
                    if (chown) {
                        SetFileOwner(fd, entry.filename, entry.uid, entry.gid);
                    }
                    SetFileMetaData(fd, entry.filename, entry.mtime, entry.btime, entry.mode);
                } break;

                case (int)rk_RawFile::Kind::Link: {
                    if (entry_type != rk_BlobType::Link) {
                        LogError("Blob '%1' is not a Link", entry.hash);
                        return false;
                    }

                    // NUL terminate the path
                    entry_blob.Append(0);

                    if (!CreateSymbolicLink(entry.filename, (const char *)entry_blob.ptr, true))
                        return false;
                } break;

                default: { RG_UNREACHABLE(); } break;
            }

            return true;
        });
    }

    return true;
}

int GetContext::GetFile(const rk_Hash &hash, rk_BlobType type, Span<const uint8_t> file_blob, const char *dest_filename)
{
    RG_ASSERT(type == rk_BlobType::File || type == rk_BlobType::Chunk);

    int fd = -1;
    RG_DEFER_N(err_guard) { close(fd); };

    char tmp_filename[4096];
    {
        PushLogFilter([](LogLevel, const char *, const char *, FunctionRef<LogFunc>) {});
        RG_DEFER_N(log_guard) { PopLogFilter(); };

        for (Size i = 0; i < 1000; i++) {
            Size len = Fmt(tmp_filename, "%1.%2", dest_filename, FmtRandom(12)).len;

            if (len >= RG_SIZE(tmp_filename) - 1) [[unlikely]] {
                PopLogFilter();
                log_guard.Disable();

                LogError("Cannot create temporary file for '%1': path too long", dest_filename);
                return -1;
            }

            // We want to show an error on last try
            if (i == 999) [[unlikely]] {
                PopLogFilter();
                log_guard.Disable();
            }

            fd = OpenDescriptor(tmp_filename, (int)OpenFlag::Write | (int)OpenFlag::Exclusive);

            if (fd >= 0) [[likely]]
                break;
        }

        if (fd < 0) [[unlikely]]
            return -1;
    }

    int64_t file_len = -1;
    switch (type) {
        case rk_BlobType::File: {
            if (file_blob.len % RG_SIZE(rk_RawChunk) != RG_SIZE(int64_t)) {
                LogError("Malformed file blob '%1'", hash);
                return -1;
            }
            file_blob.len -= RG_SIZE(int64_t);

            // Get file length from end of stream
            memcpy(&file_len, file_blob.end(), RG_SIZE(file_len));
            file_len = LittleEndian(file_len);

            if (file_len < 0) {
                LogError("Malformed file blob '%1'", hash);
                return -1;
            }
            if (!ReserveFile(fd, dest_filename, file_len))
                return -1;

            Async async(&tasks);

            // Write unencrypted file
            for (Size offset = 0; offset < file_blob.len; offset += RG_SIZE(rk_RawChunk)) {
                async.Run([=, this]() {
                    rk_RawChunk entry = {};

                    memcpy(&entry, file_blob.ptr + offset, RG_SIZE(entry));
                    entry.offset = LittleEndian(entry.offset);
                    entry.len = LittleEndian(entry.len);

                    rk_BlobType type;
                    HeapArray<uint8_t> buf;
                    if (!disk->ReadBlob(entry.hash, &type, &buf))
                        return false;

                    if (type != rk_BlobType::Chunk) [[unlikely]] {
                        LogError("Blob '%1' is not a Chunk", entry.hash);
                        return false;
                    }
                    if (buf.len != entry.len) [[unlikely]] {
                        LogError("Chunk size mismatch for '%1'", entry.hash);
                        return false;
                    }
                    if (!WriteAt(fd, dest_filename, entry.offset, buf)) {
                        LogError("Failed to write to '%1': %2", dest_filename, strerror(errno));
                        return false;
                    }

                    return true;
                });
            }

            if (!async.Sync())
                return -1;

            // Check actual file size
            if (file_blob.len) {
                const rk_RawChunk *entry = (const rk_RawChunk *)(file_blob.end() - RG_SIZE(rk_RawChunk));
                int64_t len = LittleEndian(entry->offset) + LittleEndian(entry->len);

                if (len != file_len) [[unlikely]] {
                    LogError("File size mismatch for '%1'", entry->hash);
                    return -1;
                }
            }
        } break;

        case rk_BlobType::Chunk: {
            file_len = file_blob.len;

            if (!WriteAt(fd, dest_filename, 0, file_blob)) {
                LogError("Failed to write to '%1': %2", dest_filename, strerror(errno));
                return -1;
            }
        } break;

        case rk_BlobType::Directory:
        case rk_BlobType::Snapshot:
        case rk_BlobType::Link: { RG_UNREACHABLE(); } break;
    }

    if (!FlushFile(fd, dest_filename))
        return -1;

    err_guard.Disable();
    close(fd);

    if (!RenameFile(tmp_filename, dest_filename, (int)RenameFlag::Overwrite))
        return -1;

    fd = OpenDescriptor(dest_filename, (int)OpenFlag::Append);
    if (fd < 0)
        return -1;

    // Finally :)
    stat_len += file_len;

    return fd;
}

bool rk_Get(rk_Disk *disk, const rk_Hash &hash, const rk_GetSettings &settings, const char *dest_path, int64_t *out_len)
{
    rk_BlobType type;
    HeapArray<uint8_t> blob;
    if (!disk->ReadBlob(hash, &type, &blob))
        return false;

    GetContext get(disk, settings.chown);

    switch (type) {
        case rk_BlobType::Chunk:
        case rk_BlobType::File: {
            if (!settings.force) {
                if (TestFile(dest_path) && !IsDirectoryEmpty(dest_path)) {
                    LogError("File '%1' already exists", dest_path);
                    return false;
                }
            }

            int fd = get.GetFile(hash, type, blob, dest_path);
            if (fd < 0)
                return false;
            close(fd);
        } break;

        case rk_BlobType::Directory: {
            if (!settings.force && TestFile(dest_path, FileType::Directory)) {
                if (!IsDirectoryEmpty(dest_path)) {
                    LogError("Directory '%1' exists and is not empty", dest_path);
                    return false;
                }
            } else {
                if (!MakeDirectory(dest_path, !settings.force))
                    return false;
            }

            if (!get.ExtractEntries(blob, 0, dest_path))
                return false;
        } break;

        case rk_BlobType::Snapshot: {
            if (!settings.force && TestFile(dest_path, FileType::Directory)) {
                if (!IsDirectoryEmpty(dest_path)) {
                    LogError("Directory '%1' exists and is not empty", dest_path);
                    return false;
                }
            } else {
                if (!MakeDirectory(dest_path, !settings.force))
                    return false;
            }

            // There must be at least one entry
            if (blob.len <= RG_SIZE(rk_SnapshotHeader)) {
                LogError("Malformed snapshot blob '%1'", hash);
                return false;
            }

            Span<uint8_t> entries = blob.Take(RG_SIZE(rk_SnapshotHeader), blob.len - RG_SIZE(rk_SnapshotHeader));
            unsigned int flags = (int)ExtractFlag::AllowSeparators | (settings.flat ? (int)ExtractFlag::FlattenName : 0);

            if (!get.ExtractEntries(entries, flags, dest_path))
                return false;
        } break;

        case rk_BlobType::Link: {
            blob.Append(0);

            if (!CreateSymbolicLink(dest_path, (const char *)blob.ptr, settings.force))
                return false;
        } break;
    }

    if (!get.Sync())
        return false;

    if (out_len) {
        *out_len += get.GetLen();
    }
    return true;
}

bool rk_Log(rk_Disk *disk, Allocator *alloc, HeapArray<rk_SnapshotInfo> *out_snapshots)
{
    Size prev_len = out_snapshots->len;
    RG_DEFER_N(out_guard) { out_snapshots->RemoveFrom(prev_len); };

    HeapArray<rk_Hash> hashes;
    if (!disk->ListTags(&hashes))
        return false;

    Async async(disk->GetThreads());

    // Gather snapshot information
    {
        std::mutex mutex;

        for (const rk_Hash &hash: hashes) {
            async.Run([=, &mutex]() {
                rk_SnapshotInfo snapshot = {};

                rk_BlobType type;
                HeapArray<uint8_t> blob;
                if (!disk->ReadBlob(hash, &type, &blob))
                    return false;

                if (type != rk_BlobType::Snapshot) {
                    LogError("Blob '%1' is not a Snapshot (ignoring)", hash);
                    return true;
                }
                if (blob.len <= RG_SIZE(rk_SnapshotHeader)) {
                    LogError("Malformed snapshot blob '%1' (ignoring)", hash);
                    return true;
                }

                std::lock_guard lock(mutex);
                const rk_SnapshotHeader *header = (const rk_SnapshotHeader *)blob.ptr;

                snapshot.hash = hash;
                snapshot.name = header->name[0] ? DuplicateString(header->name, alloc).ptr : nullptr;
                snapshot.time = LittleEndian(header->time);
                snapshot.len = LittleEndian(header->len);
                snapshot.stored = LittleEndian(header->stored) + blob.len;

                out_snapshots->Append(snapshot);

                return true;
            });
        }
    }

    if (!async.Sync() && out_snapshots->len == prev_len)
        return false;

    std::sort(out_snapshots->ptr + prev_len, out_snapshots->end(),
              [](const rk_SnapshotInfo &snapshot1, const rk_SnapshotInfo &snapshot2) { return snapshot1.time < snapshot2.time; });

    out_guard.Disable();
    return true;
}

class ListContext {
    rk_Disk *disk;
    rk_ListSettings settings;

    Async tasks;

public:
    ListContext(rk_Disk *disk, const rk_ListSettings &settings);

    bool RecurseEntries(Span<const uint8_t> entries, bool allow_separators, int depth,
                        Allocator *alloc, HeapArray<rk_ObjectInfo> *out_objects);

    bool Sync() { return tasks.Sync(); }
};

ListContext::ListContext(rk_Disk *disk, const rk_ListSettings &settings)
    : disk(disk), settings(settings), tasks(disk->GetThreads())
{
}

bool ListContext::RecurseEntries(Span<const uint8_t> entries, bool allow_separators, int depth,
                                 Allocator *alloc, HeapArray<rk_ObjectInfo> *out_objects)
{
    if (entries.len < RG_SIZE(int64_t)) [[unlikely]] {
        LogError("Malformed directory blob");
        return false;
    }
    entries.len -= RG_SIZE(int64_t);

    // Get total length from end of stream
    int64_t dir_len = 0;
    memcpy(&dir_len, entries.end(), RG_SIZE(dir_len));
    dir_len = LittleEndian(dir_len);

    HeapArray<EntryInfo> decoded;
    for (Size offset = 0; offset < entries.len;) {
        EntryInfo entry = {};

        Size skip = DecodeEntry(entries, offset, allow_separators, alloc, &entry);
        if (skip < 0)
            return false;
        offset += skip;

        decoded.Append(entry);
    }

    Async async(&tasks);

    struct RecurseContext {
        rk_ObjectInfo obj;
        HeapArray<rk_ObjectInfo> children;

        BlockAllocator str_alloc;
    };

    HeapArray<RecurseContext> contexts;
    contexts.AppendDefault(decoded.len);

    for (Size i = 0; i < decoded.len; i++) {
        const EntryInfo &entry = decoded[i];

        rk_ObjectInfo *obj = &contexts[i].obj;

        obj->hash = entry.hash;
        obj->depth = depth;
        switch (entry.kind) {
            case (int)rk_RawFile::Kind::Directory: { obj->type = rk_ObjectType::Directory; } break;
            case (int)rk_RawFile::Kind::File: { obj->type = rk_ObjectType::File; } break;
            case (int)rk_RawFile::Kind::Link: { obj->type = rk_ObjectType::Link; } break;
            case (int)rk_RawFile::Kind::Unknown: { obj->type = rk_ObjectType::Unknown; } break;

            default: { RG_UNREACHABLE(); } break;
        }
        obj->name = entry.basename;
        obj->mtime = entry.mtime;
        obj->btime = entry.btime;
        obj->mode = entry.mode;
        obj->uid = entry.uid;
        obj->gid = entry.gid;
        obj->size = entry.size;
        obj->readable = (entry.flags & (int)rk_RawFile::Flags::Readable);

        rk_BlobType expect_type;
        if (entry.kind == (int)rk_RawFile::Kind::Directory) {
            expect_type = rk_BlobType::Directory;
        } else if (entry.kind == (int)rk_RawFile::Kind::Link) {
            expect_type = rk_BlobType::Link;
        } else {
            continue;
        }

        async.Run([=, &contexts, this]() {
            rk_BlobType entry_type;
            HeapArray<uint8_t> entry_blob;

            if (!disk->ReadBlob(entry.hash, &entry_type, &entry_blob))
                return false;

            if (entry_type != expect_type) {
                LogError("Blob '%1' is not a %2", entry.hash, rk_BlobTypeNames[(int)expect_type]);
                return false;
            }

            switch (obj->type) {
                case rk_ObjectType::Snapshot: { RG_UNREACHABLE(); } break;

                case rk_ObjectType::Directory: {
                    if (settings.max_depth < 0 || depth < settings.max_depth) {
                        RecurseContext *ctx = &contexts[i];

                        if (!RecurseEntries(entry_blob, false, depth + 1, &ctx->str_alloc, &ctx->children))
                            return false;

                        for (const rk_ObjectInfo &child: ctx->children) {
                            obj->children += (child.depth == depth + 1);
                        }
                    }
                } break;
                case rk_ObjectType::File: {} break;
                case rk_ObjectType::Link: { obj->target = DuplicateString(entry_blob.As<const char>(), alloc).ptr; } break;
                case rk_ObjectType::Unknown: {} break;
            }

            return true;
        });
    }

    if (!async.Sync())
        return false;

    for (const RecurseContext &ctx: contexts) {
        out_objects->Append(ctx.obj);

        for (const rk_ObjectInfo &child: ctx.children) {
            rk_ObjectInfo *ptr = out_objects->Append(child);
            ptr->name = DuplicateString(ptr->name, alloc).ptr;
        }
    }

    return true;
}

bool rk_List(rk_Disk *disk, const rk_Hash &hash, const rk_ListSettings &settings,
             Allocator *alloc, HeapArray<rk_ObjectInfo> *out_objects)
{
    Size prev_len = out_objects->len;
    RG_DEFER_N(out_guard) { out_objects->RemoveFrom(prev_len); };

    rk_BlobType type;
    HeapArray<uint8_t> blob;
    if (!disk->ReadBlob(hash, &type, &blob))
        return false;

    ListContext tree(disk, settings);

    switch (type) {
        case rk_BlobType::Directory: {
            if (!tree.RecurseEntries(blob, false, 0, alloc, out_objects))
                return false;
        } break;

        case rk_BlobType::Snapshot: {
            // There must be at least one entry
            if (blob.len <= RG_SIZE(rk_SnapshotHeader)) {
                LogError("Malformed snapshot blob '%1'", hash);
                return false;
            }

            const rk_SnapshotHeader *header = (const rk_SnapshotHeader *)blob.ptr;
            rk_ObjectInfo *obj = out_objects->AppendDefault();

            obj->hash = hash;
            obj->type = rk_ObjectType::Snapshot;
            obj->name = header->name[0] ? DuplicateString(header->name, alloc).ptr : nullptr;
            obj->mtime = header->time;
            obj->btime = header->time;
            obj->size = header->len;
            obj->readable = true;

            obj->stored = header->stored;

            Span<uint8_t> entries = blob.Take(RG_SIZE(rk_SnapshotHeader), blob.len - RG_SIZE(rk_SnapshotHeader));

            if (!tree.RecurseEntries(entries, true, 1, alloc, out_objects))
                return false;

            // Reacquire correct pointer (array may have moved)
            obj = &(*out_objects)[prev_len];

            for (Size i = prev_len; i < out_objects->len; i++) {
                const rk_ObjectInfo &child = (*out_objects)[i];
                obj->children += (child.depth == 1);
            }
        } break;

        case rk_BlobType::Chunk:
        case rk_BlobType::File:
        case rk_BlobType::Link: {
            LogInfo("Expected Snapshot or Directory blob, not %1", rk_BlobTypeNames[(int)type]);
            return false;
        } break;
    }

    out_guard.Disable();
    return true;
}

const char *rk_ReadLink(rk_Disk *disk, const rk_Hash &hash, Allocator *alloc)
{
    rk_BlobType type;
    HeapArray<uint8_t> blob;
    if (!disk->ReadBlob(hash, &type, &blob))
        return nullptr;

    if (type != rk_BlobType::Link) {
        LogError("Expected symbolic link for '%1'", hash);
        return nullptr;
    }

    const char *target = DuplicateString(blob.As<const char>(), alloc).ptr;
    return target;
}

}
