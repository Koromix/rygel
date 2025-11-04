// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"
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
