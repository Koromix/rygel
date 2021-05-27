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

#include "../libcc/libcc.hh"
#include "password.hh"

namespace RG {

// XXX: Detect words using dictionary
// XXX: Detect date-like parts

static int32_t DecodeUtf8Unsafe(const char *str);

static const HashMap<int32_t, char> replacements = {
    { DecodeUtf8Unsafe("Ç"), 'c' },
    { DecodeUtf8Unsafe("Ê"), 'e' },
    { DecodeUtf8Unsafe("É"), 'e' },
    { DecodeUtf8Unsafe("È"), 'e' },
    { DecodeUtf8Unsafe("Ë"), 'e' },
    { DecodeUtf8Unsafe("A"), 'a' },
    { DecodeUtf8Unsafe("À"), 'a' },
    { DecodeUtf8Unsafe("Â"), 'a' },
    { DecodeUtf8Unsafe("I"), 'i' },
    { DecodeUtf8Unsafe("Ï"), 'i' },
    { DecodeUtf8Unsafe("U"), 'u' },
    { DecodeUtf8Unsafe("Ù"), 'u' },
    { DecodeUtf8Unsafe("Ü"), 'u' },
    { DecodeUtf8Unsafe("Ô"), 'o' },
    { DecodeUtf8Unsafe("O"), 'o' },
    { DecodeUtf8Unsafe("Ÿ"), 'y' },
    { DecodeUtf8Unsafe("ç"), 'c' },
    { DecodeUtf8Unsafe("ê"), 'e' },
    { DecodeUtf8Unsafe("é"), 'e' },
    { DecodeUtf8Unsafe("è"), 'e' },
    { DecodeUtf8Unsafe("ë"), 'e' },
    { DecodeUtf8Unsafe("a"), 'a' },
    { DecodeUtf8Unsafe("à"), 'a' },
    { DecodeUtf8Unsafe("â"), 'a' },
    { DecodeUtf8Unsafe("i"), 'i' },
    { DecodeUtf8Unsafe("ï"), 'i' },
    { DecodeUtf8Unsafe("u"), 'u' },
    { DecodeUtf8Unsafe("ù"), 'u' },
    { DecodeUtf8Unsafe("ü"), 'u' },
    { DecodeUtf8Unsafe("ô"), 'o' },
    { DecodeUtf8Unsafe("o"), 'o' },
    { DecodeUtf8Unsafe("ÿ"), 'y' },
    { DecodeUtf8Unsafe("—"), '-' },
    { DecodeUtf8Unsafe("–"), '-' }
};

// Deals with QWERTY and AZERTY for now
static const char *spatial_sequences[26] = {
    "sz", // a
    "n",  // b
    "v",  // c
    "f",  // d
    "r",  // e
    "g",  // f
    "h",  // g
    "j",  // h
    "o",  // i
    "k",  // j
    "l",  // k
    "m",  // l
    "",   // m
    "m",  // n
    "p",  // o
    "",   // p
    "ws", // q
    "t",  // r
    "d",  // s
    "y",  // t
    "i",  // u
    "b",  // v
    "ex", // w
    "c",  // x
    "u",  // y
    "xe"  // z
};

static int32_t DecodeUtf8Unsafe(const char *str)
{
    int32_t uc = -1;
    Size bytes = DecodeUtf8(str, 0, &uc);

    RG_ASSERT(bytes > 0);

    return uc;
}

static Size SimplifyText(Span<const char> password, Span<char> out_buf)
{
    RG_ASSERT(out_buf.len > 0);

    password = TrimStr(password);

    Size offset = 0;
    Size len = 0;

    while (offset < password.len) {
        int32_t uc;
        Size bytes = DecodeUtf8(password, offset, &uc);

        if (bytes == 1) {
            if (RG_UNLIKELY(len >= out_buf.len - 2)) {
                LogError("Excessive password length");
                return -1;
            }

            // Some code in later steps assume lowercase, don't omit
            // this step without good reason!
            out_buf[len++] = LowerAscii(password[offset]);
        } else if (bytes > 1) {
            // Return value is not a string but a pointer to a single char!
            const char *ptr = replacements.Find(uc);

            if (ptr) {
                if (RG_UNLIKELY(len >= out_buf.len - 2)) {
                    LogError("Excessive password length");
                    return -1;
                }

                out_buf[len++] = *ptr;
            } else {
                if (RG_UNLIKELY(len >= out_buf.len - bytes - 1)) {
                    LogError("Excessive password length");
                    return -1;
                }

                memcpy_safe(out_buf.ptr + len, password.ptr + offset, (size_t)bytes);
                len += bytes;
            }
        } else {
            LogError("Illegal UTF-8 sequence");
            return -1;
        }

        offset += bytes;
    }

    out_buf[len] = 0;
    return len;
}

static bool CheckComplexity(Span<const char> password)
{
    int score = 0;

    Bitset<256> chars;
    uint32_t classes = 0;

    for (Size i = 0; i < password.len;) {
        int c = (uint8_t)password[i];

        if (c < 32) {
            LogError("Control characters are not allowed");
            return false;
        }

        if (IsAsciiAlpha(c)) {
            score += !chars.TestAndSet(c) ? 4 : 2;
            classes |= 1 << 0;

            while (++i < password.len && IsAsciiAlpha(password[i])) {
                int next = (uint8_t)password[i];
                int diff = c - next;

                score += !chars.TestAndSet(next) &&
                         (diff < -1 || diff > 1) && !strchr(spatial_sequences[c - 'a'], next) ? 2 : 1;
                c = next;
            }
        } else if (IsAsciiDigit(c)) {
            score += !chars.TestAndSet(c) ? 4 : 2;
            classes |= 1 << 1;

            while (++i < password.len && IsAsciiDigit(password[i])) {
                int next = (uint8_t)password[i];
                int diff = c - next;

                score += !chars.TestAndSet(next) && (diff < -1 || diff > 1) ? 2 : 1;
                c = next;
            }
        } else if (IsAsciiWhite(c)) {
            score++;
            classes |= 1 << 2;

            // Consecutive white spaces characters don't count
            while (++i < password.len && IsAsciiWhite(password[i]));
        } else {
            score += !chars.TestAndSet(c) ? 4 : 1;
            classes |= 1 << 3;

            while (++i < password.len && !IsAsciiAlpha(password[i]) &&
                                         !IsAsciiDigit(password[i]) &&
                                         !IsAsciiWhite(password[i])) {
                c = (uint8_t)password[i];
                score += !chars.TestAndSet(c) ? 2 : 1;
            }
        }
    }

    // Help user!
    {
        LocalArray<const char *, 8> problems;

        if (PopCount(classes) < 3) {
            problems.Append("less than 3 character classes");
        }
        if (chars.PopCount() < 8) {
            problems.Append("less than 8 unique characters");
        }
        if (score < 32) {
            problems.Append("not complex enough");
        }

        if (problems.len) {
            LogError("Password is insufficient: %1", FmtSpan((Span<const char *>)problems));
            return false;
        }
    }

    return true;
}

bool sec_CheckPassword(Span<const char> password, Span<const char *const> blacklist)
{
    // Simplify it (casing, accents)
    LocalArray<char, 129> buf;
    buf.len = SimplifyText(password, buf.data);
    if (buf.len < 0)
        return false;
    password = buf;

    // Minimal length
    if (password.len < 8) {
        LogError("Password is too short");
        return false;
    }

    // Check for blacklisted words
    for (const char *needle: blacklist) {
        LocalArray<char, 129> buf;
        buf.len = SimplifyText(needle, buf.data);
        if (buf.len < 0)
            continue;

        if (strstr(password.ptr, buf.data)) {
            LogError("Password contains blacklisted content (username?)");
            return false;
        }
    }

    // Check complexity
    if (!CheckComplexity(password))
        return false;

    return true;
}

}
