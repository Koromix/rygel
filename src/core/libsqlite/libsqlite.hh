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

#pragma once

#include "../libcc/libcc.hh"
#include "../../../vendor/sqlite3mc/sqlite3mc.h"
#include "../../../vendor/libsodium/src/libsodium/include/sodium/crypto_hash_sha256.h"

#include <thread>

namespace RG {

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
        Size zero_len;
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
    sq_Binding(const char *str) : type(Type::String) { u.str = str; };
    sq_Binding(Span<const char> str) : type(Type::String) { u.str = str; };
    sq_Binding(Span<const uint8_t> blob) : type(Type::Blob) { u.blob = blob; };

    static sq_Binding Zeroblob(Size len)
    {
        sq_Binding binding;
        binding.type = Type::Zero;
        binding.u.zero_len = len;
        return binding;
    }
};

class sq_Statement {
    RG_DELETE_COPY(sq_Statement)

    sq_Database *db = nullptr;
    sqlite3_stmt *stmt = nullptr;
    int rc;

public:
    sq_Statement() {}
    ~sq_Statement() { Finalize(); }

    sq_Statement(sq_Statement &&other) { *this = std::move(other); }
    sq_Statement &operator=(sq_Statement &&other);

    void Finalize();

    bool IsValid() const { return stmt && (rc == SQLITE_DONE || rc == SQLITE_ROW); };
    bool IsRow() const { return stmt && rc == SQLITE_ROW; }

    bool Run();
    bool Next();
    void Reset();

    sqlite3_stmt *Leak();

    operator sqlite3_stmt *() { return stmt; }

    friend class sq_Database;
};

class sq_Database {
    RG_DELETE_COPY(sq_Database)

    struct LockTicket {
        LockTicket *prev;
        LockTicket *next;
        std::condition_variable cv;
        bool shared;
    };

    sqlite3 *db = nullptr;

    std::mutex mutex;
    LockTicket lock_root = { &lock_root, &lock_root };
    std::condition_variable transactions_cv;
    std::condition_variable requests_cv;
    int running_transaction = 0;
    std::thread::id running_transaction_thread;
    int running_requests = 0;

    bool snapshot = false;
    HeapArray<char> snapshot_path_buf;
    StreamWriter snapshot_main_writer;
    StreamReader snapshot_wal_reader;
    StreamWriter snapshot_wal_writer;
    crypto_hash_sha256_state snapshot_wal_state;
    int64_t snapshot_full_delay;
    int64_t snapshot_start;
    Size snapshot_idx;
    std::atomic_bool snapshot_data { false };

public:
    sq_Database() {}
    sq_Database(const char *filename, unsigned int flags) { Open(filename, flags); }
    ~sq_Database() { Close(); }

    bool IsValid() const { return db; }

    bool Open(const char *filename, const uint8_t key[32], unsigned int flags);
    bool Open(const char *filename, unsigned int flags) { return Open(filename, nullptr, flags); }
    bool Close();

    bool SetWAL(bool enable);
    bool SetSynchronousFull(bool enable);
    bool SetSnapshotFile(const char *filename, int64_t full_delay);

    bool GetUserVersion(int *out_version);
    bool SetUserVersion(int version);

    bool Transaction(FunctionRef<bool()> func);

    bool Prepare(const char *sql, sq_Statement *out_stmt);
    bool Run(const char *sql) { return RunWithBindings(sql, {}); }
    template <typename... Args>
    bool Run(const char *sql, Args... args)
    {
        const sq_Binding bindings[] = { sq_Binding(args)... };
        return RunWithBindings(sql, bindings);
    }

    bool RunMany(const char *sql);

    bool BackupTo(const char *filename);
    bool Checkpoint(bool restart = false);

    operator sqlite3 *() { return db; }

private:
    bool CheckpointSnapshot(bool restart = false);
    bool CheckpointDirect();

    bool LockExclusive();
    void UnlockExclusive();
    void LockShared();
    void UnlockShared();

    bool RunWithBindings(const char *sql, Span<const sq_Binding> bindings);

    friend class sq_Statement;
};

bool sq_RestoreDatabase(const char *filename, const char *dest_filename);

}
