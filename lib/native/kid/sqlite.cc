// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "vendor/sqlite3mc/sqlite3mc.h"
#include "kid.hh"

namespace K {

static void KidGenFunc(sqlite3_context *ctx, int, sqlite3_value **argv)
{
    if (sqlite3_value_type(argv[0]) != SQLITE_INTEGER) [[unlikely]] {
        sqlite3_result_error(ctx, "Invalid type passed to kid_gen()", -1);
        return;
    }

    int type = sqlite3_value_int(argv[0]);

    if (type < 0 || type > 0x7F) {
        sqlite3_result_error(ctx, "Invalid type passed to kid_gen()", -1);
        return;
    }

    KID kid;
    FillKID(type, &kid);

    sqlite3_result_blob(ctx, kid.raw, K_SIZE(kid.raw), SQLITE_TRANSIENT);
}

static void KidStrFunc(sqlite3_context *ctx, int, sqlite3_value **argv)
{
    switch (sqlite3_value_type(argv[0])) {
        case SQLITE_NULL: { sqlite3_result_null(ctx); } break;

        case SQLITE_BLOB: {
            if (sqlite3_value_bytes(argv[0]) != K_SIZE(KID::raw)) [[unlikely]] {
                sqlite3_result_error(ctx, "BLOB size does not match KID size", -1);
                return;
            }

            KID kid;
            {
                const uint8_t *ptr = (const uint8_t *)sqlite3_value_blob(argv[0]);
                MemCpy(&kid.raw, ptr, K_SIZE(kid.raw));
            }

            FmtArg arg = (FmtArg)kid;
            sqlite3_result_text(ctx, arg.u.buf, -1, SQLITE_TRANSIENT);
        } break;

        default: { sqlite3_result_error(ctx, "Cannot format non-blob KID value", -1); } break;
    }
}

bool InitKidSqlite(sqlite3 *db)
{
    if (sqlite3_create_function(db, "kid_gen", 1, SQLITE_UTF8, nullptr, KidGenFunc, nullptr, nullptr) != SQLITE_OK) {
        LogError("SQLite failed to add kid_gen() function");
        return false;
    }

    if (sqlite3_create_function(db, "kid_str", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr, KidStrFunc, nullptr, nullptr) != SQLITE_OK) {
        LogError("SQLite failed to add kid_str() function");
        return false;
    }

    return true;
}

}
