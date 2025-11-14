// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

bool qr_EncodeTextToPng(Span<const char> text, int border, StreamWriter *out_st);
bool qr_EncodeBinaryToPng(Span<const uint8_t> data, int border, StreamWriter *out_st);

bool qr_EncodeTextToBlocks(Span<const char> text, bool ansi, int border, StreamWriter *out_st);
bool qr_EncodeBinaryToBlocks(Span<const uint8_t> data, bool ansi, int border, StreamWriter *out_st);

}
