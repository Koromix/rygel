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

enum class ExtractFlag {
    AllowSeparators = 1 << 0,
    FlattenName = 1 << 1
};

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
            if (file_obj.len % RG_SIZE(kt_ChunkEntry) != RG_SIZE(int64_t)) {
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
            for (Size idx = 0, offset = 0; offset < file_obj.len; idx++, offset += RG_SIZE(kt_ChunkEntry)) {
                async.Run([=]() {
                    kt_ChunkEntry entry = {};

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
                const kt_ChunkEntry *entry = (const kt_ChunkEntry *)(file_obj.end() - RG_SIZE(kt_ChunkEntry));
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

static bool ExtractFileEntries(kt_Disk *disk, Span<const uint8_t> entries, unsigned int flags,
                               const char *dest_dirname, int64_t *out_len)
{
    BlockAllocator temp_alloc;

    // XXX: Make sure each path does not clobber a previous one

    for (Size offset = 0; offset < entries.len;) {
        const kt_FileEntry *entry = (const kt_FileEntry *)(entries.ptr + offset);

        // Skip to next entry
        Size entry_len = RG_SIZE(kt_FileEntry) + (Size)strnlen(entry->name, entries.end() - (const uint8_t *)entry->name) + 1;
        offset += entry_len;

        // Sanity checks
        if (offset > entries.len) {
            LogError("Malformed entry in directory object");
            return false;
        }
        if (entry->kind != (int8_t)kt_FileEntry::Kind::Directory && entry->kind != (int8_t)kt_FileEntry::Kind::File) {
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
        if (!(flags & (int)ExtractFlag::AllowSeparators) && strpbrk(entry->name, RG_PATH_SEPARATORS)) {
            LogError("Unsafe file name '%1'", entry->name);
            return false;
        }

        kt_ObjectType entry_type;
        HeapArray<uint8_t> entry_obj;
        if (!disk->ReadObject(entry->id, &entry_type, &entry_obj))
            return false;

        const char *entry_filename;
        if (flags & (int)ExtractFlag::FlattenName) {
            entry_filename = Fmt(&temp_alloc, "%1%/%2", dest_dirname, SplitStrReverse(entry->name, '/')).ptr;
        } else {
            entry_filename = Fmt(&temp_alloc, "%1%/%2", dest_dirname, entry->name).ptr;

            if ((flags & (int)ExtractFlag::AllowSeparators) && !EnsureDirectoryExists(entry_filename))
                return false;
        }

        switch (entry->kind) {
            case (int8_t)kt_FileEntry::Kind::Directory: {
                if (entry_type != kt_ObjectType::Directory) {
                    LogError("Object '%1' is not a directory", entry->id);
                    return false;
                }

                if (!MakeDirectory(entry_filename, false))
                    return false;
                if (!ExtractFileEntries(disk, entry_obj, 0, entry_filename, out_len))
                    return false;
            } break;
            case (int8_t)kt_FileEntry::Kind::File: {
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
            if (obj.len <= RG_SIZE(kt_SnapshotHeader)) {
                LogError("Malformed snapshot object '%1' (ignoring)", id);
                continue;
            }

            const kt_SnapshotHeader *header = (const kt_SnapshotHeader *)obj.ptr;

            snapshot.id = id;
            snapshot.name = header->name[0] ? DuplicateString(header->name, str_alloc).ptr : nullptr;
            snapshot.time = LittleEndian(header->time);
            snapshot.len = header->len;
            snapshot.stored = header->stored + obj.len;

            out_snapshots->Append(snapshot);
        }
    }

    std::sort(out_snapshots->ptr + prev_len, out_snapshots->end(),
              [](const kt_SnapshotInfo &snapshot1, const kt_SnapshotInfo &snapshot2) { return snapshot1.time < snapshot2.time; });

    out_guard.Disable();
    return true;
}

bool kt_Get(kt_Disk *disk, const kt_ID &id, const kt_GetSettings &settings, const char *dest_path, int64_t *out_len)
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

            return ExtractFileEntries(disk, obj, 0, dest_path, out_len);
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
            if (obj.len <= RG_SIZE(kt_SnapshotHeader)) {
                LogError("Malformed snapshot object '%1'", id);
                return false;
            }

            Span<uint8_t> entries = obj.Take(RG_SIZE(kt_SnapshotHeader), obj.len - RG_SIZE(kt_SnapshotHeader));
            unsigned int flags = (int)ExtractFlag::AllowSeparators | (settings.flat ? (int)ExtractFlag::FlattenName : 0);

            return ExtractFileEntries(disk, entries, flags, dest_path, out_len);
        } break;
    }

    RG_UNREACHABLE();
}

}
