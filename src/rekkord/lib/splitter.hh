// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"

namespace K {

class FastSplitter {
    Size avg;
    Size min;
    Size max;
    uint8_t salt[8];

    uint32_t mask1;
    uint32_t mask2;

    Size idx = 0;
    int64_t total = 0;

public:
    FastSplitter(Size avg, Size min, Size max, uint64_t salt8 = 0);

    Size Process(Span<const uint8_t> buf, bool last,
                 FunctionRef<bool(Size idx, int64_t total, Span<const uint8_t> chunk)> func);

private:
    Size Cut(Span<const uint8_t> buf, bool last);
};

}
