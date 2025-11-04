// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"
#include "vendor/lz4/lib/lz4.h"
#include "vendor/lz4/lib/lz4hc.h"
#include "vendor/lz4/lib/lz4frame.h"

namespace K {

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
    bool Flush(bool complete, FunctionRef<bool(Span<const uint8_t>)> func);
};

class EncodeLZ4 {
    LZ4F_cctx *encoder = nullptr;
    bool started = false;

    HeapArray<uint8_t> dynamic_buf;

public:
    EncodeLZ4();
    ~EncodeLZ4();

    bool Start(int level);

    bool Append(Span<const uint8_t> buf);
    bool Flush(bool complete, FunctionRef<Size(Span<const uint8_t>)> func);
};

}
