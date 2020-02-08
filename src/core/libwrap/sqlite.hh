// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "../../../vendor/sqlite/sqlite3.h"

#include <shared_mutex>
#include <thread>

namespace RG {

class SQLiteStatement;

class SQLiteDatabase {
    sqlite3 *db = nullptr;

    std::shared_mutex transact_mutex;
    std::thread::id transact_thread;

public:
    SQLiteDatabase() {}
    SQLiteDatabase(const char *filename, unsigned int flags) { Open(filename, flags); }
    ~SQLiteDatabase() { Close(); }

    bool IsValid() const { return db; }

    bool Open(const char *filename, unsigned int flags);
    bool Close();

    bool Transaction(FunctionRef<bool()> func);

    bool Execute(const char *sql);
    bool Prepare(const char *sql, SQLiteStatement *out_stmt);

    operator sqlite3 *() { return db; }
};

class SQLiteStatement {
    sqlite3_stmt *stmt = nullptr;
    int rc;

public:
    ~SQLiteStatement() { Finalize(); }

    void Finalize();

    bool IsValid() const { return stmt && (rc == SQLITE_DONE || rc == SQLITE_ROW); };
    bool IsDone() const { return stmt && rc == SQLITE_DONE; }

    bool Execute();
    bool Next();

    operator sqlite3_stmt *() { return stmt; }

    friend class SQLiteDatabase;
};

}
