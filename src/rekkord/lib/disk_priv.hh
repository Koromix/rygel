// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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
#include "disk.hh"

namespace RG {

static const char *const DerivationContext = "REKKORD0";
static const int MaxKeys = 24;
static const int ConfigVersion = 2;
static const int TagVersion = 2;
static const int BlobVersion = 7;
static const Size BlobSplit = Kibibytes(32);

enum class MasterDerivation {
    ConfigKey = 0,
    DataKey = 1,
    LogKey = 2,
    SignKey = 3
};

#pragma pack(push, 1)
struct KeyData {
    uint8_t salt[16];
    uint8_t nonce[24];
    int8_t role;
    uint8_t cypher[16 + MaxKeys * 32];
    uint8_t sig[64];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ConfigData {
    int8_t version;
    uint8_t cypher[64 + 2048];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct TagIntro {
    int8_t version;
    rk_Hash hash;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct BlobIntro {
    int8_t version;
    int8_t type;
    uint8_t ekey[32 + 16 + 32];
    uint8_t header[24];
};
#pragma pack(pop)

static const int CacheVersion = 7;

}
