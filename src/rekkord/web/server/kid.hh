// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

enum class KIDType: int8_t {
    Drop = 0
};
static const char *const KIDTypeNames[] = {
    "Drop"
};

union KID {
    KIDType type;
    uint8_t raw[21];

    operator FmtArg() const;
};

void FillKID(KIDType type, KID *out_kid);

bool ParseKID(Span<const char> str, KID *out_kid);
bool ParseKID(Span<const char> str, KIDType type, KID *out_kid);

}
