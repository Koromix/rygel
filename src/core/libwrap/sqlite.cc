// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "sqlite.hh"

namespace RG {

void sq_Statement::Finalize()
{
    sqlite3_finalize(stmt);
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

bool sq_Database::Open(const char *filename, unsigned int flags)
{
    static const char *const sql = R"(
        PRAGMA foreign_keys = ON;
        PRAGMA journal_mode = WAL;
        PRAGMA busy_timeout = 5000;
    )";

    RG_ASSERT(!db);
    RG_DEFER_N(out_guard) { Close(); };

    if (sqlite3_open_v2(filename, &db, flags, nullptr) != SQLITE_OK) {
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

bool sq_Database::Close()
{
    if (sqlite3_close(db) != SQLITE_OK)
        return false;
    db = nullptr;

    return true;
}

bool sq_Database::Transaction(FunctionRef<bool()> func)
{
    std::lock_guard<std::shared_mutex> lock(transact_mutex);

    transact_thread = std::this_thread::get_id();
    RG_DEFER { transact_thread = std::thread::id(); };

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

bool sq_Database::Run(const char *sql)
{
    std::shared_lock<std::shared_mutex> lock(transact_mutex, std::defer_lock);

    if (std::this_thread::get_id() != transact_thread) {
        lock.lock();
    }

    char *error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        LogError("SQLite request failed: %1", error);
        sqlite3_free(error);

        return false;
    }

    return true;
}

bool sq_Database::Prepare(const char *sql, sq_Statement *out_stmt)
{
    std::shared_lock<std::shared_mutex> lock(transact_mutex, std::defer_lock);

    if (std::this_thread::get_id() != transact_thread) {
        lock.lock();
    }

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogError("SQLite request failed: %1", sqlite3_errmsg(db));
        return false;
    }

    out_stmt->Finalize();
    out_stmt->stmt = stmt;

    return true;
}

bool sq_Database::RunWithBindings(const char *sql, Span<const sq_Binding> bindings)
{
    sq_Statement stmt;
    if (!Prepare(sql, &stmt))
        return false;

    for (int i = 0; i < (int)bindings.len; i++) {
        const sq_Binding &binding = bindings[i];

        switch (binding.type) {
            case sq_Binding::Type::Integer: { sqlite3_bind_int64(stmt, i + 1, binding.u.i); } break;
            case sq_Binding::Type::Double: { sqlite3_bind_double(stmt, i + 1, binding.u.d); } break;
            case sq_Binding::Type::String: { sqlite3_bind_text(stmt, i + 1, binding.u.str.ptr,
                                                               (int)binding.u.str.len, SQLITE_STATIC); } break;
        }
    }

    return stmt.Run();
}

}
