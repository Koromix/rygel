// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

union KID {
    int8_t type;
    uint8_t raw[21];

    operator FmtArg() const;
};

void FillKID(int8_t type, KID *out_kid);
bool ParseKID(Span<const char> str, KID *out_kid);
bool ParseKID(Span<const char> str, int8_t type, KID *out_kid);

}
