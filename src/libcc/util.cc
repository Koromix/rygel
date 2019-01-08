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
    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
        #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
    #endif
#else
    #include <dirent.h>
    #include <fcntl.h>
    #include <fnmatch.h>
    #include <sys/stat.h>
    #include <sys/types.h>
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
    #include "../../lib/miniz/miniz.h"
#endif

#include "util.hh"

// ------------------------------------------------------------------------
// Assert
// ------------------------------------------------------------------------

extern "C" void NORETURN AssertFail(const char *cond)
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
        if (UNLIKELY(!ptr)) {
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
            if (UNLIKELY(new_size && !new_ptr)) {
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
    static Allocator *default_allocator = new DEFAULT_ALLOCATOR;
    return default_allocator;
}

void *Allocator::Allocate(Allocator *alloc, Size size, unsigned int flags)
{
    DebugAssert(size >= 0);

    if (!alloc) {
        alloc = GetDefaultAllocator();
    }
    return alloc->Allocate(size, flags);
}

void Allocator::Resize(Allocator *alloc, void **ptr, Size old_size, Size new_size,
                       unsigned int flags)
{
    DebugAssert(new_size >= 0);

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
    Bucket *bucket = (Bucket *)Allocator::Allocate(allocator, SIZE(*bucket) + size, flags);

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
        Allocator::Resize(allocator, (void **)&bucket, SIZE(*bucket) + old_size,
                          SIZE(*bucket) + new_size, flags);

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
    DebugAssert(size >= 0);

    LinkedAllocator *alloc = GetAllocator();

    // Keep alignement requirements
    Size aligned_size = AlignSizeValue(size);

    if (AllocateSeparately(aligned_size)) {
        uint8_t *ptr = (uint8_t *)Allocator::Allocate(alloc, size, flags);
        return ptr;
    } else {
        if (!current_bucket || (current_bucket->used + aligned_size) > block_size) {
            current_bucket = (Bucket *)Allocator::Allocate(alloc, SIZE(Bucket) + block_size,
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
    DebugAssert(old_size >= 0);
    DebugAssert(new_size >= 0);

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
    DebugAssert(size >= 0);

    if (ptr) {
        LinkedAllocator *alloc = GetAllocator();

        Size aligned_size = AlignSizeValue(size);

        if (ptr == last_alloc) {
            current_bucket->used -= aligned_size;
            if (!current_bucket->used) {
                Allocator::Release(alloc, current_bucket, SIZE(Bucket) + block_size);
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
                if (UNLIKELY(++lengths[i] > 5))
                    goto malformed;
            } else if (!lengths[i] && c == '-' && mult == 1 && i != 1) {
                mult = -1;
            } else if (UNLIKELY(i == 2 && !(flags & (int)ParseFlag::End) && c != '/' && c != '-')) {
                break;
            } else if (UNLIKELY(!lengths[i] || (c != '/' && c != '-'))) {
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

    if (UNLIKELY((unsigned int)lengths[1] > 2))
        goto malformed;
    if (UNLIKELY((lengths[0] > 2) == (lengths[2] > 2))) {
        if (flags & (int)ParseFlag::Log) {
            LogError("Ambiguous date string '%1'", date_str);
        }
        return {};
    } else if (lengths[2] > 2) {
        std::swap(parts[0], parts[2]);
    }
    if (UNLIKELY(parts[0] < -INT16_MAX || parts[0] > INT16_MAX || (unsigned int)parts[2] > 99))
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
    DebugAssert(days >= 0);

    Date date;
    {
        // Algorithm from Richards, copied from Wikipedia:
        // https://en.wikipedia.org/w/index.php?title=Julian_day&oldid=792497863
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
    DebugAssert(IsValid());

    int julian_days;
    {
        // Straight from the Web:
        // http://www.cs.utsa.edu/~cs1063/projects/Spring2011/Project1/jdn-explanation.html
        bool adjust = st.month < 3;
        int year = st.year + 4800 - adjust;
        int month = st.month + 12 * adjust - 3;
        julian_days = st.day + (153 * month + 2) / 5 + 365 * year - 32045 +
                      year / 4 - year / 100 + year / 400;
    }

    return julian_days;
}

Date &Date::operator++()
{
    DebugAssert(IsValid());

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
    DebugAssert(IsValid());

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

uint64_t g_start_time = GetMonotonicTime();

uint64_t GetMonotonicTime()
{
#if defined(_WIN32)
    return GetTickCount64();
#elif defined(__EMSCRIPTEN__)
    return (uint64_t)emscripten_get_now();
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        LogError("clock_gettime() failed: %1", strerror(errno));
        return 0;
    }

    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
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
    DebugAssert(buf_len >= 0 && buf_len < 256);

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
                DebugAssert(!arg.value.date.value || arg.value.date.IsValid());

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
                        case FmtArg::Type::Buffer: { Assert(false); } break;
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
                                default: { Assert(false); } break;
                            }
                        } break;
                        case FmtArg::Type::Double: {
                            switch (arg.value.span.type_len) {
                                case SIZE(double): { arg2.value.d.value = *(const double *)ptr; } break;
                                case SIZE(float): { arg2.value.d.value = *(const float *)ptr; } break;
                                default: { Assert(false); } break;
                            }
                            arg2.value.d.precision = -1;
                        } break;
                        case FmtArg::Type::MemorySize: { arg2.value.size = *(const Size *)ptr; } break;
                        case FmtArg::Type::DiskSize: { arg2.value.size = *(const Size *)ptr; } break;
                        case FmtArg::Type::Date: { arg2.value.date = *(const Date *)ptr; } break;
                        case FmtArg::Type::Span: { Assert(false); } break;
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
            append(*PATH_SEPARATORS);
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
    DebugAssert(out_buf.len >= 0);

    if (!out_buf.len)
        return {};
    out_buf.len--;

    Size real_len = 0;

    DoFormat(fmt, args, [&](Span<const char> fragment) {
        if (LIKELY(real_len < out_buf.len)) {
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

    out_buf->Grow(FMT_STRING_BASE_CAPACITY);
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
    LocalArray<char, FMT_STRING_PRINT_BUFFER_SIZE> buf;
    DoFormat(fmt, args, [&](Span<const char> frag) {
        if (frag.len > ARRAY_SIZE(buf.data) - buf.len) {
            st->Write(buf);
            buf.len = 0;
        }
        if (frag.len >= ARRAY_SIZE(buf.data)) {
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
    LocalArray<char, FMT_STRING_PRINT_BUFFER_SIZE> buf;
    DoFormat(fmt, args, [&](Span<const char> frag) {
        if (frag.len > ARRAY_SIZE(buf.data) - buf.len) {
            fwrite(buf.data, 1, (size_t)buf.len, fp);
            buf.len = 0;
        }
        if (frag.len >= ARRAY_SIZE(buf.data)) {
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
static THREAD_LOCAL std::function<LogHandlerFunc> *log_handlers[16];
static THREAD_LOCAL Size log_handlers_len;

bool enable_debug = GetDebugFlag("LIBCC_DEBUG");

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

static bool ConfigLogTerminalOutput()
{
    static bool init, output_is_terminal;

    if (!init) {
#ifdef _WIN32
        static HANDLE stderr_handle = (HANDLE)_get_osfhandle(_fileno(stderr));
        static DWORD orig_console_mode;

        if (GetConsoleMode(stderr_handle, &orig_console_mode) &&
                !(orig_console_mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            // Enable VT100 escape sequences, introduced in Windows 10
            DWORD new_mode = orig_console_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            output_is_terminal = SetConsoleMode(stderr_handle, new_mode);

            if (output_is_terminal) {
                atexit([]() {
                    WriteFile(stderr_handle, "\x1B[0m", (DWORD)strlen("\x1B[0m"), nullptr, nullptr);
                    SetConsoleMode(stderr_handle, orig_console_mode);
                });
            } else {
                // Try ConEmu ANSI support for Windows < 10
                const char *conemuansi_str = getenv("ConEmuANSI");
                output_is_terminal = conemuansi_str && TestStr(conemuansi_str, "ON");
            }
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

void LogFmt(LogLevel level, const char *ctx MAYBE_UNUSED, const char *fmt, Span<const FmtArg> args)
{
    if (level == LogLevel::Debug && !enable_debug)
        return;

    char ctx_buf[128];
    {
        double time = (double)(GetMonotonicTime() - g_start_time) / 1000;

#ifdef NDEBUG
        Fmt(ctx_buf, " [%1]  ", FmtDouble(time, 3).Pad(-8));
#else
        Size ctx_len = (Size)strlen(ctx);
        if (ctx_len > 20) {
            Fmt(ctx_buf, " ...%1 [%2]  ", ctx + ctx_len - 17, FmtDouble(time, 3).Pad(-8));
        } else {
            Fmt(ctx_buf, " ...%1 [%2]  ", FmtArg(ctx).Pad(-21), FmtDouble(time, 3).Pad(-8));
        }
#endif
    }

    static std::mutex log_mutex;

    std::lock_guard<std::mutex> lock(log_mutex);
    if (log_handlers_len) {
        (*log_handlers[log_handlers_len - 1])(level, ctx_buf, fmt, args);
    } else {
        DefaultLogHandler(level, ctx_buf, fmt, args);
    }
}

void DefaultLogHandler(LogLevel level, const char *ctx,
                       const char *fmt, Span<const FmtArg> args)
{
    StartConsoleLog(level);
    Print(stderr, ctx);
    PrintFmt(fmt, args, stderr);
    EndConsoleLog();
}

void StartConsoleLog(LogLevel level)
{
    if (!ConfigLogTerminalOutput())
        return;

    switch (level)  {
        case LogLevel::Error: { fputs("\x1B[31m", stderr); } break;
        case LogLevel::Info: { fputs("\x1B[36m", stderr); } break;
        case LogLevel::Debug: { fputs("\x1B[33m", stderr); } break;
    }
}

void EndConsoleLog()
{
    fputs("\n", stderr);
    if (ConfigLogTerminalOutput()) {
        fputs("\x1B[0m", stderr);
    }
}

void PushLogHandler(std::function<LogHandlerFunc> handler)
{
    DebugAssert(log_handlers_len < ARRAY_SIZE(log_handlers));
    log_handlers[log_handlers_len++] = new std::function<LogHandlerFunc>(handler);
}

void PopLogHandler()
{
    DebugAssert(log_handlers_len > 0);
    delete log_handlers[--log_handlers_len];
}

// ------------------------------------------------------------------------
// System
// ------------------------------------------------------------------------

#ifdef _WIN32

static char *Win32ErrorString(DWORD error_code = UINT32_MAX)
{
    if (error_code == UINT32_MAX) {
        error_code = GetLastError();
    }

    static THREAD_LOCAL char str_buf[256];
    DWORD fmt_ret = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                  nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                  str_buf, SIZE(str_buf), nullptr);
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

bool TestPath(const char *path, FileType type)
{
    DWORD attr = GetFileAttributes(path);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;

    switch (type) {
        case FileType::Directory: {
            if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                LogError("Path '%1' exists but is not a directory", path);
                return false;
            }
        } break;
        case FileType::File: {
            if (attr & FILE_ATTRIBUTE_DIRECTORY) {
                LogError("Path '%1' exists but is not a file", path);
                return false;
            }
        } break;

        case FileType::Unknown: {
            // Ignore file type
        } break;
    }

    return true;
}

EnumStatus EnumerateDirectory(const char *dirname, const char *filter, Size max_files,
                              std::function<bool(const char *, const FileInfo &)> func)
{
    char find_filter[4096];
    if (!filter) {
        filter = "*";
    }
    if (snprintf(find_filter, SIZE(find_filter), "%s\\%s", dirname, filter) >= SIZE(find_filter)) {
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
    DEFER { FindClose(handle); };

    Size count = 0;
    do {
        if (UNLIKELY(++count > max_files && max_files >= 0)) {
            LogError("Partial enumation of directory '%1'", dirname);
            return EnumStatus::Partial;
        }

        FileInfo file_info;

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            file_info.type = FileType::Directory;
        } else {
            file_info.type = FileType::File;
        }

        if (!func(find_data.cFileName, file_info))
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

bool TestPath(const char *path, FileType type)
{
    struct stat sb;
    if (stat(path, &sb) < 0)
        return false;

    switch (type) {
        case FileType::Directory: {
            if (!S_ISDIR(sb.st_mode)) {
                LogError("Path '%1' exists but is not a directory", path);
                return false;
            }
        } break;
        case FileType::File: {
            if (!S_ISREG(sb.st_mode)) {
                LogError("Path '%1' exists but is not a file", path);
                return false;
            }
        } break;

        case FileType::Unknown: {
            // Ignore file type
        } break;
    }

    return true;
}

EnumStatus EnumerateDirectory(const char *dirname, const char *filter, Size max_files,
                              std::function<bool(const char *, const FileInfo &)> func)
{
    DIR *dirp = opendir(dirname);
    if (!dirp) {
        LogError("Cannot enumerate directory '%1': %2", dirname, strerror(errno));
        return EnumStatus::Error;
    }
    DEFER { closedir(dirp); };

    Size count = 0;
    dirent *dent;
    while ((dent = readdir(dirp))) {
        if ((dent->d_name[0] == '.' && !dent->d_name[1]) ||
                (dent->d_name[0] == '.' && dent->d_name[1] == '.' && !dent->d_name[2]))
            continue;

        if (!filter || !fnmatch(filter, dent->d_name, FNM_PERIOD)) {
            if (UNLIKELY(++count > max_files && max_files >= 0)) {
                LogError("Partial enumation of directory '%1'", dirname);
                return EnumStatus::Partial;
            }

            FileInfo file_info;

#ifdef _DIRENT_HAVE_D_TYPE
            if (dent->d_type != DT_UNKNOWN && dent->d_type != DT_LNK) {
                switch (dent->d_type) {
                    case DT_DIR: { file_info.type = FileType::Directory; } break;
                    case DT_REG: { file_info.type = FileType::File; } break;
                    default: { file_info.type = FileType::Unknown; } break;
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
                    file_info.type = FileType::Directory;
                } else if (S_ISREG(sb.st_mode)) {
                    file_info.type = FileType::File;
                } else {
                    file_info.type = FileType::Unknown;
                }
            }

            if (!func(dent->d_name, file_info))
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

bool EnumerateDirectoryFiles(const char *dirname, const char *filter, Size max_files,
                             Allocator *str_alloc, HeapArray<const char *> *out_files)
{
    DEFER_NC(out_guard, len = out_files->len) { out_files->RemoveFrom(len); };

    EnumStatus status = EnumerateDirectory(dirname, filter, max_files,
                                           [&](const char *filename, const FileInfo &info) {
        if (info.type == FileType::File) {
            out_files->Append(Fmt(str_alloc, "%1%/%2", dirname, filename).ptr);
        }

        return true;
    });
    if (status == EnumStatus::Error)
        return false;

    out_guard.disable();
    return true;
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
        Size path_len = (Size)GetModuleFileName(nullptr, executable_path, SIZE(executable_path));
        Assert(path_len);
        Assert(path_len < SIZE(executable_path));
    }

    return executable_path;
#elif defined(__APPLE__)
    static char executable_path[4096];

    if (!executable_path[0]) {
        uint32_t buffer_size = SIZE(executable_path);
        Assert(!_NSGetExecutablePath(executable_path, &buffer_size));
        char *path_buf = realpath(executable_path, nullptr);
        Assert(path_buf);
        Assert(strlen(path_buf) < SIZE(executable_path));
        strcpy(executable_path, path_buf);
        free(path_buf);
    }

    return executable_path;
#elif defined(__linux__)
    static char executable_path[4096];

    if (!executable_path[0]) {
        char *path_buf = realpath("/proc/self/exe", nullptr);
        Assert(path_buf);
        Assert(strlen(path_buf) < SIZE(executable_path));
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
        while (dir_len && !strchr(PATH_SEPARATORS, executable_path[--dir_len]));
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
    filename = SplitStrReverseAny(filename, PATH_SEPARATORS);

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

const char *CanonicalizePath(Span<const char> root_dir, const char *path, Allocator *alloc)
{
    bool path_is_absolute = !root_dir.len ||
                            strchr(PATH_SEPARATORS, path[0]);
#ifdef _WIN32
    path_is_absolute |= IsAsciiAlpha(path[0]) && path[1] == ':';
#endif

    Span<char> complete_path;
    if (path_is_absolute) {
        complete_path = DuplicateString(path, alloc);
    } else {
        complete_path = Fmt(alloc, "%1%/%2", root_dir, path);
    }

#ifdef _WIN32
    char *real_path = _fullpath(nullptr, complete_path.ptr, 0);
#else
    char *real_path = realpath(complete_path.ptr, nullptr);
#endif
    if (real_path) {
        DEFER { free(real_path); };

        Allocator::Release(alloc, (void *)complete_path.ptr, complete_path.len + 1);
        return DuplicateString(real_path, alloc).ptr;
    } else {
        return complete_path.ptr;
    }
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

// ------------------------------------------------------------------------
// Tasks
// ------------------------------------------------------------------------

int GetIdealThreadCount()
{
#ifdef __EMSCRIPTEN__
    static const int max_threads = 1;
#else
    static const int max_threads = []() {
        const char *env = getenv("LIBCC_THREADS");
        if (env) {
            char *end_ptr;
            long threads = strtol(env, &end_ptr, 10);
            if (end_ptr > env && !end_ptr[0] && threads > 0) {
                return (int)threads;
            } else {
                LogError("LIBCC_THREADS must be positive number (ignored)");
            }
        }
        return (int)std::thread::hardware_concurrency();
    }();
    Assert(max_threads > 0);
#endif

    return max_threads;
}

struct Task {
    std::function<bool()> func;
    Async *async;
};

struct WorkerThread {
    std::mutex mutex;
    bool running = false;
    BlockQueue<Task> tasks;
};

struct ThreadPool {
    HeapArray<WorkerThread> workers;

    std::mutex mutex;
    std::condition_variable cv;
    std::atomic_int asyncs {0};
    std::atomic_int pending_tasks {0};
};

// thread_local breaks down on MinGW when destructors are involved, work
// around this with heap allocation.
static THREAD_LOCAL ThreadPool *g_thread_pool;
static THREAD_LOCAL WorkerThread *g_worker_thread;
static THREAD_LOCAL bool g_task_running = false;

Async::Async()
{
    if (!g_thread_pool) {
        // NOTE: We're leaking one ThreadPool each time a non-worker thread uses Async for
        // the first time. That's only one leak in most cases, when the main thread is the
        // only non-worker thread using Async, but still. Something to keep in mind.
        g_thread_pool = new ThreadPool;
        g_thread_pool->workers.AppendDefault(GetIdealThreadCount());
        g_worker_thread = &g_thread_pool->workers[0];
    }

    if (!g_thread_pool->asyncs++) {
        std::lock_guard<std::mutex> pool_lock(g_thread_pool->mutex);
        for (Size i = 1; i < g_thread_pool->workers.len; i++) {
            WorkerThread *worker = &g_thread_pool->workers[i];
            if (!worker->running) {
                std::thread(RunWorker, g_thread_pool, &g_thread_pool->workers[i]).detach();
                worker->running = true;
            }
        }
    }
}

Async::~Async()
{
    Assert(!remaining_tasks);
    g_thread_pool->asyncs--;
}

void Async::AddTask(const std::function<bool()> &func)
{
    g_worker_thread->mutex.lock();
    g_worker_thread->tasks.Append({func, this});
    remaining_tasks++;
    g_worker_thread->mutex.unlock();

    if (!g_thread_pool->pending_tasks++) {
        g_thread_pool->mutex.lock();
        g_thread_pool->cv.notify_all();
        g_thread_pool->mutex.unlock();
    }
}

bool Async::Sync()
{
    if (remaining_tasks) {
        std::unique_lock<std::mutex> queue_lock(g_worker_thread->mutex);
        while (g_worker_thread->tasks.len) {
            Task task = std::move(g_worker_thread->tasks[g_worker_thread->tasks.len - 1]);
            g_worker_thread->tasks.RemoveLast();
            queue_lock.unlock();
            task.async->RunTask(&task);
            queue_lock.lock();
        }
        queue_lock.unlock();

        // TODO: This will spin too much if queues are empty but one or a few workers are
        // still processing long running tasks.
        while (remaining_tasks) {
            StealAndRunTasks();
            std::this_thread::yield();
        }
    }

    return success;
}

bool Async::IsTaskRunning()
{
    return g_task_running;
}

void Async::RunTask(Task *task)
{
    g_thread_pool->pending_tasks--;

    g_task_running = true;
    DEFER { g_task_running = false; };

    bool ret = task->func();
    success &= ret;
    remaining_tasks--;
}

void Async::RunWorker(ThreadPool *thread_pool, WorkerThread *worker)
{
    g_thread_pool = thread_pool;
    g_worker_thread = worker;

    for (;;) {
        StealAndRunTasks();

        std::unique_lock<std::mutex> pool_lock(g_thread_pool->mutex);
        while (!g_thread_pool->pending_tasks) {
#if THREAD_MAX_IDLE_TIME >= 0
            g_thread_pool->cv.wait_for(pool_lock,
                                       std::chrono::duration<int, std::milli>(THREAD_MAX_IDLE_TIME));
            if (!g_thread_pool->asyncs) {
                worker->running = false;
                return;
            }
#else
            g_thread_pool->cv.wait(pool_lock);
#endif
        }
    }
}

void Async::StealAndRunTasks()
{
    for (int i = 0; i < 48; i++) {
        Size queue_idx = rand() / (RAND_MAX / g_thread_pool->workers.len + 1);
        WorkerThread *worker = &g_thread_pool->workers[queue_idx];

        if (worker->mutex.try_lock()) {
            if (worker->tasks.len) {
                Task task = std::move(worker->tasks[0]);
                worker->tasks.RemoveFirst();
                worker->mutex.unlock();
                task.async->RunTask(&task);
                i = -1;
            } else {
                worker->mutex.unlock();
            }
        }
    }
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

    // gzip support
    bool header_done;
    uint32_t crc32;
    Size uncompressed_size;
};
StaticAssert(SIZE(MinizInflateContext::out) >= TINFL_LZ_DICT_SIZE);
#endif

bool StreamReader::Open(Span<const uint8_t> buf, const char *filename,
                        CompressionType compression_type)
{
    Close();

    DEFER_N(error_guard) {
        ReleaseResources();
        error = true;
    };

    this->filename = filename ? filename : "<memory>";

    if (!InitDecompressor(compression_type))
        return false;
    source.type = SourceType::Memory;
    source.u.memory.buf = buf;
    source.u.memory.pos = 0;

    error_guard.disable();
    return true;
}

bool StreamReader::Open(FILE *fp, const char *filename, CompressionType compression_type)
{
    Close();
    if (!fp)
        return false;

    DEFER_N(error_guard) {
        ReleaseResources();
        error = true;
    };

    DebugAssert(filename);
    this->filename = filename;

    if (!InitDecompressor(compression_type))
        return false;
    source.type = SourceType::File;
    source.u.fp = fp;

    error_guard.disable();
    return true;
}

bool StreamReader::Open(const char *filename, CompressionType compression_type)
{
    Close();

    DEFER_N(error_guard) {
        ReleaseResources();
        error = true;
    };

    DebugAssert(filename);
    this->filename = filename;

    if (!InitDecompressor(compression_type))
        return false;
    source.type = SourceType::File;
    source.u.fp = OpenFile(filename, OpenFileMode::Read);
    if (!source.u.fp)
        return false;
    source.owned = true;

    error_guard.disable();
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
    if (UNLIKELY(error))
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

    if (LIKELY(read_len >= 0)) {
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

        out_buf->Grow(raw_len);
        Size read_len = Read(raw_len, out_buf->end());
        if (read_len < 0)
            return -1;
        out_buf->len += read_len;

        return read_len;
    } else {
        DEFER_NC(buf_guard, buf_len = out_buf->len) { out_buf->RemoveFrom(buf_len); };

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

        buf_guard.disable();
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
            DEFER { _fseeki64(source.u.fp, pos, SEEK_SET); };
            if (_fseeki64(source.u.fp, 0, SEEK_END) < 0)
                return -1;
            int64_t len = _ftelli64(source.u.fp);
#else
            off64_t pos = ftello64(source.u.fp);
            DEFER { fseeko64(source.u.fp, pos, SEEK_SET); };
            if (fseeko64(source.u.fp, 0, SEEK_END) < 0)
                return -1;
            off64_t len = ftello64(source.u.fp);
#endif
            if (len > LEN_MAX) {
                static bool warned = false;
                if (!warned) {
                    LogError("Files bigger than %1 are not well supported", FmtMemSize(LEN_MAX));
                    warned = true;
                }
                len = LEN_MAX;
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
                (MinizInflateContext *)Allocator::Allocate(nullptr, SIZE(MinizInflateContext),
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
            Allocator::Release(nullptr, compression.u.miniz, SIZE(*compression.u.miniz));
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

    // gzip header is not directly supported by miniz. Currently this
    // will fail if the header is longer than 4096 bytes, which is
    // probably quite rare.
    if (compression.type == CompressionType::Gzip && !ctx->header_done) {
        uint8_t header[4096];
        Size header_len;

        header_len = ReadRaw(SIZE(header), header);
        if (header_len < 0) {
            return -1;
        } else if (header_len < 10 || header[0] != 0x1F || header[1] != 0x8B) {
            LogError("File '%1' does not look like a gzip stream", filename);
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

            while (ctx->out_len < SIZE(ctx->out)) {
                if (!ctx->in_len) {
                    ctx->in_ptr = ctx->in;
                    ctx->in_len = ReadRaw(SIZE(ctx->in), ctx->in);
                    if (ctx->in_len < 0)
                        return read_len ? read_len : ctx->in_len;
                }

                size_t in_arg = (size_t)ctx->in_len;
                size_t out_arg = (size_t)(SIZE(ctx->out) - ctx->out_len);
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
                    // gzip footer (CRC and size check)
                    if (compression.type == CompressionType::Gzip) {
                        uint32_t footer[2];
                        StaticAssert(SIZE(footer) == 8);

                        if (ctx->in_len < SIZE(footer)) {
                            memcpy(footer, ctx->in_ptr, (size_t)ctx->in_len);

                            Size missing_len = SIZE(footer) - ctx->in_len;
                            if (ReadRaw(missing_len, footer + ctx->in_len) < missing_len) {
                                if (error) {
                                    return -1;
                                } else {
                                    goto truncated_error;
                                }
                            }
                        } else {
                            memcpy(footer, ctx->in_ptr, SIZE(footer));
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
    LogError("Truncated gzip header in '%1'", filename);
    error = true;
    return -1;
#else
    DebugAssert(false);
#endif
}

Size StreamReader::ReadRaw(Size max_len, void *out_buf)
{
    ComputeStreamLen();

    Size read_len = 0;
    switch (source.type) {
        case SourceType::File: {
            read_len = (Size)fread(out_buf, 1, (size_t)max_len, source.u.fp);
            if (ferror(source.u.fp)) {
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
    if (UNLIKELY(error || eof))
        return false;

    for (;;) {
        if (!view.len) {
            buf.Grow(LINE_READER_STEP_SIZE + 1);

            Size read_len = st->Read(LINE_READER_STEP_SIZE, buf.end());
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
    ::PushLogHandler([=](LogLevel level, const char *ctx,
                         const char *fmt, Span<const FmtArg> args) {
        StartConsoleLog(level);
        Print(stderr, "%1%2(%3): ", ctx, st->filename, line_number);
        PrintFmt(fmt, args, stderr);
        EndConsoleLog();
    });
}

#ifdef MZ_VERSION
struct MinizDeflateContext {
    tdefl_compressor deflator;

    // gzip support
    uint32_t crc32;
    Size uncompressed_size;
};
#endif

bool StreamWriter::Open(HeapArray<uint8_t> *mem, const char *filename,
                        CompressionType compression_type)
{
    Close();

    DEFER_N(error_guard) {
        ReleaseResources();
        error = true;
    };

    this->filename = filename ? filename : "<memory>";

    if (!InitCompressor(compression_type))
        return false;
    dest.type = DestinationType::Memory;
    dest.u.mem = mem;

    open = true;
    error_guard.disable();
    return true;
}

bool StreamWriter::Open(FILE *fp, const char *filename, CompressionType compression_type)
{
    Close();
    if (!fp)
        return false;

    DEFER_N(error_guard) {
        ReleaseResources();
        error = true;
    };

    DebugAssert(filename);
    this->filename = filename;

    if (!InitCompressor(compression_type))
        return false;
    dest.type = DestinationType::File;
    dest.u.fp = fp;

    open = true;
    error_guard.disable();
    return true;
}

bool StreamWriter::Open(const char *filename, CompressionType compression_type)
{
    Close();

    DEFER_N(error_guard) {
        ReleaseResources();
        error = true;
    };

    DebugAssert(filename);
    this->filename = filename;

    if (!InitCompressor(compression_type))
        return false;
    dest.type = DestinationType::File;
    dest.u.fp = OpenFile(filename, OpenFileMode::Write);
    if (!dest.u.fp)
        return false;
    dest.owned = true;

    open = true;
    error_guard.disable();
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

                    success &= WriteRaw(MakeSpan((uint8_t *)gzip_footer, SIZE(gzip_footer)));
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
    if (UNLIKELY(error))
        return false;

    switch (compression.type) {
        case CompressionType::None: {
            return WriteRaw(buf);
        } break;

        case CompressionType::Gzip:
        case CompressionType::Zlib: {
#ifdef MZ_VERSION
            if (compression.type == CompressionType::Gzip) {
                if (!compression.u.miniz->uncompressed_size && buf.len) {
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
    DebugAssert(false);
}

bool StreamWriter::InitCompressor(CompressionType type)
{
    switch (type) {
        case CompressionType::None: {} break;

        case CompressionType::Gzip:
        case CompressionType::Zlib: {
#ifdef MZ_VERSION
            compression.u.miniz =
                (MinizDeflateContext *)Allocator::Allocate(nullptr, SIZE(MinizDeflateContext),
                                                           (int)Allocator::Flag::Zero);
            compression.u.miniz->crc32 = MZ_CRC32_INIT;

            int flags = (type == CompressionType::Zlib ? TDEFL_WRITE_ZLIB_HEADER : 0) | 32;

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
            Allocator::Release(nullptr, compression.u.miniz, SIZE(*compression.u.miniz));
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
            if (fwrite(buf.ptr, 1, (size_t)buf.len, dest.u.fp) != (size_t)buf.len) {
                LogError("Failed to write to '%1': %2", filename, strerror(errno));
                error = true;
                return false;
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
    DebugAssert(false);
}

bool SpliceStream(StreamReader *reader, Size max_len, StreamWriter *writer)
{
    if (reader->error)
        return false;

    Size len = 0;
    while (!reader->eof) {
        char buf[128 * 1024];
        Size read_len = reader->Read(SIZE(buf), buf);
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
    if (UNLIKELY(error))
        return LineType::Exit;

    DEFER_N(error_guard) { error = true; };

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

            error_guard.disable();
            current_section.RemoveFrom(0);
            current_section.Append(section);
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

            error_guard.disable();
            out_prop->section = current_section;
            out_prop->key = key;
            out_prop->value = value;
            return LineType::KeyValue;
        }
    }
    if (reader.error)
        return LineType::Exit;

    error_guard.disable();
    eof = true;
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
            if (len > SIZE(buf) - 1) {
                len = SIZE(buf) - 1;
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
    } else {
        current_option = opt;
        pos++;
    }

    return current_option;
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

const char *OptionParser::RequireValue(void (*usage_func)(FILE *fp))
{
    if (!ConsumeValue()) {
        LogError("Option '%1' needs an argument", current_option);
        if (usage_func) {
            usage_func(stderr);
        }
    }

    return current_value;
}
