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
// along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "src/core/libcc/libcc.hh"
#include "src/core/libwrap/json.hh"

namespace RG {

struct http_RequestInfo;
class http_IO;

struct http_ByteRange {
    Size start;
    Size end;
};

const char *http_GetMimeType(Span<const char> extension,
                             const char *default_type = "application/octet-stream");

uint32_t http_ParseAcceptableEncodings(Span<const char> encodings);
bool http_ParseRange(Span<const char> str, Size len, LocalArray<http_ByteRange, 16> *out_ranges);

void http_EncodeUrlSafe(Span<const char> str, const char *passthrough, HeapArray<char> *out_buf);
static inline void http_EncodeUrlSafe(Span<const char> str, HeapArray<char> *out_buf)
    { return http_EncodeUrlSafe(str, nullptr, out_buf); }

bool http_PreventCSRF(const http_RequestInfo &request, http_IO *io);

class http_JsonPageBuilder: public json_Writer {
    http_IO *io = nullptr;

    HeapArray<uint8_t> buf;
    StreamWriter st;

public:
    http_JsonPageBuilder(): json_Writer(&st) {}

    bool Init(http_IO *io);
    void Finish();
};

}
