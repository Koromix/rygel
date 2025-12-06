// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "cache.hh"
#include "repository.hh"
#include "tape.hh"
#include "priv_tape.hh"
#include "xattr.hh"
#include "vendor/blake3/c/blake3.h"

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

namespace K {

void MigrateLegacySnapshot1(HeapArray<uint8_t> *blob);
void MigrateLegacySnapshot2(HeapArray<uint8_t> *blob);

void MigrateLegacyEntries1(HeapArray<uint8_t> *blob, Size start);
void MigrateLegacyEntries2(HeapArray<uint8_t> *blob, Size start);
void MigrateLegacyEntries3(HeapArray<uint8_t> *blob, Size start);

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

    // These functions run asynchronously, use Sync() to wait
    bool ExtractEntries(Span<const uint8_t> blob, bool allow_separators, const char *dest_dirname);
    bool ExtractEntries(Span<const uint8_t> blob, bool allow_separators, const EntryInfo &dest);

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
    K_ASSERT(buf.len < UINT32_MAX);

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

#else

static bool WriteAt(int fd, const char *filename, int64_t offset, Span<const uint8_t> buf)
{
    while (buf.len) {
        Size written = K_RESTART_EINTR(pwrite(fd, buf.ptr, buf.len, (off_t)offset), < 0);

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

#endif

// Does not fill EntryInfo::filename
static Size DecodeEntry(Span<const uint8_t> blob, Size offset, bool allow_separators,
                        Allocator *alloc, EntryInfo *out_entry)
{
    RawEntry *ptr = (RawEntry *)(blob.ptr + offset);

    if (blob.len - offset < ptr->GetSize()) {
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
            if (extended.len - offset < K_SIZE(uint16_t)) {
                LogError("Truncated extended blob");
                return -1;
            }

            uint16_t attr_len;
            MemCpy(&attr_len, extended.ptr + offset, K_SIZE(attr_len));
            attr_len = LittleEndian(attr_len);

            if (attr_len > extended.len) {
                LogError("Invalid extended length");
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
            MemCpy(&attr_len, extended.ptr + offset, K_SIZE(attr_len));
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
    if (!allow_separators && strpbrk(entry.basename.ptr, K_PATH_SEPARATORS)) {
        LogError("Unsafe object name '%1'", entry.basename);
        return -1;
    }

    *out_entry = entry;
    return ptr->GetSize();
}

// Does not fill EntryInfo::filename
static bool DecodeEntries(Span<const uint8_t> blob, Size offset, bool allow_separators,
                          Allocator *alloc, HeapArray<EntryInfo> *out_entries)
{
    K_DEFER_NC(err_guard, len = out_entries->len) { out_entries->RemoveFrom(len); };

    while (offset < blob.len) {
        EntryInfo entry = {};

        Size skip = DecodeEntry(blob, offset, allow_separators, alloc, &entry);
        if (skip < 0)
            return false;
        offset += skip;

        out_entries->Append(entry);
    }

    err_guard.Disable();
    return true;
}

static int64_t DecodeChunks(const rk_ObjectID &oid, Span<const uint8_t> blob, HeapArray<FileChunk> *out_chunks)
{
    K_DEFER_NC(err_guard, len = out_chunks->len) { out_chunks->RemoveFrom(len); };

    int64_t file_size = -1;

    if (blob.len % K_SIZE(RawChunk) != K_SIZE(int64_t)) {
        LogError("Malformed file blob '%1'", oid);
        return -1;
    }
    blob.len -= K_SIZE(int64_t);

    // Get file length from end of stream
    MemCpy(&file_size, blob.end(), K_SIZE(file_size));
    file_size = LittleEndian(file_size);

    if (file_size < 0) {
        LogError("Malformed file blob '%1'", oid);
        return -1;
    }

    // Check coherence
    Size prev_end = 0;

    for (Size offset = 0; offset < blob.len; offset += K_SIZE(RawChunk)) {
        FileChunk chunk = {};

        RawChunk entry = {};
        MemCpy(&entry, blob.ptr + offset, K_SIZE(entry));

        chunk.offset = LittleEndian(entry.offset);
        chunk.len = LittleEndian(entry.len);
        chunk.hash = entry.hash;

        if (prev_end > chunk.offset || chunk.len < 0) [[unlikely]] {
            LogError("Malformed file blob '%1'", oid);
            return false;
        }
        prev_end = chunk.offset + chunk.len;

        out_chunks->Append(chunk);
    }

    if (blob.len >= K_SIZE(RawChunk) + K_SIZE(int64_t)) {
        const RawChunk *last = (const RawChunk *)(blob.end() - K_SIZE(RawChunk));
        int64_t size = LittleEndian(last->offset) + LittleEndian(last->len);

        if (size != file_size) [[unlikely]] {
            LogError("File size mismatch for '%1'", oid);
            return -1;
        }
    }

    err_guard.Disable();
    return file_size;
}

static inline rk_BlobCatalog KindToCatalog(int kind)
{
    switch (kind) {
        case (int)RawEntry::Kind::Directory: return rk_BlobCatalog::Meta;
        default: return rk_BlobCatalog::Raw;
    }
}

bool GetContext::ExtractEntries(Span<const uint8_t> blob, bool allow_separators, const char *dest_dirname)
{
    EntryInfo dest = {};

    if (!dest_dirname[0]) {
        dest_dirname = ".";
    }
    dest.filename = TrimStrRight(dest_dirname, K_PATH_SEPARATORS);

    return ExtractEntries(blob, allow_separators, dest);
}

bool GetContext::ExtractEntries(Span<const uint8_t> blob, bool allow_separators, const EntryInfo &dest)
{
    // XXX: Make sure each path does not clobber a previous one

    if (blob.len < K_SIZE(DirectoryHeader)) [[unlikely]] {
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
                K_DEFER { CloseDescriptor(fd); };

#if !defined(_WIN32)
                if (chown) {
                    SetFileOwner(fd, meta.filename.ptr, meta.uid, meta.gid);
                }
                SetFileMode(fd, meta.filename.ptr, meta.mode);
#endif
                SetFileTimes(fd, meta.filename.ptr, meta.mtime, meta.btime);

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
            MemCpy(ctx->meta.xattrs.ptr, xattrs.ptr, xattrs.len * K_SIZE(XAttrInfo));
        }

        ctx->chown = settings.chown;
        ctx->xattrs = settings.xattrs;
        ctx->fake = settings.fake;
    }

    if (!DecodeEntries(blob, K_SIZE(DirectoryHeader), allow_separators, &ctx->temp_alloc, &ctx->entries))
        return false;

    // Filter out invalid entries
    {
        Size j = 0;
        for (Size i = 0; i < ctx->entries.len; i++) {
            EntryInfo *entry = &ctx->entries[j];
            *entry = ctx->entries[i];

            if (entry->kind == (int)RawEntry::Kind::Unknown)
                continue;
            if (!(entry->flags & (int)RawEntry::Flags::Readable))
                continue;

            entry->filename = Fmt(&ctx->temp_alloc, "%1%/%2", dest.filename, entry->basename).ptr;

            if (!settings.fake && allow_separators && !EnsureDirectoryExists(entry->filename.ptr))
                return false;

            j++;
        }
        ctx->entries.len = j;
    }

    if (settings.unlink) {
        HashSet<Span<const char>> keep;

        for (const EntryInfo &entry: ctx->entries) {
            Span<const char> path = entry.filename;
            keep.Set(path);

            if (allow_separators) {
                SplitStrReverse(path, *K_PATH_SEPARATORS, &path);

                while (path.len > dest.filename.len) {
                    keep.Set(path);
                    SplitStrReverse(path, *K_PATH_SEPARATORS, &path);
                }
            }
        }

        if (!CleanDirectory(dest.filename, keep))
            return false;

        if (allow_separators) {
            for (const EntryInfo &entry: ctx->entries) {
                Span<const char> path = entry.filename;
                SplitStrReverse(path, *K_PATH_SEPARATORS, &path);

                while (path.len > dest.filename.len) {
                    if (!CleanDirectory(path, keep))
                        return false;
                    SplitStrReverse(path, *K_PATH_SEPARATORS, &path);
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
                        case (int)BlobType::Directory3: { MigrateLegacyEntries3(&blob, 0); } [[fallthrough]];
                        case (int)BlobType::Directory: {} break;

                        default: {
                            LogError("Blob '%1' is not a Directory", oid);
                            return false;
                        } break;
                    }

                    if (settings.verbose) {
                        Span<const char> prefix = entry.filename.Take(0, entry.filename.len - entry.basename.len - 1);
                        LogInfo(("%!D..[D]%!0 %1%/%!..+%2%/%!0"), prefix, entry.basename);
                    }

                    if (!settings.fake && !MakeDirectory(entry.filename.ptr, false))
                        return false;
                    if (!ExtractEntries(blob, false, entry))
                        return false;

                    MakeProgress(1, 0);
                } break;

                case (int)RawEntry::Kind::File: {
                    if (type != (int)BlobType::File && type != (int)BlobType::Chunk) {
                        LogError("Blob '%1' is not a File", oid);
                        return false;
                    }

                    if (settings.verbose) {
                        Span<const char> prefix = entry.filename.Take(0, entry.filename.len - entry.basename.len - 1);
                        LogInfo(("%!D..[F]%!0 %1%/%!..+%2%!0"), prefix, entry.basename);
                    }

                    int fd = -1;
                    K_DEFER { CloseDescriptor(fd); };

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
#if !defined(_WIN32)
                        if (settings.chown) {
                            SetFileOwner(fd, entry.filename.ptr, entry.uid, entry.gid);
                        }
                        SetFileMode(fd, entry.filename.ptr, entry.mode);
#endif
                        SetFileTimes(fd, entry.filename.ptr, entry.mtime, entry.btime);

                        if (settings.xattrs) {
                            WriteXAttributes(fd, entry.filename.ptr, entry.xattrs);
                        }
                    }
                } break;

                case (int)RawEntry::Kind::Link: {
                    if (type != (int)BlobType::Link) {
                        LogError("Blob '%1' is not a Link", oid);
                        return false;
                    }

                    if (settings.verbose) {
                        Span<const char> prefix = entry.filename.Take(0, entry.filename.len - entry.basename.len - 1);
                        LogInfo(("%!D..[L]%!0 %1%/%!..+%2%!0"), prefix, entry.basename);
                    }

                    // NUL terminate the path
                    blob.Append(0);

                    if (!settings.fake) {
                        if (!CreateSymbolicLink(entry.filename.ptr, (const char *)blob.ptr, settings.force))
                            return false;

#if !defined(_WIN32)
                        if (settings.chown) {
                            SetFileOwner(entry.filename.ptr, entry.uid, entry.gid);
                        }
                        SetFileTimes(entry.filename.ptr, entry.mtime, entry.btime);
#endif

                        if (settings.xattrs && entry.xattrs.len) {
                            WriteXAttributes(-1, entry.filename.ptr, entry.xattrs);
                        }
                    }

                    MakeProgress(1, 0);
                } break;

                default: { K_UNREACHABLE(); } break;
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
    K_DEFER_N(err_guard) { CloseDescriptor(fd); };

    int64_t file_size = 0;

    if (chunked) {
        HeapArray<FileChunk> chunks;
        file_size = DecodeChunks(oid, file, &chunks);
        if (file_size < 0)
            return -1;

        if (!settings.fake && !ResizeFile(fd, dest_filename, file_size))
            return -1;

        Async async(&tasks);

        for (const FileChunk &chunk: chunks) {
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
        pg_entries.SetFmt(entries, total_entries, T("%1 / %2 entries"), entries, total_entries);
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
                if (blob.len < K_SIZE(int64_t)) {
                    LogError("Malformed file blob '%1'", oid);
                    return false;
                }

                MemCpy(&file_size, blob.end() - K_SIZE(int64_t), K_SIZE(int64_t));
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
        case (int)BlobType::Directory3: { MigrateLegacyEntries3(&blob, 0); } [[fallthrough]];
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

            if (blob.len < K_SIZE(DirectoryHeader)) {
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

        case (int)BlobType::Snapshot1: { MigrateLegacySnapshot1(&blob); } [[fallthrough]];
        case (int)BlobType::Snapshot2: { MigrateLegacyEntries1(&blob, K_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case (int)BlobType::Snapshot3: { MigrateLegacyEntries2(&blob, K_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case (int)BlobType::Snapshot4: { MigrateLegacyEntries3(&blob, K_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case (int)BlobType::Snapshot5: { MigrateLegacySnapshot2(&blob); } [[fallthrough]];
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
            if (blob.len <= K_SIZE(SnapshotHeader3) + K_SIZE(DirectoryHeader)) {
                LogError("Malformed snapshot blob '%1'", oid);
                return false;
            }

            DirectoryHeader *header = (DirectoryHeader *)(blob.ptr + K_SIZE(SnapshotHeader3));
            int64_t entries = LittleEndian(header->entries);
            int64_t size = LittleEndian(header->size);

            if (settings.verbose) {
                LogInfo("Restore snapshot %!..+%1%!0", oid);
            }

            ProgressHandle progress("Restore");
            GetContext get(repo, settings, entries, size);

            Span<uint8_t> dir = blob.Take(K_SIZE(SnapshotHeader3), blob.len - K_SIZE(SnapshotHeader3));

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

bool rk_ListSnapshots(rk_Repository *repo, Allocator *alloc, HeapArray<rk_SnapshotInfo> *out_snapshots)
{
    BlockAllocator temp_alloc;

    Size prev_snapshots = out_snapshots ? out_snapshots->len : 0;
    K_DEFER_N(err_guard) { out_snapshots->RemoveFrom(prev_snapshots); };

    HeapArray<rk_TagInfo> tags;
    if (!repo->ListTags(&temp_alloc, &tags))
        return false;

    // Decode tags
    {
        HashMap<const char *, Size> map;

        for (const rk_TagInfo &tag: tags) {
            rk_SnapshotInfo snapshot = {};

            if (tag.payload.len < (Size)offsetof(SnapshotHeader3, channel) + 1 ||
                    tag.payload.len > K_SIZE(SnapshotHeader3)) {
                LogError("Malformed snapshot tag (ignoring)");
                continue;
            }

            SnapshotHeader3 header = {};
            MemCpy(&header, tag.payload.ptr, tag.payload.len);
            header.channel[K_SIZE(header.channel) - 1] = 0;

            snapshot.tag = DuplicateString(tag.name, alloc).ptr;
            snapshot.oid = tag.oid;
            snapshot.channel = DuplicateString(header.channel, alloc).ptr;
            snapshot.time = LittleEndian(header.time);
            snapshot.size = LittleEndian(header.size);
            snapshot.stored = LittleEndian(header.stored);
            snapshot.added = LittleEndian(header.added);

            out_snapshots->Append(snapshot);
        }
    }

    std::sort(out_snapshots->begin() + prev_snapshots, out_snapshots->end(),
              [](const rk_SnapshotInfo &snapshot1, const rk_SnapshotInfo &snapshot2) { return snapshot1.time < snapshot2.time; });

    err_guard.Disable();
    return true;
}

void rk_ListChannels(Span<const rk_SnapshotInfo> snapshots, Allocator *alloc, HeapArray<rk_ChannelInfo> *out_channels)
{
    Size prev_channels = out_channels->len;

    HashMap<const char *, Size> map;

    for (const rk_SnapshotInfo &snapshot: snapshots) {
        Size *ptr = map.InsertOrGet(snapshot.channel, -1);
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
            channel->oid = snapshot.oid;
            channel->time = snapshot.time;
            channel->size = snapshot.size;
        }

        channel->count++;
    }

    std::sort(out_channels->begin() + prev_channels, out_channels->end(),
              [](const rk_ChannelInfo &channel1, const rk_ChannelInfo &channel2) { return CmpStr(channel1.name, channel2.name) < 0; });
}

bool rk_ListChannels(rk_Repository *repo, Allocator *alloc, HeapArray<rk_ChannelInfo> *out_channels)
{
    BlockAllocator temp_alloc;

    HeapArray<rk_SnapshotInfo> snapshots;
    if (!rk_ListSnapshots(repo, &temp_alloc, &snapshots))
        return false;

    rk_ListChannels(snapshots, alloc, out_channels);

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
        bool found = rk_ParseOID(name, &oid);

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

    Size RecurseEntries(Span<const uint8_t> blob, bool allow_separators, int depth,
                        Allocator *alloc, HeapArray<rk_ObjectInfo> *out_objects);

private:
    void MakeProgress(int64_t entries);
};

ListContext::ListContext(rk_Repository *repo, const rk_ListSettings &settings, int64_t entries)
    : repo(repo), settings(settings), total_entries(entries)
{
}

Size ListContext::RecurseEntries(Span<const uint8_t> blob, bool allow_separators, int depth,
                                 Allocator *alloc, HeapArray<rk_ObjectInfo> *out_objects)
{
    if (blob.len < K_SIZE(DirectoryHeader)) [[unlikely]] {
        LogError("Malformed directory blob");
        return -1;
    }

    HeapArray<EntryInfo> entries;
    if (!DecodeEntries(blob, K_SIZE(DirectoryHeader), allow_separators, alloc, &entries))
        return -1;

    Async async(repo->GetAsync());

    struct RecurseContext {
        rk_ObjectInfo obj;
        HeapArray<rk_ObjectInfo> children;

        BlockAllocator str_alloc;
    };

    HeapArray<RecurseContext> contexts;
    contexts.AppendDefault(entries.len);

    MakeProgress(0);

    for (Size i = 0; i < entries.len; i++) {
        const EntryInfo &entry = entries[i];
        rk_ObjectInfo *obj = &contexts[i].obj;

        obj->oid = { KindToCatalog(entry.kind), entry.hash };
        obj->depth = depth;
        switch (entry.kind) {
            case (int)RawEntry::Kind::Directory: { obj->type = rk_ObjectType::Directory; } break;
            case (int)RawEntry::Kind::File: { obj->type = rk_ObjectType::File; } break;
            case (int)RawEntry::Kind::Link: { obj->type = rk_ObjectType::Link; } break;
            case (int)RawEntry::Kind::Unknown: { obj->type = rk_ObjectType::Unknown; } break;

            default: { K_UNREACHABLE(); } break;
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
            case rk_ObjectType::Snapshot: { K_UNREACHABLE(); } break;

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
                        case (int)BlobType::Directory3: { MigrateLegacyEntries3(&blob, 0); } [[fallthrough]];
                        case (int)BlobType::Directory: {} break;

                        default: {
                            LogError("Blob '%1' is not a Directory", obj->oid);
                            return false;
                        } break;
                    }

                    Size children = RecurseEntries(blob, false, depth + 1, &ctx->str_alloc, &ctx->children);
                    if (children < 0)
                        return false;
                    obj->children = children;

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
        return -1;

    for (const RecurseContext &ctx: contexts) {
        out_objects->Append(ctx.obj);

        for (const rk_ObjectInfo &child: ctx.children) {
            rk_ObjectInfo *ptr = out_objects->Append(child);
            ptr->name = DuplicateString(ptr->name, alloc).ptr;
        }
    }

    return entries.len;
}

void ListContext::MakeProgress(int64_t entries)
{
    if (!settings.recurse)
        return;

    entries = known_entries.fetch_add(entries, std::memory_order_relaxed) + entries;

    if (total_entries) {
        pg_entries.SetFmt(entries, total_entries, T("%1 / %2 entries"), entries, total_entries);
    } else {
        pg_entries.SetFmt(entries, total_entries, T("%1 entries"), entries);
    }
}

bool rk_ListChildren(rk_Repository *repo, const rk_ObjectID &oid, const rk_ListSettings &settings,
                     Allocator *alloc, HeapArray<rk_ObjectInfo> *out_objects)
{
    Size prev_len = out_objects->len;
    K_DEFER_N(out_guard) { out_objects->RemoveFrom(prev_len); };

    int type;
    HeapArray<uint8_t> blob;
    int64_t size;

    if (!repo->ReadBlob(oid, &type, &blob, &size))
        return false;

    switch (type) {
        case (int)BlobType::Directory1: { MigrateLegacyEntries1(&blob, 0); } [[fallthrough]];
        case (int)BlobType::Directory2: { MigrateLegacyEntries2(&blob, 0); } [[fallthrough]];
        case (int)BlobType::Directory3: { MigrateLegacyEntries3(&blob, 0); } [[fallthrough]];
        case (int)BlobType::Directory: {
            if (blob.len < K_SIZE(DirectoryHeader)) {
                LogError("Malformed directory blob '%1'", oid);
                return false;
            }

            DirectoryHeader *header = (DirectoryHeader *)blob.ptr;
            int64_t entries = LittleEndian(header->entries);

            ListContext tree(repo, settings, entries);

            if (tree.RecurseEntries(blob, false, 0, alloc, out_objects) < 0)
                return false;
        } break;

        case (int)BlobType::Snapshot1: { MigrateLegacySnapshot1(&blob); } [[fallthrough]];
        case (int)BlobType::Snapshot2: { MigrateLegacyEntries1(&blob, K_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case (int)BlobType::Snapshot3: { MigrateLegacyEntries2(&blob, K_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case (int)BlobType::Snapshot4: { MigrateLegacyEntries3(&blob, K_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case (int)BlobType::Snapshot5: { MigrateLegacySnapshot2(&blob); } [[fallthrough]];
        case (int)BlobType::Snapshot: {
            if (blob.len < K_SIZE(SnapshotHeader3) + K_SIZE(DirectoryHeader)) {
                LogError("Malformed snapshot blob '%1'", oid);
                return false;
            }

            SnapshotHeader3 *header1 = (SnapshotHeader3 *)blob.ptr;
            DirectoryHeader *header2 = (DirectoryHeader *)(header1 + 1);
            int64_t entries = LittleEndian(header2->entries);

            ListContext tree(repo, settings, entries);

            // Make sure snapshot channel is NUL terminated
            header1->channel[K_SIZE(header1->channel) - 1] = 0;

            rk_ObjectInfo *obj = out_objects->AppendDefault();

            obj->oid = oid;
            obj->type = rk_ObjectType::Snapshot;
            obj->name = DuplicateString(header1->channel, alloc).ptr;
            obj->mtime = LittleEndian(header1->time);
            obj->size = LittleEndian(header1->size);
            obj->flags |= (int)rk_ObjectFlag::Readable;
            obj->stored = LittleEndian(header1->stored) + size;
            obj->added = LittleEndian(header1->added) + (type >= (int)BlobType::Snapshot ? size : 0);

            Span<uint8_t> dir = blob.Take(K_SIZE(SnapshotHeader3), blob.len - K_SIZE(SnapshotHeader3));

            Size children = tree.RecurseEntries(dir, true, 1, alloc, out_objects);
            if (children < 0)
                return false;

            // Reacquire correct pointer (array may have moved)
            obj = &(*out_objects)[prev_len];
            obj->children = children;
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
    rk_Cache *cache;
    int64_t mark;

    uint8_t salt32[32];

    ProgressHandle pg_blobs { "Blobs" };

    std::atomic_int64_t checked_blobs = 0;

public:
    CheckContext(rk_Repository *repo, rk_Cache *cache, int64_t mark, int64_t checked);

    bool Check(const rk_ObjectID &oid, FunctionRef<bool(int, Span<const uint8_t>)> validate);

    bool CheckBlob(const rk_ObjectID &oid, FunctionRef<bool(int, Span<const uint8_t>)> validate);
    bool RecurseEntries(Span<const uint8_t> blob, bool allow_separators);

private:
    void MakeProgress(int64_t blobs);
};

CheckContext::CheckContext(rk_Repository *repo, rk_Cache *cache, int64_t mark, int64_t checked)
    : repo(repo), cache(cache), mark(mark), checked_blobs(checked)
{
    repo->MakeSalt(rk_SaltKind::BlobHash, salt32);
}

bool CheckContext::Check(const rk_ObjectID &oid, FunctionRef<bool(int, Span<const uint8_t>)> validate)
{
    // Fast path
    {
        bool valid;
        if (cache->HasCheck(oid, &valid) && valid)
            return true;
    }

    bool valid = CheckBlob(oid, validate);

    cache->PutCheck(oid, mark, valid);
    MakeProgress(1);

    return valid;
}

static void HashBlake3(int type, Span<const uint8_t> buf, const uint8_t salt[32], rk_Hash *out_hash)
{
    uint8_t salt2[32];
    MemCpy(salt2, salt, K_SIZE(salt2));
    salt2[31] ^= (uint8_t)type;

    blake3_hasher hasher;
    blake3_hasher_init_keyed(&hasher, salt2);
    blake3_hasher_update(&hasher, buf.ptr, buf.len);
    blake3_hasher_finalize(&hasher, out_hash->raw, K_SIZE(out_hash->raw));
}

bool CheckContext::CheckBlob(const rk_ObjectID &oid, FunctionRef<bool(int, Span<const uint8_t>)> validate)
{
    int type;
    HeapArray<uint8_t> blob;

    if (!repo->ReadBlob(oid, &type, &blob))
        return false;

    // Make sure hash is coherent with blob data. We need to compute the hash now, even though the error will
    // be issued later, because blob data may change when we migrate legacy blob types.
    rk_Hash hash;
    HashBlake3(type, blob, salt32, &hash);

    switch (type) {
        case (int)BlobType::Chunk: {} break;

        case (int)BlobType::File: {
            HeapArray<FileChunk> chunks;
            int64_t file_size = DecodeChunks(oid, blob, &chunks);
            if (file_size < 0)
                return false;

            Async async(repo->GetAsync());

            for (const FileChunk &chunk: chunks) {
                async.Run([=, this]() {
                    rk_ObjectID oid = { rk_BlobCatalog::Raw, chunk.hash };

                    bool valid = Check(oid, [&](int type, Span<const uint8_t> blob) {
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

                    return valid;
                });
            }

            if (!async.Sync())
                return false;
        } break;

        case (int)BlobType::Directory1: { MigrateLegacyEntries1(&blob, 0); } [[fallthrough]];
        case (int)BlobType::Directory2: { MigrateLegacyEntries2(&blob, 0); } [[fallthrough]];
        case (int)BlobType::Directory3: { MigrateLegacyEntries3(&blob, 0); } [[fallthrough]];
        case (int)BlobType::Directory: {
            if (!RecurseEntries(blob, false))
                return false;
        } break;

        case (int)BlobType::Snapshot1: { MigrateLegacySnapshot1(&blob); } [[fallthrough]];
        case (int)BlobType::Snapshot2: { MigrateLegacyEntries1(&blob, K_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case (int)BlobType::Snapshot3: { MigrateLegacyEntries2(&blob, K_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case (int)BlobType::Snapshot4: { MigrateLegacyEntries3(&blob, K_SIZE(SnapshotHeader2)); } [[fallthrough]];
        case (int)BlobType::Snapshot5: { MigrateLegacySnapshot2(&blob); } [[fallthrough]];
        case (int)BlobType::Snapshot: {
            Span<uint8_t> dir = blob.Take(K_SIZE(SnapshotHeader3), blob.len - K_SIZE(SnapshotHeader3));

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

    if (hash != oid.hash) {
        LogError("Data of blob '%1' does not match OID hash", oid);
        return false;
    }

    if (!validate(type, blob))
        return false;

    return true;
}

bool CheckContext::RecurseEntries(Span<const uint8_t> blob, bool allow_separators)
{
    BlockAllocator temp_alloc;

    if (blob.len < K_SIZE(DirectoryHeader)) [[unlikely]] {
        LogError("Malformed directory blob");
        return false;
    }

    HeapArray<EntryInfo> entries;
    if (!DecodeEntries(blob, K_SIZE(DirectoryHeader), allow_separators, &temp_alloc, &entries))
        return false;

    // Filter out invalid entries
    {
        Size j = 0;
        for (Size i = 0; i < entries.len; i++) {
            EntryInfo &entry = entries[i];
            entries[j] = entry;

            if (entry.kind == (int)RawEntry::Kind::Unknown)
                continue;
            if (!(entry.flags & (int)RawEntry::Flags::Readable))
                continue;

            j++;
        }
        entries.len = j;
    }

    Async async(repo->GetAsync());

    for (Size i = 0; i < entries.len; i++) {
        const EntryInfo *entry = &entries[i];

        async.Run([&, entry, this] () {
            rk_ObjectID oid = { KindToCatalog(entry->kind), entry->hash };

            bool valid = Check(oid, [&](int type, Span<const uint8_t>) {
                switch (entry->kind) {
                    case (int)RawEntry::Kind::Directory: {
                        if (type != (int)BlobType::Directory1 &&
                                type != (int)BlobType::Directory2 &&
                                type != (int)BlobType::Directory3 &&
                                type != (int)BlobType::Directory) {
                            LogError("Blob '%1' is not a Directory", oid);
                            return false;
                        } break;
                    } break;

                    case (int)RawEntry::Kind::File: {
                        if (type != (int)BlobType::File && type != (int)BlobType::Chunk) {
                            LogError("Blob '%1' is not a File", oid);
                            return false;
                        }
                    } break;

                    case (int)RawEntry::Kind::Link: {
                        if (type != (int)BlobType::Link && type != (int)BlobType::Chunk) {
                            LogError("Blob '%1' is not a Link", oid);
                            return false;
                        }
                    } break;

                    default: { K_UNREACHABLE(); } break;
                }

                return true;
            });

            return valid;
        });
    }

    return async.Sync();
}

void CheckContext::MakeProgress(int64_t blobs)
{
    blobs = checked_blobs.fetch_add(blobs, std::memory_order_relaxed) + blobs;
    pg_blobs.SetFmt(T("%1 blobs"), blobs);
}

bool rk_CheckSnapshots(rk_Repository *repo, Span<const rk_SnapshotInfo> snapshots, HeapArray<Size> *out_errors)
{
    BlockAllocator temp_alloc;

    rk_Cache cache;
    if (!cache.Open(repo, false))
        return false;

    int64_t mark = GetUnixTime();

    if (!cache.PruneChecks(mark - CheckDelay))
        return false;

    bool valid = true;

    // Check snapshots and blob tress
    {
        int64_t checks = cache.CountChecks();
        if (checks < 0)
            return false;

        CheckContext check(repo, &cache, mark, checks);

        ProgressHandle progress("Snapshots");
        progress.SetFmt((int64_t)0, (int64_t)snapshots.len, T("0 / %1 snapshots"), snapshots.len);

        for (Size i = 0; i < snapshots.len; i++) {
            const rk_SnapshotInfo &snapshot = snapshots[i];

            bool ret = check.Check(snapshot.oid, [&](int type, Span<const uint8_t>) {
                if (type != (int)BlobType::Snapshot1 &&
                        type != (int)BlobType::Snapshot2 &&
                        type != (int)BlobType::Snapshot3 &&
                        type != (int)BlobType::Snapshot4 &&
                        type != (int)BlobType::Snapshot5 &&
                        type != (int)BlobType::Snapshot) {
                    LogError("Blob '%1' is not a Snapshot", snapshot.oid);
                    return false;
                }

                return true;
            });

            if (!ret) {
                if (out_errors) {
                    out_errors->Append(i);
                }
                valid = false;
            }

            progress.SetFmt(i + 1, snapshots.len, T("%1 / %2 snapshots"), i + 1, snapshots.len);
        }
    }

    // Retain objects
    if (repo->CanRetain()) {
        int64_t checks = cache.CountChecks();
        if (checks < 0)
            return false;

        ProgressHandle progress("Retains");
        progress.SetFmt((int64_t)0, checks, T("0 / %1 blobs"), checks);

        Async async(repo->GetAsync());
        std::atomic_int64_t retains = 0;

        const auto retain = [&](const rk_ObjectID &oid) {
            async.Run([&, oid]() {
                if (!repo->RetainBlob(oid))
                    return false;

                int64_t value = retains.fetch_add(1, std::memory_order_relaxed) + 1;
                progress.SetFmt(value, checks, T("%1 / %2 retains"), value, checks);

                return true;
            });

            return true;
        };
        if (!cache.ListChecks(retain))
            return false;

        valid = async.Sync();
    }

    if (!cache.Close())
        return false;

    return valid;
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
    struct Buffer {
        std::mutex mutex;
        Size idx;
        HeapArray<uint8_t> data;
    };

    rk_Repository *repo;
    HeapArray<FileChunk> chunks;

    std::mutex mutex;
    Buffer buffers[4];
    int discard = 0;

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
    if (DecodeChunks(oid, blob, &chunks) < 0)
        return false;

    for (Buffer &buf: buffers) {
        buf.idx = -1;
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

        Buffer *buf = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex);

            for (Size i = 0; i < K_LEN(buffers); i++) {
                if (buffers[i].idx == idx) {
                    buf = &buffers[i];
                    buf->mutex.lock();

                    break;
                }
            }

            if (!buf) {
                buf = &buffers[discard];
                discard = (discard + 1) % K_LEN(buffers);

                buf->mutex.lock();

                buf->idx = idx;
                buf->data.RemoveFrom(0);
            }
        }

        K_DEFER { buf->mutex.unlock(); };

        if (!buf->data.len) {
            // This happens if a previous request has failed on this exact buffer
            if (buf->idx < 0) [[unlikely]] {
                LogError("Failed to read chunk");
                return false;
            }

            K_DEFER_N(err_guard) { buf->idx = -1; };

            rk_ObjectID oid = { rk_BlobCatalog::Raw, chunk.hash };
            int type;

            if (!repo->ReadBlob(oid, &type, &buf->data))
                return false;
            if (type != (int)BlobType::Chunk) [[unlikely]] {
                LogError("Blob '%1' is not a Chunk", chunk.hash);
                return false;
            }
            if (buf->data.len != chunk.len) [[unlikely]] {
                LogError("Chunk size mismatch for '%1'", chunk.hash);
                return false;
            }

            err_guard.Disable();
        }

        MemCpy(out_buf.ptr, buf->data.ptr + copy_offset, copy_len);

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
        case (int)BlobType::Directory3:
        case (int)BlobType::Directory:
        case (int)BlobType::Snapshot1:
        case (int)BlobType::Snapshot2:
        case (int)BlobType::Snapshot3:
        case (int)BlobType::Snapshot4:
        case (int)BlobType::Snapshot5:
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

    K_UNREACHABLE();
}

}
