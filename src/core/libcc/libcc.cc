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

#include "libcc.hh"
#if !defined(LIBCC_NO_MINIZ) && __has_include("vendor/miniz/miniz.h")
    #define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
    #include "vendor/miniz/miniz.h"
#endif
#if !defined(LIBCC_NO_BROTLI) && __has_include("vendor/brotli/c/include/brotli/decode.h")
    #include "vendor/brotli/c/include/brotli/decode.h"
    #include "vendor/brotli/c/include/brotli/encode.h"
#endif
#if __has_include("vendor/dragonbox/include/dragonbox/dragonbox.h")
    #include "vendor/dragonbox/include/dragonbox/dragonbox.h"
#endif

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <fcntl.h>
    #include <io.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <direct.h>
    #include <shlobj.h>
    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
        #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
    #endif

    #ifndef UNIX_PATH_MAX
        #define UNIX_PATH_MAX 108
    #endif
    typedef struct sockaddr_un {
        ADDRESS_FAMILY sun_family;
        char sun_path[UNIX_PATH_MAX];
    } SOCKADDR_UN, *PSOCKADDR_UN;

    #ifdef __MINGW32__
        // Some MinGW distributions set it to 0 by default
        int _CRT_glob = 1;
    #endif

    #define RtlGenRandom SystemFunction036
    extern "C" BOOLEAN NTAPI RtlGenRandom(PVOID RandomBuffer, ULONG RandomBufferLength);
#else
    #include <dlfcn.h>
    #include <dirent.h>
    #include <fcntl.h>
    #include <fnmatch.h>
    #include <grp.h>
    #include <poll.h>
    #include <signal.h>
    #include <spawn.h>
    #include <sys/ioctl.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <sys/wait.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <termios.h>
    #include <time.h>
    #include <unistd.h>

    extern char **environ;
#endif
#ifdef __APPLE__
    #include <sys/random.h>
    #include <mach-o/dyld.h>

    #define off64_t off_t
    #define fseeko64 fseeko
    #define ftello64 ftello
#endif
#if defined(__OpenBSD__) || defined(__FreeBSD__)
    #include <pthread_np.h>
    #include <sys/param.h>
    #include <sys/sysctl.h>

    #define off64_t off_t
    #define fseeko64 fseeko
    #define ftello64 ftello
#endif
#include <chrono>
#include <random>
#include <thread>

namespace RG {

// ------------------------------------------------------------------------
// Utility
// ------------------------------------------------------------------------

#ifndef FELIX
    #ifdef FELIX_TARGET
        const char *FelixTarget = RG_STRINGIFY(FELIX_TARGET);
    #else
        const char *FelixTarget = "????";
    #endif
    const char *FelixVersion = "(unknown version)";
    const char *FelixCompiler = "????";
#endif

extern "C" void AssertMessage(const char *filename, int line, const char *cond)
{
    fprintf(stderr, "%s:%d: Assertion '%s' failed\n", filename, line, cond);
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
        RG_CRITICAL(ptr, "Failed to allocate %1 of memory", FmtMemSize(size));

        if (flags & (int)Allocator::Flag::Zero) {
            memset_safe(ptr, 0, (size_t)size);
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
            RG_CRITICAL(new_ptr || !new_size, "Failed to resize %1 memory block to %2",
                                              FmtMemSize(old_size), FmtMemSize(new_size));

            if ((flags & (int)Allocator::Flag::Zero) && new_size > old_size) {
                memset_safe((uint8_t *)new_ptr + old_size, 0, (size_t)(new_size - old_size));
            }

            *ptr = new_ptr;
        }
    }

    void Release(void *ptr, Size) override
    {
        free(ptr);
    }
};

Allocator *GetDefaultAllocator()
{
    static Allocator *default_allocator = new RG_DEFAULT_ALLOCATOR;
    return default_allocator;
}

LinkedAllocator& LinkedAllocator::operator=(LinkedAllocator &&other)
{
    ReleaseAll();
    list = other.list;
    other.list = {};

    return *this;
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

void *BlockAllocatorBase::Allocate(Size size, unsigned int flags)
{
    RG_ASSERT(size >= 0);

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
            memset_safe(ptr, 0, size);
        }

        last_alloc = ptr;
        return ptr;
    }
}

void BlockAllocatorBase::Resize(void **ptr, Size old_size, Size new_size, unsigned int flags)
{
    RG_ASSERT(old_size >= 0);
    RG_ASSERT(new_size >= 0);

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
                memset_safe(ptr + old_size, 0, new_size - old_size);
            }
        } else if (AllocateSeparately(aligned_old_size)) {
            LinkedAllocator *alloc = GetAllocator();
            Allocator::Resize(alloc, ptr, old_size, new_size, flags);
        } else {
            void *new_ptr = Allocate(new_size, flags & ~(int)Allocator::Flag::Zero);
            if (new_size > old_size) {
                memcpy_safe(new_ptr, *ptr, old_size);

                if (flags & (int)Allocator::Flag::Zero) {
                    memset_safe(ptr + old_size, 0, new_size - old_size);
                }
            } else {
                memcpy_safe(new_ptr, *ptr, new_size);
            }

            *ptr = new_ptr;
        }
    }
}

void BlockAllocatorBase::Release(void *ptr, Size size)
{
    RG_ASSERT(size >= 0);

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

void BlockAllocatorBase::CopyFrom(BlockAllocatorBase *other)
{
    block_size = other->block_size;
    current_bucket = other->current_bucket;
    last_alloc = other->last_alloc;
}

void BlockAllocatorBase::ForgetCurrentBlock()
{
    current_bucket = nullptr;
    last_alloc = nullptr;
}

BlockAllocator& BlockAllocator::operator=(BlockAllocator &&other)
{
    allocator.operator=(std::move(other.allocator));
    CopyFrom(&other);

    return *this;
}

void BlockAllocator::ReleaseAll()
{
    ForgetCurrentBlock();
    allocator.ReleaseAll();
}

IndirectBlockAllocator& IndirectBlockAllocator::operator=(IndirectBlockAllocator &&other)
{
    allocator->operator=(std::move(*other.allocator));
    CopyFrom(&other);

    return *this;
}

void IndirectBlockAllocator::ReleaseAll()
{
    allocator->ReleaseAll();
}

// ------------------------------------------------------------------------
// Date
// ------------------------------------------------------------------------

// XXX: Rewrite the ugly parsing part
Date Date::Parse(Span<const char> date_str, unsigned int flags,
                      Span<const char> *out_remaining)
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
    RG_ASSERT(days >= 0);

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
    RG_ASSERT(IsValid());

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
    RG_ASSERT(IsValid());

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
    RG_ASSERT(IsValid());

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
    RG_ASSERT(IsValid());

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

#ifdef _WIN32
static int64_t FileTimeToUnixTime(FILETIME ft)
{
    int64_t time = ((int64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return time / 10000 - 11644473600000ll;
}
#endif

int64_t GetUnixTime()
{
#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    return FileTimeToUnixTime(ft);
#elif defined(__EMSCRIPTEN__)
    return (int64_t)emscripten_get_now();
#elif defined(__linux__)
    struct timespec ts;
    RG_CRITICAL(clock_gettime(CLOCK_REALTIME_COARSE, &ts) == 0, "clock_gettime(CLOCK_REALTIME_COARSE) failed: %1", strerror(errno));

    int64_t time = (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
    return time;
#else
    struct timespec ts;
    RG_CRITICAL(clock_gettime(CLOCK_REALTIME, &ts) == 0, "clock_gettime(CLOCK_REALTIME) failed: %1", strerror(errno));

    int64_t time = (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
    return time;
#endif
}

int64_t GetMonotonicTime()
{
#if defined(_WIN32)
    return (int64_t)GetTickCount64();
#elif defined(__EMSCRIPTEN__)
    return (int64_t)emscripten_get_now();
#elif defined(__linux__)
    struct timespec ts;
    RG_CRITICAL(clock_gettime(CLOCK_MONOTONIC_COARSE, &ts) == 0, "clock_gettime(CLOCK_MONOTONIC_COARSE) failed: %1", strerror(errno));

    return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
#else
    struct timespec ts;
    RG_CRITICAL(clock_gettime(CLOCK_MONOTONIC, &ts) == 0, "clock_gettime(CLOCK_MONOTONIC) failed: %1", strerror(errno));

    return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
#endif
}

TimeSpec DecomposeTime(int64_t time, TimeMode mode)
{
    TimeSpec spec = {};

#ifdef _WIN32
    __time64_t time64 = time / 1000;

    struct tm ti = {0};
    int offset = INT_MAX;
    switch (mode) {
        case TimeMode::Local: {
            _localtime64_s(&ti, &time64);

            struct tm utc = {0};
            _gmtime64_s(&utc, &time64);

            offset = (int)(_mktime64(&ti) - _mktime64(&utc) + (3600 * ti.tm_isdst));
        } break;

        case TimeMode::UTC: {
            _gmtime64_s(&ti, &time64);
            offset = 0;
        } break;
    }
    RG_ASSERT(offset != INT_MAX);
#else
    time_t time64 = time / 1000;

    struct tm ti = {0};
    int offset = 0;
    switch (mode) {
        case TimeMode::Local: {
            localtime_r(&time64, &ti);
            offset = ti.tm_gmtoff + ti.tm_isdst * 3600;
        } break;

        case TimeMode::UTC: {
            gmtime_r(&time64, &ti);
            offset = 0;
        } break;
    }
    RG_ASSERT(offset != INT_MAX);
#endif

    spec.year = (int16_t)(1900 + ti.tm_year);
    spec.month = (int8_t)ti.tm_mon + 1; // Whose idea was it to use 0-11? ...
    spec.day = (int8_t)ti.tm_mday;
    spec.week_day = (int8_t)(ti.tm_wday ? (ti.tm_wday + 1) : 7);
    spec.hour = (int8_t)ti.tm_hour;
    spec.min = (int8_t)ti.tm_min;
    spec.sec = (int8_t)ti.tm_sec;
    spec.msec = time % 1000;
    spec.offset = (int16_t)(offset / 60);

    return spec;
}

// ------------------------------------------------------------------------
// Strings
// ------------------------------------------------------------------------

bool CopyString(const char *str, Span<char> buf)
{
#ifdef RG_DEBUG
    RG_ASSERT(buf.len > 0);
#else
    if (RG_UNLIKELY(!buf.len))
        return false;
#endif

    Size i = 0;
    for (; str[i]; i++) {
        if (RG_UNLIKELY(i >= buf.len - 1)) {
            buf[buf.len - 1] = 0;
            return false;
        }
        buf[i] = str[i];
    }
    buf[i] = 0;

    return true;
}

bool CopyString(Span<const char> str, Span<char> buf)
{
#ifdef RG_DEBUG
    RG_ASSERT(buf.len > 0);
#else
    if (RG_UNLIKELY(!buf.len))
        return false;
#endif

    if (RG_UNLIKELY(str.len > buf.len - 1))
        return false;

    memcpy_safe(buf.ptr, str.ptr, str.len);
    buf[str.len] = 0;

    return true;
}

Span<char> DuplicateString(Span<const char> str, Allocator *alloc)
{
    char *new_str = (char *)Allocator::Allocate(alloc, str.len + 1);
    memcpy_safe(new_str, str.ptr, (size_t)str.len);
    new_str[str.len] = 0;
    return MakeSpan(new_str, str.len);
}

bool ParseBool(Span<const char> str, bool *out_value, unsigned int flags,
               Span<const char> *out_remaining)
{
#define TRY_MATCH(Match, Value) \
        do { \
            if (str == (Match)) { \
                *out_value = (Value); \
                if (out_remaining) { \
                    *out_remaining = str.Take(str.len, 0); \
                } \
                return true; \
            } else if (!(flags & (int)ParseFlag::End)) { \
                *out_value = (Value); \
                if (out_remaining) { \
                    Size match_len = strlen(Match); \
                    *out_remaining = str.Take(match_len, str.len - match_len); \
                } \
                return true; \
            } \
        } while (false)

    TRY_MATCH("1", true);
    TRY_MATCH("On", true);
    TRY_MATCH("Y", true);
    TRY_MATCH("True", true);
    TRY_MATCH("0", false);
    TRY_MATCH("Off", false);
    TRY_MATCH("N", false);
    TRY_MATCH("False", false);

    if (flags & (int)ParseFlag::Log) {
        LogError("Invalid boolean value '%1'", str);
    }
    return false;
}

// ------------------------------------------------------------------------
// Format
// ------------------------------------------------------------------------

static const char DigitPairs[201] = "00010203040506070809101112131415161718192021222324"
                                    "25262728293031323334353637383940414243444546474849"
                                    "50515253545556575859606162636465666768697071727374"
                                    "75767778798081828384858687888990919293949596979899";

static Span<char> FormatUnsignedToDecimal(uint64_t value, char out_buf[32])
{
    Size offset = 32;
    {
        int pair_idx;
        do {
            pair_idx = (int)((value % 100) * 2);
            value /= 100;
            offset -= 2;
            memcpy_safe(out_buf + offset, DigitPairs + pair_idx, 2);
        } while (value);
        offset += (pair_idx < 20);
    }

    return MakeSpan(out_buf + offset, 32 - offset);
}

static Span<char> FormatUnsignedToHex(uint64_t value, char out_buf[32])
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

static Span<char> FormatUnsignedToBinary(uint64_t value, char out_buf[64])
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

#ifdef JKJ_HEADER_DRAGONBOX
static Size FakeFloatPrecision(Span<char> buf, int K, int min_prec, int max_prec, int *out_K)
{
    RG_ASSERT(min_prec >= 0);

    if (-K < min_prec) {
        int delta = min_prec + K;
        memset_safe(buf.end(), '0', delta);

        *out_K -= delta;
        return buf.len + delta;
    } else if (-K > max_prec) {
        if (-K <= buf.len) {
            int offset = (int)buf.len + K;
            int truncate = offset + max_prec;
            int scale = offset + max_prec;

            if (buf[truncate] >= '5') {
                buf[truncate] = '0';

                for (int i = truncate - 1; i >= 0; i--) {
                    if (buf[i] == '9') {
                        buf[i] = '0' + !i;
                        truncate += !i;
                    } else {
                        buf[i]++;
                        break;
                    }
                }
            }

            *out_K -= (int)(scale - buf.len);
            return truncate;
        } else {
            buf[0] = '0' + (-K == buf.len + 1 && buf[0] >= '5');

            if (min_prec) {
                memset_safe(buf.ptr + 1, '0', min_prec - 1);
                *out_K = -min_prec;
                return min_prec;
            } else {
                *out_K = 0;
                return 1;
            }
        }
    } else {
        return buf.len;
    }
}

static Span<char> PrettifyFloat(Span<char> buf, int K, int min_prec, int max_prec)
{
    // Apply precision settings after conversion
    buf.len = FakeFloatPrecision(buf, K, min_prec, max_prec, &K);

    int KK = (int)buf.len + K;

    if (K >= 0) {
        // 1234e7 -> 12340000000

        if (!buf.len && !K) {
            K = 1;
        }

        memset_safe(buf.end(), '0', (size_t)K);
        buf.len += K;
    } else if (KK > 0) {
        // 1234e-2 -> 12.34

        memmove_safe(buf.ptr + KK + 1, buf.ptr + KK, (size_t)(buf.len - KK));
        buf.ptr[KK] = '.';
        buf.len++;
    } else {
        // 1234e-6 -> 0.001234

        int offset = 2 - KK;
        memmove_safe(buf.ptr + offset, buf.ptr, (size_t)buf.len);
        memset_safe(buf.ptr, '0', (size_t)offset);
        buf.ptr[1] = '.';
        buf.len += offset;
    }

    return buf;
}

static Span<char> ExponentiateFloat(Span<char> buf, int K, int min_prec, int max_prec)
{
    // Apply precision settings after conversion
    buf.len = FakeFloatPrecision(buf, (int)(1 - buf.len), min_prec, max_prec, &K);

    int exponent = (int)buf.len + K - 1;

    if (buf.len > 1) {
        memmove_safe(buf.ptr + 2, buf.ptr + 1, (size_t)(buf.len - 1));
        buf.ptr[1] = '.';
        buf.ptr[buf.len + 1] = 'e';
        buf.len += 2;
    } else {
        buf.ptr[1] = 'e';
        buf.len = 2;
    }

    if (exponent > 0) {
        buf.ptr[buf.len++] = '+';
    } else {
        buf.ptr[buf.len++] = '-';
        exponent = -exponent;
    }

    if (exponent >= 100) {
        buf.ptr[buf.len++] = (char)('0' + exponent / 100);
        exponent %= 100;

        int pair_idx = (int)(exponent * 2);
        memcpy_safe(buf.end(), DigitPairs + pair_idx, 2);
        buf.len += 2;
    } else if (exponent >= 10) {
        int pair_idx = (int)(exponent * 2);
        memcpy_safe(buf.end(), DigitPairs + pair_idx, 2);
        buf.len += 2;
    } else {
        buf.ptr[buf.len++] = (char)('0' + exponent);
    }

    return buf;
}
#endif

// NaN and Inf are handled by caller
template <typename T>
Span<const char> FormatFloatingPoint(T value, bool non_zero, int min_prec, int max_prec, char out_buf[128])
{
#ifdef JKJ_HEADER_DRAGONBOX
    if (non_zero) {
        auto v = jkj::dragonbox::to_decimal(value, jkj::dragonbox::policy::sign::ignore);

        Span<char> buf = FormatUnsignedToDecimal(v.significand, out_buf);
        int KK = (int)buf.len + v.exponent;

        if (KK > -6 && KK <= 21) {
            return PrettifyFloat(buf, v.exponent, min_prec, max_prec);
        } else {
            return ExponentiateFloat(buf, v.exponent, min_prec, max_prec);
        }
    } else {
        Span<char> buf = MakeSpan(out_buf, 128);

        buf[0] = '0';
        if (min_prec) {
            buf.ptr[1] = '.';
            memset_safe(buf.ptr + 2, '0', min_prec);
            buf.len = 2 + min_prec;
        } else {
            buf.len = 1;
        }

        return buf;
    }
#else
    #ifdef _MSC_VER
        #pragma message("Cannot format floating point values correctly without Dragonbox")
    #else
        #warning Cannot format floating point values correctly without Dragonbox
    #endif

    int ret = snprintf(out_buf, 128, "%g", value);
    return MakeSpan(out_buf, std::min(ret, 128));
#endif
}

template <typename AppendFunc>
static inline void ProcessArg(const FmtArg &arg, AppendFunc append)
{
    for (int i = 0; i < arg.repeat; i++) {
        LocalArray<char, 2048> out_buf;
        char num_buf[128];
        Span<const char> out = {};

        Size pad_len = arg.pad_len;

        switch (arg.type) {
            case FmtType::Str1: { out = arg.u.str1; } break;
            case FmtType::Str2: { out = arg.u.str2; } break;
            case FmtType::Buffer: { out = arg.u.buf; } break;
            case FmtType::Char: { out = MakeSpan(&arg.u.ch, 1); } break;

            case FmtType::Bool: {
                if (arg.u.b) {
                    out = "true";
                } else {
                    out = "false";
                }
            } break;

            case FmtType::Integer: {
                if (arg.u.i < 0) {
                    if (arg.pad_len < 0 && arg.pad_char == '0') {
                        append('-');
                    } else {
                        out_buf.Append('-');
                    }

                    out_buf.Append(FormatUnsignedToDecimal((uint64_t)-arg.u.i, num_buf));
                    out = out_buf;
                } else {
                    out = FormatUnsignedToDecimal((uint64_t)arg.u.i, num_buf);
                }
            } break;
            case FmtType::Unsigned: {
                out = FormatUnsignedToDecimal(arg.u.u, num_buf);
            } break;
            case FmtType::Float: {
                static const uint32_t ExponentMask = 0x7f800000u;
                static const uint32_t MantissaMask = 0x007fffffu;
                static const uint32_t SignMask = 0x80000000u;

                union { float f; uint32_t u32; } u;
                u.f = arg.u.f.value;

                if ((u.u32 & ExponentMask) == ExponentMask) {
                    uint32_t mantissa = u.u32 & MantissaMask;

                    if (mantissa) {
                        out = "NaN";
                    } else {
                        out = (u.u32 & SignMask) ? "-Inf" : "Inf";
                    }
                } else {
                    if (u.u32 & SignMask) {
                        if (arg.pad_len < 0 && arg.pad_char == '0') {
                            append('-');
                        } else {
                            out_buf.Append('-');
                        }

                        out_buf.Append(FormatFloatingPoint(-u.f, true, arg.u.f.min_prec, arg.u.f.max_prec, num_buf));
                        out = out_buf;
                    } else {
                        out = FormatFloatingPoint(u.f, u.u32, arg.u.f.min_prec, arg.u.f.max_prec, num_buf);
                    }
                }
            } break;
            case FmtType::Double: {
                static const uint64_t ExponentMask = 0x7FF0000000000000ull;
                static const uint64_t MantissaMask = 0x000FFFFFFFFFFFFFull;
                static const uint64_t SignMask = 0x8000000000000000ull;

                union { double d; uint64_t u64; } u;
                u.d = arg.u.d.value;

                if ((u.u64 & ExponentMask) == ExponentMask) {
                    uint64_t mantissa = u.u64 & MantissaMask;

                    if (mantissa) {
                        out = "NaN";
                    } else {
                        out = (u.u64 & SignMask) ? "-Inf" : "Inf";
                    }
                } else {
                    if (u.u64 & SignMask) {
                        if (arg.pad_len < 0 && arg.pad_char == '0') {
                            append('-');
                        } else {
                            out_buf.Append('-');
                        }

                        out_buf.Append(FormatFloatingPoint(-u.d, true, arg.u.d.min_prec, arg.u.d.max_prec, num_buf));
                        out = out_buf;
                    } else {
                        out = FormatFloatingPoint(u.d, u.u64, arg.u.d.min_prec, arg.u.d.max_prec, num_buf);
                    }
                }
            } break;
            case FmtType::Binary: {
                out = FormatUnsignedToBinary(arg.u.u, num_buf);
            } break;
            case FmtType::Hexadecimal: {
                out = FormatUnsignedToHex(arg.u.u, num_buf);
            } break;

            case FmtType::MemorySize: {
                double size;
                if (arg.u.i < 0) {
                    size = (double)-arg.u.i;
                    if (arg.pad_len < 0 && arg.pad_char == '0') {
                        append('-');
                    } else {
                        out_buf.Append('-');
                    }
                } else {
                    size = (double)arg.u.i;
                }

                if (size >= 1073688137.0) {
                    size /= 1073741824.0;

                    int prec = 1 + (size < 9.9995) + (size < 99.995);
                    out_buf.Append(FormatFloatingPoint(size, true, prec, prec, num_buf));
                    out_buf.Append(" GiB");
                } else if (size >= 1048524.0) {
                    size /= 1048576.0;

                    int prec = 1 + (size < 9.9995) + (size < 99.995);
                    out_buf.Append(FormatFloatingPoint(size, true, prec, prec, num_buf));
                    out_buf.Append(" MiB");
                } else if (size >= 1023.95) {
                    size /= 1024.0;

                    int prec = 1 + (size < 9.9995) + (size < 99.995);
                    out_buf.Append(FormatFloatingPoint(size, true, prec, prec, num_buf));
                    out_buf.Append(" kiB");
                } else {
                    out_buf.Append(FormatFloatingPoint(size, arg.u.i, 0, 0, num_buf));
                    out_buf.Append(" B");
                }

                out = out_buf;
            } break;
            case FmtType::DiskSize: {
                double size;
                if (arg.u.i < 0) {
                    size = (double)-arg.u.i;
                    if (arg.pad_len < 0 && arg.pad_char == '0') {
                        append('-');
                    } else {
                        out_buf.Append('-');
                    }
                } else {
                    size = (double)arg.u.i;
                }

                if (size >= 999950000.0) {
                    size /= 1000000000.0;

                    int prec = 1 + (size < 9.9995) + (size < 99.995);
                    out_buf.Append(FormatFloatingPoint(size, true, prec, prec, num_buf));
                    out_buf.Append(" GB");
                } else if (size >= 999950.0) {
                    size /= 1000000.0;

                    int prec = 1 + (size < 9.9995) + (size < 99.995);
                    out_buf.Append(FormatFloatingPoint(size, true, prec, prec, num_buf));
                    out_buf.Append(" MB");
                } else if (size >= 999.95) {
                    size /= 1000.0;

                    int prec = 1 + (size < 9.9995) + (size < 99.995);
                    out_buf.Append(FormatFloatingPoint(size, true, prec, prec, num_buf));
                    out_buf.Append(" kB");
                } else {
                    out_buf.Append(FormatFloatingPoint(size, arg.u.i, 0, 0, num_buf));
                    out_buf.Append(" B");
                }

                out = out_buf;
            } break;

            case FmtType::Date: {
                RG_ASSERT(!arg.u.date.value || arg.u.date.IsValid());

                int year = arg.u.date.st.year;
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
                if (arg.u.date.st.month < 10) {
                    out_buf.Append('0');
                }
                out_buf.Append(FormatUnsignedToDecimal((uint64_t)arg.u.date.st.month, num_buf));
                out_buf.Append('-');
                if (arg.u.date.st.day < 10) {
                    out_buf.Append('0');
                }
                out_buf.Append(FormatUnsignedToDecimal((uint64_t)arg.u.date.st.day, num_buf));
                out = out_buf;
            } break;

            case FmtType::TimeISO: {
                if (arg.u.time.offset) {
                    int offset_h = arg.u.time.offset / 60;
                    int offset_m = arg.u.time.offset % 60;

                    out_buf.len = Fmt(out_buf.data, "%1%2%3T%4%5%6.%7%8%9%10",
                                      FmtArg(arg.u.time.year).Pad0(-2), FmtArg(arg.u.time.month).Pad0(-2),
                                      FmtArg(arg.u.time.day).Pad0(-2), FmtArg(arg.u.time.hour).Pad0(-2),
                                      FmtArg(arg.u.time.min).Pad0(-2), FmtArg(arg.u.time.sec).Pad0(-2), FmtArg(arg.u.time.msec).Pad0(-3),
                                      offset_h >= 0 ? "+" : "", FmtArg(offset_h).Pad0(-2), FmtArg(offset_m).Pad0(-2)).len;
                } else {
                    out_buf.len = Fmt(out_buf.data, "%1%2%3T%4%5%6.%7Z",
                                      FmtArg(arg.u.time.year).Pad0(-2), FmtArg(arg.u.time.month).Pad0(-2),
                                      FmtArg(arg.u.time.day).Pad0(-2), FmtArg(arg.u.time.hour).Pad0(-2),
                                      FmtArg(arg.u.time.min).Pad0(-2), FmtArg(arg.u.time.sec).Pad0(-2), FmtArg(arg.u.time.msec).Pad0(-3)).len;
                }
                out = out_buf;
            } break;
            case FmtType::TimeNice: {
                int offset_h = arg.u.time.offset / 60;
                int offset_m = arg.u.time.offset % 60;

                out_buf.len = Fmt(out_buf.data, "%1-%2-%3 %4:%5:%6.%7 %8%9%10",
                                  FmtArg(arg.u.time.year).Pad0(-2), FmtArg(arg.u.time.month).Pad0(-2),
                                  FmtArg(arg.u.time.day).Pad0(-2), FmtArg(arg.u.time.hour).Pad0(-2),
                                  FmtArg(arg.u.time.min).Pad0(-2), FmtArg(arg.u.time.sec).Pad0(-2), FmtArg(arg.u.time.msec).Pad0(-3),
                                  offset_h >= 0 ? "+" : "", FmtArg(offset_h).Pad0(-2), FmtArg(offset_m).Pad0(-2)).len;
                out = out_buf;
            } break;

            case FmtType::Random: {
                RG_ASSERT(arg.u.random_len <= RG_SIZE(out_buf.data));

                for (Size j = 0; j < arg.u.random_len; j++) {
                    static const char *chars = "abcdefghijklmnopqrstuvwxyz0123456789";

                    int rnd = GetRandomIntSafe(0, (int)strlen(chars));
                    out_buf.Append(chars[rnd]);
                }

                out = out_buf;
            } break;

            case FmtType::FlagNames: {
                if (arg.u.flags.flags) {
                    Span<const char> sep = arg.u.flags.separator;
                    for (Size j = 0; j < arg.u.flags.u.names.len; j++) {
                        if (arg.u.flags.flags & (1ull << j)) {
                            out_buf.Append(arg.u.flags.u.names[j]);
                            out_buf.Append(sep);
                        }
                    }
                    out = out_buf.Take(0, out_buf.len - sep.len);
                } else {
                    out = "None";
                }
            } break;
            case FmtType::FlagOptions: {
                if (arg.u.flags.flags) {
                    Span<const char> sep = arg.u.flags.separator;
                    for (Size j = 0; j < arg.u.flags.u.options.len; j++) {
                        if (arg.u.flags.flags & (1ull << j)) {
                            out_buf.Append(arg.u.flags.u.options[j].name);
                            out_buf.Append(sep);
                        }
                    }
                    out = out_buf.Take(0, out_buf.len - sep.len);
                } else {
                    out = "None";
                }
            } break;

            case FmtType::Span: {
                FmtArg arg2;
                arg2.type = arg.u.span.type;
                arg2.repeat = arg.repeat;
                arg2.pad_len = arg.pad_len;
                arg2.pad_char = arg.pad_char;

                const uint8_t *ptr = (const uint8_t *)arg.u.span.ptr;
                for (Size j = 0; j < arg.u.span.len; j++) {
                    switch (arg.u.span.type) {
                        case FmtType::Str1: { arg2.u.str1 = *(const char **)ptr; } break;
                        case FmtType::Str2: { arg2.u.str2 = *(const Span<const char> *)ptr; } break;
                        case FmtType::Buffer: { RG_UNREACHABLE(); } break;
                        case FmtType::Char: { arg2.u.ch = *(const char *)ptr; } break;
                        case FmtType::Bool: { arg2.u.b = *(const bool *)ptr; } break;
                        case FmtType::Integer:
                        case FmtType::Unsigned:
                        case FmtType::Binary:
                        case FmtType::Hexadecimal: {
                            switch (arg.u.span.type_len) {
                                case 8: { arg2.u.u = *(const uint64_t *)ptr; } break;
                                case 4: { arg2.u.u = *(const uint32_t *)ptr; } break;
                                case 2: { arg2.u.u = *(const uint16_t *)ptr; } break;
                                case 1: { arg2.u.u = *(const uint8_t *)ptr; } break;
                                default: { RG_UNREACHABLE(); } break;
                            }
                        } break;
                        case FmtType::Float: {
                            arg2.u.f.value = *(const float *)ptr;
                            arg2.u.d.min_prec = 0;
                            arg2.u.d.max_prec = INT_MAX;
                        } break;
                        case FmtType::Double: {
                            arg2.u.d.value = *(const double *)ptr;
                            arg2.u.d.min_prec = 0;
                            arg2.u.d.max_prec = INT_MAX;
                        } break;
                        case FmtType::MemorySize:
                        case FmtType::DiskSize: { arg2.u.i = *(const int64_t *)ptr; } break;
                        case FmtType::Date: { arg2.u.date = *(const Date *)ptr; } break;
                        case FmtType::TimeISO:
                        case FmtType::TimeNice: { arg2.u.time = *(const TimeSpec *)ptr; } break;
                        case FmtType::Random: { RG_UNREACHABLE(); } break;
                        case FmtType::FlagNames: { RG_UNREACHABLE(); } break;
                        case FmtType::FlagOptions: { RG_UNREACHABLE(); } break;
                        case FmtType::Span: { RG_UNREACHABLE(); } break;
                    }
                    ptr += arg.u.span.type_len;

                    if (j) {
                        append(arg.u.span.separator);
                    }
                    ProcessArg(arg2, append);
                }

                continue;
            } break;
        }

        if (pad_len < 0) {
            pad_len = (-pad_len) - out.len;
            for (Size j = 0; j < pad_len; j++) {
                append(arg.pad_char);
            }
            append(out);
        } else if (pad_len > 0) {
            append(out);
            pad_len -= out.len;
            for (Size j = 0; j < pad_len; j++) {
                append(arg.pad_char);
            }
        } else {
            append(out);
        }
    }
}

template <typename AppendFunc>
static inline Size ProcessAnsiSpecifier(const char *spec, bool vt100, AppendFunc append)
{
    Size idx = 0;

    char buf[32] = "\x1B[";
    bool valid = true;

    // Foreground color
    switch (spec[++idx]) {
        case 'd': { strcat(buf, "30"); } break;
        case 'r': { strcat(buf, "31"); } break;
        case 'g': { strcat(buf, "32"); } break;
        case 'y': { strcat(buf, "33"); } break;
        case 'b': { strcat(buf, "34"); } break;
        case 'm': { strcat(buf, "35"); } break;
        case 'c': { strcat(buf, "36"); } break;
        case 'w': { strcat(buf, "37"); } break;
        case 'D': { strcat(buf, "90"); } break;
        case 'R': { strcat(buf, "91"); } break;
        case 'G': { strcat(buf, "92"); } break;
        case 'Y': { strcat(buf, "93"); } break;
        case 'B': { strcat(buf, "94"); } break;
        case 'M': { strcat(buf, "95"); } break;
        case 'C': { strcat(buf, "96"); } break;
        case 'W': { strcat(buf, "97"); } break;
        case '.': { strcat(buf, "39"); } break;
        case '0': {
            strcat(buf, "0");
            goto end;
        } break;
        case 0: {
            valid = false;
            goto end;
        } break;
        default: { valid = false; } break;
    }

    // Background color
    switch (spec[++idx]) {
        case 'd': { strcat(buf, ";40"); } break;
        case 'r': { strcat(buf, ";41"); } break;
        case 'g': { strcat(buf, ";42"); } break;
        case 'y': { strcat(buf, ";43"); } break;
        case 'b': { strcat(buf, ";44"); } break;
        case 'm': { strcat(buf, ";45"); } break;
        case 'c': { strcat(buf, ";46"); } break;
        case 'w': { strcat(buf, ";47"); } break;
        case 'D': { strcat(buf, ";100"); } break;
        case 'R': { strcat(buf, ";101"); } break;
        case 'G': { strcat(buf, ";102"); } break;
        case 'Y': { strcat(buf, ";103"); } break;
        case 'B': { strcat(buf, ";104"); } break;
        case 'M': { strcat(buf, ";105"); } break;
        case 'C': { strcat(buf, ";106"); } break;
        case 'W': { strcat(buf, ";107"); } break;
        case '.': { strcat(buf, ";49"); } break;
        case 0: {
            valid = false;
            goto end;
        } break;
        default: { valid = false; } break;
    }

    // Bold/dim/underline/invert
    switch (spec[++idx]) {
        case '+': { strcat(buf, ";1"); } break;
        case '-': { strcat(buf, ";2"); } break;
        case '_': { strcat(buf, ";4"); } break;
        case '^': { strcat(buf, ";7"); } break;
        case '.': {} break;
        case 0: {
            valid = false;
            goto end;
        } break;
        default: { valid = false; } break;
    }

end:
    if (!valid) {
#ifdef RG_DEBUG
        LogDebug("Format string contains invalid ANSI specifier");
#endif
        return idx;
    }

    if (vt100) {
        strcat(buf, "m");
        append(buf);
    }

    return idx;
}

template <typename AppendFunc>
static inline void DoFormat(const char *fmt, Span<const FmtArg> args, bool vt100, AppendFunc append)
{
#ifdef RG_DEBUG
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
#ifdef RG_DEBUG
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
        } else if (marker_ptr[1] == '!') {
            fmt_ptr = marker_ptr + 2 + ProcessAnsiSpecifier(marker_ptr + 1, vt100, append);
        } else if (marker_ptr[1]) {
            append(marker_ptr[0]);
            fmt_ptr = marker_ptr + 1;
#ifdef RG_DEBUG
            invalid_marker = true;
#endif
        } else {
#ifdef RG_DEBUG
            invalid_marker = true;
#endif
            break;
        }
    }

#ifdef RG_DEBUG
    if (invalid_marker && unused_arguments) {
        fprintf(stderr, "\nLog format string '%s' has invalid markers and unused arguments\n", fmt);
    } else if (unused_arguments) {
        fprintf(stderr, "\nLog format string '%s' has unused arguments\n", fmt);
    } else if (invalid_marker) {
        fprintf(stderr, "\nLog format string '%s' has invalid markers\n", fmt);
    }
#endif
}

static inline bool FormatBufferWithVt100()
{
    // In most cases, when fmt contains color tags, this is because the caller is
    // trying to make a string to show to the user. In this case, we want to generate
    // VT-100 escape sequences if the standard output is a terminal, because the
    // string will probably end up there.

    static bool use_vt100 = FileIsVt100(stdout) && FileIsVt100(stderr);
    return use_vt100;
}

Span<char> FmtFmt(const char *fmt, Span<const FmtArg> args, Span<char> out_buf)
{
    RG_ASSERT(out_buf.len >= 0);

    if (!out_buf.len)
        return {};
    out_buf.len--;

    Size available_len = out_buf.len;

    DoFormat(fmt, args, FormatBufferWithVt100(), [&](Span<const char> frag) {
        Size copy_len = std::min(frag.len, available_len);

        memcpy_safe(out_buf.end() - available_len, frag.ptr, (size_t)copy_len);
        available_len -= copy_len;
    });

    out_buf.len -= available_len;
    out_buf.ptr[out_buf.len] = 0;

    return out_buf;
}

Span<char> FmtFmt(const char *fmt, Span<const FmtArg> args, HeapArray<char> *out_buf)
{
    Size start_len = out_buf->len;

    out_buf->Grow(RG_FMT_STRING_BASE_CAPACITY);
    DoFormat(fmt, args, FormatBufferWithVt100(), [&](Span<const char> frag) {
        out_buf->Grow(frag.len + 1);
        memcpy_safe(out_buf->end(), frag.ptr, (size_t)frag.len);
        out_buf->len += frag.len;
    });
    out_buf->ptr[out_buf->len] = 0;

    return out_buf->Take(start_len, out_buf->len - start_len);
}

Span<char> FmtFmt(const char *fmt, Span<const FmtArg> args, Allocator *alloc)
{
    HeapArray<char> buf(alloc);
    FmtFmt(fmt, args, &buf);
    return buf.TrimAndLeak(1);
}

void PrintFmt(const char *fmt, Span<const FmtArg> args, StreamWriter *st)
{
    LocalArray<char, RG_FMT_STRING_PRINT_BUFFER_SIZE> buf;
    DoFormat(fmt, args, st->IsVt100(), [&](Span<const char> frag) {
        if (frag.len > RG_LEN(buf.data) - buf.len) {
            st->Write(buf);
            buf.len = 0;
        }
        if (frag.len >= RG_LEN(buf.data)) {
            st->Write(frag);
        } else {
            memcpy_safe(buf.data + buf.len, frag.ptr, (size_t)frag.len);
            buf.len += frag.len;
        }
    });
    st->Write(buf);
}

static void WriteStdComplete(Span<const char> buf, FILE *fp)
{
    while (buf.len) {
        Size write_len = (Size)fwrite(buf.ptr, 1, (size_t)buf.len, fp);
        if (RG_UNLIKELY(!write_len))
            break;
        buf = buf.Take(write_len, buf.len - write_len);
    }
}

void PrintFmt(const char *fmt, Span<const FmtArg> args, FILE *fp)
{
    LocalArray<char, RG_FMT_STRING_PRINT_BUFFER_SIZE> buf;
    DoFormat(fmt, args, FileIsVt100(fp), [&](Span<const char> frag) {
        if (frag.len > RG_LEN(buf.data) - buf.len) {
            WriteStdComplete(buf, fp);
            buf.len = 0;
        }
        if (frag.len >= RG_LEN(buf.data)) {
            WriteStdComplete(frag, fp);
        } else {
            memcpy_safe(buf.data + buf.len, frag.ptr, (size_t)frag.len);
            buf.len += frag.len;
        }
    });
    WriteStdComplete(buf, fp);
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

static int64_t start_time = GetMonotonicTime();

static std::function<LogFunc> log_handler = DefaultLogHandler;

// NOTE: LocalArray does not work with __thread, and thread_local is broken on MinGW
// when destructors are involved. So heap allocation it is, at least for now.
static RG_THREAD_LOCAL std::function<LogFilterFunc> *log_filters[16];
static RG_THREAD_LOCAL Size log_filters_len;

const char *GetQualifiedEnv(const char *name)
{
    RG_ASSERT(strlen(name) < 256);

    LocalArray<char, 1024> buf;
#if defined(FELIX) || defined(FELIX_TARGET)
    while (FelixTarget[buf.len]) {
        int c = UpperAscii(FelixTarget[buf.len]);
        buf.Append((char)c);
    }
    buf.Append('_');
#endif
    buf.Append(name);
    buf.Append(0);

#ifdef __EMSCRIPTEN__
    // Each accessed environment variable is kept in memory and thus leaked once
    static HashMap<const char *, const char *> values;

    std::pair<const char **, bool> ret = values.TrySet(name, nullptr);

    if (ret.second) {
        const char *ptr = (const char *)EM_ASM_INT({
            try {
                var name = UTF8ToString($0);
                var str = process.env[name];

                if (str == null)
                    return 0;

                var bytes = lengthBytesUTF8(str) + 1;
                var utf8 = _malloc(bytes);
                stringToUTF8(str, utf8, bytes);

                return utf8;
            } catch (error) {
                return 0;
            }
        }, buf.data);

        *ret.first = ptr;
    }

    return *ret.first;
#else
    return getenv(buf.data);
#endif
}

bool GetDebugFlag(const char *name)
{
    const char *debug = GetQualifiedEnv(name);

    if (debug) {
        bool ret;
        ParseBool(debug, &ret);

        return ret;
    } else {
        return false;
    }
}

static void RunLogFilter(Size idx, LogLevel level, const char *ctx, const char *msg)
{
    const std::function<LogFilterFunc> &func = *log_filters[idx];

    func(level, ctx, msg, [&](LogLevel level, const char *ctx, const char *msg) {
        if (idx > 0) {
            RunLogFilter(idx - 1, level, ctx, msg);
        } else {
            log_handler(level, ctx, msg);
        }
    });
}

void LogFmt(LogLevel level, const char *ctx, const char *fmt, Span<const FmtArg> args)
{
    static RG_THREAD_LOCAL bool skip = false;

    static bool init = false;
    static bool log_times;

    // Avoid deadlock if a log filter or the handler tries to log something while handling a previous call
    if (skip)
        return;
    skip = true;
    RG_DEFER { skip = false; };

    if (!init) {
        // Do this first... GetDebugFlag() might log an error or something, in which
        // case we don't want to recurse forever and crash!
        init = true;

        log_times = GetDebugFlag("LOG_TIMES");
    }

    char ctx_buf[512];
    if (log_times) {
        double time = (double)(GetMonotonicTime() - start_time) / 1000;
        Fmt(ctx_buf, "[%1] %2", FmtDouble(time, 3).Pad(-8), ctx ? ctx : "");

        ctx = ctx_buf;
    }

    char msg_buf[2048];
    {
        Size len = FmtFmt(fmt, args, msg_buf).len;
        if (len == RG_SIZE(msg_buf) - 1) {
            strcpy(msg_buf + RG_SIZE(msg_buf) - 32, "... [truncated]");
        }
    }

    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);

    if (log_filters_len) {
        RunLogFilter(log_filters_len - 1, level, ctx, msg_buf);
    } else {
        log_handler(level, ctx, msg_buf);
    }
}

void SetLogHandler(const std::function<LogFunc> &func)
{
    log_handler = func;
}

void DefaultLogHandler(LogLevel level, const char *ctx, const char *msg)
{
    switch (level)  {
        case LogLevel::Debug:
        case LogLevel::Info: { PrintLn(stderr, "%!D..%1%2%!0%3", ctx ? ctx : "", ctx ? ": " : "", msg); } break;
        case LogLevel::Warning: { PrintLn(stderr, "%!M..%1%2%!0%3", ctx ? ctx : "", ctx ? ": " : "", msg); } break;
        case LogLevel::Error: { PrintLn(stderr, "%!R..%1%2%!0%3", ctx ? ctx : "", ctx ? ": " : "", msg); } break;
    }

    fflush(stderr);
}

void PushLogFilter(const std::function<LogFilterFunc> &func)
{
    RG_ASSERT(log_filters_len < RG_LEN(log_filters));
    log_filters[log_filters_len++] = new std::function<LogFilterFunc>(func);
}

void PopLogFilter()
{
    RG_ASSERT(log_filters_len > 0);
    delete log_filters[--log_filters_len];
}

#ifdef _WIN32
bool RedirectLogToWindowsEvents(const char *name)
{
    static HANDLE log = nullptr;
    RG_ASSERT(!log);

    log = OpenEventLogA(nullptr, name);
    if (!log) {
        LogError("Failed to register event provider: %1", GetWin32ErrorString());
        return false;
    }
    atexit([]() { CloseEventLog(log); });

    SetLogHandler([](LogLevel level, const char *ctx, const char *msg) {
        WORD type = 0;
        LocalArray<wchar_t, 8192> buf_w;

        switch (level)  {
            case LogLevel::Debug:
            case LogLevel::Info: { type = EVENTLOG_INFORMATION_TYPE; } break;
            case LogLevel::Warning: { type = EVENTLOG_WARNING_TYPE; } break;
            case LogLevel::Error: { type = EVENTLOG_ERROR_TYPE; } break;
        }

        // Append context
        if (ctx) {
            Size len = ConvertUtf8ToWin32Wide(ctx, buf_w.Take(0, RG_LEN(buf_w.data) / 2));
            if (len < 0)
                return;
            wcscpy(buf_w.data + len, L": ");
            buf_w.len += len + 2;
        }

        // Append message
        {
            Size len = ConvertUtf8ToWin32Wide(msg, buf_w.TakeAvailable());
            if (len < 0)
                return;
            buf_w.len += len;
        }

        const wchar_t *ptr = buf_w.data;
        ReportEventW(log, type, 0, 0, nullptr, 1, 0, &ptr, nullptr);
    });

    return true;
}
#endif

// ------------------------------------------------------------------------
// System
// ------------------------------------------------------------------------

#ifdef _WIN32

static bool win32_utf8 = (GetACP() == CP_UTF8);

bool IsWin32Utf8()
{
    return win32_utf8;
}

Size ConvertUtf8ToWin32Wide(Span<const char> str, Span<wchar_t> out_str_w)
{
    RG_ASSERT(out_str_w.len >= 2);

    if (RG_UNLIKELY(!str.len)) {
        out_str_w[0] = 0;
        return 0;
    }

    int len = MultiByteToWideChar(CP_UTF8, 0, str.ptr, (int)str.len, out_str_w.ptr, (int)out_str_w.len - 1);
    if (!len) {
        switch (GetLastError()) {
            case ERROR_INSUFFICIENT_BUFFER: { LogError("String '%1' is too large", str); } break;
            case ERROR_NO_UNICODE_TRANSLATION: { LogError("String '%1' is not valid UTF-8", str); } break;
            default: { LogError("MultiByteToWideChar() failed: %1", GetWin32ErrorString()); } break;
        }
        return -1;
    }

    // MultiByteToWideChar() does not NUL terminate when passed in explicit string length
    out_str_w.ptr[len] = 0;

    return (Size)len;
}

Size ConvertWin32WideToUtf8(LPCWSTR str_w, Span<char> out_str)
{
    RG_ASSERT(out_str.len >= 1);

    int len = WideCharToMultiByte(CP_UTF8, 0, str_w, -1, out_str.ptr, (int)out_str.len - 1, nullptr, nullptr);
    if (!len) {
        switch (GetLastError()) {
            case ERROR_INSUFFICIENT_BUFFER: { LogError("String '<UTF-16 ?>' is too large"); } break;
            case ERROR_NO_UNICODE_TRANSLATION: { LogError("String '<UTF-16 ?>' is not valid UTF-8"); } break;
            default: { LogError("WideCharToMultiByte() failed: %1", GetWin32ErrorString()); } break;
        }
        return -1;
    }

    return (Size)len - 1;
}

char *GetWin32ErrorString(uint32_t error_code)
{
    static RG_THREAD_LOCAL char str_buf[512];

    if (error_code == UINT32_MAX) {
        error_code = GetLastError();
    }

    if (win32_utf8) {
        if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                            nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                            str_buf, RG_SIZE(str_buf), nullptr))
            goto fail;
    } else {
        wchar_t buf_w[256];
        if (!FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                            nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                            buf_w, RG_SIZE(buf_w), nullptr))
            goto fail;

        if (!WideCharToMultiByte(CP_UTF8, 0, buf_w, -1, str_buf, RG_SIZE(str_buf), nullptr, nullptr))
            goto fail;
    }

    // Truncate newlines
    {
        char *str_end = str_buf + strlen(str_buf);
        while (str_end > str_buf && (str_end[-1] == '\n' || str_end[-1] == '\r'))
            str_end--;
        *str_end = 0;
    }

    return str_buf;

fail:
    sprintf(str_buf, "Win32 error 0x%x", error_code);
    return str_buf;
}

void SetEnvironmentVar(const char *name, const char *value)
{
    RG_ASSERT(name && name[0] && !strchr(name, '='));
    RG_ASSERT(value);

    if (win32_utf8) {
        RG_CRITICAL(SetEnvironmentVariableA(name, value), "Failed to set environment variable '%1' to '%2': %3", name, value, GetWin32ErrorString());
    } else {
        wchar_t name_w[256];
        wchar_t value_w[4096];

        RG_CRITICAL(ConvertUtf8ToWin32Wide(name, name_w) >= 0, "Failed to set environment variable '%1' to '%2'", name, value);
        RG_CRITICAL(ConvertUtf8ToWin32Wide(value, value_w) >= 0, "Failed to set environment variable '%1' to '%2'", name, value);
        RG_CRITICAL(SetEnvironmentVariableW(name_w, value_w), "Failed to set environment variable '%1' to '%2': %3", name, value, GetWin32ErrorString());
    }
}

void DeleteEnvironmentVar(const char *name)
{
    RG_ASSERT(name && name[0] && !strchr(name, '='));

    if (win32_utf8) {
        RG_CRITICAL(SetEnvironmentVariableA(name, nullptr), "Failed to clear environment variable '%1': %2", name, GetWin32ErrorString());
    } else {
        wchar_t name_w[256];

        RG_CRITICAL(ConvertUtf8ToWin32Wide(name, name_w) >= 0, "Failed to clear environment variable '%1'", name);
        RG_CRITICAL(SetEnvironmentVariableW(name_w, nullptr), "Failed to clear environment variable '%1': %2", name, GetWin32ErrorString());
    }
}

static FileType FileAttributesToType(uint32_t attr)
{
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        return FileType::Directory;
    } else if (attr & FILE_ATTRIBUTE_DEVICE) {
        return FileType::Device;
    } else {
        return FileType::File;
    }
}

bool StatFile(const char *filename, unsigned int flags, FileInfo *out_info)
{
    // We don't detect symbolic links, but since they are much less of a hazard
    // than on POSIX systems we care a lot less about them.

    HANDLE h;
    if (win32_utf8) {
        h = CreateFileA(filename, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    } else {
        wchar_t filename_w[4096];
        if (ConvertUtf8ToWin32Wide(filename, filename_w) < 0)
            return false;

        h = CreateFileW(filename_w, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    }
    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (!(flags & (int)StatFlag::IgnoreMissing) ||
                (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND)) {
            LogError("Cannot stat file '%1': %2", filename, GetWin32ErrorString(err));
        }
        return false;
    }
    RG_DEFER { CloseHandle(h); };

    BY_HANDLE_FILE_INFORMATION attr;
    if (!GetFileInformationByHandle(h, &attr)) {
        LogError("Cannot stat file '%1': %2", filename, GetWin32ErrorString());
        return false;
    }

    out_info->type = FileAttributesToType(attr.dwFileAttributes);
    out_info->size = ((uint64_t)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
    out_info->mtime = FileTimeToUnixTime(attr.ftLastWriteTime);

    return true;
}

bool RenameFile(const char *src_filename, const char *dest_filename, bool overwrite, bool)
{
    DWORD flags = overwrite ? MOVEFILE_REPLACE_EXISTING : 0;

    if (win32_utf8) {
        if (!MoveFileExA(src_filename, dest_filename, flags))
            goto error;
    } else {
        wchar_t src_filename_w[4096];
        wchar_t dest_filename_w[4096];
        if (ConvertUtf8ToWin32Wide(src_filename, src_filename_w) < 0)
            return false;
        if (ConvertUtf8ToWin32Wide(dest_filename, dest_filename_w) < 0)
            return false;

        if (!MoveFileExW(src_filename_w, dest_filename_w, flags))
            goto error;
    }

    return true;

error:
    LogError("Failed to rename file '%1' to '%2': %3", src_filename, dest_filename, GetWin32ErrorString());
    return false;
}

EnumStatus EnumerateDirectory(const char *dirname, const char *filter, Size max_files,
                              FunctionRef<bool(const char *, FileType)> func)
{
    if (filter) {
        RG_ASSERT(!strpbrk(filter, RG_PATH_SEPARATORS));
    } else {
        filter = "*";
    }

    wchar_t find_filter_w[4096];
    {
        char find_filter[4096];
        if (snprintf(find_filter, RG_SIZE(find_filter), "%s\\%s", dirname, filter) >= RG_SIZE(find_filter)) {
            LogError("Cannot enumerate directory '%1': Path too long", dirname);
            return EnumStatus::Error;
        }

        if (ConvertUtf8ToWin32Wide(find_filter, find_filter_w) < 0)
            return EnumStatus::Error;
    }

    WIN32_FIND_DATAW find_data;
    HANDLE handle = FindFirstFileExW(find_filter_w, FindExInfoBasic, &find_data,
                                     FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (handle == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            // Erase the filter part from the buffer, we are about to exit anyway.
            // And no, I don't want to include wchar.h
            Size len = 0;
            while (find_filter_w[len++]);
            while (len > 0 && find_filter_w[--len] != L'\\');
            find_filter_w[len] = 0;

            DWORD attrib = GetFileAttributesW(find_filter_w);
            if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY))
                return EnumStatus::Complete;
        }

        LogError("Cannot enumerate directory '%1': %2", dirname,
                 GetWin32ErrorString());
        return EnumStatus::Error;
    }
    RG_DEFER { FindClose(handle); };

    Size count = 0;
    do {
        if ((find_data.cFileName[0] == '.' && !find_data.cFileName[1]) ||
                (find_data.cFileName[0] == '.' && find_data.cFileName[1] == '.' && !find_data.cFileName[2]))
            continue;

        if (RG_UNLIKELY(count++ >= max_files && max_files >= 0)) {
            LogError("Partial enumation of directory '%1'", dirname);
            return EnumStatus::Partial;
        }

        char filename[512];
        if (ConvertWin32WideToUtf8(find_data.cFileName, filename) < 0)
            return EnumStatus::Error;

        FileType file_type = FileAttributesToType(find_data.dwFileAttributes);

        if (!func(filename, file_type))
            return EnumStatus::Stopped;
    } while (FindNextFileW(handle, &find_data));

    if (GetLastError() != ERROR_NO_MORE_FILES) {
        LogError("Error while enumerating directory '%1': %2", dirname,
                 GetWin32ErrorString());
        return EnumStatus::Error;
    }

    return EnumStatus::Complete;
}

#else

void SetEnvironmentVar(const char *name, const char *value)
{
    RG_ASSERT(name && name[0] && !strchr(name, '='));
    RG_ASSERT(value);

    RG_CRITICAL(!setenv(name, value, 1), "Failed to set environment variable '%1' to '%2': %3", name, value, strerror(errno));
}

void DeleteEnvironmentVar(const char *name)
{
    RG_ASSERT(name && name[0] && !strchr(name, '='));
    RG_CRITICAL(!unsetenv(name), "Failed to clear environment variable '%1': %2", name, strerror(errno));
}

static FileType FileModeToType(mode_t mode)
{
    if (S_ISDIR(mode)) {
        return FileType::Directory;
    } else if (S_ISREG(mode)) {
        return FileType::File;
    } else if (S_ISBLK(mode) || S_ISCHR(mode)) {
        return FileType::Device;
    } else if (S_ISLNK(mode)) {
        return FileType::Link;
    } else if (S_ISFIFO(mode)) {
        return FileType::Pipe;
    } else if (S_ISSOCK(mode)) {
        return FileType::Socket;
    } else {
        // This... should not happen. But who knows?
        return FileType::File;
    }
}

bool StatFile(const char *filename, unsigned int flags, FileInfo *out_info)
{
    int stat_flags = (flags & (int)StatFlag::FollowSymlink) ? 0 : AT_SYMLINK_NOFOLLOW;

    struct stat sb;
    if (fstatat(AT_FDCWD, filename, &sb, stat_flags) < 0) {
        if (!(flags & (int)StatFlag::IgnoreMissing) || errno != ENOENT) {
            LogError("Cannot stat '%1': %2", filename, strerror(errno));
        }
        return false;
    }

    out_info->type = FileModeToType(sb.st_mode);
    out_info->size = (int64_t)sb.st_size;
#if defined(__linux__)
    out_info->mtime = (int64_t)sb.st_mtim.tv_sec * 1000 +
                                  (int64_t)sb.st_mtim.tv_nsec / 1000000;
#elif defined(__APPLE__)
    out_info->mtime = (int64_t)sb.st_mtimespec.tv_sec * 1000 +
                                  (int64_t)sb.st_mtimespec.tv_nsec / 1000000;
#else
    out_info->mtime = (int64_t)sb.st_mtime * 1000;
#endif

    return true;
}

static bool SyncFileDirectory(const char *filename)
{
    Span<const char> directory = GetPathDirectory(filename);

    char directory0[4096];
    if (directory.len >= RG_SIZE(directory0)) {
        LogError("Failed to sync directory '%1': path too long", directory);
        return false;
    }
    memcpy_safe(directory0, directory.ptr, directory.len);
    directory0[directory.len] = 0;

    int dirfd = RG_POSIX_RESTART_EINTR(open(directory0, O_RDONLY | O_CLOEXEC), < 0);
    if (dirfd < 0) {
        LogError("Failed to sync directory '%1': %2", directory, strerror(errno));
        return false;
    }
    RG_DEFER { close(dirfd); };

    if (fsync(dirfd) < 0) {
        LogError("Failed to sync directory '%1': %2", directory, strerror(errno));
        return false;
    }

    return true;
}

bool RenameFile(const char *src_filename, const char *dest_filename, bool overwrite, bool sync)
{
    int fd = -1;
    if (!overwrite) {
        fd = open(dest_filename, O_CREAT | O_EXCL, 0644);
        if (fd < 0) {
            if (errno == EEXIST) {
                LogError("File '%1' already exists", dest_filename);
            } else {
                LogError("Failed to rename '%1' to '%2': %3", src_filename, dest_filename, strerror(errno));
            }
            return false;
        }
    }
    RG_DEFER { close(fd); };

    // Rename the file
    if (rename(src_filename, dest_filename) < 0) {
        LogError("Failed to rename '%1' to '%2': %3", src_filename, dest_filename, strerror(errno));
        return false;
    }

    // Not much we can do if fsync fails (I think), so ignore errors.
    // Hope for the best: that's the spirit behind the POSIX filesystem API (...).
    if (sync) {
        SyncFileDirectory(src_filename);
        SyncFileDirectory(dest_filename);
    }

    return true;
}

EnumStatus EnumerateDirectory(const char *dirname, const char *filter, Size max_files,
                              FunctionRef<bool(const char *, FileType)> func)
{
    DIR *dirp = RG_POSIX_RESTART_EINTR(opendir(dirname), == nullptr);
    if (!dirp) {
        LogError("Cannot enumerate directory '%1': %2", dirname, strerror(errno));
        return EnumStatus::Error;
    }
    RG_DEFER { closedir(dirp); };

    // Avoid random failure in empty directories
    errno = 0;

    Size count = 0;
    dirent *dent;
    while ((dent = readdir(dirp))) {
        if ((dent->d_name[0] == '.' && !dent->d_name[1]) ||
                (dent->d_name[0] == '.' && dent->d_name[1] == '.' && !dent->d_name[2]))
            continue;

        if (!filter || !fnmatch(filter, dent->d_name, FNM_PERIOD)) {
            if (RG_UNLIKELY(count++ >= max_files && max_files >= 0)) {
                LogError("Partial enumation of directory '%1'", dirname);
                return EnumStatus::Partial;
            }

            FileType file_type;
#ifdef _DIRENT_HAVE_D_TYPE
            if (dent->d_type != DT_UNKNOWN) {
                switch (dent->d_type) {
                    case DT_DIR: { file_type = FileType::Directory; } break;
                    case DT_REG: { file_type = FileType::File; } break;
                    case DT_LNK: { file_type = FileType::Link; } break;
                    case DT_BLK:
                    case DT_CHR: { file_type = FileType::Device; } break;
                    case DT_FIFO: { file_type = FileType::Pipe; } break;
                    case DT_SOCK: { file_type = FileType::Socket; } break;

                    default: {
                        // This... should not happen. But who knows?
                        file_type = FileType::File;
                    } break;
                }
            } else
#endif
            {
                struct stat sb;
                if (fstatat(dirfd(dirp), dent->d_name, &sb, AT_SYMLINK_NOFOLLOW) < 0) {
                    LogError("Ignoring file '%1' in '%2' (stat failed)", dent->d_name, dirname);
                    continue;
                }

                file_type = FileModeToType(sb.st_mode);
            }

            if (!func(dent->d_name, file_type))
                return EnumStatus::Stopped;
        }

        errno = 0;
    }

    if (errno) {
        LogError("Error while enumerating directory '%1': %2", dirname, strerror(errno));
        return EnumStatus::Error;
    }

    return EnumStatus::Complete;
}

#endif

bool EnumerateFiles(const char *dirname, const char *filter, Size max_depth, Size max_files,
                    Allocator *str_alloc, HeapArray<const char *> *out_files)
{
    RG_DEFER_NC(out_guard, len = out_files->len) { out_files->RemoveFrom(len); };

    EnumStatus status = EnumerateDirectory(dirname, nullptr, max_files,
                                           [&](const char *basename, FileType file_type) {
        switch (file_type) {
            case FileType::Directory: {
                if (max_depth) {
                    const char *sub_directory = Fmt(str_alloc, "%1%/%2", dirname, basename).ptr;
                    return EnumerateFiles(sub_directory, filter, std::max((Size)-1, max_depth - 1),
                                          max_files, str_alloc, out_files);
                }
            } break;

            case FileType::File:
            case FileType::Link: {
                if (!filter || MatchPathName(basename, filter)) {
                    const char *filename = Fmt(str_alloc, "%1%/%2", dirname, basename).ptr;
                    out_files->Append(filename);
                }
            } break;

            case FileType::Device:
            case FileType::Pipe:
            case FileType::Socket: {} break;
        }

        return true;
    });
    if (status == EnumStatus::Error)
        return false;

    out_guard.Disable();
    return true;
}

bool IsDirectoryEmpty(const char *dirname)
{
    EnumStatus status = EnumerateDirectory(dirname, nullptr, -1, [](const char *, FileType) { return false; });
    return status == EnumStatus::Complete;
}

bool TestFile(const char *filename)
{
    FileInfo file_info;
    return StatFile(filename, (int)StatFlag::IgnoreMissing, &file_info);
}

bool TestFile(const char *filename, FileType type)
{
    RG_ASSERT(type != FileType::Link);

    FileInfo file_info;
    if (!StatFile(filename, (int)StatFlag::IgnoreMissing, &file_info))
        return false;

    // Don't follow, but don't warn if we just wanted a file
    if (file_info.type == FileType::Link) {
        file_info.type = FileType::File;
    }

    if (type != file_info.type) {
        switch (type) {
            case FileType::Directory: { LogError("Path '%1' is not a directory", filename); } break;
            case FileType::File: { LogError("Path '%1' is not a file", filename); } break;
            case FileType::Device: { LogError("Path '%1' is not a device", filename); } break;
            case FileType::Pipe: { LogError("Path '%1' is not a pipe", filename); } break;
            case FileType::Socket: { LogError("Path '%1' is not a socket", filename); } break;

            case FileType::Link: { RG_UNREACHABLE(); } break;
        }

        return false;
    }

    return true;
}

static Size MatchPathItem(const char *path, const char *spec)
{
    Size i = 0;

    while (spec[i] && spec[i] != '*') {
        switch (spec[i]) {
            case '?': {
                if (!path[i] || IsPathSeparator(path[i]))
                    return -1;
            } break;

#ifdef _WIN32
            case '\\':
            case '/': {
                if (!IsPathSeparator(path[i]))
                    return -1;
            } break;

            default: {
                // XXX: Use proper Unicode/locale case-folding? Or is this enough?
                if (LowerAscii(path[i]) != LowerAscii(spec[i]))
                    return -1;
            } break;
#else
            default: {
                if (path[i] != spec[i])
                    return -1;
            } break;
#endif
        }

        i++;
    }

    return i;
}

bool MatchPathName(const char *path, const char *spec)
{
    // Match head
    {
        Size match_len = MatchPathItem(path, spec);

        if (match_len < 0) {
            return false;
        } else {
            // Fast path (no wildcard)
            if (!spec[match_len])
                return !path[match_len];

            path += match_len;
            spec += match_len;
        }
    }

    // Find tail
    const char *tail = strrchr(spec, '*') + 1;

    // Match remaining items
    while (spec[0] == '*') {
        bool superstar = (spec[1] == '*');
        while (spec[0] == '*') {
            spec++;
        }

        for (;;) {
            Size match_len = MatchPathItem(path, spec);

            // We need to be greedy for the last wildcard, or we may not reach the tail
            if (match_len < 0 || (spec == tail && path[match_len])) {
                if (!path[0])
                    return false;
                if (!superstar && IsPathSeparator(path[0]))
                    return false;
                path++;
            } else {
                path += match_len;
                spec += match_len;

                break;
            }
        }
    }

    return true;
}

bool MatchPathSpec(const char *path, const char *spec)
{
    Span<const char> path2 = path;

    do {
        const char *it = SplitStrReverseAny(path2, RG_PATH_SEPARATORS, &path2).ptr;

        if (MatchPathName(it, spec))
            return true;
    } while (path2.len);

    return false;
}

bool FindExecutableInPath(Span<const char> paths, const char *name, Allocator *alloc, const char **out_path)
{
    RG_ASSERT(alloc || !out_path);

    // Fast path
    if (strpbrk(name, RG_PATH_SEPARATORS)) {
        if (!TestFile(name, FileType::File))
            return false;

        if (out_path) {
            *out_path = DuplicateString(name, alloc).ptr;
        }
        return true;
    }

    while (paths.len) {
        Span<const char> path = SplitStr(paths, RG_PATH_DELIMITER, &paths);

        LocalArray<char, 4096> buf;
        buf.len = Fmt(buf.data, "%1%/%2", path, name).len;

#ifdef _WIN32
        static const Span<const char> extensions[] = {".com", ".exe", ".bat", ".cmd"};

        for (Span<const char> ext: extensions) {
            if (RG_LIKELY(ext.len < buf.Available() - 1)) {
                memcpy_safe(buf.end(), ext.ptr, ext.len + 1);

                if (TestFile(buf.data)) {
                    if (out_path) {
                        *out_path = DuplicateString(buf.data, alloc).ptr;
                    }
                    return true;
                }
            }
        }
#else
        if (RG_LIKELY(buf.len < RG_SIZE(buf.data) - 1) && TestFile(buf.data)) {
            if (out_path) {
                *out_path = DuplicateString(buf.data, alloc).ptr;
            }
            return true;
        }
#endif
    }

    return false;
}

bool FindExecutableInPath(const char *name, Allocator *alloc, const char **out_path)
{
    RG_ASSERT(alloc || !out_path);

    // Fast path
    if (strpbrk(name, RG_PATH_SEPARATORS)) {
        if (!TestFile(name, FileType::File))
            return false;

        if (out_path) {
            *out_path = DuplicateString(name, alloc).ptr;
        }
        return true;
    }

#ifdef _WIN32
    LocalArray<char, 16384> env_buf;
    Span<const char> paths;
    if (win32_utf8) {
        paths = getenv("PATH");
    } else {
        wchar_t buf_w[RG_SIZE(env_buf.data)];
        DWORD len = GetEnvironmentVariableW(L"PATH", buf_w, RG_LEN(buf_w));

        if (!len && GetLastError() != ERROR_ENVVAR_NOT_FOUND) {
            LogError("Failed to get PATH environment variable: %1", GetWin32ErrorString());
            return false;
        } else if (len >= RG_LEN(buf_w)) {
            LogError("Failed to get PATH environment variable: buffer to small");
            return false;
        }
        buf_w[len] = 0;

        env_buf.len = ConvertWin32WideToUtf8(buf_w, env_buf.data);
        if (env_buf.len < 0)
            return false;

        paths = env_buf;
    }
#else
    Span<const char> paths = getenv("PATH");
#endif

    return FindExecutableInPath(paths, name, alloc, out_path);
}

bool SetWorkingDirectory(const char *directory)
{
#ifdef _WIN32
    if (!win32_utf8) {
        wchar_t directory_w[4096];
        if (ConvertUtf8ToWin32Wide(directory, directory_w) < 0)
            return false;

        if (!SetCurrentDirectoryW(directory_w)) {
            LogError("Failed to set current directory to '%1': %2", directory, GetWin32ErrorString());
            return false;
        }

        return true;
    }
#endif

    if (chdir(directory) < 0) {
        LogError("Failed to set current directory to '%1': %2", directory, strerror(errno));
        return false;
    }

    return true;
}

const char *GetWorkingDirectory()
{
    static RG_THREAD_LOCAL char buf[4096];

#ifdef _WIN32
    if (!win32_utf8) {
        wchar_t buf_w[RG_SIZE(buf)];
        DWORD ret = GetCurrentDirectoryW(RG_SIZE(buf_w), buf_w);
        RG_ASSERT(ret && ret <= RG_SIZE(buf_w));

        Size str_len = ConvertWin32WideToUtf8(buf_w, buf);
        RG_ASSERT(str_len >= 0);

        return buf;
    }
#endif

    const char *ptr = getcwd(buf, RG_SIZE(buf));
    RG_ASSERT(ptr);

    return buf;
}

#ifdef __OpenBSD__
RG_INIT(ExePathOpenBSD)
{
    // This can depend on PATH, which could change during execution
    // so we want to cache the result as soon as possible.
    GetApplicationExecutable();
}
#endif

const char *GetApplicationExecutable()
{
#if defined(_WIN32)
    static char executable_path[4096];

    if (!executable_path[0]) {
        if (win32_utf8) {
            Size path_len = (Size)GetModuleFileNameA(nullptr, executable_path, RG_SIZE(executable_path));
            RG_ASSERT(path_len && path_len < RG_SIZE(executable_path));
        } else {
            wchar_t path_w[RG_SIZE(executable_path)];
            Size path_len = (Size)GetModuleFileNameW(nullptr, path_w, RG_SIZE(path_w));
            RG_ASSERT(path_len && path_len < RG_LEN(path_w));

            Size str_len = ConvertWin32WideToUtf8(path_w, executable_path);
            RG_ASSERT(str_len >= 0);
        }
    }

    return executable_path;
#elif defined(__APPLE__)
    static char executable_path[4096];

    if (!executable_path[0]) {
        uint32_t buffer_size = RG_SIZE(executable_path);
        int ret = _NSGetExecutablePath(executable_path, &buffer_size);
        RG_ASSERT(!ret);

        char *path_buf = realpath(executable_path, nullptr);
        RG_ASSERT(path_buf);
        RG_ASSERT(strlen(path_buf) < RG_SIZE(executable_path));

        CopyString(path_buf, executable_path);
        free(path_buf);
    }

    return executable_path;
#elif defined(__linux__)
    static char executable_path[4096];

    if (!executable_path[0]) {
        ssize_t ret = readlink("/proc/self/exe", executable_path, RG_SIZE(executable_path));
        RG_ASSERT(ret > 0 && ret < RG_SIZE(executable_path));
    }

    return executable_path;
#elif defined(__OpenBSD__)
    static char executable_path[4096];

    if (!executable_path[0]) {
        int name[4] = {CTL_KERN, KERN_PROC_ARGS, getpid(), KERN_PROC_ARGV};

        size_t argc;
        {
            int ret = sysctl(name, RG_LEN(name), nullptr, &argc, NULL, 0);
            RG_ASSERT(ret >= 0);
            RG_ASSERT(argc >= 1);
        }

        HeapArray<char *> argv;
        {
            argv.AppendDefault(argc);
            int ret = sysctl(name, RG_LEN(name), argv.ptr, &argc, nullptr, 0);
            RG_ASSERT(ret >= 0);
        }

        if (PathIsAbsolute(argv[0])) {
            RG_ASSERT(strlen(argv[0]) < RG_SIZE(executable_path));

            CopyString(argv[0], executable_path);
        } else {
            const char *path;
            bool success = FindExecutableInPath(argv[0], GetDefaultAllocator(), &path);
            RG_ASSERT(success);
            RG_ASSERT(strlen(path) < RG_SIZE(executable_path));

            CopyString(path, executable_path);
            Allocator::Release(nullptr, (void *)path, -1);
        }
    }

    return executable_path;
#elif defined(__FreeBSD__)
    static char executable_path[4096];

    if (!executable_path[0]) {
        int name[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
        size_t len = sizeof(executable_path);

        int ret = sysctl(name, RG_LEN(name), executable_path, &len, NULL, 0);
        RG_ASSERT(ret >= 0);
        RG_ASSERT(len < RG_SIZE(executable_path));
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
    static char executable_dir[4096];

    if (!executable_dir[0]) {
        const char *executable_path = GetApplicationExecutable();
        Size dir_len = (Size)strlen(executable_path);
        while (dir_len && !IsPathSeparator(executable_path[--dir_len]));
        memcpy_safe(executable_dir, executable_path, (size_t)dir_len);
        executable_dir[dir_len] = 0;
    }

    return executable_dir;
}

Span<const char> GetPathDirectory(Span<const char> filename)
{
    Span<const char> directory;
    SplitStrReverseAny(filename, RG_PATH_SEPARATORS, &directory);

    return directory.len ? directory : ".";
}

// Names starting with a dot are not considered to be an extension (POSIX hidden files)
Span<const char> GetPathExtension(Span<const char> filename, CompressionType *out_compression_type)
{
    filename = SplitStrReverseAny(filename, RG_PATH_SEPARATORS);

    Span<const char> extension = {};
    const auto consume_next_extension = [&]() {
        Span<const char> part = SplitStrReverse(filename, '.', &filename);

        if (part.ptr > filename.ptr) {
            extension = MakeSpan(part.ptr - 1, part.len + 1);
        } else {
            extension = MakeSpan(part.end(), 0);
        }
    };

    consume_next_extension();
    if (out_compression_type) {
        if (TestStr(extension, ".gz")) {
            *out_compression_type = CompressionType::Gzip;
            consume_next_extension();
        } else {
            *out_compression_type = CompressionType::None;
        }
    }

    return extension;
}

CompressionType GetPathCompression(Span<const char> filename)
{
    CompressionType compression_type;
    GetPathExtension(filename, &compression_type);
    return compression_type;
}

Span<char> NormalizePath(Span<const char> path, Span<const char> root_directory, Allocator *alloc)
{
    RG_ASSERT(alloc);

    if (!path.len && !root_directory.len)
        return Fmt(alloc, "");

    HeapArray<char> buf(alloc);
    Size parts_count = 0;

    const auto append_normalized_path = [&](Span<const char> path) {
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

    if (root_directory.len && !PathIsAbsolute(path)) {
        append_normalized_path(root_directory);
    }
    append_normalized_path(path);

    if (!buf.len) {
        buf.Append('.');
    } else if (buf.len == 1 && IsPathSeparator(buf[0])) {
        // Root '/', keep as-is
    } else {
        // Strip last separator
        buf.len--;
    }

    // NUL terminator
    buf.Trim(1);
    buf.ptr[buf.len] = 0;

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

static bool CheckForDumbTerm()
{
    static bool init = false;
    static bool dumb = false;

    if (!init) {
        const char *term = getenv("TERM");

        dumb |= term && TestStr(term, "dumb");
        dumb |= !!getenv("NO_COLOR");
    }

    return dumb;
}

#ifdef _WIN32

int OpenDescriptor(const char *filename, unsigned int flags)
{
    DWORD access = 0;
    DWORD share = 0;
    DWORD creation = 0;
    int oflags = -1;
    switch (flags & ((int)OpenFileFlag::Read |
                     (int)OpenFileFlag::Write |
                     (int)OpenFileFlag::Append)) {
        case (int)OpenFileFlag::Read: {
            access = GENERIC_READ;
            share = FILE_SHARE_READ | FILE_SHARE_WRITE;
            creation = OPEN_EXISTING;

            oflags = _O_RDONLY | _O_BINARY | _O_NOINHERIT;
        } break;
        case (int)OpenFileFlag::Write: {
            access = GENERIC_WRITE;
            share = FILE_SHARE_READ | FILE_SHARE_WRITE;
            creation = (flags & (int)OpenFileFlag::Exclusive) ? CREATE_NEW : CREATE_ALWAYS;

            oflags = _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY | _O_NOINHERIT;
        } break;
        case (int)OpenFileFlag::Read | (int)OpenFileFlag::Write: {
            access = GENERIC_READ | GENERIC_WRITE;
            share = FILE_SHARE_READ | FILE_SHARE_WRITE;
            creation = (flags & (int)OpenFileFlag::Exclusive) ? CREATE_NEW : CREATE_ALWAYS;

            oflags = _O_RDWR | _O_CREAT | _O_TRUNC | _O_BINARY | _O_NOINHERIT;
        } break;
        case (int)OpenFileFlag::Append: {
            access = GENERIC_WRITE;
            share = FILE_SHARE_READ | FILE_SHARE_WRITE;
            creation = (flags & (int)OpenFileFlag::Exclusive) ? CREATE_NEW : CREATE_ALWAYS;

            oflags = _O_WRONLY | _O_CREAT | _O_APPEND | _O_BINARY | _O_NOINHERIT;
        } break;
    }
    RG_ASSERT(oflags >= 0);

    if (flags & (int)OpenFileFlag::Exclusive) {
        oflags |= (int)_O_EXCL;
    }
    share |= FILE_SHARE_DELETE;

    HANDLE h;
    if (win32_utf8) {
        h = CreateFileA(filename, access, share, nullptr, creation, FILE_ATTRIBUTE_NORMAL, nullptr);
    } else {
        wchar_t filename_w[4096];
        if (ConvertUtf8ToWin32Wide(filename, filename_w) < 0)
            return -1;

        h = CreateFileW(filename_w, access, share, nullptr, creation, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();

        if (err == ERROR_FILE_EXISTS) {
            LogError("File '%1' already exists", filename);
        } else {
            LogError("Cannot open '%1': %2", filename, GetWin32ErrorString(err));
        }

        return -1;
    }

    int fd = _open_osfhandle((intptr_t)h, oflags);
    if (fd < 0) {
        LogError("Cannot open '%1': %2", filename, strerror(errno));
        CloseHandle(h);

        return -1;
    }

    return fd;
}

FILE *OpenFile(const char *filename, unsigned int flags)
{
    char mode[16] = {};
    switch (flags & ((int)OpenFileFlag::Read |
                     (int)OpenFileFlag::Write |
                     (int)OpenFileFlag::Append)) {
        case (int)OpenFileFlag::Read: { CopyString("rbc", mode); } break;
        case (int)OpenFileFlag::Write: { CopyString("wbc", mode); } break;
        case (int)OpenFileFlag::Read | (int)OpenFileFlag::Write: { CopyString("w+bc", mode); } break;
        case (int)OpenFileFlag::Append: { CopyString("abc", mode); } break;
    }
    RG_ASSERT(mode[0]);

#if !defined(__GNUC__) || defined(__clang__)
    // The N modifier does not work in MinGW builds (because of old msvcrt?)
    strcat(mode, "N");
#endif

    int fd = OpenDescriptor(filename, flags);
    if (fd < 0)
        return nullptr;

    FILE *fp = _fdopen(fd, mode);
    if (!fp) {
        LogError("Cannot open '%1': %2", filename, strerror(errno));
        _close(fd);
    }

    return fp;
}

bool FlushFile(FILE *fp, const char *filename)
{
    RG_ASSERT(filename);

    if (fflush(fp) != 0) {
        LogError("Failed to sync '%1': %2", filename, strerror(errno));
        return false;
    }

    return true;
}

bool FileIsVt100(FILE *fp)
{
    static RG_THREAD_LOCAL FILE *cache_fp;
    static RG_THREAD_LOCAL bool cache_vt100;

    if (CheckForDumbTerm())
        return false;

    // Fast path, for repeated calls (such as Print in a loop)
    if (fp == cache_fp)
        return cache_vt100;

    if (fp == stdout || fp == stderr) {
        HANDLE h = (HANDLE)_get_osfhandle(_fileno(fp));

        DWORD console_mode;
        if (GetConsoleMode(h, &console_mode)) {
            static bool enable_emulation = [&]() {
                bool emulation = console_mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING;

                if (!emulation) {
                    // Enable VT100 escape sequences, introduced in Windows 10
                    DWORD new_mode = console_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                    emulation = SetConsoleMode(h, new_mode);

                    if (emulation) {
                        static HANDLE exit_handle = h;
                        static DWORD exit_mode = console_mode;

                        atexit([]() { SetConsoleMode(exit_handle, exit_mode); });
                    } else {
                        // Try ConEmu ANSI support for Windows < 10
                        const char *conemuansi_str = getenv("ConEmuANSI");
                        emulation = conemuansi_str && TestStr(conemuansi_str, "ON");
                    }
                }

                if (emulation && win32_utf8) {
                    SetConsoleCP(CP_UTF8); // Does not work yet, but it might some day
                    SetConsoleOutputCP(CP_UTF8);
                }

                return emulation;
            }();

            cache_vt100 = enable_emulation;
        } else {
            cache_vt100 = false;
        }
    } else {
        cache_vt100 = false;
    }

    cache_fp = fp;
    return cache_vt100;
}

bool MakeDirectory(const char *directory, bool error_if_exists)
{
    if (win32_utf8) {
        if (!CreateDirectoryA(directory, nullptr))
            goto error;
    } else {
        wchar_t directory_w[4096];
        if (ConvertUtf8ToWin32Wide(directory, directory_w) < 0)
            return false;

        if (!CreateDirectoryW(directory_w, nullptr))
            goto error;
    }

    return true;

error:
    DWORD err = GetLastError();

    if (err != ERROR_ALREADY_EXISTS || error_if_exists) {
        LogError("Cannot create directory '%1': %2", directory, GetWin32ErrorString(err));
        return false;
    } else {
        return true;
    }
}

bool MakeDirectoryRec(Span<const char> directory)
{
    LocalArray<wchar_t, 4096> buf_w;
    buf_w.len = ConvertUtf8ToWin32Wide(directory, buf_w.data);
    if (buf_w.len < 0)
        return false;

    // Simple case: directory already exists or only last level was missing
    if (!CreateDirectoryW(buf_w.data, nullptr)) {
        DWORD err = GetLastError();

        if (err == ERROR_ALREADY_EXISTS) {
            return true;
        } else if (err != ERROR_PATH_NOT_FOUND) {
            LogError("Cannot create directory '%1': %2", directory, strerror(errno));
            return false;
        }
    }

    for (Size offset = 1, parts = 0; offset <= buf_w.len; offset++) {
        if (!buf_w.data[offset] || buf_w[offset] == L'\\' || buf_w[offset] == L'/') {
            buf_w.data[offset] = 0;
            parts++;

            if (!CreateDirectoryW(buf_w.data, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
                Size offset8 = 0;
                while (offset8 < directory.len) {
                    parts -= IsPathSeparator(directory[offset8]);
                    if (!parts)
                        break;
                    offset8++;
                }

                LogError("Cannot create directory '%1': %2",
                         directory.Take(0, offset8), GetWin32ErrorString());
                return false;
            }

            buf_w.data[offset] = L'\\';
        }
    }

    return true;
}

bool UnlinkDirectory(const char *directory, bool error_if_missing)
{
    if (win32_utf8) {
        if (!RemoveDirectoryA(directory))
            goto error;
    } else {
        wchar_t directory_w[4096];
        if (ConvertUtf8ToWin32Wide(directory, directory_w) < 0)
            return false;

        if (!RemoveDirectoryW(directory_w))
            goto error;
    }

    return true;

error:
    DWORD err = GetLastError();

    if (err != ERROR_FILE_NOT_FOUND || error_if_missing) {
        LogError("Failed to remove directory '%1': %2", directory, GetWin32ErrorString(err));
        return false;
    } else {
        return true;
    }
}

bool UnlinkFile(const char *filename, bool error_if_missing)
{
    if (win32_utf8) {
        if (!DeleteFileA(filename))
            goto error;
    } else {
        wchar_t filename_w[4096];
        if (ConvertUtf8ToWin32Wide(filename, filename_w) < 0)
            return false;

        if (!DeleteFileW(filename_w))
            goto error;
    }

    return true;

error:
    DWORD err = GetLastError();

    if (err != ERROR_FILE_NOT_FOUND || error_if_missing) {
        LogError("Failed to remove file '%1': %2", filename, GetWin32ErrorString());
        return false;
    } else {
        return true;
    }
}

#else

int OpenDescriptor(const char *filename, unsigned int flags)
{
    int oflags = -1;
    switch (flags & ((int)OpenFileFlag::Read |
                     (int)OpenFileFlag::Write |
                     (int)OpenFileFlag::Append)) {
        case (int)OpenFileFlag::Read: { oflags = O_RDONLY | O_CLOEXEC; } break;
        case (int)OpenFileFlag::Write: { oflags = O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC; } break;
        case (int)OpenFileFlag::Read | (int)OpenFileFlag::Write: { oflags = O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC; } break;
        case (int)OpenFileFlag::Append: { oflags = O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC; } break;
    }
    RG_ASSERT(oflags >= 0);

    if (flags & (int)OpenFileFlag::Exclusive) {
        oflags |= O_EXCL;
    }

    int fd = RG_POSIX_RESTART_EINTR(open(filename, oflags, 0644), < 0);
    if (fd < 0) {
        if (errno == EEXIST) {
            LogError("File '%1' already exists", filename);
        } else {
            LogError("Cannot open '%1': %2", filename, strerror(errno));
        }
        return -1;
    }

    return fd;
}

FILE *OpenFile(const char *filename, unsigned int flags)
{
    const char *mode = nullptr;
    switch (flags & ((int)OpenFileFlag::Read |
                     (int)OpenFileFlag::Write |
                     (int)OpenFileFlag::Append)) {
        case (int)OpenFileFlag::Read: { mode = "rbe"; } break;
        case (int)OpenFileFlag::Write: { mode = "wbe"; } break;
        case (int)OpenFileFlag::Read | (int)OpenFileFlag::Write: { mode = "w+be"; } break;
        case (int)OpenFileFlag::Append: { mode = "abe"; } break;
    }
    RG_ASSERT(mode);

    int fd = OpenDescriptor(filename, flags);
    if (fd < 0)
        return nullptr;

    FILE *fp = fdopen(fd, mode);
    if (!fp) {
        LogError("Cannot open '%1': %2", filename, strerror(errno));
        close(fd);
    }

    return fp;
}

bool FlushFile(FILE *fp, const char *filename)
{
    RG_ASSERT(filename);

#ifdef __APPLE__
    if ((fflush(fp) != 0 || fsync(fileno(fp)) < 0) &&
            errno != EINVAL && errno != ENOTSUP) {
#else
    if ((fflush(fp) != 0 || fsync(fileno(fp)) < 0) && errno != EINVAL) {
#endif
        LogError("Failed to sync '%1': %2", filename, strerror(errno));
        return false;
    }

    return true;
}

bool FileIsVt100(FILE *fp)
{
    static RG_THREAD_LOCAL FILE *cache_fp;
    static RG_THREAD_LOCAL bool cache_vt100;

    if (CheckForDumbTerm())
        return false;

#ifdef __EMSCRIPTEN__
    static bool win32 = ([]() {
        int win32 = EM_ASM_INT({
            try {
                const os = require('os');

                var win32 = (os.platform() === 'win32');
                return win32 ? 1 : 0;
            } catch (err) {
                return 0;
            }
        });

        return (bool)win32;
    })();

    if (win32)
        return false;
#endif

    // Fast path, for repeated calls (such as Print in a loop)
    if (fp == cache_fp)
        return cache_vt100;

    cache_fp = fp;
    cache_vt100 = isatty(fileno(fp));

    return cache_vt100;
}

bool MakeDirectory(const char *directory, bool error_if_exists)
{
    if (mkdir(directory, 0755) < 0 && (errno != EEXIST || error_if_exists)) {
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
    memcpy_safe(buf, directory.ptr, directory.len);
    buf[directory.len] = 0;

    // Simple case: directory already exists or only last level was missing
    if (mkdir(buf, 0755) < 0) {
        if (errno == EEXIST) {
            return true;
        } else if (errno != ENOENT) {
            LogError("Cannot create directory '%1': %2", buf, strerror(errno));
            return false;
        }
    }

    for (Size offset = 1; offset <= directory.len; offset++) {
        if (!buf[offset] || IsPathSeparator(buf[offset])) {
            buf[offset] = 0;

            if (mkdir(buf, 0755) < 0 && errno != EEXIST) {
                LogError("Cannot create directory '%1': %2", buf, strerror(errno));
                return false;
            }

            buf[offset] = *RG_PATH_SEPARATORS;
        }
    }

    return true;
}

bool UnlinkDirectory(const char *directory, bool error_if_missing)
{
    if (rmdir(directory) < 0 && (errno != ENOENT || error_if_missing)) {
        LogError("Failed to remove directory '%1': %2", directory, strerror(errno));
        return false;
    }

    return true;
}

bool UnlinkFile(const char *filename, bool error_if_missing)
{
    if (unlink(filename) < 0 && (errno != ENOENT || error_if_missing)) {
        LogError("Failed to remove file '%1': %2", filename, strerror(errno));
        return false;
    }

    return true;
}

#endif

bool EnsureDirectoryExists(const char *filename)
{
    Span<const char> directory = GetPathDirectory(filename);
    return MakeDirectoryRec(directory);
}

#ifdef _WIN32

static HANDLE console_ctrl_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
static bool ignore_ctrl_event = false;

static BOOL CALLBACK ConsoleCtrlHandler(DWORD)
{
    SetEvent(console_ctrl_event);
    return (BOOL)ignore_ctrl_event;
}

static bool InitConsoleCtrlHandler()
{
    static std::once_flag flag;

    static bool success;
    std::call_once(flag, []() { success = SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE); });

    if (!success) {
        LogError("SetConsoleCtrlHandler() failed: %1", GetWin32ErrorString());
    }
    return success;
}

bool CreateOverlappedPipe(bool overlap0, bool overlap1, PipeMode mode, HANDLE out_handles[2])
{
    static LONG pipe_idx;

    HANDLE handles[2] = {};
    RG_DEFER_N(handle_guard) {
        CloseHandleSafe(&handles[0]);
        CloseHandleSafe(&handles[1]);
    };

    char pipe_name[128];
    do {
        Fmt(pipe_name, "\\\\.\\Pipe\\libcc.%1.%2",
            GetCurrentProcessId(), InterlockedIncrement(&pipe_idx));

        DWORD open_mode = PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | (overlap0 ? FILE_FLAG_OVERLAPPED : 0);
        DWORD pipe_mode = PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
        switch (mode) {
            case PipeMode::Byte: { pipe_mode |= PIPE_TYPE_BYTE; } break;
            case PipeMode::Message: { pipe_mode |= PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE; } break;
        }

        handles[0] = CreateNamedPipeA(pipe_name, open_mode, pipe_mode, 1, 8192, 8192, 0, nullptr);
        if (!handles[0] && GetLastError() != ERROR_ACCESS_DENIED) {
            LogError("Failed to create pipe: %1", GetWin32ErrorString());
            return false;
        }
    } while (!handles[0]);

    handles[1] = CreateFileA(pipe_name, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL | (overlap1 ? FILE_FLAG_OVERLAPPED : 0), nullptr);
    if (handles[1] == INVALID_HANDLE_VALUE) {
        LogError("Failed to create pipe: %1", GetWin32ErrorString());
        return false;
    }

    if (mode == PipeMode::Message) {
        DWORD value = PIPE_READMODE_MESSAGE;
        if (!SetNamedPipeHandleState(handles[1], &value, nullptr, nullptr)) {
            LogError("Failed to switch pipe to message mode: %1", GetWin32ErrorString());
            return false;
        }
    }

    handle_guard.Disable();
    out_handles[0] = handles[0];
    out_handles[1] = handles[1];
    return true;
}

void CloseHandleSafe(HANDLE *handle_ptr)
{
    if (*handle_ptr && *handle_ptr != INVALID_HANDLE_VALUE) {
        CloseHandle(*handle_ptr);
    }

    *handle_ptr = nullptr;
}

struct PendingIO {
    OVERLAPPED ov = {}; // Keep first

    bool pending = false;
    DWORD err = 0;
    Size len = -1;

    static void CALLBACK CompletionHandler(DWORD err, DWORD len, OVERLAPPED *ov)
    {
        PendingIO *self = (PendingIO *)ov;

        self->pending = false;
        self->err = err;
        self->len = err ? -1 : len;
    }
};

bool ExecuteCommandLine(const char *cmd_line, FunctionRef<Span<const uint8_t>()> in_func,
                        FunctionRef<void(Span<uint8_t> buf)> out_func, int *out_code)
{
    STARTUPINFOW si = {};

    // Convert command line
    Span<wchar_t> cmd_line_w;
    cmd_line_w.len = 4 * strlen(cmd_line) + 2;
    cmd_line_w.ptr = (wchar_t *)Allocator::Allocate(nullptr, cmd_line_w.len);
    RG_DEFER { Allocator::Release(nullptr, cmd_line_w.ptr, cmd_line_w.len); };
    if (ConvertUtf8ToWin32Wide(cmd_line, cmd_line_w) < 0)
        return false;

    // Detect CTRL+C and CTRL+BREAK events
    if (!InitConsoleCtrlHandler())
        return false;

    // Neither GenerateConsoleCtrlEvent() or TerminateProcess() manage to fully kill Clang on
    // Windows (in highly-parallel builds) after CTRL-C, with occasionnal suspended child
    // processes remaining alive. Furthermore, processes killed by GenerateConsoleCtrlEvent()
    // can trigger "MessageBox" errors, unless SetErrorMode() is used.
    //
    // TerminateJobObject() is a bit brutal, but it takes care of these issues.
    HANDLE job_handle = CreateJobObject(nullptr, nullptr);
    if (!job_handle) {
        LogError("Failed to create job object: %1", GetWin32ErrorString());
        return false;
    }
    RG_DEFER { CloseHandleSafe(&job_handle); };

    // If I die, everyone dies!
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

        if (!SetInformationJobObject(job_handle, JobObjectExtendedLimitInformation, &limits, RG_SIZE(limits))) {
            LogError("SetInformationJobObject() failed: %1", GetWin32ErrorString());
            return false;
        }
    }

    // Create read pipes
    HANDLE in_pipe[2] = {};
    RG_DEFER {
        CloseHandleSafe(&in_pipe[0]);
        CloseHandleSafe(&in_pipe[1]);
    };
    if (in_func.IsValid() && !CreateOverlappedPipe(false, true, PipeMode::Byte, in_pipe))
        return false;

    // Create write pipes
    HANDLE out_pipe[2] = {};
    RG_DEFER {
        CloseHandleSafe(&out_pipe[0]);
        CloseHandleSafe(&out_pipe[1]);
    };
    if (out_func.IsValid() && !CreateOverlappedPipe(true, false, PipeMode::Byte, out_pipe))
        return false;

    // Start process
    HANDLE process_handle;
    {
        RG_DEFER {
            CloseHandleSafe(&si.hStdInput);
            CloseHandleSafe(&si.hStdOutput);
            CloseHandleSafe(&si.hStdError);
        };
        if (in_func.IsValid() || out_func.IsValid()) {
            if (!DuplicateHandle(GetCurrentProcess(), in_func.IsValid() ? in_pipe[0] : GetStdHandle(STD_INPUT_HANDLE),
                                 GetCurrentProcess(), &si.hStdInput, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                LogError("Failed to duplicate handle: %1", GetWin32ErrorString());
                return false;
            }
            if (!DuplicateHandle(GetCurrentProcess(), out_func.IsValid() ? out_pipe[1] : GetStdHandle(STD_OUTPUT_HANDLE),
                                 GetCurrentProcess(), &si.hStdOutput, 0, TRUE, DUPLICATE_SAME_ACCESS) ||
                !DuplicateHandle(GetCurrentProcess(), out_func.IsValid() ? out_pipe[1] : GetStdHandle(STD_ERROR_HANDLE),
                                 GetCurrentProcess(), &si.hStdError, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                LogError("Failed to duplicate handle: %1", GetWin32ErrorString());
                return false;
            }
            si.dwFlags |= STARTF_USESTDHANDLES;
        }

        PROCESS_INFORMATION pi = {};
        if (!CreateProcessW(nullptr, cmd_line_w.ptr, nullptr, nullptr, TRUE, CREATE_NEW_PROCESS_GROUP,
                            nullptr, nullptr, &si, &pi)) {
            LogError("Failed to start process: %1", GetWin32ErrorString());
            return false;
        }
        if (!AssignProcessToJobObject(job_handle, pi.hProcess)) {
            CloseHandleSafe(&job_handle);
        }

        process_handle = pi.hProcess;
        CloseHandle(pi.hThread);

        CloseHandleSafe(&in_pipe[0]);
        CloseHandleSafe(&out_pipe[1]);
    }
    RG_DEFER { CloseHandleSafe(&process_handle); };

    // Read and write standard process streams
    {
        bool running = true;

        PendingIO proc_in;
        Span<const uint8_t> write_buf = {};
        PendingIO proc_out;
        uint8_t read_buf[4096];

        while (running) {
            // Try to write
            if (in_func.IsValid() && !proc_in.pending) {
                if (!proc_in.err) {
                    if (proc_in.len >= 0) {
                        write_buf.ptr += proc_in.len;
                        write_buf.len -= proc_in.len;
                    }

                    if (!write_buf.len) {
                        write_buf = in_func();
                        RG_ASSERT(write_buf.len >= 0);
                    }

                    if (write_buf.len) {
                        RG_ASSERT(write_buf.len < UINT_MAX);

                        if (!WriteFileEx(in_pipe[1], write_buf.ptr, (DWORD)write_buf.len,
                                         &proc_in.ov, PendingIO::CompletionHandler)) {
                            proc_in.err = GetLastError();
                        }
                    } else {
                        CloseHandleSafe(&in_pipe[1]);
                    }
                }

                if (proc_in.err && proc_in.err != ERROR_BROKEN_PIPE && proc_in.err != ERROR_NO_DATA) {
                    LogError("Failed to write to process: %1", GetWin32ErrorString(proc_in.err));
                }
                proc_in.pending = true;
            }

            // Try to read
            if (out_func.IsValid() && !proc_out.pending) {
                if (!proc_out.err) {
                    if (proc_out.len >= 0) {
                        out_func(MakeSpan(read_buf, proc_out.len));
                        proc_out.len = -1;
                    }

                    if (proc_out.len && !ReadFileEx(out_pipe[0], read_buf, RG_SIZE(read_buf), &proc_out.ov, PendingIO::CompletionHandler)) {
                        proc_out.err = GetLastError();
                    }
                }

                if (proc_out.err && proc_out.err != ERROR_BROKEN_PIPE && proc_out.err != ERROR_NO_DATA) {
                    LogError("Failed to read process output: %1", GetWin32ErrorString(proc_out.err));
                }
                proc_out.pending = true;
            }

            HANDLE events[2] = {
                process_handle,
                console_ctrl_event
            };

            running = (WaitForMultipleObjectsEx(RG_LEN(events), events, FALSE, INFINITE, TRUE) > WAIT_OBJECT_0 + 1);
        }
    }

    // Terminate any remaining I/O
    if (in_pipe[1]) {
        CancelIo(in_pipe[1]);
        CloseHandleSafe(&in_pipe[1]);
    }
    if (out_pipe[0]) {
        CancelIo(out_pipe[0]);
        CloseHandleSafe(&out_pipe[0]);
    }

    // Wait for process exit
    {
        HANDLE events[2] = {
            process_handle,
            console_ctrl_event
        };

        if (WaitForMultipleObjects(RG_LEN(events), events, FALSE, INFINITE) == WAIT_FAILED) {
            LogError("WaitForMultipleObjects() failed: %1", GetWin32ErrorString());
            return false;
        }
    }

    // Get exit code
    DWORD exit_code;
    if (WaitForSingleObject(console_ctrl_event, 0) == WAIT_OBJECT_0) {
        TerminateJobObject(job_handle, STATUS_CONTROL_C_EXIT);
        exit_code = STATUS_CONTROL_C_EXIT;
    } else if (!GetExitCodeProcess(process_handle, &exit_code)) {
        LogError("GetExitCodeProcess() failed: %1", GetWin32ErrorString());
        return false;
    }

    // Mimic POSIX SIGINT
    if (exit_code == STATUS_CONTROL_C_EXIT) {
        exit_code = 130;
    }

    *out_code = (int)exit_code;
    return true;
}

#else

#if defined(__OpenBSD__) || defined(__FreeBSD__)
static const pthread_t main_thread = pthread_self();
#endif
static std::atomic_bool flag_interrupt = false;
static std::atomic_bool explicit_interrupt = false;
static int interrupt_pfd[2] = {-1, -1};

static void SetSignalHandler(int signal, struct sigaction *prev, void (*func)(int))
{
    struct sigaction action = {};

    action.sa_handler = func;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigaction(signal, &action, prev);
}

static void DefaultSignalHandler(int signal)
{
#if defined(__OpenBSD__) || defined(__FreeBSD__)
    if (!pthread_main_np()) {
        pthread_kill(main_thread, signal);
        return;
    }
#endif

    pid_t pid = getpid();
    RG_ASSERT(pid > 1);

    if (interrupt_pfd[1] >= 0) {
        char dummy = 0;
        RG_IGNORE write(interrupt_pfd[1], &dummy, 1);
    }

    if (flag_interrupt) {
        explicit_interrupt = true;
    } else {
        int code = (signal == SIGINT) ? 130 : 1;
        exit(code);
    }
}

RG_INIT(SetupDefaultHandlers)
{
    int ret = setpgid(0, 0);
    RG_ASSERT(!ret);

    SetSignalHandler(SIGINT, nullptr, DefaultSignalHandler);
    SetSignalHandler(SIGTERM, nullptr, DefaultSignalHandler);
    SetSignalHandler(SIGHUP, nullptr, DefaultSignalHandler);
    SetSignalHandler(SIGPIPE, nullptr, [](int) {});
}

RG_EXIT(TerminateChildren)
{
    pid_t pid = getpid();
    RG_ASSERT(pid > 1);

    SetSignalHandler(SIGTERM, nullptr, [](int) {});
    kill(-pid, SIGTERM);
}

bool CreatePipe(int pfd[2])
{
#ifdef __APPLE__
    if (pipe(pfd) < 0) {
        LogError("Failed to create pipe: %1", strerror(errno));
        return false;
    }

    if (fcntl(pfd[0], F_SETFD, FD_CLOEXEC) < 0 || fcntl(pfd[1], F_SETFD, FD_CLOEXEC) < 0) {
        LogError("Failed to set FD_CLOEXEC on pipe: %1", strerror(errno));
        return false;
    }
    if (fcntl(pfd[0], F_SETFL, O_NONBLOCK) < 0 || fcntl(pfd[1], F_SETFL, O_NONBLOCK) < 0) {
        LogError("Failed to set O_NONBLOCK on pipe: %1", strerror(errno));
        return false;
    }

    return true;
#else
    if (pipe2(pfd, O_CLOEXEC | O_NONBLOCK) < 0)  {
        LogError("Failed to create pipe: %1", strerror(errno));
        return false;
    }

    return true;
#endif
}

void CloseDescriptorSafe(int *fd_ptr)
{
    if (*fd_ptr >= 0) {
        close(*fd_ptr);
    }

    *fd_ptr = -1;
}

bool ExecuteCommandLine(const char *cmd_line, FunctionRef<Span<const uint8_t>()> in_func,
                        FunctionRef<void(Span<uint8_t> buf)> out_func, int *out_code)
{
    // Create read pipes
    int in_pfd[2] = {-1, -1};
    RG_DEFER {
        CloseDescriptorSafe(&in_pfd[0]);
        CloseDescriptorSafe(&in_pfd[1]);
    };
    if (in_func.IsValid()) {
        if (!CreatePipe(in_pfd))
            return false;
        if (fcntl(in_pfd[1], F_SETFL, O_NONBLOCK) < 0) {
            LogError("Failed to set O_NONBLOCK on pipe: %1", strerror(errno));
            return false;
        }
    }

    // Create write pipes
    int out_pfd[2] = {-1, -1};
    RG_DEFER {
        CloseDescriptorSafe(&out_pfd[0]);
        CloseDescriptorSafe(&out_pfd[1]);
    };
    if (out_func.IsValid()) {
        if (!CreatePipe(out_pfd))
            return false;
        if (fcntl(out_pfd[0], F_SETFL, O_NONBLOCK) < 0) {
            LogError("Failed to set O_NONBLOCK on pipe: %1", strerror(errno));
            return false;
        }
    }

    // Create child termination pipe
    {
        static bool success = ([]() {
            if (!CreatePipe(interrupt_pfd))
                return false;

            atexit([]() {
                CloseDescriptorSafe(&interrupt_pfd[0]);
                CloseDescriptorSafe(&interrupt_pfd[1]);
            });

            return true;
        })();

        if (!success) {
            LogError("Failed to create termination pipe");
            return false;
        }
    }

    // Start process
    pid_t pid;
    {
        posix_spawn_file_actions_t file_actions;
        if ((errno = posix_spawn_file_actions_init(&file_actions))) {
            LogError("Failed to set up standard process descriptors: %1", strerror(errno));
            return false;
        }
        RG_DEFER { posix_spawn_file_actions_destroy(&file_actions); };

        if (in_func.IsValid() && (errno = posix_spawn_file_actions_adddup2(&file_actions, in_pfd[0], STDIN_FILENO))) {
            LogError("Failed to set up standard process descriptors: %1", strerror(errno));
            return false;
        }
        if (out_func.IsValid() && ((errno = posix_spawn_file_actions_adddup2(&file_actions, out_pfd[1], STDOUT_FILENO)) ||
                                   (errno = posix_spawn_file_actions_adddup2(&file_actions, out_pfd[1], STDERR_FILENO)))) {
            LogError("Failed to set up standard process descriptors: %1", strerror(errno));
            return false;
        }

        const char *argv[] = {"sh", "-c", cmd_line, nullptr};
        if ((errno = posix_spawn(&pid, "/bin/sh", &file_actions, nullptr,
                                 const_cast<char **>(argv), environ))) {
            LogError("Failed to start process: %1", strerror(errno));
            return false;
        }

        CloseDescriptorSafe(&in_pfd[0]);
        CloseDescriptorSafe(&out_pfd[1]);
    }

    Span<const uint8_t> write_buf = {};
    bool terminate = false;

    // Read and write standard process streams
    while (in_pfd[1] >= 0 || out_pfd[0] >= 0) {
        LocalArray<struct pollfd, 3> pfds;
        int in_idx = -1, out_idx = -1, term_idx = -1;
        if (in_pfd[1] >= 0) {
            in_idx = pfds.len;
            pfds.Append({in_pfd[1], POLLOUT});
        }
        if (out_pfd[0] >= 0) {
            out_idx = pfds.len;
            pfds.Append({out_pfd[0], POLLIN});
        }
        if (interrupt_pfd[0] >= 0) {
            term_idx = pfds.len;
            pfds.Append({interrupt_pfd[0], POLLIN});
        }

        if (RG_POSIX_RESTART_EINTR(poll(pfds.data, (nfds_t)pfds.len, -1), < 0) < 0) {
            LogError("Failed to poll process I/O: %1", strerror(errno));
            break;
        }

        unsigned int in_revents = (in_idx >= 0) ? pfds[in_idx].revents : 0;
        unsigned int out_revents = (out_idx >= 0) ? pfds[out_idx].revents : 0;
        unsigned int term_revents = (term_idx >= 0) ? pfds[term_idx].revents : 0;

        // Try to write
        if (in_revents & (POLLHUP | POLLERR)) {
            CloseDescriptorSafe(&in_pfd[1]);
        } else if (in_revents & POLLOUT) {
            RG_ASSERT(in_func.IsValid());

            if (!write_buf.len) {
                write_buf = in_func();
                RG_ASSERT(write_buf.len >= 0);
            }

            if (write_buf.len) {
                ssize_t write_len = write(in_pfd[1], write_buf.ptr, (size_t)write_buf.len);

                if (write_len > 0) {
                    write_buf.ptr += write_len;
                    write_buf.len -= (Size)write_len;
                } else if (!write_len) {
                    CloseDescriptorSafe(&in_pfd[1]);
                } else {
                    LogError("Failed to write process input: %1", strerror(errno));
                    CloseDescriptorSafe(&in_pfd[1]);
                }
            } else {
                CloseDescriptorSafe(&in_pfd[1]);
            }
        }

        // Try to read
        if (out_revents & (POLLHUP | POLLERR)) {
            break;
        } else if (out_revents & POLLIN) {
            RG_ASSERT(out_func.IsValid());

            uint8_t read_buf[4096];
            ssize_t read_len = read(out_pfd[0], read_buf, RG_SIZE(read_buf));

            if (read_len > 0) {
                out_func(MakeSpan(read_buf, read_len));
            } else if (!read_len) {
                // Does this happen? Should trigger POLLHUP instead, but who knows
                break;
            } else {
                LogError("Failed to read process output: %1", strerror(errno));
                break;
            }
        }

        if (term_revents) {
            kill(pid, SIGTERM);
            terminate = true;

            break;
        }
    }

    // Done reading and writing
    CloseDescriptorSafe(&in_pfd[1]);
    CloseDescriptorSafe(&out_pfd[0]);

    // Wait for process exit
    int status;
    {
        int64_t start = GetMonotonicTime();

        for (;;) {
            int ret = RG_POSIX_RESTART_EINTR(waitpid(pid, &status, terminate ? WNOHANG : 0), < 0);

            if (ret < 0) {
                LogError("Failed to wait for process exit: %1", strerror(errno));
                return false;
            } else if (!ret) {
                int64_t delay = GetMonotonicTime() - start;

                if (delay < 2000) {
                    // A timeout on waitpid would be better, but... sigh
                    WaitDelay(10);
                } else {
                    kill(pid, SIGKILL);
                    terminate = false;
                }
            } else {
                break;
            }
        }
    }

    if (WIFSIGNALED(status)) {
        *out_code = 128 + WTERMSIG(status);
    } else if (WIFEXITED(status)) {
        *out_code = WEXITSTATUS(status);
    } else {
        *out_code = -1;
    }
    return true;
}

#endif

bool ExecuteCommandLine(const char *cmd_line, Span<const uint8_t> in_buf, Size max_len,
                        HeapArray<uint8_t> *out_buf, int *out_code)
{
    Size start_len = out_buf->len;
    RG_DEFER_N(out_guard) { out_buf->RemoveFrom(start_len); };

    // Check virtual memory limits
    {
        Size memory_max = RG_SIZE_MAX - out_buf->len - 1;

        if (RG_UNLIKELY(memory_max <= 0)) {
            LogError("Exhausted memory limit");
            return false;
        }

        RG_ASSERT(max_len);
        max_len = (max_len >= 0) ? std::min(max_len, memory_max) : memory_max;
    }

    // Don't f*ck up the log
    bool warned = false;

    bool success = ExecuteCommandLine(cmd_line, [&]() { return in_buf; },
                                                [&](Span<uint8_t> buf) {
        if (out_buf->len - start_len <= max_len - buf.len) {
            out_buf->Append(buf);
        } else if (!warned) {
            LogError("Truncated output");
            warned = true;
        }
    }, out_code);
    if (!success)
        return false;

    out_guard.Disable();
    return true;
}

#ifdef _WIN32

static HANDLE wait_msg_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);

void WaitDelay(int64_t delay)
{
    RG_ASSERT(delay >= 0);
    RG_ASSERT(delay < 1000ll * INT32_MAX);

    while (delay) {
        DWORD delay32 = (DWORD)std::min(delay, (int64_t)UINT32_MAX);
        delay -= delay32;

        Sleep(delay32);
    }
}

WaitForResult WaitForInterrupt(int64_t timeout)
{
    ignore_ctrl_event = InitConsoleCtrlHandler();
    RG_ASSERT(ignore_ctrl_event);

    HANDLE events[] = {
        console_ctrl_event,
        wait_msg_event
    };

    DWORD ret;
    if (timeout >= 0) {
        do {
            DWORD timeout32 = (DWORD)std::min(timeout, (int64_t)UINT32_MAX);
            timeout -= timeout32;

            ret = WaitForMultipleObjects(RG_LEN(events), events, FALSE, timeout32);
        } while (ret == WAIT_TIMEOUT && timeout);
    } else {
        ret = WaitForMultipleObjects(RG_LEN(events), events, FALSE, INFINITE);
    }

    switch (ret) {
        case WAIT_OBJECT_0: return WaitForResult::Interrupt;
        case WAIT_OBJECT_0 + 1: {
            ResetEvent(wait_msg_event);
            return WaitForResult::Message;
        } break;
        default: {
            RG_ASSERT(ret == WAIT_TIMEOUT);
            return WaitForResult::Timeout;
        } break;
    }
}

void SignalWaitFor()
{
    SetEvent(wait_msg_event);
}

#else

void WaitDelay(int64_t delay)
{
    RG_ASSERT(delay >= 0);
    RG_ASSERT(delay < 1000ll * INT32_MAX);

    struct timespec ts;
    ts.tv_sec = (int)(delay / 1000);
    ts.tv_nsec = (int)((delay % 1000) * 1000000);

    struct timespec rem;
    while (nanosleep(&ts, &rem) < 0) {
        RG_ASSERT(errno == EINTR);
        ts = rem;
    }
}

WaitForResult WaitForInterrupt(int64_t timeout)
{
    static std::atomic_bool message = false;

    flag_interrupt = true;
    SetSignalHandler(SIGUSR1, nullptr, [](int) { message = true; });

    if (timeout >= 0) {
        struct timespec ts;
        ts.tv_sec = (int)(timeout / 1000);
        ts.tv_nsec = (int)((timeout % 1000) * 1000000);

        struct timespec rem;
        while (!explicit_interrupt && !message && nanosleep(&ts, &rem) < 0) {
            RG_ASSERT(errno == EINTR);
            ts = rem;
        }
    } else {
        while (!explicit_interrupt && !message) {
            pause();
        }
    }

    if (explicit_interrupt) {
        return WaitForResult::Interrupt;
    } else if (message) {
        message = false;
        return WaitForResult::Message;
    } else {
        return WaitForResult::Timeout;
    }
}

void SignalWaitFor()
{
    pid_t pid = getpid();
    kill(pid, SIGUSR1);
}

#endif

int GetCoreCount()
{
#ifdef __EMSCRIPTEN__
    return 1;
#else
    static int cores;

    if (!cores) {
        const char *env = GetQualifiedEnv("CORES");

        if (env) {
            char *end_ptr;
            long value = strtol(env, &end_ptr, 10);
            if (end_ptr > env && !end_ptr[0] && value > 0) {
                cores = (int)value;
            } else {
                LogError("OVERRIDE_CORES must be positive number (ignored)");
            }
        } else {
            cores = (int)std::thread::hardware_concurrency();
        }

        RG_ASSERT(cores > 0);
    }

    return cores;
#endif
}

#ifndef _WIN32
bool DropRootIdentity()
{
    uid_t uid = getuid();
    uid_t euid = geteuid();
    gid_t gid = getgid();

    if (!uid) {
        LogError("This program must not be run as root");
        return false;
    }
    if (uid != euid) {
        LogDebug("Dropping SUID privileges...");
    }

    if (!euid && setgroups(1, &gid) < 0)
        goto error;
    if (setregid(gid, gid) < 0)
        goto error;
    if (setreuid(uid, uid) < 0)
        goto error;
    RG_CRITICAL(setuid(0) < 0, "Managed to regain root privileges");

    return true;

error:
    LogError("Failed to drop root privilegies: %1", strerror(errno));
    return false;
}
#endif

#ifdef __linux__
bool NotifySystemd()
{
    const char *addr_str = getenv("NOTIFY_SOCKET");
    if (!addr_str)
        return true;

    struct sockaddr_un addr;
    if (addr_str[0] == '@') {
        addr_str++;

        if (strlen(addr_str) >= sizeof(addr.sun_path) - 1) {
            LogError("Abstract socket address in NOTIFY_SOCKET is too long");
            return false;
        }

        addr.sun_family = AF_UNIX;
        addr.sun_path[0] = 0;
        CopyString(addr_str, MakeSpan(addr.sun_path + 1, RG_SIZE(addr.sun_path) - 1));
    } else if (addr_str[0] == '/') {
        if (strlen(addr_str) >= sizeof(addr.sun_path)) {
            LogError("Socket pathname in NOTIFY_SOCKET is too long");
            return false;
        }

        addr.sun_family = AF_UNIX;
        CopyString(addr_str, addr.sun_path);
    } else {
        LogError("Invalid socket address in NOTIFY_SOCKET");
        return false;
    }

#ifdef __APPLE__
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
        LogError("Failed to create UNIX socket: %1", strerror(errno));
        return false;
    }
    RG_DEFER { close(fd); };

    fcntl(fd, F_SETFD, FD_CLOEXEC);
#else
    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        LogError("Failed to create UNIX socket: %1", strerror(errno));
        return false;
    }
    RG_DEFER { close(fd); };
#endif

    struct iovec iov = {};
    struct msghdr msg = {};
    iov.iov_base = (void *)"READY=1";
    iov.iov_len = strlen("READY=1");
    msg.msg_name = &addr;
    msg.msg_namelen = RG_OFFSET_OF(struct sockaddr_un, sun_path) + strlen(addr_str);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (sendmsg(fd, &msg, MSG_NOSIGNAL) < 0) {
        LogError("Failed to send message to systemd: %1", strerror(errno));
        return false;
    }

    DeleteEnvironmentVar("NOTIFY_SOCKET");
    return true;
}
#endif

// ------------------------------------------------------------------------
// Standard paths
// ------------------------------------------------------------------------

#ifdef _WIN32

const char *GetUserConfigPath(const char *name, Allocator *alloc)
{
    RG_ASSERT(!strchr(RG_PATH_SEPARATORS, name[0]));

    static char cache_dir[4096];
    static std::once_flag flag;

    std::call_once(flag, []() {
        wchar_t *dir = nullptr;
        RG_DEFER { CoTaskMemFree(dir); };

        RG_CRITICAL(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &dir) == S_OK,
                    "Failed to retrieve path to roaming user AppData");
        RG_CRITICAL(ConvertWin32WideToUtf8(dir, cache_dir) >= 0,
                    "Path to roaming AppData is invalid or too big");
    });

    const char *path = Fmt(alloc, "%1%/%2", cache_dir, name).ptr;
    return path;
}

const char *GetUserCachePath(const char *name, Allocator *alloc)
{
    RG_ASSERT(!strchr(RG_PATH_SEPARATORS, name[0]));

    static char cache_dir[4096];
    static std::once_flag flag;

    std::call_once(flag, []() {
        wchar_t *dir = nullptr;
        RG_DEFER { CoTaskMemFree(dir); };

        RG_CRITICAL(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &dir) == S_OK,
                    "Failed to retrieve path to local user AppData");
        RG_CRITICAL(ConvertWin32WideToUtf8(dir, cache_dir) >= 0,
                    "Path to local AppData is invalid or too big");
    });

    const char *path = Fmt(alloc, "%1%/%2", cache_dir, name).ptr;
    return path;
}

const char *GetTemporaryDirectory()
{
    static char temp_dir[4096];
    static std::once_flag flag;

    std::call_once(flag, []() {
        Size len;
        if (win32_utf8) {
            len = (Size)GetTempPathA(RG_SIZE(temp_dir), temp_dir);
            RG_CRITICAL(len < RG_SIZE(temp_dir), "Temporary directory path is too big");
        } else {
            static wchar_t dir_w[4096];
            Size len_w = (Size)GetTempPathW(RG_LEN(dir_w), dir_w);

            RG_CRITICAL(len_w < RG_LEN(dir_w), "Temporary directory path is too big");

            len = ConvertWin32WideToUtf8(dir_w, temp_dir);
            RG_CRITICAL(len >= 0, "Temporary directory path is invalid or too big");
        }

        while (len > 0 && IsPathSeparator(temp_dir[len - 1])) {
            len--;
        }
        temp_dir[len] = 0;
    });

    return temp_dir;
}

#else

const char *GetUserConfigPath(const char *name, Allocator *alloc)
{
    RG_ASSERT(!strchr(RG_PATH_SEPARATORS, name[0]));

    const char *xdg = getenv("XDG_CONFIG_HOME");

    if (xdg) {
        const char *path = Fmt(alloc, "%1%/%2", xdg, name).ptr;
        return path;
    } else {
        const char *home = getenv("HOME");
        RG_CRITICAL(home, "Failed to get HOME environment variable: %1", strerror(errno));

        const char *path = Fmt(alloc, "%1%/.config/%2", home, name).ptr;
        return path;
    }
}

const char *GetUserCachePath(const char *name, Allocator *alloc)
{
    RG_ASSERT(!strchr(RG_PATH_SEPARATORS, name[0]));

    const char *xdg = getenv("XDG_CACHE_HOME");

    if (xdg) {
        const char *path = Fmt(alloc, "%1%/%2", xdg, name).ptr;
        return path;
    } else {
        const char *home = getenv("HOME");
        RG_CRITICAL(home, "Failed to get HOME environment variable: %1", strerror(errno));

        const char *path = Fmt(alloc, "%1%/.cache/%2", home, name).ptr;
        return path;
    }
}

static const char *GetSystemConfigPath(const char *name, Allocator *alloc)
{
    RG_ASSERT(!strchr(RG_PATH_SEPARATORS, name[0]));

    const char *path = Fmt(alloc, "/etc/%1", name).ptr;
    return path;
}

const char *GetTemporaryDirectory()
{
    static char temp_dir[4096];
    static std::once_flag flag;

    std::call_once(flag, []() {
        Span<const char> env = getenv("TMPDIR");

        while (env.len > 0 && IsPathSeparator(env[env.len - 1])) {
            env.len--;
        }

        if (env.len && env.len < RG_SIZE(temp_dir)) {
            CopyString(env, temp_dir);
        } else {
            CopyString("/tmp", temp_dir);
        }
    });

    return temp_dir;
}

#endif

const char *FindConfigFile(const char *name, Allocator *alloc, LocalArray<const char *, 4> *out_possibilities)
{
    decltype(GetUserConfigPath) *funcs[] = {
        [](const char *name, Allocator *alloc) {
            Span<const char> dir = GetApplicationDirectory();

            const char *filename = Fmt(alloc, "%1%/%2", dir, name).ptr;
            return filename;
        },
        GetUserConfigPath,
#ifndef _WIN32
        GetSystemConfigPath
#endif
    };

    const char *filename = nullptr;

    for (const auto &func: funcs) {
        const char *path = func(name, alloc);

        if (TestFile(path, FileType::File)) {
            filename = path;
        }
        if (out_possibilities) {
            out_possibilities->Append(path);
        }
    }

    return filename;
}

static const char *CreateTemporaryPath(Span<const char> directory, const char *prefix, const char *extension,
                                       Allocator *alloc, FunctionRef<bool(const char *path)> create)
{
    RG_ASSERT(alloc);

    HeapArray<char> filename(alloc);
    filename.Append(directory);
    filename.Append(*RG_PATH_SEPARATORS);
    filename.Append(prefix);

    Size change_offset = filename.len;

    PushLogFilter([](LogLevel, const char *, const char *, FunctionRef<LogFunc>) {});
    RG_DEFER_N(log_guard) { PopLogFilter(); };

    for (Size i = 0; i < 1000; i++) {
        // We want to show an error on last try
        if (RG_UNLIKELY(i == 999)) {
            PopLogFilter();
            log_guard.Disable();
        }

        filename.RemoveFrom(change_offset);
        Fmt(&filename, "%1%2", FmtRandom(24), extension);

        if (create(filename.ptr)) {
            const char *ret = filename.TrimAndLeak(1).ptr;
            return ret;
        }
    }

    return nullptr;
}

const char *CreateTemporaryFile(Span<const char> directory, const char *prefix, const char *extension,
                                Allocator *alloc, FILE **out_fp)
{
    return CreateTemporaryPath(directory, prefix, extension, alloc, [&](const char *path) {
        int flags = (int)OpenFileFlag::Read | (int)OpenFileFlag::Write |
                    (int)OpenFileFlag::Exclusive;

        FILE *fp = OpenFile(path, flags);

        if (fp) {
            if (out_fp) {
                *out_fp = fp;
            } else {
                fclose(fp);
            }

            return true;
        } else {
            return false;
        }
    });
}

const char *CreateTemporaryDirectory(Span<const char> directory, const char *prefix, Allocator *alloc)
{
    return CreateTemporaryPath(directory, prefix, "", alloc, [&](const char *path) {
        return MakeDirectory(path);
    });
}

// ------------------------------------------------------------------------
// Random
// ------------------------------------------------------------------------

static inline uint32_t ROTL32(uint32_t v, int n)
{
    return (v << n) | (v >> (32 - n));
}

static inline uint64_t ROTL64(uint64_t v, int n) {
    return (v << n) | (v >> (64 - n));
}

FastRandom::FastRandom()
{
    do {
        FillRandomSafe(state, RG_SIZE(state));
    } while (std::all_of(std::begin(state), std::end(state), [](uint64_t v) { return !v; }));
}

FastRandom::FastRandom(uint64_t seed)
{
    // splitmix64 generator to seed xoshiro256++, as recommended

    seed += 0x9e3779b97f4a7c15;

    for (int i = 0; i < 4; i++) {
        seed = (seed ^ (seed >> 30)) * 0xbf58476d1ce4e5b9;
        seed = (seed ^ (seed >> 27)) * 0x94d049bb133111eb;
        state[i] = seed ^ (seed >> 31);
    }
}

void FastRandom::Fill(void *out_buf, Size len)
{
    for (Size i = 0; i < len; i += 8) {
        uint64_t rnd = Next();

        Size copy_len = std::min(RG_SIZE(rnd), len - i);
        memcpy((uint8_t *)out_buf + i, &rnd, copy_len);
    }
}

int FastRandom::GetInt(int min, int max)
{
    int range = max - min;
    RG_ASSERT(range >= 2);

    unsigned int treshold = (UINT_MAX - UINT_MAX % range);

    unsigned int x;
    do {
        Fill(&x, RG_SIZE(x));
    } while (x >= treshold);
    x %= range;

    return min + (int)x;
}

uint64_t FastRandom::Next()
{
    // xoshiro256++ by David Blackman and Sebastiano Vigna (vigna@acm.org)
    // Hopefully I did not screw it up :)

    uint64_t result = ROTL64(state[0] + state[3], 23) + state[0];
    uint64_t t = state[1] << 17;

    state[2] ^= state[0];
    state[3] ^= state[1];
    state[1] ^= state[2];
    state[0] ^= state[3];
    state[2] ^= t;
    state[3] = ROTL64(state[3], 45);

    return result;
}

static RG_THREAD_LOCAL Size rnd_remain;
static RG_THREAD_LOCAL int64_t rnd_time;
#ifndef _WIN32
static RG_THREAD_LOCAL pid_t rnd_pid;
#endif
static RG_THREAD_LOCAL uint32_t rnd_state[16];
static RG_THREAD_LOCAL uint8_t rnd_buf[64];
static RG_THREAD_LOCAL Size rnd_offset;

static void InitChaCha20(uint32_t state[16], uint32_t key[8], uint32_t iv[2])
{
    static const char magic[] = "expand 32-byte k";

    // Sensitive to endianness
    memcpy(state, magic, 16);
    memcpy(state + 4, key, 32);
    state[12] = 0;
    state[13] = 0;
    memcpy(state + 14, iv, 8);
}

static void RunChaCha20(uint32_t state[16], uint8_t out_buf[64])
{
    uint32_t *out_buf32 = (uint32_t *)out_buf;

    uint32_t x[16];
    memcpy(x, state, RG_SIZE(x));

    for (Size i = 0; i < 20; i += 2) {
        x[0] += x[4];   x[12] = ROTL32(x[12] ^ x[0], 16);
        x[1] += x[5];   x[13] = ROTL32(x[13] ^ x[1], 16);
        x[2] += x[6];   x[14] = ROTL32(x[14] ^ x[2], 16);
        x[3] += x[7];   x[15] = ROTL32(x[15] ^ x[3], 16);

        x[8]  += x[12]; x[4]  = ROTL32(x[4] ^ x[8],  12);
        x[9]  += x[13]; x[5]  = ROTL32(x[5] ^ x[9],  12);
        x[10] += x[14]; x[6]  = ROTL32(x[6] ^ x[10], 12);
        x[11] += x[15]; x[7]  = ROTL32(x[7] ^ x[11], 12);

        x[0] += x[4];   x[12] = ROTL32(x[12] ^ x[0], 8);
        x[1] += x[5];   x[13] = ROTL32(x[13] ^ x[1], 8);
        x[2] += x[6];   x[14] = ROTL32(x[14] ^ x[2], 8);
        x[3] += x[7];   x[15] = ROTL32(x[15] ^ x[3], 8);

        x[8]  += x[12]; x[4]  = ROTL32(x[4] ^ x[8],  7);
        x[9]  += x[13]; x[5]  = ROTL32(x[5] ^ x[9],  7);
        x[10] += x[14]; x[6]  = ROTL32(x[6] ^ x[10], 7);
        x[11] += x[15]; x[7]  = ROTL32(x[7] ^ x[11], 7);

        x[0] += x[5];   x[15] = ROTL32(x[15] ^ x[0], 16);
        x[1] += x[6];   x[12] = ROTL32(x[12] ^ x[1], 16);
        x[2] += x[7];   x[13] = ROTL32(x[13] ^ x[2], 16);
        x[3] += x[4];   x[14] = ROTL32(x[14] ^ x[3], 16);

        x[10] += x[15]; x[5]  = ROTL32(x[5] ^ x[10], 12);
        x[11] += x[12]; x[6]  = ROTL32(x[6] ^ x[11], 12);
        x[8]  += x[13]; x[7]  = ROTL32(x[7] ^ x[8],  12);
        x[9]  += x[14]; x[4]  = ROTL32(x[4] ^ x[9],  12);

        x[0] += x[5];   x[15] = ROTL32(x[15] ^ x[0], 8);
        x[1] += x[6];   x[12] = ROTL32(x[12] ^ x[1], 8);
        x[2] += x[7];   x[13] = ROTL32(x[13] ^ x[2], 8);
        x[3] += x[4];   x[14] = ROTL32(x[14] ^ x[3], 8);

        x[10] += x[15]; x[5]  = ROTL32(x[5] ^ x[10], 7);
        x[11] += x[12]; x[6]  = ROTL32(x[6] ^ x[11], 7);
        x[8]  += x[13]; x[7]  = ROTL32(x[7] ^ x[8],  7);
        x[9]  += x[14]; x[4]  = ROTL32(x[4] ^ x[9],  7);
    }

    for (Size i = 0; i < RG_LEN(x); i++) {
        out_buf32[i] = LittleEndian(x[i] + state[i]);
    }

    state[12]++;
    state[13] += !state[12];
}

void ZeroMemorySafe(void *ptr, Size len)
{
#ifdef _WIN32
    SecureZeroMemory(ptr, (SIZE_T)len);
#else
    memset_safe(ptr, 0, (size_t)len);
    __asm__ __volatile__("" : : "r"(ptr) : "memory");
#endif
}

void FillRandomSafe(void *out_buf, Size len)
{
    bool reseed = false;

    // Reseed every 4 megabytes, or every hour, or after a fork
    reseed |= (rnd_remain <= 0);
    reseed |= (GetMonotonicTime() - rnd_time > 3600 * 1000);
#ifndef _WIN32
    reseed |= (getpid() != rnd_pid);
#endif

    if (reseed) {
        struct { uint32_t key[8]; uint32_t iv[2]; } buf;

        memset(rnd_state, 0, RG_SIZE(rnd_state));
#ifdef _WIN32
        RG_CRITICAL(RtlGenRandom(&buf, RG_SIZE(buf)), "RtlGenRandom() failed: %s", GetWin32ErrorString());
#else
        RG_CRITICAL(getentropy(&buf, RG_SIZE(buf)) == 0, "getentropy() failed: %s", strerror(errno));
#endif

        InitChaCha20(rnd_state, buf.key, buf.iv);
        ZeroMemorySafe(&buf, RG_SIZE(buf));

        rnd_remain = Mebibytes(4);
        rnd_time = GetMonotonicTime();
#ifndef _WIN32
        rnd_pid = getpid();
#endif

        rnd_offset = RG_SIZE(rnd_buf);
    }

    Size copy_len = std::min(RG_SIZE(rnd_buf) - rnd_offset, len);
    memcpy_safe(out_buf, rnd_buf + rnd_offset, (size_t)copy_len);
    ZeroMemorySafe(rnd_buf + rnd_offset, copy_len);
    rnd_offset += copy_len;

    for (Size i = copy_len; i < len; i += RG_SIZE(rnd_buf)) {
        RunChaCha20(rnd_state, rnd_buf);

        copy_len = std::min(RG_SIZE(rnd_buf), len - i);
        memcpy_safe((uint8_t *)out_buf + i, rnd_buf, (size_t)copy_len);
        ZeroMemorySafe(rnd_buf, copy_len);
        rnd_offset = copy_len;
    }

    rnd_remain -= len;
}

int GetRandomIntSafe(int min, int max)
{
    int range = max - min;
    RG_ASSERT(range >= 2);

    unsigned int treshold = (UINT_MAX - UINT_MAX % range);

    unsigned int x;
    do {
        FillRandomSafe(&x, RG_SIZE(x));
    } while (x >= treshold);
    x %= range;

    return min + (int)x;
}

// ------------------------------------------------------------------------
// Sockets
// ------------------------------------------------------------------------

int OpenIPSocket(SocketType type, int port, SocketMode mode)
{
    RG_ASSERT(type == SocketType::Dual || type == SocketType::IPv4 || type == SocketType::IPv6);

    int family = (type == SocketType::IPv4) ? AF_INET : AF_INET6;

    int flags = 0;
    switch (mode) {
        case SocketMode::Stream: { flags = SOCK_STREAM; } break;
        case SocketMode::Messages: { flags = SOCK_DGRAM; } break;
    }

#ifdef _WIN32
    SOCKET fd = socket(family, flags, 0);
    if (fd == INVALID_SOCKET) {
        LogError("Failed to create AF_INET socket: %1", strerror(errno));
        return -1;
    }
    RG_DEFER_N(err_guard) { closesocket(fd); };
#else
    int fd = socket(family, flags, 0);
    if (fd < 0) {
        LogError("Failed to create AF_INET socket: %1", strerror(errno));
        return -1;
    }
    RG_DEFER_N(err_guard) { close(fd); };

    int reuseport = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuseport, sizeof(reuseport));
#endif

    if (type == SocketType::IPv4) {
        struct sockaddr_in addr = {};

        addr.sin_family = AF_INET;
        addr.sin_port = htons((uint16_t)port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            LogError("Failed to bind to port %1: %2", port, strerror(errno));
            return -1;
        }
    } else {
        struct sockaddr_in6 addr = {};
        int v6only = (type == SocketType::IPv6);

        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons((uint16_t)port);
        addr.sin6_addr = IN6ADDR_ANY_INIT;

#if defined(_WIN32)
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&v6only, sizeof(v6only)) < 0) {
#elif defined(__OpenBSD__)
        if (!v6only) {
            LogError("Dual-stack sockets are not supported on OpenBSD");
            return -1;
        } else if (false) {
#else
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) < 0) {
#endif
            LogError("Failed to change dual-stack socket option: %1", strerror(errno));
            return -1;
        }

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            LogError("Failed to bind to port %1: %2", port, strerror(errno));
            return -1;
        }
    }

    err_guard.Disable();
    return (int)fd;
}

int OpenUnixSocket(const char *path, SocketMode mode)
{
    int flags = 0;
    switch (mode) {
        case SocketMode::Stream: { flags = SOCK_STREAM; } break;
        case SocketMode::Messages: { flags = SOCK_SEQPACKET; } break;
    }

#if defined(_WIN32)
    SOCKET fd = socket(AF_UNIX, flags, 0);
    if (fd == INVALID_SOCKET) {
        LogError("Failed to create AF_UNIX socket: %1", strerror(errno));
        return -1;
    }
    RG_DEFER_N(err_guard) { closesocket(fd); };
#elif defined(__APPLE__)
    int fd = (int)socket(AF_UNIX, flags, 0);
    if (fd < 0) {
        LogError("Failed to create AF_UNIX socket: %1", strerror(errno));
        return -1;
    }
    RG_DEFER_N(err_guard) { close(fd); };

    fcntl(fd, F_SETFD, FD_CLOEXEC);
#else
    flags |= SOCK_CLOEXEC;

    int fd = (int)socket(AF_UNIX, flags, 0);
    if (fd < 0) {
        LogError("Failed to create AF_UNIX socket: %1", strerror(errno));
        return -1;
    }
    RG_DEFER_N(err_guard) { close(fd); };
#endif

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    if (!CopyString(path, addr.sun_path)) {
        LogError("Excessive UNIX socket path length");
        return -1;
    }

    unlink(path);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LogError("Failed to bind socket to '%1': %2", path, strerror(errno));
        return -1;
    }
    chmod(path, 0666);

    err_guard.Disable();
    return (int)fd;
}

int ConnectToUnixSocket(const char *path, SocketMode mode)
{
    int flags = 0;
    switch (mode) {
        case SocketMode::Stream: { flags = SOCK_STREAM; } break;
        case SocketMode::Messages: { flags = SOCK_SEQPACKET; } break;
    }

#if defined(_WIN32)
    SOCKET fd = socket(AF_UNIX, flags, 0);
    if (fd == INVALID_SOCKET) {
        LogError("Failed to create AF_UNIX socket: %1", strerror(errno));
        return -1;
    }
    RG_DEFER_N(err_guard) { closesocket(fd); };
#elif defined(__APPLE__)
    int fd = (int)socket(AF_UNIX, flags, 0);
    if (fd < 0) {
        LogError("Failed to create AF_UNIX socket: %1", strerror(errno));
        return -1;
    }
    RG_DEFER_N(err_guard) { close(fd); };

    fcntl(fd, F_SETFD, FD_CLOEXEC);
#else
    flags |= SOCK_CLOEXEC;

    int fd = (int)socket(AF_UNIX, flags, 0);
    if (fd < 0) {
        LogError("Failed to create AF_UNIX socket: %1", strerror(errno));
        return -1;
    }
    RG_DEFER_N(err_guard) { close(fd); };
#endif

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    if (!CopyString(path, addr.sun_path)) {
        LogError("Excessive UNIX socket path length");
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, RG_SIZE(addr)) < 0) {
        LogError("Failed to connect to '%1': %2", path, strerror(errno));
        return -1;
    }

    err_guard.Disable();
    return (int)fd;
}

void CloseSocket(int fd)
{
#ifdef _WIN32
    shutdown((SOCKET)fd, SD_BOTH);
    closesocket((SOCKET)fd);
#else
    shutdown(fd, SHUT_RDWR);
    close(fd);
#endif
}

// ------------------------------------------------------------------------
// Tasks
// ------------------------------------------------------------------------

struct Task {
    Async *async;
    std::function<bool()> func;
};

struct TaskQueue {
    std::mutex queue_mutex;
    BucketArray<Task> tasks;
};

class AsyncPool {
    RG_DELETE_COPY(AsyncPool)

    std::mutex pool_mutex;
    std::condition_variable pending_cv;
    std::condition_variable sync_cv;

    // Manipulate with pool_mutex locked
    int refcount = 0;

    int async_count = 0;
    HeapArray<bool> workers_state;

    HeapArray<TaskQueue> queues;
    int next_queue_idx = 0;
    std::atomic_int pending_tasks {0};

public:
    AsyncPool(int threads, bool leak);

    void RegisterAsync();
    void UnregisterAsync();

    void AddTask(Async *async, const std::function<bool()> &func);

    void RunWorker(int worker_idx);
    void SyncOn(Async *async);

    void RunTasks(int queue_idx);
    void RunTask(Task *task);
};

// thread_local breaks down on MinGW when destructors are involved, work
// around this with heap allocation.
static RG_THREAD_LOCAL AsyncPool *async_default_pool = nullptr;
static RG_THREAD_LOCAL AsyncPool *async_running_pool = nullptr;
static RG_THREAD_LOCAL int async_running_worker_idx;
static RG_THREAD_LOCAL bool async_running_task = false;

Async::Async(int threads, bool stop_after_error)
    : stop_after_error(stop_after_error)
{
    RG_ASSERT(threads);

    if (threads > 0) {
        pool = new AsyncPool(threads, false);
    } else if (async_running_pool) {
        pool = async_running_pool;
    } else {
        if (!async_default_pool) {
            // NOTE: We're leaking one AsyncPool each time a non-worker thread uses Async()
            // for the first time. That's only one leak in most cases, when the main thread
            // is the only non-worker thread using Async, but still. Something to keep in mind.

            threads = GetCoreCount();
            async_default_pool = new AsyncPool(threads, true);
        }

        pool = async_default_pool;
    }

    pool->RegisterAsync();
}

Async::~Async()
{
    RG_ASSERT(!remaining_tasks);
    pool->UnregisterAsync();
}

void Async::Run(const std::function<bool()> &func)
{
    pool->AddTask(this, func);
}

bool Async::Sync()
{
    pool->SyncOn(this);
    return success;
}

bool Async::IsTaskRunning()
{
    return async_running_task;
}

int Async::GetWorkerIdx()
{
    return async_running_worker_idx;
}

AsyncPool::AsyncPool(int threads, bool leak)
{
    if (threads > RG_ASYNC_MAX_THREADS) {
        LogError("Async cannot use more than %1 threads", RG_ASYNC_MAX_THREADS);
        threads = RG_ASYNC_MAX_THREADS;
    }

    // The first queue is for the main thread, whereas workers_state[0] is
    // not used but it's easier to index it the same way.
    workers_state.AppendDefault(threads);
    queues.AppendDefault(threads);

    refcount = leak;
}

void AsyncPool::RegisterAsync()
{
    std::lock_guard<std::mutex> lock_pool(pool_mutex);

    if (!async_count++) {
        for (int i = 1; i < workers_state.len; i++) {
            if (!workers_state[i]) {
                std::thread(&AsyncPool::RunWorker, this, i).detach();

                refcount++;
                workers_state[i] = true;
            }
        }
    }
}

void AsyncPool::UnregisterAsync()
{
    std::lock_guard<std::mutex> lock_pool(pool_mutex);
    async_count--;
}

void AsyncPool::AddTask(Async *async, const std::function<bool()> &func)
{
    if (async_running_pool != async->pool) {
        for (;;) {
            TaskQueue *queue = &queues[next_queue_idx];

            if (--next_queue_idx < 0) {
                next_queue_idx = (int)workers_state.len - 1;
            }

            std::unique_lock<std::mutex> lock_queue(queue->queue_mutex, std::try_to_lock);
            if (lock_queue.owns_lock()) {
                queue->tasks.Append({async, func});
                break;
            }
        }
    } else {
        TaskQueue *queue = &queues[async_running_worker_idx];

        std::lock_guard<std::mutex> lock_queue(queue->queue_mutex);
        queue->tasks.Append({async, func});
    }

    async->remaining_tasks++;

    // Wake up workers and syncing threads (extra help)
    if (!pending_tasks++) {
        std::lock_guard<std::mutex> lock_pool(pool_mutex);

        pending_cv.notify_all();
        sync_cv.notify_all();
    }
}

void AsyncPool::RunWorker(int worker_idx)
{
    async_running_pool = this;
    async_running_worker_idx = worker_idx;

    std::unique_lock<std::mutex> lock_pool(pool_mutex);

    while (async_count) {
        lock_pool.unlock();
        RunTasks(worker_idx);
        lock_pool.lock();

        std::chrono::duration<int, std::milli> duration(RG_ASYNC_MAX_IDLE_TIME); // Thanks C++
        pending_cv.wait_for(lock_pool, duration, [&]() { return !!pending_tasks; });
    }

    workers_state[worker_idx] = false;
    if (!--refcount) {
        lock_pool.unlock();
        delete this;
    }
}

void AsyncPool::SyncOn(Async *async)
{
    RG_DEFER_C(pool = async_running_pool,
               worker_idx = async_running_worker_idx) {
        async_running_pool = pool;
        async_running_worker_idx = worker_idx;
    };

    async_running_pool = this;
    async_running_worker_idx = 0;

    while (async->remaining_tasks) {
        RunTasks(async_running_worker_idx);

        std::unique_lock<std::mutex> lock_sync(pool_mutex);
        sync_cv.wait(lock_sync, [&]() { return pending_tasks || !async->remaining_tasks; });
    }
}

void AsyncPool::RunTasks(int queue_idx)
{
    // The '12' factor is pretty arbitrary, don't try to find meaning there
    for (int i = 0; i < workers_state.len * 12; i++) {
        TaskQueue *queue = &queues[queue_idx];
        std::unique_lock<std::mutex> lock_queue(queue->queue_mutex, std::try_to_lock);

        if (lock_queue.owns_lock() && queue->tasks.len) {
            Task task = std::move(queue->tasks[0]);

            queue->tasks.RemoveFirst();
            queue->tasks.Trim();

            lock_queue.unlock();

            RunTask(&task);
        } else {
            queue_idx = (++queue_idx < queues.len) ? queue_idx : 0;
        }
    }
}

void AsyncPool::RunTask(Task *task)
{
    Async *async = task->async;

    RG_DEFER_C(running = async_running_task) { async_running_task = running; };
    async_running_task = true;

    bool run = !async->stop_after_error ||
               async->success.load(std::memory_order_relaxed);

    pending_tasks--;
    if (run && !task->func()) {
        async->success = false;
    }

    if (!--async->remaining_tasks) {
        std::lock_guard<std::mutex> lock_sync(pool_mutex);
        sync_cv.notify_all();
    }
}

// ------------------------------------------------------------------------
// Fibers
// ------------------------------------------------------------------------

#if defined(_WIN32)

static RG_THREAD_LOCAL int fib_fibers;
static RG_THREAD_LOCAL void *fib_self;
static RG_THREAD_LOCAL bool fib_run;

Fiber::Fiber(const std::function<bool()> &f, Size stack_size)
    : f(f)
{
    if (!fib_self) {
        fib_self = ConvertThreadToFiber(nullptr);

        if (!fib_self) {
            LogError("Failed to convert thread to fiber: %1", GetWin32ErrorString());
            return;
        }
    }
    RG_DEFER_N(self_guard) {
        if (!fib_fibers) {
            RG_CRITICAL(ConvertFiberToThread(), "ConvertFiberToThread() failed: %1", GetWin32ErrorString());
            fib_self = nullptr;
        }
    };

    fiber = CreateFiber((SIZE_T)stack_size, FiberCallback, this);
    if (!fiber) {
        LogError("Failed to create fiber: %1", GetWin32ErrorString());
        return;
    }

    self_guard.Disable();
    done = false;
    fib_fibers++;
}

Fiber::~Fiber()
{
    if (fib_run) {
        // We are forced to execute it until the end
        Finalize();
        fib_run = false;
    }

    if (fiber) {
        DeleteFiber(fiber);
        fiber = nullptr;

        if (!--fib_fibers && fib_self) {
            RG_CRITICAL(ConvertFiberToThread(), "ConvertFiberToThread() failed: %1", GetWin32ErrorString());
            fib_self = nullptr;
        }
    }
}

void Fiber::SwitchTo()
{
    if (RG_UNLIKELY(!fiber))
        return;

    if (!done) {
        fib_run = true;
        SwitchToFiber(fiber);
    }
}

bool Fiber::Finalize()
{
    if (RG_UNLIKELY(!fiber))
        return false;

    if (!done) {
        fib_run = false;
        SwitchToFiber(fiber);

        RG_ASSERT(done);
    }

    return success;
}

bool Fiber::SwitchBack()
{
    if (fib_run) {
        SwitchToFiber(fib_self);
        return true;
    } else {
        return false;
    }
}

void WINAPI Fiber::FiberCallback(void *udata)
{
    Fiber *self = (Fiber *)udata;

    self->success = self->f();
    self->done = true;

    SwitchToFiber(fib_self);
}

#elif defined(RG_FIBER_USE_UCONTEXT)

static RG_THREAD_LOCAL ucontext_t fib_self;
static RG_THREAD_LOCAL ucontext_t *fib_run;

Fiber::Fiber(const std::function<bool()> &f, Size stack_size)
    : f(f)
{
    if (getcontext(&fib_self) < 0) {
        LogError("Failed to get fiber context: %1", strerror(errno));
        return;
    }
    memcpy(&ucp, &fib_self, RG_SIZE(ucp));

    ucp.uc_stack.ss_sp = Allocator::Allocate(nullptr, stack_size);
    ucp.uc_stack.ss_size = (size_t)stack_size;
    ucp.uc_link = nullptr;

    unsigned int high = (unsigned int)((uint64_t)this >> 32);
    unsigned int low = (unsigned int)((uint64_t)this & 0xFFFFFFFFull);
    makecontext(&ucp, (void (*)())FiberCallback, 2, high, low);

    done = false;
}

Fiber::~Fiber()
{
    if (fib_run) {
        // We are forced to execute it until the end
        Finalize();
        fib_run = nullptr;
    }
}

void Fiber::SwitchTo()
{
    if (RG_UNLIKELY(!ucp.uc_stack.ss_sp))
        return;

    if (!done) {
        fib_run = &ucp;
        RG_CRITICAL(swapcontext(&fib_self, &ucp) == 0, "swapcontext() failed: %1", strerror(errno));
    }
}

bool Fiber::Finalize()
{
    if (RG_UNLIKELY(!ucp.uc_stack.ss_sp))
        return false;

    if (!done) {
        fib_run = nullptr;
        RG_CRITICAL(swapcontext(&fib_self, &ucp) == 0, "swapcontext() failed: %1", strerror(errno));

        RG_ASSERT(done);
    }

    return success;
}

bool Fiber::SwitchBack()
{
    if (fib_run) {
        RG_CRITICAL(swapcontext(fib_run, &fib_self) == 0, "swapcontext() failed: %1", strerror(errno));
        return true;
    } else {
        return false;
    }
}

void Fiber::FiberCallback(unsigned int high, unsigned int low)
{
    Fiber *self = (Fiber *)(((uint64_t)high << 32) | (uint64_t)low);

    self->success = self->f();
    self->done = true;

    RG_CRITICAL(swapcontext(&self->ucp, &fib_self) == 0, "swapcontext() failed: %1", strerror(errno));
}

#else

#warning makecontext API is not available, using slower thread-based implementation

static RG_THREAD_LOCAL std::unique_lock<std::mutex> *fib_lock;
static RG_THREAD_LOCAL Fiber *fib_self;

Fiber::Fiber(const std::function<bool()> &f, Size stack_size)
    : f(f)
{
    thread = std::thread(ThreadCallback, this);
    done = false;

    while (toggle == 1) {
        cv.wait(lock);
    }
}

Fiber::~Fiber()
{
    // We are forced to execute it until the end
    Finalize();

    if (thread.joinable()) {
        thread.join();
    }
}

void Fiber::SwitchTo()
{
    if (!done) {
        Toggle(1, &lock);
    }
}

bool Fiber::Finalize()
{
    fib_lock = nullptr;
    SwitchTo();

    return success;
}

bool Fiber::SwitchBack()
{
    if (fib_lock) {
        Fiber *self = fib_self;
        self->Toggle(0, fib_lock);

        return true;
    } else {
        return false;
    }
}

void Fiber::ThreadCallback(void *udata)
{
    Fiber *self = (Fiber *)udata;

    std::unique_lock<std::mutex> lock(self->mutex);

    fib_lock = &lock;
    fib_self = self;

    // Wait for our turn
    self->Toggle(0, fib_lock);

    self->success = self->f();
    self->done = true;

    self->toggle = 0;
    self->cv.notify_one();
}

void Fiber::Toggle(int to, std::unique_lock<std::mutex> *lock)
{
    toggle = to;

    cv.notify_one();
    while (toggle == to) {
        cv.wait(*lock);
    }
}

#endif

// ------------------------------------------------------------------------
// Streams
// ------------------------------------------------------------------------

#ifdef _WIN32
RG_INIT(BinaryStdIO)
{
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
}
#endif

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

#ifdef BROTLI_DEFAULT_MODE
struct BrotliDecompressContext {
    BrotliDecoderState *state;
    bool done;

    uint8_t in[256 * 1024];
    Size in_len;

    uint8_t out[256 * 1024];
    Size out_len;
};
#endif

bool StreamReader::Open(Span<const uint8_t> buf, const char *filename,
                        CompressionType compression_type)
{
    Close(true);

    RG_DEFER_N(err_guard) { error = true; };
    error = false;

    this->filename = filename ? DuplicateString(filename, &str_alloc).ptr : "<memory>";

    source.type = SourceType::Memory;
    source.u.memory.buf = buf;
    source.u.memory.pos = 0;

    if (!InitDecompressor(compression_type))
        return false;

    err_guard.Disable();
    return true;
}

bool StreamReader::Open(FILE *fp, const char *filename, CompressionType compression_type)
{
    Close(true);

    RG_DEFER_N(err_guard) { error = true; };
    error = false;

    RG_ASSERT(fp);
    RG_ASSERT(filename);
    this->filename = DuplicateString(filename, &str_alloc).ptr;

    source.type = SourceType::File;
    source.u.file.fp = fp;
    source.u.file.owned = false;

    if (!InitDecompressor(compression_type))
        return false;

    err_guard.Disable();
    return true;
}

bool StreamReader::Open(const char *filename, CompressionType compression_type)
{
    Close(true);

    RG_DEFER_N(err_guard) { error = true; };
    error = false;

    RG_ASSERT(filename);
    this->filename = DuplicateString(filename, &str_alloc).ptr;

    source.type = SourceType::File;
    source.u.file.fp = OpenFile(filename, (int)OpenFileFlag::Read);
    if (!source.u.file.fp)
        return false;
    source.u.file.owned = true;

    if (!InitDecompressor(compression_type))
        return false;

    err_guard.Disable();
    return true;
}

bool StreamReader::Open(const std::function<Size(Span<uint8_t>)> &func, const char *filename,
                        CompressionType compression_type)
{
    Close(true);

    RG_DEFER_N(err_guard) { error = true; };
    error = false;

    this->filename = filename ? DuplicateString(filename, &str_alloc).ptr : "<closure>";

    source.type = SourceType::Function;
    new (&source.u.func) std::function<Size(Span<uint8_t>)>(func);

    if (!InitDecompressor(compression_type))
        return false;

    err_guard.Disable();
    return true;
}

bool StreamReader::Close(bool implicit)
{
    RG_ASSERT(implicit || this != &stdin_st);

    switch (compression.type) {
        case CompressionType::None: {} break;

        case CompressionType::Gzip:
        case CompressionType::Zlib: {
#ifdef MZ_VERSION
            Allocator::Release(nullptr, compression.u.miniz, RG_SIZE(*compression.u.miniz));
            compression.u.miniz = nullptr;
#else
            RG_UNREACHABLE();
#endif
        } break;

        case CompressionType::Brotli: {
#ifdef BROTLI_DEFAULT_MODE
            BrotliDecompressContext *ctx = compression.u.brotli;

            if (ctx) {
                if (ctx->state) {
                    BrotliDecoderDestroyInstance(ctx->state);
                }

                Allocator::Release(nullptr, ctx, RG_SIZE(*ctx));
                compression.u.brotli = nullptr;
            }
#else
            RG_UNREACHABLE();
#endif
        } break;
    }

    switch (source.type) {
        case SourceType::Memory: { source.u.memory = {}; } break;
        case SourceType::File: {
            if (source.u.file.owned && source.u.file.fp) {
                fclose(source.u.file.fp);
            }

            source.u.file.fp = nullptr;
            source.u.file.owned = false;
        } break;
        case SourceType::Function: { source.u.func.~function(); } break;
    }

    bool ret = !filename || !error;

    filename = nullptr;
    error = true;
    compression.type = CompressionType::None;
    source.type = SourceType::Memory;
    source.eof = false;
    eof = false;
    raw_len = -1;
    raw_read = 0;
    str_alloc.ReleaseAll();

    return ret;
}

bool StreamReader::Rewind()
{
    if (RG_UNLIKELY(error))
        return false;

    switch (source.type) {
        case SourceType::Memory: { source.u.memory.pos = 0; } break;
        case SourceType::File: {
            if (fseek(source.u.file.fp, 0, SEEK_SET) < 0) {
                LogError("Failed to rewind '%1': %2", filename, strerror(errno));
                error = true;
                return false;
            }
        } break;
        case SourceType::Function: {
            LogError("Cannot rewind stream '%1'", filename);
            error = true;
            return false;
        } break;
    }

    switch (compression.type) {
        case CompressionType::None: {} break;

        case CompressionType::Gzip:
        case CompressionType::Zlib: {
#ifdef MZ_VERSION
            memset(compression.u.miniz, 0, sizeof(*compression.u.miniz));
            tinfl_init(&compression.u.miniz->inflator);
            compression.u.miniz->crc32 = MZ_CRC32_INIT;
#else
            RG_UNREACHABLE();
#endif
        } break;

        case CompressionType::Brotli: {
#ifdef BROTLI_DEFAULT_MODE
            if (compression.u.brotli->state) {
                BrotliDecoderDestroyInstance(compression.u.brotli->state);
                compression.u.brotli->state = nullptr;
            }
            compression.u.brotli->state = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
#else
            RG_UNREACHABLE();
#endif
        } break;
    }

    source.eof = false;
    eof = false;

    return true;
}

Size StreamReader::Read(Span<uint8_t> out_buf)
{
    if (RG_UNLIKELY(error))
        return -1;

    Size read_len = 0;
    switch (compression.type) {
        case CompressionType::None: {
            read_len = ReadRaw(out_buf.len, out_buf.ptr);
            eof = source.eof;
        } break;

        case CompressionType::Gzip:
        case CompressionType::Zlib: {
#ifdef MZ_VERSION
            read_len = ReadInflate(out_buf.len, out_buf.ptr);
#else
            RG_UNREACHABLE();
#endif
        } break;

        case CompressionType::Brotli: {
#ifdef BROTLI_DEFAULT_MODE
            read_len = ReadBrotli(out_buf.len, out_buf.ptr);
#else
            RG_UNREACHABLE();
#endif
        } break;
    }

    return read_len;
}

Size StreamReader::ReadAll(Size max_len, HeapArray<uint8_t> *out_buf)
{
    if (RG_UNLIKELY(error))
        return -1;

    RG_DEFER_NC(buf_guard, buf_len = out_buf->len) { out_buf->RemoveFrom(buf_len); };

    // Check virtual memory limits
    {
        Size memory_max = RG_SIZE_MAX - out_buf->len - 1;

        if (RG_UNLIKELY(memory_max <= 0)) {
            LogError("Exhausted memory limit reading file '%1'", filename);
            return -1;
        }

        RG_ASSERT(max_len);
        max_len = (max_len >= 0) ? std::min(max_len, memory_max) : memory_max;
    }

    // For some files (such as in /proc), the file size is reported as 0 even though there
    // is content inside, because these files are generated on demand. So we need to take
    // the slow path for apparently empty files.
    if (compression.type == CompressionType::None && ComputeRawLen() > 0) {
        if (raw_len > max_len) {
            LogError("File '%1' is too large (limit = %2)", filename, FmtDiskSize(max_len));
            return -1;
        }

        // Count one trailing byte (if possible) to avoid reallocation for users
        // who need/want to append a NUL character.
        out_buf->Grow((Size)raw_len + 1);

        Size read_len = Read((Size)raw_len, out_buf->end());
        if (read_len < 0)
            return -1;
        out_buf->len += read_len;

        buf_guard.Disable();
        return read_len;
    } else {
        Size total_len = 0;

        while (!eof) {
            Size grow = std::min(total_len ? Megabytes(1) : Kibibytes(64), RG_SIZE_MAX - out_buf->len);
            out_buf->Grow(grow);

            Size read_len = Read(out_buf->Available(), out_buf->end());
            if (read_len < 0)
                return -1;

            if (RG_UNLIKELY(read_len > max_len - total_len)) {
                LogError("File '%1' is too large (limit = %2)", filename, FmtDiskSize(max_len));
                return -1;
            }

            total_len += read_len;
            out_buf->len += read_len;
        }

        buf_guard.Disable();
        return total_len;
    }
}

int64_t StreamReader::ComputeRawLen()
{
    if (RG_UNLIKELY(error))
        return -1;
    if (raw_read || raw_len >= 0)
        return raw_len;

    switch (source.type) {
        case SourceType::Memory: {
            raw_len = source.u.memory.buf.len;
        } break;

        case SourceType::File: {
#ifdef _WIN32
            int fd = _fileno(source.u.file.fp);
            struct __stat64 sb;
            if (_fstat64(fd, &sb) < 0)
                return -1;
            raw_len = (int64_t)sb.st_size;
#else
            int fd = fileno(source.u.file.fp);
            struct stat sb;
            if (fstat(fd, &sb) < 0 || S_ISFIFO(sb.st_mode) | S_ISSOCK(sb.st_mode))
                return -1;
            raw_len = (int64_t)sb.st_size;
#endif
        } break;

        case SourceType::Function: {
            return -1;
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
            LogError("Deflate decompression not available for '%1'", filename);
            error = true;
            return false;
#endif
        } break;

        case CompressionType::Brotli: {
#ifdef BROTLI_DEFAULT_MODE
            compression.u.brotli =
                (BrotliDecompressContext *)Allocator::Allocate(nullptr, RG_SIZE(BrotliDecompressContext),
                                                               (int)Allocator::Flag::Zero);
            compression.u.brotli->state = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
#else
            LogError("Brotli decompression not available for '%1'", filename);
            error = true;
            return false;
#endif
        } break;
    }
    compression.type = type;

    return true;
}

#ifdef MZ_VERSION
Size StreamReader::ReadInflate(Size max_len, void *out_buf)
{
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
            if ((mz_crc32(MZ_CRC32_INIT, header, (size_t)header_offset) & 0xFFFF) == crc16) {
                LogError("Failed header CRC16 check in '%s'", filename);
                error = true;
                return -1;
            }
            header_offset += 2;
        }

        // Put back remaining data in the buffer
        memcpy_safe(ctx->in, header + header_offset, (size_t)(header_len - header_offset));
        ctx->in_ptr = ctx->in;
        ctx->in_len = header_len - header_offset;

        ctx->header_done = true;
    }

    // Inflate (with miniz)
    {
        Size read_len = 0;
        for (;;) {
            if (max_len < ctx->out_len) {
                memcpy_safe(out_buf, ctx->out_ptr, (size_t)max_len);
                read_len += max_len;
                ctx->out_ptr += max_len;
                ctx->out_len -= max_len;

                return read_len;
            } else {
                memcpy_safe(out_buf, ctx->out_ptr, (size_t)ctx->out_len);
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
                            memcpy_safe(footer, ctx->in_ptr, (size_t)ctx->in_len);

                            Size missing_len = RG_SIZE(footer) - ctx->in_len;
                            if (ReadRaw(missing_len, footer + ctx->in_len) < missing_len) {
                                if (error) {
                                    return -1;
                                } else {
                                    goto truncated_error;
                                }
                            }
                        } else {
                            memcpy_safe(footer, ctx->in_ptr, RG_SIZE(footer));
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
}
#endif

#ifdef BROTLI_DEFAULT_MODE
Size StreamReader::ReadBrotli(Size max_len, void *out_buf)
{
    BrotliDecompressContext *ctx = compression.u.brotli;

    for (;;) {
        if (ctx->out_len || ctx->done) {
            Size copy_len = std::min(max_len, ctx->out_len);

            ctx->out_len -= copy_len;
            memcpy(out_buf, ctx->out, copy_len);
            memmove(ctx->out, ctx->out + copy_len, ctx->out_len);

            eof = !ctx->out_len && ctx->done;
            return copy_len;
        }

        if (ctx->in_len < RG_SIZE(ctx->in)) {
            Size raw_len = ReadRaw(RG_SIZE(ctx->in) - ctx->in_len, ctx->in + ctx->in_len);
            if (raw_len < 0)
                return -1;
            ctx->in_len += raw_len;
        }

        const uint8_t *next_in = ctx->in;
        uint8_t *next_out = ctx->out + ctx->out_len;
        size_t avail_in = (size_t)ctx->in_len;
        size_t avail_out = (size_t)(RG_SIZE(ctx->out) - ctx->out_len);

        BrotliDecoderResult ret = BrotliDecoderDecompressStream(ctx->state, &avail_in, &next_in,
                                                                &avail_out, &next_out, nullptr);

        if (ret == BROTLI_DECODER_RESULT_SUCCESS) {
            ctx->done = true;
        } else if (ret == BROTLI_DECODER_RESULT_ERROR) {
            LogError("Malformed Brotli stream in '%1'", filename);
            error = true;
            return -1;
        } else if (ret == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
            LogError("Truncated Brotli stream in '%1'", filename);
            error = true;
            return -1;
        }

        ctx->out_len = next_out - ctx->out - ctx->out_len;
    }

    RG_UNREACHABLE();
}
#endif

Size StreamReader::ReadRaw(Size max_len, void *out_buf)
{
    ComputeRawLen();

    Size read_len = 0;
    switch (source.type) {
        case SourceType::Memory: {
            read_len = source.u.memory.buf.len - source.u.memory.pos;
            if (read_len > max_len) {
                read_len = max_len;
            }
            memcpy_safe(out_buf, source.u.memory.buf.ptr + source.u.memory.pos, (size_t)read_len);
            source.u.memory.pos += read_len;
            source.eof = (source.u.memory.pos >= source.u.memory.buf.len);
        } break;

        case SourceType::File: {
            clearerr(source.u.file.fp);

restart:
            read_len = (Size)fread(out_buf, 1, (size_t)max_len, source.u.file.fp);
            if (ferror(source.u.file.fp)) {
                if (errno == EINTR)
                    goto restart;

                LogError("Error while reading file '%1': %2", filename, strerror(errno));
                error = true;
                return -1;
            }
            source.eof = (bool)feof(source.u.file.fp);
        } break;

        case SourceType::Function: {
            read_len = source.u.func(MakeSpan((uint8_t *)out_buf, max_len));
            if (read_len < 0) {
                error = true;
                return -1;
            }
            source.eof = (read_len == 0);
        } break;
    }

    raw_read += read_len;
    return read_len;
}

// XXX: Maximum line length
bool LineReader::Next(Span<char> *out_line)
{
    if (eof) {
        line_number = 0;
        return false;
    }
    if (RG_UNLIKELY(error))
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
        memmove_safe(buf.ptr, line.ptr, (size_t)buf.len);
    }
}

void LineReader::PushLogFilter()
{
    RG::PushLogFilter([this](LogLevel level, const char *ctx, const char *msg, FunctionRef<LogFunc> func) {
        char ctx_buf[1024];

        if (line_number > 0) {
            Fmt(ctx_buf, "%1(%2)%3%4", st->GetFileName(), line_number, ctx ? ": " : "", ctx ? ctx : "");
        } else {
            Fmt(ctx_buf, "%1%2%3", st->GetFileName(), ctx ? ": " : "", ctx ? ctx : "");
        }

        func(level, ctx_buf, msg);
    });
}

#ifdef MZ_VERSION
struct MinizDeflateContext {
    tdefl_compressor deflator;

    // Gzip support
    uint32_t crc32;
    Size uncompressed_size;

    // Used to buffer small writes
    LocalArray<uint8_t, 1024> buf;
};
#endif

bool StreamWriter::Open(HeapArray<uint8_t> *mem, const char *filename,
                        CompressionType compression_type, CompressionSpeed compression_speed)
{
    Close(true);

    RG_DEFER_N(err_guard) { error = true; };
    error = false;

    this->filename = filename ? DuplicateString(filename, &str_alloc).ptr : "<memory>";

    dest.type = DestinationType::Memory;
    dest.u.mem.memory = mem;
    dest.u.mem.start = mem->len;
    dest.vt100 = false;

    if (!InitCompressor(compression_type, compression_speed))
        return false;

    err_guard.Disable();
    return true;
}

bool StreamWriter::Open(FILE *fp, const char *filename,
                        CompressionType compression_type, CompressionSpeed compression_speed)
{
    Close(true);

    RG_DEFER_N(err_guard) { error = true; };
    error = false;

    RG_ASSERT(fp);
    RG_ASSERT(filename);
    this->filename = DuplicateString(filename, &str_alloc).ptr;

    dest.type = DestinationType::File;
    memset_safe(&dest.u.file, 0, RG_SIZE(dest.u.file));
    dest.u.file.fp = fp;
    dest.vt100 = FileIsVt100(fp);

    if (!InitCompressor(compression_type, compression_speed))
        return false;

    err_guard.Disable();
    return true;
}

bool StreamWriter::Open(const char *filename, unsigned int flags,
                        CompressionType compression_type, CompressionSpeed compression_speed)
{
    Close(true);

    RG_DEFER_N(err_guard) { error = true; };
    error = false;

    RG_ASSERT(filename);
    this->filename = DuplicateString(filename, &str_alloc).ptr;

    dest.type = DestinationType::File;
    memset_safe(&dest.u.file, 0, RG_SIZE(dest.u.file));

    if (flags & (int)StreamWriterFlag::Atomic) {
        Span<const char> directory = GetPathDirectory(filename);

        if (flags & (int)StreamWriterFlag::Exclusive) {
            FILE *fp = OpenFile(filename, (int)OpenFileFlag::Write |
                                          (int)OpenFileFlag::Exclusive);
            if (!fp)
                return false;
            fclose(fp);

            dest.u.file.tmp_exclusive = true;
        }

        dest.u.file.tmp_filename = CreateTemporaryFile(directory, "", ".tmp", &str_alloc, &dest.u.file.fp);
        if (!dest.u.file.tmp_filename)
            return false;
        dest.u.file.owned = true;
    } else {
        unsigned int open_flags = (int)OpenFileFlag::Write;
        open_flags |= (flags & (int)StreamWriterFlag::Exclusive) ? (int)OpenFileFlag::Exclusive : 0;

        dest.u.file.fp = OpenFile(filename, open_flags);
        if (!dest.u.file.fp)
            return false;
        dest.u.file.owned = true;
    }
    dest.vt100 = FileIsVt100(dest.u.file.fp);

    if (!InitCompressor(compression_type, compression_speed))
        return false;

    err_guard.Disable();
    return true;
}

bool StreamWriter::Open(const std::function<bool(Span<const uint8_t>)> &func, const char *filename,
                        CompressionType compression_type, CompressionSpeed compression_speed)
{
    Close(true);

    RG_DEFER_N(err_guard) { error = true; };
    error = false;

    this->filename = filename ? DuplicateString(filename, &str_alloc).ptr : "<closure>";

    dest.type = DestinationType::Function;
    new (&dest.u.func) std::function<bool(Span<const uint8_t>)>(func);
    dest.vt100 = false;

    if (!InitCompressor(compression_type, compression_speed))
        return false;

    err_guard.Disable();
    return true;
}

bool StreamWriter::Flush()
{
    if (RG_UNLIKELY(error))
        return false;

    switch (dest.type) {
        case DestinationType::Memory: return true;
        case DestinationType::File: return FlushFile(dest.u.file.fp, filename);
        case DestinationType::Function: return true;
    }

    RG_UNREACHABLE();
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
            MinizDeflateContext *ctx = compression.u.miniz;

            if (ctx->buf.len) {
                Size copy_len = std::min(buf.len, ctx->buf.Available());

                memcpy_safe(ctx->buf.end(), buf.ptr, copy_len);
                ctx->buf.len += copy_len;
                buf.ptr += copy_len;
                buf.len -= copy_len;
            }

            if (buf.len) {
                if (ctx->buf.len && !WriteDeflate(ctx->buf))
                    return false;
                ctx->buf.Clear();

                if (buf.len >= RG_SIZE(ctx->buf.data) / 2) {
                    if (!WriteDeflate(buf))
                        return false;
                } else {
                    memcpy_safe(ctx->buf.data, buf.ptr, buf.len);
                    ctx->buf.len = buf.len;
                }
            }

            return true;
#endif
        } break;

        case CompressionType::Brotli: {
#ifdef BROTLI_DEFAULT_MODE
            return WriteBrotli(buf);
#endif
        } break;
    }

    RG_UNREACHABLE();
}

bool StreamWriter::Close(bool implicit)
{
    RG_ASSERT(implicit || this != &stdout_st);
    RG_ASSERT(implicit || this != &stderr_st);

    switch (compression.type) {
        case CompressionType::None: {} break;

        case CompressionType::Gzip:
        case CompressionType::Zlib: {
#ifdef MZ_VERSION
            MinizDeflateContext *ctx = compression.u.miniz;

            RG_DEFER { 
                Allocator::Release(nullptr, ctx, RG_SIZE(*ctx));
                compression.u.miniz = nullptr;
            };

            if (IsValid() && ctx) {
                if (ctx->buf.len && !WriteDeflate(ctx->buf)) {
                    error = true;
                    break;
                }

                uint8_t dummy; // Avoid UB in miniz
                tdefl_status status = tdefl_compress_buffer(&ctx->deflator, &dummy, 0, TDEFL_FINISH);
                if (status != TDEFL_STATUS_DONE) {
                    if (status != TDEFL_STATUS_PUT_BUF_FAILED) {
                        LogError("Failed to end Deflate stream for '%1", filename);
                    }

                    error = true;
                    break;
                }

                if (compression.type == CompressionType::Gzip) {
                    uint32_t gzip_footer[] = {
                        LittleEndian(ctx->crc32),
                        LittleEndian((uint32_t)ctx->uncompressed_size)
                    };

                    if (!WriteRaw(MakeSpan((uint8_t *)gzip_footer, RG_SIZE(gzip_footer)))) {
                        error = true;
                        break;
                    }
                }
            }
#endif
        } break;

        case CompressionType::Brotli: {
#ifdef BROTLI_DEFAULT_MODE
            BrotliEncoderState *state = compression.u.brotli;
            uint8_t output_buf[2048];

            if (state) {
                RG_DEFER {
                    BrotliEncoderDestroyInstance(state);
                    compression.u.brotli = nullptr;
                };

                do {
                    const uint8_t *next_in = nullptr;
                    uint8_t *next_out = output_buf;
                    size_t avail_in = 0;
                    size_t avail_out = RG_SIZE(output_buf);

                    if (!BrotliEncoderCompressStream(state, BROTLI_OPERATION_FINISH,
                                                     &avail_in, &next_in, &avail_out, &next_out, nullptr)) {
                        LogError("Failed to compress '%1' with Brotli", filename);
                        error = true;
                        break;
                    }
                    if (!WriteRaw(MakeSpan(output_buf, next_out - output_buf))) {
                        error = true;
                        break;
                    }
                } while (BrotliEncoderHasMoreOutput(state));
            }
#endif
        } break;
    }

    switch (dest.type) {
        case DestinationType::Memory: { dest.u.mem = {}; } break;

        case DestinationType::File: {
            if (IsValid() && !FlushFile(dest.u.file.fp, filename)) {
                error = true;
            }

            if (dest.u.file.tmp_filename) {
                if (IsValid() && implicit) {
                    LogDebug("Deleting implicitly closed file '%1'", filename);
                    error = true;
                }

                if (IsValid()) {
                    fclose(dest.u.file.fp);
                    dest.u.file.owned = false;

                    if (RenameFile(dest.u.file.tmp_filename, filename, true)) {
                        dest.u.file.tmp_filename = nullptr;
                        dest.u.file.tmp_exclusive = false;
                    } else {
                        error = true;
                    }
                } else {
                    error = true;
                }
            }

            if (dest.u.file.owned && dest.u.file.fp) {
                fclose(dest.u.file.fp);
            }

            // Try to clean up, though we can't do much if that fails (except log error)
            if (dest.u.file.tmp_filename) {
                UnlinkFile(dest.u.file.tmp_filename);
            }
            if (dest.u.file.tmp_exclusive && filename) {
                UnlinkFile(filename);
            }

            memset_safe(&dest.u.file, 0, RG_SIZE(dest.u.file));
        } break;

        case DestinationType::Function: {
            error |= IsValid() && !dest.u.func({});
            dest.u.func.~function();
        } break;
    }

    bool ret = !filename || !error;

    filename = nullptr;
    error = true;
    compression.type = CompressionType::None;
    dest.type = DestinationType::Memory;
    str_alloc.ReleaseAll();

    return ret;
}

bool StreamWriter::InitCompressor(CompressionType type, CompressionSpeed speed)
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

            int flags = 0;
            switch (speed) {
                case CompressionSpeed::Default: { flags = 32 | TDEFL_GREEDY_PARSING_FLAG; } break;
                case CompressionSpeed::Slow: { flags = 512; } break;
                case CompressionSpeed::Fast: { flags = 1 | TDEFL_GREEDY_PARSING_FLAG; } break;
            }
            flags |= (type == CompressionType::Zlib ? TDEFL_WRITE_ZLIB_HEADER : 0);

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
                    8,          // Deflate
                    0,          // FLG
                    0, 0, 0, 0, // MTIME
                    0,          // XFL
                    0           // OS
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

        case CompressionType::Brotli: {
#ifdef BROTLI_DEFAULT_MODE
            BrotliEncoderState *state = BrotliEncoderCreateInstance(nullptr, nullptr, nullptr);
            compression.u.brotli = state;

            RG_STATIC_ASSERT(BROTLI_MIN_QUALITY == 0 && BROTLI_MAX_QUALITY == 11);

            switch (speed) {
                case CompressionSpeed::Default: { BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, 6); } break;
                case CompressionSpeed::Slow: { BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, 11); } break;
                case CompressionSpeed::Fast: { BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, 0); } break;
            }
#else
            LogError("Brotli compression not available for '%1'", filename);
            error = true;
            return false;
#endif
        } break;
    }

    compression.type = type;
    compression.speed = speed;

    return true;
}

#ifdef MZ_VERSION
bool StreamWriter::WriteDeflate(Span<const uint8_t> buf)
{
    MinizDeflateContext *ctx = compression.u.miniz;

    if (compression.type == CompressionType::Gzip) {
        ctx->crc32 = (uint32_t)mz_crc32(ctx->crc32, buf.ptr, (size_t)buf.len);
        ctx->uncompressed_size += buf.len;
    }

    tdefl_status status = tdefl_compress_buffer(&ctx->deflator, buf.ptr, (size_t)buf.len, TDEFL_NO_FLUSH);
    if (status < TDEFL_STATUS_OKAY) {
        if (status != TDEFL_STATUS_PUT_BUF_FAILED) {
            LogError("Failed to deflate stream to '%1'", filename);
        }

        error = true;
        return false;
    }

    return true;
}
#endif

#ifdef BROTLI_DEFAULT_MODE
bool StreamWriter::WriteBrotli(Span<const uint8_t> buf)
{
    BrotliEncoderState *state = compression.u.brotli;
    uint8_t output_buf[2048];

    while (buf.len || BrotliEncoderHasMoreOutput(state)) {
        const uint8_t *next_in = buf.ptr;
        uint8_t *next_out = output_buf;
        size_t avail_in = (size_t)buf.len;
        size_t avail_out = RG_SIZE(output_buf);

        if (!BrotliEncoderCompressStream(state, BROTLI_OPERATION_PROCESS,
                                         &avail_in, &next_in, &avail_out, &next_out, nullptr)) {
            error = true;
            return false;
        }
        if (!WriteRaw(MakeSpan(output_buf, next_out - output_buf))) {
            error = true;
            return false;
        }

        buf.len -= next_in - buf.ptr;
        buf.ptr = next_in;
    }

    return true;
}
#endif

bool StreamWriter::WriteRaw(Span<const uint8_t> buf)
{
    switch (dest.type) {
        case DestinationType::Memory: {
            // dest.u.memory->Append(buf) would work but it's probably slower
            dest.u.mem.memory->Grow(buf.len);
            memcpy_safe(dest.u.mem.memory->ptr + dest.u.mem.memory->len, buf.ptr, (size_t)buf.len);
            dest.u.mem.memory->len += buf.len;

            return true;
        } break;

        case DestinationType::File: {
            while (buf.len) {
                size_t write_len = fwrite(buf.ptr, 1, (size_t)buf.len, dest.u.file.fp);

                if (ferror(dest.u.file.fp)) {
                    if (errno == EINTR) {
                        clearerr(dest.u.file.fp);
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

        case DestinationType::Function: {
            // Empty writes are used to "close" the file.. don't!
            if (!buf.len)
                return true;

            bool ret = dest.u.func(buf);
            error |= !ret;
            return ret;
        } break;
    }

    RG_UNREACHABLE();
}

bool SpliceStream(StreamReader *reader, int64_t max_len, StreamWriter *writer)
{
    if (!reader->IsValid())
        return false;

    int64_t total_len = 0;
    do {
        LocalArray<uint8_t, 16384> buf;
        buf.len = reader->Read(buf.data);
        if (buf.len < 0)
            return false;

        if (RG_UNLIKELY(max_len >= 0 && buf.len > max_len - total_len)) {
            LogError("File '%1' is too large (limit = %2)", reader->GetFileName(), FmtDiskSize(max_len));
            return false;
        }
        total_len += buf.len;

        if (!writer->Write(buf))
            return false;
    } while (!reader->IsEOF());

    return true;
}

// ------------------------------------------------------------------------
// INI
// ------------------------------------------------------------------------

static bool CheckIniKey(Span<const char> key)
{
    const auto test_char = [](char c) { return IsAsciiAlphaOrDigit(c) || c == '_' ||
                                               c == '-' || c == '.' || c == '/'; };

    if (!key.len) {
        LogError("INI key cannot be empty");
        return false;
    }
    if (!std::all_of(key.begin(), key.end(), test_char)) {
        LogError("INI key must only contain alphanumeric, '.', '-' or '_' characters");
        return false;
    }

    return true;
}

IniParser::LineType IniParser::FindNextLine(IniProperty *out_prop)
{
    if (RG_UNLIKELY(error))
        return LineType::Exit;

    RG_DEFER_N(err_guard) { error = true; };

    Span<char> line;
    while (reader.Next(&line)) {
        line = TrimStr(line);

        if (!line.len || line[0] == ';' || line[0] == '#') {
            // Ignore this line (empty or comment)
        } else if (line[0] == '[') {
            if (line.len < 2 || line[line.len - 1] != ']') {
                LogError("Malformed [section] line");
                return LineType::Exit;
            }

            Span<const char> section = TrimStr(line.Take(1, line.len - 2));
            if (!section.len) {
                LogError("Empty section name");
                return LineType::Exit;
            }

            current_section.RemoveFrom(0);
            current_section.Append(section);

            err_guard.Disable();
            return LineType::Section;
        } else {
            Span<char> value;
            Span<const char> key = TrimStr(SplitStr(line, '=', &value));
            if (!key.len || key.end() == line.end()) {
                LogError("Expected [section] or <key> = <value> pair");
                return LineType::Exit;
            }
            if (!CheckIniKey(key))
                return LineType::Exit;
            value = TrimStr(value);
            *value.end() = 0;

            out_prop->section = current_section;
            out_prop->key = key;
            out_prop->value = value;

            err_guard.Disable();
            return LineType::KeyValue;
        }
    }
    if (!reader.IsValid())
        return LineType::Exit;

    eof = true;

    err_guard.Disable();
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
// Assets
// ------------------------------------------------------------------------

#ifdef FELIX_HOT_ASSETS

static char assets_filename[4096];
static int64_t assets_last_check = -1;
static HeapArray<AssetInfo> assets;
static HashTable<const char *, const AssetInfo *> assets_map;
static BlockAllocator assets_alloc;
static bool assets_ready;

bool ReloadAssets()
{
    const Span<const AssetInfo> *lib_assets = nullptr;

    // Make asset library filename
    if (!assets_filename[0]) {
        Span<const char> prefix = GetApplicationExecutable();
#ifdef _WIN32
        SplitStrReverse(prefix, '.', &prefix);
#endif

        Fmt(assets_filename, "%1_assets%2", prefix, RG_SHARED_LIBRARY_EXTENSION);
    }

    // Check library time
    {
        FileInfo file_info;
        if (!StatFile(assets_filename, &file_info))
            return false;

        if (assets_last_check == file_info.mtime)
            return false;
        assets_last_check = file_info.mtime;
    }

#ifdef _WIN32
    HMODULE h;
    if (win32_utf8) {
        h = LoadLibraryA(assets_filename);
    } else {
        wchar_t filename_w[4096];
        if (ConvertUtf8ToWin32Wide(assets_filename, filename_w) < 0)
            return false;

        h = LoadLibraryW(filename_w);
    }
    if (!h) {
        LogError("Cannot load library '%1'", assets_filename);
        return false;
    }
    RG_DEFER { FreeLibrary(h); };

    lib_assets = (const Span<const AssetInfo> *)GetProcAddress(h, "PackedAssets");
#else
    void *h = dlopen(assets_filename, RTLD_LAZY | RTLD_LOCAL);
    if (!h) {
        LogError("Cannot load library '%1': %2", assets_filename, dlerror());
        return false;
    }
    RG_DEFER { dlclose(h); };

    lib_assets = (const Span<const AssetInfo> *)dlsym(h, "PackedAssets");
#endif
    if (!lib_assets) {
        LogError("Cannot find symbol '%1' in library '%2'", "PackedAssets", assets_filename);
        return false;
    }

    // We are not allowed to fail from now on
    assets.Clear();
    assets_map.Clear();
    assets_alloc.ReleaseAll();

    for (const AssetInfo &asset: *lib_assets) {
        AssetInfo asset_copy;

        asset_copy.name = DuplicateString(asset.name, &assets_alloc).ptr;
        uint8_t *data_ptr = (uint8_t *)Allocator::Allocate(&assets_alloc, asset.data.len);
        memcpy_safe(data_ptr, asset.data.ptr, (size_t)asset.data.len);
        asset_copy.data = {data_ptr, asset.data.len};
        asset_copy.compression_type = asset.compression_type;
        asset_copy.source_map = DuplicateString(asset.source_map, &assets_alloc).ptr;

        assets.Append(asset_copy);
    }
    for (const AssetInfo &asset: assets) {
        assets_map.Set(&asset);
    }

    assets_ready = true;
    return true;
}

Span<const AssetInfo> GetPackedAssets()
{
    if (!assets_ready) {
        ReloadAssets();
        RG_ASSERT(assets_ready);
    }

    return assets;
}

const AssetInfo *FindPackedAsset(const char *name)
{
    if (!assets_ready) {
        ReloadAssets();
        RG_ASSERT(assets_ready);
    }

    return assets_map.FindValue(name, nullptr);
}

#else

HashTable<const char *, const AssetInfo *> PackedAssets_map;
static bool assets_ready;

void InitPackedMap(Span<const AssetInfo> assets)
{
    if (RG_LIKELY(!assets_ready)) {
        for (const AssetInfo &asset: assets) {
            PackedAssets_map.Set(&asset);
        }
    }
}

#endif

// This won't win any beauty or speed contest (especially when writing
// a compressed stream) but whatever.
Span<const uint8_t> PatchAsset(const AssetInfo &asset, Allocator *alloc,
                               FunctionRef<void(const char *, StreamWriter *)> func)
{
    RG_ASSERT(alloc);

    HeapArray<uint8_t> buf(alloc);

    StreamReader reader(asset.data, nullptr, asset.compression_type);
    StreamWriter writer(&buf, nullptr, asset.compression_type);

    char c;
    while (reader.Read(1, &c) == 1) {
        if (c == '{') {
            char name[33] = {};
            Size name_len = reader.Read(1, &name[0]);
            RG_ASSERT(name_len >= 0);

            if (IsAsciiAlpha(name[0]) || name[0] == '_') {
                do {
                    Size read_len = reader.Read(1, &name[name_len]);
                    RG_ASSERT(read_len >= 0);

                    if (name[name_len] == '}') {
                        name[name_len] = 0;
                        func(name, &writer);

                        break;
                    } else if (!IsAsciiAlphaOrDigit(name[name_len]) && name[name_len] != '_') {
                        writer.Write('{');
                        writer.Write(name, name_len + 1);

                        break;
                    }
                } while (++name_len < RG_SIZE(name));
            } else {
                writer.Write('{');
                writer.Write(name[0]);
            }
        } else {
            writer.Write(c);
        }
    }
    RG_ASSERT(reader.IsValid());

    bool success = writer.Close();
    RG_ASSERT(success);

    return buf.Leak();
}

// ------------------------------------------------------------------------
// Option parser
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

const char *OptionParser::Next()
{
    current_option = nullptr;
    current_value = nullptr;
    test_failed = false;

    // Support aggregate short options, such as '-fbar'. Note that this can also be
    // parsed as the short option '-f' with value 'bar', if the user calls
    // ConsumeOptionValue() after getting '-f'.
    if (smallopt_offset) {
        const char *opt = args[pos];

        buf[1] = opt[smallopt_offset];
        current_option = buf;

        if (!opt[++smallopt_offset]) {
            smallopt_offset = 0;
            pos++;
        }

        return current_option;
    }

    if (mode == OptionMode::Stop && (pos >= limit || !IsOption(args[pos]))) {
        limit = pos;
        return nullptr;
    }

    // Skip non-options, do the permutation once we reach an option or the last argument
    Size next_index = pos;
    while (next_index < limit && !IsOption(args[next_index])) {
        next_index++;
    }
    if (mode == OptionMode::Rotate) {
        std::rotate(args.ptr + pos, args.ptr + next_index, args.end());
        limit -= (next_index - pos);
    } else if (mode == OptionMode::Skip) {
        pos = next_index;
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
            memcpy_safe(buf, opt, (size_t)len);
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
        std::rotate(args.ptr + pos + 1, args.ptr + limit, args.end());
        limit = pos;
        pos++;
    } else if (opt[2]) {
        // We either have aggregated short options or one short option with a value,
        // depending on whether or not the user calls ConsumeOptionValue().
        buf[0] = '-';
        buf[1] = opt[1];
        buf[2] = 0;
        current_option = buf;
        smallopt_offset = opt[2] ? 2 : 0;

        // The main point of Skip mode is to be able to parse arguments in
        // multiple passes. This does not work well with ambiguous short options
        // (such as -oOption, which can be interpeted as multiple one-char options
        // or one -o option with a value), so force the value interpretation.
        if (mode == OptionMode::Skip) {
            ConsumeValue();
        }
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
    if (smallopt_offset == 2 && args[pos][2]) {
        smallopt_offset = 0;
        current_value = args[pos] + 2;
        pos++;
    // Support '-f bar' and '--foo bar', see ConsumeOption() for '--foo=bar'
    } else if (current_option != buf && pos < limit && !IsOption(args[pos])) {
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

bool OptionParser::Test(const char *test1, const char *test2, OptionType type)
{
    RG_ASSERT(test1 && IsOption(test1));
    RG_ASSERT(!test2 || IsOption(test2));

    if (TestStr(test1, current_option) || (test2 && TestStr(test2, current_option))) {
        switch (type) {
            case OptionType::NoValue: {
                if (current_value) {
                    LogError("Option '%1' does not support values", current_option);
                    test_failed = true;
                    return false;
                }
            } break;
            case OptionType::Value: {
                if (!ConsumeValue()) {
                    LogError("Option '%1' requires a value", current_option);
                    test_failed = true;
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

void OptionParser::LogUnknownError() const
{
    if (!TestHasFailed()) {
        LogError("Unknown option '%1'", current_option);
    }
}

// ------------------------------------------------------------------------
// Console prompter (simplified readline)
// ------------------------------------------------------------------------

static bool input_is_raw;
#ifdef _WIN32
static HANDLE stdin_handle;
static DWORD input_orig_mode;
#else
static struct termios input_orig_tio;
#endif

ConsolePrompter::ConsolePrompter()
{
    entries.AppendDefault();
}

static bool EnableRawMode()
{
#if defined(__EMSCRIPTEN__)
    return false;
#elif defined(_WIN32)
    static bool init_atexit = false;

    if (!input_is_raw) {
        stdin_handle = (HANDLE)_get_osfhandle(_fileno(stdin));

        if (GetConsoleMode(stdin_handle, &input_orig_mode)) {
            input_is_raw = SetConsoleMode(stdin_handle, ENABLE_WINDOW_INPUT);

            if (input_is_raw && !init_atexit) {
                atexit([]() { SetConsoleMode(stdin_handle, input_orig_mode); });
                init_atexit = true;
            }
        }
    }

    return input_is_raw;
#else
    static bool init_atexit = false;

    if (!input_is_raw) {
        if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &input_orig_tio) >= 0) {
            struct termios raw = input_orig_tio;
            cfmakeraw(&raw);

            input_is_raw = (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) >= 0);

            if (input_is_raw && !init_atexit) {
                atexit([]() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &input_orig_tio); });
                init_atexit = true;
            }
        }
    }

    return input_is_raw;
#endif
}

static void DisableRawMode()
{
    if (input_is_raw) {
#ifdef _WIN32
        input_is_raw = !SetConsoleMode(stdin_handle, input_orig_mode);
#else
        input_is_raw = !(tcsetattr(STDIN_FILENO, TCSAFLUSH, &input_orig_tio) >= 0);
#endif
    }
}

#ifndef _WIN32
static void IgnoreSigWinch(struct sigaction *old_sa)
{
    struct sigaction sa;

    sa.sa_handler = [](int) {};
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGWINCH, &sa, old_sa);
}
#endif

bool ConsolePrompter::Read(Span<const char> *out_str)
{
#ifndef _WIN32
    struct sigaction old_sa;
    IgnoreSigWinch(&old_sa);
    RG_DEFER { sigaction(SIGWINCH, &old_sa, nullptr); };
#endif

    if (FileIsVt100(stderr) && EnableRawMode()) {
        RG_DEFER {
            Print(stderr, "%!0");
            DisableRawMode();
        };

        return ReadRaw(out_str);
    } else {
        return ReadBuffered(out_str);
    }
}

bool ConsolePrompter::ReadYN(bool *out_value)
{
#ifndef _WIN32
    struct sigaction old_sa;
    IgnoreSigWinch(&old_sa);
    RG_DEFER { sigaction(SIGWINCH, &old_sa, nullptr); };
#endif

    if (FileIsVt100(stderr) && EnableRawMode()) {
        RG_DEFER {
            Print(stderr, "%!0");
            DisableRawMode();
        };

        return ReadRawYN(out_value);
    } else {
        return ReadBufferedYN(out_value);
    }
}

void ConsolePrompter::Commit()
{
    str.len = TrimStrRight(str.Take(), "\r\n").len;

    if (str.len) {
        std::swap(str, entries[entries.len - 1]);
        entries.AppendDefault();
    }
    entry_idx = entries.len - 1;
    str.RemoveFrom(0);
    str_offset = 0;

    rows = 0;
    rows_with_extra = 0;
    x = 0;
    y = 0;
}

bool ConsolePrompter::ReadRaw(Span<const char> *out_str)
{
    fflush(stderr);

    prompt_columns = ComputeWidth(prompt);

    str_offset = str.len;
    RenderRaw();

    int32_t uc;
    while ((uc = ReadChar()) >= 0) {
        // Fix display if terminal is resized
        if (GetConsoleSize().x != columns) {
            RenderRaw();
        }

        switch (uc) {
            case 0x1B: {
                LocalArray<char, 16> buf;

                const auto match_escape = [&](const char *seq) {
                    RG_ASSERT(strlen(seq) < RG_SIZE(buf.data));

                    for (Size i = 0; seq[i]; i++) {
                        if (i >= buf.len) {
                            uc = ReadChar();

                            if (uc >= 128) {
                                // Got some kind of non-ASCII character, make sure nothing else matches
                                buf.Append(0);
                                return false;
                            }

                            buf.Append((char)uc);
                        }
                        if (buf[i] != seq[i])
                            return false;
                    }

                    return true;
                };

                if (match_escape("[1;5D")) { // Ctrl-Left
                    str_offset = FindBackward(str_offset, " \t\r\n");
                    RenderRaw();
                } else if (match_escape("[1;5C")) { // Ctrl-Right
                    str_offset = FindForward(str_offset, " \t\r\n");
                    RenderRaw();
                } else if (match_escape("[3~")) { // Delete
                    if (str_offset < str.len) {
                        Delete(str_offset, SkipForward(str_offset, 1));
                        RenderRaw();
                    }
                } else if (match_escape("\x7F")) { // Alt-Backspace
                    Delete(FindBackward(str_offset, " \t\r\n"), str_offset);
                    RenderRaw();
                } else if (match_escape("d")) { // Alt-D
                    Delete(str_offset, FindForward(str_offset, " \t\r\n"));
                    RenderRaw();
                } else if (match_escape("[A")) { // Up
                    fake_input = "\x10";
                } else if (match_escape("[B")) { // Down
                    fake_input = "\x0E";
                } else if (match_escape("[D")) { // Left
                    fake_input = "\x02";
                } else if (match_escape("[C")) { // Right
                    fake_input = "\x06";
                } else if (match_escape("[H")) { // Home
                    fake_input = "\x01";
                } else if (match_escape("[F")) { // End
                    fake_input = "\x05";
                }
            } break;

            case 0x2: { // Left
                if (str_offset > 0) {
                    str_offset = SkipBackward(str_offset, 1);
                    RenderRaw();
                }
            } break;
            case 0x6: { // Right
                if (str_offset < str.len) {
                    str_offset = SkipForward(str_offset, 1);
                    RenderRaw();
                }
            } break;
            case 0xE: { // Down
                Span<const char> remain = str.Take(str_offset, str.len - str_offset);
                SplitStr(remain, '\n', &remain);

                if (remain.len) {
                    Span<const char> line = SplitStr(remain, '\n', &remain);

                    Size line_offset = std::min(line.len, (Size)x - prompt_columns);
                    str_offset = std::min((Size)(line.ptr - str.ptr + line_offset), str.len);

                    RenderRaw();
                } else if (entry_idx < entries.len - 1) {
                    ChangeEntry(entry_idx + 1);
                    RenderRaw();
                }
            } break;
            case 0x10: { // Up
                Span<const char> remain = str.Take(0, str_offset);
                SplitStrReverse(remain, '\n', &remain);

                if (remain.len) {
                    Span<const char> line = SplitStrReverse(remain, '\n', &remain);

                    Size line_offset = std::min(line.len, (Size)x - prompt_columns);
                    str_offset = std::min((Size)(line.ptr - str.ptr + line_offset), str.len);

                    RenderRaw();
                } else if (entry_idx > 0) {
                    ChangeEntry(entry_idx - 1);
                    RenderRaw();
                }
            } break;

            case 0x1: { // Home
                str_offset = FindBackward(str_offset, "\n");
                RenderRaw();
            } break;
            case 0x5: { // End
                str_offset = FindForward(str_offset, "\n");
                RenderRaw();
            } break;

            case 0x8:
            case 0x7F: { // Backspace
                if (str.len) {
                    Delete(SkipBackward(str_offset, 1), str_offset);
                    RenderRaw();
                }
            } break;
            case 0x3: { // Ctrl-C
                if (str.len) {
                    str.RemoveFrom(0);
                    str_offset = 0;
                    entry_idx = entries.len - 1;
                    entries[entry_idx].RemoveFrom(0);

                    RenderRaw();
                } else {
                    fputs("\r\n", stderr);
                    fflush(stderr);
                    return false;
                }
            } break;
            case 0x4: { // Ctrl-D
                if (str.len) {
                    Delete(str_offset, SkipForward(str_offset, 1));
                    RenderRaw();
                } else {
                    return false;
                }
            } break;
            case 0x14: { // Ctrl-T
                Size middle = SkipBackward(str_offset, 1);
                Size start = SkipBackward(middle, 1);

                if (start < middle) {
                    std::rotate(str.ptr + start, str.ptr + middle, str.ptr + str_offset);
                    RenderRaw();
                }
            } break;
            case 0xB: { // Ctrl-K
                Delete(str_offset, FindForward(str_offset, "\n"));
                RenderRaw();
            } break;
            case 0x15: { // Ctrl-U
                Delete(FindBackward(str_offset, "\n"), str_offset);
                RenderRaw();
            } break;
            case 0xC: { // Ctrl-L
                fputs("\x1B[2J\x1B[999A", stderr);
                RenderRaw();
            } break;

            case '\r':
            case '\n': {
                if (rows > y) {
                    fprintf(stderr, "\x1B[%dB", rows - y);
                }
                fputs("\r\n", stderr);
                fflush(stderr);
                y = rows + 1;

                EnsureNulTermination();
                if (out_str) {
                    *out_str = str;
                }
                return true;
            } break;

            default: {
                LocalArray<char, 16> frag;
                if (uc == '\t') {
                    frag.Append("    ");
                } else if (uc >= 32) {
                    frag.len = EncodeUtf8(uc, frag.data);
                } else {
                    continue;
                }

                str.Grow(frag.len);
                memmove_safe(str.ptr + str_offset + frag.len, str.ptr + str_offset, (size_t)(str.len - str_offset));
                memcpy_safe(str.ptr + str_offset, frag.data, (size_t)frag.len);
                str.len += frag.len;
                str_offset += frag.len;

                if (!mask && str_offset == str.len && uc < 128 && x + frag.len < columns) {
                    fwrite(frag.data, 1, (size_t)frag.len, stderr);
                    fflush(stderr);
                    x += (int)frag.len;
                } else {
                    RenderRaw();
                }
            } break;
        }
    }

    EnsureNulTermination();
    if (out_str) {
        *out_str = str;
    }
    return true;
}

bool ConsolePrompter::ReadRawYN(bool *out_value)
{
    const char *yn = "[Y/N]";

    fflush(stderr);

    prompt_columns = ComputeWidth(prompt) + ComputeWidth(yn) + 1;

    str.RemoveFrom(0);
    str_offset = 0;
    RenderRaw();
    Print(stderr, "%!D..%1%!0 ", yn);

    int32_t uc;
    while ((uc = ReadChar()) >= 0) {
        // Fix display if terminal is resized
        if (GetConsoleSize().x != columns) {
            RenderRaw();
            Print(stderr, "%!D..[Y/N]%!0 ");
        }

        switch (uc) {
            case 0x3: { // Ctrl-C
                fputs("\r\n", stderr);
                fflush(stderr);

                return false;
            } break;
            case 0x4: { // Ctrl-D
                return false;
            } break;

            case 'Y':
            case 'y': {
                fputs("Y\n", stderr);
                fflush(stderr);

                *out_value = true;
                return true;
            } break;
            case 'N':
            case 'n': {
                fputs("N\n", stderr);
                fflush(stderr);

                *out_value = false;
                return true;
            } break;
        }
    }

    return false;
}

bool ConsolePrompter::ReadBuffered(Span<const char> *out_str)
{
    prompt_columns = ComputeWidth(prompt);

    RenderBuffered();

    int c;
    while ((c = fgetc(stdin)) != EOF) {
        if (c == '\n') {
            EnsureNulTermination();
            if (out_str) {
                *out_str = str;
            }
            return true;
        } else if (c >= 32 || c == '\t') {
            str.Append((char)c);
        }
    }

    if (ferror(stdin)) {
        LogError("Failed to read from standard input: %1", strerror(errno));
        return false;
    }

    // EOF
    return false;
}

bool ConsolePrompter::ReadBufferedYN(bool *out_value)
{
    const char *yn = "[Yes/No]";

    prompt_columns = ComputeWidth(prompt) + ComputeWidth(yn) + 1;

    for (;;) {
        str.RemoveFrom(0);
        str_offset = 0;
        RenderBuffered();
        Print(stderr, "%1 ", yn);

        int c;
        while ((c = fgetc(stdin)) != EOF) {
            if (c == '\n') {
                if (TestStrI(str, "y") || TestStrI(str, "yes")) {
                    *out_value = true;
                    return true;
                } else if (TestStrI(str, "n") || TestStrI(str, "no")) {
                    *out_value = false;
                    return true;
                } else {
                    break;
                }
            } else if (c >= 32 || c == '\t') {
                str.Append((char)c);
            }
        }

        if (ferror(stdin)) {
            LogError("Failed to read from standard input: %1", strerror(errno));
            return false;
        } else if (feof(stdin)) {
            return false;
        }
    }
}

void ConsolePrompter::ChangeEntry(Size new_idx)
{
    if (str.len) {
        std::swap(str, entries[entry_idx]);
    }

    str.RemoveFrom(0);
    str.Append(entries[new_idx]);
    str_offset = str.len;
    entry_idx = new_idx;
}

Size ConsolePrompter::SkipForward(Size offset, Size count)
{
    if (offset < str.len) {
        offset++;

        while (offset < str.len && (((str[offset] & 0xC0) == 0x80) || --count)) {
            offset++;
        }
    }

    return offset;
}

Size ConsolePrompter::SkipBackward(Size offset, Size count)
{
    if (offset > 0) {
        offset--;

        while (offset > 0 && (((str[offset] & 0xC0) == 0x80) || --count)) {
            offset--;
        }
    }

    return offset;
}

Size ConsolePrompter::FindForward(Size offset, const char *chars)
{
    while (offset < str.len && strchr(chars, str[offset])) {
        offset++;
    }
    while (offset < str.len && !strchr(chars, str[offset])) {
        offset++;
    }

    return offset;
}

Size ConsolePrompter::FindBackward(Size offset, const char *chars)
{
    if (offset > 0) {
        offset--;

        while (offset > 0 && strchr(chars, str[offset])) {
            offset--;
        }
        while (offset > 0 && !strchr(chars, str[offset - 1])) {
            offset--;
        }
    }

    return offset;
}

void ConsolePrompter::Delete(Size start, Size end)
{
    RG_ASSERT(start >= 0);
    RG_ASSERT(end >= start && end <= str.len);

    memmove_safe(str.ptr + start, str.ptr + end, str.len - end);
    str.len -= end - start;

    if (str_offset > end) {
        str_offset -= end - start;
    } else if (str_offset > start) {
        str_offset = start;
    }
}

void ConsolePrompter::RenderRaw()
{
    columns = GetConsoleSize().x;
    rows = 0;

    int mask_columns = mask ? ComputeWidth(mask) : 0;

    // Hide cursor during refresh
    fprintf(stderr, "\x1B[?25l");
    if (y) {
        fprintf(stderr, "\x1B[%dA", y);
    }

    // Output prompt(s) and string lines
    {
        Size i = 0;
        int x2 = prompt_columns;

        Print(stderr, "\r%!0%1%!..+", prompt);

        for (;;) {
            if (i == str_offset) {
                x = x2;
                y = rows;
            }
            if (i >= str.len)
                break;

            Size bytes = std::min((Size)CountUtf8Bytes(str[i]), str.len - i);
            int width = mask ? mask_columns : ComputeWidth(str.Take(i, bytes));

            if (x2 + width >= columns || str[i] == '\n') {
                FmtArg prefix = FmtArg(str[i] == '\n' ? '.' : ' ').Repeat(prompt_columns - 1);
                Print(stderr, "\x1B[0K\r\n%!D.+%1%!0 %!..+", prefix);

                x2 = prompt_columns;
                rows++;
            }
            if (width > 0) {
                if (mask) {
                    fputs(mask, stderr);
                } else {
                    fwrite(str.ptr + i, 1, (size_t)bytes, stderr);
                }
            }

            x2 += width;
            i += bytes;
        }
        fputs("\x1B[0K", stderr);
    }

    // Clear remaining rows
    for (int i = rows; i < rows_with_extra; i++) {
        fprintf(stderr, "\r\n\x1B[0K");
    }
    rows_with_extra = std::max(rows_with_extra, rows);

    // Fix up cursor and show it
    if (rows_with_extra > y) {
        fprintf(stderr, "\x1B[%dA", rows_with_extra - y);
    }
    fprintf(stderr, "\r\x1B[%dC", x);
    fprintf(stderr, "\x1B[?25h");

    fflush(stderr);
}

void ConsolePrompter::RenderBuffered()
{
    Span<const char> remain = str;
    Span<const char> line = SplitStr(remain, '\n', &remain);

    Print(stderr, "%1%2", prompt, line);
    while (remain.len) {
        line = SplitStr(remain, '\n', &remain);
        Print(stderr, "\n%1 %2", FmtArg('.').Repeat(prompt_columns - 1), line);
    }
}

Vec2<int> ConsolePrompter::GetConsoleSize()
{
#ifdef _WIN32
    HANDLE h = (HANDLE)_get_osfhandle(_fileno(stderr));

    CONSOLE_SCREEN_BUFFER_INFO screen;
    if (GetConsoleScreenBufferInfo(h, &screen))
        return {screen.dwSize.X, screen.dwSize.Y};
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) >= 0 && ws.ws_col)
        return {ws.ws_col, ws.ws_row};
#endif

    // Give up!
    return {80, 24};
}

int32_t ConsolePrompter::ReadChar()
{
    if (fake_input[0]) {
        int c = fake_input[0];
        fake_input++;
        return c;
    }

#ifdef _WIN32
    HANDLE h = (HANDLE)_get_osfhandle(_fileno(stdin));

    for (;;) {
        INPUT_RECORD ev;
        DWORD ev_len;
        if (!ReadConsoleInputW(h, &ev, 1, &ev_len))
            return -1;
        if (!ev_len)
            return -1;

        if (ev.EventType == KEY_EVENT && ev.Event.KeyEvent.bKeyDown) {
            bool ctrl = ev.Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED);
            bool alt = ev.Event.KeyEvent.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED);

            if (ctrl && !alt) {
                switch (ev.Event.KeyEvent.wVirtualKeyCode) {
                    case 'A': return 0x1;
                    case 'B': return 0x2;
                    case 'C': return 0x3;
                    case 'D': return 0x4;
                    case 'E': return 0x5;
                    case 'F': return 0x6;
                    case 'H': return 0x8;
                    case 'K': return 0xB;
                    case 'L': return 0xC;
                    case 'N': return 0xE;
                    case 'P': return 0x10;
                    case 'T': return 0x14;
                    case 'U': return 0x15;
                    case VK_LEFT: {
                        fake_input = "[1;5D";
                        return 0x1B;
                    } break;
                    case VK_RIGHT: {
                        fake_input = "[1;5C";
                        return 0x1B;
                    } break;
                }
            } else {
                if (alt) {
                    switch (ev.Event.KeyEvent.wVirtualKeyCode) {
                        case VK_BACK: {
                            fake_input = "\x7F";
                            return 0x1B;
                        } break;
                        case 'D': {
                            fake_input = "d";
                            return 0x1B;
                        } break;
                    }
                }

                switch (ev.Event.KeyEvent.wVirtualKeyCode) {
                    case VK_UP: return 0x10;
                    case VK_DOWN: return 0xE;
                    case VK_LEFT: return 0x2;
                    case VK_RIGHT: return 0x6;
                    case VK_HOME: return 0x1;
                    case VK_END: return 0x5;
                    case VK_RETURN: return '\r';
                    case VK_BACK: return 0x8;
                    case VK_DELETE: {
                        fake_input = "[3~";
                        return 0x1B;
                    } break;

                    default: {
                        uint32_t uc = ev.Event.KeyEvent.uChar.UnicodeChar;

                        if ((uc - 0xD800u) < 0x800u) {
                            if ((uc & 0xFC00u) == 0xD800u) {
                                surrogate_buf = uc;
                                return 0;
                            } else if (surrogate_buf && (uc & 0xFC00) == 0xDC00) {
                                uc = (surrogate_buf << 10) + uc - 0x35FDC00;
                            } else {
                                // Yeah something is up. Give up on this character.
                                surrogate_buf = 0;
                                return 0;
                            }
                        }

                        return (int32_t)uc;
                    } break;
                }
            }
        } else if (ev.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            return 0;
        }
    }
#else
    int32_t uc = fgetc(stdin);
    if (uc < 0)
        goto eof;

    if (uc >= 128) {
        Size bytes = CountUtf8Bytes((char)uc);

        LocalArray<char, 4> buf;
        buf.Append((char)uc);
        buf.len += fread(buf.end(), 1, bytes - 1, stdin);
        if (buf.len < 1)
            goto eof;

        if (buf.len != bytes)
            return 0;
        if (DecodeUtf8(buf, 0, &uc) != bytes)
            return 0;
    }

    return uc;

eof:
    if (ferror(stdin)) {
        if (errno == EINTR) {
            // Could be SIGWINCH, give the user a chance to deal with it
            return 0;
        } else {
            LogError("Failed to read from standard input: %1", strerror(errno));
            return -1;
        }
    } else {
        // EOF
        return -1;
    }
#endif
}

int ConsolePrompter::ComputeWidth(Span<const char> str)
{
    int width = 0;

    for (char c: str) {
        // XXX: For now we assume all codepoints take a single column,
        // we need to use something like wcwidth() instead.
        width += ((uint8_t)c >= 32 && !((c & 0xC0) == 0x80));
    }

    return width;
}

void ConsolePrompter::EnsureNulTermination()
{
    str.Grow(1);
    str.ptr[str.len] = 0;
}

const char *Prompt(const char *prompt, const char *default_value, const char *mask, Allocator *alloc)
{
    RG_ASSERT(alloc);

    ConsolePrompter prompter;

    prompter.prompt = prompt;
    prompter.mask = mask;
    prompter.str.allocator = alloc;
    if (default_value) {
        prompter.str.Append(default_value);
    }

    if (!prompter.Read())
        return nullptr;

    const char *str = prompter.str.Leak().ptr;
    return str;
}

bool PromptYN(const char *prompt, bool *out_value)
{
    ConsolePrompter prompter;
    prompter.prompt = prompt;

    return prompter.ReadYN(out_value);
}

}
