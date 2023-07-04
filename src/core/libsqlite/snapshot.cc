// Copyright 2023 Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in 
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "src/core/libcc/libcc.hh"
#include "snapshot.hh"
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
#define SNAPSHOT_VERSION 2
#define SNAPSHOT_SIGNATURE "SQLITESNAPSHOT"

// This should warn us in most cases when we break the file format
static_assert(RG_SIZE(SnapshotHeader::signature) == RG_SIZE(SNAPSHOT_SIGNATURE));
static_assert(RG_SIZE(SnapshotHeader) == 20);
static_assert(RG_SIZE(FrameData) == 40);

bool sq_Database::SetSnapshotDirectory(const char *directory, int64_t full_delay)
{
    RG_ASSERT(!snapshot);

    LockExclusive();
    RG_DEFER { UnlockExclusive(); };

    RG_DEFER_N(err_guard) {
        snapshot_main_writer.Close();
        snapshot_wal_reader.Close();
        snapshot_wal_writer.Close();
    };

    const char *db_filename = sqlite3_db_filename(db, "main");
    const char *wal_filename = sqlite3_filename_wal(db_filename);

    // Reset snapshot information
    snapshot_path_buf.Clear();
    Fmt(&snapshot_path_buf, "%1%/", directory);
    snapshot_full_delay = full_delay;
    snapshot_frame = 0;
    snapshot_data = false;

    // Configure database to let us manipulate the WAL manually
    if (!RunMany(R"(PRAGMA locking_mode = EXCLUSIVE;
                    PRAGMA journal_mode = WAL;
                    PRAGMA auto_vacuum = 0;
                    PRAGMA cache_spill = false;)"))
        return false;

    // Open permanent WAL stream
    if (snapshot_wal_reader.Open(wal_filename) != OpenResult::Success)
        return false;

    // Set up WAL hook to copy new pages
    sqlite3_wal_hook(db, [](void *udata, sqlite3 *, const char *, int) {
        sq_Database *db = (sq_Database *)udata;
        db->snapshot_cv.notify_one();
        return SQLITE_OK;
    }, this);

    // Start snapshot mode
    snapshot = true;
    snapshot_thread = std::thread(&sq_Database::RunCopyThread, this);

    err_guard.Disable();

    return true;
}

bool sq_Database::StopSnapshot()
{
    bool success = true;

    if (!snapshot)
        return true;

    success &= Checkpoint();

    if (snapshot_thread.joinable()) {
        // Wake up copy thread if needed
        {
            std::lock_guard<std::mutex> lock(snapshot_mutex);

            snapshot = false;
            snapshot_cv.notify_one();
        }

        // And wait for it to end!
        snapshot_thread.join();
    }

    snapshot_main_writer.Close();
    snapshot_wal_reader.Close();
    snapshot_wal_writer.Close();

    snapshot = false;

    return success;
}

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

    bool locked = false;
    bool success = true;

    RG_DEFER {
        if (locked) {
            UnlockExclusive();
        }

        if (!success) {
            // If anything went wrong, do a full snapshot next time
            // Assuming the caller wants to carry on :)
            snapshot_start = 0;
        }
    };

    snapshot_checkpointing = true;
    RG_DEFER { snapshot_checkpointing = false; };

    std::lock_guard<std::mutex> lock(snapshot_mutex);

    // Restart snapshot stream if forced or needed
    restart |= !snapshot_wal_writer.IsValid();
    restart |= (now - snapshot_start >= snapshot_full_delay);

    if (restart) {
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
            Fmt(&snapshot_path_buf, ".%1", FmtArg(0).Pad0(-16));

            StreamReader reader(db_filename.ptr);
            StreamWriter writer(snapshot_path_buf.ptr, (int)StreamWriterFlag::Atomic,
                                CompressionType::LZ4, CompressionSpeed::Fast);

            FrameData frame;
            frame.mtime = LittleEndian(now);

            success &= SpliceWithChecksum(&reader, &writer, frame.sha256);
            success &= snapshot_main_writer.Write((const uint8_t *)&frame, RG_SIZE(frame));
        }

        // Flush snapshot header to disk
        success &= snapshot_main_writer.Flush();

        locked = !LockExclusive();
        RG_ASSERT(locked);

        // Restart WAL frame copies
        snapshot_start = now;
        snapshot_frame = 0;
        success &= OpenNextFrame(now);

        if (!snapshot_data)
            return success;
    } else {
        if (!snapshot_data)
            return success;

        locked = !LockExclusive();
        RG_ASSERT(locked);
    }

    success &= CopyWAL(true);

retry:
    // Perform SQLite checkpoint, with truncation so that we can just copy each WAL file
    int ret = sqlite3_wal_checkpoint_v2(db, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr);
    if (ret != SQLITE_OK) {
        if (success && ret == SQLITE_LOCKED) {
            lock_reads = true;

            WaitDelay(10);
            goto retry;
        }

        LogError("SQLite checkpoint failed: %1", sqlite3_errmsg(db));
        success = false;
    }

    lock_reads = false;
    success &= OpenNextFrame(now);

    return success;
}

// Call with exclusive lock!
bool sq_Database::OpenNextFrame(int64_t now)
{
    bool success = true;

    // Write frame checksum
    if (snapshot_frame) {
        FrameData frame;
        frame.mtime = LittleEndian(now);
        crypto_hash_sha256_final(&snapshot_wal_state, frame.sha256);

        success &= snapshot_main_writer.Write((const uint8_t *)&frame, RG_SIZE(frame));
        success &= snapshot_main_writer.Flush();
    }

    snapshot_frame++;
    snapshot_data = false;

    RG_DEFER_C(len = snapshot_path_buf.len) {
        snapshot_path_buf.RemoveFrom(len);
        snapshot_path_buf.ptr[snapshot_path_buf.len] = 0;
    };
    Fmt(&snapshot_path_buf, ".%1", FmtArg(snapshot_frame).Pad0(-16));

    // Open new WAL copy for writing
    success &= snapshot_wal_writer.Close();
    success &= snapshot_wal_writer.Open(snapshot_path_buf.ptr, 0,
                                        CompressionType::LZ4, CompressionSpeed::Fast);

    // Rewind WAL reader
    success &= snapshot_wal_reader.Rewind();
    crypto_hash_sha256_init(&snapshot_wal_state);

    return success;
}

void sq_Database::RunCopyThread()
{
    std::unique_lock<std::mutex> lock(snapshot_mutex);

    while (snapshot) {
        CopyWAL(false);
        snapshot_cv.wait(lock);
    }
}

bool sq_Database::CopyWAL(bool full)
{
    while (full || !snapshot_checkpointing.load(std::memory_order_relaxed)) {
        LocalArray<uint8_t, 16384> buf;

        buf.len = snapshot_wal_reader.Read(buf.data);
        if (buf.len < 0)
            return false;
        if (!buf.len)
            break;

        if (!snapshot_wal_writer.Write(buf))
            return false;
        crypto_hash_sha256_update(&snapshot_wal_state, buf.data, buf.len);

        snapshot_data = true;
    }

    return true;
}

Size sq_SnapshotInfo::FindFrame(int64_t mtime) const
{
    Size frame_idx = 0;

    while (++frame_idx < frames.len && frames[frame_idx].mtime <= mtime);
    frame_idx--;

    return frame_idx;
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
        char *orig_filename = (char *)AllocateRaw(&out_set->str_alloc, sh.filename_len + 1);
        if (st.Read(sh.filename_len, orig_filename) != sh.filename_len) {
            LogError("Truncated snapshot header in '%1' (skipping)", filename);
            continue;
        }
        orig_filename[sh.filename_len] = 0;

        // Insert or reuse previous snapshot
        sq_SnapshotInfo *snapshot;
        {
            Size prev_idx = *snapshots_map.TrySet(orig_filename, out_set->snapshots.len);

            if (prev_idx >= out_set->snapshots.len) {
                snapshot = out_set->snapshots.AppendDefault();
                snapshot->orig_filename = orig_filename;
            } else {
                snapshot = &out_set->snapshots[prev_idx];
            }
        }
        RG_DEFER {
            if (!snapshot->generations.len) {
                out_set->snapshots.RemoveLast(1);
                snapshots_map.Remove(orig_filename);
            }
        };

        sq_SnapshotGeneration generation = {};

        generation.base_filename = DuplicateString(filename, &out_set->str_alloc).ptr;
        generation.frame_idx = snapshot->frames.len;

        // Read snapshot frames
        do {
            sq_SnapshotFrame frame = {};

            FrameData raw_frame;
            if (Size read_len = st.Read(RG_SIZE(raw_frame), &raw_frame); read_len != RG_SIZE(raw_frame)) {
                if (read_len) {
                    LogError("Truncated snapshot frame in '%1' (ignoring)", filename);
                }
                break;
            }
            raw_frame.mtime = LittleEndian(raw_frame.mtime);

            frame.generation_idx = snapshot->generations.len;
            frame.mtime = raw_frame.mtime;
            memcpy_safe(frame.sha256, raw_frame.sha256, RG_SIZE(frame.sha256));

            snapshot->frames.Append(frame);
        } while (!st.IsEOF());
        if (!st.IsValid())
            continue;

        generation.frames = snapshot->frames.len - generation.frame_idx;
        if (!generation.frames) {
            LogError("Empty snapshot file '%1' (skipping)", filename);
            continue;
        }
        generation.ctime = snapshot->frames[generation.frame_idx].mtime;
        generation.mtime = snapshot->frames[generation.frame_idx + generation.frames - 1].mtime;

        // Commit generation (and snapshot)
        snapshot->generations.Append(generation);
    }

    for (sq_SnapshotInfo &snapshot: out_set->snapshots) {
        std::sort(snapshot.generations.begin(), snapshot.generations.end(),
                  [](sq_SnapshotGeneration &version1, sq_SnapshotGeneration &version2) {
            return version1.mtime < version2.mtime;
        });

        snapshot.ctime = snapshot.generations[0].ctime;
        snapshot.mtime = snapshot.generations[snapshot.generations.len - 1].mtime;
    }

    out_guard.Disable();
    return true;
}

bool sq_RestoreSnapshot(const sq_SnapshotInfo &snapshot, Size frame_idx, const char *dest_filename, bool overwrite)
{
    BlockAllocator temp_alloc;

    const sq_SnapshotGeneration *generation;
    if (frame_idx >= 0) {
        const sq_SnapshotFrame &frame = snapshot.frames[frame_idx];
        generation = &snapshot.generations[frame.generation_idx];
    } else {
        if (!snapshot.frames.len) {
            LogError("This snapshot does not contain any frame");
            return false;
        }

        generation = &snapshot.generations[snapshot.generations.len - 1];
        frame_idx = snapshot.frames.len - 1;
    }

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
    Fmt(&path_buf, "%1", generation->base_filename);

    // Copy initial database
    {
        const sq_SnapshotFrame &frame = snapshot.frames[generation->frame_idx];

        RG_DEFER_C(len = path_buf.len) { path_buf.ptr[path_buf.len = len] = 0; };
        Fmt(&path_buf, ".%1", FmtArg(0).Pad0(-16));

        StreamReader reader(path_buf.ptr, CompressionType::LZ4);
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
    for (Size i = 1, j = generation->frame_idx + 1; j <= frame_idx; i++, j++) {
        const sq_SnapshotFrame &frame = snapshot.frames[j];

        RG_DEFER_C(len = path_buf.len) { path_buf.ptr[path_buf.len = len] = 0; };
        Fmt(&path_buf, ".%1", FmtArg(i).Pad0(-16));

        StreamReader reader(path_buf.ptr, CompressionType::LZ4);
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

    return true;
}

}
