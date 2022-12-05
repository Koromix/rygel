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

#include "src/core/libcc/libcc.hh"
#include "password.hh"
#include "password_dict.inc"

namespace RG {

// XXX: Should we try to detect date-like parts?
// XXX: Use compact and RO-only data structure made for big dictionaries
// XXX: Add proper names to dictionary, and automatically manage plurals

static const Size MinLength = 8;

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
    { DecodeUtf8Unsafe("ÿ"), "y" }
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
    Size bytes = DecodeUtf8(str, &uc);

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

static bool SearchWord(const char *word)
{
    Size start = 0;
    Size end = RG_LEN(pwd_DictWords);

    while (end > start) {
        Size i = (start + end) / 2;
        const char *needle = pwd_DictRaw + pwd_DictWords[i];
        int cmp = CmpStr(word, needle);

        if (cmp > 0) {
            start = i + 1;
        } else if (cmp < 0) {
            end = i;
        } else {
            return true;
        }
    }

    return false;
}

static bool CheckComplexity(Span<const char> password)
{
    RG_ASSERT(password.len >= MinLength);

    int score = 0;

    Bitset<256> chars;
    uint32_t classes = 0;

    RG_STATIC_ASSERT(MinLength > 2);
    if (password[0] == ' ' || password[password.len - 1] == ' ') {
        LogError("Password must not start or end with space");
        return false;
    }

    for (Size i = 0; i < password.len;) {
        int c = (uint8_t)password[i];

        if (c < 32) {
            LogError("Control characters are not allowed");
            return false;
        }

        if (IsAsciiAlpha(c)) {
            score += !chars.TestAndSet(c) ? 4 : 2;
            classes |= 1 << 0;

            int prev_score = score;
            LocalArray<char, 32> word_buf;
            char reverse_buf[RG_SIZE(word_buf.data)];

            word_buf.Append((char)c);
            reverse_buf[RG_SIZE(reverse_buf) - 2] = (char)c;
            while (++i < password.len && IsAsciiAlpha(password[i])) {
                int next = (uint8_t)password[i];
                int diff = c - next;

                score += !chars.TestAndSet(next) &&
                         (diff < -1 || diff > 1) && !strchr(spatial_sequences[c - 'a'], next) ? 2 : 1;
                c = next;

                if (RG_LIKELY(word_buf.Available() > 1)) {
                    word_buf.Append((char)c);
                    reverse_buf[RG_SIZE(reverse_buf) - word_buf.len - 1] = (char)c;
                }
            }
            word_buf.data[word_buf.len] = 0;
            reverse_buf[RG_SIZE(reverse_buf) - 1] = 0;

            const char *reverse_word = std::end(reverse_buf) - word_buf.len - 1;

            if (SearchWord(word_buf.data) || SearchWord(reverse_word)) {
                score = (int)(prev_score + word_buf.len / 2);
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
        if (chars.PopCount() < 8) {
            LogError("Password has less than 8 unique characters");
            return false;
        }

        bool simple = PopCount(classes) < (password.len < 16 ? 3 : 2) || score < 32;

        if (simple) {
            LogError("Password is not safe (use more characters, more words, or more special characters)");
            return false;
        }
    }

    return true;
}

bool pwd_CheckPassword(Span<const char> password, Span<const char *const> blacklist)
{
    // Simplify it (casing, accents)
    LocalArray<char, 129> buf;
    buf.len = SimplifyText(password, buf.data);
    if (buf.len < 0)
        return false;
    password = buf;

    // Minimal length
    if (!password.len) {
        LogError("Password is empty");
        return false;
    } else if (password.len < MinLength) {
        LogError("Password is too short");
        return false;
    }

    // Check for blacklisted words
    for (const char *needle: blacklist) {
        LocalArray<char, 129> buf2;
        buf2.len = SimplifyText(needle, buf2.data);
        if (buf2.len < 0)
            continue;

        Span<char> remain = buf2;

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

bool pwd_GeneratePassword(unsigned int flags, Span<char> out_password)
{
    static const char *const AllChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*";
    static const char *const UpperChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const char *const UpperCharsNoAmbi = "ABCDEFGHJKLMNPQRSTUVWXYZ";
    static const char *const LowerChars = "abcdefghijklmnopqrstuvwxyz";
    static const char *const LowerCharsNoAmbi = "abcdefghijkmnopqrstuvwxyz";
    static const char *const NumberChars = "0123456789";
    static const char *const NumberCharsNoAmbi = "23456789";
    static const char *const SpecialChars = "!@#$%^&*";

    if (out_password.len < 9) {
        LogError("Refusing to generate password less than 8 characters");
        return false;
    }

    int uppers = (flags & (int)pwd_GenerateFlag::Uppers) ? 1 : 0;
    int lowers = (flags & (int)pwd_GenerateFlag::Lowers) ? 1 : 0;
    int numbers = (flags & (int)pwd_GenerateFlag::Numbers) ? 1 : 0;
    int specials = (flags & (int)pwd_GenerateFlag::Specials) ? 1 : 0;
    Size all = out_password.len - uppers - lowers - numbers - specials;
    bool ambiguous = flags & (int)pwd_GenerateFlag::Ambiguous;

    for (int i = 0; i < 1000; i++) {
        Fmt(out_password, "%1%2%3%4%5", FmtRandom(uppers, ambiguous ? UpperChars : UpperCharsNoAmbi),
                                        FmtRandom(lowers, ambiguous ? LowerChars : LowerCharsNoAmbi),
                                        FmtRandom(numbers, ambiguous ? NumberChars : NumberCharsNoAmbi),
                                        FmtRandom(specials, SpecialChars),
                                        FmtRandom(all, AllChars));

        FastRandomInt rng;
        std::shuffle(out_password.begin(), out_password.end() - 1, rng);

        if ((flags & (int)pwd_GenerateFlag::Check) && !pwd_CheckPassword(out_password.ptr))
            continue;

        return true;
    }

    LogError("Failed to generate secure password");
    return false;
}

}
