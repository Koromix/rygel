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
