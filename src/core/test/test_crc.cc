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
#include "src/core/crc/crc.hh"
#include "test.hh"

namespace RG {

 TEST_FUNCTION("crc/CRC64xz")
{
#define TEST_CRC(Str, Expected) \
        do { \
            Span<const char> span = (Str); \
            TEST_EQ(CRC64xz(0, span.As<const uint8_t>()), (Expected)); \
        } while (false)

    TEST_CRC("", 0ull);
    TEST_CRC("123456789", 0x995DC9BBDF1939FAull);
    TEST_CRC("Lorem ipsum dolor sit amet, consectetur adipiscing elit. In suscipit lacinia odio, ut maximus lorem aliquet vel. "
             "Fusce lacus sapien, interdum nec laoreet at, pretium vel tortor. Nunc id urna eget augue maximus pharetra vitae et quam. "
             "Suspendisse potenti. Praesent vitae maximus magna. Nunc tempor metus ipsum, eu venenatis metus cursus in. "
             "Donec rutrum sem a arcu pulvinar tristique. Nulla facilisi. Sed eu fringilla augue. Mauris tempus bibendum massa, eu euismod justo convallis eget. "
             "Morbi sit amet facilisis nunc, et pharetra nunc. Nullam gravida mi vitae mauris viverra, non accumsan ante egestas. "
             "Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas.", 0x20C36CB9E094C3A8ull);

#undef TEST_CRC
}

TEST_FUNCTION("crc/CRC64nvme")
{
#define TEST_CRC(Str, Expected) \
        do { \
            Span<const char> span = (Str); \
            TEST_EQ(CRC64nvme(0, span.As<const uint8_t>()), (Expected)); \
        } while (false)

    TEST_CRC("", 0ull);
    TEST_CRC("123456789", 0xAE8B14860A799888ull);
    TEST_CRC("Lorem ipsum dolor sit amet, consectetur adipiscing elit. In suscipit lacinia odio, ut maximus lorem aliquet vel. "
             "Fusce lacus sapien, interdum nec laoreet at, pretium vel tortor. Nunc id urna eget augue maximus pharetra vitae et quam. "
             "Suspendisse potenti. Praesent vitae maximus magna. Nunc tempor metus ipsum, eu venenatis metus cursus in. "
             "Donec rutrum sem a arcu pulvinar tristique. Nulla facilisi. Sed eu fringilla augue. Mauris tempus bibendum massa, eu euismod justo convallis eget. "
             "Morbi sit amet facilisis nunc, et pharetra nunc. Nullam gravida mi vitae mauris viverra, non accumsan ante egestas. "
             "Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas.", 0xDA3CA874A87E0AC1ull);

#undef TEST_CRC
}

}
