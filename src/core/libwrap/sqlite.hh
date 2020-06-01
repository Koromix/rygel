// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "../../../vendor/sqlite/sqlite3.h"

#include <shared_mutex>
#include <thread>

namespace RG {

class sq_Binding {
public:
    enum class Type {
        Integer,
        Double,
        String
    };

    Type type;
    union {
        int64_t i;
        double d;
        Span<const char> str;
    } u;

    sq_Binding(unsigned char i)  : type(Type::Integer) { u.i = i; }
    sq_Binding(short i) : type(Type::Integer) { u.i = i; }
    sq_Binding(unsigned short i) : type(Type::Integer) { u.i = i; }
    sq_Binding(int i) : type(Type::Integer) { u.i = i; }
    sq_Binding(unsigned int i) : type(Type::Integer) { u.i = i; }
    sq_Binding(double d) : type(Type::Double) { u.d = d; };
    sq_Binding(const char *str) : type(Type::String) { u.str = str; };
    sq_Binding(Span<const char> str) : type(Type::String) { u.str = str; };
};

class sq_Statement {
    RG_DELETE_COPY(sq_Statement)

    sqlite3_stmt *stmt = nullptr;
    int rc;

public:
    sq_Statement() {}
    ~sq_Statement() { Finalize(); }

    void Finalize();

    bool IsValid() const { return stmt && (rc == SQLITE_DONE || rc == SQLITE_ROW); };
    bool IsRow() const { return stmt && rc == SQLITE_ROW; }

    bool Run();
    bool Next();
    void Reset();

    operator sqlite3_stmt *() { return stmt; }

    friend class sq_Database;
};

class sq_Database {
    RG_DELETE_COPY(sq_Database)

    sqlite3 *db = nullptr;

    std::shared_mutex transact_mutex;
    std::atomic<std::thread::id> transact_thread;

public:
    sq_Database() {}
    sq_Database(const char *filename, unsigned int flags) { Open(filename, flags); }
    ~sq_Database() { Close(); }

    bool IsValid() const { return db; }

    bool Open(const char *filename, unsigned int flags);
    bool Close();

    bool Transaction(FunctionRef<bool()> func);

    bool Run(const char *sql);
    template <typename... Args>
    bool Run(const char *sql, Args... args)
    {
        const sq_Binding bindings[] = { sq_Binding(args)... };
        return RunWithBindings(sql, bindings);
    }

    bool Prepare(const char *sql, sq_Statement *out_stmt);

    operator sqlite3 *() { return db; }

private:
    bool RunWithBindings(const char *sql, Span<const sq_Binding> bindings);
};

}
