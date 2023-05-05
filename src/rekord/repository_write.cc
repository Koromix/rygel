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

#include "src/core/libcc/libcc.hh"
#include "disk.hh"
#include "repository.hh"
#include "splitter.hh"
#include "vendor/blake3/c/blake3.h"

#ifndef _WIN32
    #include <unistd.h>
#endif

namespace RG {

static const Size ChunkAverage = Kibibytes(2048);
static const Size ChunkMin = Kibibytes(1024);
static const Size ChunkMax = Kibibytes(8192);

enum class PutResult {
    Success,
    Ignore,
    Error
};

class PutContext {
    rk_Disk *disk;

    Span<const uint8_t> salt;
    uint64_t salt64;

    std::atomic<int64_t> stat_len { 0 };
    std::atomic<int64_t> stat_written { 0 };

    Async dir_async;
    Async file_async;

public:
    PutContext(rk_Disk *disk);

    PutResult PutDirectory(const char *src_dirname, bool follow_symlinks, rk_ID *out_id);
    PutResult PutFile(const char *src_filename, rk_ID *out_id, int64_t *out_len = nullptr);

    int64_t GetLen() const { return stat_len; }
    int64_t GetWritten() const { return stat_written; }
};

static void HashBlake3(rk_ObjectType type, Span<const uint8_t> buf, const uint8_t salt[32], rk_ID *out_id)
{
    blake3_hasher hasher;

    uint8_t salt2[32];
    memcpy(salt2, salt, RG_SIZE(salt2));
    salt2[31] ^= (uint8_t)type;

    blake3_hasher_init_keyed(&hasher, salt2);
    blake3_hasher_update(&hasher, buf.ptr, buf.len);
    blake3_hasher_finalize(&hasher, out_id->hash, RG_SIZE(out_id->hash));
}

PutContext::PutContext(rk_Disk *disk)
    : disk(disk), salt(disk->GetSalt()),
      dir_async(disk->GetThreads()), file_async(disk->GetThreads())
{
    RG_ASSERT(salt.len == BLAKE3_KEY_LEN); // 32 bytes
    memcpy(&salt64, salt.ptr, RG_SIZE(salt64));
}

PutResult PutContext::PutDirectory(const char *src_dirname, bool follow_symlinks, rk_ID *out_id)
{
    BlockAllocator temp_alloc;

    sq_Database *db = disk->GetCache();

    struct PendingDirectory {
        Size parent_idx = -1;
        Size parent_entry = -1;

        const char *dirname = nullptr;
        HeapArray<uint8_t> obj;
        bool failed = false;

        std::atomic_int64_t total_len { 0 };

        rk_ID id = {};
    };

    Async async(&dir_async);
    bool success = true;

    // Enumerate directory hierarchy and process files
    BucketArray<PendingDirectory> pending_directories;
    {
        PendingDirectory *pending0 = pending_directories.AppendDefault();
        pending0->dirname = src_dirname;

        for (Size i = 0 ; i < pending_directories.len; i++) {
            PendingDirectory *pending = &pending_directories[i];

            EnumResult ret = EnumerateDirectory(pending->dirname, nullptr, -1,
                                                [&](const char *basename, FileType) {
                const char *filename = Fmt(&temp_alloc, "%1%/%2", pending->dirname, basename).ptr;

                Size entry_len = RG_SIZE(rk_FileEntry) + strlen(basename) + 1;
                rk_FileEntry *entry = (rk_FileEntry *)pending->obj.AppendDefault(entry_len);

                // Stat file
                {
                    unsigned int flags = follow_symlinks ? (int)StatFlag::FollowSymlink : 0;

                    FileInfo file_info = {};
                    StatResult ret = StatFile(filename, flags, &file_info);

                    if (ret == StatResult::Success) {
                        entry->stated = true;

                        switch (file_info.type) {
                            case FileType::Directory: {
                                entry->kind = (int8_t)rk_FileEntry::Kind::Directory;

                                PendingDirectory *ptr = pending_directories.AppendDefault();

                                ptr->parent_idx = i;
                                ptr->parent_entry = (const uint8_t *)entry - pending->obj.ptr;
                                ptr->dirname = filename;
                            } break;

                            case FileType::File: {
                                entry->kind = (int8_t)rk_FileEntry::Kind::File;
                                entry->size = LittleEndian(file_info.size);
                            } break;
#ifndef _WIN32
                            case FileType::Link: { entry->kind = (int8_t)rk_FileEntry::Kind::Link; } break;
#endif

#ifdef _WIN32
                            case FileType::Link:
#endif
                            case FileType::Device:
                            case FileType::Pipe:
                            case FileType::Socket: {
                                entry->kind = (int8_t)rk_FileEntry::Kind::Unknown;
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

                CopyString(basename, MakeSpan(entry->name, entry_len - RG_SIZE(rk_FileEntry)));

                return true;
            });

            if (ret != EnumResult::Success) {
                pending->failed = true;
                pending->obj.RemoveFrom(0);

                if (ret == EnumResult::AccessDenied || ret == EnumResult::MissingPath) {
                    continue;
                } else {
                    success = false;
                    break;
                }
            }

            // Process data entries (file, links)
            for (Size offset = 0; offset < pending->obj.len;) {
                rk_FileEntry *entry = (rk_FileEntry *)(pending->obj.ptr + offset);
                const char *filename = Fmt(&temp_alloc, "%1%/%2", pending->dirname, entry->name).ptr;

                switch ((rk_FileEntry::Kind)entry->kind) {
                    case rk_FileEntry::Kind::Directory: {} break; // Already processed

                    case rk_FileEntry::Kind::File: {
                        // Skip file analysis if metadata is unchanged
                        {
                            sq_Statement stmt;
                            if (!db->Prepare("SELECT mtime, mode, size, id FROM stats WHERE path = ?1", &stmt)) {
                                success = false;
                                break;
                            }
                            sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);

                            if (stmt.Step()) {
                                int64_t mtime = sqlite3_column_int64(stmt, 0);
                                uint32_t mode = (uint32_t)sqlite3_column_int64(stmt, 1);
                                int64_t size = sqlite3_column_int64(stmt, 2);
                                Span<const uint8_t> id = MakeSpan((const uint8_t *)sqlite3_column_blob(stmt, 3),
                                                                  sqlite3_column_bytes(stmt, 3));

                                if (id.len == RG_SIZE(rk_ID) && mtime == LittleEndian(entry->mtime) &&
                                                                mode == LittleEndian(entry->mode) &&
                                                                size == LittleEndian(entry->size)) {
                                    memcpy(&entry->id, id.ptr, RG_SIZE(rk_ID));

                                    entry->readable = true;
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
                            Size file_len = 0;
                            PutResult ret = PutFile(filename, &entry->id, &file_len);

                            if (ret == PutResult::Success) {
                                entry->readable = true;
                                pending->total_len += file_len;

                                return true;
                            } else {
                                bool persist = (ret != PutResult::Error);
                                return persist;
                            }
                        });
                    } break;

                    case rk_FileEntry::Kind::Link: {
#ifdef _WIN32
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

                            HashBlake3(rk_ObjectType::Link, target, salt.ptr, &entry->id);

                            Size ret = disk->WriteObject(entry->id, rk_ObjectType::Link, target);
                            if (ret < 0)
                                return false;
                            stat_written += ret;

                            stat_len += target.len;

                            entry->readable = true;

                            return true;
                        });
#endif
                    } break;

                    case rk_FileEntry::Kind::Unknown: {} break;
                }

                Size entry_len = RG_SIZE(rk_FileEntry) + strlen(entry->name) + 1;
                offset += entry_len;
            }
        }
    }

    if (!async.Sync())
        return PutResult::Error;
    if (!success)
        return PutResult::Error;

    // Finalize and upload directory objects
    async.Run([&]() {
        for (Size i = pending_directories.len - 1; i >= 0; i--) {
            PendingDirectory *pending = &pending_directories[i];

            int64_t len_64le = LittleEndian(pending->total_len.load());
            pending->obj.Append(MakeSpan((const uint8_t *)&len_64le, RG_SIZE(len_64le)));

            HashBlake3(rk_ObjectType::Directory, pending->obj, salt.ptr, &pending->id);

            if (pending->parent_idx >= 0) {
                PendingDirectory *parent = &pending_directories[pending->parent_idx];
                rk_FileEntry *entry = (rk_FileEntry *)(parent->obj.ptr + pending->parent_entry);

                entry->id = pending->id;
                entry->readable = !pending->failed;

                parent->total_len += pending->total_len;
            }

            async.Run([pending, this]() mutable {
                Size written = disk->WriteObject(pending->id, rk_ObjectType::Directory, pending->obj);
                if (written < 0)
                    return false;
                stat_written += written;

                stat_len += pending->obj.len;

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

                Size limit = pending.obj.len - RG_SIZE(int64_t);

                for (Size offset = 0; offset < limit;) {
                    rk_FileEntry *entry = (rk_FileEntry *)(pending.obj.ptr + offset);
                    const char *filename = Fmt(&temp_alloc, "%1%/%2", pending.dirname, entry->name).ptr;

                    if (entry->readable && entry->kind == (int8_t)rk_FileEntry::Kind::File) {
                        if (!db->Run(R"(INSERT INTO stats (path, mtime, mode, size, id)
                                            VALUES (?1, ?2, ?3, ?4, ?5)
                                            ON CONFLICT (path) DO UPDATE SET mtime = excluded.mtime,
                                                                             mode = excluded.mode,
                                                                             size = excluded.size,
                                                                             id = excluded.id)",
                                     filename, entry->mtime, entry->mode, entry->size,
                                     MakeSpan((const uint8_t *)&entry->id, RG_SIZE(entry->id))))
                            return false;
                    }

                    Size entry_len = RG_SIZE(rk_FileEntry) + strlen(entry->name) + 1;
                    offset += entry_len;
                }
            }

            return true;
        });
        return success;
    });

    if (!async.Sync())
        return PutResult::Error;

    rk_ID dir_id = pending_directories[0].id;
    RG_ASSERT(pending_directories[0].parent_idx < 0);

    *out_id = dir_id;
    return PutResult::Success;
}

PutResult PutContext::PutFile(const char *src_filename, rk_ID *out_id, int64_t *out_len)
{
    StreamReader st;
    {
        OpenResult ret = st.Open(src_filename);
        if (ret != OpenResult::Success) {
            bool ignore = (ret == OpenResult::AccessDenied || ret == OpenResult::MissingPath);
            return ignore ? PutResult::Ignore : PutResult::Error;
        }
    }

    HeapArray<uint8_t> file_obj;
    Size file_len = 0;

    // Split the file
    {
        rk_Splitter splitter(ChunkAverage, ChunkMin, ChunkMax, salt64);

        HeapArray<uint8_t> buf;
        {
            Size needed = (st.ComputeRawLen() >= 0) ? st.ComputeRawLen() : Mebibytes(16);
            needed = std::clamp(needed, ChunkMax, Mebibytes(128));

            buf.SetCapacity(needed);
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
            Size needed = (remain.len / ChunkMin + 1) * RG_SIZE(rk_ChunkEntry) + 8;
            file_obj.Grow(needed);

            // Chunk file and write chunks out in parallel
            do {
                Size processed = splitter.Process(remain, st.IsEOF(), [&](Size idx, int64_t total, Span<const uint8_t> chunk) {
                    RG_ASSERT(idx * RG_SIZE(rk_ChunkEntry) == file_obj.len);
                    file_obj.len += RG_SIZE(rk_ChunkEntry);

                    async.Run([&, idx, total, chunk]() {
                        rk_ChunkEntry entry = {};

                        entry.offset = LittleEndian(total);
                        entry.len = LittleEndian((int32_t)chunk.len);

                        HashBlake3(rk_ObjectType::Chunk, chunk, salt.ptr, &entry.id);

                        Size ret = disk->WriteObject(entry.id, rk_ObjectType::Chunk, chunk);
                        if (ret < 0)
                            return false;
                        stat_written += ret;

                        memcpy(file_obj.ptr + idx * RG_SIZE(entry), &entry, RG_SIZE(entry));

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

            memmove_safe(buf.ptr, remain.ptr, remain.len);
            buf.len = remain.len;
        } while (!st.IsEOF() || buf.len);
    }

    // Write list of chunks (unless there is exactly one)
    rk_ID file_id = {};
    if (file_obj.len != RG_SIZE(rk_ChunkEntry)) {
        int64_t len_64le = LittleEndian(st.GetRawRead());
        file_obj.Append(MakeSpan((const uint8_t *)&len_64le, RG_SIZE(len_64le)));

        HashBlake3(rk_ObjectType::File, file_obj, salt.ptr, &file_id);

        Size ret = disk->WriteObject(file_id, rk_ObjectType::File, file_obj);
        if (ret < 0)
            return PutResult::Error;
        stat_written += ret;
    } else {
        const rk_ChunkEntry *entry0 = (const rk_ChunkEntry *)file_obj.ptr;
        file_id = entry0->id;
    }

    stat_len += file_len;

    *out_id = file_id;
    if (out_len) {
        *out_len = file_len;
    }
    return PutResult::Success;
}

bool rk_Put(rk_Disk *disk, const rk_PutSettings &settings, Span<const char *const> filenames,
            rk_ID *out_id, int64_t *out_len, int64_t *out_written)
{
    BlockAllocator temp_alloc;

    RG_ASSERT(filenames.len >= 1);

    if (settings.raw && settings.name) {
        LogError("Cannot use snapshot name in raw mode");
        return false;
    }
    if (settings.raw && filenames.len != 1) {
        LogError("Only one object can be backup up in raw mode");
        return false;
    }
    if (settings.name && strlen(settings.name) >= RG_SIZE(rk_SnapshotHeader::name)) {
        LogError("Snapshot name '%1' is too long (limit is %2 bytes)", settings.name, RG_SIZE(rk_SnapshotHeader::name));
        return false;
    }

    Span<const uint8_t> salt = disk->GetSalt();
    RG_ASSERT(salt.len == BLAKE3_KEY_LEN); // 32 bytes

    HeapArray<uint8_t> snapshot_obj;
    rk_SnapshotHeader *header = (rk_SnapshotHeader *)snapshot_obj.AppendDefault(RG_SIZE(rk_SnapshotHeader));

    CopyString(settings.name ? settings.name : "", header->name);
    header->time = LittleEndian(GetUnixTime());

    PutContext put(disk);

    // Process snapshot entries
    for (const char *filename: filenames) {
        Span<char> name = NormalizePath(filename, GetWorkingDirectory(), &temp_alloc);

        if (!name.len) {
            LogError("Cannot backup empty filename");
            return false;
        }

        RG_ASSERT(PathIsAbsolute(name.ptr));
        RG_ASSERT(!PathContainsDotDot(name.ptr));

        Size entry_len = RG_SIZE(rk_FileEntry) + name.len + 1;
        rk_FileEntry *entry = (rk_FileEntry *)snapshot_obj.Grow(entry_len);

        // Transform name (same length or shorter)
        {
            bool changed = false;

#ifdef _WIN32
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

            memcpy(entry->name, name.ptr, name.len);
            entry->name[name.len] = 0;

            if (changed) {
                LogWarning("Storing '%1' as '%2'", filename, entry->name);
            }
        }

        // Adjust entry length
        entry_len = RG_SIZE(rk_FileEntry) + name.len + 1;
        snapshot_obj.len += entry_len;

        FileInfo file_info;
        if (StatFile(filename, (int)StatFlag::FollowSymlink, &file_info) != StatResult::Success)
            return false;
        entry->stated = true;

        switch (file_info.type) {
            case FileType::Directory: {
                entry->kind = (int8_t)rk_FileEntry::Kind::Directory;

                if (put.PutDirectory(filename, settings.follow_symlinks, &entry->id) != PutResult::Success)
                    return false;

                entry->readable = true;
            } break;
            case FileType::File: {
                entry->kind = (int8_t)rk_FileEntry::Kind::File;
                entry->size = LittleEndian((uint32_t)file_info.size);

                if (put.PutFile(filename, &entry->id) != PutResult::Success)
                    return false;

                entry->readable = true;
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

    rk_ID id = {};
    if (!settings.raw) {
        header->len = LittleEndian(total_len);
        header->stored = LittleEndian(total_written);

        int64_t len_64le = LittleEndian(total_len);
        snapshot_obj.Append(MakeSpan((const uint8_t *)&len_64le, RG_SIZE(len_64le)));

        HashBlake3(rk_ObjectType::Snapshot, snapshot_obj, salt.ptr, &id);

        // Write snapshot object
        {
            Size ret = disk->WriteObject(id, rk_ObjectType::Snapshot, snapshot_obj);
            if (ret < 0)
                return false;
            total_written += ret;
        }

        // Create tag file
        {
            Size ret = disk->WriteTag(id);
            if (ret < 0)
                return false;
            total_written += ret;
        }
    } else {
        const rk_FileEntry *entry = (const rk_FileEntry *)(snapshot_obj.ptr + RG_SIZE(rk_SnapshotHeader));
        id = entry->id;
    }

    *out_id = id;
    if (out_len) {
        *out_len += total_len;
    }
    if (out_written) {
        *out_written += total_written;
    }
    return true;
}

}
