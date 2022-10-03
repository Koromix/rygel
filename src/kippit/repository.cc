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
    #include <unistd.h>
#endif

namespace RG {

static const Size ChunkAverage = Kibibytes(1024);
static const Size ChunkMin = Kibibytes(512);
static const Size ChunkMax = Kibibytes(2048);

#pragma pack(push, 1)
struct SnapshotHeader {
    char name[512];
    int64_t time; // Little Endian
};
#pragma pack(pop)

#pragma pack(push, 1)
struct FileEntry {
    enum class Kind {
        Directory = 0,
        File = 1
    };

    kt_ID id;
    int8_t kind; // Kind
    int64_t mtime;
    uint32_t mode;
    char name[];
};
#pragma pack(pop)
RG_STATIC_ASSERT(RG_SIZE(FileEntry) == 45);

#pragma pack(push, 1)
struct ChunkEntry {
    int64_t offset; // Little Endian
    int32_t len;    // Little Endian
    kt_ID id;
};
#pragma pack(pop)
RG_STATIC_ASSERT(RG_SIZE(ChunkEntry) == 44);

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

    if (!SetFilePointerEx(h, {.QuadPart = len}, nullptr, FILE_BEGIN)) {
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

        if (!WriteFile(h, buf.ptr, buf.len, &written, &ov)) {
            LogError("Failed to write to '%1': %2", filename, GetWin32ErrorString());
            return false;
        }

        offset += (Size)written;
        buf.ptr += (Size)written;
        buf.len -= (Size)written;
    }

    return true;
}

#else

static bool ReserveFile(int fd, const char *filename, int64_t len)
{
    if (ftruncate(fd, len) < 0) {
        LogError("Failed to reserve file '%1': %2", filename, strerror(errno));
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

#endif

static void HashBlake3(Span<const uint8_t> buf, const uint8_t *salt, kt_ID *out_id)
{
    blake3_hasher hasher;

    blake3_hasher_init_keyed(&hasher, salt);
    blake3_hasher_update(&hasher, buf.ptr, buf.len);
    blake3_hasher_finalize(&hasher, out_id->hash, RG_SIZE(out_id->hash));
}

static bool PutFile(kt_Disk *disk, const char *src_filename, kt_ID *out_id, int64_t *out_written)
{
    Span<const uint8_t> salt = disk->GetSalt();
    RG_ASSERT(salt.len == BLAKE3_KEY_LEN); // 32 bytes

    StreamReader st;
    {
        OpenResult ret = st.Open(src_filename);
        if (ret != OpenResult::Success) {
            bool ignore = (ret == OpenResult::AccessDenied || ret == OpenResult::MissingPath);
            return ignore;
        }
    }

    HeapArray<uint8_t> file_obj;
    std::atomic<int64_t> file_written = 0;

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
            Async async;

            // Fill buffer
            Size read = st.Read(buf.TakeAvailable());
            if (read < 0)
                return false;
            buf.len += read;

            Span<const uint8_t> remain = buf;

            // We can't relocate in the inner loop
            Size needed = (remain.len / ChunkMin + 1) * RG_SIZE(ChunkEntry) + 8;
            file_obj.Grow(needed);

            // Chunk file and write chunks out in parallel
            do {
                Size processed = chunker.Process(remain, st.IsEOF(), [&](Size idx, int64_t total, Span<const uint8_t> chunk) {
                    RG_ASSERT(idx * RG_SIZE(ChunkEntry) == file_obj.len);
                    file_obj.len += RG_SIZE(ChunkEntry);

                    async.Run([=, &file_written, &file_obj]() {
                        ChunkEntry entry = {};

                        entry.offset = LittleEndian(total);
                        entry.len = LittleEndian((int32_t)chunk.len);

                        HashBlake3(chunk, salt.ptr, &entry.id);

                        Size ret = disk->WriteObject(entry.id, kt_ObjectType::Chunk, chunk);
                        if (ret < 0)
                            return false;
                        file_written += ret;

                        memcpy(file_obj.ptr + idx * RG_SIZE(entry), &entry, RG_SIZE(entry));

                        return true;
                    });

                    return true;
                });
                if (processed < 0)
                    return false;
                if (!processed)
                    break;

                remain.ptr += processed;
                remain.len -= processed;
            } while (remain.len);

            if (!async.Sync())
                return false;

            memmove_safe(buf.ptr, remain.ptr, remain.len);
            buf.len = remain.len;
        } while (!st.IsEOF() || buf.len);
    }

    // Write list of chunks (unless there is exactly one)
    kt_ID file_id = {};
    if (file_obj.len != RG_SIZE(ChunkEntry)) {
        int64_t len_64le = LittleEndian(st.GetRawRead());
        file_obj.Append(MakeSpan((const uint8_t *)&len_64le, RG_SIZE(len_64le)));

        HashBlake3(file_obj, salt.ptr, &file_id);

        Size ret = disk->WriteObject(file_id, kt_ObjectType::File, file_obj);
        if (ret < 0)
            return false;
        file_written += ret;
    } else {
        const ChunkEntry *entry0 = (const ChunkEntry *)file_obj.ptr;
        file_id = entry0->id;
    }

    *out_id = file_id;
    if (out_written) {
        *out_written += file_written;
    }
    return true;
}

static bool GetFile(kt_Disk *disk, const kt_ID &id, kt_ObjectType type,
                    Span<const uint8_t> file_obj, const char *dest_filename, int64_t *out_len)
{
    RG_ASSERT(type == kt_ObjectType::File || type == kt_ObjectType::Chunk);

    // Open destination file
    int fd = OpenDescriptor(dest_filename, (int)OpenFlag::Write);
    if (fd < 0)
        return false;
    RG_DEFER { close(fd); };

    int64_t file_len = -1;
    switch (type) {
        case kt_ObjectType::File: {
            if (file_obj.len % RG_SIZE(ChunkEntry) != RG_SIZE(int64_t)) {
                LogError("Malformed file object '%1'", id);
                return false;
            }

            file_obj.len -= RG_SIZE(int64_t);

            // Prepare destination file
            file_len = LittleEndian(*(const int64_t *)file_obj.end());
            if (file_len < 0) {
                LogError("Malformed file object '%1'", id);
                return false;
            }
            if (!ReserveFile(fd, dest_filename, file_len))
                return false;

            Async async;

            // Write unencrypted file
            for (Size idx = 0, offset = 0; offset < file_obj.len; idx++, offset += RG_SIZE(ChunkEntry)) {
                async.Run([=]() {
                    ChunkEntry entry = {};

                    memcpy(&entry, file_obj.ptr + offset, RG_SIZE(entry));
                    entry.offset = LittleEndian(entry.offset);
                    entry.len = LittleEndian(entry.len);

                    kt_ObjectType type;
                    HeapArray<uint8_t> buf;
                    if (!disk->ReadObject(entry.id, &type, &buf))
                        return false;

                    if (RG_UNLIKELY(type != kt_ObjectType::Chunk)) {
                        LogError("Object '%1' is not a chunk", entry.id);
                        return false;
                    }
                    if (RG_UNLIKELY(buf.len != entry.len)) {
                        LogError("Chunk size mismatch for '%1'", entry.id);
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
                return false;

            // Check actual file size
            if (file_obj.len) {
                const ChunkEntry *entry = (const ChunkEntry *)(file_obj.end() - RG_SIZE(ChunkEntry));
                int64_t len = LittleEndian(entry->offset) + LittleEndian(entry->len);

                if (RG_UNLIKELY(len != file_len)) {
                    LogError("File size mismatch for '%1'", entry->id);
                    return false;
                }
            }
        } break;

        case kt_ObjectType::Chunk: {
            file_len = file_obj.len;

            if (!WriteAt(fd, dest_filename, 0, file_obj)) {
                LogError("Failed to write to '%1': %2", dest_filename, strerror(errno));
                return false;
            }
        } break;

        case kt_ObjectType::Directory:
        case kt_ObjectType::Snapshot: { RG_UNREACHABLE(); } break;
    }

    if (!FlushFile(fd, dest_filename))
        return false;

    if (out_len) {
        *out_len += file_len;
    }
    return true;
}

static bool PutDirectory(kt_Disk *disk, const char *src_dirname, kt_ID *out_id, int64_t *out_written)
{
    BlockAllocator temp_alloc;

    Span<const uint8_t> salt = disk->GetSalt();
    RG_ASSERT(salt.len == BLAKE3_KEY_LEN); // 32 bytes

    HeapArray<uint8_t> dir_obj;
    std::atomic<int64_t> dir_written = 0;

    EnumResult ret = EnumerateDirectory(src_dirname, nullptr, -1,
                                        [&](const char *basename, FileType) {
        const char *filename = Fmt(&temp_alloc, "%1%/%2", src_dirname, basename).ptr;

        Size entry_len = RG_SIZE(FileEntry) + strlen(basename) + 1;
        FileEntry *entry = (FileEntry *)dir_obj.AppendDefault(entry_len);

        FileInfo file_info;
        {
            StatResult ret = StatFile(filename, &file_info);
            if (ret != StatResult::Success) {
                bool ignore = (ret == StatResult::AccessDenied || ret == StatResult::MissingPath);
                return ignore;
            }
        }

        switch (file_info.type) {
            case FileType::Directory: {
                entry->kind = (int8_t)FileEntry::Kind::Directory;

                int64_t written = 0;
                if (!PutDirectory(disk, filename, &entry->id, &written))
                    return false;
                dir_written += written;
            } break;
            case FileType::File: {
                entry->kind = (int8_t)FileEntry::Kind::File;

                int64_t written = 0;
                if (!PutFile(disk, filename, &entry->id, &written))
                    return false;
                dir_written += written;
            } break;

            case FileType::Link:
            case FileType::Device:
            case FileType::Pipe:
            case FileType::Socket: {
                LogWarning("Ignoring special file '%1' (%2)", filename, FileTypeNames[(int)file_info.type]);

                dir_obj.RemoveLast(entry_len);
                return true;
            } break;
        }

        entry->mtime = file_info.mtime;
        entry->mode = (uint32_t)file_info.mode;
        CopyString(basename, MakeSpan(entry->name, entry_len - RG_SIZE(FileEntry)));

        return true;
    });
    if (ret != EnumResult::Success) {
        bool ignore = (ret == EnumResult::AccessDenied || ret == EnumResult::MissingPath);
        return ignore;
    }

    kt_ID dir_id = {};
    {
        HashBlake3(dir_obj, salt.ptr, &dir_id);

        Size ret = disk->WriteObject(dir_id, kt_ObjectType::Directory, dir_obj);
        if (ret < 0)
            return false;
        dir_written += ret;
    }

    *out_id = dir_id;
    if (out_written) {
        *out_written += dir_written;
    }
    return true;
}

static bool ExtractFileEntries(kt_Disk *disk, Span<const uint8_t> entries, bool allow_sep,
                               const char *dest_dirname, int64_t *out_len)
{
    BlockAllocator temp_alloc;

    // XXX: Make sure each path does not clobber a previous one

    for (Size offset = 0; offset < entries.len;) {
        const FileEntry *entry = (const FileEntry *)(entries.ptr + offset);

        // Skip to next entry
        Size entry_len = RG_SIZE(FileEntry) + (Size)strnlen(entry->name, entries.end() - (const uint8_t *)entry->name) + 1;
        offset += entry_len;

        // Sanity checks
        if (offset > entries.len) {
            LogError("Malformed entry in directory object");
            return false;
        }
        if (entry->kind != (int8_t)FileEntry::Kind::Directory && entry->kind != (int8_t)FileEntry::Kind::File) {
            LogError("Unknown file kind 0x%1", FmtHex((unsigned int)entry->kind));
            return false;
        }
        if (!entry->name[0] || PathContainsDotDot(entry->name)) {
            LogError("Unsafe file name '%1'", entry->name);
            return false;
        }
        if (PathIsAbsolute(entry->name)) {
            LogError("Unsafe file name '%1'", entry->name);
            return false;
        }
        if (!allow_sep && strpbrk(entry->name, RG_PATH_SEPARATORS)) {
            LogError("Unsafe file name '%1'", entry->name);
            return false;
        }

        kt_ObjectType entry_type;
        HeapArray<uint8_t> entry_obj;
        if (!disk->ReadObject(entry->id, &entry_type, &entry_obj))
            return false;

        const char *entry_filename = Fmt(&temp_alloc, "%1%/%2", dest_dirname, entry->name).ptr;
        if (allow_sep && !EnsureDirectoryExists(entry_filename))
            return false;

        switch (entry->kind) {
            case (int8_t)FileEntry::Kind::Directory: {
                if (entry_type != kt_ObjectType::Directory) {
                    LogError("Object '%1' is not a directory", entry->id);
                    return false;
                }

                if (!MakeDirectory(entry_filename, false))
                    return false;
                if (!ExtractFileEntries(disk, entry_obj, false, entry_filename, out_len))
                    return false;
            } break;
            case (int8_t)FileEntry::Kind::File: {
                if (entry_type != kt_ObjectType::File && entry_type != kt_ObjectType::Chunk) {
                    LogError("Object '%1' is not a file", entry->id);
                    return false;
                }

                if (!GetFile(disk, entry->id, entry_type, entry_obj, entry_filename, out_len))
                    return false;
            } break;

            default: {
                LogError("Unknown file kind 0x%1", FmtHex((unsigned int)entry->kind));
                return false;
            } break;
        }
    }

    return true;
}

bool kt_Put(kt_Disk *disk, const kt_PutSettings &settings, Span<const char *const> filenames, kt_ID *out_id, int64_t *out_written)
{
    RG_ASSERT(filenames.len >= 1);

    if (settings.raw && settings.name) {
        LogError("Cannot use snapshot name in raw mode");
        return false;
    }
    if (settings.raw && filenames.len != 1) {
        LogError("Only one object can be backup up in raw mode");
        return false;
    }
    if (settings.name && strlen(settings.name) >= RG_SIZE(SnapshotHeader::name)) {
        LogError("Snapshot name '%1' is too long (limit is %2 bytes)", settings.name, RG_SIZE(SnapshotHeader::name));
        return false;
    }

    Span<const uint8_t> salt = disk->GetSalt();
    RG_ASSERT(salt.len == BLAKE3_KEY_LEN); // 32 bytes

    HeapArray<uint8_t> snapshot_obj;
    int64_t written = 0;

    // Snapshot header
    {
        SnapshotHeader *header = (SnapshotHeader *)snapshot_obj.AppendDefault(RG_SIZE(SnapshotHeader));

        CopyString(settings.name ? settings.name : "", header->name);
        header->time = LittleEndian(GetUnixTime());
    }

    // Process snapshot entries
    for (const char *filename: filenames) {
        Size entry_len = RG_SIZE(FileEntry) + strlen(filename) + 1;
        FileEntry *entry = (FileEntry *)snapshot_obj.AppendDefault(entry_len);

        const char *name;
        {
            Span<char> dest = MakeSpan(entry->name, entry_len - RG_SIZE(FileEntry));
            bool changed = false;

            name = filename;

            // Change Windows paths, even on non-Windows platforms
            if (IsAsciiAlpha(name[0]) && name[1] == ':') {
                entry->name[0] = LowerAscii(name[0]);
                entry->name[1] = '/';

                dest = dest.Take(2, dest.len - 2);
                name += 2;
                changed = true;
            }

            while (strchr("/\\", name[0])) {
                snapshot_obj.len--;
                entry_len--;

                name++;

                changed = true;
            }

            if (!name[0]) {
                LogError("Cannot backup empty filename");
                return false;
            }

            for (Size i = 0; name[i]; i++) {
                int c = name[i];
                dest[i] = (char)(c == '\\' ? '/' : c);
            }

            if (changed) {
                LogWarning("Storing '%1' as '%2'", filename, entry->name);
            }
        }

        FileInfo file_info;
        if (StatFile(filename, (int)StatFlag::FollowSymlink, &file_info) != StatResult::Success)
            return false;

        switch (file_info.type) {
            case FileType::Directory: {
                entry->kind = (int8_t)FileEntry::Kind::Directory;

                if (!PutDirectory(disk, filename, &entry->id, &written))
                    return false;
            } break;
            case FileType::File: {
                entry->kind = (int8_t)FileEntry::Kind::File;

                if (!PutFile(disk, filename, &entry->id, &written))
                    return false;
            } break;

            case FileType::Link:
            case FileType::Device:
            case FileType::Pipe:
            case FileType::Socket: {
                LogError("Cannot backup special file '%1' (%2)", filename, FileTypeNames[(int)file_info.type]);
                return false;
            } break;
        }

        entry->mtime = file_info.mtime;
        entry->mode = (uint32_t)file_info.mode;
    }

    kt_ID id = {};
    if (!settings.raw) {
        HashBlake3(snapshot_obj, salt.ptr, &id);

        // Write snapshot object
        {
            Size ret = disk->WriteObject(id, kt_ObjectType::Snapshot, snapshot_obj);
            if (ret < 0)
                return false;
            written += ret;
        }

        // Create tag file
        {
            Size ret = disk->WriteTag(id);
            if (ret < 0)
                return false;
            written += ret;
        }
    } else {
        const FileEntry *entry = (const FileEntry *)(snapshot_obj.ptr + RG_SIZE(SnapshotHeader));
        id = entry->id;
    }

    *out_id = id;
    if (out_written) {
        *out_written += written;
    }
    return true;
}

bool kt_List(kt_Disk *disk, Allocator *str_alloc, HeapArray<kt_SnapshotInfo> *out_snapshots)
{
    Size prev_len = out_snapshots->len;
    RG_DEFER_N(out_guard) { out_snapshots->RemoveFrom(prev_len); };

    HeapArray<kt_ID> ids;
    if (!disk->ListTags(&ids))
        return false;

    // Gather snapshot information
    {
        // Reuse for performance
        kt_ObjectType type;
        HeapArray<uint8_t> obj;

        for (const kt_ID &id: ids) {
            kt_SnapshotInfo snapshot = {};

            obj.RemoveFrom(0);
            if (!disk->ReadObject(id, &type, &obj))
                return false;

            if (type != kt_ObjectType::Snapshot) {
                LogError("Object '%1' is not a snapshot (ignoring)", id);
                continue;
            }
            if (obj.len <= RG_SIZE(SnapshotHeader)) {
                LogError("Malformed snapshot object '%1' (ignoring)", id);
                continue;
            }

            const SnapshotHeader *header = (const SnapshotHeader *)obj.ptr;

            snapshot.id = id;
            snapshot.name = header->name[0] ? DuplicateString(header->name, str_alloc).ptr : nullptr;
            snapshot.time = LittleEndian(header->time);

            out_snapshots->Append(snapshot);
        }
    }

    std::sort(out_snapshots->ptr + prev_len, out_snapshots->end(),
              [](const kt_SnapshotInfo &snapshot1, const kt_SnapshotInfo &snapshot2) { return snapshot1.time < snapshot2.time; });

    out_guard.Disable();
    return true;
}

bool kt_Get(kt_Disk *disk, const kt_ID &id, const char *dest_path, int64_t *out_len)
{
    kt_ObjectType type;
    HeapArray<uint8_t> obj;
    if (!disk->ReadObject(id, &type, &obj))
        return false;

    switch (type) {
        case kt_ObjectType::Chunk:
        case kt_ObjectType::File: {
            if (TestFile(dest_path) && !IsDirectoryEmpty(dest_path)) {
                LogError("File '%1' already exists", dest_path);
                return false;
            }

            return GetFile(disk, id, type, obj, dest_path, out_len);
        } break;

        case kt_ObjectType::Directory: {
            if (TestFile(dest_path, FileType::Directory)) {
                if (!IsDirectoryEmpty(dest_path)) {
                    LogError("Directory '%1' exists and is not empty", dest_path);
                    return false;
                }
            } else {
                if (!MakeDirectory(dest_path))
                    return false;
            }

            return ExtractFileEntries(disk, obj, false, dest_path, out_len);
        }

        case kt_ObjectType::Snapshot: {
            if (TestFile(dest_path, FileType::Directory)) {
                if (!IsDirectoryEmpty(dest_path)) {
                    LogError("Directory '%1' exists and is not empty", dest_path);
                    return false;
                }
            } else {
                if (!MakeDirectory(dest_path))
                    return false;
            }

            // There must be at least one entry
            if (obj.len <= RG_SIZE(SnapshotHeader)) {
                LogError("Malformed snapshot object '%1'", id);
                return false;
            }

            Span<const uint8_t> entries = obj.Take(RG_SIZE(SnapshotHeader), obj.len - RG_SIZE(SnapshotHeader));
            return ExtractFileEntries(disk, entries, true, dest_path, out_len);
        } break;
    }

    RG_UNREACHABLE();
}

}
