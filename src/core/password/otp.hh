// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include "src/core/base/base.hh"

namespace RG {

enum class pwd_HotpAlgorithm {
    SHA1, // Only choice supported by Google Authenticator
    SHA256,
    SHA512
};
static const char *const pwd_HotpAlgorithmNames[] = {
    "SHA1",
    "SHA256",
    "SHA512"
};

// Use 33 bytes or more (32 + NUL byte) for security, which translates to 160 bits
void pwd_GenerateSecret(Span<char> out_buf);
bool pwd_CheckSecret(const char *secret);

const char *pwd_GenerateHotpUrl(const char *label, const char *username, const char *issuer,
                                pwd_HotpAlgorithm algo, const char *secret, int digits, Allocator *alloc);

int pwd_ComputeHotp(const char *secret, pwd_HotpAlgorithm algo, int64_t counter, int digits);
bool pwd_CheckHotp(const char *secret, pwd_HotpAlgorithm algo, int64_t min, int64_t max, int digits, const char *code);

}
