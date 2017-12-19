/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <inttypes.h>
#include <string.h>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
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
#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#endif

#include "kutil.hh"

// ------------------------------------------------------------------------
// Memory / Allocator
// ------------------------------------------------------------------------

// This Allocator design should allow efficient and mostly-transparent use of memory
// arenas and simple pointer-bumping allocator. This will be implemented later, for
// now it's just a doubly linked list of malloc() memory blocks.

static Allocator default_allocator;

#define PTR_TO_BUCKET(Ptr) \
    ((AllocatorBucket *)((uint8_t *)(Ptr) - OFFSET_OF(AllocatorBucket, data)))

Allocator::~Allocator()
{
    if (LIKELY(this != &default_allocator)) {
        ReleaseAll();
    }
}

void Allocator::ReleaseAll(Allocator *alloc)
{
    if (!alloc) {
        alloc = &default_allocator;
    }
    alloc->ReleaseAll();
}

void *Allocator::Allocate(Allocator *alloc, Size size, unsigned int flags)
{
    if (!alloc) {
        alloc = &default_allocator;
    }
    return alloc->Allocate(size, flags);
}

void Allocator::Resize(Allocator *alloc, void **ptr, Size old_size, Size new_size,
                       unsigned int flags)
{
    if (!alloc) {
        alloc = &default_allocator;
    }
    alloc->Resize(ptr, old_size, new_size, flags);
}

void Allocator::Release(Allocator *alloc, void *ptr, Size size)
{
    if (!alloc) {
        alloc = &default_allocator;
    }
    alloc->Release(ptr, size);
}

void *Allocator::Allocate(Size size, unsigned int flags)
{
    DebugAssert(size >= 0);

    if (!size)
        return nullptr;

    AllocatorBucket *bucket = (AllocatorBucket *)malloc((size_t)(SIZE(*bucket) + size));
    if (!bucket) {
        LogError("Failed to allocate %1 of memory", FmtMemSize(size));
        abort();
    }
    list.prev->next = &bucket->head;
    bucket->head.prev = list.prev;
    list.prev = &bucket->head;
    bucket->head.next = &list;

    if (flags & (int)Flag::Zero) {
        memset(bucket->data, 0, (size_t)size);
    }

    return bucket->data;
}

void Allocator::Resize(void **ptr, Size old_size, Size new_size, unsigned int flags)
{
    DebugAssert(old_size >= 0);
    DebugAssert(new_size >= 0);

    if (!*ptr) {
        *ptr = Allocate(new_size, flags | (int)Flag::Resizable);
        return;
    }
    if (!new_size) {
        Release(*ptr, old_size);
        *ptr = nullptr;
        return;
    }

    AllocatorBucket *bucket = PTR_TO_BUCKET(*ptr);
    AllocatorBucket *new_bucket =
        (AllocatorBucket *)realloc(bucket, (size_t)(SIZE(*new_bucket) + new_size));
    if (!new_bucket) {
        LogError("Failed to resize %1 memory block to %2",
                 FmtMemSize(old_size), FmtMemSize(new_size));
        abort();
    }
    new_bucket->head.prev->next = &new_bucket->head;
    new_bucket->head.next->prev = &new_bucket->head;
    *ptr = new_bucket->data;

    if (flags & (int)Flag::Zero && new_size > old_size) {
        memset(new_bucket->data + old_size, 0, (size_t)(new_size - old_size));
    }
}

void Allocator::Release(void *ptr, Size size)
{
    DebugAssert(size >= 0);

    if (!ptr)
        return;

    AllocatorBucket *bucket = PTR_TO_BUCKET(ptr);
    bucket->head.next->prev = bucket->head.prev;
    bucket->head.prev->next = bucket->head.next;
    free(bucket);
}

void Allocator::ReleaseAll()
{
    AllocatorList *head = list.next;
    while (head != &list) {
        AllocatorList *next = head->next;
        free(head);
        head = next;
    }
    list = { &list, &list };
}

// ------------------------------------------------------------------------
// Date
// ------------------------------------------------------------------------

Date Date::FromString(const char *date_str, bool strict)
{
    Date date = {};

GCC_PUSH_IGNORE(-Wformat-nonliteral)
    int parts[3];
    const auto TryFormat = [&](const char *format) {
        int end_offset;
        int parts_count = sscanf(date_str, format, &parts[0], &parts[1], &parts[2], &end_offset);
        return parts_count == 3 && !date_str[end_offset];
    };
    if (!TryFormat("%6d-%2u-%2u%n")) {
        if (!TryFormat("%6d/%2u/%6d%n")) {
            LogError("Malformed date string '%1'", date_str);
            return date;
        }
    }
GCC_POP_IGNORE()

    if (parts[2] >= 100 || parts[2] <= -100) {
        std::swap(parts[0], parts[2]);
    } else if (parts[0] < 100 && parts[0] > -100) {
        LogError("Ambiguous date string '%1'", date_str);
        return date;
    }
    if (parts[0] > UINT16_MAX || parts[1] > UINT8_MAX || parts[2] > UINT8_MAX) {
        LogError("Invalid date string '%1'", date_str);
        return date;
    }

    date.st.year = (int16_t)parts[0];
    date.st.month = (int8_t)parts[1];
    date.st.day = (int8_t)parts[2];
    if (strict && !date.IsValid()) {
        LogError("Invalid date string '%1'", date_str);
        date.value = 0;
    }

    return date;
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

uint64_t start_time = GetMonotonicTime();

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

    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 10000000;
#endif
}

// ------------------------------------------------------------------------
// Strings
// ------------------------------------------------------------------------

Span<const char> MakeString(Allocator *alloc, Span<const char> bytes)
{
    char *new_str = (char *)Allocator::Allocate(alloc, bytes.len + 1);
    memcpy(new_str, bytes.ptr, (size_t)bytes.len);
    new_str[bytes.len] = 0;
    return MakeSpan(new_str, bytes.len);
}

Span<const char> DuplicateString(Allocator *alloc, const char *str, Size max_len)
{
    Size str_len = (Size)strlen(str);
    if (max_len >= 0 && str_len > max_len) {
        str_len = max_len;
    }
    char *new_str = (char *)Allocator::Allocate(alloc, str_len + 1);
    memcpy(new_str, str, (size_t)str_len);
    new_str[str_len] = 0;
    return MakeSpan(new_str, str_len);
}

// ------------------------------------------------------------------------
// Format
// ------------------------------------------------------------------------

template <typename AppendFunc>
static inline void WriteUnsignedAsDecimal(uint64_t value, AppendFunc append)
{
    static const char literals[] = "0123456789";

    char buf[32];
    Size len = SIZE(buf);
    do {
        uint64_t digit = value % 10;
        value /= 10;
        buf[--len] = literals[digit];
    } while (value);

    append(MakeSpan(buf + len, SIZE(buf) - len));
}

template <typename AppendFunc>
static inline void WriteUnsignedAsHex(uint64_t value, AppendFunc append)
{
    static const char literals[] = "0123456789ABCDEF";

    char buf[32];
    Size len = SIZE(buf);
    do {
        uint64_t digit = value & 0xF;
        value >>= 4;
        buf[--len] = literals[digit];
    } while (value);

    append(MakeSpan(buf + len, SIZE(buf) - len));
}

template <typename AppendFunc>
static inline void WriteUnsignedAsBinary(uint64_t value, AppendFunc append)
{
    char buf[64];
    Size msb = 64 - (Size)CountLeadingZeros(value);
    for (Size i = 0; i < msb; i++) {
        bool bit = (value >> (msb - i - 1)) & 0x1;
        buf[i] = bit ? '1' : '0';
    }

    append(MakeSpan(buf, msb));
}

template <typename AppendFunc>
static inline void WriteDouble(double value, int precision, AppendFunc append)
{
    char buf[256];
    // That's the lazy way to do it, it'll do for now
    int buf_len;
    if (precision >= 0) {
        buf_len = snprintf(buf, SIZE(buf), "%.*f", precision, value);
    } else {
        buf_len = snprintf(buf, SIZE(buf), "%g", value);
    }
    Assert(buf_len >= 0 && buf_len <= SIZE(buf));

    append(MakeSpan(buf, (Size)buf_len));
}

template <typename AppendFunc>
static inline void ProcessArg(const FmtArg &arg, AppendFunc append)
{
    for (int i = 0; i < arg.repeat; i++) {
        switch (arg.type) {
            case FmtArg::Type::StrRef: {
                append(arg.value.str_ref);
            } break;

            case FmtArg::Type::StrBuf: {
                append(arg.value.str_buf);
            } break;

            case FmtArg::Type::Char: {
                append(Span<const char>(&arg.value.ch, 1));
            } break;

            case FmtArg::Type::Bool: {
                if (arg.value.b) {
                    append("true");
                } else {
                    append("false");
                }
            } break;

            case FmtArg::Type::Integer: {
                if (arg.value.i < 0) {
                    append("-");
                    WriteUnsignedAsDecimal((uint64_t)-arg.value.i, append);
                } else {
                    WriteUnsignedAsDecimal((uint64_t)arg.value.i, append);
                }
            } break;

            case FmtArg::Type::Unsigned: {
                WriteUnsignedAsDecimal(arg.value.u, append);
            } break;

            case FmtArg::Type::Double: {
                WriteDouble(arg.value.d.value, arg.value.d.precision, append);
            } break;

            case FmtArg::Type::Binary: {
                if (arg.value.u) {
                    append("0b");
                    WriteUnsignedAsBinary(arg.value.u, append);
                } else {
                    append('0');
                }
            } break;

            case FmtArg::Type::Hexadecimal: {
                if (arg.value.u) {
                    append("0x");
                    WriteUnsignedAsHex(arg.value.u, append);
                } else {
                    append('0');
                }
            } break;

            case FmtArg::Type::MemorySize: {
                size_t size_unsigned;
                if (arg.value.size >= 0) {
                    size_unsigned = (size_t)arg.value.size;
                    append("-");
                } else {
                    size_unsigned = (size_t)-arg.value.size;
                }
                if (size_unsigned > 1024 * 1024) {
                    double size_mib = (double)size_unsigned / (1024.0 * 1024.0);
                    WriteDouble(size_mib, 2, append);
                    append(" MiB");
                } else if (size_unsigned > 1024) {
                    double size_kib = (double)size_unsigned / 1024.0;
                    WriteDouble(size_kib, 2, append);
                    append(" kiB");
                } else {
                    WriteUnsignedAsDecimal(size_unsigned, append);
                    append(" B");
                }
            } break;

            case FmtArg::Type::DiskSize: {
                size_t size_unsigned;
                if (arg.value.size >= 0) {
                    size_unsigned = (size_t)arg.value.size;
                    append("-");
                } else {
                    size_unsigned = (size_t)-arg.value.size;
                }
                if (size_unsigned > 1000 * 1000) {
                    double size_mib = (double)size_unsigned / (1000.0 * 1000.0);
                    WriteDouble(size_mib, 2, append);
                    append(" MB");
                } else if (size_unsigned > 1024) {
                    double size_kib = (double)size_unsigned / 1000.0;
                    WriteDouble(size_kib, 2, append);
                    append(" kB");
                } else {
                    WriteUnsignedAsDecimal(size_unsigned, append);
                    append(" B");
                }
            } break;

            case FmtArg::Type::Date: {
                DebugAssert(arg.value.date.IsValid());
                int year = arg.value.date.st.year;
                if (year < 0) {
                    append("-");
                    year = -year;
                }
                if (year < 10) {
                    append("000");
                } else if (year < 100) {
                    append("00");
                } else if (year < 1000) {
                    append("0");
                }
                WriteUnsignedAsDecimal((uint64_t)year, append);
                append("-");
                if (arg.value.date.st.month < 10) {
                    append("0");
                }
                WriteUnsignedAsDecimal((uint64_t)arg.value.date.st.month, append);
                append("-");
                if (arg.value.date.st.day < 10) {
                    append("0");
                }
                WriteUnsignedAsDecimal((uint64_t)arg.value.date.st.day, append);
            } break;

            case FmtArg::Type::List: {
                if (arg.value.list.args.len) {
                    ProcessArg(arg.value.list.args[0], append);

                    Span<const char> separator = arg.value.list.separator;
                    for (Size j = 1; j < arg.value.list.args.len; j++) {
                        append(separator);
                        ProcessArg(arg.value.list.args[j], append);
                    }
                }
            } break;
        }
    }
}

template <typename AppendFunc>
static inline void DoFormat(const char *fmt, Span<const FmtArg> args,
                            AppendFunc append)
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

Span<char> FmtFmt(Span<char> buf, const char *fmt, Span<const FmtArg> args)
{
    DebugAssert(buf.len >= 0);

    if (!buf.len)
        return {};
    buf.len--;

    Size real_len = 0;

    DoFormat(fmt, args, [&](Span<const char> fragment) {
        if (LIKELY(real_len < buf.len)) {
            Size copy_len = fragment.len;
            if (copy_len > buf.len - real_len) {
                copy_len = buf.len - real_len;
            }
            memcpy(buf.ptr + real_len, fragment.ptr, (size_t)copy_len);
        }
        real_len += fragment.len;
    });
    if (real_len < buf.len) {
        buf.len = real_len;
    }
    buf.ptr[buf.len] = 0;

    return buf;
}

Span<char> FmtFmt(Allocator *alloc, const char *fmt, Span<const FmtArg> args)
{
    char *buf = (char *)Allocator::Allocate(alloc, FMT_STRING_BASE_CAPACITY,
                                            (int)Allocator::Flag::Resizable);
    Size buf_len = 0;
    // Cheat a little bit to make room for the NUL byte
    Size buf_capacity = FMT_STRING_BASE_CAPACITY - 1;

    DoFormat(fmt, args, [&](Span<const char> fragment) {
        // Same thing, use >= and <= to make sure we have enough place for the NUL byte
        if (fragment.len >= buf_capacity - buf_len) {
            Size new_capacity = buf_capacity;
            do {
                new_capacity = (Size)((float)new_capacity * FMT_STRING_GROWTH_FACTOR);
            } while (fragment.len >= new_capacity - buf_len);
            Allocator::Resize(alloc, (void **)&buf, buf_capacity, new_capacity);
            buf_capacity = new_capacity;
        }
        memcpy(buf + buf_len, fragment.ptr, (size_t)fragment.len);
        buf_len += fragment.len;
    });
    buf[buf_len] = 0;

    return MakeSpan(buf, buf_len);
}

void PrintFmt(FILE *fp, const char *fmt, Span<const FmtArg> args)
{
    LocalArray<char, FMT_STRING_PRINT_BUFFER_SIZE> buf;
    DoFormat(fmt, args, [&](Span<const char> fragment) {
        if (fragment.len > ARRAY_SIZE(buf.data) - buf.len) {
            fwrite(buf.data, 1, (size_t)buf.len, fp);
            buf.len = 0;
        }
        if (fragment.len >= ARRAY_SIZE(buf.data)) {
            fwrite(fragment.ptr, 1, (size_t)fragment.len, fp);
        } else {
            memcpy(buf.data + buf.len, fragment.ptr, (size_t)fragment.len);
            buf.len += fragment.len;
        }
    });
    fwrite(buf.data, 1, (size_t)buf.len, fp);
}

// ------------------------------------------------------------------------
// Debug and errors
// ------------------------------------------------------------------------

static LocalArray<std::function<LogHandlerFunc>, 16> log_handlers = {
    DefaultLogHandler
};

bool enable_debug = []() {
#ifdef __EMSCRIPTEN__
    return EM_ASM_INT({
        try {
            var debug_env_name = UTF8ToString($0);
            return (process.env[debug_env_name] !== undefined && process.env[debug_env_name] != 0);
        } catch (error) {
            return 0;
        }
    }, DEBUG_ENV_NAME);
#else
    const char *debug = getenv(DEBUG_ENV_NAME);
    if (!debug || TestStr(debug, "0")) {
        return false;
    } else if (TestStr(debug, "1")) {
        return true;
    } else {
        LogError("%1 should contain value '0' or '1'", DEBUG_ENV_NAME);
        return true;
    }
#endif
}();

static bool ConfigLogTerminalOutput()
{
    static bool init, output_is_terminal;

    if (!init) {
#ifdef _WIN32
        static HANDLE stderr_handle;
        static DWORD orig_console_mode;

        stderr_handle = (HANDLE)_get_osfhandle(_fileno(stderr));
        output_is_terminal = GetConsoleMode(stderr_handle, &orig_console_mode);
        if (output_is_terminal && !(orig_console_mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            // Enable VT100 escape sequences, introduced in Windows 10
            DWORD new_mode = orig_console_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            output_is_terminal = SetConsoleMode(stderr_handle, new_mode);
            atexit([]() {
                SetConsoleMode(stderr_handle, orig_console_mode);
            });
        }
#else
        output_is_terminal = isatty(fileno(stderr));
#endif

        init = true;
    }

    return output_is_terminal;
}

void LogFmt(LogLevel level, const char *ctx, const char *fmt, Span<const FmtArg> args)
{
    if (!log_handlers.len)
        return;
    if (level == LogLevel::Debug && !enable_debug)
        return;

    char ctx_buf[128];
    {
        double time = (double)(GetMonotonicTime() - start_time) / 1000;
        Size ctx_len = (Size)strlen(ctx);
        if (ctx_len > 20) {
            snprintf(ctx_buf, SIZE(ctx_buf), " ...%s [%8.3f]  ", ctx + ctx_len - 17, time);
        } else {
            snprintf(ctx_buf, SIZE(ctx_buf), "%21s [%8.3f]  ", ctx, time);
        }
    }

    log_handlers[log_handlers.len - 1](level, ctx_buf, fmt, args);
}

void DefaultLogHandler(LogLevel level, const char *ctx,
                       const char *fmt, Span<const FmtArg> args)
{
    StartConsoleLog(level);
    Print(stderr, ctx);
    PrintFmt(stderr, fmt, args);
    PrintLn(stderr);
    EndConsoleLog();
}

void StartConsoleLog(LogLevel level)
{
    if (!ConfigLogTerminalOutput())
        return;

    switch (level)  {
        case LogLevel::Error: { fputs("\x1B[31m", stderr); } break;
        case LogLevel::Info: break;
        case LogLevel::Debug: { fputs("\x1B[36m", stderr); } break;
    }
}

void EndConsoleLog()
{
    if (!ConfigLogTerminalOutput())
        return;

    fputs("\x1B[0m", stderr);
}

void PushLogHandler(std::function<LogHandlerFunc> handler)
{
    log_handlers.Append(handler);
}

void PopLogHandler()
{
    log_handlers.RemoveLast();
}

// ------------------------------------------------------------------------
// System
// ------------------------------------------------------------------------

bool ReadFile(const char *filename, Size max_size,
              Allocator *alloc, Span<uint8_t> *out_data)
{
    FILE *fp = fopen(filename, "rb" FOPEN_COMMON_FLAGS);
    if (!fp) {
        LogError("Cannot open '%1': %2", filename, strerror(errno));
        return false;
    }
    DEFER { fclose(fp); };

    Span<uint8_t> data;
    fseek(fp, 0, SEEK_END);
    data.len = ftell(fp);
    if (data.len > max_size) {
        LogError("File '%1' is too large (limit = %2)", filename, FmtDiskSize(max_size));
        return false;
    }
    fseek(fp, 0, SEEK_SET);

    data.ptr = (uint8_t *)Allocator::Allocate(alloc, data.len);
    DEFER_N(data_guard) { Allocator::Release(alloc, data.ptr, data.len); };
    if (fread(data.ptr, 1, (size_t)data.len, fp) != (size_t)data.len || ferror(fp)) {
        LogError("Error while reading file '%1'", filename);
        return false;
    }

    data_guard.disable();
    *out_data = data;
    return true;
}

#ifdef _WIN32

static char *Win32ErrorString(DWORD error_code = UINT32_MAX)
{
    if (error_code == UINT32_MAX) {
        error_code = GetLastError();
    }

    static thread_local char str_buf[256];
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

EnumStatus EnumerateDirectory(const char *dirname, const char *filter,
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
        LogError("Cannot enumerate directory '%1': %2", dirname,
                 Win32ErrorString());
        return EnumStatus::Error;
    }
    DEFER { FindClose(handle); };

    do {
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

EnumStatus EnumerateDirectory(const char *dirname, const char *filter,
                              std::function<bool(const char *, const FileInfo &)> func)
{
    DIR *dirp = opendir(dirname);
    if (!dirp) {
        LogError("Cannot enumerate directory '%1': %2", dirname, strerror(errno));
        return EnumStatus::Error;
    }
    DEFER { closedir(dirp); };

    dirent *dent;
    while ((dent = readdir(dirp))) {
        if ((dent->d_name[0] == '.' && !dent->d_name[1]) ||
                (dent->d_name[0] == '.' && dent->d_name[1] == '.' && !dent->d_name[2]))
            continue;

        if (!fnmatch(filter, dent->d_name, FNM_PERIOD)) {
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

bool EnumerateDirectoryFiles(const char *dirname, const char *filter, Allocator *str_alloc,
                             HeapArray<const char *> *out_files, Size max_files)
{
    Assert(max_files > 0);

    DEFER_NC(out_guard, len = out_files->len) { out_files->RemoveFrom(len); };

    EnumStatus status = EnumerateDirectory(dirname, filter,
                                           [&](const char *filename, const FileInfo &info) {
        if (info.type == FileType::File) {
            out_files->Append(Fmt(str_alloc, "%1%/%2", dirname, filename).ptr);
        }
        return true;
    });
    if (status == EnumStatus::Error)
        return false;

    if (status == EnumStatus::Partial) {
        LogError("Partial enumeration of directory '%1'", dirname);
    }

    out_guard.disable();
    return true;
}

#ifdef __EMSCRIPTEN__
static bool running_in_node;

INIT(MountHostFilesystem)
{
    running_in_node = EM_ASM_INT({
        try {
            if (process.platform == 'win32') {
                var path = require('path');

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
        Size path_len = GetModuleFileName(nullptr, executable_path, SIZE(executable_path));
        Assert(path_len);
        Assert(path_len < SIZE(executable_path));
    }

    return executable_path;
#elif defined(__linux__)
    static char executable_path[4096];

    if (!executable_path[0]) {
        char *path_buf = realpath("/proc/self/exe", nullptr);
        Assert(path_buf);
        Size path_len = (Size)strlen(path_buf);
        Assert(path_len < SIZE(executable_path));
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
#if defined(_WIN32) || defined(__linux__)
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

Size GetPathExtension(const char *filename, Span<char> out_buf,
                      CompressionType *out_compression_type)
{
    Size len = (Size)strlen(filename);
    DebugAssert(len >= 0);

    Size ext_offset = len;
    const auto SkipOneExt = [&]() {
        len = ext_offset;
        while (ext_offset && filename[--ext_offset] != '.');
    };

    SkipOneExt();
    if (out_compression_type) {
        if (TestStr(filename + ext_offset, ".gz")) {
            *out_compression_type = CompressionType::Gzip;
            SkipOneExt();
        } else {
            *out_compression_type = CompressionType::None;
        }
    }

    Size copy_len = len - ext_offset;
    if (copy_len > out_buf.len) {
        copy_len = out_buf.len;
    }
    memcpy(out_buf.ptr, filename + ext_offset, (size_t)copy_len);

    return copy_len;
}

// ------------------------------------------------------------------------
// Streams
// ------------------------------------------------------------------------

#if __has_include("../../lib/miniz/miniz.h") && !defined(KUTIL_NO_MINIZ)
    #define MINIZ_NO_STDIO
    #define MINIZ_NO_TIME
    #define MINIZ_NO_ARCHIVE_APIS
    #define MINIZ_NO_ARCHIVE_WRITING_APIS
    #define MINIZ_NO_ZLIB_APIS
    #define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
    #define MINIZ_NO_MALLOC
    #include "../../lib/miniz/miniz.h"
#endif

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

    if (filename) {
        this->filename = DuplicateString(&str_alloc, filename).ptr;
    }

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

    DEFER_N(error_guard) {
        ReleaseResources();
        error = true;
    };

    if (filename) {
        this->filename = DuplicateString(&str_alloc, filename).ptr;
    }

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

    this->filename = DuplicateString(&str_alloc, filename).ptr;

    if (!InitDecompressor(compression_type))
        return false;
    source.type = SourceType::File;
    source.u.fp = fopen(filename, "rb" FOPEN_COMMON_FLAGS);
    if (!source.u.fp) {
        LogError("Cannot open file '%1'", filename);
        source.error = true;
        return false;
    }
    source.owned = true;

    error_guard.disable();
    return true;
}

void StreamReader::Close()
{
    ReleaseResources();

    filename = "?";
    source.error = false;
    source.eof = false;
    error = false;
    eof = false;
    Allocator::ReleaseAll(&str_alloc);
}

Size StreamReader::Read(Size max_len, void *out_buf)
{
    if (UNLIKELY(error)) {
        LogError("Cannot read from '%1' after error", filename);
        return -1;
    }

    switch (compression.type) {
        case CompressionType::None: {
            Size read_len = ReadRaw(max_len, out_buf);
            error |= source.error;
            return read_len;
        } break;

        case CompressionType::Gzip:
        case CompressionType::Zlib: {
            return Deflate(max_len, out_buf);
        } break;
    }
    Assert(false);
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
            error = source.error;
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
                    if (ctx->in_len < 0) {
                        if (read_len) {
                            return read_len;
                        } else {
                            error |= source.error;
                            return ctx->in_len;
                        }
                    }
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
                                if (source.error) {
                                    error = true;
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
    Assert(false);
#endif
}

Size StreamReader::ReadRaw(Size max_len, void *out_buf)
{
    if (UNLIKELY(source.error))
        return -1;

    switch (source.type) {
        case SourceType::File: {
            size_t read_len = fread(out_buf, 1, (size_t)max_len, source.u.fp);
            if (ferror(source.u.fp)) {
                LogError("Error while reading file '%1'", filename);
                source.error = true;
                return -1;
            }
            if (feof(source.u.fp)) {
                source.eof = true;
            }
            return (Size)read_len;
        }

        case SourceType::Memory: {
            Size copy_len = source.u.memory.buf.len - source.u.memory.pos;
            if (copy_len > max_len) {
                copy_len = max_len;
            }
            memcpy(out_buf, source.u.memory.buf.ptr + source.u.memory.pos, (size_t)copy_len);
            source.u.memory.pos += copy_len;
            return copy_len;
        } break;
    }
    Assert(false);
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

    if (filename) {
        this->filename = DuplicateString(&str_alloc, filename).ptr;
    }

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

    DEFER_N(error_guard) {
        ReleaseResources();
        error = true;
    };

    if (filename) {
        this->filename = DuplicateString(&str_alloc, filename).ptr;
    }

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

    this->filename = DuplicateString(&str_alloc, filename).ptr;

    if (!InitCompressor(compression_type))
        return false;
    dest.type = DestinationType::File;
    dest.u.fp = fopen(filename, "wb" FOPEN_COMMON_FLAGS);
    if (!dest.u.fp) {
        LogError("Cannot open file '%1'", filename);
        return false;
    }
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
                if (fflush(dest.u.fp) != 0 ||
                        _commit(_fileno(dest.u.fp)) < 0) {
#else
                if (fflush(dest.u.fp) != 0 ||
                        fsync(fileno(dest.u.fp)) < 0) {
#endif
                    LogError("Failed to finalize writing to '%1'", filename);
                    success = false;
                }
            } break;

            case DestinationType::Memory: {} break;
        }
    }

    ReleaseResources();

    filename = "?";
    open = false;
    error = false;
    Allocator::ReleaseAll(&str_alloc);

    return success;
}

bool StreamWriter::Write(Span<const uint8_t> buf)
{
    if (UNLIKELY(error)) {
        LogError("Cannot write to '%1' after error", filename);
        return false;
    }

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
    Assert(false);
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
                LogError("Failed to write to '%1'", filename);
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
    Assert(false);
}

// ------------------------------------------------------------------------
// Option Parser
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

const char *OptionParser::ConsumeOption()
{
    Size next_index;
    const char *opt;

    current_option = nullptr;
    current_value = nullptr;

    // Support aggregate short options, such as '-fbar'. Note that this can also be
    // parsed as the short option '-f' with value 'bar', if the user calls
    // ConsumeOptionValue() after getting '-f'.
    if (smallopt_offset) {
        opt = args[pos];
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
    next_index = pos;
    while (next_index < limit && !IsOption(args[next_index])) {
        next_index++;
    }
    RotateArgs(args.ptr, pos, next_index, args.len);
    limit -= (next_index - pos);
    if (pos >= limit)
        return nullptr;
    opt = args[pos];

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

const char *OptionParser::ConsumeOptionValue()
{
    if (current_value)
        return current_value;

    const char *arg = args[pos];

    // Support '-fbar' where bar is the value, but only for the first short option
    // if it's an aggregate.
    if (smallopt_offset == 1 && arg[2]) {
        smallopt_offset = 0;
        current_value = arg + 2;
        pos++;
    // Support '-f bar' and '--foo bar', see ConsumeOption() for '--foo=bar'
    } else if (!smallopt_offset && pos < args.len && !IsOption(arg)) {
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

const char *OptionParser::RequireOptionValue(void (*usage_func)(FILE *fp))
{
    if (!ConsumeOptionValue()) {
        PrintLn(stderr, "Option '%1' needs an argument", current_option);
        if (usage_func) {
            usage_func(stderr);
        }
    }

    return current_value;
}
