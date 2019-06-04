// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <io.h>
    #include <direct.h>
    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
        #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
    #endif

    extern "C" __declspec(dllimport) int __stdcall PathMatchSpecA(const char *pszFile, const char *pszSpec);
#else
    #include <dirent.h>
    #include <fcntl.h>
    #include <fnmatch.h>
    #include <signal.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <time.h>
    #include <unistd.h>
#endif
#ifdef __APPLE__
    #include <mach-o/dyld.h>

    #define off64_t off_t
    #define fseeko64 fseeko
    #define ftello64 ftello
#endif
#include <chrono>
#include <condition_variable>
#include <thread>

#ifndef LIBCC_NO_MINIZ
    #define MINIZ_NO_STDIO
    #define MINIZ_NO_TIME
    #define MINIZ_NO_ARCHIVE_APIS
    #define MINIZ_NO_ARCHIVE_WRITING_APIS
    #define MINIZ_NO_ZLIB_APIS
    #define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
    #define MINIZ_NO_MALLOC
    #include "../../vendor/miniz/miniz.h"
#endif

#include "libcc.hh"

namespace RG {

// ------------------------------------------------------------------------
// Assert
// ------------------------------------------------------------------------

extern "C" void RG_NORETURN AssertFail(const char *cond)
{
    fprintf(stderr, "Assertion '%s' failed\n", cond);
    abort();
}

// ------------------------------------------------------------------------
// Memory / Allocator
// ------------------------------------------------------------------------

// This Allocator design should allow efficient and mostly-transparent use of memory
// arenas and simple pointer-bumping allocator. This will be implemented later, for
// now it's just a doubly linked list of malloc() memory blocks.

class MallocAllocator: public Allocator {
protected:
    void *Allocate(Size size, unsigned int flags = 0) override
    {
        void *ptr = malloc((size_t)size);
        if (RG_UNLIKELY(!ptr)) {
            LogError("Failed to allocate %1 of memory", FmtMemSize(size));
            abort();
        }
        if (flags & (int)Allocator::Flag::Zero) {
            memset(ptr, 0, (size_t)size);
        }
        return ptr;
    }

    void Resize(void **ptr, Size old_size, Size new_size, unsigned int flags = 0) override
    {
        if (!new_size) {
            Release(*ptr, old_size);
            *ptr = nullptr;
        } else {
            void *new_ptr = realloc(*ptr, (size_t)new_size);
            if (RG_UNLIKELY(new_size && !new_ptr)) {
                LogError("Failed to resize %1 memory block to %2",
                         FmtMemSize(old_size), FmtMemSize(new_size));
                abort();
            }
            if ((flags & (int)Allocator::Flag::Zero) && new_size > old_size) {
                memset((uint8_t *)new_ptr + old_size, 0, (size_t)(new_size - old_size));
            }
            *ptr = new_ptr;
        }
    }

    void Release(void *ptr, Size) override
    {
        free(ptr);
    }
};

static Allocator *GetDefaultAllocator()
{
    static Allocator *default_allocator = new RG_DEFAULT_ALLOCATOR;
    return default_allocator;
}

void *Allocator::Allocate(Allocator *alloc, Size size, unsigned int flags)
{
    RG_ASSERT_DEBUG(size >= 0);

    if (!alloc) {
        alloc = GetDefaultAllocator();
    }
    return alloc->Allocate(size, flags);
}

void Allocator::Resize(Allocator *alloc, void **ptr, Size old_size, Size new_size,
                       unsigned int flags)
{
    RG_ASSERT_DEBUG(new_size >= 0);

    if (!alloc) {
        alloc = GetDefaultAllocator();
    }
    alloc->Resize(ptr, old_size, new_size, flags);
}

void Allocator::Release(Allocator *alloc, void *ptr, Size size)
{
    if (!alloc) {
        alloc = GetDefaultAllocator();
    }
    alloc->Release(ptr, size);
}

void LinkedAllocator::ReleaseAll()
{
    Node *head = list.next;
    while (head) {
        Node *next = head->next;
        Allocator::Release(allocator, head, -1);
        head = next;
    }
    list = {};
}

void *LinkedAllocator::Allocate(Size size, unsigned int flags)
{
    Bucket *bucket = (Bucket *)Allocator::Allocate(allocator, RG_SIZE(*bucket) + size, flags);

    if (list.prev) {
        list.prev->next = &bucket->head;
        bucket->head.prev = list.prev;
        bucket->head.next = nullptr;
        list.prev = &bucket->head;
    } else {
        list.prev = &bucket->head;
        list.next = &bucket->head;
        bucket->head.prev = nullptr;
        bucket->head.next = nullptr;
    }

    return bucket->data;
}

void LinkedAllocator::Resize(void **ptr, Size old_size, Size new_size, unsigned int flags)
{
    if (!*ptr) {
        *ptr = Allocate(new_size, flags);
    } else if (!new_size) {
        Release(*ptr, old_size);
        *ptr = nullptr;
    } else {
        Bucket *bucket = PointerToBucket(*ptr);
        Allocator::Resize(allocator, (void **)&bucket, RG_SIZE(*bucket) + old_size,
                          RG_SIZE(*bucket) + new_size, flags);

        if (bucket->head.next) {
            bucket->head.next->prev = &bucket->head;
        } else {
            list.prev = &bucket->head;
        }
        if (bucket->head.prev) {
            bucket->head.prev->next = &bucket->head;
        } else {
            list.next = &bucket->head;
        }

        *ptr = bucket->data;
    }
}

void LinkedAllocator::Release(void *ptr, Size size)
{
    if (ptr) {
        Bucket *bucket = PointerToBucket(ptr);

        if (bucket->head.next) {
            bucket->head.next->prev = bucket->head.prev;
        } else {
            list.prev = bucket->head.prev;
        }
        if (bucket->head.prev) {
            bucket->head.prev->next = bucket->head.next;
        } else {
            list.next = bucket->head.next;
        }

        Allocator::Release(allocator, bucket, size);
    }
}

void BlockAllocatorBase::ForgetCurrentBlock()
{
    current_bucket = nullptr;
    last_alloc = nullptr;
}

void *BlockAllocatorBase::Allocate(Size size, unsigned int flags)
{
    RG_ASSERT_DEBUG(size >= 0);

    LinkedAllocator *alloc = GetAllocator();

    // Keep alignement requirements
    Size aligned_size = AlignSizeValue(size);

    if (AllocateSeparately(aligned_size)) {
        uint8_t *ptr = (uint8_t *)Allocator::Allocate(alloc, size, flags);
        return ptr;
    } else {
        if (!current_bucket || (current_bucket->used + aligned_size) > block_size) {
            current_bucket = (Bucket *)Allocator::Allocate(alloc, RG_SIZE(Bucket) + block_size,
                                                           flags & ~(int)Allocator::Flag::Zero);
            current_bucket->used = 0;
        }

        uint8_t *ptr = current_bucket->data + current_bucket->used;
        current_bucket->used += aligned_size;

        if (flags & (int)Allocator::Flag::Zero) {
            memset(ptr, 0, size);
        }

        last_alloc = ptr;
        return ptr;
    }
}

void BlockAllocatorBase::Resize(void **ptr, Size old_size, Size new_size, unsigned int flags)
{
    RG_ASSERT_DEBUG(old_size >= 0);
    RG_ASSERT_DEBUG(new_size >= 0);

    if (!new_size) {
        Release(*ptr, old_size);
    } else {
        if (!*ptr) {
            old_size = 0;
        }

        Size aligned_old_size = AlignSizeValue(old_size);
        Size aligned_new_size = AlignSizeValue(new_size);
        Size aligned_delta = aligned_new_size - aligned_old_size;

        // Try fast path
        if (*ptr && *ptr == last_alloc && (current_bucket->used + aligned_delta) <= block_size &&
                !AllocateSeparately(aligned_new_size)) {
            current_bucket->used += aligned_delta;

            if ((flags & (int)Allocator::Flag::Zero) && new_size > old_size) {
                memset(ptr + old_size, 0, new_size - old_size);
            }
        } else if (AllocateSeparately(aligned_old_size)) {
            LinkedAllocator *alloc = GetAllocator();
            Allocator::Resize(alloc, ptr, old_size, new_size, flags);
        } else {
            void *new_ptr = Allocate(new_size, flags & ~(int)Allocator::Flag::Zero);
            if (new_size > old_size) {
                memcpy(new_ptr, *ptr, old_size);

                if (flags & (int)Allocator::Flag::Zero) {
                    memset(ptr + old_size, 0, new_size - old_size);
                }
            } else {
                memcpy(new_ptr, *ptr, new_size);
            }

            *ptr = new_ptr;
        }
    }
}

void BlockAllocatorBase::Release(void *ptr, Size size)
{
    RG_ASSERT_DEBUG(size >= 0);

    if (ptr) {
        LinkedAllocator *alloc = GetAllocator();

        Size aligned_size = AlignSizeValue(size);

        if (ptr == last_alloc) {
            current_bucket->used -= aligned_size;
            if (!current_bucket->used) {
                Allocator::Release(alloc, current_bucket, RG_SIZE(Bucket) + block_size);
                current_bucket = nullptr;
            }
            last_alloc = nullptr;
        } else if (AllocateSeparately(aligned_size)) {
            Allocator::Release(alloc, ptr, size);
        }
    }
}

void BlockAllocator::ReleaseAll()
{
    ForgetCurrentBlock();
    allocator.ReleaseAll();
}

// ------------------------------------------------------------------------
// Date
// ------------------------------------------------------------------------

// TODO: Rewrite the ugly parsing part
Date Date::FromString(Span<const char> date_str, int flags, Span<const char> *out_remaining)
{
    Date date;

    int parts[3] = {};
    int lengths[3] = {};
    Size offset = 0;
    for (int i = 0; i < 3; i++) {
        int mult = 1;
        while (offset < date_str.len) {
            char c = date_str[offset];
            int digit = c - '0';
            if ((unsigned int)digit < 10) {
                parts[i] = (parts[i] * 10) + digit;
                if (RG_UNLIKELY(++lengths[i] > 5))
                    goto malformed;
            } else if (!lengths[i] && c == '-' && mult == 1 && i != 1) {
                mult = -1;
            } else if (RG_UNLIKELY(i == 2 && !(flags & (int)ParseFlag::End) && c != '/' && c != '-')) {
                break;
            } else if (RG_UNLIKELY(!lengths[i] || (c != '/' && c != '-'))) {
                goto malformed;
            } else {
                offset++;
                break;
            }
            offset++;
        }
        parts[i] *= mult;
    }
    if ((flags & (int)ParseFlag::End) && offset < date_str.len)
        goto malformed;

    if (RG_UNLIKELY((unsigned int)lengths[1] > 2))
        goto malformed;
    if (RG_UNLIKELY((lengths[0] > 2) == (lengths[2] > 2))) {
        if (flags & (int)ParseFlag::Log) {
            LogError("Ambiguous date string '%1'", date_str);
        }
        return {};
    } else if (lengths[2] > 2) {
        std::swap(parts[0], parts[2]);
    }
    if (RG_UNLIKELY(parts[0] < -INT16_MAX || parts[0] > INT16_MAX || (unsigned int)parts[2] > 99))
        goto malformed;

    date.st.year = (int16_t)parts[0];
    date.st.month = (int8_t)parts[1];
    date.st.day = (int8_t)parts[2];
    if ((flags & (int)ParseFlag::Validate) && !date.IsValid()) {
        if (flags & (int)ParseFlag::Log) {
            LogError("Invalid date string '%1'", date_str);
        }
        return {};
    }

    if (out_remaining) {
        *out_remaining = date_str.Take(offset, date_str.len - offset);
    }
    return date;

malformed:
    if (flags & (int)ParseFlag::Log) {
        LogError("Malformed date string '%1'", date_str);
    }
    return {};
}

Date Date::FromJulianDays(int days)
{
    RG_ASSERT_DEBUG(days >= 0);

    // Algorithm from Richards, copied from Wikipedia:
    // https://en.wikipedia.org/w/index.php?title=Julian_day&oldid=792497863

    Date date;
    {
        int f = days + 1401 + (((4 * days + 274277) / 146097) * 3) / 4 - 38;
        int e = 4 * f + 3;
        int g = e % 1461 / 4;
        int h = 5 * g + 2;
        date.st.day = (int8_t)(h % 153 / 5 + 1);
        date.st.month = (int8_t)((h / 153 + 2) % 12 + 1);
        date.st.year = (int16_t)((e / 1461) - 4716 + (date.st.month < 3));
    }

    return date;
}

int Date::ToJulianDays() const
{
    RG_ASSERT_DEBUG(IsValid());

    // Straight from the Web:
    // http://www.cs.utsa.edu/~cs1063/projects/Spring2011/Project1/jdn-explanation.html

    int julian_days;
    {
        bool adjust = st.month < 3;
        int year = st.year + 4800 - adjust;
        int month = st.month + 12 * adjust - 3;

        julian_days = st.day + (153 * month + 2) / 5 + 365 * year - 32045 +
                      year / 4 - year / 100 + year / 400;
    }

    return julian_days;
}

int Date::GetWeekDay() const
{
    RG_ASSERT_DEBUG(IsValid());

    // Zeller's congruence:
    // https://en.wikipedia.org/wiki/Zeller%27s_congruence

    int week_day;
    {
        int year = st.year;
        int month = st.month;
        if (month < 3) {
            year--;
            month += 12;
        }

        int century = year / 100;
        year %= 100;

        week_day = (st.day + (13 * (month + 1) / 5) + year + year / 4 + century / 4 + 5 * century + 5) % 7;
    }

    return week_day;
}

Date &Date::operator++()
{
    RG_ASSERT_DEBUG(IsValid());

    if (st.day < DaysInMonth(st.year, st.month)) {
        st.day++;
    } else if (st.month < 12) {
        st.month++;
        st.day = 1;
    } else {
        st.year++;
        st.month = 1;
        st.day = 1;
    }

    return *this;
}

Date &Date::operator--()
{
    RG_ASSERT_DEBUG(IsValid());

    if (st.day > 1) {
        st.day--;
    } else if (st.month > 1) {
        st.month--;
        st.day = DaysInMonth(st.year, st.month);
    } else {
        st.year--;
        st.month = 12;
        st.day = DaysInMonth(st.year, st.month);
    }

    return *this;
}

// ------------------------------------------------------------------------
// Time
// ------------------------------------------------------------------------

int64_t g_start_time = GetMonotonicTime();

int64_t GetMonotonicTime()
{
#if defined(_WIN32)
    return (int64_t)GetTickCount64();
#elif defined(__EMSCRIPTEN__)
    return (int64_t)emscripten_get_now();
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        LogError("clock_gettime() failed: %1", strerror(errno));
        return 0;
    }

    return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
#endif
}

// ------------------------------------------------------------------------
// Strings
// ------------------------------------------------------------------------

Span<char> DuplicateString(Span<const char> str, Allocator *alloc)
{
    char *new_str = (char *)Allocator::Allocate(alloc, str.len + 1);
    memcpy(new_str, str.ptr, (size_t)str.len);
    new_str[str.len] = 0;
    return MakeSpan(new_str, str.len);
}

// ------------------------------------------------------------------------
// Format
// ------------------------------------------------------------------------

static Span<const char> FormatUnsignedToDecimal(uint64_t value, char out_buf[32])
{
    static char digit_pairs[201] = "00010203040506070809101112131415161718192021222324"
                                   "25262728293031323334353637383940414243444546474849"
                                   "50515253545556575859606162636465666768697071727374"
                                   "75767778798081828384858687888990919293949596979899";

    Size offset = 32;
    {
        int pair_idx;
        do {
            pair_idx = (int)((value % 100) * 2);
            value /= 100;
            offset -= 2;
            memcpy(out_buf + offset, digit_pairs + pair_idx, 2);
        } while (value);
        offset += (pair_idx < 20);
    }

    return MakeSpan(out_buf + offset, 32 - offset);
}

static Span<const char> FormatUnsignedToHex(uint64_t value, char out_buf[32])
{
    static const char literals[] = "0123456789ABCDEF";

    Size offset = 32;
    do {
        uint64_t digit = value & 0xF;
        value >>= 4;
        out_buf[--offset] = literals[digit];
    } while (value);

    return MakeSpan(out_buf + offset, 32 - offset);
}

static Span<const char> FormatUnsignedToBinary(uint64_t value, char out_buf[64])
{
    Size msb = 64 - (Size)CountLeadingZeros(value);
    if (!msb) {
        msb = 1;
    }

    for (Size i = 0; i < msb; i++) {
        bool bit = (value >> (msb - i - 1)) & 0x1;
        out_buf[i] = bit ? '1' : '0';
    }

    return MakeSpan(out_buf, msb);
}

static Span<const char> FormatDouble(double value, int precision, char out_buf[256])
{
    // That's the lazy way to do it, it'll do for now
    int buf_len;
    if (precision >= 0) {
        buf_len = snprintf(out_buf, 256, "%.*f", precision, value);
    } else {
        buf_len = snprintf(out_buf, 256, "%g", value);
    }
    RG_ASSERT_DEBUG(buf_len >= 0 && buf_len < 256);

    return MakeSpan(out_buf, (Size)buf_len);
}

template <typename AppendFunc>
static inline void ProcessArg(const FmtArg &arg, AppendFunc append)
{
    for (int i = 0; i < arg.repeat; i++) {
        LocalArray<char, 512> out_buf;
        char num_buf[256];
        Span<const char> out;

        Size pad_len = arg.pad_len;

        switch (arg.type) {
            case FmtArg::Type::Str1: { out = arg.value.str1; } break;
            case FmtArg::Type::Str2: { out = arg.value.str2; } break;
            case FmtArg::Type::Buffer: { out = arg.value.buf; } break;
            case FmtArg::Type::Char: { out = MakeSpan(&arg.value.ch, 1); } break;

            case FmtArg::Type::Bool: {
                if (arg.value.b) {
                    out = "true";
                } else {
                    out = "false";
                }
            } break;

            case FmtArg::Type::Integer: {
                if (arg.value.i < 0) {
                    if (arg.pad_len < 0 && arg.pad_char == '0') {
                        append('-');
                        pad_len++;
                    } else {
                        out_buf.Append('-');
                    }
                    out_buf.Append(FormatUnsignedToDecimal((uint64_t)-arg.value.i, num_buf));
                    out = out_buf;
                } else {
                    out = FormatUnsignedToDecimal((uint64_t)arg.value.i, num_buf);
                }
            } break;
            case FmtArg::Type::Unsigned: {
                out = FormatUnsignedToDecimal(arg.value.u, num_buf);
            } break;
            case FmtArg::Type::Double: {
                if (arg.value.i < 0 && arg.pad_len < 0 && arg.pad_char == '0') {
                    append('-');
                    pad_len++;

                    out = FormatDouble(-arg.value.d.value, arg.value.d.precision, num_buf);
                } else {
                    out = FormatDouble(arg.value.d.value, arg.value.d.precision, num_buf);
                }
            } break;
            case FmtArg::Type::Binary: {
                out = FormatUnsignedToBinary(arg.value.u, num_buf);
            } break;
            case FmtArg::Type::Hexadecimal: {
                out = FormatUnsignedToHex(arg.value.u, num_buf);
            } break;

            case FmtArg::Type::MemorySize: {
                size_t size_unsigned;
                if (arg.value.size < 0) {
                    size_unsigned = (size_t)-arg.value.size;
                    if (arg.pad_len < 0 && arg.pad_char == '0') {
                        append('-');
                        pad_len++;
                    } else {
                        out_buf.Append('-');
                    }
                } else {
                    size_unsigned = (size_t)arg.value.size;
                }
                if (size_unsigned > 1024 * 1024) {
                    double size_mib = (double)size_unsigned / (1024.0 * 1024.0);
                    out_buf.Append(FormatDouble(size_mib, 2, num_buf));
                    out_buf.Append(" MiB");
                } else if (size_unsigned > 1024) {
                    double size_kib = (double)size_unsigned / 1024.0;
                    out_buf.Append(FormatDouble(size_kib, 2, num_buf));
                    out_buf.Append(" kiB");
                } else {
                    out_buf.Append(FormatUnsignedToDecimal(size_unsigned, num_buf));
                    out_buf.Append(" B");
                }
                out = out_buf;
            } break;
            case FmtArg::Type::DiskSize: {
                size_t size_unsigned;
                if (arg.value.size < 0) {
                    size_unsigned = (size_t)-arg.value.size;
                    if (arg.pad_len < 0 && arg.pad_char == '0') {
                        append('-');
                        pad_len++;
                    } else {
                        out_buf.Append('-');
                    }
                } else {
                    size_unsigned = (size_t)arg.value.size;
                }
                if (size_unsigned > 1000 * 1000) {
                    double size_mib = (double)size_unsigned / (1000.0 * 1000.0);
                    out_buf.Append(FormatDouble(size_mib, 2, num_buf));
                    out_buf.Append(" MB");
                } else if (size_unsigned > 1000) {
                    double size_kib = (double)size_unsigned / 1000.0;
                    out_buf.Append(FormatDouble(size_kib, 2, num_buf));
                    out_buf.Append(" kB");
                } else {
                    out_buf.Append(FormatUnsignedToDecimal(size_unsigned, num_buf));
                    out_buf.Append(" B");
                }
                out = out_buf;
            } break;

            case FmtArg::Type::Date: {
                RG_ASSERT_DEBUG(!arg.value.date.value || arg.value.date.IsValid());

                int year = arg.value.date.st.year;
                if (year < 0) {
                    out_buf.Append('-');
                    year = -year;
                }
                if (year < 10) {
                    out_buf.Append("000");
                } else if (year < 100) {
                    out_buf.Append("00");
                } else if (year < 1000) {
                    out_buf.Append('0');
                }
                out_buf.Append(FormatUnsignedToDecimal((uint64_t)year, num_buf));
                out_buf.Append('-');
                if (arg.value.date.st.month < 10) {
                    out_buf.Append('0');
                }
                out_buf.Append(FormatUnsignedToDecimal((uint64_t)arg.value.date.st.month, num_buf));
                out_buf.Append('-');
                if (arg.value.date.st.day < 10) {
                    out_buf.Append('0');
                }
                out_buf.Append(FormatUnsignedToDecimal((uint64_t)arg.value.date.st.day, num_buf));
                out = out_buf;
            } break;

            case FmtArg::Type::Span: {
                FmtArg arg2;
                arg2.type = arg.value.span.type;
                arg2.repeat = arg.repeat;
                arg2.pad_len = arg.pad_len;
                arg2.pad_char = arg.pad_char;

                const uint8_t *ptr = (const uint8_t *)arg.value.span.ptr;
                for (Size j = 0; j < arg.value.span.len; j++) {
                    switch (arg.value.span.type) {
                        case FmtArg::Type::Str1: { arg2.value.str1 = *(const char **)ptr; } break;
                        case FmtArg::Type::Str2: { arg2.value.str2 = *(const Span<const char> *)ptr; } break;
                        case FmtArg::Type::Buffer: { RG_ASSERT(false); } break;
                        case FmtArg::Type::Char: { arg2.value.ch = *(const char *)ptr; } break;
                        case FmtArg::Type::Bool: { arg2.value.b = *(const bool *)ptr; } break;
                        case FmtArg::Type::Integer:
                        case FmtArg::Type::Unsigned:
                        case FmtArg::Type::Binary:
                        case FmtArg::Type::Hexadecimal: {
                            switch (arg.value.span.type_len) {
                                case 8: { arg2.value.u = *(const uint64_t *)ptr; } break;
                                case 4: { arg2.value.u = *(const uint32_t *)ptr; } break;
                                case 2: { arg2.value.u = *(const uint16_t *)ptr; } break;
                                case 1: { arg2.value.u = *(const uint8_t *)ptr; } break;
                                default: { RG_ASSERT(false); } break;
                            }
                        } break;
                        case FmtArg::Type::Double: {
                            switch (arg.value.span.type_len) {
                                case RG_SIZE(double): { arg2.value.d.value = *(const double *)ptr; } break;
                                case RG_SIZE(float): { arg2.value.d.value = *(const float *)ptr; } break;
                                default: { RG_ASSERT(false); } break;
                            }
                            arg2.value.d.precision = -1;
                        } break;
                        case FmtArg::Type::MemorySize: { arg2.value.size = *(const Size *)ptr; } break;
                        case FmtArg::Type::DiskSize: { arg2.value.size = *(const Size *)ptr; } break;
                        case FmtArg::Type::Date: { arg2.value.date = *(const Date *)ptr; } break;
                        case FmtArg::Type::Span: { RG_ASSERT(false); } break;
                    }
                    ptr += arg.value.span.type_len;

                    if (j) {
                        append(arg.value.span.separator);
                    }
                    ProcessArg(arg2, append);
                }

                out = {};
                pad_len = 0;
            } break;
        }

        if (pad_len < 0) {
            pad_len = (-pad_len) - out.len;
            for (Size i = 0; i < pad_len; i++) {
                append(arg.pad_char);
            }
            append(out);
        } else if (pad_len > 0) {
            append(out);
            pad_len -= out.len;
            for (Size i = 0; i < pad_len; i++) {
                append(arg.pad_char);
            }
        } else {
            append(out);
        }
    }
}

template <typename AppendFunc>
static inline void DoFormat(const char *fmt, Span<const FmtArg> args, AppendFunc append)
{
#ifndef NDEBUG
    bool invalid_marker = false;
    uint32_t unused_arguments = ((uint32_t)1 << args.len) - 1;
#endif

    const char *fmt_ptr = fmt;
    for (;;) {
        // Find the next marker (or the end of string) and write everything before it
        const char *marker_ptr = fmt_ptr;
        while (marker_ptr[0] && marker_ptr[0] != '%') {
            marker_ptr++;
        }
        append(MakeSpan(fmt_ptr, (Size)(marker_ptr - fmt_ptr)));
        if (!marker_ptr[0])
            break;

        // Try to interpret this marker as a number
        Size idx = 0;
        Size idx_end = 1;
        for (;;) {
            // Unsigned cast makes the test below quicker, don't remove it or it'll break
            unsigned int digit = (unsigned int)marker_ptr[idx_end] - '0';
            if (digit > 9)
                break;
            idx = (Size)(idx * 10) + (Size)digit;
            idx_end++;
        }

        // That was indeed a number
        if (idx_end > 1) {
            idx--;
            if (idx < args.len) {
                ProcessArg<AppendFunc>(args[idx], append);
#ifndef NDEBUG
                unused_arguments &= ~((uint32_t)1 << idx);
            } else {
                invalid_marker = true;
#endif
            }
            fmt_ptr = marker_ptr + idx_end;
        } else if (marker_ptr[1] == '%') {
            append('%');
            fmt_ptr = marker_ptr + 2;
        } else if (marker_ptr[1] == '/') {
            append(*RG_PATH_SEPARATORS);
            fmt_ptr = marker_ptr + 2;
        } else if (marker_ptr[1]) {
            append(marker_ptr[0]);
            fmt_ptr = marker_ptr + 1;
#ifndef NDEBUG
            invalid_marker = true;
#endif
        } else {
#ifndef NDEBUG
            invalid_marker = true;
#endif
            break;
        }
    }

#ifndef NDEBUG
    if (invalid_marker && unused_arguments) {
        fprintf(stderr, "\nLog format string '%s' has invalid markers and unused arguments\n", fmt);
    } else if (unused_arguments) {
        fprintf(stderr, "\nLog format string '%s' has unused arguments\n", fmt);
    } else if (invalid_marker) {
        fprintf(stderr, "\nLog format string '%s' has invalid markers\n", fmt);
    }
#endif
}

Span<char> FmtFmt(const char *fmt, Span<const FmtArg> args, Span<char> out_buf)
{
    RG_ASSERT_DEBUG(out_buf.len >= 0);

    if (!out_buf.len)
        return {};
    out_buf.len--;

    Size real_len = 0;

    DoFormat(fmt, args, [&](Span<const char> fragment) {
        if (RG_LIKELY(real_len < out_buf.len)) {
            Size copy_len = fragment.len;
            if (copy_len > out_buf.len - real_len) {
                copy_len = out_buf.len - real_len;
            }
            memcpy(out_buf.ptr + real_len, fragment.ptr, (size_t)copy_len);
        }
        real_len += fragment.len;
    });
    if (real_len < out_buf.len) {
        out_buf.len = real_len;
    }
    out_buf.ptr[out_buf.len] = 0;

    return out_buf;
}

Span<char> FmtFmt(const char *fmt, Span<const FmtArg> args, HeapArray<char> *out_buf)
{
    Size start_len = out_buf->len;

    out_buf->Grow(RG_FMT_STRING_BASE_CAPACITY);
    DoFormat(fmt, args, [&](Span<const char> frag) {
        out_buf->Grow(frag.len + 1);
        memcpy(out_buf->end(), frag.ptr, (size_t)frag.len);
        out_buf->len += frag.len;
    });
    out_buf->ptr[out_buf->len] = 0;

    return out_buf->Take(start_len, out_buf->len - start_len);
}

Span<char> FmtFmt(const char *fmt, Span<const FmtArg> args, Allocator *alloc)
{
    HeapArray<char> buf(alloc);
    FmtFmt(fmt, args, &buf);
    return buf.Leak();
}

void PrintFmt(const char *fmt, Span<const FmtArg> args, StreamWriter *st)
{
    LocalArray<char, RG_FMT_STRING_PRINT_BUFFER_SIZE> buf;
    DoFormat(fmt, args, [&](Span<const char> frag) {
        if (frag.len > RG_LEN(buf.data) - buf.len) {
            st->Write(buf);
            buf.len = 0;
        }
        if (frag.len >= RG_LEN(buf.data)) {
            st->Write(frag);
        } else {
            memcpy(buf.data + buf.len, frag.ptr, (size_t)frag.len);
            buf.len += frag.len;
        }
    });
    st->Write(buf);
}

void PrintFmt(const char *fmt, Span<const FmtArg> args, FILE *fp)
{
    // TODO: Deal properly with partial writes in PrintFmt(FILE) overload

    LocalArray<char, RG_FMT_STRING_PRINT_BUFFER_SIZE> buf;
    DoFormat(fmt, args, [&](Span<const char> frag) {
        if (frag.len > RG_LEN(buf.data) - buf.len) {
            fwrite(buf.data, 1, (size_t)buf.len, fp);
            buf.len = 0;
        }
        if (frag.len >= RG_LEN(buf.data)) {
            fwrite(frag.ptr, 1, (size_t)frag.len, fp);
        } else {
            memcpy(buf.data + buf.len, frag.ptr, (size_t)frag.len);
            buf.len += frag.len;
        }
    });
    fwrite(buf.data, 1, (size_t)buf.len, fp);
}

void PrintLnFmt(const char *fmt, Span<const FmtArg> args, StreamWriter *st)
{
    PrintFmt(fmt, args, st);
    st->Write('\n');
}
void PrintLnFmt(const char *fmt, Span<const FmtArg> args, FILE *fp)
{
    PrintFmt(fmt, args, fp);
    fputc('\n', fp);
}

// ------------------------------------------------------------------------
// Debug and errors
// ------------------------------------------------------------------------

// NOTE: LocalArray does not work with __thread, and thread_local is broken on MinGW
// when destructors are involved. So heap allocation it is, at least for now.
static RG_THREAD_LOCAL std::function<LogHandlerFunc> *log_handlers[16];
static RG_THREAD_LOCAL Size log_handlers_len;

static RG_THREAD_LOCAL char log_last_error[512];

bool GetDebugFlag(const char *name)
{
    LogDebug("Checked debug flag '%1'", name);

#ifdef __EMSCRIPTEN__
    return EM_ASM_INT({
        try {
            var debug_env_name = UTF8ToString($0);
            return (process.env[debug_env_name] !== undefined && process.env[debug_env_name] != 0);
        } catch (error) {
            return 0;
        }
    }, name);
#else
    const char *debug = getenv(name);

    if (!debug || TestStr(debug, "0")) {
        return false;
    } else if (TestStr(debug, "1")) {
        return true;
    } else {
        LogError("%1 should contain value '0' or '1'", name);
        return true;
    }
#endif
}

bool LogUsesTerminalOutput()
{
    static bool init, output_is_terminal;

    if (!init) {
#ifdef _WIN32
        static HANDLE stderr_handle = (HANDLE)_get_osfhandle(_fileno(stderr));
        static DWORD orig_console_mode;

        if (GetConsoleMode(stderr_handle, &orig_console_mode)) {
            output_is_terminal = orig_console_mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING;

            if (!output_is_terminal) {
                // Enable VT100 escape sequences, introduced in Windows 10
                DWORD new_mode = orig_console_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                output_is_terminal = SetConsoleMode(stderr_handle, new_mode);

                if (output_is_terminal) {
                    atexit([]() {
                        SetConsoleMode(stderr_handle, orig_console_mode);
                    });
                } else {
                    // Try ConEmu ANSI support for Windows < 10
                    const char *conemuansi_str = getenv("ConEmuANSI");
                    output_is_terminal = conemuansi_str && TestStr(conemuansi_str, "ON");
                }
            }
        }

        if (output_is_terminal) {
            atexit([]() {
                ::WriteFile(stderr_handle, "\x1B[0m", (DWORD)strlen("\x1B[0m"), nullptr, nullptr);
            });
        }
#else
        output_is_terminal = isatty(fileno(stderr));
        if (output_is_terminal) {
            atexit([]() {
                size_t ret = write(fileno(stderr), "\x1B[0m", strlen("\x1B[0m"));
                (void)ret;
            });
        }
#endif

        init = true;
    }

    return output_is_terminal;
}

void LogFmt(LogLevel level, const char *fmt, Span<const FmtArg> args)
{
    static std::mutex log_mutex;

    char ctx_buf[128];
    char msg_buf[RG_SIZE(log_last_error)];
    {
        double time = (double)(GetMonotonicTime() - g_start_time) / 1000;
        Fmt(ctx_buf, " [%1] ", FmtDouble(time, 3).Pad(-8));

        FmtFmt(fmt, args, msg_buf);
    }

    if (level == LogLevel::Error) {
        strcpy(log_last_error, msg_buf);
    }

    // FIXME: Avoid need for mutex in Log API
    std::lock_guard<std::mutex> lock(log_mutex);
    if (log_handlers_len) {
        (*log_handlers[log_handlers_len - 1])(level, ctx_buf, msg_buf);
    } else {
        DefaultLogHandler(level, ctx_buf, msg_buf);
    }
}

void DefaultLogHandler(LogLevel level, const char *ctx, const char *msg)
{
    StartConsoleLog(level);
    Print(stderr, "%1%2", ctx, msg);
    EndConsoleLog();
}

void StartConsoleLog(LogLevel level)
{
    if (LogUsesTerminalOutput()) {
        switch (level)  {
            case LogLevel::Error: { fputs("\x1B[31m", stderr); } break;
            case LogLevel::Info: { fputs("\x1B[96m", stderr); } break;
            case LogLevel::Debug: { fputs("\x1B[90m", stderr); } break;
        }
    }
}

void EndConsoleLog()
{
    if (LogUsesTerminalOutput()) {
        fputs("\x1B[0m", stderr);
    }
    fputs("\n", stderr);
}

void PushLogHandler(std::function<LogHandlerFunc> handler)
{
    RG_ASSERT_DEBUG(log_handlers_len < RG_LEN(log_handlers));
    log_handlers[log_handlers_len++] = new std::function<LogHandlerFunc>(handler);
}

void PopLogHandler()
{
    RG_ASSERT_DEBUG(log_handlers_len > 0);
    delete log_handlers[--log_handlers_len];
}

const char *GetLastLogError()
{
    return log_last_error;
}

// ------------------------------------------------------------------------
// System
// ------------------------------------------------------------------------

#ifdef _WIN32

char *Win32ErrorString(uint32_t error_code)
{
    if (error_code == UINT32_MAX) {
        error_code = GetLastError();
    }

    static RG_THREAD_LOCAL char str_buf[256];
    DWORD fmt_ret = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                  nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                  str_buf, RG_SIZE(str_buf), nullptr);
    if (fmt_ret) {
        char *str_end = str_buf + strlen(str_buf);
        // FormatMessage adds newlines, remove them
        while (str_end > str_buf && (str_end[-1] == '\n' || str_end[-1] == '\r'))
            str_end--;
        *str_end = 0;
    } else {
        strcpy(str_buf, "(unknown)");
    }

    return str_buf;
}

static FileType FileAttributesToType(uint32_t attr)
{
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        return FileType::Directory;
    } else if (attr & FILE_ATTRIBUTE_DEVICE) {
        return FileType::Unknown;
    } else {
        return FileType::File;
    }
}

static int64_t FileTimeToUnixTime(FILETIME ft)
{
    int64_t time = ((int64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return time / 10000000 - 11644473600ll;
}

bool StatFile(const char *filename, bool error_if_missing, FileInfo *out_info)
{
    HANDLE h = CreateFile(filename, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (error_if_missing ||
                (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND)) {
            LogError("Cannot stat file '%1': %2", filename, Win32ErrorString(err));
        }
        return false;
    }
    RG_DEFER { CloseHandle(h); };

    BY_HANDLE_FILE_INFORMATION attr;
    if (!GetFileInformationByHandle(h, &attr)) {
        LogError("Cannot stat file '%1': %2", filename, Win32ErrorString());
        return false;
    }

    out_info->type = FileAttributesToType(attr.dwFileAttributes);
    out_info->size = ((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
    out_info->modification_time = FileTimeToUnixTime(attr.ftLastWriteTime);

    return true;
}

EnumStatus EnumerateDirectory(const char *dirname, const char *filter, Size max_files,
                              std::function<bool(const char *, FileType)> func)
{
    char find_filter[4096];
    if (!filter) {
        filter = "*";
    }
    if (snprintf(find_filter, RG_SIZE(find_filter), "%s\\%s", dirname, filter) >= RG_SIZE(find_filter)) {
        LogError("Cannot enumerate directory '%1': Path too long", dirname);
        return EnumStatus::Error;
    }

    WIN32_FIND_DATA find_data;
    HANDLE handle = FindFirstFileEx(find_filter, FindExInfoBasic, &find_data,
                                    FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (handle == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            DWORD attrib = GetFileAttributes(dirname);
            if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY))
                return EnumStatus::Done;
        }

        LogError("Cannot enumerate directory '%1': %2", dirname,
                 Win32ErrorString());
        return EnumStatus::Error;
    }
    RG_DEFER { FindClose(handle); };

    Size count = 0;
    do {
        if ((find_data.cFileName[0] == '.' && !find_data.cFileName[1]) ||
                (find_data.cFileName[0] == '.' && find_data.cFileName[1] == '.' && !find_data.cFileName[2]))
            continue;

        if (RG_UNLIKELY(++count > max_files && max_files >= 0)) {
            LogError("Partial enumation of directory '%1'", dirname);
            return EnumStatus::Partial;
        }

        FileType file_type = FileAttributesToType(find_data.dwFileAttributes);

        if (!func(find_data.cFileName, file_type))
            return EnumStatus::Partial;
    } while (FindNextFile(handle, &find_data));

    if (GetLastError() != ERROR_NO_MORE_FILES) {
        LogError("Error while enumerating directory '%1': %2", dirname,
                 Win32ErrorString());
        return EnumStatus::Error;
    }

    return EnumStatus::Done;
}

#else

bool StatFile(const char *filename, bool error_if_missing, FileInfo *out_info)
{
    struct stat sb;
    if (stat(filename, &sb) < 0) {
        if (error_if_missing || errno != ENOENT) {
            LogError("Cannot stat '%1': %2", filename, strerror(errno));
        }
        return false;
    }

    if (S_ISDIR(sb.st_mode)) {
        out_info->type = FileType::Directory;
    } else if (S_ISREG(sb.st_mode)) {
        out_info->type = FileType::File;
    } else {
        out_info->type = FileType::Unknown;
    }

    out_info->size = (int64_t)sb.st_size;
#if defined(__linux__)
    out_info->modification_time = (int64_t)sb.st_mtim.tv_sec * 1000 +
                                  (int64_t)sb.st_mtim.tv_nsec / 1000000;
#elif defined(__APPLE__)
    out_info->modification_time = (int64_t)sb.st_mtimespec.tv_sec * 1000 +
                                  (int64_t)sb.st_mtimespec.tv_nsec / 1000000;
#else
    out_info->modification_time = (int64_t)sb.st_mtime * 1000;
#endif

    return true;
}

EnumStatus EnumerateDirectory(const char *dirname, const char *filter, Size max_files,
                              std::function<bool(const char *, FileType)> func)
{
    DIR *dirp = opendir(dirname);
    if (!dirp) {
        LogError("Cannot enumerate directory '%1': %2", dirname, strerror(errno));
        return EnumStatus::Error;
    }
    RG_DEFER { closedir(dirp); };

    Size count = 0;
    dirent *dent;
    while ((dent = readdir(dirp))) {
        if ((dent->d_name[0] == '.' && !dent->d_name[1]) ||
                (dent->d_name[0] == '.' && dent->d_name[1] == '.' && !dent->d_name[2]))
            continue;

        if (!filter || !fnmatch(filter, dent->d_name, FNM_PERIOD)) {
            if (RG_UNLIKELY(++count > max_files && max_files >= 0)) {
                LogError("Partial enumation of directory '%1'", dirname);
                return EnumStatus::Partial;
            }

            FileType file_type;
#ifdef _DIRENT_HAVE_D_TYPE
            if (dent->d_type != DT_UNKNOWN && dent->d_type != DT_LNK) {
                switch (dent->d_type) {
                    case DT_DIR: { file_type = FileType::Directory; } break;
                    case DT_REG: { file_type = FileType::File; } break;
                    default: { file_type = FileType::Unknown; } break;
                }
            } else
#endif
            {
                struct stat sb;
                if (fstatat(dirfd(dirp), dent->d_name, &sb, 0) < 0) {
                    LogError("Ignoring file '%1' in '%2' (stat failed)",
                             dent->d_name, dirname);
                    continue;
                }
                if (S_ISDIR(sb.st_mode)) {
                    file_type = FileType::Directory;
                } else if (S_ISREG(sb.st_mode)) {
                    file_type = FileType::File;
                } else {
                    file_type = FileType::Unknown;
                }
            }

            if (!func(dent->d_name, file_type))
                return EnumStatus::Partial;
        }

        errno = 0;
    }

    if (errno) {
        LogError("Error while enumerating directory '%1': %2", dirname, strerror(errno));
        return EnumStatus::Error;
    }

    return EnumStatus::Done;
}

#endif

bool EnumerateFiles(const char *dirname, const char *filter, Size max_depth, Size max_files,
                    Allocator *str_alloc, HeapArray<const char *> *out_files)
{
    RG_DEFER_NC(out_guard, len = out_files->len) { out_files->RemoveFrom(len); };

    EnumStatus status = EnumerateDirectory(dirname, filter, max_files,
                                           [&](const char *filename, FileType file_type) {
        switch (file_type) {
            case FileType::Directory: {
                if (max_depth) {
                    const char *sub_directory = Fmt(str_alloc, "%1%/%2", dirname, filename).ptr;
                    return EnumerateFiles(sub_directory, filter, std::max((Size)-1, max_depth - 1),
                                          max_files, str_alloc, out_files);
                }
            } break;
            case FileType::File: {
                out_files->Append(Fmt(str_alloc, "%1%/%2", dirname, filename).ptr);
            } break;

            case FileType::Unknown: {} break;
        }

        return true;
    });
    if (status == EnumStatus::Error)
        return false;

    out_guard.Disable();
    return true;
}

bool TestFile(const char *filename, FileType type)
{
    FileInfo file_info;
    if (!StatFile(filename, false, &file_info))
        return false;

    if (type != FileType::Unknown && type != file_info.type) {
        switch (type) {
            case FileType::Directory: { LogError("Path '%1' is not a directory", filename); } break;
            case FileType::File: { LogError("Path '%1' is not a file", filename); } break;
            case FileType::Unknown: { RG_ASSERT_DEBUG(false); } break;
        }

        return false;
    }

    return true;
}

bool MatchPathName(const char *name, const char *pattern)
{
#ifdef _WIN32
    return PathMatchSpecA(name, pattern);
#else
    return !fnmatch(pattern, name, 0);
#endif
}

bool SetWorkingDirectory(const char *directory)
{
#ifdef _WIN32
    if (!SetCurrentDirectory(directory)) {
        LogError("Failed to set current directory to '%1': %2", directory, Win32ErrorString());
        return false;
    }
#else
    if (chdir(directory) < 0) {
        LogError("Failed to set current directory to '%1': %2", directory, strerror(errno));
        return false;
    }
#endif

    return true;
}

const char *GetWorkingDirectory()
{
    static RG_THREAD_LOCAL char buf[4096];

#ifdef _WIN32
    DWORD ret = GetCurrentDirectory(RG_SIZE(buf), buf);
    RG_ASSERT(ret && ret <= RG_SIZE(buf));
#else
    RG_ASSERT(getcwd(buf, RG_SIZE(buf)));
#endif

    return buf;
}

#ifdef __EMSCRIPTEN__
static bool running_in_node;

INIT(MountHostFilesystem)
{
    running_in_node = EM_ASM_INT({
        try {
            var path = require('path');
            if (process.platform == 'win32') {
                FS.mkdir('/host');
                for (var c = 'a'.charCodeAt(0); c <= 'z'.charCodeAt(0); c++) {
                    var disk_path = String.fromCharCode(c) + ':';
                    var mount_point = '/host/' + String.fromCharCode(c);
                    FS.mkdir(mount_point);
                    try {
                        FS.mount(NODEFS, { root: disk_path }, mount_point);
                    } catch(error) {
                        FS.rmdir(mount_point);
                    }
                }

                var real_app_dir = path.dirname(process.mainModule.filename);
                var app_dir = '/host/' + real_app_dir[0].toLowerCase() +
                              real_app_dir.substr(2).replace(/\\\\\\\\/g, '/');
            } else {
                FS.mkdir('/host');
                FS.mount(NODEFS, { root: '/' }, '/host');

                var app_dir = '/host' + path.dirname(process.mainModule.filename);
            }
        } catch (error) {
            // Running in browser (maybe)
            return 0;
        }

        FS.mkdir('/work');
        FS.mount(NODEFS, { root: '.' }, '/work');
        FS.symlink(app_dir, '/app');

        return 1;
    });

    if (running_in_node) {
        chdir("/work");
    }
}
#endif

const char *GetApplicationExecutable()
{
#if defined(_WIN32)
    static char executable_path[4096];

    if (!executable_path[0]) {
        Size path_len = (Size)GetModuleFileName(nullptr, executable_path, RG_SIZE(executable_path));
        RG_ASSERT(path_len);
        RG_ASSERT(path_len < RG_SIZE(executable_path));
    }

    return executable_path;
#elif defined(__APPLE__)
    static char executable_path[4096];

    if (!executable_path[0]) {
        uint32_t buffer_size = RG_SIZE(executable_path);
        RG_ASSERT(!_NSGetExecutablePath(executable_path, &buffer_size));
        char *path_buf = realpath(executable_path, nullptr);
        RG_ASSERT(path_buf);
        RG_ASSERT(strlen(path_buf) < RG_SIZE(executable_path));
        strcpy(executable_path, path_buf);
        free(path_buf);
    }

    return executable_path;
#elif defined(__linux__)
    static char executable_path[4096];

    if (!executable_path[0]) {
        char *path_buf = realpath("/proc/self/exe", nullptr);
        RG_ASSERT(path_buf);
        RG_ASSERT(strlen(path_buf) < RG_SIZE(executable_path));
        strcpy(executable_path, path_buf);
        free(path_buf);
    }

    return executable_path;
#elif defined(__EMSCRIPTEN__)
    return nullptr;
#else
    #error GetApplicationExecutable() not implemented for this platform
#endif
}

const char *GetApplicationDirectory()
{
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
    static char executable_dir[4096];

    if (!executable_dir[0]) {
        const char *executable_path = GetApplicationExecutable();
        Size dir_len = (Size)strlen(executable_path);
        while (dir_len && !IsPathSeparator(executable_path[--dir_len]));
        memcpy(executable_dir, executable_path, (size_t)dir_len);
        executable_dir[dir_len] = 0;
    }

    return executable_dir;
#elif defined(__EMSCRIPTEN__)
    return running_in_node ? "/app" : nullptr;
#else
    #error GetApplicationDirectory() not implemented for this platform
#endif
}

CompressionType GetPathCompression(Span<const char> filename)
{
    CompressionType compression_type;
    GetPathExtension(filename, &compression_type);
    return compression_type;
}

// Names starting with a dot are not considered to be an extension (POSIX hidden files)
Span<const char> GetPathExtension(Span<const char> filename, CompressionType *out_compression_type)
{
    filename = SplitStrReverseAny(filename, RG_PATH_SEPARATORS);

    Span<const char> extension = {};
    const auto GetNextExtension = [&]() {
        extension = SplitStrReverse(filename, '.', &filename);
        if (extension.ptr > filename.ptr) {
            extension.ptr--;
            extension.len++;
        } else {
            extension = {};
        }
    };

    GetNextExtension();
    if (out_compression_type) {
        if (TestStr(extension, ".gz")) {
            *out_compression_type = CompressionType::Gzip;
            GetNextExtension();
        } else {
            *out_compression_type = CompressionType::None;
        }
    }

    return extension;
}

Span<char> NormalizePath(Span<const char> path, Span<const char> root_directory,
                               Allocator *alloc)
{
    if (!path.len && !root_directory.len)
        return Fmt(alloc, "");

    HeapArray<char> buf;
    buf.allocator = alloc;

    const auto AppendNormalizedPath = [&](Span<const char> path) {
        Size parts_count = 0;

        if (!buf.len && PathIsAbsolute(path)) {
            Span<const char> prefix = SplitStrAny(path, RG_PATH_SEPARATORS, &path);
            buf.Append(prefix);
            buf.Append(*RG_PATH_SEPARATORS);
        }

        while (path.len) {
            Span<const char> part = SplitStrAny(path, RG_PATH_SEPARATORS, &path);

            if (part == "..") {
                if (parts_count) {
                    while (--buf.len && !IsPathSeparator(buf.ptr[buf.len - 1]));
                    parts_count--;
                } else {
                    buf.Append("..");
                    buf.Append(*RG_PATH_SEPARATORS);
                }
            } else if (part == ".") {
                // Skip
            } else if (part.len) {
                buf.Append(part);
                buf.Append(*RG_PATH_SEPARATORS);
                parts_count++;
            }
        }
    };

    if (root_directory.len && (!path.len || !PathIsAbsolute(path))) {
        AppendNormalizedPath(root_directory);
    }
    AppendNormalizedPath(path);

    if (!buf.len) {
        buf.Append('.');
        buf.Append(0);
    } else if (buf.len == 1 && IsPathSeparator(buf[0])) {
        // Root '/', keep as-is
        buf.Append(0);
    } else {
        // Strip last separator
        buf.ptr[--buf.len] = 0;
    }

    return buf.Leak();
}

bool PathIsAbsolute(const char *path)
{
#ifdef _WIN32
    if (IsAsciiAlpha(path[0]) && path[1] == ':')
        return true;
#endif

    return IsPathSeparator(path[0]);
}
bool PathIsAbsolute(Span<const char> path)
{
#ifdef _WIN32
    if (path.len >= 2 && IsAsciiAlpha(path[0]) && path[1] == ':')
        return true;
#endif

    return path.len && IsPathSeparator(path[0]);
}

bool PathContainsDotDot(const char *path)
{
    const char *ptr = path;

    while ((ptr = strstr(ptr, ".."))) {
        if ((ptr == path || IsPathSeparator(ptr[-1])) && (IsPathSeparator(ptr[2]) || !ptr[2]))
            return true;
        ptr += 2;
    }

    return false;
}

FILE *OpenFile(const char *path, OpenFileMode mode)
{
    char mode_str[8] = {};
    switch (mode) {
        case OpenFileMode::Read: { strcpy(mode_str, "rb"); } break;
        case OpenFileMode::Write: { strcpy(mode_str, "wb"); } break;
        case OpenFileMode::Append: { strcpy(mode_str, "ab"); } break;
    }
#ifndef _WIN32
    // Set the O_CLOEXEC flag
    strcat(mode_str, "e");
#else
    // Set commit flag (_commit when fflush is called)
    strcat(mode_str, "c");
#endif

    FILE *fp = fopen(path, mode_str);
    if (!fp) {
        LogError("Cannot open '%1': %2", path, strerror(errno));
    }
    return fp;
}

bool MakeDirectory(const char *directory, bool error_if_exists)
{
#ifdef _WIN32
    if (_mkdir(directory) < 0 && (errno != EEXIST || error_if_exists)) {
#else
    if (mkdir(directory, 0755) < 0 && (errno != EEXIST || error_if_exists)) {
#endif
        LogError("Cannot create directory '%1': %2", directory, strerror(errno));
        return false;
    }

    return true;
}

bool MakeDirectoryRec(Span<const char> directory)
{
    char buf[4096];
    if (RG_UNLIKELY(directory.len >= RG_SIZE(buf))) {
        LogError("Path '%1' is too large", directory);
        return false;
    }
    memcpy(buf, directory.ptr, directory.len);
    buf[directory.len] = 0;

    Size offset = directory.len + 1;
    for (; offset > 0; offset--) {
        if (!buf[offset] || IsPathSeparator(buf[offset])) {
            buf[offset] = 0;

#ifdef _WIN32
            if (_mkdir(buf) == 0 || errno == EEXIST) {
#else
            if (mkdir(buf, 0755) == 0 || errno == EEXIST) {
#endif
                break;
            } else if (errno != ENOENT) {
                LogError("Cannot create directory '%1': %2", buf, strerror(errno));
                return false;
            }
        }
    }

    for (; offset < directory.len; offset++) {
        if (!buf[offset]) {
            buf[offset] = *RG_PATH_SEPARATORS;

            if (!MakeDirectory(buf, false)) {
                LogError("Cannot create directory '%1': %2", buf, strerror(errno));
                return false;
            }
        }
    }

    return true;
}

bool EnsureDirectoryExists(const char *filename)
{
    Span<const char> directory;
    SplitStrReverseAny(filename, RG_PATH_SEPARATORS, &directory);

    return MakeDirectoryRec(directory);
}

void WaitForDelay(int64_t delay)
{
    RG_ASSERT_DEBUG(delay >= 0);
    RG_ASSERT_DEBUG(delay < 1000ll * INT32_MAX);

#ifdef _WIN32
    while (delay) {
        DWORD delay32 = (DWORD)std::min(delay, (int64_t)UINT32_MAX);
        delay -= delay32;

        Sleep(delay32);
    }
#else
    struct timespec ts;
    ts.tv_sec = (int)(delay / 1000);
    ts.tv_nsec = (int)((delay % 1000) * 1000000);

    struct timespec rem;
    while (nanosleep(&ts, &rem) < 0) {
        RG_ASSERT(errno == EINTR);
        ts = rem;
    }
#endif
}

#ifdef _WIN32
// We can't use a lambda in WaitForConsoleInterruption because it has to use
// the __stdcall calling convention, and MinGW (unlike MSVC) cannot convert
// lambdas to non-cdecl functions pointers.
static HANDLE console_ctrl_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);

static BOOL CALLBACK ConsoleCtrlHandler(DWORD)
{
    SetEvent(console_ctrl_event);
    return (BOOL)TRUE;
}

bool WaitForConsoleInterruption(int64_t delay)
{
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    if (delay >= 0) {
        while (delay) {
            DWORD delay32 = (DWORD)std::min(delay, (int64_t)UINT32_MAX);
            delay -= delay32;

            if (WaitForSingleObject(console_ctrl_event, delay32) == WAIT_OBJECT_0)
                return true;
        }

        return false;
    } else {
        return WaitForSingleObject(console_ctrl_event, INFINITE) == WAIT_OBJECT_0;
    }
}
#else
bool WaitForConsoleInterruption(int64_t delay)
{
    static volatile bool run = true;

    signal(SIGINT, [](int) { run = false; });
    signal(SIGTERM, [](int) { run = false; });

    if (delay >= 0) {
        struct timespec ts;
        ts.tv_sec = (int)(delay / 1000);
        ts.tv_nsec = (int)((delay % 1000) * 1000000);

        struct timespec rem;
        while (run && nanosleep(&ts, &rem) < 0) {
            RG_ASSERT(errno == EINTR);
            ts = rem;
        }
    } else {
        while (run) {
            pause();
        }
    }

    return !run;
}
#endif

// ------------------------------------------------------------------------
// Tasks
// ------------------------------------------------------------------------

struct Task {
    Async *async;
    std::function<bool()> func;
};

struct TaskQueue {
    std::mutex mutex;
    BlockQueue<Task> tasks;
};

struct AsyncWorker {
    bool running;
    Size queue_idx;
};

class AsyncPool {
    std::mutex mutex;
    std::condition_variable cv;

    Size async_count = 0;
    HeapArray<AsyncWorker> workers;

    HeapArray<TaskQueue> queues;
    Size next_queue_idx = 0;
    std::atomic_int pending_tasks {0};

public:
    AsyncPool(Size worker_count);

    void RegisterAsync();
    void UnregisterAsync();

    void AddTask(Async *async, const std::function<bool()> &func);

    void RunWorker(Size worker_idx);

    void RunTasks(Size queue_idx);
    void RunTask(Task *task);
};

// thread_local breaks down on MinGW when destructors are involved, work
// around this with heap allocation.
static RG_THREAD_LOCAL AsyncPool *g_async_pool = nullptr;
static RG_THREAD_LOCAL Size g_async_worker_idx;
static RG_THREAD_LOCAL bool g_task_running = false;
static int g_max_workers;

void Async::SetWorkerCount(int max_threads)
{
    RG_ASSERT_DEBUG(max_threads > 0);
    RG_ASSERT_DEBUG(!g_async_pool);

#ifdef __EMSCRIPTEN__
    LogError("Cannot use parallelism on Emscripten platform");
    g_max_workers = 1;
#else
    g_max_workers = max_threads;
#endif
}

int Async::GetWorkerCount()
{
    if (!g_max_workers) {
#ifdef __EMSCRIPTEN__
        g_max_workers = 1;
#else
        const char *env = getenv("LIBCC_THREADS");
        if (env) {
            char *end_ptr;
            long threads = strtol(env, &end_ptr, 10);
            if (end_ptr > env && !end_ptr[0] && threads > 0) {
                g_max_workers = (int)threads;
            } else {
                LogError("LIBCC_THREADS must be positive number (ignored)");
            }
        } else {
            g_max_workers = (int)std::thread::hardware_concurrency();
        }

        RG_ASSERT(g_max_workers > 0);
#endif
    }

    return g_max_workers;
}

Async::Async()
{
    if (!g_async_pool) {
        // NOTE: We're leaking one ThreadPool each time a non-worker thread uses Async for
        // the first time. That's only one leak in most cases, when the main thread is the
        // only non-worker thread using Async, but still. Something to keep in mind.
        g_async_pool = new AsyncPool(GetWorkerCount());
        g_async_worker_idx = 0;
    }

    g_async_pool->RegisterAsync();
}

Async::~Async()
{
    RG_ASSERT(!remaining_tasks);

    g_async_pool->UnregisterAsync();
}

void Async::AddTask(const std::function<bool()> &func)
{
    g_async_pool->AddTask(this, func);
}

bool Async::Sync()
{
    // TODO: This will spin too much if queues are empty but one or a few workers are
    // still processing long running tasks.
    while (remaining_tasks) {
        g_async_pool->RunTasks(g_async_worker_idx);
        std::this_thread::yield();
    }

    return success;
}

bool Async::IsTaskRunning()
{
    return g_task_running;
}

AsyncPool::AsyncPool(Size worker_count)
{
    workers.AppendDefault(worker_count);
    queues.AppendDefault(worker_count);
}

void AsyncPool::RegisterAsync()
{
    std::lock_guard<std::mutex> pool_lock(mutex);

    if (!async_count++) {
        for (Size i = 1; i < workers.len; i++) {
            AsyncWorker *worker = &workers[i];

            if (!worker->running) {
                std::thread(&AsyncPool::RunWorker, this, i).detach();
                worker->running = true;
            }
        }
    }
}

void AsyncPool::UnregisterAsync()
{
    std::lock_guard<std::mutex> pool_lock(g_async_pool->mutex);
    async_count--;
}

void AsyncPool::AddTask(Async *async, const std::function<bool()> &func)
{
    // If we are the main thread, and haven't reached Sync() yet, try to assign
    // task to a "random" queue first.
    if (!g_task_running && workers.len > 1) {
        for (;;) {
            TaskQueue *queue = &queues[next_queue_idx];

            if (--next_queue_idx < 0) {
                next_queue_idx = workers.len - 1;
            }

            std::unique_lock<std::mutex> lock_queue(queue->mutex, std::try_to_lock);
            if (lock_queue.owns_lock()) {
                queue->tasks.Append({async, func});
                break;
            }
        }
    } else {
        TaskQueue *queue = &queues[g_async_worker_idx];

        std::lock_guard<std::mutex> lock_queue(queue->mutex);
        queue->tasks.Append({async, func});
    }

    async->remaining_tasks++;
    if (!g_async_pool->pending_tasks++) {
        std::lock_guard<std::mutex> lock_pool(g_async_pool->mutex);
        g_async_pool->cv.notify_all();
    }
}

void AsyncPool::RunWorker(Size worker_idx)
{
    g_async_pool = this;
    g_async_worker_idx = worker_idx;

    AsyncWorker *worker = &workers[worker_idx];

    for (;;) {
        RunTasks(worker_idx);

        std::unique_lock<std::mutex> lock_pool(mutex);
        while (!pending_tasks) {
#if RG_THREAD_MAX_IDLE_TIME >= 0
            cv.wait_for(lock_pool,
                        std::chrono::duration<int, std::milli>(RG_THREAD_MAX_IDLE_TIME));
            if (!async_count) {
                worker->running = false;
                return;
            }
#else
            cv.wait(lock_pool);
#endif
        }
    }
}

void AsyncPool::RunTasks(Size queue_idx)
{
    for (Size i = 0; i < 48; i++) {
        TaskQueue *queue = &queues[queue_idx];

        std::unique_lock<std::mutex> lock_queue(queue->mutex, std::try_to_lock);
        if (lock_queue.owns_lock() && queue->tasks.len) {
            Task task = std::move(queue->tasks[0]);
            queue->tasks.RemoveFirst();

            lock_queue.unlock();
            RunTask(&task);

            i = -1;
        }

        queue_idx++;
        if (queue_idx >= queues.len) {
            queue_idx = 0;
        }
    }
}

void AsyncPool::RunTask(Task *task)
{
    Async *async = task->async;

    g_task_running = true;
    RG_DEFER { g_task_running = false; };

    pending_tasks--;
    if (async->success && !task->func()) {
        async->success = false;
    }
    async->remaining_tasks--;
}

// ------------------------------------------------------------------------
// Streams
// ------------------------------------------------------------------------

StreamReader stdin_st(stdin, "<stdin>");
StreamWriter stdout_st(stdout, "<stdout>");
StreamWriter stderr_st(stderr, "<stderr>");

#ifdef MZ_VERSION
struct MinizInflateContext {
    tinfl_decompressor inflator;
    bool done;

    uint8_t in[256 * 1024];
    uint8_t *in_ptr;
    Size in_len;

    uint8_t out[256 * 1024];
    uint8_t *out_ptr;
    Size out_len;

    // Gzip support
    bool header_done;
    uint32_t crc32;
    Size uncompressed_size;
};
RG_STATIC_ASSERT(RG_SIZE(MinizInflateContext::out) >= TINFL_LZ_DICT_SIZE);
#endif

bool StreamReader::Open(Span<const uint8_t> buf, const char *filename,
                        CompressionType compression_type)
{
    RG_ASSERT(!this->filename);

    RG_DEFER_N(error_guard) {
        ReleaseResources();
        error = true;
    };

    this->filename = filename ? filename : "<memory>";

    source.type = SourceType::Memory;
    source.u.memory.buf = buf;
    source.u.memory.pos = 0;

    if (!InitDecompressor(compression_type))
        return false;

    error_guard.Disable();
    return true;
}

bool StreamReader::Open(FILE *fp, const char *filename, CompressionType compression_type)
{
    RG_ASSERT(!this->filename);

    RG_DEFER_N(error_guard) {
        ReleaseResources();
        error = true;
    };

    RG_ASSERT_DEBUG(fp);
    RG_ASSERT_DEBUG(filename);
    this->filename = filename;

    source.type = SourceType::File;
    source.u.fp = fp;

    if (!InitDecompressor(compression_type))
        return false;

    error_guard.Disable();
    return true;
}

bool StreamReader::Open(const char *filename, CompressionType compression_type)
{
    RG_ASSERT(!this->filename);

    RG_DEFER_N(error_guard) {
        ReleaseResources();
        error = true;
    };

    RG_ASSERT_DEBUG(filename);
    this->filename = filename;

    source.type = SourceType::File;
    source.u.fp = OpenFile(filename, OpenFileMode::Read);
    if (!source.u.fp)
        return false;
    source.owned = true;

    if (!InitDecompressor(compression_type))
        return false;

    error_guard.Disable();
    return true;
}

void StreamReader::Close()
{
    ReleaseResources();

    filename = nullptr;
    source.eof = false;
    raw_len = -1;
    read = 0;
    raw_read = 0;
    error = false;
    eof = false;
}

Size StreamReader::Read(Size max_len, void *out_buf)
{
    if (RG_UNLIKELY(error))
        return -1;

    Size read_len = 0;
    switch (compression.type) {
        case CompressionType::None: {
            read_len = ReadRaw(max_len, out_buf);
            eof = source.eof;
        } break;

        case CompressionType::Gzip:
        case CompressionType::Zlib: {
            read_len = Deflate(max_len, out_buf);
        } break;
    }

    if (RG_LIKELY(read_len >= 0)) {
        read += read_len;
    }
    return read_len;
}

Size StreamReader::ReadAll(Size max_len, HeapArray<uint8_t> *out_buf)
{
    if (error)
        return -1;

    if (compression.type == CompressionType::None && ComputeStreamLen() >= 0) {
        if (raw_len > max_len) {
            LogError("File '%1' is too large (limit = %2)", filename, FmtDiskSize(max_len));
            return -1;
        }

        // Add one trailing byte to avoid reallocation for users who append a NUL character
        out_buf->Grow(raw_len + 1);
        Size read_len = Read(raw_len, out_buf->end());
        if (read_len < 0)
            return -1;
        out_buf->len += read_len;

        return read_len;
    } else {
        RG_DEFER_NC(buf_guard, buf_len = out_buf->len) { out_buf->RemoveFrom(buf_len); };

        Size read_len, total_len = 0;
        out_buf->Grow(Megabytes(1));
        while ((read_len = Read(out_buf->Available(), out_buf->end())) > 0) {
            total_len += read_len;
            if (total_len > max_len) {
                LogError("File '%1' is too large (limit = %2)", filename, FmtDiskSize(max_len));
                return -1;
            }
            out_buf->len += read_len;
            out_buf->Grow(Megabytes(1));
        }
        if (error)
            return -1;

        buf_guard.Disable();
        return total_len;
    }
}

Size StreamReader::ComputeStreamLen()
{
    if (raw_read || raw_len >= 0)
        return raw_len;

    switch (source.type) {
        case SourceType::File: {
#ifdef _WIN32
            int64_t pos = _ftelli64(source.u.fp);
            RG_DEFER { _fseeki64(source.u.fp, pos, SEEK_SET); };
            if (_fseeki64(source.u.fp, 0, SEEK_END) < 0)
                return -1;
            int64_t len = _ftelli64(source.u.fp);
#else
            off64_t pos = ftello64(source.u.fp);
            RG_DEFER { fseeko64(source.u.fp, pos, SEEK_SET); };
            if (fseeko64(source.u.fp, 0, SEEK_END) < 0)
                return -1;
            off64_t len = ftello64(source.u.fp);
#endif
            if (len > RG_SIZE_MAX) {
                static bool warned = false;
                if (!warned) {
                    LogError("Files bigger than %1 are not well supported", FmtMemSize(RG_SIZE_MAX));
                    warned = true;
                }
                len = RG_SIZE_MAX;
            }
            raw_len = (Size)len;
        } break;

        case SourceType::Memory: {
            raw_len = source.u.memory.buf.len;
        } break;
    }

    return raw_len;
}

bool StreamReader::InitDecompressor(CompressionType type)
{
    switch (type) {
        case CompressionType::None: {} break;

        case CompressionType::Gzip:
        case CompressionType::Zlib: {
#ifdef MZ_VERSION
            compression.u.miniz =
                (MinizInflateContext *)Allocator::Allocate(nullptr, RG_SIZE(MinizInflateContext),
                                                           (int)Allocator::Flag::Zero);
            tinfl_init(&compression.u.miniz->inflator);
            compression.u.miniz->crc32 = MZ_CRC32_INIT;
#else
            LogError("Deflate compression not available for '%1'", filename);
            error = true;
            return false;
#endif
        } break;
    }
    compression.type = type;

    return true;
}

void StreamReader::ReleaseResources()
{
    switch (compression.type) {
        case CompressionType::None: {} break;

        case CompressionType::Gzip:
        case CompressionType::Zlib: {
#ifdef MZ_VERSION
            Allocator::Release(nullptr, compression.u.miniz, RG_SIZE(*compression.u.miniz));
#endif
        } break;
    }
    compression.type = CompressionType::None;

    if (source.owned) {
        switch (source.type) {
            case SourceType::File: {
                if (source.u.fp) {
                    fclose(source.u.fp);
                }
            } break;

            case SourceType::Memory: {} break;
        }
        source.owned = false;
    }
}

Size StreamReader::Deflate(Size max_len, void *out_buf)
{
#ifdef MZ_VERSION
    MinizInflateContext *ctx = compression.u.miniz;

    // Gzip header is not directly supported by miniz. Currently this
    // will fail if the header is longer than 4096 bytes, which is
    // probably quite rare.
    if (compression.type == CompressionType::Gzip && !ctx->header_done) {
        uint8_t header[4096];
        Size header_len;

        header_len = ReadRaw(RG_SIZE(header), header);
        if (header_len < 0) {
            return -1;
        } else if (header_len < 10 || header[0] != 0x1F || header[1] != 0x8B) {
            LogError("File '%1' does not look like a Gzip stream", filename);
            error = true;
            return -1;
        }

        Size header_offset = 10;
        if (header[3] & 0x4) { // FEXTRA
            if (header_len - header_offset < 2)
                goto truncated_error;
            uint16_t extra_len = (uint16_t)((header[11] << 8) | header[10]);
            if (extra_len > header_len - header_offset)
                goto truncated_error;
            header_offset += extra_len;
        }
        if (header[3] & 0x8) { // FNAME
            uint8_t *end_ptr = (uint8_t *)memchr(header + header_offset, '\0',
                                                 (size_t)(header_len - header_offset));
            if (!end_ptr)
                goto truncated_error;
            header_offset = end_ptr - header + 1;
        }
        if (header[3] & 0x10) { // FCOMMENT
            uint8_t *end_ptr = (uint8_t *)memchr(header + header_offset, '\0',
                                                 (size_t)(header_len - header_offset));
            if (!end_ptr)
                goto truncated_error;
            header_offset = end_ptr - header + 1;
        }
        if (header[3] & 0x2) { // FHCRC
            if (header_len - header_offset < 2)
                goto truncated_error;
            uint16_t crc16 = (uint16_t)(header[1] << 8 | header[0]);
            // TODO: Test this actually works
            if ((mz_crc32(MZ_CRC32_INIT, header, (size_t)header_offset) & 0xFFFF) == crc16) {
                LogError("Failed header CRC16 check in '%s'", filename);
                error = true;
                return -1;
            }
            header_offset += 2;
        }

        // Put back remaining data in the buffer
        memcpy(ctx->in, header + header_offset, (size_t)(header_len - header_offset));
        ctx->in_ptr = ctx->in;
        ctx->in_len = header_len - header_offset;

        ctx->header_done = true;
    }

    // Inflate (with miniz)
    {
        Size read_len = 0;
        for (;;) {
            if (max_len < ctx->out_len) {
                memcpy(out_buf, ctx->out_ptr, (size_t)max_len);
                read_len += max_len;
                ctx->out_ptr += max_len;
                ctx->out_len -= max_len;

                return read_len;
            } else {
                memcpy(out_buf, ctx->out_ptr, (size_t)ctx->out_len);
                read_len += ctx->out_len;
                out_buf = (uint8_t *)out_buf + ctx->out_len;
                max_len -= ctx->out_len;
                ctx->out_ptr = ctx->out;
                ctx->out_len = 0;

                if (ctx->done) {
                    eof = true;
                    return read_len;
                }
            }

            while (ctx->out_len < RG_SIZE(ctx->out)) {
                if (!ctx->in_len) {
                    ctx->in_ptr = ctx->in;
                    ctx->in_len = ReadRaw(RG_SIZE(ctx->in), ctx->in);
                    if (ctx->in_len < 0)
                        return read_len ? read_len : ctx->in_len;
                }

                size_t in_arg = (size_t)ctx->in_len;
                size_t out_arg = (size_t)(RG_SIZE(ctx->out) - ctx->out_len);
                uint32_t flags = (uint32_t)
                    ((compression.type == CompressionType::Zlib ? TINFL_FLAG_PARSE_ZLIB_HEADER : 0) |
                     (source.eof ? 0 : TINFL_FLAG_HAS_MORE_INPUT));

                tinfl_status status = tinfl_decompress(&ctx->inflator, ctx->in_ptr, &in_arg,
                                                       ctx->out, ctx->out + ctx->out_len,
                                                       &out_arg, flags);

                if (compression.type == CompressionType::Gzip) {
                    ctx->crc32 = (uint32_t)mz_crc32(ctx->crc32, ctx->out + ctx->out_len, out_arg);
                    ctx->uncompressed_size += (Size)out_arg;
                }

                ctx->in_ptr += (Size)in_arg;
                ctx->in_len -= (Size)in_arg;
                ctx->out_len += (Size)out_arg;

                if (status == TINFL_STATUS_DONE) {
                    // Gzip footer (CRC and size check)
                    if (compression.type == CompressionType::Gzip) {
                        uint32_t footer[2];
                        RG_STATIC_ASSERT(RG_SIZE(footer) == 8);

                        if (ctx->in_len < RG_SIZE(footer)) {
                            memcpy(footer, ctx->in_ptr, (size_t)ctx->in_len);

                            Size missing_len = RG_SIZE(footer) - ctx->in_len;
                            if (ReadRaw(missing_len, footer + ctx->in_len) < missing_len) {
                                if (error) {
                                    return -1;
                                } else {
                                    goto truncated_error;
                                }
                            }
                        } else {
                            memcpy(footer, ctx->in_ptr, RG_SIZE(footer));
                        }
                        footer[0] = LittleEndian(footer[0]);
                        footer[1] = LittleEndian(footer[1]);

                        if (ctx->crc32 != footer[0] ||
                                (uint32_t)ctx->uncompressed_size != footer[1]) {
                            LogError("Failed CRC32 or size check in GZip stream '%1'", filename);
                            error = true;
                            return -1;
                        }
                    }

                    ctx->done = true;
                    break;
                } else if (status < TINFL_STATUS_DONE) {
                    LogError("Failed to decompress '%1' (Deflate)", filename);
                    error = true;
                    return -1;
                }
            }
        }
    }

truncated_error:
    LogError("Truncated Gzip header in '%1'", filename);
    error = true;
    return -1;
#else
    RG_ASSERT_DEBUG(false);
#endif
}

Size StreamReader::ReadRaw(Size max_len, void *out_buf)
{
    ComputeStreamLen();

    Size read_len = 0;
    switch (source.type) {
        case SourceType::File: {
restart:
            read_len = (Size)fread(out_buf, 1, (size_t)max_len, source.u.fp);
            if (ferror(source.u.fp)) {
                if (errno == EINTR)
                    goto restart;

                LogError("Error while reading file '%1': %2", filename, strerror(errno));
                error = true;
                return -1;
            }
            source.eof |= (bool)feof(source.u.fp);
        } break;

        case SourceType::Memory: {
            read_len = source.u.memory.buf.len - source.u.memory.pos;
            if (read_len > max_len) {
                read_len = max_len;
            }
            memcpy(out_buf, source.u.memory.buf.ptr + source.u.memory.pos, (size_t)read_len);
            source.u.memory.pos += read_len;
            source.eof |= (source.u.memory.pos >= source.u.memory.buf.len);
        } break;
    }

    raw_read += read_len;
    return read_len;
}

// TODO: Maximum line length
bool LineReader::Next(Span<char> *out_line)
{
    if (RG_UNLIKELY(error || eof))
        return false;

    for (;;) {
        if (!view.len) {
            buf.Grow(RG_LINE_READER_STEP_SIZE + 1);

            Size read_len = st->Read(RG_LINE_READER_STEP_SIZE, buf.end());
            if (read_len < 0) {
                error = true;
                return false;
            }
            buf.len += read_len;
            eof = !read_len;

            view = buf;
        }

        line = SplitStrLine(view, &view);
        if (view.len || eof) {
            line.ptr[line.len] = 0;
            line_number++;
            *out_line = line;
            return true;
        }

        buf.len = view.ptr - line.ptr;
        memmove(buf.ptr, line.ptr, (size_t)buf.len);
    }
}

void LineReader::PushLogHandler()
{
    RG::PushLogHandler([=](LogLevel level, const char *ctx, const char *msg) {
        StartConsoleLog(level);
        Print(stderr, "%1%2(%3): %4", ctx, st->filename, line_number, msg);
        EndConsoleLog();
    });
}

#ifdef MZ_VERSION
struct MinizDeflateContext {
    tdefl_compressor deflator;

    // Gzip support
    uint32_t crc32;
    Size uncompressed_size;
};
#endif

bool StreamWriter::Open(HeapArray<uint8_t> *mem, const char *filename,
                        CompressionType compression_type)
{
    RG_ASSERT(!this->filename);

    RG_DEFER_N(error_guard) {
        ReleaseResources();
        error = true;
    };

    this->filename = filename ? filename : "<memory>";

    dest.type = DestinationType::Memory;
    dest.u.mem = mem;

    if (!InitCompressor(compression_type))
        return false;

    open = true;
    error_guard.Disable();
    return true;
}

bool StreamWriter::Open(FILE *fp, const char *filename, CompressionType compression_type)
{
    RG_ASSERT(!this->filename);

    RG_DEFER_N(error_guard) {
        ReleaseResources();
        error = true;
    };

    RG_ASSERT_DEBUG(fp);
    RG_ASSERT_DEBUG(filename);
    this->filename = filename;

    dest.type = DestinationType::File;
    dest.u.fp = fp;

    if (!InitCompressor(compression_type))
        return false;

    open = true;
    error_guard.Disable();
    return true;
}

bool StreamWriter::Open(const char *filename, CompressionType compression_type)
{
    RG_ASSERT(!this->filename);

    RG_DEFER_N(error_guard) {
        ReleaseResources();
        error = true;
    };

    RG_ASSERT_DEBUG(filename);
    this->filename = filename;

    dest.type = DestinationType::File;
    dest.u.fp = OpenFile(filename, OpenFileMode::Write);
    if (!dest.u.fp)
        return false;
    dest.owned = true;

    if (!InitCompressor(compression_type))
        return false;

    open = true;
    error_guard.Disable();
    return true;
}

bool StreamWriter::Close()
{
    bool success = !error;

    if (open && !error) {
        switch (compression.type) {
            case CompressionType::None: {} break;

            case CompressionType::Gzip:
            case CompressionType::Zlib: {
#ifdef MZ_VERSION
                MinizDeflateContext *ctx = compression.u.miniz;

                tdefl_status status = tdefl_compress_buffer(&ctx->deflator, nullptr, 0,
                                                            TDEFL_FINISH);
                if (status != TDEFL_STATUS_DONE) {
                    if (status != TDEFL_STATUS_PUT_BUF_FAILED) {
                        LogError("Failed to end Deflate stream for '%1", filename);
                    }
                    success = false;
                }

                if (compression.type == CompressionType::Gzip) {
                    uint32_t gzip_footer[] = {
                        LittleEndian(ctx->crc32),
                        LittleEndian((uint32_t)ctx->uncompressed_size)
                    };

                    success &= WriteRaw(MakeSpan((uint8_t *)gzip_footer, RG_SIZE(gzip_footer)));
                }
#endif
            } break;
        }

        switch (dest.type) {
            case DestinationType::File: {
#ifdef _WIN32
                if (fflush(dest.u.fp) != 0) {
#else
                if ((fflush(dest.u.fp) != 0 || fsync(fileno(dest.u.fp)) < 0) && errno != EINVAL) {
#endif
                    LogError("Failed to finalize writing to '%1': %2", filename, strerror(errno));
                    success = false;
                }
            } break;

            case DestinationType::Memory: {} break;
        }
    }

    ReleaseResources();

    filename = nullptr;
    open = false;
    error = false;

    return success;
}

bool StreamWriter::Write(Span<const uint8_t> buf)
{
    if (RG_UNLIKELY(error))
        return false;

    switch (compression.type) {
        case CompressionType::None: {
            return WriteRaw(buf);
        } break;

        case CompressionType::Gzip:
        case CompressionType::Zlib: {
#ifdef MZ_VERSION
            if (compression.type == CompressionType::Gzip) {
                compression.u.miniz->crc32 = (uint32_t)mz_crc32(compression.u.miniz->crc32,
                                                                buf.ptr, (size_t)buf.len);
                compression.u.miniz->uncompressed_size += buf.len;
            }

            tdefl_status status = tdefl_compress_buffer(&compression.u.miniz->deflator,
                                                        buf.ptr, (size_t)buf.len, TDEFL_NO_FLUSH);
            if (status < TDEFL_STATUS_OKAY) {
                if (status != TDEFL_STATUS_PUT_BUF_FAILED) {
                    LogError("Failed to deflate stream to '%1'", filename);
                }
                error = true;
                return false;
            }

            return true;
#endif
        } break;
    }
    RG_ASSERT_DEBUG(false);
}

bool StreamWriter::InitCompressor(CompressionType type)
{
    switch (type) {
        case CompressionType::None: {} break;

        case CompressionType::Gzip:
        case CompressionType::Zlib: {
#ifdef MZ_VERSION
            compression.u.miniz =
                (MinizDeflateContext *)Allocator::Allocate(nullptr, RG_SIZE(MinizDeflateContext),
                                                           (int)Allocator::Flag::Zero);
            compression.u.miniz->crc32 = MZ_CRC32_INIT;

            int flags = 32 | (type == CompressionType::Zlib ? TDEFL_WRITE_ZLIB_HEADER : 0);

            tdefl_status status = tdefl_init(&compression.u.miniz->deflator,
                                             [](const void *buf, int len, void *udata) {
                StreamWriter *st = (StreamWriter *)udata;
                return (int)st->WriteRaw(MakeSpan((uint8_t *)buf, len));
            }, this, flags);
            if (status != TDEFL_STATUS_OKAY) {
                LogError("Failed to initialize Deflate compression for '%1'", filename);
                error = true;
                return false;
            }

            if (type == CompressionType::Gzip) {
                static uint8_t gzip_header[] = {
                    0x1F, 0x8B, // Fixed bytes
                    8, // Deflate
                    0, // FLG
                    0, 0, 0, 0, // MTIME
                    0, // XFL
                    0 // OS
                };

                if (!WriteRaw(gzip_header))
                    return false;
            }
#else
            LogError("Deflate compression not available for '%1'", filename);
            error = true;
            return false;
#endif
        } break;
    }
    compression.type = type;

    return true;
}

void StreamWriter::ReleaseResources()
{
    switch (compression.type) {
        case CompressionType::None: {} break;

        case CompressionType::Gzip:
        case CompressionType::Zlib: {
#ifdef MZ_VERSION
            Allocator::Release(nullptr, compression.u.miniz, RG_SIZE(*compression.u.miniz));
#endif
        } break;
    }
    compression.type = CompressionType::None;

    if (dest.owned) {
        switch (dest.type) {
            case DestinationType::File: {
                if (dest.u.fp) {
                    fclose(dest.u.fp);
                }
            } break;

            case DestinationType::Memory: {} break;
        }
        dest.owned = false;
    }
}

bool StreamWriter::WriteRaw(Span<const uint8_t> buf)
{
    switch (dest.type) {
        case DestinationType::File: {
            while (buf.len) {
                size_t write_len = fwrite(buf.ptr, 1, (size_t)buf.len, dest.u.fp);

                if (ferror(dest.u.fp)) {
                    if (errno == EINTR) {
                        clearerr(dest.u.fp);
                    } else {
                        LogError("Failed to write to '%1': %2", filename, strerror(errno));
                        error = true;
                        return false;
                    }
                }

                buf.ptr += write_len;
                buf.len -= write_len;
            }

            return true;
        } break;

        case DestinationType::Memory: {
            // dest.u.mem->Append(buf) would work but it's probably slower
            dest.u.mem->Grow(buf.len);
            memcpy(dest.u.mem->ptr + dest.u.mem->len, buf.ptr, (size_t)buf.len);
            dest.u.mem->len += buf.len;

            return true;
        } break;
    }
    RG_ASSERT_DEBUG(false);
}

bool SpliceStream(StreamReader *reader, Size max_len, StreamWriter *writer)
{
    if (reader->error)
        return false;

    Size len = 0;
    while (!reader->eof) {
        char buf[16 * 1024];
        Size read_len = reader->Read(RG_SIZE(buf), buf);
        if (read_len < 0)
            return false;

        len += read_len;
        if (len > max_len) {
            LogError("File '%1' is too large (limit = %2)", reader->filename, FmtDiskSize(max_len));
            return false;
        }

        if (!writer->Write(buf, read_len))
            return false;
    }

    return true;
}

// ------------------------------------------------------------------------
// INI
// ------------------------------------------------------------------------

static inline bool IsAsciiIdChar(char c)
{
    return IsAsciiAlphaOrDigit(c) || c == '_' || c == '-' || c == '.' || c == ' ';
}

IniParser::LineType IniParser::FindNextLine(IniProperty *out_prop)
{
    if (RG_UNLIKELY(error))
        return LineType::Exit;

    RG_DEFER_N(error_guard) { error = true; };

    Span<char> line;
    while (reader.Next(&line)) {
        line = TrimStr(line);

        if (!line.len || line[0] == ';' || line[0] == '#') {
            // Ignore this line (empty or comment)
        } else if (line[0] == '[') {
            if (line.len < 2 || line[line.len - 1] != ']') {
                LogError("%1(%2): Malformed section line", reader.st->filename, reader.line_number);
                return LineType::Exit;
            }

            Span<const char> section = TrimStr(line.Take(1, line.len - 2));
            if (!section.len) {
                LogError("%1(%2): Empty section name", reader.st->filename, reader.line_number);
                return LineType::Exit;
            }
            if (!std::all_of(section.begin(), section.end(), IsAsciiIdChar)) {
                LogError("%1(%2): Section names can only contain alphanumeric characters, '_', '-', '.' or ' '",
                         reader.st->filename, reader.line_number);
                return LineType::Exit;
            }

            current_section.RemoveFrom(0);
            current_section.Append(section);

            error_guard.Disable();
            return LineType::Section;
        } else {
            Span<char> value;
            Span<const char> key = TrimStr(SplitStr(line, '=', &value));
            if (!key.len || key.end() == line.end()) {
                LogError("%1(%2): Malformed key=value", reader.st->filename, reader.line_number);
                return LineType::Exit;
            }
            if (!std::all_of(key.begin(), key.end(), IsAsciiIdChar)) {
                LogError("%1(%2): Key names can only contain alphanumeric characters, '_', '-' or '.'",
                         reader.st->filename, reader.line_number);
                return LineType::Exit;
            }
            value = TrimStr(value);
            *value.end() = 0;

            out_prop->section = current_section;
            out_prop->key = key;
            out_prop->value = value;

            error_guard.Disable();
            return LineType::KeyValue;
        }
    }
    if (reader.error)
        return LineType::Exit;

    eof = true;

    error_guard.Disable();
    return LineType::Exit;
}

bool IniParser::Next(IniProperty *out_prop)
{
    LineType type;
    while ((type = FindNextLine(out_prop)) == LineType::Section);
    return type == LineType::KeyValue;
}

bool IniParser::NextInSection(IniProperty *out_prop)
{
    LineType type = FindNextLine(out_prop);
    return type == LineType::KeyValue;
}

// ------------------------------------------------------------------------
// Options
// ------------------------------------------------------------------------

static inline bool IsOption(const char *arg)
{
    return arg[0] == '-' && arg[1];
}

static inline bool IsLongOption(const char *arg)
{
    return arg[0] == '-' && arg[1] == '-' && arg[2];
}

static inline bool IsDashDash(const char *arg)
{
    return arg[0] == '-' && arg[1] == '-' && !arg[2];
}

static void ReverseArgs(const char **args, Size start, Size end)
{
    for (Size i = 0; i < (end - start) / 2; i++) {
        const char *tmp = args[start + i];
        args[start + i] = args[end - i - 1];
        args[end - i - 1] = tmp;
    }
}

static void RotateArgs(const char **args, Size start, Size mid, Size end)
{
    if (start == mid || mid == end)
        return;

    ReverseArgs(args, start, mid);
    ReverseArgs(args, mid, end);
    ReverseArgs(args, start, end);
}

const char *OptionParser::Next()
{
    current_option = nullptr;
    current_value = nullptr;

    // Support aggregate short options, such as '-fbar'. Note that this can also be
    // parsed as the short option '-f' with value 'bar', if the user calls
    // ConsumeOptionValue() after getting '-f'.
    if (smallopt_offset) {
        const char *opt = args[pos];
        smallopt_offset++;
        if (opt[smallopt_offset]) {
            buf[1] = opt[smallopt_offset];
            current_option = buf;
            return current_option;
        } else {
            smallopt_offset = 0;
            pos++;
        }
    }

    // Skip non-options, do the permutation once we reach an option or the last argument
    Size next_index = pos;
    while (next_index < limit && !IsOption(args[next_index])) {
        next_index++;
    }
    if (flags & (int)Flag::SkipNonOptions) {
        pos = next_index;
    } else {
        RotateArgs(args.ptr, pos, next_index, args.len);
        limit -= (next_index - pos);
    }
    if (pos >= limit)
        return nullptr;

    const char *opt = args[pos];

    if (IsLongOption(opt)) {
        const char *needle = strchr(opt, '=');
        if (needle) {
            // We can reorder args, but we don't want to change strings. So copy the
            // option up to '=' in our buffer. And store the part after '=' as the
            // current value.
            Size len = needle - opt;
            if (len > RG_SIZE(buf) - 1) {
                len = RG_SIZE(buf) - 1;
            }
            memcpy(buf, opt, (size_t)len);
            buf[len] = 0;
            current_option = buf;
            current_value = needle + 1;
        } else {
            current_option = opt;
        }
        pos++;
    } else if (IsDashDash(opt)) {
        // We may have previously moved non-options to the end of args. For example,
        // at this point 'a b c -- d e' is reordered to '-- d e a b c'. Fix it.
        RotateArgs(args.ptr, pos + 1, limit, args.len);
        limit = pos;
        pos++;
    } else if (opt[2]) {
        // We either have aggregated short options or one short option with a value,
        // depending on whether or not the user calls ConsumeOptionValue().
        buf[0] = '-';
        buf[1] = opt[1];
        buf[2] = 0;
        current_option = buf;
        smallopt_offset = 1;

        // The main point of SkipNonOptions is to be able to parse arguments in
        // multiple passes. This does not work well with ambiguous short options
        // (such as -oOption, which can be interpeted as multiple one-char options
        // or one -o option with a value), so force the value interpretation.
        if (flags & (int)Flag::SkipNonOptions) {
            ConsumeValue();
        }
    } else {
        current_option = opt;
        pos++;
    }

    return current_option;
}

bool OptionParser::Test(const char *test1, const char *test2, OptionType type)
{
    RG_ASSERT_DEBUG(test1 && IsOption(test1));
    RG_ASSERT_DEBUG(!test2 || IsOption(test2));

    if (TestStr(test1, current_option) || (test2 && TestStr(test2, current_option))) {
        switch (type) {
            case OptionType::NoValue: {
                if (current_value) {
                    LogError("Option '%1' does not support values", current_option);
                    return false;
                }
            } break;
            case OptionType::Value: {
                if (!ConsumeValue()) {
                    LogError("Option '%1' requires a value", current_option);
                    return false;
                }
            } break;
            case OptionType::OptionalValue: {
                ConsumeValue();
            } break;
        }

        return true;
    } else {
        return false;
    }
}

const char *OptionParser::ConsumeValue()
{
    if (current_value)
        return current_value;

    // Support '-fbar' where bar is the value, but only for the first short option
    // if it's an aggregate.
    if (smallopt_offset == 1 && args[pos][2]) {
        smallopt_offset = 0;
        current_value = args[pos] + 2;
        pos++;
    // Support '-f bar' and '--foo bar', see ConsumeOption() for '--foo=bar'
    } else if (!smallopt_offset && pos < limit && !IsOption(args[pos])) {
        current_value = args[pos];
        pos++;
    }

    return current_value;
}

const char *OptionParser::ConsumeNonOption()
{
    if (pos == args.len)
        return nullptr;
    // Beyond limit there are only non-options, the limit is moved when we move non-options
    // to the end or upon encouteering a double dash '--'.
    if (pos < limit && IsOption(args[pos]))
        return nullptr;

    return args[pos++];
}

void OptionParser::ConsumeNonOptions(HeapArray<const char *> *non_options)
{
    const char *non_option;
    while ((non_option = ConsumeNonOption())) {
        non_options->Append(non_option);
    }
}

}
