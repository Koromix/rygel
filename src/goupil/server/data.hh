// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../../vendor/sqlite/sqlite3.h"
#include "../../libcc/libcc.hh"

namespace RG {

class SQLiteDatabase {
    sqlite3 *db = nullptr;

public:
    SQLiteDatabase() {}
    SQLiteDatabase(const char *filename, unsigned int flags) { Open(filename, flags); }
    ~SQLiteDatabase() { Close(); }

    bool IsValid() const { return db; }

    bool Open(const char *filename, unsigned int flags);
    bool Close();

    bool CreateSchema();

    operator sqlite3 *() { return db; }
};

}
