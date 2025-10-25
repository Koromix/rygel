// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "vendor/sqlite3mc/sqlite3mc.h"

// Work around -Wzero-as-null-pointer-constant warning
#undef SQLITE_STATIC
#define SQLITE_STATIC      ((sqlite3_destructor_type)nullptr)

#include <thread>

namespace K {

class sq_Database;

class sq_Binding {
public:
    enum class Type {
        Null,
        Integer,
        Double,
        String,
        Blob,
        Zero
    };

    Type type;
    union {
        int64_t i;
        double d;
        Span<const char> str;
        Span<const uint8_t> blob;
        int64_t zero_len;
    } u;

    sq_Binding() : type(Type::Null) {}
    sq_Binding(unsigned char i) : type(Type::Integer) { u.i = i; }
    sq_Binding(short i) : type(Type::Integer) { u.i = i; }
    sq_Binding(unsigned short i) : type(Type::Integer) { u.i = i; }
    sq_Binding(int i) : type(Type::Integer) { u.i = i; }
    sq_Binding(unsigned int i) : type(Type::Integer) { u.i = i; }
    sq_Binding(long i) : type(Type::Integer) { u.i = i; }
    sq_Binding(long long i) : type(Type::Integer) { u.i = i; }
    sq_Binding(double d) : type(Type::Double) { u.d = d; };
    sq_Binding(const char *str) : type(Type::String) { u.str = str; }; // nullptr results in NULL binding
    sq_Binding(Span<const char> str) : type(Type::String) { u.str = str; };
    sq_Binding(Span<const uint8_t> blob) : type(Type::Blob) { u.blob = blob; };

    static sq_Binding Zeroblob(int64_t len)
    {
        sq_Binding binding;
        binding.type = Type::Zero;
        binding.u.zero_len = len;
        return binding;
    }
};

class sq_Statement {
    K_DELETE_COPY(sq_Statement)

    sq_Database *db = nullptr;
    sqlite3_stmt *stmt = nullptr;
    bool unlock = false;

    int rc = 0;

public:
    sq_Statement() {}
    ~sq_Statement() { Finalize(); }

    sq_Statement(sq_Statement &&other) { *this = std::move(other); }
    sq_Statement &operator=(sq_Statement &&other);

    void Finalize();

    bool IsValid() const { return stmt && (rc == SQLITE_DONE || rc == SQLITE_ROW); };
    bool IsRow() const { return stmt && rc == SQLITE_ROW; }
    bool IsDone() const { return stmt && rc == SQLITE_DONE; }

    bool Run();
    bool Step();
    void Reset();

    bool GetSingleValue(int *out_value);
    bool GetSingleValue(int64_t *out_value);
    bool GetSingleValue(double *out_value);
    bool GetSingleValue(const char **out_value);

    operator sqlite3_stmt *() { return stmt; }

    friend class sq_Database;
};

class sq_Database {
    K_DELETE_COPY(sq_Database)

    struct LockWaiter {
        LockWaiter *prev;
        LockWaiter *next;
        bool shared;
        bool run;
    };

    sqlite3 *db = nullptr;

    // This wrapper uses a read-write lock that can be locked and unlocked
    // in different threads and FIFO scheduling to avoid starvation.
    // It is also reentrant, so that running requests inside an exclusive
    // lock (inside a transaction basically) works correctly.
    std::mutex wait_mutex;
    std::condition_variable wait_cv;
    LockWaiter wait_root = { &wait_root, &wait_root, false, false };
    int running_exclusive = 0;
    int running_shared = 0;
    std::thread::id running_exclusive_thread;
    std::atomic_bool lock_reads { false };

    struct sq_SnapshotPriv *snapshot = nullptr;

public:
    sq_Database() {}
    sq_Database(const char *filename, unsigned int flags) { Open(filename, flags); }
    ~sq_Database() { Close(); }

    bool IsValid() const { return db; }

    bool Open(const char *filename, unsigned int flags);
    bool Close();

    bool SetWAL(bool enable);

    // Cannot be used if core/sqlite is built without SQLITE_SNAPSHOTS, will trigger link error
    bool SetSnapshotDirectory(const char *directory, int64_t full_delay);
    bool UsesSnapshot() const { return snapshot; }

    bool GetUserVersion(int *out_version);
    bool SetUserVersion(int version);

    bool Transaction(FunctionRef<bool()> func);

    bool Prepare(const char *sql, sq_Statement *out_stmt);
    template <typename... Args>
    bool Prepare(const char *sql, sq_Statement *out_stmt, Args... args)
    {
        const sq_Binding bindings[] = { sq_Binding(args)... };
        return PrepareWithBindings(sql, bindings, out_stmt);
    }

    bool Run(const char *sql) { return RunWithBindings(sql, {}); }
    template <typename... Args>
    bool Run(const char *sql, Args... args)
    {
        const sq_Binding bindings[] = { sq_Binding(args)... };
        return RunWithBindings(sql, bindings);
    }

    bool RunMany(const char *sql);

    bool TableExists(const char *table);
    bool ColumnExists(const char *table, const char *column);

    bool BackupTo(const char *filename);
    bool Checkpoint(bool restart = false);

    operator sqlite3 *() { return db; }

private:
    bool StopSnapshot();

    bool CheckpointSnapshot(bool restart = false);
    bool CheckpointDirect();

    void RunCopyThread();
    bool CopyWAL(bool full);
    bool OpenNextFrame(int64_t now);

    bool LockExclusive();
    void UnlockExclusive();
    void LockShared();
    void UnlockShared();
    inline void Wait(std::unique_lock<std::mutex> *lock, bool shared);
    inline void WakeUpWaiters();

    bool PrepareWithBindings(const char *sql, Span<const sq_Binding> bindings, sq_Statement *out_stmt);
    bool RunWithBindings(const char *sql, Span<const sq_Binding> bindings);

    friend class sq_Statement;
};

}
