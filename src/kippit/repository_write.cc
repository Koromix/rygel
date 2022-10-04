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

namespace RG {

static const Size ChunkAverage = Kibibytes(1024);
static const Size ChunkMin = Kibibytes(512);
static const Size ChunkMax = Kibibytes(2048);

static void HashBlake3(Span<const uint8_t> buf, const uint8_t *salt, kt_ID *out_id)
{
    blake3_hasher hasher;

    blake3_hasher_init_keyed(&hasher, salt);
    blake3_hasher_update(&hasher, buf.ptr, buf.len);
    blake3_hasher_finalize(&hasher, out_id->hash, RG_SIZE(out_id->hash));
}

enum class PutResult {
    Success,
    Ignore,
    Error
};

static PutResult PutFile(kt_Disk *disk, const char *src_filename,
                         kt_ID *out_id, int64_t *out_len, int64_t *out_written)
{
    Span<const uint8_t> salt = disk->GetSalt();
    RG_ASSERT(salt.len == BLAKE3_KEY_LEN); // 32 bytes

    StreamReader st;
    {
        OpenResult ret = st.Open(src_filename);
        if (ret != OpenResult::Success) {
            bool ignore = (ret == OpenResult::AccessDenied || ret == OpenResult::MissingPath);
            return ignore ? PutResult::Ignore : PutResult::Error;
        }
    }

    HeapArray<uint8_t> file_obj;
    int64_t file_len = 0;
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
                return PutResult::Error;
            buf.len += read;
            file_len += read;

            Span<const uint8_t> remain = buf;

            // We can't relocate in the inner loop
            Size needed = (remain.len / ChunkMin + 1) * RG_SIZE(kt_ChunkEntry) + 8;
            file_obj.Grow(needed);

            // Chunk file and write chunks out in parallel
            do {
                Size processed = chunker.Process(remain, st.IsEOF(), [&](Size idx, int64_t total, Span<const uint8_t> chunk) {
                    RG_ASSERT(idx * RG_SIZE(kt_ChunkEntry) == file_obj.len);
                    file_obj.len += RG_SIZE(kt_ChunkEntry);

                    async.Run([=, &file_written, &file_obj]() {
                        kt_ChunkEntry entry = {};

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
        file_written += ret;
    } else {
        const kt_ChunkEntry *entry0 = (const kt_ChunkEntry *)file_obj.ptr;
        file_id = entry0->id;
    }

    *out_id = file_id;
    if (out_len) {
        *out_len += file_len;
    }
    if (out_written) {
        *out_written += file_written;
    }
    return PutResult::Success;
}

static PutResult PutDirectory(kt_Disk *disk, const char *src_dirname,
                              kt_ID *out_id, int64_t *out_len, int64_t *out_written)
{
    BlockAllocator temp_alloc;

    Span<const uint8_t> salt = disk->GetSalt();
    RG_ASSERT(salt.len == BLAKE3_KEY_LEN); // 32 bytes

    HeapArray<uint8_t> dir_obj;
    int64_t total_written = 0;

    src_dirname = DuplicateString(TrimStrRight(src_dirname, RG_PATH_SEPARATORS), &temp_alloc).ptr;

    EnumResult ret = EnumerateDirectory(src_dirname, nullptr, -1,
                                        [&](const char *basename, FileType) {
        const char *filename = Fmt(&temp_alloc, "%1%/%2", src_dirname, basename).ptr;

        Size entry_len = RG_SIZE(kt_FileEntry) + strlen(basename) + 1;
        kt_FileEntry *entry = (kt_FileEntry *)dir_obj.AppendDefault(entry_len);
        RG_DEFER_N(entry_guard) { dir_obj.RemoveLast(entry_len); };

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
                entry->kind = (int8_t)kt_FileEntry::Kind::Directory;

                PutResult ret = PutDirectory(disk, filename, &entry->id, out_len, &total_written);

                if (ret != PutResult::Success) {
                    bool ignore = (ret == PutResult::Ignore);
                    return ignore;
                }
            } break;
            case FileType::File: {
                entry->kind = (int8_t)kt_FileEntry::Kind::File;

                PutResult ret = PutFile(disk, filename, &entry->id, out_len, &total_written);

                if (ret != PutResult::Success) {
                    bool ignore = (ret == PutResult::Ignore);
                    return ignore;
                }
            } break;

            case FileType::Link:
            case FileType::Device:
            case FileType::Pipe:
            case FileType::Socket: {
                LogWarning("Ignoring special file '%1' (%2)", filename, FileTypeNames[(int)file_info.type]);
                return true;
            } break;
        }

        entry->mtime = file_info.mtime;
        entry->mode = (uint32_t)file_info.mode;
        CopyString(basename, MakeSpan(entry->name, entry_len - RG_SIZE(kt_FileEntry)));

        entry_guard.Disable();
        return true;
    });
    if (ret != EnumResult::Success) {
        bool ignore = (ret == EnumResult::AccessDenied || ret == EnumResult::MissingPath);
        return ignore ? PutResult::Ignore : PutResult::Error;
    }

    kt_ID dir_id = {};
    {
        HashBlake3(dir_obj, salt.ptr, &dir_id);

        Size ret = disk->WriteObject(dir_id, kt_ObjectType::Directory, dir_obj);
        if (ret < 0)
            return PutResult::Error;
        total_written += ret;
    }

    *out_id = dir_id;
    if (out_written) {
        *out_written += total_written;
    }
    return PutResult::Success;
}

bool kt_Put(kt_Disk *disk, const kt_PutSettings &settings, Span<const char *const> filenames,
            kt_ID *out_id, int64_t *out_len, int64_t *out_written)
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

    int64_t total_len = 0;
    int64_t total_written = 0;

    // Process snapshot entries
    for (const char *filename: filenames) {
        Size entry_len = RG_SIZE(kt_FileEntry) + strlen(filename) + 1;
        kt_FileEntry *entry = (kt_FileEntry *)snapshot_obj.AppendDefault(entry_len);

        Span<const char> name;
        {
            Span<char> dest = MakeSpan(entry->name, entry_len - RG_SIZE(kt_FileEntry));
            bool changed = false;

            name = TrimStrRight(filename, RG_PATH_SEPARATORS);

            // Change Windows paths, even on non-Windows platforms
            if (IsAsciiAlpha(name.ptr[0]) && name.ptr[1] == ':') {
                entry->name[0] = LowerAscii(name[0]);
                entry->name[1] = '/';

                dest = dest.Take(2, dest.len - 2);
                name = name.Take(2, name.len - 2);
                changed = true;
            }

            while (strchr("/\\", name.ptr[0])) {
                snapshot_obj.len--;
                entry_len--;

                name = name.Take(1, name.len - 1);

                changed = true;
            }

            if (!name.len) {
                LogError("Cannot backup empty filename");
                return false;
            }

            for (Size i = 0; i < name.len; i++) {
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
                entry->kind = (int8_t)kt_FileEntry::Kind::Directory;

                if (PutDirectory(disk, filename, &entry->id, &total_len, &total_written) != PutResult::Success)
                    return false;
            } break;
            case FileType::File: {
                entry->kind = (int8_t)kt_FileEntry::Kind::File;

                if (PutFile(disk, filename, &entry->id, &total_len, &total_written) != PutResult::Success)
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
        header->len = LittleEndian(total_len);
        header->stored = LittleEndian(total_written);

        HashBlake3(snapshot_obj, salt.ptr, &id);

        // Write snapshot object
        {
            Size ret = disk->WriteObject(id, kt_ObjectType::Snapshot, snapshot_obj);
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
