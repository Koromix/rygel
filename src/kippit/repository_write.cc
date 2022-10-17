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
#include "chunker.hh"
#include "disk.hh"
#include "repository.hh"
#include "vendor/blake3/c/blake3.h"

#ifndef _WIN32
    #include <unistd.h>
#endif

namespace RG {

static const Size ChunkAverage = Kibibytes(1024);
static const Size ChunkMin = Kibibytes(512);
static const Size ChunkMax = Kibibytes(2048);

enum class PutResult {
    Success,
    Ignore,
    Error
};

class PutContext {
    kt_Disk *disk;
    Span<const uint8_t> salt;

    Async uploads;

    std::atomic<int64_t> stat_len {0};
    std::atomic<int64_t> stat_written {0};

public:
    PutContext(kt_Disk *disk);

    PutResult PutDirectory(const char *src_dirname, bool follow_symlinks, kt_ID *out_id);
    PutResult PutFile(const char *src_filename, kt_ID *out_id);

    bool Sync() { return uploads.Sync(); }

    int64_t GetLen() const { return stat_len; }
    int64_t GetWritten() const { return stat_written; }
};

static void HashBlake3(Span<const uint8_t> buf, const uint8_t *salt, kt_ID *out_id)
{
    blake3_hasher hasher;

    blake3_hasher_init_keyed(&hasher, salt);
    blake3_hasher_update(&hasher, buf.ptr, buf.len);
    blake3_hasher_finalize(&hasher, out_id->hash, RG_SIZE(out_id->hash));
}

class AsyncUpload {
    Span<const uint8_t> obj;

public:
    AsyncUpload(Span<const uint8_t> obj) : obj(obj) {}
    ~AsyncUpload() { ReleaseRaw(nullptr, obj.ptr, -1); }

    // Fake copy operator that moves the data instead ;) We need this to move the
    // data into the Async system, which needs std::function to be copy-constructible.
    AsyncUpload(const AsyncUpload &other)
    {
        AsyncUpload *p = (AsyncUpload *)&other;

        obj = p->obj;
        p->obj = {};
    }
    AsyncUpload &operator=(const AsyncUpload &other) = delete;

    Span<const uint8_t> Get() const { return obj; }
};

PutContext::PutContext(kt_Disk *disk)
    : disk(disk), salt(disk->GetSalt()), uploads(GetCoreCount() * 10)
{
    RG_ASSERT(salt.len == BLAKE3_KEY_LEN); // 32 bytes
}

PutResult PutContext::PutDirectory(const char *src_dirname, bool follow_symlinks, kt_ID *out_id)
{
    BlockAllocator temp_alloc;

    sq_Database *db = disk->GetCache();

    HeapArray<uint8_t> dir_obj;

    src_dirname = DuplicateString(TrimStrRight(src_dirname, RG_PATH_SEPARATORS), &temp_alloc).ptr;

    EnumResult ret = EnumerateDirectory(src_dirname, nullptr, -1,
                                        [&](const char *basename, FileType) {
        const char *filename = Fmt(&temp_alloc, "%1%/%2", src_dirname, basename).ptr;

        Size entry_len = RG_SIZE(kt_FileEntry) + strlen(basename) + 1;
        kt_FileEntry *entry = (kt_FileEntry *)dir_obj.AppendDefault(entry_len);
        RG_DEFER_N(entry_guard) { dir_obj.RemoveLast(entry_len); };

        FileInfo file_info;
        {
            unsigned int flags = follow_symlinks ? (int)StatFlag::FollowSymlink : 0;
            StatResult ret = StatFile(filename, flags, &file_info);

            if (ret != StatResult::Success) {
                bool ignore = (ret == StatResult::AccessDenied || ret == StatResult::MissingPath);
                return ignore;
            }
        }

        switch (file_info.type) {
            case FileType::Directory: { entry->kind = (int8_t)kt_FileEntry::Kind::Directory; } break;
            case FileType::File: {
                entry->kind = (int8_t)kt_FileEntry::Kind::File;
                entry->size = LittleEndian(file_info.size);
            } break;
#ifndef _WIN32
            case FileType::Link: { entry->kind = (int8_t)kt_FileEntry::Kind::Link; } break;
#endif

#ifdef _WIN32
            case FileType::Link:
#endif
            case FileType::Device:
            case FileType::Pipe:
            case FileType::Socket: {
                LogWarning("Ignoring special file '%1' (%2)", filename, FileTypeNames[(int)file_info.type]);
                return true;
            } break;
        }

        entry->mtime = LittleEndian(file_info.mtime);
        entry->mode = LittleEndian((uint32_t)file_info.mode);
        CopyString(basename, MakeSpan(entry->name, entry_len - RG_SIZE(kt_FileEntry)));

        entry_guard.Disable();
        return true;
    });
    if (ret != EnumResult::Success) {
        bool ignore = (ret == EnumResult::AccessDenied || ret == EnumResult::MissingPath);
        return ignore ? PutResult::Ignore : PutResult::Error;
    }

    Async async(&uploads);

    // Process data entries (file, links)
    for (Size offset = 0; offset < dir_obj.len;) {
        kt_FileEntry *entry = (kt_FileEntry *)(dir_obj.ptr + offset);
        const char *filename = Fmt(&temp_alloc, "%1%/%2", src_dirname, entry->name).ptr;

        switch ((kt_FileEntry::Kind)entry->kind) {
            case kt_FileEntry::Kind::Directory: {} break; // Already processed

            case kt_FileEntry::Kind::File: {
                // XXX: absolute path

                // Skip file analysis if metadata is unchanged
                {
                    sq_Statement stmt;
                    if (!db->Prepare("SELECT mtime, mode, size, id FROM stats WHERE path = ?1", &stmt))
                        return PutResult::Error;
                    sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);

                    if (stmt.Step()) {
                        int64_t mtime = sqlite3_column_int64(stmt, 0);
                        uint32_t mode = (uint32_t)sqlite3_column_int64(stmt, 1);
                        int64_t size = sqlite3_column_int64(stmt, 2);
                        Span<const uint8_t> id = MakeSpan((const uint8_t *)sqlite3_column_blob(stmt, 3),
                                                          sqlite3_column_bytes(stmt, 3));

                        if (id.len == RG_SIZE(kt_ID) && mtime == LittleEndian(entry->mtime) &&
                                                        mode == LittleEndian(entry->mode) &&
                                                        size == LittleEndian(entry->size)) {
                            memcpy(&entry->id, id.ptr, RG_SIZE(kt_ID));

                            if (disk->HasObject(entry->id))
                                break;
                        }
                    } else if (!stmt.IsValid()) {
                        return PutResult::Error;
                    }
                }

                async.Run([=, this]() {
                    PutResult ret = PutFile(filename, &entry->id);

                    bool persist = (ret != PutResult::Error);
                    return persist;
                });
            } break;

            case kt_FileEntry::Kind::Link: {
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
                            return false;
                        }

                        target.len = (Size)ret;
                    }

                    HashBlake3(target, salt.ptr, &entry->id);

                    Size ret = disk->WriteObject(entry->id, kt_ObjectType::Link, target);
                    if (ret < 0)
                        return false;
                    stat_written += ret;

                    return true;
                });
#endif
            } break;
        }

        Size entry_len = RG_SIZE(kt_FileEntry) + strlen(entry->name) + 1;
        offset += entry_len;
    }

    // Process directory entries
    for (Size offset = 0; offset < dir_obj.len;) {
        kt_FileEntry *entry = (kt_FileEntry *)(dir_obj.ptr + offset);

        if (entry->kind == (int8_t)kt_FileEntry::Kind::Directory) {
            const char *filename = Fmt(&temp_alloc, "%1%/%2", src_dirname, entry->name).ptr;

            PutResult ret = PutDirectory(filename, follow_symlinks, &entry->id);

            if (ret == PutResult::Error)
                return PutResult::Error;
        }

        Size entry_len = RG_SIZE(kt_FileEntry) + strlen(entry->name) + 1;
        offset += entry_len;
    }

    if (!async.Sync())
        return PutResult::Error;

    kt_ID dir_id = {};
    HashBlake3(dir_obj, salt.ptr, &dir_id);

    // Update cached stats
    for (Size offset = 0; offset < dir_obj.len;) {
        kt_FileEntry *entry = (kt_FileEntry *)(dir_obj.ptr + offset);
        const char *filename = Fmt(&temp_alloc, "%1%/%2", src_dirname, entry->name).ptr;

        if (entry->kind == (int8_t)kt_FileEntry::Kind::File) {
            if (!db->Run(R"(INSERT INTO stats (path, mtime, mode, size, id)
                                VALUES (?1, ?2, ?3, ?4, ?5)
                                ON CONFLICT (path) DO UPDATE SET mtime = excluded.mtime,
                                                                 mode = excluded.mode,
                                                                 size = excluded.size,
                                                                 id = excluded.id)",
                         filename, entry->mtime, entry->mode, entry->size,
                         MakeSpan((const uint8_t *)&entry->id, RG_SIZE(entry->id))))
                return PutResult::Error;
        }

        Size entry_len = RG_SIZE(kt_FileEntry) + strlen(entry->name) + 1;
        offset += entry_len;
    }

    // Upload directory object
    uploads.Run([dir_id, data = AsyncUpload(dir_obj.Leak()), this]() {
        Span<const uint8_t> obj = data.Get();

        Size written = disk->WriteObject(dir_id, kt_ObjectType::Directory2, obj);
        if (written < 0)
            return false;
        stat_written += written;

        return true;
    });

    *out_id = dir_id;
    return PutResult::Success;
}

PutResult PutContext::PutFile(const char *src_filename, kt_ID *out_id)
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

    // Split the file
    {
        kt_Chunker chunker(ChunkAverage, ChunkMin, ChunkMax);

        HeapArray<uint8_t> buf;
        {
            Size needed = (st.ComputeRawLen() >= 0) ? st.ComputeRawLen() : Mebibytes(16);
            needed = std::clamp(needed, Mebibytes(2), Mebibytes(128));

            buf.SetCapacity(needed);
        }

        do {
            Async async(&uploads);

            // Fill buffer
            Size read = st.Read(buf.TakeAvailable());
            if (read < 0)
                return PutResult::Error;
            buf.len += read;
            stat_len += read;

            Span<const uint8_t> remain = buf;

            // We can't relocate in the inner loop
            Size needed = (remain.len / ChunkMin + 1) * RG_SIZE(kt_ChunkEntry) + 8;
            file_obj.Grow(needed);

            // Chunk file and write chunks out in parallel
            do {
                Size processed = chunker.Process(remain, st.IsEOF(), [&](Size idx, int64_t total, Span<const uint8_t> chunk) {
                    RG_ASSERT(idx * RG_SIZE(kt_ChunkEntry) == file_obj.len);
                    file_obj.len += RG_SIZE(kt_ChunkEntry);

                    async.Run([&, idx, total, chunk]() {
                        kt_ChunkEntry entry = {};

                        entry.offset = LittleEndian(total);
                        entry.len = LittleEndian((int32_t)chunk.len);

                        HashBlake3(chunk, salt.ptr, &entry.id);

                        Size ret = disk->WriteObject(entry.id, kt_ObjectType::Chunk, chunk);
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
    kt_ID file_id = {};
    if (file_obj.len != RG_SIZE(kt_ChunkEntry)) {
        int64_t len_64le = LittleEndian(st.GetRawRead());
        file_obj.Append(MakeSpan((const uint8_t *)&len_64le, RG_SIZE(len_64le)));

        HashBlake3(file_obj, salt.ptr, &file_id);

        Size ret = disk->WriteObject(file_id, kt_ObjectType::File, file_obj);
        if (ret < 0)
            return PutResult::Error;
        stat_written += ret;
    } else {
        const kt_ChunkEntry *entry0 = (const kt_ChunkEntry *)file_obj.ptr;
        file_id = entry0->id;
    }

    *out_id = file_id;
    return PutResult::Success;
}

bool kt_Put(kt_Disk *disk, const kt_PutSettings &settings, Span<const char *const> filenames,
            kt_ID *out_id, int64_t *out_len, int64_t *out_written)
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
    if (settings.name && strlen(settings.name) >= RG_SIZE(kt_SnapshotHeader::name)) {
        LogError("Snapshot name '%1' is too long (limit is %2 bytes)", settings.name, RG_SIZE(kt_SnapshotHeader::name));
        return false;
    }

    Span<const uint8_t> salt = disk->GetSalt();
    RG_ASSERT(salt.len == BLAKE3_KEY_LEN); // 32 bytes

    HeapArray<uint8_t> snapshot_obj;
    kt_SnapshotHeader *header = (kt_SnapshotHeader *)snapshot_obj.AppendDefault(RG_SIZE(kt_SnapshotHeader));

    CopyString(settings.name ? settings.name : "", header->name);
    header->time = LittleEndian(GetUnixTime());

    PutContext put(disk);

    // Process snapshot entries
    for (const char *filename: filenames) {
        Span<char> name = NormalizePath(filename, &temp_alloc);

        Size entry_len = RG_SIZE(kt_FileEntry) + name.len + 1;
        kt_FileEntry *entry = (kt_FileEntry *)snapshot_obj.Grow(entry_len);

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

            while (StartsWith(name, "../")) {
                name = name.Take(3, name.len - 3);
                changed = true;
            }
            if (TestStr(name, "..")) {
                name = {};
            }
            while (name.ptr[0] == '/') {
                name = name.Take(1, name.len - 1);
                changed = true;
            }

            if (!name.len) {
                LogError("Cannot backup empty filename");
                return false;
            }

            memcpy(entry->name, name.ptr, name.len);
            entry->name[name.len] = 0;

            if (changed) {
                LogWarning("Storing '%1' as '%2'", filename, entry->name);
            }
        }

        // Adjust entry length
        entry_len = RG_SIZE(kt_FileEntry) + name.len + 1;
        snapshot_obj.len += entry_len;

        FileInfo file_info;
        if (StatFile(filename, (int)StatFlag::FollowSymlink, &file_info) != StatResult::Success)
            return false;

        switch (file_info.type) {
            case FileType::Directory: {
                entry->kind = (int8_t)kt_FileEntry::Kind::Directory;

                if (put.PutDirectory(filename, settings.follow_symlinks, &entry->id) != PutResult::Success)
                    return false;
            } break;
            case FileType::File: {
                entry->kind = (int8_t)kt_FileEntry::Kind::File;
                entry->size = LittleEndian((uint32_t)file_info.size);

                if (put.PutFile(filename, &entry->id) != PutResult::Success)
                    return false;
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
        entry->mode = LittleEndian((uint32_t)file_info.mode);
    }

    if (!put.Sync())
        return false;

    int64_t total_len = put.GetLen();
    int64_t total_written = put.GetWritten();

    kt_ID id = {};
    if (!settings.raw) {
        header->len = LittleEndian(total_len);
        header->stored = LittleEndian(total_written);

        HashBlake3(snapshot_obj, salt.ptr, &id);

        // Write snapshot object
        {
            Size ret = disk->WriteObject(id, kt_ObjectType::Snapshot2, snapshot_obj);
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
        const kt_FileEntry *entry = (const kt_FileEntry *)(snapshot_obj.ptr + RG_SIZE(kt_SnapshotHeader));
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
