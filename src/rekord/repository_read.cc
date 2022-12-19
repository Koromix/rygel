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
    #include <sys/stat.h>
    #include <sys/time.h>
    #include <unistd.h>
#endif

namespace RG {

enum class ExtractFlag {
    AllowSeparators = 1 << 0,
    FlattenName = 1 << 1
};

class GetContext {
    rk_Disk *disk;

    Async tasks;

    std::atomic<int64_t> stat_len {0};

public:
    GetContext(rk_Disk *disk);

    bool ExtractEntries(rk_ObjectType type, Span<const uint8_t> entries, unsigned int flags, const char *dest_dirname);
    int GetFile(const rk_ID &id, rk_ObjectType type, Span<const uint8_t> file_obj, const char *dest_filename);

    bool Sync() { return tasks.Sync(); }

    int64_t GetLen() const { return stat_len; }
};

GetContext::GetContext(rk_Disk *disk)
    : disk(disk), tasks(disk->GetThreads())
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

static bool CreateSymbolicLink(const char *filename, const char *target)
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

static bool CreateSymbolicLink(const char *filename, const char *target)
{
    if (symlink(target, filename) < 0) {
        LogError("Failed to create symbolic link '%1': %2", filename, strerror(errno));
        return false;
    }

    return true;
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

bool GetContext::ExtractEntries(rk_ObjectType type, Span<const uint8_t> entries,
                                unsigned int flags, const char *dest_dirname)
{
    // XXX: Make sure each path does not clobber a previous one

    std::shared_ptr<BlockAllocator> temp_alloc = std::make_shared<BlockAllocator>();

    for (Size offset = 0; offset < entries.len;) {
        rk_FileEntry entry;
        const char *name = nullptr;

        // Avoid MSVC error C2466
        memset(&entry, 0, RG_SIZE(entry));

        if (type == rk_ObjectType::Directory1 || type == rk_ObjectType::Snapshot1) {
            rk_FileEntry::V1 *v1 = (rk_FileEntry::V1 *)(entries.ptr + offset);

            if (entries.len - offset < RG_SIZE(*v1)) {
                LogError("Malformed entry in directory object");
                return false;
            }

            entry.id = v1->id;
            entry.kind = v1->kind;
            entry.mtime = LittleEndian(v1->mtime);
            entry.btime = entry.mtime;
            entry.mode = LittleEndian(v1->mode);
            name = v1->name;
        } else if (type == rk_ObjectType::Directory2 || type == rk_ObjectType::Snapshot2) {
            rk_FileEntry::V2 *v2 = (rk_FileEntry::V2 *)(entries.ptr + offset);

            if (entries.len - offset < RG_SIZE(*v2)) {
                LogError("Malformed entry in directory object");
                return false;
            }

            entry.id = v2->id;
            entry.kind = v2->kind;
            entry.mtime = LittleEndian(v2->mtime);
            entry.btime = entry.mtime;
            entry.mode = LittleEndian(v2->mode);
            entry.size = LittleEndian(v2->size);
            name = v2->name;
        } else if (type == rk_ObjectType::Directory3 || type == rk_ObjectType::Snapshot3) {
            rk_FileEntry *v3 = (rk_FileEntry *)(entries.ptr + offset);

            if (entries.len - offset < RG_SIZE(*v3)) {
                LogError("Malformed entry in directory object");
                return false;
            }

            entry.id = v3->id;
            entry.kind = v3->kind;
            entry.mtime = LittleEndian(v3->mtime);
            entry.btime = LittleEndian(v3->btime);
            entry.mode = LittleEndian(v3->mode);
            entry.size = LittleEndian(v3->size);
            name = v3->name;
        } else {
            RG_UNREACHABLE();
        }

        // Skip entry for next iteration
        {
            const uint8_t *end = (const uint8_t *)memchr(name, 0, entries.end() - (const uint8_t *)name);

            if (!end) {
                LogError("Malformed entry in directory object");
                return false;
            }

            offset = end - entries.ptr + 1;
        }

        // Sanity checks
        if (entry.kind != (int8_t)rk_FileEntry::Kind::Directory &&
                entry.kind != (int8_t)rk_FileEntry::Kind::File &&
                entry.kind != (int8_t)rk_FileEntry::Kind::Link) {
            LogError("Unknown file kind 0x%1", FmtHex((unsigned int)entry.kind));
            return false;
        }
        if (!name[0] || PathContainsDotDot(name)) {
            LogError("Unsafe file name '%1'", name);
            return false;
        }
        if (PathIsAbsolute(name)) {
            LogError("Unsafe file name '%1'", name);
            return false;
        }
        if (!(flags & (int)ExtractFlag::AllowSeparators) && strpbrk(name, RG_PATH_SEPARATORS)) {
            LogError("Unsafe file name '%1'", name);
            return false;
        }

        rk_ID entry_id = entry.id;
        int8_t entry_kind = entry.kind;
        int64_t entry_mtime = entry.mtime;
        int64_t entry_btime = entry.btime;
        uint32_t entry_mode = entry.mode;

        const char *entry_filename;
        if (flags & (int)ExtractFlag::FlattenName) {
            entry_filename = Fmt(temp_alloc.get(), "%1%/%2", dest_dirname, SplitStrReverse(name, '/')).ptr;
        } else {
            entry_filename = Fmt(temp_alloc.get(), "%1%/%2", dest_dirname, name).ptr;

            if ((flags & (int)ExtractFlag::AllowSeparators) && !EnsureDirectoryExists(entry_filename))
                return false;
        }

        tasks.Run([=, temp_alloc = temp_alloc, this] () {
            rk_ObjectType entry_type;
            HeapArray<uint8_t> entry_obj;
            if (!disk->ReadObject(entry_id, &entry_type, &entry_obj))
                return false;

            switch (entry_kind) {
                case (int8_t)rk_FileEntry::Kind::Directory: {
                    if (entry_type != rk_ObjectType::Directory1 &&
                            entry_type != rk_ObjectType::Directory2 &&
                            entry_type != rk_ObjectType::Directory3) {
                        LogError("Object '%1' is not a directory", entry_id);
                        return false;
                    }

                    if (!MakeDirectory(entry_filename, false))
                        return false;

                    if (!ExtractEntries(entry_type, entry_obj, 0, entry_filename))
                        return false;

                    // Set directory metadata
                    {
                        int fd = OpenDescriptor(entry_filename, (int)OpenFlag::Write | (int)OpenFlag::Directory);
                        SetFileMetaData(fd, entry_filename, entry_mtime, entry_btime, entry_mode);
                        close(fd);
                    }
                } break;
                case (int8_t)rk_FileEntry::Kind::File: {
                    if (entry_type != rk_ObjectType::File && entry_type != rk_ObjectType::Chunk) {
                        LogError("Object '%1' is not a file", entry_id);
                        return false;
                    }

                    int fd = GetFile(entry_id, entry_type, entry_obj, entry_filename);
                    if (fd < 0)
                        return false;
                    RG_DEFER { close(fd); };

                    SetFileMetaData(fd, entry_filename, entry_mtime, entry_btime, entry_mode);
                } break;
                case (int8_t)rk_FileEntry::Kind::Link: {
                    if (entry_type != rk_ObjectType::Link) {
                        LogError("Object '%1' is not a link", entry_id);
                        return false;
                    }

                    // NUL terminate the path
                    entry_obj.Append(0);

                    if (!CreateSymbolicLink(entry_filename, (const char *)entry_obj.ptr))
                        return false;
                } break;

                default: { RG_UNREACHABLE(); } break;
            }

            return true;
        });
    }

    return true;
}

int GetContext::GetFile(const rk_ID &id, rk_ObjectType type, Span<const uint8_t> file_obj, const char *dest_filename)
{
    RG_ASSERT(type == rk_ObjectType::File || type == rk_ObjectType::Chunk);

    // Open destination file
    int fd = OpenDescriptor(dest_filename, (int)OpenFlag::Write);
    if (fd < 0)
        return -1;
    RG_DEFER_N(err_guard) { close(fd); };

    int64_t file_len = -1;
    switch (type) {
        case rk_ObjectType::File: {
            if (file_obj.len % RG_SIZE(rk_ChunkEntry) != RG_SIZE(int64_t)) {
                LogError("Malformed file object '%1'", id);
                return -1;
            }

            file_obj.len -= RG_SIZE(int64_t);

            // Prepare destination file
            file_len = LittleEndian(*(const int64_t *)file_obj.end());
            if (file_len < 0) {
                LogError("Malformed file object '%1'", id);
                return -1;
            }
            if (!ReserveFile(fd, dest_filename, file_len))
                return -1;

            Async async(&tasks);

            // Write unencrypted file
            for (Size offset = 0; offset < file_obj.len; offset += RG_SIZE(rk_ChunkEntry)) {
                async.Run([=, this]() {
                    rk_ChunkEntry entry = {};

                    memcpy(&entry, file_obj.ptr + offset, RG_SIZE(entry));
                    entry.offset = LittleEndian(entry.offset);
                    entry.len = LittleEndian(entry.len);

                    rk_ObjectType type;
                    HeapArray<uint8_t> buf;
                    if (!disk->ReadObject(entry.id, &type, &buf))
                        return false;

                    if (RG_UNLIKELY(type != rk_ObjectType::Chunk)) {
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
                return -1;

            // Check actual file size
            if (file_obj.len) {
                const rk_ChunkEntry *entry = (const rk_ChunkEntry *)(file_obj.end() - RG_SIZE(rk_ChunkEntry));
                int64_t len = LittleEndian(entry->offset) + LittleEndian(entry->len);

                if (RG_UNLIKELY(len != file_len)) {
                    LogError("File size mismatch for '%1'", entry->id);
                    return -1;
                }
            }
        } break;

        case rk_ObjectType::Chunk: {
            file_len = file_obj.len;

            if (!WriteAt(fd, dest_filename, 0, file_obj)) {
                LogError("Failed to write to '%1': %2", dest_filename, strerror(errno));
                return -1;
            }
        } break;

        case rk_ObjectType::Directory1:
        case rk_ObjectType::Directory2:
        case rk_ObjectType::Directory3:
        case rk_ObjectType::Snapshot1:
        case rk_ObjectType::Snapshot2:
        case rk_ObjectType::Snapshot3:
        case rk_ObjectType::Link: { RG_UNREACHABLE(); } break;
    }

    if (!FlushFile(fd, dest_filename))
        return -1;

    stat_len += file_len;

    err_guard.Disable();
    return fd;
}

bool rk_Get(rk_Disk *disk, const rk_ID &id, const rk_GetSettings &settings, const char *dest_path, int64_t *out_len)
{
    rk_ObjectType type;
    HeapArray<uint8_t> obj;
    if (!disk->ReadObject(id, &type, &obj))
        return false;

    GetContext get(disk);

    switch (type) {
        case rk_ObjectType::Chunk:
        case rk_ObjectType::File: {
            if (TestFile(dest_path) && !IsDirectoryEmpty(dest_path)) {
                LogError("File '%1' already exists", dest_path);
                return false;
            }

            int fd = get.GetFile(id, type, obj, dest_path);
            if (fd < 0)
                return false;
            close(fd);
        } break;

        case rk_ObjectType::Directory1:
        case rk_ObjectType::Directory2:
        case rk_ObjectType::Directory3: {
            if (TestFile(dest_path, FileType::Directory)) {
                if (!IsDirectoryEmpty(dest_path)) {
                    LogError("Directory '%1' exists and is not empty", dest_path);
                    return false;
                }
            } else {
                if (!MakeDirectory(dest_path))
                    return false;
            }

            if (!get.ExtractEntries(type, obj, 0, dest_path))
                return false;
        } break;

        case rk_ObjectType::Snapshot1:
        case rk_ObjectType::Snapshot2:
        case rk_ObjectType::Snapshot3: {
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
            if (obj.len <= RG_SIZE(rk_SnapshotHeader)) {
                LogError("Malformed snapshot object '%1'", id);
                return false;
            }

            Span<uint8_t> entries = obj.Take(RG_SIZE(rk_SnapshotHeader), obj.len - RG_SIZE(rk_SnapshotHeader));
            unsigned int flags = (int)ExtractFlag::AllowSeparators | (settings.flat ? (int)ExtractFlag::FlattenName : 0);

            if (!get.ExtractEntries(type, entries, flags, dest_path))
                return false;
        } break;

        case rk_ObjectType::Link: {
            obj.Append(0);

            if (!CreateSymbolicLink(dest_path, (const char *)obj.ptr))
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

bool rk_List(rk_Disk *disk, Allocator *str_alloc, HeapArray<rk_SnapshotInfo> *out_snapshots)
{
    Size prev_len = out_snapshots->len;
    RG_DEFER_N(out_guard) { out_snapshots->RemoveFrom(prev_len); };

    HeapArray<rk_ID> ids;
    if (!disk->ListTags(&ids))
        return false;

    Async async(disk->GetThreads());

    // Gather snapshot information
    {
        std::mutex mutex;

        for (const rk_ID &id: ids) {
            async.Run([=, &mutex]() {
                rk_SnapshotInfo snapshot = {};

                rk_ObjectType type;
                HeapArray<uint8_t> obj;
                if (!disk->ReadObject(id, &type, &obj))
                    return false;

                if (type != rk_ObjectType::Snapshot1 && type != rk_ObjectType::Snapshot2) {
                    LogError("Object '%1' is not a snapshot (ignoring)", id);
                    return true;
                }
                if (obj.len <= RG_SIZE(rk_SnapshotHeader)) {
                    LogError("Malformed snapshot object '%1' (ignoring)", id);
                    return true;
                }

                std::lock_guard lock(mutex);
                const rk_SnapshotHeader *header = (const rk_SnapshotHeader *)obj.ptr;

                snapshot.id = id;
                snapshot.name = header->name[0] ? DuplicateString(header->name, str_alloc).ptr : nullptr;
                snapshot.time = LittleEndian(header->time);
                snapshot.len = LittleEndian(header->len);
                snapshot.stored = LittleEndian(header->stored) + obj.len;

                out_snapshots->Append(snapshot);

                return true;
            });
        }
    }

    if (!async.Sync())
        return false;

    std::sort(out_snapshots->ptr + prev_len, out_snapshots->end(),
              [](const rk_SnapshotInfo &snapshot1, const rk_SnapshotInfo &snapshot2) { return snapshot1.time < snapshot2.time; });

    out_guard.Disable();
    return true;
}

}
