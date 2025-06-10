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
#include "repository.hh"
#include "tape.hh"
#include "priv_tape.hh"
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

static const int64_t CheckDelay = 7 * 86400000;

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
    rk_Repository *repo;
    rk_RestoreSettings settings;

    ProgressHandle pg_entries { "Entries" };
    ProgressHandle pg_size { "Size" };

    int64_t total_entries = 0;
    int64_t total_size = 0;
    std::atomic_int64_t restored_entries = 0;
    std::atomic_int64_t restored_size = 0;

    Async tasks;

    std::atomic<int64_t> stat_size { 0 };

public:
    GetContext(rk_Repository *repo, const rk_RestoreSettings &settings, int64_t entries, int64_t size);

    bool ExtractEntries(Span<const uint8_t> entries, bool allow_separators, const char *dest_dirname);
    bool ExtractEntries(Span<const uint8_t> entries, bool allow_separators, const EntryInfo &dest);

    int GetFile(const rk_ObjectID &oid, bool chunked, Span<const uint8_t> file, const char *dest_filename);

    bool Sync() { return tasks.Sync(); }

    int64_t GetSize() const { return stat_size; }

private:
    bool CleanDirectory(Span<const char> dirname, const HashSet<Span<const char>> &keep);

    void MakeProgress(int64_t entries, int64_t size);
};

GetContext::GetContext(rk_Repository *repo, const rk_RestoreSettings &settings, int64_t entries, int64_t size)
    : repo(repo), settings(settings), total_entries(entries), total_size(size), tasks(repo->GetAsync())
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
        RawEntry *ptr = (RawEntry *)(blob->ptr + offset);
        Size skip = ptr->GetSize() - 16;

        if (blob->len - offset < skip)
            break;

        entries.Grow(ptr->GetSize());
        MemCpy(entries.end(), blob->ptr + offset, skip);
        MemMove(entries.end() + offsetof(RawEntry, btime), entries.end() + offsetof(RawEntry, ctime), skip - offsetof(RawEntry, ctime));
        MemSet(entries.end() + offsetof(RawEntry, atime), 0, RG_SIZE(RawEntry::atime));
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
    RawEntry *ptr = (RawEntry *)(entries.ptr + offset);

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
    if (entry.kind != (int)RawEntry::Kind::Directory &&
            entry.kind != (int)RawEntry::Kind::File &&
            entry.kind != (int)RawEntry::Kind::Link &&
            entry.kind != (int)RawEntry::Kind::Unknown) {
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

static inline rk_BlobCatalog KindToCatalog(int kind)
{
    switch (kind) {
        case (int)RawEntry::Kind::Directory: return rk_BlobCatalog::Meta;
        default: return rk_BlobCatalog::Raw;
    }
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
        bool xattrs = false;
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

                if (xattrs) {
                    WriteXAttributes(fd, meta.filename.ptr, meta.xattrs);
                }
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

        ctx->chown = settings.chown;
        ctx->xattrs = settings.xattrs;
        ctx->fake = settings.fake;
    }

    for (Size offset = RG_SIZE(DirectoryHeader); offset < entries.len;) {
        EntryInfo entry = {};

        Size skip = DecodeEntry(entries, offset, allow_separators, &ctx->temp_alloc, &entry);
        if (skip < 0)
            return false;
        offset += skip;

        if (entry.kind == (int)RawEntry::Kind::Unknown)
            continue;
        if (!(entry.flags & (int)RawEntry::Flags::Readable))
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
            rk_ObjectID oid = { KindToCatalog(entry.kind), entry.hash };

            int type;
            HeapArray<uint8_t> blob;

            if (!repo->ReadBlob(oid, &type, &blob))
                return false;

            switch (entry.kind) {
                case (int)RawEntry::Kind::Directory: {
                    switch (type) {
                        case (int)BlobType::Directory1: { MigrateLegacyEntries1(&blob, 0); } [[fallthrough]];
                        case (int)BlobType::Directory2: { MigrateLegacyEntries2(&blob, 0); } [[fallthrough]];
                        case (int)BlobType::Directory: {} break;

                        default: {
                            LogError("Blob '%1' is not a Directory", entry.hash);
                            return false;
                        } break;
                    }

                    if (settings.verbose) {
                        Span<const char> prefix = entry.filename.Take(0, entry.filename.len - entry.basename.len - 1);
                        LogInfo("%!D..[D]%!0 %1%/%!..+%2%/%!0", prefix, entry.basename);
                    }

                    if (!settings.fake && !MakeDirectory(entry.filename.ptr, false))
                        return false;
                    if (!ExtractEntries(blob, false, entry))
                        return false;

                    MakeProgress(1, 0);
                } break;

                case (int)RawEntry::Kind::File: {
                    if (type != (int)BlobType::File && type != (int)BlobType::Chunk) {
                        LogError("Blob '%1' is not a File", entry.hash);
                        return false;
                    }

                    if (settings.verbose) {
                        Span<const char> prefix = entry.filename.Take(0, entry.filename.len - entry.basename.len - 1);
                        LogInfo("%!D..[F]%!0 %1%/%!..+%2%!0", prefix, entry.basename);
                    }

                    int fd = -1;
                    RG_DEFER { CloseDescriptor(fd); };

                    if (entry.size) {
                        bool chunked = (type == (int)BlobType::File);

                        fd = GetFile(oid, chunked, blob, entry.filename.ptr);
                        if (!settings.fake && fd < 0)
                            return false;
                    } else if (!settings.fake) {
                        fd = OpenFile(entry.filename.ptr, (int)OpenFlag::Write);
                        if (fd < 0)
                            return false;
                    }

                    if (!settings.fake) {
                        if (settings.chown) {
                            SetFileOwner(fd, entry.filename.ptr, entry.uid, entry.gid);
                        }
                        SetFileMetaData(fd, entry.filename.ptr, entry.mtime, entry.btime, entry.mode);

                        if (settings.xattrs) {
                            WriteXAttributes(fd, entry.filename.ptr, entry.xattrs);
                        }
                    }
                } break;

                case (int)RawEntry::Kind::Link: {
                    if (type != (int)BlobType::Link) {
                        LogError("Blob '%1' is not a Link", entry.hash);
                        return false;
                    }

                    if (settings.verbose) {
                        Span<const char> prefix = entry.filename.Take(0, entry.filename.len - entry.basename.len - 1);
                        LogInfo("%!D..[L]%!0 %1%/%!..+%2%!0", prefix, entry.basename);
                    }

                    // NUL terminate the path
                    blob.Append(0);

                    if (!settings.fake) {
                        if (!CreateSymbolicLink(entry.filename.ptr, (const char *)blob.ptr, settings.force))
                            return false;

                        if (settings.xattrs && entry.xattrs.len) {
                            int fd = OpenFile(entry.filename.ptr, (int)OpenFlag::Write);
                            RG_DEFER { CloseDescriptor(fd); };

                            if (fd >= 0) {
                                WriteXAttributes(fd, entry.filename.ptr, entry.xattrs);
                            }
                        }
                    }

                    MakeProgress(1, 0);
                } break;

                default: { RG_UNREACHABLE(); } break;
            }

            return true;
        });
    }

    return true;
}

int GetContext::GetFile(const rk_ObjectID &oid, bool chunked, Span<const uint8_t> file, const char *dest_filename)
{
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

    if (chunked) {
        if (file.len % RG_SIZE(RawChunk) != RG_SIZE(int64_t)) {
            LogError("Malformed file blob '%1'", oid);
            return -1;
        }
        file.len -= RG_SIZE(int64_t);

        // Get file length from end of stream
        MemCpy(&file_size, file.end(), RG_SIZE(file_size));
        file_size = LittleEndian(file_size);

        if (file_size < 0) {
            LogError("Malformed file blob '%1'", oid);
            return -1;
        }

        if (!settings.fake && !ResizeFile(fd, dest_filename, file_size))
            return -1;

        Async async(&tasks);

        // Check coherence
        Size prev_end = 0;

        // Write unencrypted file
        for (Size offset = 0; offset < file.len; offset += RG_SIZE(RawChunk)) {
            FileChunk chunk = {};

            RawChunk entry = {};
            MemCpy(&entry, file.ptr + offset, RG_SIZE(entry));

            chunk.offset = LittleEndian(entry.offset);
            chunk.len = LittleEndian(entry.len);
            chunk.hash = entry.hash;

            if (prev_end > chunk.offset || chunk.len < 0) [[unlikely]] {
                LogError("Malformed file blob '%1'", oid);
                return false;
            }
            prev_end = chunk.offset + chunk.len;

            async.Run([=, this]() {
                rk_ObjectID oid = { rk_BlobCatalog::Raw, chunk.hash };

                int type;
                HeapArray<uint8_t> blob;

                if (!repo->ReadBlob(oid, &type, &blob))
                    return false;
                if (type != (int)BlobType::Chunk) [[unlikely]] {
                    LogError("Blob '%1' is not a Chunk", oid);
                    return false;
                }
                if (blob.len != chunk.len) [[unlikely]] {
                    LogError("Chunk size mismatch for '%1'", oid);
                    return false;
                }

                if (!settings.fake && !WriteAt(fd, dest_filename, chunk.offset, blob)) {
                    LogError("Failed to write to '%1': %2", dest_filename, strerror(errno));
                    return false;
                }

                MakeProgress(0, chunk.len);

                return true;
            });
        }

        // Only process tasks for this Async, a standard Sync would run other tasks (such as other file tasks)
        // which could take a while and could provoke an accumulation of unfinished file tasks with many open
        // file descriptors and the appearence of slow progress.
        if (!async.SyncSoon())
            return -1;

        // Check actual file size
        if (file.len >= RG_SIZE(RawChunk) + RG_SIZE(int64_t)) {
            const RawChunk *entry = (const RawChunk *)(file.end() - RG_SIZE(RawChunk));
            int64_t size = LittleEndian(entry->offset) + LittleEndian(entry->len);

            if (size != file_size) [[unlikely]] {
                LogError("File size mismatch for '%1'", oid);
                return -1;
            }
        }

        MakeProgress(1, 0);
    } else {
        file_size = file.len;

        if (!settings.fake && !WriteAt(fd, dest_filename, 0, file)) {
            LogError("Failed to write to '%1': %2", dest_filename, strerror(errno));
            return -1;
        }

        MakeProgress(1, file.len);
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

void GetContext::MakeProgress(int64_t entries, int64_t size)
{
    entries = restored_entries.fetch_add(entries, std::memory_order_relaxed) + entries;
    size = restored_size.fetch_add(size, std::memory_order_relaxed) + size;

    if (!settings.verbose) {
        pg_entries.SetFmt(entries, total_entries, "%1 / %2 entries", entries, total_entries);
        pg_size.SetFmt(size, total_size, "%1 / %2", FmtDiskSize(size), FmtDiskSize(total_size));
    }
}

bool rk_Restore(rk_Repository *repo, const rk_ObjectID &oid, const rk_RestoreSettings &settings, const char *dest_path, int64_t *out_size)
{
    int type;
    HeapArray<uint8_t> blob;

    if (!repo->ReadBlob(oid, &type, &blob))
        return false;

    switch (type) {
        case (int)BlobType::Chunk:
        case (int)BlobType::File: {
            if (!settings.force) {
                if (TestFile(dest_path) && !IsDirectoryEmpty(dest_path)) {
                    LogError("File '%1' already exists", dest_path);
                    return false;
                }
            }

            bool chunked = (type == (int)BlobType::File);
            int64_t file_size = 0;

            if (chunked) {
                if (blob.len < RG_SIZE(int64_t)) {
                    LogError("Malformed file blob '%1'", oid);
                    return false;
                }

                MemCpy(&file_size, blob.end() - RG_SIZE(int64_t), RG_SIZE(int64_t));
                file_size = LittleEndian(file_size);
            }

            if (settings.verbose) {
                LogInfo("Restore file %!..+%1%!0", oid);
            }

            GetContext get(repo, settings, 1, file_size);

            int fd = get.GetFile(oid, chunked, blob, dest_path);
            if (!settings.fake && fd < 0)
                return false;
            CloseDescriptor(fd);

            if (out_size) {
                *out_size += get.GetSize();
            }
        } break;

        case (int)BlobType::Directory1: { MigrateLegacyEntries1(&blob, 0); } [[fallthrough]];
        case (int)BlobType::Directory2: { MigrateLegacyEntries2(&blob, 0); } [[fallthrough]];
        case (int)BlobType::Directory: {
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
                LogError("Malformed directory blob '%1'", oid);
                return false;
            }

            DirectoryHeader *header = (DirectoryHeader *)blob.ptr;
            int64_t entries = LittleEndian(header->entries);
            int64_t size = LittleEndian(header->size);

            if (settings.verbose) {
                LogInfo("Restore directory %!..+%1%!0", oid);
            }

            ProgressHandle progress("Restore");
            GetContext get(repo, settings, entries, size);

            if (!get.ExtractEntries(blob, false, dest_path))
                return false;
            if (!get.Sync())
                return false;

            if (out_size) {
                *out_size += get.GetSize();
            }
        } break;

        case (int)BlobType::Snapshot1: { static_assert(RG_SIZE(SnapshotHeader1) == RG_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case (int)BlobType::Snapshot2: { MigrateLegacyEntries1(&blob, RG_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case (int)BlobType::Snapshot3: { MigrateLegacyEntries2(&blob, RG_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case (int)BlobType::Snapshot: {
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
                LogError("Malformed snapshot blob '%1'", oid);
                return false;
            }

            DirectoryHeader *header = (DirectoryHeader *)(blob.ptr + RG_SIZE(SnapshotHeader2));
            int64_t entries = LittleEndian(header->entries);
            int64_t size = LittleEndian(header->size);

            if (settings.verbose) {
                LogInfo("Restore snapshot %!..+%1%!0", oid);
            }

            ProgressHandle progress("Restore");
            GetContext get(repo, settings, entries, size);

            Span<uint8_t> dir = blob.Take(RG_SIZE(SnapshotHeader2), blob.len - RG_SIZE(SnapshotHeader2));

            if (!get.ExtractEntries(dir, true, dest_path))
                return false;
            if (!get.Sync())
                return false;

            if (out_size) {
                *out_size += get.GetSize();
            }
        } break;

        case (int)BlobType::Link: {
            blob.Append(0);

            if (settings.verbose) {
                LogInfo("Restore symbolic link '%1' to '%2'", oid, dest_path);
            }
            if (settings.fake)
                break;

            if (!CreateSymbolicLink(dest_path, (const char *)blob.ptr, settings.force))
                return false;
        } break;

        default: {
            LogError("Invalid blob type %1", type);
            return false;
        } break;
    }

    return true;
}

bool rk_ListSnapshots(rk_Repository *repo, Allocator *alloc,
                      HeapArray<rk_SnapshotInfo> *out_snapshots, HeapArray<rk_ChannelInfo> *out_channels)
{
    RG_ASSERT(out_snapshots || out_channels);

    BlockAllocator temp_alloc;

    Size prev_snapshots = out_snapshots ? out_snapshots->len : 0;
    Size prev_channels = out_channels ? out_channels->len : 0;
    RG_DEFER_N(err_guard) {
        if (out_snapshots) {
            out_snapshots->RemoveFrom(prev_snapshots);
        }
        if (out_channels) {
            out_channels->RemoveFrom(prev_channels);
        }
    };

    HeapArray<rk_TagInfo> tags;
    if (!repo->ListTags(&temp_alloc, &tags))
        return false;

    // Decode tags
    {
        HashMap<const char *, Size> map;

        for (const rk_TagInfo &tag: tags) {
            if (tag.payload.len < (Size)offsetof(SnapshotHeader2, channel) + 1 ||
                    tag.payload.len > RG_SIZE(SnapshotHeader2)) {
                LogError("Malformed snapshot tag (ignoring)");
                continue;
            }

            SnapshotHeader2 header = {};
            MemCpy(&header, tag.payload.ptr, tag.payload.len);
            header.channel[RG_SIZE(header.channel) - 1] = 0;

            const char *name = DuplicateString(header.channel, &temp_alloc).ptr;
            int64_t time = LittleEndian(header.time);

            if (out_snapshots) {
                rk_SnapshotInfo snapshot = {};

                snapshot.tag = DuplicateString(tag.prefix, alloc).ptr;
                snapshot.oid = tag.oid;
                snapshot.channel = DuplicateString(name, alloc).ptr;
                snapshot.time = LittleEndian(header.time);
                snapshot.size = LittleEndian(header.size);
                snapshot.storage = LittleEndian(header.storage);

                out_snapshots->Append(snapshot);
            }

            if (out_channels) {
                Size *ptr = map.TrySet(name, -1);
                Size idx = *ptr;

                if (idx < 0) {
                    rk_ChannelInfo channel = {};

                    channel.name = DuplicateString(name, alloc).ptr;

                    idx = out_channels->len;
                    *ptr = idx;

                    out_channels->Append(channel);
                }

                rk_ChannelInfo *channel = &(*out_channels)[idx];

                if (time > channel->time) {
                    channel->oid = tag.oid;
                    channel->time = time;
                    channel->size = LittleEndian(header.size);
                }

                channel->count++;
            }
        }
    }

    if (out_snapshots) {
        std::sort(out_snapshots->begin() + prev_snapshots, out_snapshots->end(),
                  [](const rk_SnapshotInfo &snapshot1, const rk_SnapshotInfo &snapshot2) { return snapshot1.time < snapshot2.time; });
    }
    if (out_channels) {
        std::sort(out_channels->begin() + prev_channels, out_channels->end(),
                  [](const rk_ChannelInfo &channel1, const rk_ChannelInfo &channel2) { return CmpStr(channel1.name, channel2.name) < 0; });
    }

    err_guard.Disable();
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

static bool ParseOID(Span<const char> str, rk_ObjectID *out_oid)
{
    if (str.len != 65)
        return false;

    // Decode prefix
    {
        const auto ptr = std::find(std::begin(rk_BlobCatalogNames), std::end(rk_BlobCatalogNames), str[0]);

        if (ptr == std::end(rk_BlobCatalogNames))
            return false;

        out_oid->catalog = (rk_BlobCatalog)(ptr - rk_BlobCatalogNames);
    }

    for (Size i = 2, j = 0; i < 66; i += 2, j++) {
        int high = ParseHexadecimalChar(str[i - 1]);
        int low = ParseHexadecimalChar(str[i]);

        if (high < 0 || low < 0)
            return false;

        out_oid->hash.hash[j] = (uint8_t)((high << 4) | low);
    }

    return true;
}

bool rk_LocateObject(rk_Repository *repo, Span<const char> identifier, rk_ObjectID *out_oid)
{
    BlockAllocator temp_alloc;

    Span<const char> path;
    Span<const char> name = TrimStrRight(SplitStr(identifier, ':', &path), "/");
    bool has_path = (path.ptr > name.end());

    rk_ObjectID oid;
    {
        bool found = ParseOID(name, &oid);

        if (!found) {
            HeapArray<rk_SnapshotInfo> snapshots;
            if (!rk_ListSnapshots(repo, &temp_alloc, &snapshots))
                return false;

            for (Size i = snapshots.len - 1; i >= 0; i--) {
                const rk_SnapshotInfo &snapshot = snapshots[i];

                if (TestStr(name, snapshot.channel)) {
                    oid = snapshot.oid;

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

            if (!rk_ListChildren(repo, oid, {}, &temp_alloc, &objects))
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
                oid = obj.oid;

                match = true;
                break;
            }

            if (!match)
                goto missing;
        } while (path.len);
    }

    *out_oid = oid;
    return true;

missing:
    LogError("Cannot find object '%1'", identifier);
    return false;
}

class ListContext {
    rk_Repository *repo;
    rk_ListSettings settings;

    ProgressHandle pg_entries { "Entries" };

    int64_t total_entries = 0;
    std::atomic_int64_t known_entries = 0;

public:
    ListContext(rk_Repository *repo, const rk_ListSettings &settings, int64_t entries);

    bool RecurseEntries(Span<const uint8_t> entries, bool allow_separators, int depth,
                        Allocator *alloc, HeapArray<rk_ObjectInfo> *out_objects);

private:
    void MakeProgress(int64_t entries);
};

ListContext::ListContext(rk_Repository *repo, const rk_ListSettings &settings, int64_t entries)
    : repo(repo), settings(settings), total_entries(entries)
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

    Async async(repo->GetAsync());

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

        obj->oid = { KindToCatalog(entry.kind), entry.hash };
        obj->depth = depth;
        switch (entry.kind) {
            case (int)RawEntry::Kind::Directory: { obj->type = rk_ObjectType::Directory; } break;
            case (int)RawEntry::Kind::File: { obj->type = rk_ObjectType::File; } break;
            case (int)RawEntry::Kind::Link: { obj->type = rk_ObjectType::Link; } break;
            case (int)RawEntry::Kind::Unknown: { obj->type = rk_ObjectType::Unknown; } break;

            default: { RG_UNREACHABLE(); } break;
        }
        obj->name = entry.basename.ptr;
        obj->mtime = entry.mtime;
        obj->ctime = entry.ctime;
        if (entry.flags & (int)RawEntry::Flags::AccessTime) {
            obj->flags |= (int)rk_ObjectFlag::AccessTime;
            obj->atime = entry.atime;
        }
        obj->btime = entry.btime;
        obj->mode = entry.mode;
        obj->uid = entry.uid;
        obj->gid = entry.gid;
        obj->size = entry.size;
        obj->flags |= (entry.flags & (int)RawEntry::Flags::Readable) ? (int)rk_ObjectFlag::Readable : 0;

        if (!(entry.flags & (int)RawEntry::Flags::Readable))
            continue;

        switch (obj->type) {
            case rk_ObjectType::Snapshot: { RG_UNREACHABLE(); } break;

            case rk_ObjectType::Directory: {
                if (!settings.recurse)
                    break;

                async.Run([=, &contexts, this]() {
                    RecurseContext *ctx = &contexts[i];

                    int type;
                    HeapArray<uint8_t> blob;

                    if (!repo->ReadBlob(obj->oid, &type, &blob))
                        return false;

                    switch (type) {
                        case (int)BlobType::Directory1: { MigrateLegacyEntries1(&blob, 0); } [[fallthrough]];
                        case (int)BlobType::Directory2: { MigrateLegacyEntries2(&blob, 0); } [[fallthrough]];
                        case (int)BlobType::Directory: {} break;

                        default: {
                            LogError("Blob '%1' is not a Directory", entry.hash);
                            return false;
                        } break;
                    }

                    if (!RecurseEntries(blob, false, depth + 1, &ctx->str_alloc, &ctx->children))
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

void ListContext::MakeProgress(int64_t entries)
{
    entries = known_entries.fetch_add(entries, std::memory_order_relaxed) + entries;

    if (total_entries) {
        pg_entries.SetFmt(entries, total_entries, "%1 / %2 entries", entries, total_entries);
    } else {
        pg_entries.SetFmt(entries, total_entries, "%1 entries", entries);
    }
}

bool rk_ListChildren(rk_Repository *repo, const rk_ObjectID &oid, const rk_ListSettings &settings,
                     Allocator *alloc, HeapArray<rk_ObjectInfo> *out_objects)
{
    Size prev_len = out_objects->len;
    RG_DEFER_N(out_guard) { out_objects->RemoveFrom(prev_len); };

    int type;
    HeapArray<uint8_t> blob;

    if (!repo->ReadBlob(oid, &type, &blob))
        return false;

    switch (type) {
        case (int)BlobType::Directory1: { MigrateLegacyEntries1(&blob, 0); } [[fallthrough]];
        case (int)BlobType::Directory2: { MigrateLegacyEntries2(&blob, 0); } [[fallthrough]];
        case (int)BlobType::Directory: {
            if (blob.len < RG_SIZE(DirectoryHeader)) {
                LogError("Malformed directory blob '%1'", oid);
                return false;
            }

            DirectoryHeader *header = (DirectoryHeader *)blob.ptr;
            int64_t entries = LittleEndian(header->entries);

            ProgressHandle progress;
            ListContext tree(repo, settings, entries);

            if (!tree.RecurseEntries(blob, false, 0, alloc, out_objects))
                return false;
        } break;

        case (int)BlobType::Snapshot1: {
            static_assert(RG_SIZE(SnapshotHeader1) == RG_SIZE(SnapshotHeader2));

            if (blob.len <= RG_SIZE(SnapshotHeader1)) {
                LogError("Malformed snapshot blob '%1'", oid);
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
        case (int)BlobType::Snapshot2: { MigrateLegacyEntries1(&blob, RG_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case (int)BlobType::Snapshot3: { MigrateLegacyEntries2(&blob, RG_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case (int)BlobType::Snapshot: {
            if (blob.len < RG_SIZE(SnapshotHeader2) + RG_SIZE(DirectoryHeader)) {
                LogError("Malformed snapshot blob '%1'", oid);
                return false;
            }

            SnapshotHeader2 *header1 = (SnapshotHeader2 *)blob.ptr;
            DirectoryHeader *header2 = (DirectoryHeader *)(header1 + 1);
            int64_t entries = LittleEndian(header2->entries);

            ListContext tree(repo, settings, entries);

            // Make sure snapshot channel is NUL terminated
            header1->channel[RG_SIZE(header1->channel) - 1] = 0;

            rk_ObjectInfo *obj = out_objects->AppendDefault();

            obj->oid = oid;
            obj->type = rk_ObjectType::Snapshot;
            obj->name = DuplicateString(header1->channel, alloc).ptr;
            obj->mtime = LittleEndian(header1->time);
            obj->size = LittleEndian(header1->size);
            obj->flags |= (int)rk_ObjectFlag::Readable;
            obj->storage = LittleEndian(header1->storage);

            Span<uint8_t> dir = blob.Take(RG_SIZE(SnapshotHeader2), blob.len - RG_SIZE(SnapshotHeader2));

            if (!tree.RecurseEntries(dir, true, 1, alloc, out_objects))
                return false;

            // Reacquire correct pointer (array may have moved)
            obj = &(*out_objects)[prev_len];

            for (Size i = prev_len; i < out_objects->len; i++) {
                const rk_ObjectInfo &child = (*out_objects)[i];
                obj->children += (child.depth == 1);
            }
        } break;

        case (int)BlobType::Chunk:
        case (int)BlobType::File:
        case (int)BlobType::Link: {
            LogInfo("Expected Snapshot or Directory blob, not %1", BlobTypeNames[type]);
            return false;
        } break;

        default: {
            LogError("Invalid blob type %1", type);
            return false;
        } break;
    }

    out_guard.Disable();
    return true;
}

class CheckContext {
    rk_Repository *repo;
    sq_Database *db;
    int64_t mark;

    ProgressHandle pg_blobs { "Blobs" };

    int64_t total_blobs = 0;
    std::atomic_int64_t checked_blobs = 0;

    Async tasks;

public:
    CheckContext(rk_Repository *repo, sq_Database *db, int64_t mark, int64_t blobs);

    bool CheckBlob(const rk_ObjectID &oid);
    bool RecurseEntries(Span<const uint8_t> entries, bool allow_separators);

private:
    void MakeProgress(int64_t blobs);
};

CheckContext::CheckContext(rk_Repository *repo, sq_Database *db, int64_t mark, int64_t blobs)
    : repo(repo), db(db), mark(mark), total_blobs(blobs), tasks(repo->GetAsync())
{
}

bool CheckContext::CheckBlob(const rk_ObjectID &oid)
{
    char key[128];
    Fmt(key, "%1", oid);

    // Ignore recently checked objects
    {
        sq_Statement stmt;
        if (!db->Prepare("SELECT oid FROM checks WHERE oid = ?1", &stmt, key))
            return false;

        if (stmt.Step())
            return true;
        if (!stmt.IsValid())
            return false;
    }

    int type;
    HeapArray<uint8_t> blob;

    if (!repo->ReadBlob(oid, &type, &blob))
        return false;

    MakeProgress(1);

    switch (type) {
        case (int)BlobType::Chunk: {} break;

        case (int)BlobType::File: {
            int64_t file_size = 0;

            if (blob.len % RG_SIZE(RawChunk) != RG_SIZE(int64_t)) {
                LogError("Malformed file blob '%1'", oid);
                return false;
            }
            blob.len -= RG_SIZE(int64_t);

            // Get file length from end of stream
            MemCpy(&file_size, blob.end(), RG_SIZE(file_size));
            file_size = LittleEndian(file_size);

            if (file_size < 0) {
                LogError("Malformed file blob '%1'", oid);
                return false;
            }

            Size prev_end = 0;

            for (Size offset = 0; offset < blob.len; offset += RG_SIZE(RawChunk)) {
                FileChunk chunk = {};

                RawChunk entry = {};
                MemCpy(&entry, blob.ptr + offset, RG_SIZE(entry));

                chunk.offset = LittleEndian(entry.offset);
                chunk.len = LittleEndian(entry.len);
                chunk.hash = entry.hash;

                if (prev_end > chunk.offset || chunk.len < 0) [[unlikely]] {
                    LogError("Malformed file blob '%1'", oid);
                    return false;
                }
                prev_end = chunk.offset + chunk.len;

                tasks.Run([=, this]() {
                    rk_ObjectID oid = { rk_BlobCatalog::Raw, chunk.hash };

                    int type;
                    HeapArray<uint8_t> blob;

                    if (!repo->ReadBlob(oid, &type, &blob))
                        return false;
                    if (type != (int)BlobType::Chunk) [[unlikely]] {
                        LogError("Blob '%1' is not a Chunk", oid);
                        return false;
                    }
                    if (blob.len != chunk.len) [[unlikely]] {
                        LogError("Chunk size mismatch for '%1'", oid);
                        return false;
                    }

                    return true;
                });
            }

            if (blob.len >= RG_SIZE(RawChunk) + RG_SIZE(int64_t)) {
                const RawChunk *entry = (const RawChunk *)(blob.end() - RG_SIZE(RawChunk));
                int64_t size = LittleEndian(entry->offset) + LittleEndian(entry->len);

                if (size != file_size) [[unlikely]] {
                    LogError("File size mismatch for '%1'", oid);
                    return false;
                }
            }
        } break;

        case (int)BlobType::Directory1: { MigrateLegacyEntries1(&blob, 0); } [[fallthrough]];
        case (int)BlobType::Directory2: { MigrateLegacyEntries2(&blob, 0); } [[fallthrough]];
        case (int)BlobType::Directory: {
            if (!RecurseEntries(blob, false))
                return false;
        } break;

        case (int)BlobType::Snapshot1: {
            static_assert(RG_SIZE(SnapshotHeader1) == RG_SIZE(SnapshotHeader2));

            if (blob.len <= RG_SIZE(SnapshotHeader1)) {
                LogError("Malformed snapshot blob '%1'", oid);
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
        case (int)BlobType::Snapshot2: { MigrateLegacyEntries1(&blob, RG_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case (int)BlobType::Snapshot3: { MigrateLegacyEntries2(&blob, RG_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case (int)BlobType::Snapshot: {
            Span<uint8_t> dir = blob.Take(RG_SIZE(SnapshotHeader2), blob.len - RG_SIZE(SnapshotHeader2));

            if (!RecurseEntries(dir, true))
                return false;
        } break;

        case (int)BlobType::Link: {
            // XXX: Check that the symbolic link target looks legit?
        } break;

        default: {
            LogError("Invalid blob type %1", type);
            return false;
        } break;
    }

    if (!db->Run(R"(INSERT INTO checks (oid, mark)
                    VALUES (?1, ?2)
                    ON CONFLICT DO UPDATE SET mark = excluded.mark)",
                 key, mark))
        return false;

    repo->CommitCache();

    return true;
}

bool CheckContext::RecurseEntries(Span<const uint8_t> entries, bool allow_separators)
{
    BlockAllocator temp_alloc;

    if (entries.len < RG_SIZE(DirectoryHeader)) [[unlikely]] {
        LogError("Malformed directory blob");
        return false;
    }

    HeapArray<EntryInfo> decoded;
    for (Size offset = RG_SIZE(DirectoryHeader); offset < entries.len;) {
        EntryInfo entry = {};

        Size skip = DecodeEntry(entries, offset, allow_separators, &temp_alloc, &entry);
        if (skip < 0)
            return false;
        offset += skip;

        if (entry.kind == (int)RawEntry::Kind::Unknown)
            continue;
        if (!(entry.flags & (int)RawEntry::Flags::Readable))
            continue;

        // Don't keep dangling pointers (once function returns)
        entry.basename = {};
        entry.xattrs = {};

        decoded.Append(entry);
    }

    Async async(&tasks);

    for (Size i = 0; i < decoded.len; i++) {
        const EntryInfo *entry = &decoded[i];

        async.Run([&, entry, this] () {
            rk_ObjectID oid = { KindToCatalog(entry->kind), entry->hash };
            return CheckBlob(oid);
        });
    }

    if (!async.Sync())
        return false;

    return true;
}

void CheckContext::MakeProgress(int64_t blobs)
{
    blobs = checked_blobs.fetch_add(blobs, std::memory_order_relaxed) + blobs;
    pg_blobs.SetFmt(blobs, total_blobs, "%1 / %2 blobs", blobs, total_blobs);
}

bool rk_CheckSnapshots(rk_Repository *repo, Span<const rk_SnapshotInfo> snapshots)
{
    BlockAllocator temp_alloc;

    sq_Database *db = repo->OpenCache(false);
    if (!db)
        return false;
    if (!repo->ResetCache(true))
        return false;

    int64_t mark = GetUnixTime();

    if (!db->Run("DELETE FROM checks WHERE mark < ?1", mark - CheckDelay))
        return false;

    int64_t blobs;
    {
        sq_Statement stmt;
        if (!db->Prepare(R"(SELECT COUNT(oid)
                            FROM blobs
                            WHERE oid NOT IN (SELECT oid FROM checks))", &stmt))
            return false;
        if (!stmt.GetSingleValue(&blobs))
            return false;
    }

    CheckContext check(repo, db, mark, blobs);

    for (const rk_SnapshotInfo &snapshot: snapshots) {
        if (!check.CheckBlob(snapshot.oid))
            return false;
    }

    return true;
}

const char *rk_ReadLink(rk_Repository *repo, const rk_ObjectID &oid, Allocator *alloc)
{
    int type;
    HeapArray<uint8_t> blob;

    if (!repo->ReadBlob(oid, &type, &blob))
        return nullptr;
    if (type != (int)BlobType::Link) {
        LogError("Expected symbolic link for '%1'", oid);
        return nullptr;
    }

    const char *target = DuplicateString(blob.As<const char>(), alloc).ptr;
    return target;
}

class FileHandle: public rk_FileHandle {
    rk_Repository *repo;
    HeapArray<FileChunk> chunks;

    std::mutex buf_mutex;
    Size buf_idx = -1;
    HeapArray<uint8_t> buf;

public:
    FileHandle(rk_Repository *repo) : repo(repo) {}

    bool Init(const rk_ObjectID &oid, Span<const uint8_t> blob);
    Size Read(int64_t offset, Span<uint8_t> out_buf) override;
};

class ChunkHandle: public rk_FileHandle {
    HeapArray<uint8_t> chunk;

public:
    ChunkHandle(HeapArray<uint8_t> &blob);

    Size Read(int64_t offset, Span<uint8_t> out_buf) override;
};

bool FileHandle::Init(const rk_ObjectID &oid, Span<const uint8_t> blob)
{
    if (blob.len % RG_SIZE(RawChunk) != RG_SIZE(int64_t)) {
        LogError("Malformed file blob '%1'", oid);
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
            LogError("Malformed file blob '%1'", oid);
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

                rk_ObjectID oid = { rk_BlobCatalog::Raw, chunk.hash };

                int type;

                if (!repo->ReadBlob(oid, &type, &buf))
                    return false;
                if (type != (int)BlobType::Chunk) [[unlikely]] {
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

std::unique_ptr<rk_FileHandle> rk_OpenFile(rk_Repository *repo, const rk_ObjectID &oid)
{
    int type;
    HeapArray<uint8_t> blob;

    if (!repo->ReadBlob(oid, &type, &blob))
        return nullptr;

    switch (type) {
        case (int)BlobType::File: {
            std::unique_ptr<FileHandle> handle = std::make_unique<FileHandle>(repo);
            if (!handle->Init(oid, blob))
                return nullptr;
            return handle;
        } break;

        case (int)BlobType::Chunk: {
            std::unique_ptr<ChunkHandle> handle = std::make_unique<ChunkHandle>(blob);
            return handle;
        } break;

        case (int)BlobType::Directory1:
        case (int)BlobType::Directory2:
        case (int)BlobType::Directory:
        case (int)BlobType::Snapshot1:
        case (int)BlobType::Snapshot2:
        case (int)BlobType::Snapshot3:
        case (int)BlobType::Snapshot:
        case (int)BlobType::Link: {
            LogError("Expected file for '%1'", oid);
            return nullptr;
        } break;

        default: {
            LogError("Invalid blob type %1", type);
            return nullptr;
        } break;
    }

    RG_UNREACHABLE();
}

}
