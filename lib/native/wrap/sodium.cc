// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "vendor/libsodium/src/libsodium/include/sodium.h"

namespace K {

static const char *ImplementationName();
static uint32_t GetRandom32();
static void FillBuffer(void *const buf, const size_t size);

static randombytes_implementation BaseRandom = {
    .implementation_name = ImplementationName,
    .random = GetRandom32,
    .stir = nullptr,
    .uniform = nullptr,
    .buf = FillBuffer,
    .close = nullptr
};

K_INIT(libsodium)
{
    K_CRITICAL(!sodium_init(), "Failed to initialize libsodium");
    randombytes_set_implementation(&BaseRandom);
}

static const char *ImplementationName()
{
    return "rygel";
}

static uint32_t GetRandom32()
{
    uint32_t rnd = (uint32_t)GetRandom();
    return rnd;
}

static void FillBuffer(void *const buf, const size_t size)
{
    FillRandomSafe(buf, size);
}

}
