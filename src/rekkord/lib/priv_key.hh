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
#include "key.hh"

namespace K {

static const char *const DerivationContext = "REKKORD0";
static const int MaxKeys = 24;

enum class MasterDerivation {
    ConfigKey = 0,
    DataKey = 1,
    LogKey = 2,
    NeutralKey = 3
};

#pragma pack(push, 1)
struct KeyData {
    char prefix[5];
    struct Badge {
        uint8_t kid[16];
        int8_t type;
        uint8_t pkey[32];
        uint8_t sig[64];
    } badge;
    uint8_t keys[32 * MaxKeys];
    uint8_t sig[64];
};
#pragma pack(pop)
static_assert(K_SIZE(KeyData) == 950);
static_assert(K_SIZE(KeyData::Badge) == 113);
static_assert(K_SIZE(rk_KeySet::badge) == 113);

}
