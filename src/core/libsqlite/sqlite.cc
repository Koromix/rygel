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
#include "sqlite.hh"

namespace RG {

static RG_THREAD_LOCAL bool busy_thread = false;

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

bool sq_Statement::Step()
{
    return Run() && rc == SQLITE_ROW;
}

void sq_Statement::Reset()
{
    int ret = sqlite3_reset(stmt);
    RG_ASSERT(ret == SQLITE_OK);
}

bool sq_Database::Open(const char *filename, const uint8_t key[32], unsigned int flags)
{
    static const char *const sql = R"(
        PRAGMA locking_mode = NORMAL;
        PRAGMA foreign_keys = ON;
        PRAGMA synchronous = NORMAL;
        PRAGMA busy_timeout = 15000;
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
        sqlite3_free(error);

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
    snapshot_data = false;

    // Configure database to let us manipulate the WAL manually
    if (!RunMany(R"(PRAGMA locking_mode = EXCLUSIVE;
                    PRAGMA journal_mode = WAL;
                    PRAGMA auto_vacuum = 0;)"))
        return false;

    // Open permanent WAL stream
    if (!snapshot_wal_reader.Open(wal_filename))
        return false;

    // Perform initial checkpoint
    if (!CheckpointSnapshot(true))
        return false;

    // Set up WAL hook to copy new pages
    sqlite3_wal_hook(db, [](void *udata, sqlite3 *, const char *, int) {
        sq_Database *db = (sq_Database *)udata;
        db->CopyWAL();
        return SQLITE_OK;
    }, this);

    snapshot = true;
    err_guard.Disable();

    return true;
}

bool sq_Database::Close()
{
    bool success = true;

    if (snapshot) {
        success &= Checkpoint();

        snapshot_main_writer.Close();
        snapshot_wal_reader.Close();
        snapshot_wal_writer.Close();

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
    if (!stmt.Step())
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

bool sq_Database::Checkpoint(bool restart)
{
    if (snapshot) {
        return CheckpointSnapshot(restart);
    } else {
        return CheckpointDirect();
    }
}

bool sq_Database::CheckpointDirect()
{
    bool nested = LockExclusive();
    RG_ASSERT(!nested);
    RG_DEFER { UnlockExclusive(); };

    int ret = sqlite3_wal_checkpoint_v2(db, nullptr, SQLITE_CHECKPOINT_FULL, nullptr, nullptr);
    if (ret != SQLITE_OK) {
        if (ret == SQLITE_LOCKED) {
            LogDebug("Could not checkpoint because of connection LOCK, will try again later");
            return true;
        }

        LogError("SQLite checkpoint failed: %1", sqlite3_errmsg(db));
        return false;
    }

    return true;
}

bool sq_Database::LockExclusive()
{
    std::unique_lock<std::mutex> lock(wait_mutex);

    // Wait for our turn if anything else (exclusive or shared) is running,
    // unless it is from this exact same thread.
    if (running_exclusive || running_shared) {
        if (running_exclusive_thread == std::this_thread::get_id()) {
            running_exclusive++;
            return true;
        }

        Wait(&lock, false);
    }

    RG_ASSERT(!running_exclusive);
    RG_ASSERT(!running_shared);

    running_exclusive = 1;
    running_exclusive_thread = std::this_thread::get_id();
    busy_thread = true;

    return false;
}

void sq_Database::UnlockExclusive()
{
    std::lock_guard<std::mutex> lock(wait_mutex);

    if (!--running_exclusive) {
        running_exclusive_thread = std::thread::id();
        busy_thread = false;

        WakeUpWaiters();
    }
}

void sq_Database::LockShared()
{
    std::unique_lock<std::mutex> lock(wait_mutex);

    // Wait for our turn if there's an exclusive lock or if there is one pending,
    // unless it is from this exact same thread or (in the second case) we're already
    // doing database stuff (even on another database) in this thread.
    if (running_exclusive) {
        if (running_exclusive_thread == std::this_thread::get_id()) {
            running_shared++;
            return;
        }

        Wait(&lock, true);
    } else if (wait_root.next != &wait_root) {
        if (busy_thread) {
            running_shared++;
            return;
        }

        Wait(&lock, true);
    }

    running_shared++;
    busy_thread = true;
}

void sq_Database::UnlockShared()
{
    std::lock_guard<std::mutex> lock(wait_mutex);

    if (!--running_shared && !running_exclusive) {
        busy_thread = false;
        WakeUpWaiters();
    }
}

inline void sq_Database::Wait(std::unique_lock<std::mutex> *lock, bool shared)
{
    LockWaiter waiter;

    waiter.prev = &wait_root;
    waiter.next = wait_root.next;
    waiter.next->prev = &waiter;
    wait_root.next = &waiter;
    waiter.shared = shared;

    do {
        waiter.cv.wait(*lock);
    } while (running_exclusive || (!shared && running_shared));

    waiter.prev->next = waiter.next;
    waiter.next->prev = waiter.prev;
}

inline void sq_Database::WakeUpWaiters()
{
    LockWaiter *waiter = wait_root.next;

    if (waiter != &wait_root) {
        waiter->cv.notify_one();

        if (waiter->shared) {
            waiter = waiter->next;

            while (waiter->shared) {
                waiter->cv.notify_one();
                waiter = waiter->next;
            }
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

}
