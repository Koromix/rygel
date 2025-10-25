// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "repository.hh"

namespace K {

static const int ConfigVersion = 3;
static const int TagVersion = 1;
static const int BlobVersion = 7;
static const Size BlobSplit = Kibibytes(32);

#pragma pack(push, 1)
struct ConfigData {
    int8_t version;
    uint16_t len;
    uint8_t nonce[24];
    uint8_t cypher[16 + 4096];
    uint8_t sig[64];
};
#pragma pack(pop)
static_assert(K_SIZE(ConfigData) == 4203);

#pragma pack(push, 1)
struct TagIntro {
    int8_t version;
    rk_ObjectID oid;
    uint8_t prefix[16];
    uint8_t key[24];
    int8_t count;
};
#pragma pack(pop)
static_assert(K_SIZE(TagIntro) == 75);

#pragma pack(push, 1)
struct BlobIntro {
    int8_t version;
    int8_t type;
    uint8_t ekey[32 + 16 + 32];
    uint8_t header[24];
};
#pragma pack(pop)
static_assert(K_SIZE(BlobIntro) == 106);

}
