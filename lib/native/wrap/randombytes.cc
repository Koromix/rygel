// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

// Include this when building mlkem-native or mldsa-native, to provide a suitable random generator.

#include "lib/native/base/base.hh"

namespace K {

extern "C" int randombytes(uint8_t *out, size_t outlen)
{
    FillRandomSafe(out, (Size)outlen);
    return 0;
}

}
