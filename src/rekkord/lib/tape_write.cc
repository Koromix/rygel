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
#include "splitter.hh"
#include "tape.hh"
#include "xattr.hh"
#include "priv_tape.hh"
#include "vendor/blake3/c/blake3.h"

#if !defined(_WIN32)
    #include <fcntl.h>
#endif

namespace RG {

static const Size ChunkAverage = Kibibytes(2048);
static const Size ChunkMin = Kibibytes(1024);
static const Size ChunkMax = Kibibytes(8192);

static const Size FileBigSize = Mebibytes(64);
static const Size FileDefaultSize = 2 * ChunkMax;
static const int FileBigLimit = 4;

enum class PutResult {
    Success,
    Ignore,
    Error
};

class PutContext {
    rk_Repository *repo;
    sq_Database *db;
    rk_SaveSettings settings;

    uint8_t salt32[32];
    uint64_t salt8;

    ProgressHandle pg_stored { "Stored" };

    std::atomic_int64_t put_size { 0 };
    std::atomic_int64_t put_stored { 0 };
    std::atomic_int64_t put_entries { 0 };

    Async dir_tasks;
    Async file_tasks;

    std::atomic_int big_semaphore { FileBigLimit };

public:
    PutContext(rk_Repository *repo, sq_Database *db, const rk_SaveSettings &settings);

    PutResult PutDirectory(const char *src_dirname, bool follow, rk_Hash *out_hash, int64_t *out_subdirs = nullptr);
    PutResult PutFile(const char *src_filename, rk_Hash *out_hash, int64_t *out_size = nullptr, int64_t *out_stored = nullptr);

    int64_t GetSize() const { return put_size; }
    int64_t GetStored() const { return put_stored; }
    int64_t GetEntries() const { return put_entries; }

private:
    void MakeProgress(int64_t written);
};

static void HashBlake3(BlobType type, Span<const uint8_t> buf, const uint8_t salt[32], rk_Hash *out_hash)
{
    uint8_t salt2[32];
    MemCpy(salt2, salt, RG_SIZE(salt2));
    salt2[31] ^= (uint8_t)type;

    blake3_hasher hasher;
    blake3_hasher_init_keyed(&hasher, salt2);
    blake3_hasher_update(&hasher, buf.ptr, buf.len);
    blake3_hasher_finalize(&hasher, out_hash->raw, RG_SIZE(out_hash->raw));
}

static void PackExtended(const char *filename,  Span<const XAttrInfo> xattrs, HeapArray<uint8_t> *out_extended)
{
    Size prev_len = out_extended->len;
    RG_DEFER_N(err_guard) { out_extended->RemoveFrom(prev_len); };

    for (const XAttrInfo &xattr: xattrs) {
        Size key_len = strlen(xattr.key);
        Size total_len = key_len + 1 + xattr.value.len;

        if (total_len > UINT16_MAX) {
            LogWarning("Cannot store xattr '%1' for '%2': too big", xattr.key, filename);
            continue;
        }

        uint16_t len_16le = LittleEndian((uint16_t)total_len);

        out_extended->Append(MakeSpan((const uint8_t *)&len_16le, RG_SIZE(len_16le)));
        out_extended->Append(MakeSpan((const uint8_t *)xattr.key, key_len + 1));
        out_extended->Append(xattr.value);
    }

    if (out_extended->len - prev_len > INT16_MAX) {
        LogWarning("Cannot store xattrs for '%1': too big", filename);
        return;
    }

    err_guard.Disable();
}

PutContext::PutContext(rk_Repository *repo, sq_Database *db, const rk_SaveSettings &settings)
    : repo(repo), db(db), settings(settings),
      dir_tasks(repo->GetAsync()), file_tasks(repo->GetAsync())
{
    repo->MakeSalt(rk_SaltKind::BlobHash, salt32);

    // Seed the CDC splitter too
    {
        uint8_t buf[RG_SIZE(salt8)];
        repo->MakeSalt(rk_SaltKind::SplitterSeed, buf);
        MemCpy(&salt8, buf, RG_SIZE(salt8));
    }
}

PutResult PutContext::PutDirectory(const char *src_dirname, bool follow, rk_Hash *out_hash, int64_t *out_subdirs)
{
    BlockAllocator temp_alloc;

    struct PendingDirectory {
        Size parent_idx = -1;
        Size parent_entry = -1;

        const char *dirname = nullptr;
        HeapArray<uint8_t> blob;
        HeapArray<int64_t> stored;
        bool failed = false;

        std::atomic_int64_t size { 0 };
        int64_t entries = 0;
        int64_t subdirs = 0;

        rk_Hash hash = {};
    };

    Async async(&dir_tasks);
    bool success = true;

    // Reuse for performance
    HeapArray<XAttrInfo> xattrs;
    HeapArray<uint8_t> extended;

    // Enumerate directory hierarchy and process files
    BucketArray<PendingDirectory> pending_directories;
    {
        PendingDirectory *pending0 = pending_directories.AppendDefault();

        pending0->dirname = src_dirname;
        pending0->blob.AppendDefault(RG_SIZE(DirectoryHeader));

        for (Size i = 0; i < pending_directories.count; i++) {
            PendingDirectory *pending = &pending_directories[i];

            // We can't use pending->entries because if does not count non-stored entities (such as pipes)
            Size children = 0;

            const auto callback = [&](const char *basename, FileType file_type) {
                const char *filename = Fmt(&temp_alloc, "%1%/%2", pending->dirname, basename).ptr;

                RawEntry *entry = nullptr;
                bool skip = false;

                int fd = -1;
                RG_DEFER { CloseDescriptor(fd); };

                children++;

#if !defined(_WIN32)
#if defined(__linux__)
                if (settings.noatime) {
                    int flags = O_RDONLY | O_CLOEXEC | (follow ? 0 : O_NOFOLLOW) | O_NOATIME;
                    fd = open(filename, flags);
                }
#endif

                // Open file
                if (fd < 0) {
                    int flags = O_RDONLY | O_CLOEXEC | (follow ? 0 : O_NOFOLLOW);
                    fd = open(filename, flags);

                    if (fd < 0) {
                        // We cannot open symbolic links with O_NOFOLLOW but we still care about them!
                        // Most systems return ELOOP, but FreeBSD uses EMLINK.
                        bool ignore = !follow && (errno == EMLINK || errno == ELOOP);

                        if (!ignore) {
                            LogError("Cannot open '%1': %2", filename, strerror(errno));
                            skip = true;
                        }
                    }
                }

                // Read extended attributes, best effort
                if (settings.xattrs) {
                    xattrs.RemoveFrom(0);
                    extended.RemoveFrom(0);

                    if (!skip) [[likely]] {
                        ReadXAttributes(fd, filename, file_type, &temp_alloc, &xattrs);
                        PackExtended(filename, xattrs, &extended);
                    }
                }
#else
                (void)file_type;
                (void)follow;
#endif

                // Create raw entry
                {
                    Size basename_len = strlen(basename);
                    Size entry_len = RG_SIZE(RawEntry) + basename_len + extended.len;

                    entry = (RawEntry *)pending->blob.AppendDefault(entry_len);

                    entry->name_len = (uint16_t)basename_len;
                    entry->extended_len = (uint16_t)extended.len;
                    MemCpy(entry->GetName().ptr, basename, basename_len);
                    MemCpy(entry->GetExtended().ptr, extended.ptr, extended.len);
                }

                if (skip) [[unlikely]]
                    return true;

                // Stat file
                {
                    FileInfo file_info = {};
                    StatResult ret = StatFile(fd, filename, &file_info);

                    if (ret == StatResult::Success) {
                        entry->flags |= LittleEndian((int16_t)RawEntry::Flags::Stated);

                        switch (file_info.type) {
                            case FileType::Directory: {
                                entry->kind = (int16_t)RawEntry::Kind::Directory;

                                PendingDirectory *ptr = pending_directories.AppendDefault();

                                ptr->parent_idx = i;
                                ptr->parent_entry = (const uint8_t *)entry - pending->blob.ptr;
                                ptr->dirname = filename;
                                ptr->blob.AppendDefault(RG_SIZE(DirectoryHeader));

                                pending->entries++;
                                pending->subdirs++;
                            } break;

                            case FileType::File: {
                                entry->kind = (int16_t)RawEntry::Kind::File;
                                entry->size = LittleEndian(file_info.size);

                                pending->entries++;
                            } break;
#if !defined(_WIN32)
                            case FileType::Link: {
                                entry->kind = (int16_t)RawEntry::Kind::Link;
                                pending->entries++;
                            } break;
#endif

#if defined(_WIN32)
                            case FileType::Link:
#endif
                            case FileType::Device:
                            case FileType::Pipe:
                            case FileType::Socket: {
                                entry->kind = (int16_t)RawEntry::Kind::Unknown;
                                LogWarning("Ignoring special file '%1' (%2)", filename, FileTypeNames[(int)file_info.type]);
                            } break;
                        }

                        entry->mtime = LittleEndian(file_info.mtime);
                        entry->ctime = LittleEndian(file_info.ctime);
                        if (settings.atime) {
                            entry->flags |= LittleEndian((int16_t)RawEntry::Flags::AccessTime);
                            entry->atime = LittleEndian(file_info.atime);
                        }
                        entry->btime = LittleEndian(file_info.btime);
                        entry->mode = LittleEndian((uint32_t)file_info.mode);
                        entry->uid = LittleEndian(file_info.uid);
                        entry->gid = LittleEndian(file_info.gid);
                    }
                }

                return true;
            };

#if defined(__linux__)
            EnumResult ret;
            {
                int fd = settings.noatime ? open(pending->dirname, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOATIME) : -1;

                if (fd >= 0) {
                    ret = EnumerateDirectory(fd, pending->dirname, nullptr, -1, callback);
                } else {
                    ret = EnumerateDirectory(pending->dirname, nullptr, -1, callback);
                }
            }
#else
            EnumResult ret = EnumerateDirectory(pending->dirname, nullptr, -1, callback);
#endif

            if (ret != EnumResult::Success) {
                pending->failed = true;
                pending->blob.RemoveFrom(0);

                if (ret == EnumResult::AccessDenied || ret == EnumResult::MissingPath) {
                    continue;
                } else {
                    success = false;
                    break;
                }
            }

            // Don't reallocate
            pending->stored.Reserve(children);

            // Process data entries (file, links)
            for (Size offset = RG_SIZE(DirectoryHeader); offset < pending->blob.len;) {
                RawEntry *entry = (RawEntry *)(pending->blob.ptr + offset);
                int64_t *stored = pending->stored.AppendDefault();

                const char *filename = Fmt(&temp_alloc, "%1%/%2", pending->dirname, entry->GetName()).ptr;

                switch ((RawEntry::Kind)entry->kind) {
                    case RawEntry::Kind::Directory: {} break; // Already processed

                    case RawEntry::Kind::File: {
                        // Skip file analysis if metadata is unchanged
                        {
                            sq_Statement stmt;
                            if (!db->Prepare(R"(SELECT mtime, ctime, mode, size, hash, stored
                                                FROM stats
                                                WHERE path = ?1)", &stmt)) {
                                success = false;
                                break;
                            }
                            sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);

                            if (stmt.Step()) {
                                int64_t mtime = sqlite3_column_int64(stmt, 0);
                                int64_t ctime = sqlite3_column_int64(stmt, 1);
                                uint32_t mode = (uint32_t)sqlite3_column_int64(stmt, 2);
                                int64_t size = sqlite3_column_int64(stmt, 3);
                                Span<const uint8_t> hash = MakeSpan((const uint8_t *)sqlite3_column_blob(stmt, 4),
                                                                    sqlite3_column_bytes(stmt, 4));
                                int64_t written = sqlite3_column_int64(stmt, 5);

                                if (hash.len == RG_SIZE(rk_Hash) && mtime == entry->mtime &&
                                                                    ctime == entry->ctime &&
                                                                    mode == entry->mode &&
                                                                    size == entry->size) {
                                    MemCpy(&entry->hash, hash.ptr, RG_SIZE(rk_Hash));

                                    entry->flags |= LittleEndian((int16_t)RawEntry::Flags::Readable);
                                    pending->size += size;

                                    // Done by PutFile in theory, but we're skipping it
                                    put_size += size;
                                    MakeProgress(written);
                                    *stored = written;

                                    break;
                                }
                            } else if (!stmt.IsValid()) {
                                success = false;
                                break;
                            }
                        }

                        async.Run([=, this]() {
                            int64_t file_size = 0;
                            PutResult ret = PutFile(filename, &entry->hash, &file_size, stored);

                            if (ret == PutResult::Success) {
                                entry->flags |= LittleEndian((int16_t)RawEntry::Flags::Readable);
                                pending->size += file_size;

                                return true;
                            } else {
                                bool persist = (ret != PutResult::Error);
                                return persist;
                            }
                        });
                    } break;

                    case RawEntry::Kind::Link: {
#if defined(_WIN32)
                        RG_UNREACHABLE();
#else
                        async.Run([=, this]() {
                            LocalArray<uint8_t, 4096> target;
                            {
                                ssize_t ret = readlink(filename, (char *)target.data, RG_SIZE(target.data));

                                if (ret < 0) {
                                    LogError("Failed to read symbolic link '%1': %2", filename, strerror(errno));

                                    bool ignore = (errno == EACCES || errno == ENOENT);
                                    return ignore;
                                } else if (ret >= RG_SIZE(target)) {
                                    LogError("Failed to read symbolic link '%1': target too long", filename);
                                    return true;
                                }

                                target.len = (Size)ret;
                            }

                            HashBlake3(BlobType::Link, target, salt32, &entry->hash);
                            rk_ObjectID oid = { rk_BlobCatalog::Raw, entry->hash };

                            Size written = repo->WriteBlob(oid, (int)BlobType::Link, target);
                            if (written < 0)
                                return false;

                            put_size += target.len;
                            MakeProgress(written);

                            entry->flags |= LittleEndian((int16_t)RawEntry::Flags::Readable);

                            return true;
                        });
#endif
                    } break;

                    case RawEntry::Kind::Unknown: {} break;
                }

                offset += entry->GetSize();
            }
        }
    }

    if (!async.Sync())
        return PutResult::Error;
    if (!success)
        return PutResult::Error;

    // Finalize and upload directory blobs
    async.Run([&]() {
        for (Size i = pending_directories.count - 1; i >= 0; i--) {
            PendingDirectory *pending = &pending_directories[i];
            DirectoryHeader *header = (DirectoryHeader *)pending->blob.ptr;

            header->size = LittleEndian(pending->size.load());
            header->entries = LittleEndian(pending->entries);

            HashBlake3(BlobType::Directory, pending->blob, salt32, &pending->hash);

            if (pending->parent_idx >= 0) {
                PendingDirectory *parent = &pending_directories[pending->parent_idx];
                RawEntry *entry = (RawEntry *)(parent->blob.ptr + pending->parent_entry);

                entry->hash = pending->hash;
                if (!pending->failed) {
                    entry->flags |= LittleEndian((int16_t)RawEntry::Flags::Readable);
                    entry->size = LittleEndian(pending->subdirs);
                }

                parent->size += pending->size;
                parent->entries += pending->entries;
            }

            async.Run([pending, this]() mutable {
                rk_ObjectID oid = { rk_BlobCatalog::Meta, pending->hash };

                Size written = repo->WriteBlob(oid, (int)BlobType::Directory, pending->blob);
                if (written < 0)
                    return false;

                put_size += pending->blob.len;
                MakeProgress(written);

                return true;
            });
        }

        return true;
    });

    // Update cached stats
    async.Run([&]() {
        for (const PendingDirectory &pending: pending_directories) {
            if (pending.failed)
                continue;

            Size limit = pending.blob.len - RG_SIZE(int64_t);
            Size idx = 0;

            for (Size offset = RG_SIZE(DirectoryHeader); offset < limit;) {
                RawEntry *entry = (RawEntry *)(pending.blob.ptr + offset);
                int64_t stored = pending.stored[idx++];

                const char *filename = Fmt(&temp_alloc, "%1%/%2", pending.dirname, entry->GetName()).ptr;
                int flags = LittleEndian(entry->flags);

                if ((flags & (int)RawEntry::Flags::Readable) &&
                        entry->kind == (int16_t)RawEntry::Kind::File) {
                    if (!db->Run(R"(INSERT INTO stats (path, mtime, ctime, mode, size, hash, stored)
                                    VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)
                                    ON CONFLICT (path) DO UPDATE SET mtime = excluded.mtime,
                                                                     ctime = excluded.ctime,
                                                                     mode = excluded.mode,
                                                                     size = excluded.size,
                                                                     hash = excluded.hash,
                                                                     stored = excluded.stored)",
                                 filename, entry->mtime, entry->ctime, entry->mode, entry->size,
                                 MakeSpan((const uint8_t *)&entry->hash, RG_SIZE(entry->hash)), stored))
                        return false;
                }

                offset += entry->GetSize();
            }
        }

        return true;
    });

    if (!async.Sync())
        return PutResult::Error;

    const PendingDirectory &pending0 = pending_directories[0];
    RG_ASSERT(pending0.parent_idx < 0);

    put_entries += 1 + pending0.entries;

    *out_hash = pending0.hash;
    if (out_subdirs) {
        *out_subdirs = pending0.subdirs;
    }
    return PutResult::Success;
}

PutResult PutContext::PutFile(const char *src_filename, rk_Hash *out_hash, int64_t *out_size, int64_t *out_stored)
{
    StreamReader st;
    {
#if defined(__linux__)
        int fd = settings.noatime ? open(src_filename, O_RDONLY | O_CLOEXEC | O_NOATIME) : -1;

        if (fd >= 0) {
            st.Open(fd, src_filename);
            st.SetDescriptorOwned(true);
        }
#endif

        if (!st.IsValid()) {
            OpenResult ret = st.Open(src_filename);

            if (ret != OpenResult::Success) {
                bool ignore = (ret == OpenResult::AccessDenied || ret == OpenResult::MissingPath);
                return ignore ? PutResult::Ignore : PutResult::Error;
            }
        }
    }

    HeapArray<uint8_t> file_blob;
    int64_t file_size = 0;
    std::atomic_int64_t file_stored = 0;

    // Split the file
    {
        FastSplitter splitter(ChunkAverage, ChunkMin, ChunkMax, salt8);

        bool use_big_buffer = (--big_semaphore >= 0);
        RG_DEFER { big_semaphore++; };

        HeapArray<uint8_t> buf;
        if (use_big_buffer) {
            Size needed = (st.ComputeRawLen() >= 0) ? st.ComputeRawLen() : FileDefaultSize;
            needed = std::clamp(needed, ChunkMax, FileBigSize);

            buf.SetCapacity(needed);
        } else {
            buf.SetCapacity(FileDefaultSize);
        }

        do {
            Async async(&file_tasks);

            // Fill buffer
            Size read = st.Read(buf.TakeAvailable());
            if (read < 0)
                return PutResult::Error;
            buf.len += read;
            file_size += read;

            Span<const uint8_t> remain = buf;

            // We can't relocate in the inner loop
            Size needed = (remain.len / ChunkMin + 1) * RG_SIZE(RawChunk) + 8;
            file_blob.Grow(needed);

            // Chunk file and write chunks out in parallel
            do {
                Size processed = splitter.Process(remain, st.IsEOF(), [&](Size idx, int64_t total, Span<const uint8_t> chunk) {
                    RG_ASSERT(idx * RG_SIZE(RawChunk) == file_blob.len);
                    file_blob.len += RG_SIZE(RawChunk);

                    async.Run([&, idx, total, chunk]() {
                        RawChunk entry = {};

                        entry.offset = LittleEndian(total);
                        entry.len = LittleEndian((int32_t)chunk.len);

                        HashBlake3(BlobType::Chunk, chunk, salt32, &entry.hash);
                        rk_ObjectID oid = { rk_BlobCatalog::Raw, entry.hash };

                        Size written = repo->WriteBlob(oid, (int)BlobType::Chunk, chunk);
                        if (written < 0)
                            return false;

                        MakeProgress(written);
                        file_stored += written;

                        MemCpy(file_blob.ptr + idx * RG_SIZE(entry), &entry, RG_SIZE(entry));

                        return true;
                    });

                    return true;
                });
                if (processed < 0)
                    return PutResult::Error;
                if (!processed)
                    break;

                remain.ptr += processed;
                remain.len -= processed;
            } while (remain.len);

            // We don't want to run other file tasks because that could cause us to allocate way
            // too much heap memory for the fill buffer.
            if (!async.SyncSoon())
                return PutResult::Error;

            MemMove(buf.ptr, remain.ptr, remain.len);
            buf.len = remain.len;
        } while (!st.IsEOF() || buf.len);
    }

    rk_Hash file_hash = {};

    // Write list of chunks (unless there is exactly one)
    if (file_blob.len != RG_SIZE(RawChunk)) {
        int64_t len_64le = LittleEndian(st.GetRawRead());
        file_blob.Append(MakeSpan((const uint8_t *)&len_64le, RG_SIZE(len_64le)));

        HashBlake3(BlobType::File, file_blob, salt32, &file_hash);
        rk_ObjectID oid = { rk_BlobCatalog::Raw, file_hash };

        Size written = repo->WriteBlob(oid, (int)BlobType::File, file_blob);
        if (written < 0)
            return PutResult::Error;

        MakeProgress(written);
        file_stored += written;
    } else {
        const RawChunk *entry0 = (const RawChunk *)file_blob.ptr;
        file_hash = entry0->hash;
    }

    put_size += file_size;

    *out_hash = file_hash;
    if (out_size) {
        *out_size = file_size;
    }
    if (out_stored) {
        *out_stored += file_stored;
    }
    return PutResult::Success;
}

void PutContext::MakeProgress(int64_t delta)
{
    int64_t stored = put_stored.fetch_add(delta, std::memory_order_relaxed) + delta;
    pg_stored.SetFmt("%1 stored", FmtDiskSize(stored));
}

bool rk_Save(rk_Repository *repo, const rk_SaveSettings &settings, Span<const char *const> filenames,
             rk_ObjectID *out_oid, int64_t *out_size, int64_t *out_stored)
{
    BlockAllocator temp_alloc;

    RG_ASSERT(filenames.len >= 1);

    if (settings.raw) {
        if (filenames.len != 1) {
            LogError("Only one object can be saved up in raw mode");
            return false;
        }
    } else {
        RG_ASSERT(settings.channel);

        if (!settings.channel[0]) {
            LogError("Snapshot channel cannot be empty");
            return false;
        }
        if (strlen(settings.channel) > rk_MaxSnapshotChannelLength) {
            LogError("Snapshot channel '%1' is too long (limit is %2 bytes)", settings.channel, rk_MaxSnapshotChannelLength);
            return false;
        }
    }

    sq_Database *db = repo->OpenCache(true);
    if (!db)
        return false;

    uint8_t salt32[BLAKE3_KEY_LEN];
    repo->MakeSalt(rk_SaltKind::BlobHash, salt32);

    HeapArray<uint8_t> snapshot_blob;
    snapshot_blob.AppendDefault(RG_SIZE(SnapshotHeader2) + RG_SIZE(DirectoryHeader));

    PutContext put(repo, db, settings);

    // Reuse for performance
    HeapArray<XAttrInfo> xattrs;
    HeapArray<uint8_t> extended;

    rk_ObjectID oid = {};

    // Process snapshot entries
    for (const char *filename: filenames) {
        Span<char> name = NormalizePath(filename, GetWorkingDirectory(), &temp_alloc);

        if (!name.len) {
            LogError("Cannot backup empty filename");
            return false;
        }

        int fd = -1;
        RG_DEFER { CloseDescriptor(fd); };

#if defined(__linux__)
        fd = settings.noatime ? open(filename, O_RDONLY | O_CLOEXEC | O_NOATIME) : -1;
#endif

        // Open file
        if (fd < 0) {
            fd = OpenFile(filename, (int)OpenFlag::Read | (int)OpenFlag::Directory);

            if (fd < 0)
                return false;
        }

        FileInfo file_info;
        if (StatFile(fd, filename, (int)StatFlag::FollowSymlink, &file_info) != StatResult::Success)
            return false;

        if (settings.xattrs) {
            xattrs.RemoveFrom(0);
            extended.RemoveFrom(0);

            ReadXAttributes(fd, filename, file_info.type, &temp_alloc, &xattrs);
            PackExtended(filename, xattrs, &extended);
        }

        RG_ASSERT(PathIsAbsolute(name.ptr));
        RG_ASSERT(!PathContainsDotDot(name.ptr));

        Size entry_len = RG_SIZE(RawEntry) + name.len + extended.len;
        RawEntry *entry = (RawEntry *)snapshot_blob.Grow(entry_len);
        MemSet(entry, 0, entry_len);

        // Transform name (same length or shorter)
        {
            bool changed = false;

#if defined(_WIN32)
            for (char &c: name) {
                c = (c == '\\') ? '/' : c;
            }

            if (IsAsciiAlpha(name.ptr[0]) && name.ptr[1] == ':') {
                name[1] = UpperAscii(name[0]);
                name[0] = '/';

                changed = true;
            }
#endif

            name = name.Take(1, name.len - 1);

            if (changed) {
                LogWarning("Storing '%1' as '%2'", filename, name);
            }
        }

        entry->flags |= LittleEndian((int16_t)RawEntry::Flags::Stated);
        entry->name_len = LittleEndian((uint16_t)name.len);
        entry->extended_len = LittleEndian((uint16_t)extended.len);
        MemCpy(entry->GetName().ptr, name.ptr, name.len);
        MemCpy(entry->GetExtended().ptr, extended.ptr, extended.len);

        snapshot_blob.len += entry->GetSize();

        switch (file_info.type) {
            case FileType::Directory: {
                entry->kind = (int16_t)RawEntry::Kind::Directory;

                int64_t subdirs = 0;
                if (put.PutDirectory(filename, settings.follow, &entry->hash, &subdirs) != PutResult::Success)
                    return false;
                entry->size = LittleEndian(subdirs);

                entry->flags |= LittleEndian((int16_t)RawEntry::Flags::Readable);

                // Will be changed for full (non-raw) snapshots
                oid.catalog = rk_BlobCatalog::Meta;
            } break;
            case FileType::File: {
                entry->kind = (int16_t)RawEntry::Kind::File;
                entry->size = LittleEndian((uint32_t)file_info.size);

                if (put.PutFile(filename, &entry->hash) != PutResult::Success)
                    return false;

                entry->flags |= LittleEndian((int16_t)RawEntry::Flags::Readable);

                // Will be changed for full (non-raw) snapshots
                oid.catalog = rk_BlobCatalog::Raw;
            } break;

            case FileType::Link: { RG_UNREACHABLE(); } break;

            case FileType::Device:
            case FileType::Pipe:
            case FileType::Socket: {
                LogError("Cannot backup special file '%1' (%2)", filename, FileTypeNames[(int)file_info.type]);
                return false;
            } break;
        }

        entry->mtime = LittleEndian(file_info.mtime);
        entry->ctime = LittleEndian(file_info.ctime);
        if (settings.atime) {
            entry->flags |= LittleEndian((int16_t)RawEntry::Flags::AccessTime);
            entry->atime = LittleEndian(file_info.atime);
        }
        entry->btime = LittleEndian(file_info.btime);
        entry->mode = LittleEndian((uint32_t)file_info.mode);
        entry->uid = LittleEndian(file_info.uid);
        entry->gid = LittleEndian(file_info.gid);
    }

    int64_t total_size = put.GetSize();
    int64_t total_stored = put.GetStored();
    int64_t total_entries = put.GetEntries();

    if (!settings.raw) {
        SnapshotHeader2 *header1 = (SnapshotHeader2 *)snapshot_blob.ptr;
        DirectoryHeader *header2 = (DirectoryHeader *)(header1 + 1);

        header1->time = LittleEndian(GetUnixTime());
        CopyString(settings.channel, header1->channel);
        header1->size = LittleEndian(total_size);
        header1->storage = LittleEndian(total_stored);

        header2->size = LittleEndian(total_size);
        header2->entries = LittleEndian(total_entries);

        oid.catalog = rk_BlobCatalog::Meta;
        HashBlake3(BlobType::Snapshot, snapshot_blob, salt32, &oid.hash);

        // Write snapshot blob
        {
            Size written = repo->WriteBlob(oid, (int)BlobType::Snapshot, snapshot_blob);
            if (written < 0)
                return false;
            total_stored += written;
        }

        // Create tag file
        {
            Size payload_len = offsetof(SnapshotHeader2, channel) + strlen(header1->channel) + 1;
            Span<const uint8_t> payload = MakeSpan((const uint8_t *)header1, payload_len);

            if (!repo->WriteTag(oid, payload))
                return false;
        }
    } else {
        const RawEntry *entry0 = (const RawEntry *)(snapshot_blob.ptr + RG_SIZE(SnapshotHeader2) + RG_SIZE(DirectoryHeader));
        oid.hash = entry0->hash;
    }

    *out_oid = oid;
    if (out_size) {
        *out_size += total_size;
    }
    if (out_stored) {
        *out_stored += total_stored;
    }
    return true;
}

}
