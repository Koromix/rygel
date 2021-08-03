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

enum sec_HotpAlgorithm {
    SHA1, // Only choice supported by Google Authenticator
    SHA256,
    SHA512
};
static const char *const sec_HotpAlgorithmNames[] = {
    "SHA1",
    "SHA256",
    "SHA512"
};

// Use 33 bytes or more (32 + NUL byte) for security, which translates to 160 bits
void sec_GenerateSecret(Span<char> out_buf);
bool sec_CheckSecret(const char *secret);

const char *sec_GenerateHotpUrl(const char *label, const char *username, const char *issuer,
                                sec_HotpAlgorithm algo, const char *secret, int digits, Allocator *alloc);
bool sec_GenerateHotpPng(const char *url, int border, HeapArray<uint8_t> *out_buf);

int sec_ComputeHotp(const char *secret, sec_HotpAlgorithm algo, int64_t counter, int digits);
bool sec_CheckHotp(const char *secret, sec_HotpAlgorithm algo, int64_t counter, int digits, int window, const char *code);

}
