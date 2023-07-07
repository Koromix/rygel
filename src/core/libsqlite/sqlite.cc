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
#include "sqlite.hh"

namespace RG {

sq_Statement &sq_Statement::operator=(sq_Statement &&other)
{
    Finalize();

    db = other.db;
    stmt = other.stmt;
    unlock = other.unlock;
    other.db = nullptr;
    other.stmt = nullptr;
    other.unlock = false;

    return *this;
}

void sq_Statement::Finalize()
{
    if (db) {
        sqlite3_finalize(stmt);

        if (unlock) {
            db->UnlockShared();
        }
    }

    db = nullptr;
    stmt = nullptr;
    unlock = false;
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

bool sq_Statement::GetSingleValue(int *out_value)
{
    if (!Step()) {
        LogError("Missing expected SQLite single value");
        return false;
    }

    *out_value = sqlite3_column_int(stmt, 0);
    return true;
}

bool sq_Statement::GetSingleValue(int64_t *out_value)
{
    if (!Step()) {
        LogError("Missing expected SQLite single value");
        return false;
    }

    *out_value = sqlite3_column_int64(stmt, 0);
    return true;
}

bool sq_Statement::GetSingleValue(double *out_value)
{
    if (!Step()) {
        LogError("Missing expected SQLite single value");
        return false;
    }

    *out_value = sqlite3_column_double(stmt, 0);
    return true;
}

bool sq_Database::Open(const char *filename, const uint8_t key[32], unsigned int flags)
{
    static const char *const sql = R"(
        PRAGMA locking_mode = NORMAL;
        PRAGMA foreign_keys = ON;
        PRAGMA synchronous = NORMAL;
        PRAGMA busy_timeout = 15000;
        PRAGMA cache_size = -16384;
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

bool sq_Database::Close()
{
    bool success = true;

    success &= StopSnapshot();

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
    out_stmt->Finalize();

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogError("SQLite request failed: %1", sqlite3_errmsg(db));
        return false;
    }

    if (!sqlite3_stmt_readonly(stmt) || lock_reads.load(std::memory_order_relaxed)) {
        // The destructor of sq_Statement will call UnlockShared() if needed
        LockShared();
        out_stmt->unlock = true;
    }

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
    if (running_exclusive) {
        if (running_exclusive_thread == std::this_thread::get_id()) {
            running_exclusive++;
            return true;
        }

        Wait(&lock, false);
    } else if (running_shared) {
        Wait(&lock, false);
    } else if (wait_root.next != &wait_root) {
        Wait(&lock, false);
    }

    RG_ASSERT(!running_exclusive);
    RG_ASSERT(!running_shared);

    running_exclusive = 1;
    running_exclusive_thread = std::this_thread::get_id();

    return false;
}

void sq_Database::UnlockExclusive()
{
    std::lock_guard<std::mutex> lock(wait_mutex);

    running_exclusive--;
    WakeUpWaiters();
}

void sq_Database::LockShared()
{
    std::unique_lock<std::mutex> lock(wait_mutex);

    // Wait for our turn if there's an exclusive lock or if there is one pending,
    // unless it is from this exact same thread.
    if (running_exclusive) {
        if (running_exclusive_thread == std::this_thread::get_id()) {
            running_shared++;
            return;
        }

        Wait(&lock, true);
    } else if (wait_root.next != &wait_root) {
        Wait(&lock, true);
    }

    RG_ASSERT(!running_exclusive);

    running_shared++;
}

void sq_Database::UnlockShared()
{
    std::lock_guard<std::mutex> lock(wait_mutex);

    running_shared--;
    WakeUpWaiters();
}

inline void sq_Database::Wait(std::unique_lock<std::mutex> *lock, bool shared)
{
    LockWaiter waiter = {};

    waiter.next = &wait_root;
    waiter.prev = wait_root.prev;
    wait_root.prev->next = &waiter;
    wait_root.prev = &waiter;

    waiter.shared = shared;

    do {
        wait_cv.wait(*lock);
    } while (!waiter.run);

    waiter.prev->next = waiter.next;
    waiter.next->prev = waiter.prev;
}

inline void sq_Database::WakeUpWaiters()
{
    if (running_exclusive || running_shared)
        return;

    LockWaiter *waiter = wait_root.next;

    waiter->run = true;

    if (waiter->shared) {
        RG_ASSERT(waiter != &wait_root);

        waiter = waiter->next;

        while (waiter->shared) {
            waiter->run = true;
            waiter = waiter->next;
        }
    }

    wait_cv.notify_all();
}

bool sq_Database::PrepareWithBindings(const char *sql, Span<const sq_Binding> bindings, sq_Statement *out_stmt)
{
    if (!Prepare(sql, out_stmt))
        return false;

    for (int i = 0; i < (int)bindings.len; i++) {
        const sq_Binding &binding = bindings[i];

        switch (binding.type) {
            case sq_Binding::Type::Null: { sqlite3_bind_null(*out_stmt, i + 1); } break;
            case sq_Binding::Type::Integer: { sqlite3_bind_int64(*out_stmt, i + 1, binding.u.i); } break;
            case sq_Binding::Type::Double: { sqlite3_bind_double(*out_stmt, i + 1, binding.u.d); } break;
            case sq_Binding::Type::String: { sqlite3_bind_text(*out_stmt, i + 1, binding.u.str.ptr,
                                                               (int)binding.u.str.len, SQLITE_STATIC); } break;
            case sq_Binding::Type::Blob: { sqlite3_bind_blob64(*out_stmt, i + 1, binding.u.blob.ptr,
                                                               binding.u.blob.len, SQLITE_STATIC); } break;
            case sq_Binding::Type::Zero: { sqlite3_bind_zeroblob64(*out_stmt, i + 1, binding.u.zero_len); } break;
        }
    }

    return true;
}

bool sq_Database::RunWithBindings(const char *sql, Span<const sq_Binding> bindings)
{
    sq_Statement stmt;
    if (!PrepareWithBindings(sql, bindings, &stmt))
        return false;
    return stmt.Run();
}

}
