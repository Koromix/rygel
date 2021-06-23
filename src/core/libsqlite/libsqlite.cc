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

namespace RG {

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
    RG_ASSERT(!snapshot_fp);

    // Configure database to let us manipulate the WAL manually
    if (!RunMany(R"(PRAGMA locking_mode = EXCLUSIVE;
                    PRAGMA wal_autocheckpoint = 0;
                    PRAGMA auto_vacuum = 0;)"))
        return false;

    // Open WAL file
    {
        const char *db_filename = sqlite3_db_filename(db, "main");
        const char *wal_filename = sqlite3_filename_wal(db_filename);

        snapshot_fp = OpenFile(wal_filename, (int)OpenFileFlag::Read);
        if (!snapshot_fp)
            return false;
    }

    // Reset snapshot information
    Fmt(&snapshot_filename_buf, "%1", filename);
    snapshot_full_delay = full_delay;
    snapshot_idx = 0;

    if (!Checkpoint()) {
        fclose(snapshot_fp);
        snapshot_fp = nullptr;

        return false;
    }

    return true;
}

bool sq_Database::Close()
{
    bool success = true;

    if (snapshot_fp) {
        success &= Checkpoint();

        fclose(snapshot_fp);
        snapshot_fp = nullptr;
        snapshot_filename_buf.Clear();
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

bool sq_Database::Checkpoint()
{
    BlockAllocator temp_alloc;

    bool locked = false;
    RG_DEFER {
        if (locked) {
            UnlockExclusive();
        }
    };

    const char *db_filename = sqlite3_db_filename(db, "main");
    const char *wal_filename = sqlite3_filename_wal(db_filename);
    bool success = true;

    if (snapshot_fp) {
        int64_t now = GetMonotonicTime();

        if (snapshot_idx > 0 && now - snapshot_start < snapshot_full_delay) {
            bool empty;
            {
                long position;

                success &= !fseek(snapshot_fp, 0, SEEK_END);
                position = ftell(snapshot_fp);
                success &= (position >= 0);
                success &= !fseek(snapshot_fp, 0, SEEK_SET);

                if (RG_LIKELY(success)) {
                    empty = !position;
                } else {
                    LogError("Failed to compute WAL size: %1", strerror(errno));
                    empty = true;
                }
            }

            locked = !LockExclusive();
            RG_ASSERT(locked);

            // Copy WAL file if needed
            if (!empty) {
                RG_DEFER_C(len = snapshot_filename_buf.len) { snapshot_filename_buf.ptr[snapshot_filename_buf.len = len] = 0; };
                Fmt(&snapshot_filename_buf, "-%1", FmtHex(snapshot_idx).Pad0(-16));

                StreamReader reader(snapshot_fp, wal_filename);
                StreamWriter writer(snapshot_filename_buf.ptr, (int)StreamWriterFlag::Exclusive |
                                                               (int)StreamWriterFlag::Atomic);
                success &= SpliceStream(&reader, -1, &writer);
                success &= writer.Close();

                snapshot_idx++;
            }

            // Reset and do a full snapshot after error
            snapshot_idx = success ? snapshot_idx : 0;
        } else {
            // Perform initial snapshot
            {
                RG_DEFER_C(len = snapshot_filename_buf.len) { snapshot_filename_buf.ptr[snapshot_filename_buf.len = len] = 0; };
                Fmt(&snapshot_filename_buf, "-%1", FmtHex(0).Pad0(-16));

                StreamReader reader(db_filename);
                StreamWriter writer(snapshot_filename_buf.ptr, (int)StreamWriterFlag::Exclusive |
                                                               (int)StreamWriterFlag::Atomic);
                success &= SpliceStream(&reader, -1, &writer);
                success &= writer.Close();
            }

            // Delete all WAL copies
            for (Size i = 1;; i++) {
                RG_DEFER_C(len = snapshot_filename_buf.len) { snapshot_filename_buf.ptr[snapshot_filename_buf.len = len] = 0; };
                Fmt(&snapshot_filename_buf, "-%1", FmtHex(i).Pad0(-16));

                if (!TestFile(snapshot_filename_buf.ptr))
                    break;

                UnlinkFile(snapshot_filename_buf.ptr);
            }

            snapshot_start = now;
            snapshot_idx = 1;

            locked = !LockExclusive();
            RG_ASSERT(locked);
        }
    } else {
        locked = !LockExclusive();
        RG_ASSERT(locked);
    }

    int mode = snapshot_fp ? SQLITE_CHECKPOINT_TRUNCATE : SQLITE_CHECKPOINT_FULL;
    if (sqlite3_wal_checkpoint_v2(db, nullptr, mode, nullptr, nullptr) != SQLITE_OK) {
        LogError("SQLite checkpoint failed: %1", sqlite3_errmsg(db));
        success = false;
    }

    return success;
}

bool sq_Database::LockExclusive()
{
    std::unique_lock<std::mutex> lock(mutex);

    if ((running_transaction && running_transaction_thread != std::this_thread::get_id()) ||
            running_requests) {
        do {
            transactions_cv.wait(lock);
        } while (running_transaction || running_requests);
    }
    running_transaction++;
    running_transaction_thread = std::this_thread::get_id();

    return running_transaction > 1;
}

void sq_Database::UnlockExclusive()
{
    std::unique_lock<std::mutex> lock(mutex);

    running_transaction--;
    transactions_cv.notify_one();
    requests_cv.notify_all();
}

void sq_Database::LockShared()
{
    std::unique_lock<std::mutex> lock(mutex);

    if (running_transaction && running_transaction_thread != std::this_thread::get_id()) {
        do {
            requests_cv.wait(lock);
        } while (running_transaction);
    }
    running_requests++;
}

void sq_Database::UnlockShared()
{
    std::unique_lock<std::mutex> lock(mutex);

    if (!--running_requests) {
        transactions_cv.notify_one();
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

static bool CopyFile(const char *src, const char *dest)
{
    StreamReader reader(src);
    StreamWriter writer(dest);

    if (!SpliceStream(&reader, -1, &writer))
        return false;
    if (!writer.Close())
        return false;

    return true;
}

bool sq_RestoreDatabase(const char *filename, const char *dest_filename)
{
    BlockAllocator temp_alloc;

    const char *wal_filename = Fmt(&temp_alloc, "%1-wal", dest_filename).ptr;

    HeapArray<char> path_buf(&temp_alloc);
    Fmt(&path_buf, "%1", filename);

    // Copy initial database
    {
        RG_DEFER_C(len = path_buf.len) { path_buf.ptr[path_buf.len = len] = 0; };
        Fmt(&path_buf, "-%1", FmtHex(0).Pad0(-16));

        LogInfo("Copy database from '%1'", path_buf);

        if (!CopyFile(path_buf.ptr, dest_filename))
            return false;
    }

    // Apply WAL copies
    for (Size i = 1;; i++) {
        RG_DEFER_C(len = path_buf.len) { path_buf.ptr[path_buf.len = len] = 0; };
        Fmt(&path_buf, "-%1", FmtHex(i).Pad0(-16));

        if (!TestFile(path_buf.ptr))
            break;

        LogInfo("Applying WAL '%1'", path_buf);

        if (!CopyFile(path_buf.ptr, wal_filename))
            return false;

        sq_Database db;
        if (!db.Open(dest_filename, SQLITE_OPEN_READWRITE))
            return false;
        if (!db.Run("PRAGMA user_version;"))
            return false;
        if (!db.Close())
            return false;

        if (TestFile(wal_filename)) {
            LogError("SQLite is not using the WAL for some reason");
            return false;
        }
    }

    LogInfo("Database '%1' restored", dest_filename);
    return true;
}

}
