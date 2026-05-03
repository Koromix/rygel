// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "kid.hh"

namespace K {

KID::operator FmtArg() const
{
    static const char EncodedChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    FmtArg arg;

    arg.type = FmtType::Buffer;

#define ENCODE(Dest, Src) \
        do { \
            arg.u.buf[(Dest) + 0] = EncodedChars[raw[(Src) + 0] >> 2]; \
            arg.u.buf[(Dest) + 1] = EncodedChars[((raw[(Src) + 0] & 0x3) << 4) | (raw[(Src) + 1] >> 4)]; \
            arg.u.buf[(Dest) + 2] = EncodedChars[((raw[(Src) + 1] & 0xF) << 2) | (raw[(Src) + 2] >> 6)]; \
            arg.u.buf[(Dest) + 3] = EncodedChars[raw[(Src) + 2] & 0x3F]; \
        } while (false)

    ENCODE(0, 0);
    ENCODE(4, 3);
    ENCODE(8, 6);
    ENCODE(12, 9);
    ENCODE(16, 12);
    ENCODE(20, 15);
    ENCODE(24, 18);

#undef ENCODE

    arg.u.buf[28] = 0;

    return arg;
}

void FillKID(KIDType type, KID *out_kid)
{
    FillRandomSafe(out_kid->raw);
    out_kid->type = type;
}

static inline uint8_t DecodeChar(int c, bool *out_valid)
{
    switch (c) {
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case 'G':
        case 'H':
        case 'I':
        case 'J':
        case 'K':
        case 'L':
        case 'M':
        case 'N':
        case 'O':
        case 'P':
        case 'Q':
        case 'R':
        case 'S':
        case 'T':
        case 'U':
        case 'V':
        case 'W':
        case 'X':
        case 'Y':
        case 'Z': return (uint8_t)(c - 'A');

        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'h':
        case 'i':
        case 'j':
        case 'k':
        case 'l':
        case 'm':
        case 'n':
        case 'o':
        case 'p':
        case 'q':
        case 'r':
        case 's':
        case 't':
        case 'u':
        case 'v':
        case 'w':
        case 'x':
        case 'y':
        case 'z': return (uint8_t)(c - 'a' + 26);

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': return (uint8_t)(c - '0' + 52);

        case '-': return 62;
        case '_': return 63;

        default: {
            *out_valid = false;
            return 0;
        } break;
    }
}

bool ParseKID(Span<const char> str, KID *out_kid)
{
    KID kid;

    if (str.len != 28) {
        if (str.len) {
            LogError("Invalid KID length");
        } else {
            LogError("Missing KID value");
        }
        return false;
    }

    bool valid = true;

#define DECODE(Dest, Src) \
        do { \
            uint8_t a = DecodeChar(str[(Src) + 0], &valid); \
            uint8_t b = DecodeChar(str[(Src) + 1], &valid); \
            uint8_t c = DecodeChar(str[(Src) + 2], &valid); \
            uint8_t d = DecodeChar(str[(Src) + 3], &valid); \
             \
            kid.raw[(Dest) + 0] = (a << 2) | (b >> 4); \
            kid.raw[(Dest) + 1] = (b << 4) | (c >> 2); \
            kid.raw[(Dest) + 2] = (c << 6) | d; \
        } while (false)

    DECODE(0, 0);
    DECODE(3, 4);
    DECODE(6, 8);
    DECODE(9, 12);
    DECODE(12, 16);
    DECODE(15, 20);
    DECODE(18, 24);

#undef DECODE

    static_assert(std::is_unsigned_v<typeof(kid.raw[0])>);
    valid &= (kid.raw[0] < K_LEN(KIDTypeNames));

    if (!valid) {
        LogError("Failed to decode KID");
        return false;
    }

    *out_kid = kid;
    return true;
}

bool ParseKID(Span<const char> str, KIDType type, KID *out_kid)
{
    if (!ParseKID(str, out_kid))
        return false;

    if (out_kid->type != type) {
        LogError("Unexpected KID type");
        return false;
    }

    return true;
}

}
