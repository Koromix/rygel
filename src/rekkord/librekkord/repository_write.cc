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
#include "splitter.hh"
#include "priv_repository.hh"
#include "vendor/blake3/c/blake3.h"

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
    rk_Disk *disk;
    sq_Database *db;

    Span<const uint8_t> salt;
    uint64_t salt64;

    std::atomic<int64_t> stat_len { 0 };
    std::atomic<int64_t> stat_written { 0 };

    Async dir_async;
    Async file_async;

    std::atomic_int big_semaphore { FileBigLimit };

public:
    PutContext(rk_Disk *disk, sq_Database *db);

    PutResult PutDirectory(const char *src_dirname, bool follow_symlinks, rk_Hash *out_hash, int64_t *out_subdirs = nullptr);
    PutResult PutFile(const char *src_filename, rk_Hash *out_hash, int64_t *out_len = nullptr);

    int64_t GetLen() const { return stat_len; }
    int64_t GetWritten() const { return stat_written; }
};

static void HashBlake3(rk_BlobType type, Span<const uint8_t> buf, const uint8_t salt[32], rk_Hash *out_hash)
{
    blake3_hasher hasher;

    uint8_t salt2[32];
    MemCpy(salt2, salt, RG_SIZE(salt2));
    salt2[31] ^= (uint8_t)type;

    blake3_hasher_init_keyed(&hasher, salt2);
    blake3_hasher_update(&hasher, buf.ptr, buf.len);
    blake3_hasher_finalize(&hasher, out_hash->hash, RG_SIZE(out_hash->hash));
}

PutContext::PutContext(rk_Disk *disk, sq_Database *db)
    : disk(disk), db(db), salt(disk->GetSalt()),
      dir_async(disk->GetThreads()), file_async(disk->GetThreads())
{
    RG_ASSERT(salt.len == BLAKE3_KEY_LEN); // 32 bytes
    MemCpy(&salt64, salt.ptr, RG_SIZE(salt64));
}

PutResult PutContext::PutDirectory(const char *src_dirname, bool follow_symlinks, rk_Hash *out_hash, int64_t *out_subdirs)
{
    BlockAllocator temp_alloc;

    struct PendingDirectory {
        Size parent_idx = -1;
        Size parent_entry = -1;

        const char *dirname = nullptr;
        HeapArray<uint8_t> blob;
        bool failed = false;

        std::atomic_int64_t total_len { 0 };
        int64_t subdirs = 0;

        rk_Hash hash = {};
    };

    Async async(&dir_async);
    bool success = true;

    // Enumerate directory hierarchy and process files
    BucketArray<PendingDirectory> pending_directories;
    {
        PendingDirectory *pending0 = pending_directories.AppendDefault();
        pending0->dirname = src_dirname;

        for (Size i = 0; i < pending_directories.count; i++) {
            PendingDirectory *pending = &pending_directories[i];

            EnumResult ret = EnumerateDirectory(pending->dirname, nullptr, -1,
                                                [&](const char *basename, FileType) {
                const char *filename = Fmt(&temp_alloc, "%1%/%2", pending->dirname, basename).ptr;

                Size basename_len = strlen(basename);
                Size entry_len = RG_SIZE(RawFile) + basename_len;

                RawFile *entry = (RawFile *)pending->blob.AppendDefault(entry_len);

                // Stat file
                {
                    unsigned int flags = follow_symlinks ? (int)StatFlag::FollowSymlink : 0;

                    FileInfo file_info = {};
                    StatResult ret = StatFile(filename, flags, &file_info);

                    if (ret == StatResult::Success) {
                        entry->flags |= LittleEndian((int16_t)RawFile::Flags::Stated);

                        switch (file_info.type) {
                            case FileType::Directory: {
                                entry->kind = (int16_t)RawFile::Kind::Directory;

                                PendingDirectory *ptr = pending_directories.AppendDefault();

                                ptr->parent_idx = i;
                                ptr->parent_entry = (const uint8_t *)entry - pending->blob.ptr;
                                ptr->dirname = filename;

                                pending->subdirs++;
                            } break;

                            case FileType::File: {
                                entry->kind = (int16_t)RawFile::Kind::File;
                                entry->size = LittleEndian(file_info.size);
                            } break;
#if !defined(_WIN32)
                            case FileType::Link: { entry->kind = (int16_t)RawFile::Kind::Link; } break;
#endif

#if defined(_WIN32)
                            case FileType::Link:
#endif
                            case FileType::Device:
                            case FileType::Pipe:
                            case FileType::Socket: {
                                entry->kind = (int16_t)RawFile::Kind::Unknown;
                                LogWarning("Ignoring special file '%1' (%2)", filename, FileTypeNames[(int)file_info.type]);
                            } break;
                        }

                        entry->mtime = LittleEndian(file_info.mtime);
                        entry->btime = LittleEndian(file_info.btime);
                        entry->mode = LittleEndian((uint32_t)file_info.mode);
                        entry->uid = LittleEndian(file_info.uid);
                        entry->gid = LittleEndian(file_info.gid);
                    }
                }

                entry->name_len = (int16_t)basename_len;
                MemCpy(entry->GetName().ptr, basename, basename_len);

                return true;
            });

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

            // Process data entries (file, links)
            for (Size offset = 0; offset < pending->blob.len;) {
                RawFile *entry = (RawFile *)(pending->blob.ptr + offset);

                const char *filename = Fmt(&temp_alloc, "%1%/%2", pending->dirname, entry->GetName()).ptr;

                switch ((RawFile::Kind)entry->kind) {
                    case RawFile::Kind::Directory: {} break; // Already processed

                    case RawFile::Kind::File: {
                        // Skip file analysis if metadata is unchanged
                        {
                            sq_Statement stmt;
                            if (!db->Prepare("SELECT mtime, btime, mode, size, hash FROM stats WHERE path = ?1", &stmt)) {
                                success = false;
                                break;
                            }
                            sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);

                            if (stmt.Step()) {
                                int64_t mtime = sqlite3_column_int64(stmt, 0);
                                int64_t btime = sqlite3_column_int64(stmt, 1);
                                uint32_t mode = (uint32_t)sqlite3_column_int64(stmt, 2);
                                int64_t size = sqlite3_column_int64(stmt, 3);
                                Span<const uint8_t> hash = MakeSpan((const uint8_t *)sqlite3_column_blob(stmt, 4),
                                                                    sqlite3_column_bytes(stmt, 4));

                                if (hash.len == RG_SIZE(rk_Hash) && mtime == entry->mtime &&
                                                                    btime == entry->btime &&
                                                                    mode == entry->mode &&
                                                                    size == entry->size) {
                                    MemCpy(&entry->hash, hash.ptr, RG_SIZE(rk_Hash));

                                    entry->flags |= LittleEndian((int16_t)RawFile::Flags::Readable);
                                    pending->total_len += size;

                                    // Done by PutFile in theory, but we're skipping it
                                    stat_len += size;

                                    break;
                                }
                            } else if (!stmt.IsValid()) {
                                success = false;
                                break;
                            }
                        }

                        async.Run([=, this]() {
                            int64_t file_len = 0;
                            PutResult ret = PutFile(filename, &entry->hash, &file_len);

                            if (ret == PutResult::Success) {
                                entry->flags |= LittleEndian((int16_t)RawFile::Flags::Readable);
                                pending->total_len += file_len;

                                return true;
                            } else {
                                bool persist = (ret != PutResult::Error);
                                return persist;
                            }
                        });
                    } break;

                    case RawFile::Kind::Link: {
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

                            HashBlake3(rk_BlobType::Link, target, salt.ptr, &entry->hash);

                            Size ret = disk->WriteBlob(entry->hash, rk_BlobType::Link, target);
                            if (ret < 0)
                                return false;
                            stat_written += ret;

                            stat_len += target.len;

                            entry->flags |= LittleEndian((int16_t)RawFile::Flags::Readable);

                            return true;
                        });
#endif
                    } break;

                    case RawFile::Kind::Unknown: {} break;
                }

                offset += entry->GetSize();
            }
        }
    }

    if (!async.Sync())
        return PutResult::Error;
    if (!success)
        return PutResult::Error;

    // We won't be able to resize/reallocate in async tasks
    // Make sure there is room for the length at the end!
    for (PendingDirectory &pending: pending_directories) {
        Size needed = RG_SIZE(int64_t);
        pending.blob.AppendDefault(needed);
    }

    // Finalize and upload directory blobs
    async.Run([&]() {
        for (Size i = pending_directories.count - 1; i >= 0; i--) {
            PendingDirectory *pending = &pending_directories[i];

            int64_t len_64le = LittleEndian(pending->total_len.load());
            MemCpy(pending->blob.end() - RG_SIZE(len_64le), &len_64le, RG_SIZE(len_64le));

            HashBlake3(rk_BlobType::Directory, pending->blob, salt.ptr, &pending->hash);

            if (pending->parent_idx >= 0) {
                PendingDirectory *parent = &pending_directories[pending->parent_idx];
                RawFile *entry = (RawFile *)(parent->blob.ptr + pending->parent_entry);

                entry->hash = pending->hash;
                if (!pending->failed) {
                    entry->flags |= LittleEndian((int16_t)RawFile::Flags::Readable);
                    entry->size = LittleEndian(pending->subdirs);
                }

                parent->total_len += pending->total_len;
            }

            async.Run([pending, this]() mutable {
                Size written = disk->WriteBlob(pending->hash, rk_BlobType::Directory, pending->blob);
                if (written < 0)
                    return false;
                stat_written += written;

                stat_len += pending->blob.len;

                return true;
            });
        }

        return true;
    });

    // Update cached stats
    async.Run([&]() {
        bool success = db->Transaction([&]() {
            for (const PendingDirectory &pending: pending_directories) {
                if (pending.failed)
                    continue;

                Size limit = pending.blob.len - RG_SIZE(int64_t);

                for (Size offset = 0; offset < limit;) {
                    RawFile *entry = (RawFile *)(pending.blob.ptr + offset);

                    const char *filename = Fmt(&temp_alloc, "%1%/%2", pending.dirname, entry->GetName()).ptr;
                    int flags = LittleEndian(entry->flags);

                    if ((flags & (int)RawFile::Flags::Readable) &&
                            entry->kind == (int16_t)RawFile::Kind::File) {
                        if (!db->Run(R"(INSERT INTO stats (path, mtime, btime, mode, size, hash)
                                            VALUES (?1, ?2, ?3, ?4, ?5, ?6)
                                            ON CONFLICT (path) DO UPDATE SET mtime = excluded.mtime,
                                                                             btime = excluded.btime,
                                                                             mode = excluded.mode,
                                                                             size = excluded.size,
                                                                             hash = excluded.hash)",
                                     filename, entry->mtime, entry->btime, entry->mode, entry->size,
                                     MakeSpan((const uint8_t *)&entry->hash, RG_SIZE(entry->hash))))
                            return false;
                    }

                    offset += entry->GetSize();
                }
            }

            return true;
        });
        return success;
    });

    if (!async.Sync())
        return PutResult::Error;

    RG_ASSERT(pending_directories[0].parent_idx < 0);

    *out_hash = pending_directories[0].hash;
    if (out_subdirs) {
        *out_subdirs = pending_directories[0].subdirs;
    }
    return PutResult::Success;
}

PutResult PutContext::PutFile(const char *src_filename, rk_Hash *out_hash, int64_t *out_len)
{
    StreamReader st;
    {
        OpenResult ret = st.Open(src_filename);
        if (ret != OpenResult::Success) {
            bool ignore = (ret == OpenResult::AccessDenied || ret == OpenResult::MissingPath);
            return ignore ? PutResult::Ignore : PutResult::Error;
        }
    }

    HeapArray<uint8_t> file_blob;
    Size file_len = 0;

    // Split the file
    {
        rk_Splitter splitter(ChunkAverage, ChunkMin, ChunkMax, salt64);

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
            Async async(&file_async);

            // Fill buffer
            Size read = st.Read(buf.TakeAvailable());
            if (read < 0)
                return PutResult::Error;
            buf.len += read;
            file_len += read;

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

                        HashBlake3(rk_BlobType::Chunk, chunk, salt.ptr, &entry.hash);

                        Size ret = disk->WriteBlob(entry.hash, rk_BlobType::Chunk, chunk);
                        if (ret < 0)
                            return false;
                        stat_written += ret;

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

            if (!async.Sync())
                return PutResult::Error;

            MemMove(buf.ptr, remain.ptr, remain.len);
            buf.len = remain.len;
        } while (!st.IsEOF() || buf.len);
    }

    // Write list of chunks (unless there is exactly one)
    rk_Hash file_hash = {};
    if (file_blob.len != RG_SIZE(RawChunk)) {
        int64_t len_64le = LittleEndian(st.GetRawRead());
        file_blob.Append(MakeSpan((const uint8_t *)&len_64le, RG_SIZE(len_64le)));

        HashBlake3(rk_BlobType::File, file_blob, salt.ptr, &file_hash);

        Size ret = disk->WriteBlob(file_hash, rk_BlobType::File, file_blob);
        if (ret < 0)
            return PutResult::Error;
        stat_written += ret;
    } else {
        const RawChunk *entry0 = (const RawChunk *)file_blob.ptr;
        file_hash = entry0->hash;
    }

    stat_len += file_len;

    *out_hash = file_hash;
    if (out_len) {
        *out_len = file_len;
    }
    return PutResult::Success;
}

bool rk_Put(rk_Disk *disk, const rk_PutSettings &settings, Span<const char *const> filenames,
            rk_Hash *out_hash, int64_t *out_len, int64_t *out_written)
{
    BlockAllocator temp_alloc;

    RG_ASSERT(filenames.len >= 1);

    if (settings.raw) {
        if (settings.name) {
            LogError("Cannot use snapshot name in raw mode");
            return false;
        }
        if (filenames.len != 1) {
            LogError("Only one object can be saved up in raw mode");
            return false;
        }
    } else {
        if (!settings.name || !settings.name[0]) {
            LogError("Snapshot name cannot be empty");
            return false;
        }
        if (strlen(settings.name) >= RG_SIZE(SnapshotHeader2::name)) {
            LogError("Snapshot name '%1' is too long (limit is %2 bytes)", settings.name, RG_SIZE(SnapshotHeader2::name));
            return false;
        }
    }

    sq_Database *db = disk->OpenCache();
    if (!db)
        return false;

    Span<const uint8_t> salt = disk->GetSalt();
    RG_ASSERT(salt.len == BLAKE3_KEY_LEN); // 32 bytes

    HeapArray<uint8_t> snapshot_blob;
    snapshot_blob.AppendDefault(RG_SIZE(SnapshotHeader2));

    PutContext put(disk, db);

    // Process snapshot entries
    for (const char *filename: filenames) {
        Span<char> name = NormalizePath(filename, GetWorkingDirectory(), &temp_alloc);

        if (!name.len) {
            LogError("Cannot backup empty filename");
            return false;
        }

        RG_ASSERT(PathIsAbsolute(name.ptr));
        RG_ASSERT(!PathContainsDotDot(name.ptr));

        Size entry_len = RG_SIZE(RawFile) + name.len;
        RawFile *entry = (RawFile *)snapshot_blob.Grow(entry_len);
        MemSet(entry, 0, entry_len);

        // Transform name (same length or shorter)
        {
            bool changed = false;

#if defined(_WIN32)
            for (char &c: name) {
                c = (c == '\\') ? '/' : c;
            }

            if (IsAsciiAlpha(name.ptr[0]) && name.ptr[1] == ':') {
                name[1] = LowerAscii(name[0]);
                name[0] = '/';

                changed = true;
            }
#endif

            name = name.Take(1, name.len - 1);

            entry->name_len = (int16_t)name.len;
            MemCpy(entry->GetName().ptr, name.ptr, name.len);

            if (changed) {
                LogWarning("Storing '%1' as '%2'", filename, entry->GetName());
            }
        }

        snapshot_blob.len += entry->GetSize();

        FileInfo file_info;
        if (StatFile(filename, (int)StatFlag::FollowSymlink, &file_info) != StatResult::Success)
            return false;
        entry->flags |= LittleEndian((int16_t)RawFile::Flags::Stated);

        switch (file_info.type) {
            case FileType::Directory: {
                entry->kind = (int16_t)RawFile::Kind::Directory;

                int64_t subdirs = 0;
                if (put.PutDirectory(filename, settings.follow_symlinks, &entry->hash, &subdirs) != PutResult::Success)
                    return false;
                entry->size = LittleEndian(subdirs);

                entry->flags |= LittleEndian((int16_t)RawFile::Flags::Readable);
            } break;
            case FileType::File: {
                entry->kind = (int16_t)RawFile::Kind::File;
                entry->size = LittleEndian((uint32_t)file_info.size);

                if (put.PutFile(filename, &entry->hash) != PutResult::Success)
                    return false;

                entry->flags |= LittleEndian((int16_t)RawFile::Flags::Readable);
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
        entry->btime = LittleEndian(file_info.btime);
        entry->mode = LittleEndian((uint32_t)file_info.mode);
        entry->uid = LittleEndian(file_info.uid);
        entry->gid = LittleEndian(file_info.gid);
    }

    int64_t total_len = put.GetLen();
    int64_t total_written = put.GetWritten();

    rk_Hash hash = {};
    if (!settings.raw) {
        SnapshotHeader2 *header = (SnapshotHeader2 *)snapshot_blob.ptr;

        header->time = LittleEndian(GetUnixTime());
        CopyString(settings.name, header->name);
        header->len = LittleEndian(total_len);
        header->stored = LittleEndian(total_written);

        int64_t len_64le = LittleEndian(total_len);
        snapshot_blob.Append(MakeSpan((const uint8_t *)&len_64le, RG_SIZE(len_64le)));

        HashBlake3(rk_BlobType::Snapshot2, snapshot_blob, salt.ptr, &hash);

        // Write snapshot blob
        {
            Size ret = disk->WriteBlob(hash, rk_BlobType::Snapshot2, snapshot_blob);
            if (ret < 0)
                return false;
            total_written += ret;
        }

        // Create tag file
        {
            Size payload_len = offsetof(SnapshotHeader2, name) + strlen(header->name) + 1;
            Span<const uint8_t> payload = MakeSpan((const uint8_t *)header, payload_len);

            Size ret = disk->WriteTag(hash, payload);
            if (ret < 0)
                return false;
            total_written += ret;
        }
    } else {
        const RawFile *entry = (const RawFile *)(snapshot_blob.ptr + RG_SIZE(SnapshotHeader2));
        hash = entry->hash;
    }

    *out_hash = hash;
    if (out_len) {
        *out_len += total_len;
    }
    if (out_written) {
        *out_written += total_written;
    }
    return true;
}

}
