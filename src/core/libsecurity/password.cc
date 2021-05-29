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

static const HashMap<int32_t, const char *> replacements = {
    { DecodeUtf8Unsafe("Ç"), "c" },
    { DecodeUtf8Unsafe("È"), "e" },
    { DecodeUtf8Unsafe("É"), "e" },
    { DecodeUtf8Unsafe("Ê"), "e" },
    { DecodeUtf8Unsafe("Ë"), "e" },
    { DecodeUtf8Unsafe("À"), "a" },
    { DecodeUtf8Unsafe("Å"), "a" },
    { DecodeUtf8Unsafe("Â"), "a" },
    { DecodeUtf8Unsafe("Ä"), "a" },
    { DecodeUtf8Unsafe("Î"), "i" },
    { DecodeUtf8Unsafe("Ï"), "i" },
    { DecodeUtf8Unsafe("Ù"), "u" },
    { DecodeUtf8Unsafe("Ü"), "u" },
    { DecodeUtf8Unsafe("Û"), "u" },
    { DecodeUtf8Unsafe("Ú"), "u" },
    { DecodeUtf8Unsafe("Ñ"), "n" },
    { DecodeUtf8Unsafe("Ô"), "o" },
    { DecodeUtf8Unsafe("Ó"), "o" },
    { DecodeUtf8Unsafe("Ö"), "o" },
    { DecodeUtf8Unsafe("Œ"), "oe" },
    { DecodeUtf8Unsafe("Ÿ"), "y" },

    { DecodeUtf8Unsafe("ç"), "c" },
    { DecodeUtf8Unsafe("è"), "e" },
    { DecodeUtf8Unsafe("é"), "e" },
    { DecodeUtf8Unsafe("ê"), "e" },
    { DecodeUtf8Unsafe("ë"), "e" },
    { DecodeUtf8Unsafe("à"), "a" },
    { DecodeUtf8Unsafe("å"), "a" },
    { DecodeUtf8Unsafe("â"), "a" },
    { DecodeUtf8Unsafe("ä"), "a" },
    { DecodeUtf8Unsafe("î"), "i" },
    { DecodeUtf8Unsafe("ï"), "i" },
    { DecodeUtf8Unsafe("ù"), "u" },
    { DecodeUtf8Unsafe("ü"), "u" },
    { DecodeUtf8Unsafe("û"), "u" },
    { DecodeUtf8Unsafe("ú"), "u" },
    { DecodeUtf8Unsafe("ñ"), "n" },
    { DecodeUtf8Unsafe("ô"), "o" },
    { DecodeUtf8Unsafe("ó"), "o" },
    { DecodeUtf8Unsafe("ö"), "o" },
    { DecodeUtf8Unsafe("œ"), "oe" },
    { DecodeUtf8Unsafe("ÿ"), "y" },

    { DecodeUtf8Unsafe("—"), "-" },
    { DecodeUtf8Unsafe("–"), "-" }
};

// Deals with QWERTY and AZERTY for now (left-to-right and right-to-left)
static const char *spatial_sequences[26] = {
    "sz",  // a
    "nv",  // b
    "vx",  // c
    "fs",  // d
    "rz",  // e
    "gd",  // f
    "hf",  // g
    "jg",  // h
    "ou",  // i
    "kh",  // j
    "lj",  // k
    "mk",  // l
    "ln",  // m
    "mb",  // n
    "pi",  // o
    "o",   // p
    "ws",  // q
    "te",  // r
    "dqa", // s
    "yr",  // t
    "iy",  // u
    "bc",  // v
    "exq", // w
    "cwz", // x
    "ut",  // y
    "xea"  // z
};

static int32_t DecodeUtf8Unsafe(const char *str)
{
    int32_t uc = -1;
    Size bytes = DecodeUtf8(str, 0, &uc);

    RG_ASSERT(bytes > 0);
    RG_ASSERT(!str[bytes]);

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
            const char *ptr = replacements.FindValue(uc, nullptr);

            if (ptr) {
                bytes = strlen(ptr);
            } else {
                ptr = password.ptr + offset;
            }

            if (RG_UNLIKELY(len >= out_buf.len - bytes - 1)) {
                LogError("Excessive password length");
                return -1;
            }

            memcpy_safe(out_buf.ptr + len, ptr, (size_t)bytes);
            len += bytes;
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
            score += !chars.TestAndSet(c) ? 2 : 1;
            classes |= 1 << 1;

            while (++i < password.len && IsAsciiDigit(password[i])) {
                int next = (uint8_t)password[i];
                int diff = c - next;

                score += !chars.TestAndSet(next) && (diff < -1 || diff > 1) ? 2 : 1;
                c = next;
            }
        } else if (IsAsciiWhite(c)) {
            score++;

            // Consecutive white spaces characters don't count
            while (++i < password.len && IsAsciiWhite(password[i]));
        } else {
            score += !chars.TestAndSet(c) ? 4 : 1;
            classes |= 1 << 2;

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

        Span<char> remain = buf;

        do {
            Span<char> frag = SplitStrAny(remain, " _-./", &remain);
            frag.ptr[frag.len] = 0;

            if (strstr(password.ptr, frag.ptr)) {
                LogError("Password contains blacklisted content (username?)");
                return false;
            }
        } while (remain.len);
    }

    // Check complexity
    if (!CheckComplexity(password))
        return false;

    return true;
}

}
