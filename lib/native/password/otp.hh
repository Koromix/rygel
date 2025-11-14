// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

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
