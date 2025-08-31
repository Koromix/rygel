// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "src/core/base/base.hh"

namespace K {

static const Size rk_MaximumKeySize = 4096;
static const Size rk_MasterKeySize = 32;

enum class rk_KeyType {
    Master = 0,
    WriteOnly = 1,
    ReadWrite = 2,
    LogOnly = 3
};
static const char *const rk_KeyTypeNames[] = {
    "Master",
    "WriteOnly",
    "ReadWrite",
    "LogOnly"
};

enum class rk_AccessMode {
    Config = 1 << 0,
    Read = 1 << 1,
    Write = 1 << 2,
    Log = 1 << 3
};
static const char *const rk_AccessModeNames[] = {
    "Config",
    "Read",
    "Write",
    "Log"
};

struct rk_KeySet {
    uint8_t kid[16];

    rk_KeyType type;
    unsigned int modes;

    struct Keys {
        uint8_t ckey[32];
        uint8_t akey[32];
        uint8_t dkey[32];
        uint8_t wkey[32];
        uint8_t lkey[32];
        uint8_t tkey[32];
        uint8_t nkey[32];
        uint8_t vkey[32];
        uint8_t skey[32];
        uint8_t pkey[32];
    } keys;

    uint8_t badge[113];

    bool HasMode(rk_AccessMode mode) const { return modes & (int)mode; }
};

Size rk_ReadRawKey(const char *filename, Span<uint8_t> out_raw);
bool rk_SaveRawKey(Span<const uint8_t> raw, const char *filename);

bool rk_DeriveMasterKey(Span<const uint8_t> mkey, rk_KeySet *out_keys);
bool rk_LoadKeyFile(const char *filename, rk_KeySet *out_keys);
bool rk_ExportKeyFile(const rk_KeySet &keys, rk_KeyType type, const char *filename, rk_KeySet *out_keys = nullptr);

}
