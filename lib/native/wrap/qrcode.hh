// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

struct qr_RawCode {
    int size;
    uint8_t bits[4096];

    bool GetValue(int x, int y) const;
};

inline bool qr_RawCode::GetValue(int x, int y) const
{
    if (x < 0 || x >= size || y < 0 || y >= size)
        return false;

    int offset = y * size + x;
    int idx = offset >> 3;
    int bit = offset & 0x7;

    return bits[idx] & (1u << bit);
}

bool qr_EncodeText(Span<const char> text, qr_RawCode *out_code);
bool qr_EncodeBinary(Span<const uint8_t> data, qr_RawCode *out_code);

void qr_ExportPng(const qr_RawCode &qr, int border, StreamWriter *out_st);
void qr_ExportPng(const qr_RawCode &qr, int border, HeapArray<uint8_t> *out_png);
void qr_ExportBlocks(const qr_RawCode &qr, bool ansi, int border, StreamWriter *out_st);

}
