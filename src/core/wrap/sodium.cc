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

#include "src/core/base/base.hh"
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
