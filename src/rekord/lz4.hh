// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "src/core/libcc/libcc.hh"
#include "vendor/lz4/lib/lz4frame.h"

namespace RG {

class DecodeLZ4 {
    LZ4F_dctx *decoder = nullptr;
    bool done = false;

    HeapArray<uint8_t> in_buf;
    Size in_hint = Kibibytes(128);

    uint8_t out_buf[256 * 1024];

public:
    DecodeLZ4();
    ~DecodeLZ4();

    Span<uint8_t> PrepareAppend(Size needed);
    bool Flush(bool eof, FunctionRef<bool(Span<const uint8_t>)> func);
};

class EncodeLZ4 {
    LZ4F_cctx *encoder = nullptr;
    bool started = false;

    HeapArray<uint8_t> dynamic_buf;

public:
    EncodeLZ4();
    ~EncodeLZ4();

    bool Start();

    bool Append(Span<const uint8_t> buf);
    bool Flush(bool eof, FunctionRef<bool(Span<const uint8_t>)> func);
};

}
