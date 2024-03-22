// Copyright 2023 Niels Martignène <niels.martignene@protonmail.com>
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
#include "password.hh"
#include "password_dict.inc"

namespace RG {

// XXX: Should we try to detect date-like parts?
// XXX: Use compact and RO-only data structure made for big dictionaries
// XXX: Add proper names to dictionary, and automatically manage plurals

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
            if (len >= out_buf.len - 2) [[unlikely]] {
                LogError("Excessive password length");
                return -1;
            }

            // Some code in later steps assume lowercase, don't omit
            // this step without good reason!
            out_buf[len++] = LowerAscii(password[offset]);
        } else if (bytes > 1) {
            const char *ptr = replacements.FindValue(uc, nullptr);
            Size expand = bytes;

            if (ptr) {
                expand = strlen(ptr);
            } else {
                ptr = password.ptr + offset;
            }

            if (len >= out_buf.len - expand - 1) [[unlikely]] {
                LogError("Excessive password length");
                return -1;
            }

            memcpy_safe(out_buf.ptr + len, ptr, (size_t)expand);
            len += expand;
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

static bool CheckComplexity(Span<const char> password, unsigned int flags)
{
    RG_ASSERT(password.len >= pwd_MinLength);

    int score = 0;

    Bitset<256> chars;
    uint32_t classes = 0;

    static_assert(pwd_MinLength > 2);

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

                if (word_buf.Available() > 1) [[likely]] {
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
    if (chars.PopCount() < 8) {
        LogError("Password has less than 8 unique characters");
        return false;
    }
    if (flags & (int)pwd_CheckFlag::Classes) {
        if (password.len < 16) {
            if (PopCount(classes) < 3) {
                LogError("Passwords less than 16 characters must include symbols");
                return false;
            }
        } else {
            if (PopCount(classes) < 2) {
                LogError("Passwords must contain at least two character classes");
                return false;
            }
        }
    }
    if ((flags & (int)pwd_CheckFlag::Score) && score < 32) {
        LogError("Password is not complex enough (score %1 of %2)", score, 32);
        return false;
    }

    return true;
}

bool pwd_CheckPassword(Span<const char> password, Span<const char *const> blacklist, unsigned int flags)
{
    // Simplify it (casing, accents)
    LocalArray<char, 513> buf;
    buf.len = SimplifyText(password, buf.data);
    if (buf.len < 0)
        return false;
    password = buf;

    // Length limits
    if (!password.len) {
        LogError("Password is empty");
        return false;
    } else if (password.len < pwd_MinLength) {
        LogError("Password is too short");
        return false;
    } else if (password.len >= pwd_MaxLength) {
        LogError("Password is too long");
        return false;
    }

    // Check for blacklisted words
    for (const char *needle: blacklist) {
        LocalArray<char, 513> buf2;
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
    if (!CheckComplexity(password, flags))
        return false;

    return true;
}

bool pwd_GeneratePassword(unsigned int flags, Span<char> out_password)
{
    static const char *const UpperChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const char *const UpperCharsNoAmbi = "ABCDEFGHJKLMNPQRSTUVWXYZ";
    static const char *const LowerChars = "abcdefghijklmnopqrstuvwxyz";
    static const char *const LowerCharsNoAmbi = "abcdefghijkmnopqrstuvwxyz";
    static const char *const DigitChars = "0123456789";
    static const char *const DigitCharsNoAmbi = "23456789";
    static const char *const SpecialChars = "_-.[]";
    static const char *const DangerousChars = "!@#$%^&*";

    if (out_password.len < 9) {
        LogError("Refusing to generate password less than 8 characters");
        return false;
    }

    // Drop non-sensical combinations
    if (flags & (int)pwd_GenerateFlag::Uppers) {
        flags &= ~(int)pwd_GenerateFlag::UppersNoAmbi;
    }
    if (flags & (int)pwd_GenerateFlag::Lowers) {
        flags &= ~(int)pwd_GenerateFlag::LowersNoAmbi;
    }
    if (flags & (int)pwd_GenerateFlag::Digits) {
        flags &= ~(int)pwd_GenerateFlag::DigitsNoAmbi;
    }

    LocalArray<char, 256> all_chars;

#define TAKE_CHARS(VarName, FlagName, Chars) \
        int VarName = (flags & (int)FlagName) ? 1 : 0; \
        if (VarName) { \
            all_chars.Append(Chars); \
            all--; \
        }

    Size all = out_password.len - 1;

    TAKE_CHARS(uppers, pwd_GenerateFlag::Uppers, UpperChars);
    TAKE_CHARS(uppers_noambi, pwd_GenerateFlag::UppersNoAmbi, UpperCharsNoAmbi);
    TAKE_CHARS(lowers, pwd_GenerateFlag::Lowers, LowerChars);
    TAKE_CHARS(lowers_noambi, pwd_GenerateFlag::LowersNoAmbi, LowerCharsNoAmbi);
    TAKE_CHARS(digits, pwd_GenerateFlag::Digits, DigitChars);
    TAKE_CHARS(digits_noambi, pwd_GenerateFlag::DigitsNoAmbi, DigitCharsNoAmbi);
    TAKE_CHARS(specials, pwd_GenerateFlag::Specials, SpecialChars);
    TAKE_CHARS(dangerous, pwd_GenerateFlag::Dangerous, DangerousChars);
    all_chars.Append(0);

    if (all_chars.len < 2) {
        LogError("No character class is allowed");
        return false;
    }

    // One try should be enough but let's make sure!
    {
        PushLogFilter([](LogLevel, const char *, const char *, FunctionRef<LogFunc>) {});
        RG_DEFER_N(log_guard) { PopLogFilter(); };

        for (int i = 0; i < 1000; i++) {
            Fmt(out_password, "%1%2%3%4%5%6%7%8%9",
                    FmtRandom(uppers, UpperChars),
                    FmtRandom(uppers_noambi, UpperCharsNoAmbi),
                    FmtRandom(lowers, LowerChars),
                    FmtRandom(lowers_noambi, LowerCharsNoAmbi),
                    FmtRandom(digits, DigitChars),
                    FmtRandom(digits_noambi, DigitCharsNoAmbi),
                    FmtRandom(specials, SpecialChars),
                    FmtRandom(dangerous, DangerousChars),
                    FmtRandom(all, all_chars.data));

            FastRandomRNG<size_t> rng;
            std::shuffle(out_password.begin(), out_password.end() - 1, rng);

            if ((flags & (int)pwd_GenerateFlag::Check) && !pwd_CheckPassword(out_password.ptr))
                continue;

            return true;
        }
    }

    LogError("Failed to generate secure password");
    return false;
}

}
