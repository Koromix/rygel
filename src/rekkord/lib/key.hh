// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"

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
