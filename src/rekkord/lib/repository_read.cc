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
#include "disk.hh"
#include "repository.hh"
#include "repository_priv.hh"
#include "xattr.hh"

#if defined(_WIN32)
    #if !defined(NOMINMAX)
        #define NOMINMAX
    #endif
    #if !defined(WIN32_LEAN_AND_MEAN)
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <io.h>
#else
    #include <sys/stat.h>
    #include <sys/time.h>
#endif

namespace RG {

// Fix mess caused by windows.h
#if defined(CreateSymbolicLink)
    #undef CreateSymbolicLink
#endif

struct EntryInfo {
    rk_Hash hash;

    int kind;
    unsigned int flags;

    Span<const char> basename;
    Span<const char> filename;

    int64_t mtime;
    int64_t ctime;
    int64_t atime;
    int64_t btime;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    int64_t size;

    Span<XAttrInfo> xattrs;
};

struct FileChunk {
    int64_t offset;
    int64_t len;
    rk_Hash hash;
};

class GetContext {
    rk_Disk *disk;
    rk_GetSettings settings;

    ProgressHandle *progress;

    int64_t total_size = 0;
    std::atomic_int64_t restored_size = 0;

    Async tasks;

    std::atomic<int64_t> stat_size { 0 };

public:
    GetContext(rk_Disk *disk, const rk_GetSettings &settings, ProgressHandle *progress, int64_t total);

    bool ExtractEntries(Span<const uint8_t> entries, bool allow_separators, const char *dest_dirname);
    bool ExtractEntries(Span<const uint8_t> entries, bool allow_separators, const EntryInfo &dest);

    int GetFile(const rk_Hash &hash, rk_BlobType type, Span<const uint8_t> file_blob, const char *dest_filename);

    bool Sync() { return tasks.Sync(); }

    int64_t GetSize() const { return stat_size; }

private:
    bool CleanDirectory(Span<const char> dirname, const HashSet<Span<const char>> &keep);

    void MakeProgress(int64_t delta);
};

GetContext::GetContext(rk_Disk *disk, const rk_GetSettings &settings, ProgressHandle *progress, int64_t total)
    : disk(disk), settings(settings), progress(progress), total_size(total), tasks(disk->GetAsync())
{
}

#if defined(_WIN32)

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

static void SetFileOwner(int, const char *, uint32_t, uint32_t)
{
}

#else

static bool WriteAt(int fd, const char *filename, int64_t offset, Span<const uint8_t> buf)
{
    while (buf.len) {
        Size written = RG_RESTART_EINTR(pwrite(fd, buf.ptr, buf.len, (off_t)offset), < 0);

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

#endif

static void MigrateLegacyEntries1(HeapArray<uint8_t> *blob, Size start)
{
    if (blob->len < RG_SIZE(int64_t))
        return;

    blob->Grow(RG_SIZE(DirectoryHeader));

    MemMove(blob->ptr + start + RG_SIZE(DirectoryHeader), blob->ptr + start, blob->len);
    blob->len += RG_SIZE(DirectoryHeader) - RG_SIZE(int64_t);

    DirectoryHeader *header = (DirectoryHeader *)(blob->ptr + start);

    MemCpy(&header->size, blob->end(), RG_SIZE(int64_t));
    header->entries = 0;
}

static void MigrateLegacyEntries2(HeapArray<uint8_t> *blob, Size start)
{
    HeapArray<uint8_t> entries;
    Size offset = start + RG_SIZE(DirectoryHeader);

    while (offset < blob->len) {
        RawFile *ptr = (RawFile *)(blob->ptr + offset);
        Size skip = ptr->GetSize() - 16;

        if (blob->len - offset < skip)
            break;

        entries.Grow(ptr->GetSize());
        MemCpy(entries.end(), blob->ptr + offset, skip);
        MemMove(entries.end() + offsetof(RawFile, btime), entries.end() + offsetof(RawFile, ctime), skip - offsetof(RawFile, ctime));
        MemSet(entries.end() + offsetof(RawFile, atime), 0, RG_SIZE(RawFile::atime));
        entries.len += ptr->GetSize();

        offset += skip;
    }

    blob->RemoveFrom(start + RG_SIZE(DirectoryHeader));
    blob->Append(entries);
}

// Does not fill EntryInfo::filename
static Size DecodeEntry(Span<const uint8_t> entries, Size offset, bool allow_separators,
                        Allocator *alloc, EntryInfo *out_entry)
{
    RawFile *ptr = (RawFile *)(entries.ptr + offset);

    if (entries.len - offset < ptr->GetSize()) {
        LogError("Malformed entry in directory blob");
        return -1;
    }

    EntryInfo entry = {};

    entry.hash = ptr->hash;
    entry.kind = LittleEndian(ptr->kind);
    entry.flags = LittleEndian(ptr->flags);
    entry.basename = DuplicateString(ptr->GetName(), alloc);

#if defined(_WIN32)
    if (allow_separators) {
        Span<char> basename = entry.basename.As<char>();

        for (char &c: basename) {
            c = (c == '\\') ? '/' : c;
        }
    }
#endif

    entry.mtime = LittleEndian(ptr->mtime);
    entry.ctime = LittleEndian(ptr->ctime);
    entry.atime = LittleEndian(ptr->atime);
    entry.btime = LittleEndian(ptr->btime);
    entry.mode = LittleEndian(ptr->mode);
    entry.uid = LittleEndian(ptr->uid);
    entry.gid = LittleEndian(ptr->gid);
    entry.size = LittleEndian(ptr->size);

    if (ptr->extended_len) {
        Span<const uint8_t> extended = ptr->GetExtended();

        // Count ahead of time to avoid reallocations
        Size count = 0;

        for (Size offset = 0; offset < extended.len;) {
            if (extended.len - offset < RG_SIZE(uint16_t)) {
                LogError("Truncated extended blob");
                return -1;
            }

            uint16_t attr_len;
            MemCpy(&attr_len, extended.ptr + offset, RG_SIZE(attr_len));
            attr_len = LittleEndian(attr_len);

            if (attr_len > extended.len) {
                LogError("Invalid extended length prefix");
                return -1;
            }

            count++;
            offset += 2 + attr_len;
        }

        HeapArray<XAttrInfo> xattrs(alloc);
        xattrs.Reserve(count);

        for (Size offset = 0; offset < extended.len;) {
            XAttrInfo *xattr = xattrs.AppendDefault();

            uint16_t attr_len;
            MemCpy(&attr_len, extended.ptr + offset, RG_SIZE(attr_len));
            attr_len = LittleEndian(attr_len);

            Span<const uint8_t> attr = MakeSpan(extended.ptr + offset + 2, attr_len);
            const uint8_t *split = (const uint8_t *)memchr(attr.ptr, 0, (size_t)attr.len);

            if (!split) {
                LogError("Invalid extended length prefix");
                return -1;
            }

            Size key_len = split - attr.ptr;
            Size value_len = attr.len - key_len - 1;

            xattr->key = DuplicateString(MakeSpan((const char *)attr.ptr, key_len), alloc).ptr;
            xattr->value = AllocateSpan<uint8_t>(alloc, value_len);
            MemCpy(xattr->value.ptr, attr.end() - value_len, value_len);

            offset += 2 + attr_len;
        }

        entry.xattrs = xattrs.Leak();
    }

    // Sanity checks
    if (entry.kind != (int8_t)RawFile::Kind::Directory &&
            entry.kind != (int8_t)RawFile::Kind::File &&
            entry.kind != (int8_t)RawFile::Kind::Link &&
            entry.kind != (int8_t)RawFile::Kind::Unknown) {
        LogError("Unknown object kind 0x%1", FmtHex((unsigned int)entry.kind));
        return -1;
    }
    if (!entry.basename.len || PathContainsDotDot(entry.basename.ptr)) {
        LogError("Unsafe object name '%1'", entry.basename);
        return -1;
    }
    if (PathIsAbsolute(entry.basename.ptr)) {
        LogError("Unsafe object name '%1'", entry.basename);
        return -1;
    }
    if (!allow_separators && strpbrk(entry.basename.ptr, RG_PATH_SEPARATORS)) {
        LogError("Unsafe object name '%1'", entry.basename);
        return -1;
    }

    *out_entry = entry;
    return ptr->GetSize();
}

bool GetContext::ExtractEntries(Span<const uint8_t> entries, bool allow_separators, const char *dest_dirname)
{
    EntryInfo dest = {};

    if (!dest_dirname[0]) {
        dest_dirname = ".";
    }
    dest.filename = TrimStrRight(dest_dirname, RG_PATH_SEPARATORS);

    return ExtractEntries(entries, allow_separators, dest);
}

bool GetContext::ExtractEntries(Span<const uint8_t> entries, bool allow_separators, const EntryInfo &dest)
{
    // XXX: Make sure each path does not clobber a previous one

    if (entries.len < RG_SIZE(DirectoryHeader)) [[unlikely]] {
        LogError("Malformed directory blob");
        return false;
    }

    struct SharedContext {
        BlockAllocator temp_alloc;

        EntryInfo meta = {};
        bool chown = false;
        bool fake = false;

        HeapArray<EntryInfo> entries;

        ~SharedContext() {
            if (!fake && meta.filename.len) {
                int fd = OpenFile(meta.filename.ptr, (int)OpenFlag::Write | (int)OpenFlag::Directory);
                if (fd < 0)
                    return;
                RG_DEFER { CloseDescriptor(fd); };

                // Set directory metadata
                if (chown) {
                    SetFileOwner(fd, meta.filename.ptr, meta.uid, meta.gid);
                }
                SetFileMetaData(fd, meta.filename.ptr, meta.mtime, meta.btime, meta.mode);

                WriteXAttributes(fd, meta.filename.ptr, meta.xattrs);
            }
        }
    };

    std::shared_ptr<SharedContext> ctx = std::make_shared<SharedContext>();

    if (dest.basename.len) {
        ctx->meta = dest;
        ctx->meta.filename = DuplicateString(dest.filename, &ctx->temp_alloc);

        if (ctx->meta.xattrs.len) {
            Span<XAttrInfo> xattrs = ctx->meta.xattrs;
            ctx->meta.xattrs = AllocateSpan<XAttrInfo>(&ctx->temp_alloc, xattrs.len);
            MemCpy(ctx->meta.xattrs.ptr, xattrs.ptr, xattrs.len * RG_SIZE(XAttrInfo));
        }

        ctx->chown = settings.restore_owner;
        ctx->fake = settings.fake;
    }

    for (Size offset = RG_SIZE(DirectoryHeader); offset < entries.len;) {
        EntryInfo entry = {};

        Size skip = DecodeEntry(entries, offset, allow_separators, &ctx->temp_alloc, &entry);
        if (skip < 0)
            return false;
        offset += skip;

        if (entry.kind == (int)RawFile::Kind::Unknown)
            continue;
        if (!(entry.flags & (int)RawFile::Flags::Readable))
            continue;

        entry.filename = Fmt(&ctx->temp_alloc, "%1%/%2", dest.filename, entry.basename).ptr;

        if (!settings.fake && allow_separators && !EnsureDirectoryExists(entry.filename.ptr))
            return false;

        ctx->entries.Append(entry);
    }

    if (settings.unlink) {
        HashSet<Span<const char>> keep;

        for (const EntryInfo &entry: ctx->entries) {
            Span<const char> path = entry.filename;
            keep.Set(path);

            if (allow_separators) {
                SplitStrReverse(path, *RG_PATH_SEPARATORS, &path);

                while (path.len > dest.filename.len) {
                    keep.Set(path);
                    SplitStrReverse(path, *RG_PATH_SEPARATORS, &path);
                }
            }
        }

        if (!CleanDirectory(dest.filename, keep))
            return false;

        if (allow_separators) {
            for (const EntryInfo &entry: ctx->entries) {
                Span<const char> path = entry.filename;
                SplitStrReverse(path, *RG_PATH_SEPARATORS, &path);

                while (path.len > dest.filename.len) {
                    if (!CleanDirectory(path, keep))
                        return false;
                    SplitStrReverse(path, *RG_PATH_SEPARATORS, &path);
                }
            }
        }
    }

    for (const EntryInfo &entry: ctx->entries) {
        tasks.Run([=, ctx = ctx, this] () {
            rk_BlobType entry_type;
            HeapArray<uint8_t> entry_blob;
            if (!disk->ReadBlob(entry.hash, &entry_type, &entry_blob))
                return false;

            switch (entry.kind) {
                case (int)RawFile::Kind::Directory: {
                    if (entry_type != rk_BlobType::Directory1 &&
                            entry_type != rk_BlobType::Directory2 &&
                            entry_type != rk_BlobType::Directory3) {
                        LogError("Blob '%1' is not a Directory", entry.hash);
                        return false;
                    }

                    if (settings.verbose) {
                        Span<const char> prefix = entry.filename.Take(0, entry.filename.len - entry.basename.len - 1);
                        LogInfo("%!D..[D]%!0 %1%/%!..+%2%/%!0", prefix, entry.basename);
                    }

                    switch (entry_type) {
                        case rk_BlobType::Directory1: { MigrateLegacyEntries1(&entry_blob, 0); } [[fallthrough]];
                        case rk_BlobType::Directory2: { MigrateLegacyEntries2(&entry_blob, 0); } [[fallthrough]];
                        default: {} break;
                    }

                    if (!settings.fake && !MakeDirectory(entry.filename.ptr, false))
                        return false;
                    if (!ExtractEntries(entry_blob, false, entry))
                        return false;
                } break;

                case (int)RawFile::Kind::File: {
                    if (entry_type != rk_BlobType::File && entry_type != rk_BlobType::Chunk) {
                        LogError("Blob '%1' is not a File", entry.hash);
                        return false;
                    }

                    if (settings.verbose) {
                        Span<const char> prefix = entry.filename.Take(0, entry.filename.len - entry.basename.len - 1);
                        LogInfo("%!D..[F]%!0 %1%/%!..+%2%!0", prefix, entry.basename);
                    }

                    if (settings.fake) {
                        stat_size += entry.size;
                        break;
                    }

                    int fd = GetFile(entry.hash, entry_type, entry_blob, entry.filename.ptr);
                    if (fd < 0)
                        return false;
                    RG_DEFER { CloseDescriptor(fd); };

                    // Set file metadata
                    if (settings.restore_owner) {
                        SetFileOwner(fd, entry.filename.ptr, entry.uid, entry.gid);
                    }
                    SetFileMetaData(fd, entry.filename.ptr, entry.mtime, entry.btime, entry.mode);

                    WriteXAttributes(fd, entry.filename.ptr, entry.xattrs);
                } break;

                case (int)RawFile::Kind::Link: {
                    if (entry_type != rk_BlobType::Link) {
                        LogError("Blob '%1' is not a Link", entry.hash);
                        return false;
                    }

                    // NUL terminate the path
                    entry_blob.Append(0);

                    if (settings.verbose) {
                        Span<const char> prefix = entry.filename.Take(0, entry.filename.len - entry.basename.len - 1);
                        LogInfo("%!D..[L]%!0 %1%/%!..+%2%!0", prefix, entry.basename);
                    }

                    if (!settings.fake) {
                        if (!CreateSymbolicLink(entry.filename.ptr, (const char *)entry_blob.ptr, settings.force))
                            return false;

                        if (entry.xattrs.len) {
                            int fd = OpenFile(entry.filename.ptr, (int)OpenFlag::Write);
                            RG_DEFER { CloseDescriptor(fd); };

                            if (fd >= 0) {
                                WriteXAttributes(fd, entry.filename.ptr, entry.xattrs);
                            }
                        }
                    }
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

    // Use StreamWriter machinery to do atomic replace, even through we do not write through it
    // and want to keep the descriptor at the end.
    StreamWriter writer;

    if (!settings.fake) {
        if (!writer.Open(dest_filename, (int)StreamWriterFlag::Atomic))
            return -1;
        writer.SetDescriptorOwned(false);
    }

    int fd = !settings.fake ? writer.GetDescriptor() : -1;
    RG_DEFER_N(err_guard) { CloseDescriptor(fd); };

    int64_t file_size = 0;

    switch (type) {
        case rk_BlobType::File: {
            if (file_blob.len % RG_SIZE(RawChunk) != RG_SIZE(int64_t)) {
                LogError("Malformed file blob '%1'", hash);
                return -1;
            }
            file_blob.len -= RG_SIZE(int64_t);

            // Get file length from end of stream
            MemCpy(&file_size, file_blob.end(), RG_SIZE(file_size));
            file_size = LittleEndian(file_size);

            if (file_size < 0) {
                LogError("Malformed file blob '%1'", hash);
                return -1;
            }
            if (settings.fake)
                break;

            if (!ResizeFile(fd, dest_filename, file_size))
                return -1;

            Async async(&tasks);

            // Check coherence
            Size prev_end = 0;

            // Write unencrypted file
            for (Size offset = 0; offset < file_blob.len; offset += RG_SIZE(RawChunk)) {
                FileChunk chunk = {};

                RawChunk entry = {};
                MemCpy(&entry, file_blob.ptr + offset, RG_SIZE(entry));

                chunk.offset = LittleEndian(entry.offset);
                chunk.len = LittleEndian(entry.len);
                chunk.hash = entry.hash;

                if (prev_end > chunk.offset || chunk.len < 0) [[unlikely]] {
                    LogError("Malformed file blob '%1'", hash);
                    return false;
                }
                prev_end = chunk.offset + chunk.len;

                async.Run([=, this]() {
                    rk_BlobType type;
                    HeapArray<uint8_t> buf;
                    if (!disk->ReadBlob(chunk.hash, &type, &buf))
                        return false;

                    if (type != rk_BlobType::Chunk) [[unlikely]] {
                        LogError("Blob '%1' is not a Chunk", chunk.hash);
                        return false;
                    }
                    if (buf.len != chunk.len) [[unlikely]] {
                        LogError("Chunk size mismatch for '%1'", chunk.hash);
                        return false;
                    }
                    if (!WriteAt(fd, dest_filename, chunk.offset, buf)) {
                        LogError("Failed to write to '%1': %2", dest_filename, strerror(errno));
                        return false;
                    }

                    MakeProgress(chunk.len);

                    return true;
                });
            }

            // Only process tasks for this Async, a standard Sync would run other tasks (such as other file tasks)
            // which could take a while and could provoke an accumulation of unfinished file tasks with many open
            // file descriptors and the appearence of slow progress.
            if (!async.SyncSoon())
                return -1;

            // Check actual file size
            if (file_blob.len >= RG_SIZE(RawChunk) + RG_SIZE(int64_t)) {
                const RawChunk *entry = (const RawChunk *)(file_blob.end() - RG_SIZE(RawChunk));
                int64_t size = LittleEndian(entry->offset) + LittleEndian(entry->len);

                if (size != file_size) [[unlikely]] {
                    LogError("File size mismatch for '%1'", entry->hash);
                    return -1;
                }
            }
        } break;

        case rk_BlobType::Chunk: {
            file_size = file_blob.len;

            if (settings.fake)
                break;

            if (!WriteAt(fd, dest_filename, 0, file_blob)) {
                LogError("Failed to write to '%1': %2", dest_filename, strerror(errno));
                return -1;
            }
        } break;

        case rk_BlobType::Directory1:
        case rk_BlobType::Directory2:
        case rk_BlobType::Directory3:
        case rk_BlobType::Snapshot1:
        case rk_BlobType::Snapshot2:
        case rk_BlobType::Snapshot3:
        case rk_BlobType::Snapshot4:
        case rk_BlobType::Link: { RG_UNREACHABLE(); } break;
    }

    if (!settings.fake && !writer.Close())
        return -1;

    // Finally :)
    stat_size += file_size;

    err_guard.Disable();
    return fd;
}

bool GetContext::CleanDirectory(Span<const char> dirname, const HashSet<Span<const char>> &keep)
{
    BlockAllocator temp_alloc;

    std::function<bool(const char *)> clean_directory = [&](const char *dirname) {
        EnumResult ret = EnumerateDirectory(dirname, nullptr, -1, [&](const char *basename, const FileInfo &file_info) {
            const char *filename = Fmt(&temp_alloc, "%1%/%2", dirname, basename).ptr;

            if (keep.Find(filename))
                return true;

            if (file_info.type == FileType::Directory) {
                if (!clean_directory(filename))
                    return false;

                if (settings.verbose) {
                    LogInfo("Delete directory '%1'", filename);
                }
                if (settings.fake)
                    return true;

                return UnlinkDirectory(filename);
            } else {
                if (settings.verbose) {
                    LogInfo("Delete file '%1'", filename);
                }
                if (settings.fake)
                    return true;

                return UnlinkFile(filename);
            }
        });
        if (ret != EnumResult::Success)
            return false;

        return true;
    };

    const char *copy = DuplicateString(dirname, &temp_alloc).ptr;
    return clean_directory(copy);
}

void GetContext::MakeProgress(int64_t delta)
{
    int64_t restored = restored_size.fetch_add(delta, std::memory_order_relaxed) + delta;

    if (!settings.verbose) {
        progress->SetFmt(restored, total_size, "%1 / %2", FmtDiskSize(restored), FmtDiskSize(total_size));
    }
}

bool rk_Get(rk_Disk *disk, const rk_Hash &hash, const rk_GetSettings &settings, const char *dest_path, int64_t *out_size)
{
    rk_BlobType type;
    HeapArray<uint8_t> blob;
    if (!disk->ReadBlob(hash, &type, &blob))
        return false;

    switch (type) {
        case rk_BlobType::Chunk:
        case rk_BlobType::File: {
            if (!settings.force) {
                if (TestFile(dest_path) && !IsDirectoryEmpty(dest_path)) {
                    LogError("File '%1' already exists", dest_path);
                    return false;
                }
            }

            int64_t file_size = 0;

            if (type == rk_BlobType::File) {
                if (blob.len < RG_SIZE(int64_t)) {
                    LogError("Malformed file blob '%1'", hash);
                    return false;
                }

                MemCpy(&file_size, blob.end() - RG_SIZE(int64_t), RG_SIZE(int64_t));
                file_size = LittleEndian(file_size);
            }

            if (settings.verbose) {
                LogInfo("Restore file %!..+%1%!0", hash);
            }

            ProgressHandle progress("Restore");
            GetContext get(disk, settings, &progress, file_size);

            int fd = get.GetFile(hash, type, blob, dest_path);
            if (!settings.fake && fd < 0)
                return false;
            CloseDescriptor(fd);

            if (out_size) {
                *out_size += get.GetSize();
            }
        } break;

        case rk_BlobType::Directory1: { MigrateLegacyEntries1(&blob, 0); } [[fallthrough]];
        case rk_BlobType::Directory2: { MigrateLegacyEntries2(&blob, 0); } [[fallthrough]];
        case rk_BlobType::Directory3: {
            if (!settings.fake) {
                if (!settings.force && TestFile(dest_path, FileType::Directory)) {
                    if (!IsDirectoryEmpty(dest_path)) {
                        LogError("Directory '%1' exists and is not empty", dest_path);
                        return false;
                    }
                } else {
                    if (!MakeDirectory(dest_path, !settings.force))
                        return false;
                }
            }

            if (blob.len < RG_SIZE(DirectoryHeader)) {
                LogError("Malformed directory blob '%1'", hash);
                return false;
            }

            DirectoryHeader *header = (DirectoryHeader *)blob.ptr;
            int64_t total = LittleEndian(header->size);

            if (settings.verbose) {
                LogInfo("Restore directory %!..+%1%!0", hash);
            }

            ProgressHandle progress("Restore");
            GetContext get(disk, settings, &progress, total);

            if (!get.ExtractEntries(blob, false, dest_path))
                return false;
            if (!get.Sync())
                return false;

            if (out_size) {
                *out_size += get.GetSize();
            }
        } break;

        case rk_BlobType::Snapshot1: { static_assert(RG_SIZE(SnapshotHeader1) == RG_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case rk_BlobType::Snapshot2: { MigrateLegacyEntries1(&blob, RG_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case rk_BlobType::Snapshot3: { MigrateLegacyEntries2(&blob, RG_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case rk_BlobType::Snapshot4: {
            if (!settings.fake) {
                if (!settings.force && TestFile(dest_path, FileType::Directory)) {
                    if (!IsDirectoryEmpty(dest_path)) {
                        LogError("Directory '%1' exists and is not empty", dest_path);
                        return false;
                    }
                } else {
                    if (!MakeDirectory(dest_path, !settings.force))
                        return false;
                }
            }

            // There must be at least one entry
            if (blob.len <= RG_SIZE(SnapshotHeader2) + RG_SIZE(DirectoryHeader)) {
                LogError("Malformed snapshot blob '%1'", hash);
                return false;
            }

            DirectoryHeader *header = (DirectoryHeader *)(blob.ptr + RG_SIZE(SnapshotHeader2));
            int64_t total = LittleEndian(header->size);

            if (settings.verbose) {
                LogInfo("Restore snapshot %!..+%1%!0", hash);
            }

            ProgressHandle progress("Restore");
            GetContext get(disk, settings, &progress, total);

            Span<uint8_t> entries = blob.Take(RG_SIZE(SnapshotHeader2), blob.len - RG_SIZE(SnapshotHeader2));

            if (!get.ExtractEntries(entries, true, dest_path))
                return false;
            if (!get.Sync())
                return false;

            if (out_size) {
                *out_size += get.GetSize();
            }
        } break;

        case rk_BlobType::Link: {
            blob.Append(0);

            if (settings.verbose) {
                LogInfo("Restore symbolic link '%1' to '%2'", hash, dest_path);
            }
            if (settings.fake)
                break;

            if (!CreateSymbolicLink(dest_path, (const char *)blob.ptr, settings.force))
                return false;
        } break;
    }

    return true;
}

bool rk_Snapshots(rk_Disk *disk, Allocator *alloc, HeapArray<rk_SnapshotInfo> *out_snapshots)
{
    BlockAllocator temp_alloc;

    Size prev_len = out_snapshots->len;
    RG_DEFER_N(out_guard) { out_snapshots->RemoveFrom(prev_len); };

    HeapArray<rk_TagInfo> tags;
    if (!disk->ListTags(&temp_alloc, &tags))
        return false;

    for (const rk_TagInfo &tag: tags) {
        rk_SnapshotInfo snapshot = {};

        if (tag.payload.len < (Size)offsetof(SnapshotHeader2, channel) + 1 ||
                tag.payload.len > RG_SIZE(SnapshotHeader2)) {
            LogError("Malformed snapshot tag (ignoring)");
            continue;
        }

        SnapshotHeader2 header = {};
        MemCpy(&header, tag.payload.ptr, tag.payload.len);
        header.channel[RG_SIZE(header.channel) - 1] = 0;

        snapshot.tag = DuplicateString(tag.id, alloc).ptr;
        snapshot.hash = tag.hash;
        snapshot.channel = DuplicateString(header.channel, alloc).ptr;
        snapshot.time = LittleEndian(header.time);
        snapshot.size = LittleEndian(header.size);
        snapshot.storage = LittleEndian(header.storage);

        out_snapshots->Append(snapshot);
    }

    std::sort(out_snapshots->ptr + prev_len, out_snapshots->end(),
              [](const rk_SnapshotInfo &snapshot1, const rk_SnapshotInfo &snapshot2) { return snapshot1.time < snapshot2.time; });

    out_guard.Disable();
    return true;
}

bool rk_Channels(rk_Disk *disk, Allocator *alloc, HeapArray<rk_ChannelInfo> *out_channels)
{
    BlockAllocator temp_alloc;

    Size prev_len = out_channels->len;
    RG_DEFER_N(out_guard) { out_channels->RemoveFrom(prev_len); };

    HeapArray<rk_SnapshotInfo> snapshots;
    if (!rk_Snapshots(disk, &temp_alloc, &snapshots))
        return false;

    HashMap<const char *, Size> map;

    for (const rk_SnapshotInfo &snapshot: snapshots) {
        Size *ptr = map.TrySet(snapshot.channel, -1);
        Size idx = *ptr;

        if (idx < 0) {
            rk_ChannelInfo channel = {};

            channel.name = DuplicateString(snapshot.channel, alloc).ptr;

            idx = out_channels->len;
            *ptr = idx;

            out_channels->Append(channel);
        }

        rk_ChannelInfo *channel = &(*out_channels)[idx];

        if (snapshot.time > channel->time) {
            channel->hash = snapshot.hash;
            channel->time = std::max(channel->time, snapshot.time);
            channel->size = snapshot.size;
        }

        channel->count++;
    }

    std::sort(out_channels->ptr + prev_len, out_channels->end(),
              [](const rk_ChannelInfo &channel1, const rk_ChannelInfo &channel2) { return CmpStr(channel1.name, channel2.name) < 0; });

    out_guard.Disable();
    return true;
}

class ListContext {
    rk_Disk *disk;
    rk_ListSettings settings;

    ProgressHandle *progress;

    int64_t total_entries = 0;
    std::atomic_int64_t known_entries = 0;

public:
    ListContext(rk_Disk *disk, const rk_ListSettings &settings, ProgressHandle *progress, int64_t total);

    bool RecurseEntries(Span<const uint8_t> entries, bool allow_separators, int depth,
                        Allocator *alloc, HeapArray<rk_ObjectInfo> *out_objects);

private:
    void MakeProgress(int64_t delta);
};

ListContext::ListContext(rk_Disk *disk, const rk_ListSettings &settings, ProgressHandle *progress, int64_t total)
    : disk(disk), settings(settings), progress(progress), total_entries(total)
{
}

bool ListContext::RecurseEntries(Span<const uint8_t> entries, bool allow_separators, int depth,
                                 Allocator *alloc, HeapArray<rk_ObjectInfo> *out_objects)
{
    if (entries.len < RG_SIZE(DirectoryHeader)) [[unlikely]] {
        LogError("Malformed directory blob");
        return false;
    }

    HeapArray<EntryInfo> decoded;
    for (Size offset = RG_SIZE(DirectoryHeader); offset < entries.len;) {
        EntryInfo entry = {};

        Size skip = DecodeEntry(entries, offset, allow_separators, alloc, &entry);
        if (skip < 0)
            return false;
        offset += skip;

        decoded.Append(entry);
    }

    Async async(disk->GetAsync());

    struct RecurseContext {
        rk_ObjectInfo obj;
        HeapArray<rk_ObjectInfo> children;

        BlockAllocator str_alloc;
    };

    HeapArray<RecurseContext> contexts;
    contexts.AppendDefault(decoded.len);

    MakeProgress(0);

    for (Size i = 0; i < decoded.len; i++) {
        const EntryInfo &entry = decoded[i];

        rk_ObjectInfo *obj = &contexts[i].obj;

        obj->hash = entry.hash;
        obj->depth = depth;
        switch (entry.kind) {
            case (int)RawFile::Kind::Directory: { obj->type = rk_ObjectType::Directory; } break;
            case (int)RawFile::Kind::File: { obj->type = rk_ObjectType::File; } break;
            case (int)RawFile::Kind::Link: { obj->type = rk_ObjectType::Link; } break;
            case (int)RawFile::Kind::Unknown: { obj->type = rk_ObjectType::Unknown; } break;

            default: { RG_UNREACHABLE(); } break;
        }
        obj->name = entry.basename.ptr;
        obj->mtime = entry.mtime;
        obj->ctime = entry.ctime;
        obj->atime = entry.atime;
        obj->btime = entry.btime;
        obj->mode = entry.mode;
        obj->uid = entry.uid;
        obj->gid = entry.gid;
        obj->size = entry.size;
        obj->readable = (entry.flags & (int)RawFile::Flags::Readable);

        if (!(entry.flags & (int)RawFile::Flags::Readable))
            continue;

        switch (obj->type) {
            case rk_ObjectType::Snapshot: { RG_UNREACHABLE(); } break;

            case rk_ObjectType::Directory: {
                if (!settings.recurse)
                    break;

                async.Run([=, &contexts, this]() {
                    RecurseContext *ctx = &contexts[i];

                    rk_BlobType entry_type;
                    HeapArray<uint8_t> entry_blob;

                    if (!disk->ReadBlob(entry.hash, &entry_type, &entry_blob))
                        return false;

                    if (entry_type != rk_BlobType::Directory1 &&
                            entry_type != rk_BlobType::Directory2 &&
                            entry_type != rk_BlobType::Directory3) {
                        LogError("Blob '%1' is not a Directory", entry.hash);
                        return false;
                    }

                    switch (entry_type) {
                        case rk_BlobType::Directory1: { MigrateLegacyEntries1(&entry_blob, 0); } [[fallthrough]];
                        case rk_BlobType::Directory2: { MigrateLegacyEntries2(&entry_blob, 0); } [[fallthrough]];
                        default: {} break;
                    }

                    if (!RecurseEntries(entry_blob, false, depth + 1, &ctx->str_alloc, &ctx->children))
                        return false;

                    for (const rk_ObjectInfo &child: ctx->children) {
                        obj->children += (child.depth == depth + 1);
                    }

                    MakeProgress(1);

                    return true;
                });
            } break;

            case rk_ObjectType::File:
            case rk_ObjectType::Link:
            case rk_ObjectType::Unknown: {
                MakeProgress(1);
            } break;
        }
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

void ListContext::MakeProgress(int64_t delta)
{
    int64_t known = known_entries.fetch_add(delta, std::memory_order_relaxed) + delta;

    if (total_entries) {
        progress->SetFmt(known, total_entries, "%1 / %2 entries", known, total_entries);
    } else {
        progress->SetFmt(known, total_entries, "%1 entries", known);
    }
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

    switch (type) {
        case rk_BlobType::Directory1: { MigrateLegacyEntries1(&blob, 0); } [[fallthrough]];
        case rk_BlobType::Directory2: { MigrateLegacyEntries2(&blob, 0); } [[fallthrough]];
        case rk_BlobType::Directory3: {
            if (blob.len < RG_SIZE(DirectoryHeader)) {
                LogError("Malformed directory blob '%1'", hash);
                return false;
            }

            DirectoryHeader *header = (DirectoryHeader *)blob.ptr;
            int64_t total = LittleEndian(header->entries);

            ProgressHandle progress;
            ListContext tree(disk, settings, &progress, total);

            if (!tree.RecurseEntries(blob, false, 0, alloc, out_objects))
                return false;
        } break;

        case rk_BlobType::Snapshot1: {
            static_assert(RG_SIZE(SnapshotHeader1) == RG_SIZE(SnapshotHeader2));

            if (blob.len <= RG_SIZE(SnapshotHeader1)) {
                LogError("Malformed snapshot blob '%1'", hash);
                return false;
            }

            SnapshotHeader1 *header1 = (SnapshotHeader1 *)blob.ptr;
            SnapshotHeader2 header2 = {};

            header2.time = header1->time;
            header2.size = header1->size;
            header2.storage = header1->storage;
            MemCpy(header2.channel, header1->channel, RG_SIZE(header2.channel));

            MemCpy(blob.ptr, &header2, RG_SIZE(SnapshotHeader2));
        } [[fallthrough]];
        case rk_BlobType::Snapshot2: { MigrateLegacyEntries1(&blob, RG_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case rk_BlobType::Snapshot3: { MigrateLegacyEntries2(&blob, RG_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case rk_BlobType::Snapshot4: {
            if (blob.len < RG_SIZE(SnapshotHeader2) + RG_SIZE(DirectoryHeader)) {
                LogError("Malformed snapshot blob '%1'", hash);
                return false;
            }

            SnapshotHeader2 *header1 = (SnapshotHeader2 *)blob.ptr;
            DirectoryHeader *header2 = (DirectoryHeader *)(header1 + 1);
            int64_t total = LittleEndian(header2->entries);

            ProgressHandle progress;
            ListContext tree(disk, settings, &progress, total);

            // Make sure snapshot channel is NUL terminated
            header1->channel[RG_SIZE(header1->channel) - 1] = 0;

            rk_ObjectInfo *obj = out_objects->AppendDefault();

            obj->hash = hash;
            obj->type = rk_ObjectType::Snapshot;
            obj->name = DuplicateString(header1->channel, alloc).ptr;
            obj->mtime = LittleEndian(header1->time);
            obj->size = LittleEndian(header1->size);
            obj->readable = true;

            obj->storage = LittleEndian(header1->storage);

            Span<uint8_t> entries = blob.Take(RG_SIZE(SnapshotHeader2), blob.len - RG_SIZE(SnapshotHeader2));

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

static inline int ParseHexadecimalChar(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else {
        return -1;
    }
}

static bool ParseHash(Span<const char> str, rk_Hash *out_hash)
{
    for (Size i = 1, j = 0; i < str.len; i += 2, j++) {
        int high = ParseHexadecimalChar(str[i - 1]);
        int low = ParseHexadecimalChar(str[i]);

        if (high < 0 || low < 0)
            return false;

        out_hash->hash[j] = (uint8_t)((high << 4) | low);
    }

    return true;
}

bool rk_Locate(rk_Disk *disk, Span<const char> identifier, rk_Hash *out_hash)
{
    BlockAllocator temp_alloc;

    Span<const char> path;
    Span<const char> name = TrimStrRight(SplitStr(identifier, ':', &path), "/");
    bool has_path = (path.ptr > name.end());

    rk_Hash hash;
    {
        bool found = ParseHash(name, &hash);

        if (!found) {
            HeapArray<rk_SnapshotInfo> snapshots;
            if (!rk_Snapshots(disk, &temp_alloc, &snapshots))
                return false;

            for (Size i = snapshots.len - 1; i >= 0; i--) {
                const rk_SnapshotInfo &snapshot = snapshots[i];

                if (TestStr(name, snapshot.channel)) {
                    hash = snapshot.hash;

                    found = true;
                    break;
                }
            }

            if (!found)
                goto missing;
        }
    }

    // Traverse subpath (if any)
    if (has_path) {
        path = TrimStrRight(path, "/");

        // Reuse for performance
        HeapArray<rk_ObjectInfo> objects;

        do {
            objects.RemoveFrom(0);

            if (!rk_List(disk, hash, {}, &temp_alloc, &objects))
                return false;

            bool match = false;

            for (const rk_ObjectInfo &obj: objects) {
                Span<const char> name = obj.name;

                if (obj.type == rk_ObjectType::Snapshot)
                    continue;
                if (!StartsWith(path, name))
                    continue;
                if (path.len > name.len && path[name.len] != '/')
                    continue;

                path = TrimStrLeft(path.Take(name.len, path.len - name.len), "/");
                hash = obj.hash;

                match = true;
                break;
            }

            if (!match)
                goto missing;
        } while (path.len);
    }

    *out_hash = hash;
    return true;

missing:
    LogError("Cannot find object '%1'", identifier);
    return false;
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

class FileHandle: public rk_FileHandle {
    rk_Disk *disk;
    HeapArray<FileChunk> chunks;

    std::mutex buf_mutex;
    Size buf_idx = -1;
    HeapArray<uint8_t> buf;

public:
    FileHandle(rk_Disk *disk) : disk(disk) {}

    bool Init(const rk_Hash &hash, Span<const uint8_t> blob);
    Size Read(int64_t offset, Span<uint8_t> out_buf) override;
};

class ChunkHandle: public rk_FileHandle {
    HeapArray<uint8_t> chunk;

public:
    ChunkHandle(HeapArray<uint8_t> &blob);

    Size Read(int64_t offset, Span<uint8_t> out_buf) override;
};

bool FileHandle::Init(const rk_Hash &hash, Span<const uint8_t> blob)
{
    if (blob.len % RG_SIZE(RawChunk) != RG_SIZE(int64_t)) {
        LogError("Malformed file blob '%1'", hash);
        return false;
    }
    blob.len -= RG_SIZE(int64_t);

    // Get file length from end of stream
    int64_t file_size = 0;
    MemCpy(&file_size, blob.end(), RG_SIZE(file_size));
    file_size = LittleEndian(file_size);

    // Check coherence
    Size prev_end = 0;

    for (Size offset = 0; offset < blob.len; offset += RG_SIZE(RawChunk)) {
        FileChunk chunk = {};

        RawChunk entry = {};
        MemCpy(&entry, blob.ptr + offset, RG_SIZE(entry));

        chunk.offset = LittleEndian(entry.offset);
        chunk.len = LittleEndian(entry.len);
        chunk.hash = entry.hash;

        if (prev_end > chunk.offset || chunk.len < 0) [[unlikely]] {
            LogError("Malformed file blob '%1'", hash);
            return false;
        }
        prev_end = chunk.offset + chunk.len;

        chunks.Append(chunk);
    }

    // Check actual file size
    if (blob.len >= RG_SIZE(RawChunk) + RG_SIZE(int64_t)) {
        const RawChunk *entry = (const RawChunk *)(blob.end() - RG_SIZE(RawChunk));
        int64_t len = LittleEndian(entry->offset) + LittleEndian(entry->len);

        if (len != file_size) [[unlikely]] {
            LogError("File size mismatch for '%1'", entry->hash);
            return false;
        }
    }

    return true;
}

Size FileHandle::Read(int64_t offset, Span<uint8_t> out_buf)
{
    Size read_size = 0;

    FileChunk *first = std::lower_bound(chunks.begin(), chunks.end(), offset,
                                        [](const FileChunk &chunk, int64_t offset) {
        return chunk.offset + chunk.len < offset;
    });
    Size idx = first - chunks.ptr;

    while (idx < chunks.len) {
        const FileChunk &chunk = chunks[idx];

        Size copy_offset = offset - chunk.offset;
        Size copy_len = (Size)std::min(chunk.len - copy_offset, (int64_t)out_buf.len);

        // Load blob and copy
        {
            std::lock_guard<std::mutex> lock(buf_mutex);

            if (buf_idx != idx) {
                buf.RemoveFrom(0);

                rk_BlobType type;
                if (!disk->ReadBlob(chunk.hash, &type, &buf))
                    return false;

                if (type != rk_BlobType::Chunk) [[unlikely]] {
                    LogError("Blob '%1' is not a Chunk", chunk.hash);
                    return false;
                }
                if (buf.len != chunk.len) [[unlikely]] {
                    LogError("Chunk size mismatch for '%1'", chunk.hash);
                    return false;
                }

                buf_idx = idx;
            }

            MemCpy(out_buf.ptr, buf.ptr + copy_offset, copy_len);
        }

        offset += copy_len;
        out_buf.ptr += copy_len;
        out_buf.len -= copy_len;
        read_size += copy_len;

        if (!out_buf.len)
            break;

        idx++;
    }

    return read_size;
}

ChunkHandle::ChunkHandle(HeapArray<uint8_t> &blob)
{
    std::swap(chunk, blob);
}

Size ChunkHandle::Read(int64_t offset, Span<uint8_t> out_buf)
{
    Size copy_offset = (Size)std::min(offset, (int64_t)chunk.len);
    Size copy_len = std::min(chunk.len - copy_offset, out_buf.len);

    MemCpy(out_buf.ptr, chunk.ptr + copy_offset, copy_len);

    return copy_len;
}

std::unique_ptr<rk_FileHandle> rk_OpenFile(rk_Disk *disk, const rk_Hash &hash)
{
    rk_BlobType type;
    HeapArray<uint8_t> blob;
    if (!disk->ReadBlob(hash, &type, &blob))
        return nullptr;

    switch (type) {
        case rk_BlobType::File: {
            std::unique_ptr<FileHandle> handle = std::make_unique<FileHandle>(disk);
            if (!handle->Init(hash, blob))
                return nullptr;
            return handle;
        } break;

        case rk_BlobType::Chunk: {
            std::unique_ptr<ChunkHandle> handle = std::make_unique<ChunkHandle>(blob);
            return handle;
        } break;

        case rk_BlobType::Directory1:
        case rk_BlobType::Directory2:
        case rk_BlobType::Directory3:
        case rk_BlobType::Snapshot1:
        case rk_BlobType::Snapshot2:
        case rk_BlobType::Snapshot3:
        case rk_BlobType::Snapshot4:
        case rk_BlobType::Link: {
            LogError("Expected file for '%1'", hash);
            return nullptr;
        } break;
    }

    RG_UNREACHABLE();
}

}
