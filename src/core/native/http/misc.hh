// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"
#include "src/core/native/wrap/json.hh"

namespace K {

struct http_RequestInfo;
class http_IO;

struct http_ByteRange {
    Size start;
    Size end;
};

uint32_t http_ParseAcceptableEncodings(Span<const char> encodings);
bool http_ParseRange(Span<const char> str, Size len, LocalArray<http_ByteRange, 16> *out_ranges);

bool http_PreventCSRF(http_IO *io);

bool http_ParseJson(http_IO *io, int64_t max_len, FunctionRef<bool(json_Parser *json)> func);
bool http_SendJson(http_IO *io, int status, FunctionRef<void(json_Writer *json)> func);

}
