#include <inttypes.h>
#include <string.h>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <io.h>
    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
        #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
    #endif
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
    ((AllocatorBucket *)((uint8_t *)(Ptr) - OffsetOf(AllocatorBucket, data)))

void Allocator::ReleaseAll(Allocator *alloc)
{
    if (!alloc) {
        alloc = &default_allocator;
    }
    alloc->ReleaseAll();
}

void *Allocator::Allocate(Allocator *alloc, size_t size, unsigned int flags)
{
    if (!alloc) {
        alloc = &default_allocator;
    }
    return alloc->Allocate(size, flags);
}

void Allocator::Resize(Allocator *alloc, void **ptr, size_t old_size, size_t new_size,
                       unsigned int flags)
{
    if (!alloc) {
        alloc = &default_allocator;
    }
    alloc->Resize(ptr, old_size, new_size, flags);
}

void Allocator::Release(Allocator *alloc, void *ptr, size_t size)
{
    if (!alloc) {
        alloc = &default_allocator;
    }
    alloc->Release(ptr, size);
}

char *Allocator::MakeString(Allocator *alloc, const ArrayRef<const char> &bytes)
{
    if (!alloc) {
        alloc = &default_allocator;
    }
    return alloc->MakeString(bytes);
}

char *Allocator::DuplicateString(Allocator *alloc, const char *str, size_t max_len)
{
    if (!alloc) {
        alloc = &default_allocator;
    }
    return alloc->DuplicateString(str, max_len);
}

void *Allocator::Allocate(size_t size, unsigned int flags)
{
    AllocatorBucket *bucket = (AllocatorBucket *)malloc(sizeof(*bucket) + size);
    if (!bucket) {
        Abort("Failed to allocate %1 of memory", FmtMemSize(size));
    }
    list.prev->next = &bucket->head;
    bucket->head.prev = list.prev;
    list.prev = &bucket->head;
    bucket->head.next = &list;

    if (flags & ZeroMemory) {
        memset(bucket->data, 0, size);
    }

    return bucket->data;
}

char *Allocator::MakeString(const ArrayRef<const char> &bytes)
{
    char *str = (char *)Allocate(bytes.len + 1);
    memcpy(str, bytes.ptr, bytes.len);
    str[bytes.len] = 0;
    return str;
}

char *Allocator::DuplicateString(const char *str, size_t max_len)
{
    size_t str_len = strlen(str);
    if (str_len > max_len) {
        str_len = max_len;
    }
    char *new_str = (char *)Allocate(str_len + 1);
    memcpy(new_str, str, str_len);
    new_str[str_len] = 0;
    return new_str;
}

void Allocator::Resize(void **ptr, size_t old_size, size_t new_size, unsigned int flags)
{
    if (!*ptr) {
        *ptr = Allocate(new_size, flags | Resizable);
        return;
    }

    AllocatorBucket *bucket = PTR_TO_BUCKET(*ptr);
    AllocatorBucket *new_bucket = (AllocatorBucket *)realloc(bucket, sizeof(*new_bucket) + new_size);
    if (!new_bucket) {
        Abort("Failed to resize %1 memory block to %2", FmtMemSize(old_size), FmtMemSize(new_size));
    }
    new_bucket->head.prev->next = &new_bucket->head;
    new_bucket->head.next->prev = &new_bucket->head;
    *ptr = new_bucket->data;

    if (flags & ZeroMemory && new_size > old_size) {
        memset(new_bucket->data + old_size, 0, new_size - old_size);
    }
}

void Allocator::Release(void *ptr, size_t)
{
    if (!ptr) {
        return ;
    }

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
// String Format
// ------------------------------------------------------------------------

template <typename AppendFunc>
static inline void WriteUnsignedAsDecimal(uint64_t value, AppendFunc append)
{
    static const char literals[] = "0123456789";

    char buf[32];
    size_t len = sizeof(buf);
    do {
        uint64_t digit = value % 10;
        value /= 10;
        buf[--len] = literals[digit];
    } while (value);

    append(ArrayRef<const char>(buf + len, sizeof(buf) - len));
}

template <typename AppendFunc>
static inline void WriteUnsignedAsHex(uint64_t value, AppendFunc append)
{
    static const char literals[] = "0123456789ABCDEF";

    char buf[32];
    size_t len = sizeof(buf);
    do {
        uint64_t digit = value & 0xF;
        value >>= 4;
        buf[--len] = literals[digit];
    } while (value);

    append(ArrayRef<const char>(buf + len, sizeof(buf) - len));
}

template <typename AppendFunc>
static inline void WriteUnsignedAsBinary(uint64_t value, AppendFunc append)
{
    char buf[64];
    size_t msb = 64 - (size_t)CountLeadingZeros(value);
    for (size_t i = 0; i < msb; i++) {
        bool bit = (value >> (msb - i - 1)) & 0x1;
        buf[i] = bit ? '1' : '0';
    }

    append(ArrayRef<const char>(buf, msb));
}

template <typename AppendFunc>
static inline void WriteDouble(double value, int precision, AppendFunc append)
{
    char buf[256];
    // That's the lazy way to do it, it'll do for now
    int buf_len;
    if (precision >= 0) {
        buf_len = snprintf(buf, sizeof(buf), "%.*f", precision, value);
    } else {
        buf_len = snprintf(buf, sizeof(buf), "%g", value);
    }
    Assert(buf_len >= 0 && (size_t)buf_len <= sizeof(buf));

    append(MakeArrayRef(buf, (size_t)buf_len));
}

template <typename AppendFunc>
static inline void ProcessArg(const FmtArg &arg, AppendFunc append)
{
    for (unsigned int i = 0; i < arg.repeat; i++) {
        switch (arg.type) {
            case FmtArg::Type::String: {
                append(arg.value.str);
            } break;
            case FmtArg::Type::Char: {
                append(ArrayRef<const char>(&arg.value.ch, 1));
            } break;
            case FmtArg::Type::Bool: {
                if (arg.value.b) {
                    append(MakeStrRef("true"));
                } else {
                    append(MakeStrRef("false"));
                }
            } break;
            case FmtArg::Type::Integer: {
                if (arg.value.i < 0) {
                    append(MakeStrRef("-"));
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
                    append(MakeStrRef("0b"));
                    WriteUnsignedAsBinary(arg.value.u, append);
                } else {
                    append('0');
                }
            } break;
            case FmtArg::Type::Hexadecimal: {
                if (arg.value.u) {
                    append(MakeStrRef("0x"));
                    WriteUnsignedAsHex(arg.value.u, append);
                } else {
                    append('0');
                }
            } break;
            case FmtArg::Type::MemorySize: {
                if (arg.value.size > 1024 * 1024) {
                    double len_mib = (double)arg.value.size / (1024.0 * 1024.0);
                    WriteDouble(len_mib, 2, append);
                    append(MakeStrRef(" MiB"));
                } else if (arg.value.size > 1024) {
                    double len_kib = (double)arg.value.size / 1024.0;
                    WriteDouble(len_kib, 2, append);
                    append(MakeStrRef(" kiB"));
                } else {
                    WriteUnsignedAsDecimal(arg.value.size, append);
                    append(MakeStrRef(" B"));
                }
            } break;
            case FmtArg::Type::DiskSize: {
                if (arg.value.size > 1000 * 1000) {
                    double len_mb = (double)arg.value.size / (1000.0 * 1000.0);
                    WriteDouble(len_mb, 2, append);
                    append(MakeStrRef(" MB"));
                } else if (arg.value.size > 1000) {
                    double len_kb = (double)arg.value.size / 1000.0;
                    WriteDouble(len_kb, 2, append);
                    append(MakeStrRef(" kB"));
                } else {
                    WriteUnsignedAsDecimal(arg.value.size, append);
                    append(MakeStrRef(" B"));
                }
            } break;
            case FmtArg::Type::Date: {
                DebugAssert(arg.value.date.IsValid());
                if (arg.value.date.st.year < 10) {
                    append(MakeStrRef("000"));
                } else if (arg.value.date.st.year < 100) {
                    append(MakeStrRef("00"));
                } else if (arg.value.date.st.year < 1000) {
                    append(MakeStrRef("0"));
                }
                WriteUnsignedAsDecimal(arg.value.date.st.year, append);
                append(MakeStrRef("-"));
                if (arg.value.date.st.month < 10) {
                    append(MakeStrRef("0"));
                }
                WriteUnsignedAsDecimal(arg.value.date.st.month, append);
                append(MakeStrRef("-"));
                if (arg.value.date.st.day < 10) {
                    append(MakeStrRef("0"));
                }
                WriteUnsignedAsDecimal(arg.value.date.st.day, append);
            } break;
            case FmtArg::Type::List: {
                if (arg.value.list.args.len) {
                    ProcessArg(arg.value.list.args[0], append);
                    ArrayRef<const char> separator = MakeStrRef(arg.value.list.separator);
                    for (size_t j = 1; j < arg.value.list.args.len; j++) {
                        append(separator);
                        ProcessArg(arg.value.list.args[j], append);
                    }
                }
            } break;
        }
    }
}

template <typename AppendFunc>
static inline void DoFormat(const char *fmt, const ArrayRef<const FmtArg> &args,
                            AppendFunc append)
{
#ifndef NDEBUG
    bool invalid_marker = false;
    uint32_t unused_arguments = (1 << args.len) - 1;
#endif

    const char *fmt_ptr = fmt;
    for (;;) {
        // Find the next marker (or the end of string) and write everything before it
        const char *marker_ptr = fmt_ptr;
        while (marker_ptr[0] && marker_ptr[0] != '%') {
            marker_ptr++;
        }
        append(ArrayRef<const char>(fmt_ptr, (size_t)(marker_ptr - fmt_ptr)));
        if (!marker_ptr[0]) {
            break;
        }

        // Try to interpret this marker as a number
        size_t idx = 0;
        size_t idx_end = 1;
        for (;;) {
            // Unsigned cast makes the test below quicker, don't remove it or it'll break
            unsigned int digit = (unsigned int)marker_ptr[idx_end] - '0';
            if (digit > 9) {
                break;
            }
            idx = (idx * 10) + digit;
            idx_end++;
        }

        // That was indeed a number
        if (idx_end > 1) {
            idx--;
            if (idx < args.len) {
                ProcessArg<AppendFunc>(args[idx], append);
#ifndef NDEBUG
                unused_arguments &= ~(1 << idx);
            } else {
                invalid_marker = true;
#endif
            }
            fmt_ptr = marker_ptr + idx_end;
        } else if (marker_ptr[1] == '%') {
            append('%');
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

size_t FmtString(const ArrayRef<char> &buf, const char *fmt, const ArrayRef<const FmtArg> &args)
{
    size_t real_len = 0;

    DoFormat(fmt, args, [&](const ArrayRef<const char> &fragment) {
        if (real_len < buf.len) {
            size_t copy_len = fragment.len;
            if (copy_len > buf.len - real_len) {
                copy_len = buf.len - real_len;
            }
            memcpy(buf.ptr + real_len, fragment.ptr, copy_len);
        }
        real_len += fragment.len;
    });
    if (real_len < buf.len) {
        buf[real_len] = 0;
    } else if (buf.len) {
        buf[buf.len - 1] = 0;
    }
    real_len++;

    return real_len;
}

char *FmtString(Allocator *alloc, const char *fmt, const ArrayRef<const FmtArg> &args)
{
    char *buf = (char *)Allocator::Allocate(alloc, FMT_STRING_BASE_CAPACITY, Allocator::Resizable);
    size_t buf_len = 0;
    // Cheat a little bit to make room for the NUL byte
    size_t buf_capacity = FMT_STRING_BASE_CAPACITY - 1;

    DoFormat(fmt, args, [&](const ArrayRef<const char> &fragment) {
        // Same thing, use >= and <= to make sure we have enough place for the NUL byte
        if (fragment.len >= buf_capacity - buf_len) {
            size_t new_capacity = buf_capacity;
            do {
                new_capacity = (size_t)(new_capacity * FMT_STRING_GROWTH_FACTOR);
            } while (fragment.len <= new_capacity - buf_len);
            Allocator::Resize(alloc, (void **)&buf, buf_capacity, new_capacity);
            buf_capacity = new_capacity;
        }
        memcpy(buf + buf_len, fragment.ptr, fragment.len);
        buf_len += fragment.len;
    });
    buf[buf_len] = 0;

    return buf;
}

void FmtPrint(FILE *fp, const char *fmt, const ArrayRef<const FmtArg> &args)
{
    LocalArray<char, FMT_STRING_PRINT_BUFFER_SIZE> buf;
    DoFormat(fmt, args, [&](const ArrayRef<const char> &fragment) {
        if (fragment.len > CountOf(buf.data) - buf.len) {
            fwrite(buf.data, 1, buf.len, fp);
            buf.len = 0;
        }
        if (fragment.len >= CountOf(buf.data)) {
            fwrite(fragment.ptr, 1, fragment.len, fp);
        } else {
            memcpy(buf.data + buf.len, fragment.ptr, fragment.len);
            buf.len += fragment.len;
        }
    });
    fwrite(buf.data, 1, buf.len, fp);
}

static bool ConfigTerminalOutput()
{
    static bool init, output_is_terminal;

    if (!init) {
        static HANDLE stdout_handle;
        static DWORD orig_console_mode;

#ifdef _WIN32
        stdout_handle = (HANDLE)_get_osfhandle(_fileno(stdout));
        output_is_terminal = GetConsoleMode(stdout_handle, &orig_console_mode);
        if (output_is_terminal && !(orig_console_mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            // Enable VT100 escape sequences, introduced in Windows 10
            DWORD new_mode = orig_console_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            output_is_terminal = SetConsoleMode(stdout_handle, new_mode);
            atexit([]() {
                SetConsoleMode(stdout_handle, orig_console_mode);
            });
        }
#else
        int stdout_fd = fileno(stdout);
        output_is_terminal = isatty(stdout_fd);
#endif
        init = true;
    }

    return output_is_terminal;
}

void FmtLog(LogLevel level, const char *ctx, const char *fmt, const ArrayRef<const FmtArg> &args)
{
    FILE *fp = stderr;
    const char *end_marker = nullptr;

    if (ConfigTerminalOutput()) {
        switch (level)  {
            case LogLevel::Error: {
                fputs("\x1B[31m", fp);
                end_marker = "\x1B[0m";
            } break;
            case LogLevel::Info: break;
            case LogLevel::Debug: {
                fputs("\x1B[36m", fp);
                end_marker = "\x1B[0m";
            } break;
        }
    }

    {
        size_t ctx_len = strlen(ctx);
        if (ctx_len > 27) {
            fprintf(fp, " ...%s  ", ctx + ctx_len - 24);
        } else {
            fprintf(fp, "%28s  ", ctx);
        }
    }
    FmtPrint(fp, fmt, args);
    if (end_marker) {
        fputs(end_marker, fp);
    }
    fputc('\n', fp);
}

// ------------------------------------------------------------------------
// Files
// ------------------------------------------------------------------------

bool ReadFile(Allocator *alloc, const char *filename, uint64_t max_size,
              uint8_t **rdata, uint64_t *rlen)
{
#ifdef _WIN32
    FILE *fp = fopen(filename, "rb");
#else
    FILE *fp = fopen(filename, "rbe");
#endif
    if (!fp) {
        // TODO: Detailed error messages
        LogError("Failed to open '%1'", filename);
        return false;
    }
    DEFER { fclose(fp); };

    uint64_t data_len;
    fseek(fp, 0, SEEK_END);
    data_len = ftell(fp);
    if (data_len > max_size) {
        LogError("File '%1' is too big", filename);
        return false;
    }
    fseek(fp, 0, SEEK_SET);

    uint8_t *data = (uint8_t *)Allocator::Allocate(alloc, data_len);
    DEFER_N(data_guard) { Allocator::Release(alloc, data, data_len); };

    if (fread(data, 1, data_len, fp) != data_len || ferror(fp)) {
        LogError("Error while reading file '%1'", filename);
        return false;
    }

    data_guard.disable();
    *rdata = data;
    *rlen = data_len;
    return true;
}
