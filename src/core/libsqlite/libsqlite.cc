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
#include "libsqlite.hh"
#include "../../../vendor/libsodium/src/libsodium/include/sodium.h"

namespace RG {

const char *SnapshotSignature = "SQLITESNAPSHOT01";

#pragma pack(push, 1)
struct FrameData {
    int64_t mtime;
    uint8_t sha256[32];
};
#pragma pack(pop)

sq_Statement &sq_Statement::operator=(sq_Statement &&other)
{
    Finalize();

    db = other.db;
    stmt = other.stmt;
    other.db = nullptr;
    other.stmt = nullptr;

    return *this;
}

void sq_Statement::Finalize()
{
    if (db) {
        db->UnlockShared();
        sqlite3_finalize(stmt);
    }

    db = nullptr;
    stmt = nullptr;
}

bool sq_Statement::Run()
{
    rc = sqlite3_step(stmt);

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        LogError("SQLite Error: %1", sqlite3_errmsg(sqlite3_db_handle(stmt)));
        return false;
    }

    return true;
}

bool sq_Statement::Next()
{
    return Run() && rc == SQLITE_ROW;
}

void sq_Statement::Reset()
{
    int ret = sqlite3_reset(stmt);
    RG_ASSERT(ret == SQLITE_OK);
}

sqlite3_stmt *sq_Statement::Leak()
{
    RG_ASSERT(db);

    sqlite3_stmt *copy = stmt;

    db->UnlockShared();
    db = nullptr;
    stmt = nullptr;

    return copy;
}

bool sq_Database::Open(const char *filename, const uint8_t key[32], unsigned int flags)
{
    static const char *const sql = R"(
        PRAGMA locking_mode = NORMAL;
        PRAGMA foreign_keys = ON;
        PRAGMA synchronous = NORMAL;
        PRAGMA busy_timeout = 5000;
    )";

    RG_ASSERT(!db);
    RG_DEFER_N(out_guard) { Close(); };

    if (sqlite3_open_v2(filename, &db, flags, nullptr) != SQLITE_OK) {
        LogError("SQLite failed to open '%1': %2", filename, sqlite3_errmsg(db));
        return false;
    }

    if (key && sqlite3_key(db, key, 32) != SQLITE_OK) {
        LogError("SQLite failed to open '%1': %2", filename, sqlite3_errmsg(db));
        return false;
    }

    char *error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        LogError("SQLite failed to open '%1': %2", filename, error);
        return false;
    }

    out_guard.Disable();
    return true;
}

bool sq_Database::SetWAL(bool enable)
{
    const char *sql = enable ? "PRAGMA journal_mode = WAL" : "PRAGMA journal_mode = DELETE";
    return Run(sql);
}

bool sq_Database::SetSynchronousFull(bool enable)
{
    const char *sql = enable ? "PRAGMA synchronous = FULL" : "PRAGMA synchronous = NORMAL";
    return Run(sql);
}

bool sq_Database::SetSnapshotFile(const char *filename, int64_t full_delay)
{
    RG_ASSERT(!snapshot);

    // Configure database to let us manipulate the WAL manually
    if (!RunMany(R"(PRAGMA locking_mode = EXCLUSIVE;
                    PRAGMA journal_mode = WAL;
                    PRAGMA auto_vacuum = 0;)"))
        return false;

    // Disable WAL autocommit and use the hook to set flag when WAL contains pages
    sqlite3_wal_hook(db, [](void *udata, sqlite3 *, const char *, int) {
        sq_Database *db = (sq_Database *)udata;
        db->snapshot_progress = true;
        return SQLITE_OK;
    }, this);

    // Reset snapshot information
    snapshot_full_delay = full_delay;
    snapshot_idx = 0;
    snapshot_progress = false;

    if (!snapshot_main_writer.Open(filename))
        return false;
    if (!CheckpointSnapshot()) {
        snapshot_main_writer.Close();
        return false;
    }

    snapshot = true;
    return true;
}

bool sq_Database::Close()
{
    bool success = true;

    if (snapshot) {
        success &= Checkpoint();

        snapshot_main_writer.Close();
        snapshot = false;
    }

    int ret = sqlite3_close(db);
    if (ret != SQLITE_OK) {
        LogError("Failed to close SQLite database: %1", sqlite3_errstr(ret));
        success = false;
    }
    db = nullptr;

    return success;
}

bool sq_Database::GetUserVersion(int *out_version)
{
    sq_Statement stmt;
    if (!Prepare("PRAGMA user_version", &stmt))
        return false;
    if (!stmt.Next())
        return false;

    *out_version = sqlite3_column_int(stmt, 0);
    return true;
}

bool sq_Database::SetUserVersion(int version)
{
    char buf[128];
    Fmt(buf, "PRAGMA user_version = %1", version);
    return Run(buf);
}

bool sq_Database::Transaction(FunctionRef<bool()> func)
{
    bool nested = LockExclusive();
    RG_DEFER { UnlockExclusive(); };

    if (nested) {
        return func();
    } else {
        if (!Run("BEGIN IMMEDIATE TRANSACTION"))
            return false;
        RG_DEFER_N(rollback_guard) { Run("ROLLBACK"); };

        if (!func())
            return false;
        if (!Run("COMMIT"))
            return false;

        rollback_guard.Disable();
        return true;
    }
}

bool sq_Database::Prepare(const char *sql, sq_Statement *out_stmt)
{
    LockShared();
    RG_DEFER_N(lock_guard) { UnlockShared(); };

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogError("SQLite request failed: %1", sqlite3_errmsg(db));
        return false;
    }

    // sq_Statement will call UnlockShared()
    lock_guard.Disable();
    out_stmt->Finalize();
    out_stmt->db = this;
    out_stmt->stmt = stmt;

    return true;
}

bool sq_Database::RunMany(const char *sql)
{
    LockShared();
    RG_DEFER { UnlockShared(); };

    char *error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        LogError("SQLite request failed: %1", error);
        sqlite3_free(error);

        return false;
    }

    return true;
}

bool sq_Database::BackupTo(const char *filename)
{
    sq_Database dest_db;
    if (!dest_db.Open(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE))
        return false;
    if (!dest_db.RunMany(R"(PRAGMA locking_mode = EXCLUSIVE;
                            PRAGMA journal_mode = MEMORY;
                            PRAGMA synchronous = FULL;)"))
        return false;

    sqlite3_backup *backup = sqlite3_backup_init(dest_db, "main", db, "main");
    if (!backup)
        return false;
    RG_DEFER { sqlite3_backup_finish(backup); };

restart:
    int ret = sqlite3_backup_step(backup, -1);
    if (ret != SQLITE_DONE) {
        if (ret == SQLITE_OK || ret == SQLITE_BUSY || ret == SQLITE_LOCKED) {
            WaitDelay(100);
            goto restart;
        } else {
            LogError("SQLite Error: %1", sqlite3_errstr(ret));
            return false;
        }
    }

    sqlite3_backup_finish(backup);
    backup = nullptr;

    return dest_db.Close();
}

static bool SpliceWithChecksum(StreamReader *reader, StreamWriter *writer, uint8_t out_hash[32])
{
    if (!reader->IsValid())
        return false;

    crypto_hash_sha256_state state;
    crypto_hash_sha256_init(&state);

    while (!reader->IsEOF()) {
        LocalArray<uint8_t, 16384> buf;
        buf.len = reader->Read(buf.data);
        if (buf.len < 0)
            return false;

        if (!writer->Write(buf))
            return false;
        crypto_hash_sha256_update(&state, buf.data, buf.len);
    }

    if (!writer->Close())
        return false;
    crypto_hash_sha256_final(&state, out_hash);

    return true;
}

bool sq_Database::Checkpoint()
{
    if (snapshot) {
        return CheckpointSnapshot();
    } else {
        return CheckpointDirect();
    }
}

bool sq_Database::CheckpointSnapshot()
{
    BlockAllocator temp_alloc;

    const char *snapshot_filename = snapshot_main_writer.GetFileName();
    const char *db_filename = sqlite3_db_filename(db, "main");
    const char *wal_filename = sqlite3_filename_wal(db_filename);
    int64_t now = GetUnixTime();

    Async async;
    bool success = true;

    // Restart snapshot stream if needed
    if (!snapshot_idx || now - snapshot_start >= snapshot_full_delay) {
        success &= snapshot_main_writer.Reset();
        success &= snapshot_main_writer.Write(SnapshotSignature);

        // Perform initial copy
        {
            const char *dest_filename = Fmt(&temp_alloc, "%1-0000000000000000", snapshot_filename).ptr;

            StreamReader reader(db_filename);
            StreamWriter writer(dest_filename, (int)StreamWriterFlag::Atomic,
                                CompressionType::Gzip, CompressionSpeed::Fast);

            FrameData frame;
            frame.mtime = LittleEndian(now);

            success &= SpliceWithChecksum(&reader, &writer, frame.sha256);
            success &= snapshot_main_writer.Write((const uint8_t *)&frame, RG_SIZE(frame));
            success &= snapshot_main_writer.Flush();
        }

        // Delete all WAL copies
        {
            HeapArray<char> filename_buf(&temp_alloc);
            filename_buf.Append(snapshot_main_writer.GetFileName());

            for (Size i = 1;; i++) {
                RG_DEFER_C(len = filename_buf.len) {
                    filename_buf.RemoveFrom(len);
                    filename_buf.ptr[filename_buf.len] = 0;
                };
                Fmt(&filename_buf, "-%1", FmtHex(i).Pad0(-16));

                if (!TestFile(filename_buf.ptr))
                    break;

                success &= UnlinkFile(filename_buf.ptr);
            }
        }

        snapshot_start = now;
        snapshot_idx = 1;
    }

    bool nested = LockExclusive();
    RG_ASSERT(!nested);

    // Copy WAL file if needed
    if (snapshot_progress) {
        Span<const char> snapshot_directory = GetPathDirectory(snapshot_filename);

        FILE *fp;
        const char *tmp_filename = CreateTemporaryFile(snapshot_directory, "", ".tmp", &temp_alloc, &fp);
        const char *dest_filename = Fmt(&temp_alloc, "%1-%2", snapshot_filename, FmtHex(snapshot_idx).Pad0(-16)).ptr;

        if (tmp_filename) {
            StreamReader reader(wal_filename);
            StreamWriter writer(fp, tmp_filename);

            FrameData frame;
            frame.mtime = LittleEndian(now);

            success &= SpliceWithChecksum(&reader, &writer, frame.sha256);
            success &= snapshot_main_writer.Write((const uint8_t *)&frame, RG_SIZE(frame));
            success &= snapshot_main_writer.Flush();

            async.Run([=]() {
                RG_DEFER { 
                    fclose(fp);
                    UnlinkFile(tmp_filename);
                };

                StreamReader reader(tmp_filename);
                StreamWriter writer(dest_filename, (int)StreamWriterFlag::Atomic,
                                    CompressionType::Gzip, CompressionSpeed::Fast);

                if (!SpliceStream(&reader, -1, &writer))
                    return false;
                if (!reader.Close())
                    return false;
                if (!writer.Close())
                    return false;

                return true;
            });

            snapshot_idx++;
            snapshot_progress = false;
        } else {
            success = false;
        }
    }

    // Perform SQLite checkpoint, with truncation so that we can just copy each WAL file
    if (sqlite3_wal_checkpoint_v2(db, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr) != SQLITE_OK) {
        LogError("SQLite checkpoint failed: %1", sqlite3_errmsg(db));
        success = false;
    }

    UnlockExclusive();
    success &= async.Sync();

    // If anything goes wrong, do a full snapshot next time
    snapshot_idx = success ? snapshot_idx : 0;

    return success;
}

bool sq_Database::CheckpointDirect()
{
    bool nested = LockExclusive();
    RG_ASSERT(!nested);
    RG_DEFER { UnlockExclusive(); };

    if (sqlite3_wal_checkpoint_v2(db, nullptr, SQLITE_CHECKPOINT_FULL, nullptr, nullptr) != SQLITE_OK) {
        LogError("SQLite checkpoint failed: %1", sqlite3_errmsg(db));
        return false;
    }

    return true;
}

bool sq_Database::LockExclusive()
{
    std::unique_lock<std::mutex> lock(mutex);

    // Handle nested lock
    if (running_transaction && running_transaction_thread == std::this_thread::get_id()) {
        running_transaction++;
        return true;
    }

    // Wait for our turn
    if (running_transaction || running_requests) {
        LockTicket ticket;

        ticket.prev = &lock_root;
        ticket.next = lock_root.next;
        ticket.next->prev = &ticket;
        lock_root.next = &ticket;
        ticket.shared = false;

        do {
            ticket.cv.wait(lock);
        } while (running_transaction || running_requests);

        ticket.prev->next = ticket.next;
        ticket.next->prev = ticket.prev;
    }

    running_transaction++;
    running_transaction_thread = std::this_thread::get_id();

    return false;
}

void sq_Database::UnlockExclusive()
{
    std::unique_lock<std::mutex> lock(mutex);

    if (!--running_transaction) {
        LockTicket *ticket = lock_root.next;

        if (ticket != &lock_root) {
            do {
                ticket->cv.notify_one();
                ticket = ticket->next;
            } while (ticket->shared);
        }
    }
}

void sq_Database::LockShared()
{
    std::unique_lock<std::mutex> lock(mutex);

    // Wait for our turn
    if (running_transaction || lock_root.next != &lock_root) {
        LockTicket ticket;

        ticket.prev = &lock_root;
        ticket.next = lock_root.next;
        ticket.next->prev = &ticket;
        lock_root.next = &ticket;
        ticket.shared = true;

        do {
            ticket.cv.wait(lock);
        } while (running_transaction);

        ticket.prev->next = ticket.next;
        ticket.next->prev = ticket.prev;
    }

    running_requests++;
}

void sq_Database::UnlockShared()
{
    std::unique_lock<std::mutex> lock(mutex);

    if (!--running_requests) {
        LockTicket *ticket = lock_root.next;

        if (ticket != &lock_root) {
            do {
                ticket->cv.notify_one();
                ticket = ticket->next;
            } while (ticket->shared);
        }
    }
}

bool sq_Database::RunWithBindings(const char *sql, Span<const sq_Binding> bindings)
{
    sq_Statement stmt;
    if (!Prepare(sql, &stmt))
        return false;

    for (int i = 0; i < (int)bindings.len; i++) {
        const sq_Binding &binding = bindings[i];

        switch (binding.type) {
            case sq_Binding::Type::Null: { sqlite3_bind_null(stmt, i + 1); } break;
            case sq_Binding::Type::Integer: { sqlite3_bind_int64(stmt, i + 1, binding.u.i); } break;
            case sq_Binding::Type::Double: { sqlite3_bind_double(stmt, i + 1, binding.u.d); } break;
            case sq_Binding::Type::String: { sqlite3_bind_text(stmt, i + 1, binding.u.str.ptr,
                                                               (int)binding.u.str.len, SQLITE_STATIC); } break;
            case sq_Binding::Type::Blob: { sqlite3_bind_blob64(stmt, i + 1, binding.u.blob.ptr,
                                                               binding.u.blob.len, SQLITE_STATIC); } break;
            case sq_Binding::Type::Zero: { sqlite3_bind_zeroblob64(stmt, i + 1, binding.u.zero_len); } break;
        }
    }

    return stmt.Run();
}

static void LogFrameTime(const char *type, const char *filename, int64_t mtime)
{
    TimeSpec spec = {};
    DecomposeUnixTime(mtime, TimeMode::UTC, &spec);

    LogInfo("Restoring %1 '%2' (%3-%4-%5 %6:%7:%8.%9)",
            type, SplitStrReverseAny(filename, RG_PATH_SEPARATORS),
            FmtArg(spec.year).Pad0(-2), FmtArg(spec.month).Pad0(-2), FmtArg(spec.day).Pad0(-2),
            FmtArg(spec.hour).Pad0(-2), FmtArg(spec.min).Pad0(-2), FmtArg(spec.sec).Pad0(-2),
            FmtArg(spec.msec).Pad0(-3));
}

bool sq_RestoreDatabase(const char *filename, const char *dest_filename)
{
    BlockAllocator temp_alloc;

    // Safety check
    if (TestFile(dest_filename)) {
        LogError("Refusing to overwrite '%1'", dest_filename);
        return false;
    }

    Span<const uint8_t> frames;
    {
        HeapArray<uint8_t> buf(&temp_alloc);
        if (!ReadFile(filename, Megabytes(32), &buf))
            return false;

        if (!StartsWith(MakeSpan((const char *)buf.ptr, buf.len), SnapshotSignature)) {
            LogError("Unexpected file signature");
            return false;
        }

        Size signature_len = strlen(SnapshotSignature);
        frames = buf.Leak();
        frames = frames.Take(signature_len, frames.len - signature_len);
    }

    const char *wal_filename = Fmt(&temp_alloc, "%1-wal", dest_filename).ptr;
    RG_DEFER { UnlinkFile(wal_filename); };

    HeapArray<char> path_buf(&temp_alloc);
    Fmt(&path_buf, "%1", filename);

    // Copy initial database
    {
        if (frames.len < RG_SIZE(FrameData)) {
            LogError("Checksum file '%1' is truncated", filename);
            return false;
        }

        FrameData frame;
        memcpy_safe(&frame, frames.ptr, RG_SIZE(frame));
        frame.mtime = LittleEndian(frame.mtime);
        frames = frames.Take(RG_SIZE(frame), frames.len - RG_SIZE(frame));

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
    for (Size i = 1; frames.len >= RG_SIZE(FrameData); i++) {
        FrameData frame;
        memcpy_safe(&frame, frames.ptr, RG_SIZE(frame));
        frame.mtime = LittleEndian(frame.mtime);
        frames = frames.Take(RG_SIZE(frame), frames.len - RG_SIZE(frame));

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

    if (frames.len) {
        LogError("Snapshot file '%1' appears truncated", filename);
        return false;
    }

    LogInfo("Database '%1' restored", dest_filename);
    return true;
}

}
