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

#include "../libcc/libcc.hh"
#include "collect.hh"
#include "sqlite.hh"

namespace RG {

#pragma pack(push, 1)
struct SnapshotHeader {
    char signature[15];
    int8_t version;
    int32_t filename_len;
};
struct FrameData {
    int64_t mtime;
    uint8_t sha256[32];
};
#pragma pack(pop)
#define SNAPSHOT_VERSION 1
#define SNAPSHOT_SIGNATURE "SQLITESNAPSHOT"

// This should warn us in most cases when we break the file format
RG_STATIC_ASSERT(RG_SIZE(SnapshotHeader::signature) == RG_SIZE(SNAPSHOT_SIGNATURE));
RG_STATIC_ASSERT(RG_SIZE(SnapshotHeader) == 20);
RG_STATIC_ASSERT(RG_SIZE(FrameData) == 40);

static bool SpliceWithChecksum(StreamReader *reader, StreamWriter *writer, uint8_t out_hash[32])
{
    if (!reader->IsValid())
        return false;

    crypto_hash_sha256_state state;
    crypto_hash_sha256_init(&state);

    do {
        LocalArray<uint8_t, 16384> buf;
        buf.len = reader->Read(buf.data);
        if (buf.len < 0)
            return false;

        if (!writer->Write(buf))
            return false;
        crypto_hash_sha256_update(&state, buf.data, buf.len);
    } while (!reader->IsEOF());

    if (!writer->Close())
        return false;
    crypto_hash_sha256_final(&state, out_hash);

    return true;
}

bool sq_Database::CheckpointSnapshot(bool restart)
{
    Span<const char> db_filename = sqlite3_db_filename(db, "main");
    int64_t now = GetUnixTime();

    bool success = true;

    bool locked = false;
    RG_DEFER {
        if (locked) {
            UnlockExclusive();
        }
    };

    // Restart snapshot stream if needed
    if (restart || now - snapshot_start >= snapshot_full_delay) {
        snapshot_path_buf.len = SplitStrReverseAny(snapshot_path_buf, RG_PATH_SEPARATORS).ptr - snapshot_path_buf.ptr;
        snapshot_path_buf.ptr[snapshot_path_buf.len] = 0;

        // Start new checksum file
        {
            Size base_len = snapshot_path_buf.len;

            snapshot_main_writer.Close();
            for (int i = 0; i < 1000; i++) {
                snapshot_path_buf.len = base_len;
                Fmt(&snapshot_path_buf, "%1.dbsnap", FmtRandom(24));

                if (snapshot_main_writer.Open(snapshot_path_buf.ptr, (int)StreamWriterFlag::Exclusive))
                    break;
            }

            SnapshotHeader sh;
            CopyString(SNAPSHOT_SIGNATURE, sh.signature);
            sh.version = SNAPSHOT_VERSION;
            sh.filename_len = LittleEndian((int32_t)db_filename.len);

            success &= snapshot_main_writer.Write(&sh, RG_SIZE(sh));
            success &= snapshot_main_writer.Write(db_filename);
        }

        // Perform initial copy
        {
            RG_DEFER_C(len = snapshot_path_buf.len) {
                snapshot_path_buf.RemoveFrom(len);
                snapshot_path_buf.ptr[snapshot_path_buf.len] = 0;
            };
            Fmt(&snapshot_path_buf, "-%1", FmtHex(0).Pad0(-16));

            StreamReader reader(db_filename.ptr);
            StreamWriter writer(snapshot_path_buf.ptr, (int)StreamWriterFlag::Atomic,
                                CompressionType::Gzip, CompressionSpeed::Fast);

            FrameData frame;
            frame.mtime = LittleEndian(now);

            success &= SpliceWithChecksum(&reader, &writer, frame.sha256);
            success &= snapshot_main_writer.Write((const uint8_t *)&frame, RG_SIZE(frame));
            success &= snapshot_main_writer.Flush();
        }

        LockExclusive();
        locked = true;

        // Delete all WAL copies
        {
            snapshot_wal_writer.Close();

            for (Size i = 1;; i++) {
                RG_DEFER_C(len = snapshot_path_buf.len) {
                    snapshot_path_buf.RemoveFrom(len);
                    snapshot_path_buf.ptr[snapshot_path_buf.len] = 0;
                };
                Fmt(&snapshot_path_buf, "-%1", FmtHex(i).Pad0(-16));

                if (!TestFile(snapshot_path_buf.ptr))
                    break;

                success &= UnlinkFile(snapshot_path_buf.ptr);
            }
        }

        snapshot_start = now;
        snapshot_idx = 0;
        snapshot_data = true;
    } else {
        LockExclusive();
        locked = true;

        success &= CopyWAL();
    }

    // Perform SQLite checkpoint, with truncation so that we can just copy each WAL file
    int ret = sqlite3_wal_checkpoint_v2(db, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr);
    if (ret != SQLITE_OK) {
        if (success && ret == SQLITE_LOCKED) {
            LogDebug("Could not checkpoint because of connection LOCK, will try again later");
            return true;
        }

        LogError("SQLite checkpoint failed: %1", sqlite3_errmsg(db));
        success = false;
    }

    // Switch to next WAL copy
    if (snapshot_data) {
        snapshot_idx++;
        snapshot_data = false;

        RG_DEFER_C(len = snapshot_path_buf.len) {
            snapshot_path_buf.RemoveFrom(len);
            snapshot_path_buf.ptr[snapshot_path_buf.len] = 0;
        };
        Fmt(&snapshot_path_buf, "-%1", FmtHex(snapshot_idx).Pad0(-16));

        if (snapshot_idx > 1) {
            FrameData frame;
            frame.mtime = LittleEndian(now);
            crypto_hash_sha256_final(&snapshot_wal_state, frame.sha256);

            success &= snapshot_main_writer.Write((const uint8_t *)&frame, RG_SIZE(frame));
            success &= snapshot_main_writer.Flush();
        }

        // Open new WAL copy for writing
        success &= snapshot_wal_writer.Close();
        success &= snapshot_wal_writer.Open(snapshot_path_buf.ptr, 0, CompressionType::Gzip, CompressionSpeed::Fast);

        // Rewind WAL reader
        success &= snapshot_wal_reader.Rewind();
        crypto_hash_sha256_init(&snapshot_wal_state);
    }

    // If anything went wrong, do a full snapshot next time
    // Assuming the caller wants to carry on :)
    if (!success) {
        snapshot_start = 0;
    }

    return success;
}

bool sq_Database::CopyWAL()
{
    do {
        LocalArray<uint8_t, 16384> buf;
        buf.len = snapshot_wal_reader.Read(buf.data);
        if (buf.len < 0)
            return false;

        if (!snapshot_wal_writer.Write(buf))
            return false;
        crypto_hash_sha256_update(&snapshot_wal_state, buf.data, buf.len);

        snapshot_data |= buf.len;
    } while (!snapshot_wal_reader.IsEOF());

    return true;
}

bool sq_CollectSnapshots(Span<const char *> filenames, sq_SnapshotSet *out_set)
{
    RG_ASSERT(!out_set->snapshots.len);

    RG_DEFER_N(out_guard) {
        out_set->snapshots.Clear();
        out_set->str_alloc.ReleaseAll();
    };

    HashMap<const char *, Size> snapshots_map;

    for (const char *filename: filenames) {
        StreamReader st(filename);
        if (!st.IsValid())
            return false;

        SnapshotHeader sh;
        if (st.Read(RG_SIZE(sh), &sh) != RG_SIZE(sh)) {
            LogError("Truncated snapshot header in '%1' (skipping)", filename);
            continue;
        }
        if (strncmp(sh.signature, SNAPSHOT_SIGNATURE, RG_SIZE(sh.signature)) != 0) {
            LogError("File '%1' does not have snapshot signature", filename);
            return false;
        }
        if (sh.version != SNAPSHOT_VERSION) {
            LogError("Cannot load '%1' (version %2), expected version %3", filename, sh.version, SNAPSHOT_VERSION);
            return false;
        }
        sh.filename_len = LittleEndian(sh.filename_len);

        // Read original filename
        char *orig_filename = (char *)Allocator::Allocate(&out_set->str_alloc, sh.filename_len + 1, 0);
        if (st.Read(sh.filename_len, orig_filename) != sh.filename_len) {
            LogError("Truncated snapshot header in '%1' (skipping)", filename);
            continue;
        }
        orig_filename[sh.filename_len] = 0;

        // Insert or reuse previous snapshot
        sq_SnapshotInfo *snapshot;
        {
            Size prev_idx = *snapshots_map.TrySet(orig_filename, out_set->snapshots.len).first;

            if (prev_idx >= out_set->snapshots.len) {
                snapshot = out_set->snapshots.AppendDefault();
                snapshot->orig_filename = orig_filename;
            } else {
                snapshot = &out_set->snapshots[prev_idx];
            }
        }
        RG_DEFER {
            if (!snapshot->versions.len) {
                out_set->snapshots.RemoveLast(1);
                snapshots_map.Remove(orig_filename);
            }
        };

        sq_SnapshotInfo::Version version = {};

        version.base_filename = DuplicateString(filename, &out_set->str_alloc).ptr;
        version.frame_idx = snapshot->frames.len;

        // Read snapshot frames
        do {
            sq_SnapshotInfo::Frame frame = {};

            FrameData raw_frame;
            if (Size read_len = st.Read(RG_SIZE(raw_frame), &raw_frame); read_len != RG_SIZE(raw_frame)) {
                if (read_len) {
                    LogError("Truncated snapshot frame in '%1' (ignoring)", filename);
                }
                break;
            }
            raw_frame.mtime = LittleEndian(raw_frame.mtime);

            frame.mtime = raw_frame.mtime;
            memcpy_safe(frame.sha256, raw_frame.sha256, RG_SIZE(frame.sha256));

            snapshot->frames.Append(frame);
        } while (!st.IsEOF());
        if (!st.IsValid())
            continue;

        version.frames = snapshot->frames.len - version.frame_idx;
        if (!version.frames) {
            LogError("Empty snapshot file '%1' (skipping)", filename);
            continue;
        }
        version.ctime = snapshot->frames[version.frame_idx].mtime;
        version.mtime = snapshot->frames[version.frame_idx + version.frames - 1].mtime;

        // Commit version (and snapshot)
        snapshot->versions.Append(version);
    }

    for (sq_SnapshotInfo &snapshot: out_set->snapshots) {
        std::sort(snapshot.versions.begin(), snapshot.versions.end(),
                  [](sq_SnapshotInfo::Version &version1, sq_SnapshotInfo::Version &version2) {
            return version1.ctime < version2.ctime;
        });

        snapshot.ctime = snapshot.versions[0].ctime;
        snapshot.mtime = snapshot.versions[snapshot.versions.len - 1].mtime;
    }

    out_guard.Disable();
    return true;
}

static void LogFrameTime(const char *type, const char *filename, int64_t mtime)
{
    const char *basename = SplitStrReverseAny(filename, RG_PATH_SEPARATORS).ptr;
    TimeSpec spec = DecomposeTime(mtime);

    LogInfo("Restoring %1 '%2' (%3)", type, basename, spec);
}

bool sq_RestoreSnapshot(const sq_SnapshotInfo &snapshot, const char *dest_filename, bool overwrite)
{
    BlockAllocator temp_alloc;

    const sq_SnapshotInfo::Version &version = snapshot.versions[snapshot.versions.len - 1];

    const char *wal_filename = Fmt(&temp_alloc, "%1-wal", dest_filename).ptr;
    RG_DEFER { UnlinkFile(wal_filename); };

    // Safety check
    if (overwrite) {
        UnlinkFile(dest_filename);
    } else if (TestFile(dest_filename)) {
        LogError("Refusing to overwrite '%1'", dest_filename);
        return false;
    }
    UnlinkFile(wal_filename);

    HeapArray<char> path_buf(&temp_alloc);
    Fmt(&path_buf, "%1", version.base_filename);

    // Copy initial database
    {
        const sq_SnapshotInfo::Frame &frame = snapshot.frames[version.frame_idx];

        RG_DEFER_C(len = path_buf.len) { path_buf.ptr[path_buf.len = len] = 0; };
        Fmt(&path_buf, "-%1", FmtHex(0).Pad0(-16));

        LogFrameTime("database", path_buf.ptr, frame.mtime);

        StreamReader reader(path_buf.ptr, CompressionType::Gzip);
        StreamWriter writer(dest_filename);
        uint8_t sha256[32];

        if (!SpliceWithChecksum(&reader, &writer, sha256))
            return false;

        if (memcmp(sha256, frame.sha256, RG_SIZE(sha256))) {
            LogError("Database copy checksum does not match");
            return false;
        }
    }

    // Apply WAL copies
    for (Size i = 1; i < version.frames; i++) {
        const sq_SnapshotInfo::Frame &frame = snapshot.frames[version.frame_idx + i];

        RG_DEFER_C(len = path_buf.len) { path_buf.ptr[path_buf.len = len] = 0; };
        Fmt(&path_buf, "-%1", FmtHex(i).Pad0(-16));

        LogFrameTime("WAL", path_buf.ptr, frame.mtime);

        StreamReader reader(path_buf.ptr, CompressionType::Gzip);
        StreamWriter writer(wal_filename);
        uint8_t sha256[32];

        if (!SpliceWithChecksum(&reader, &writer, sha256))
            return false;

        if (memcmp(sha256, frame.sha256, RG_SIZE(sha256))) {
            LogError("WAL copy checksum does not match");
            return false;
        }

        sq_Database db;
        if (!db.Open(dest_filename, SQLITE_OPEN_READWRITE))
            return false;
        if (!db.Run("PRAGMA user_version;"))
            return false;
        if (!db.Close())
            return false;

        if (TestFile(wal_filename)) {
            LogError("SQLite won't replay the WAL for some reason");
            return false;
        }
    }

    LogInfo("Database '%1' restored", dest_filename);
    return true;
}

}
