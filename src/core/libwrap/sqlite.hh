// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "../../../vendor/sqlite3mc/sqlite3mc.h"

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

    sqlite3 *db = nullptr;

    std::mutex mutex;
    std::condition_variable transactions_cv;
    std::condition_variable requests_cv;
    int running_transaction = 0;
    std::thread::id running_transaction_thread;
    int running_requests = 0;

public:
    sq_Database() {}
    sq_Database(const char *filename, unsigned int flags) { Open(filename, flags); }
    ~sq_Database() { Close(); }

    bool IsValid() const { return db; }

    bool Open(const char *filename, unsigned int flags);
    bool Close();

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

    operator sqlite3 *() { return db; }

private:
    bool LockExclusive();
    void UnlockExclusive();
    void LockShared();
    void UnlockShared();

    bool RunWithBindings(const char *sql, Span<const sq_Binding> bindings);

    friend class sq_Statement;
};

}
