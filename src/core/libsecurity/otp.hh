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

#include "../libcc/libcc.hh"

namespace RG {

static inline Size sec_GetBase32EncodedLength(Size bytes)
{
    return (bytes + 4) * 8 / 5 + 8; // Account for (optional) padding and NUL byte
}

static inline Size sec_GetBase32DecodedLength(Size len)
{
    return (len + 7) / 8 * 5;
}

Size sec_EncodeBase32(Span<const uint8_t> bytes, bool pad, Span<char> out_buf);
Size sec_DecodeBase32(Span<const char> b32, Span<uint8_t> out_buf);

const char *sec_GenerateHotpUrl(const char *label, const char *username, const char *issuer,
                                const char *secret, int digits, Allocator *alloc);
bool sec_GenerateHotpPng(const char *url, HeapArray<uint8_t> *out_buf);

int sec_ComputeHotp(Span<const uint8_t> key, int64_t counter, int digits);
bool sec_CheckHotp(Span<const uint8_t> key, int64_t counter, int digits, int window, const char *code);

}
