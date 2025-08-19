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
#include "repository.hh"

namespace RG {

static const int ConfigVersion = 2;
static const int TagVersion = 1;
static const int BlobVersion = 7;
static const Size BlobSplit = Kibibytes(32);

#pragma pack(push, 1)
struct ConfigData {
    int8_t version;
    uint8_t cypher[64 + 2048];
};
#pragma pack(pop)
static_assert(RG_SIZE(ConfigData) == 2113);

#pragma pack(push, 1)
struct TagIntro {
    int8_t version;
    rk_ObjectID oid;
    uint8_t prefix[16];
    uint8_t key[24];
    int8_t count;
};
#pragma pack(pop)
static_assert(RG_SIZE(TagIntro) == 75);

#pragma pack(push, 1)
struct BlobIntro {
    int8_t version;
    int8_t type;
    uint8_t ekey[32 + 16 + 32];
    uint8_t header[24];
};
#pragma pack(pop)
static_assert(RG_SIZE(BlobIntro) == 106);

}
