#pragma once

#define __STDC_FORMAT_MACROS
#include <algorithm>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits>
#include <utility>
#ifdef _WIN32
    #include <intrin.h>
#endif
#ifdef _MSC_VER
    #define ENABLE_INTSAFE_SIGNED_FUNCTIONS
    #include <intsafe.h>
    #pragma intrinsic(_BitScanReverse)
    #pragma intrinsic(_BitScanReverse64)
    #pragma intrinsic(__rdtsc)
#endif

// ------------------------------------------------------------------------
// Config
// ------------------------------------------------------------------------

#define DYNAMICARRAY_BASE_CAPACITY 8
#define DYNAMICARRAY_GROWTH_FACTOR 2

#define SPARSETABLE_BASE_CAPACITY 32
#define SPARSETABLE_GROWTH_FACTOR 2
#define SPARSETABLE_MAX_LOAD_FACTOR 0.5f

#define FMT_STRING_BASE_CAPACITY 128
#define FMT_STRING_GROWTH_FACTOR 1.5f
#define FMT_STRING_PRINT_BUFFER_SIZE 1024

// ------------------------------------------------------------------------
// Utilities
// ------------------------------------------------------------------------

#define STRINGIFY_(a) #a
#define STRINGIFY(a) STRINGIFY_(a)
#define CONCAT_(a, b) a ## b
#define CONCAT(a, b) CONCAT_(a, b)
#define UNIQUE_ID(prefix) CONCAT(prefix, __LINE__)
#define FORCE_EXPAND(x) x

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
    #define ARCH_LITTLE_ENDIAN
#else
    #error Machine architecture not supported
#endif

#if defined(__GNUC__)
    #define MAYBE_UNUSED __attribute__((unused))
    #define FORCE_INLINE __attribute__((always_inline)) inline
    #define LIKELY(cond) __builtin_expect(!!(cond), 1)
    #define UNLIKELY(cond) __builtin_expect(!!(cond), 0)
    #define THREAD_LOCAL __thread

    #ifndef SCNd8
        #define SCNd8 "hhd"
    #endif
    #ifndef SCNi8
        #define SCNi8 "hhd"
    #endif
#elif defined(_MSC_VER)
    #define MAYBE_UNUSED
    #define FORCE_INLINE __forceinline
    #define LIKELY(cond) (cond)
    #define UNLIKELY(cond) (cond)
    #define THREAD_LOCAL __declspec(thread)
#else
    #error Compiler not supported
#endif

#define READ_ONLY_GLOBAL(Type, Name) \
    static Type Name##_priv; \
    const Type *const Name = &Name##_priv
#if defined(SEPARATE_RUNNER) && defined(_WIN32)
    #ifdef BUILDING_RUNNER
        #define RUNNER_SYMBOL __declspec(dllexport)
        #define CORE_SYMBOL __declspec(dllimport)
    #else
        #define RUNNER_SYMBOL __declspec(dllimport)
        #define CORE_SYMBOL __declspec(dllexport)
    #endif
#else
    #define RUNNER_SYMBOL
    #define CORE_SYMBOL
#endif

constexpr uint32_t MakeUInt32(uint16_t high, uint16_t low) { return ((uint32_t)high << 16) | low; }
constexpr uint64_t MakeUInt64(uint32_t high, uint32_t low) { return ((uint64_t)high << 32) | low; }

constexpr size_t Mebibytes(size_t len) { return len * 1024 * 1024; }
constexpr size_t Kibibytes(size_t len) { return len * 1024; }
constexpr size_t Megabytes(size_t len) { return len * 1000 * 1000; }
constexpr size_t Kilobytes(size_t len) { return len * 1000; }

#define OffsetOf(Type, Member) ((size_t)&(((Type *)nullptr)->Member))

template <typename T, size_t N>
constexpr size_t CountOf(T const (&)[N])
{
    return N;
}

template <typename T, size_t N, typename Pred>
T *FindLinear(T (&arr)[N], Pred pred)
{
    for (auto &it: arr)
    {
        if (pred(it))
            return &it;
    }
    return nullptr;
}
template <typename T, typename Pred>
T *FindLinear(T *arr, size_t size, Pred pred)
{
    for (size_t i = 0; i < size; i++) {
        if (pred(arr[i]))
            return &arr[i];
    }
    return nullptr;
}
template <typename Coll, typename Pred>
typename Coll::value_type *FindLinear(Coll &coll, Pred pred)
{
    for (auto &it: coll) {
        if (pred(it))
            return &it;
    }
    return nullptr;
}

#if defined(__GNUC__)
    static inline int CountLeadingZeros(uint32_t u)
    {
        if (!u) {
            return 32;
        }

        return __builtin_clz(u);
    }
    static inline int CountLeadingZeros(uint64_t u)
    {
        if (!u) {
            return 64;
        }

    #if UINT64_MAX == ULONG_MAX
        return __builtin_clzl(u);
    #elif UINT64_MAX == ULLONG_MAX
        return __builtin_clzll(u);
    #else
        #error Neither unsigned long nor unsigned long long is a 64-bit unsigned integer
    #endif
    }
#elif defined(_MSC_VER)
    static inline int CountLeadingZeros(uint32_t u)
    {
        unsigned long leading_zero;
        if (_BitScanReverse(&leading_zero, u)) {
            return 31 - leading_zero;
        } else {
            return 32;
        }
    }
    static inline int CountLeadingZeros(uint64_t u)
    {
        unsigned long leading_zero;
    #ifdef _WIN64
        if (_BitScanReverse64(&leading_zero, u)) {
            return 63 - leading_zero;
        } else {
            return 64;
        }
    #else
        unsigned long leading_zero;
        if (_BitScanReverse(&leading_zero, u >> 32)) {
            return 31 - leading_zero;
        } else if (_BitScanReverse(&leading_zero, (uint32_t)u)) {
            return 63 - leading_zero;
        } else {
            return 64;
        }
    #endif
    }
#else
    #error No implementation of CountLeadingZeros() for this compiler / toolchain
#endif

template <typename T, typename = typename std::enable_if<std::is_enum<T>::value, T>>
typename std::underlying_type<T>::type MaskEnum(T value)
{
    return 1 << static_cast<typename std::underlying_type<T>::type>(value);
}

template <typename Fun>
class ScopeGuard {
    Fun f;
    bool enabled = true;

public:
    ScopeGuard() = delete;
    ScopeGuard(Fun f_) : f(std::move(f_)) {}
    ~ScopeGuard()
    {
        if (enabled)
            f();
    }

    // Those two methods allow for lambda assignement, which makes possible the VZ_DEFER
    // syntax possible.
    ScopeGuard(ScopeGuard &&other)
        : f(std::move(other.f)), enabled(other.enabled)
    {
        other.disable();
    }

    void disable() { enabled = false; }

    ScopeGuard(ScopeGuard &) = delete;
    ScopeGuard& operator=(const ScopeGuard &) = delete;
};

// Honestly, I don't understand all the details in there, this comes from Andrei Alexandrescu.
// https://channel9.msdn.com/Shows/Going+Deep/C-and-Beyond-2012-Andrei-Alexandrescu-Systematic-Error-Handling-in-C
enum class ScopeGuardDeferHelper {};
template <typename Fun>
ScopeGuard<Fun> operator+(ScopeGuardDeferHelper, Fun &&f)
{
    return ScopeGuard<Fun>(std::forward<Fun>(f));
}

// Write 'DEFER { code };' to do something at the end of the current scope, you
// can use DEFER_N(Name) if you need to disable the guard for some reason, and
// DEFER_NC(Name, Captures) if you need to capture values.
#define DEFER \
    auto UNIQUE_ID(defer) = ScopeGuardDeferHelper() + [&]()
#define DEFER_N(Name) \
    auto Name = ScopeGuardDeferHelper() + [&]()
#define DEFER_NC(Name, ...) \
    auto Name = ScopeGuardDeferHelper() + [&, __VA_ARGS__]()

// I'd love to make ArrayRef default to { nullptr, 0 } but unfortunately that makes
// it a non-POD and prevents putting it in a union.
template <typename T>
struct ArrayRef {
    T *ptr;
    size_t len;

    ArrayRef() = default;
    constexpr ArrayRef(T &value) : ptr(&value), len(1) {}
    constexpr ArrayRef(T *ptr_, size_t len_) : ptr(ptr_), len(len_) {}
    template <size_t N>
    constexpr ArrayRef(T (&arr)[N]) : ptr(arr), len(N) {}

    T *begin() const { return ptr; }
    T *end() const { return ptr + len; }

    bool IsValid() const { return ptr; }

    T &operator[](size_t idx) const;

    operator ArrayRef<const T>() const { return ArrayRef<const T>(ptr, len); }

    ArrayRef Take(size_t sub_offset, size_t sub_len) const;
    ArrayRef Between(size_t sub_offset, size_t sub_end) const;
};

// Unfortunately C strings ("foobar") are implicity converted to ArrayRef<T> with the
// templated array constructor. But the NUL terminator is obviously counted in. Since I
// don't want to give up implicit conversion for every type, so instead we need to
// specialize ArrayRef<const char> and ArrayRef<char>.
template <>
struct ArrayRef<const char> {
    const char *ptr;
    size_t len;

    ArrayRef() = default;
    constexpr ArrayRef(const char &value) : ptr(&value), len(1) {}
    explicit constexpr ArrayRef(const char *ptr_, size_t len_) : ptr(ptr_), len(len_) {}
    template <size_t N>
    explicit constexpr ArrayRef(const char (&arr)[N]) : ptr(arr), len(N) {}

    const char *begin() const { return ptr; }
    const char *end() const { return ptr + len; }

    bool IsValid() const { return ptr; }

    const char &operator[](size_t idx) const;

    ArrayRef Take(size_t sub_offset, size_t sub_len) const;
    ArrayRef Between(size_t sub_offset, size_t sub_end) const;
};
template <>
struct ArrayRef<char> {
    char *ptr;
    size_t len;

    ArrayRef() = default;
    constexpr ArrayRef(char &value) : ptr(&value), len(1) {}
    explicit constexpr ArrayRef(char *ptr_, size_t len_) : ptr(ptr_), len(len_) {}
    template <size_t N>
    explicit constexpr ArrayRef(char (&arr)[N]) : ptr(arr), len(N) {}

    char *begin() const { return ptr; }
    char *end() const { return ptr + len; }

    bool IsValid() const { return ptr; }

    char &operator[](size_t idx) const;

    operator ArrayRef<const char>() const { return ArrayRef<const char>(ptr, len); }

    ArrayRef Take(size_t sub_offset, size_t sub_len) const;
    ArrayRef Between(size_t sub_offset, size_t sub_end) const;
};

template <typename T>
static inline constexpr ArrayRef<T> MakeArrayRef(T *ptr, size_t len)
{
    return ArrayRef<T>(ptr, len);
}
template <typename T, size_t N>
static inline constexpr ArrayRef<T> MakeArrayRef(T (&arr)[N])
{
    return ArrayRef<T>(arr, N);
}

static inline ArrayRef<char> MakeStrRef(char *str)
{
    return ArrayRef<char>(str, strlen(str));
}
static inline ArrayRef<const char> MakeStrRef(const char *str)
{
    return ArrayRef<const char>(str, strlen(str));
}
static inline ArrayRef<char> MakeStrRef(char *str, size_t max_len)
{
    return ArrayRef<char>(str, strnlen(str, max_len));
}
static inline ArrayRef<const char> MakeStrRef(const char *str, size_t max_len)
{
    return ArrayRef<const char>(str, strnlen(str, max_len));
}

// ------------------------------------------------------------------------
// Overflow Safety
// ------------------------------------------------------------------------

#if defined(__GNUC__)
    template <typename T> bool AddOverflow(T a, T b, T *rret)
        { return __builtin_add_overflow(a, b, rret); }
    template <typename T> bool SubOverflow(T a, T b, T *rret)
        { return __builtin_sub_overflow(a, b, rret); }
    template <typename T> bool MulOverflow(T a, T b, T *rret)
        { return __builtin_mul_overflow(a, b, rret); }
#elif defined(_MSC_VER)
    // Unfortunately intsafe.h functions change rret to an invalid value when overflow would
    // occur. We want to keep the old value in, like GCC instrinsics do. The good solution
    // would be to implement our own functions, but I'm too lazy to do it right now and test
    // it's working. That'll do for now.
    #define DEFINE_OVERFLOW_OPERATION(Type, Operation, Func) \
        static inline bool Operation##Overflow(Type a, Type b, Type *rret) \
        { \
            Type backup = *rret; \
            if (!Func(a, b, rret)) { \
                return false; \
            } else { \
                *rret = backup; \
                return true; \
            } \
        }
    #define OVERFLOW_OPERATIONS(Type, Prefix) \
        DEFINE_OVERFLOW_OPERATION(Type, Add, Prefix##Add) \
        DEFINE_OVERFLOW_OPERATION(Type, Sub, Prefix##Sub) \
        DEFINE_OVERFLOW_OPERATION(Type, Mul, Prefix##Mult)

    OVERFLOW_OPERATIONS(signed char, Int8)
    OVERFLOW_OPERATIONS(unsigned char, UInt8)
    OVERFLOW_OPERATIONS(short, Short)
    OVERFLOW_OPERATIONS(unsigned short, UShort)
    OVERFLOW_OPERATIONS(int, Int)
    OVERFLOW_OPERATIONS(unsigned int, UInt)
    OVERFLOW_OPERATIONS(long, Long)
    OVERFLOW_OPERATIONS(unsigned long, ULong)
    OVERFLOW_OPERATIONS(long long, LongLong)
    OVERFLOW_OPERATIONS(unsigned long long, ULongLong)

    #undef OVERFLOW_OPERATIONS
    #undef DEFINE_OVERFLOW_OPERATION
#else
    #error Overflow operations are not implemented for this compilter
#endif

// ------------------------------------------------------------------------
// Date and Time
// ------------------------------------------------------------------------

union Date {
    struct {
#ifdef ARCH_LITTLE_ENDIAN
        int8_t day;
        int8_t month;
        int16_t year;
#else
        int16_t year;
        int8_t month;
        int8_t day;
#endif
    } st;
    int32_t value;

    bool operator==(const Date &other) const { return value == other.value; }
    bool operator!=(const Date &other) const { return value != other.value; }
    bool operator>(const Date &other) const { return value > other.value; }
    bool operator>=(const Date &other) const { return value >= other.value; }
    bool operator<(const Date &other) const { return value < other.value; }
    bool operator<=(const Date &other) const { return value <= other.value; }

    bool IsValid() const;
};

inline bool Date::IsValid() const
{
    static const int8_t days_per_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    bool leap_month = (st.month == 2 && ((st.year % 4 == 0 && st.year % 100 != 0) || st.year % 400 == 0));
    if (st.month < 1 || st.month > 12)
        return false;
    if (st.day < 1 || st.day > (days_per_month[st.month - 1] + leap_month))
        return false;
    return true;
}

// ------------------------------------------------------------------------
// Memory / Allocator
// ------------------------------------------------------------------------

struct AllocatorList {
    AllocatorList *prev;
    AllocatorList *next;
};

struct AllocatorBucket {
    AllocatorList head;
    alignas(8) uint8_t data[];
};

class Allocator {
    AllocatorList list = { &list, &list };

public:
    enum Flag {
        ZeroMemory = 1,
        Resizable = 2
    };

    Allocator() = default;
    Allocator(Allocator &) = delete;
    Allocator &operator=(const Allocator &) = delete;
    ~Allocator() { ReleaseAll(); }

    static void ReleaseAll(Allocator *alloc);

    static void *Allocate(Allocator *alloc, size_t size, unsigned int flags = 0);
    static void Resize(Allocator *alloc, void **ptr, size_t old_size, size_t new_size,
                       unsigned int flags = 0);
    static void Release(Allocator *alloc, void *ptr, size_t size);

    static char *MakeString(Allocator *alloc, const ArrayRef<const char> &bytes);
    static char *DuplicateString(Allocator *alloc, const char *str, size_t max_len = SIZE_MAX);

private:
    void ReleaseAll();

    void *Allocate(size_t size, unsigned int flags = 0);
    void Resize(void **ptr, size_t old_size, size_t new_size, unsigned int flags = 0);
    void Release(void *ptr, size_t size);

    char *MakeString(const ArrayRef<const char> &bytes);
    char *DuplicateString(const char *str, size_t max_len = SIZE_MAX);
};

// ------------------------------------------------------------------------
// String Format
// ------------------------------------------------------------------------

struct FmtArg {
    enum class Type {
        StrRef,
        Char,
        Bool,
        Integer,
        Unsigned,
        Double,
        Binary,
        Hexadecimal,
        MemorySize,
        DiskSize,
        Date,
        List
    };

    Type type;
    union {
        ArrayRef<const char> str_ref;
        char ch;
        bool b;
        int64_t i;
        uint64_t u;
        struct {
            double value;
            int precision;
        } d;
        const void *ptr;
        size_t size;
        Date date;
        struct {
            ArrayRef<FmtArg> args;
            const char *separator;
        } list;
    } value;
    int repeat = 1;

    FmtArg() = default;
    FmtArg(const char *str) : type(Type::StrRef) { value.str_ref = MakeStrRef(str ? str : "(null)"); }
    FmtArg(const ArrayRef<const char> str) : type(Type::StrRef) { value.str_ref = str; }
    FmtArg(char c) : type(Type::Char) { value.ch = c; }
    FmtArg(bool b) : type(Type::Bool) { value.b = b; }
    FmtArg(unsigned char u)  : type(Type::Unsigned) { value.u = u; }
    FmtArg(short i) : type(Type::Integer) { value.i = i; }
    FmtArg(unsigned short u) : type(Type::Unsigned) { value.u = u; }
    FmtArg(int i) : type(Type::Integer) { value.i = i; }
    FmtArg(unsigned int u) : type(Type::Unsigned) { value.u = u; }
    FmtArg(long i) : type(Type::Integer) { value.i = i; }
    FmtArg(unsigned long u) : type(Type::Unsigned) { value.u = u; }
    FmtArg(long long i) : type(Type::Integer) { value.i = i; }
    FmtArg(unsigned long long u) : type(Type::Unsigned) { value.u = u; }
    FmtArg(double d) : type(Type::Double) { value.d = { d, -1 }; }
    FmtArg(const void *ptr) : type(Type::Hexadecimal) { value.u = (uint64_t)ptr; }
    FmtArg(const Date &date) : type(Type::Date) { value.date = date; }

    FmtArg &Repeat(int new_repeat) { repeat = new_repeat; return *this; }
};

static inline FmtArg FmtBin(uint64_t u)
{
    FmtArg arg;
    arg.type = FmtArg::Type::Binary;
    arg.value.u = u;
    return arg;
}
static inline FmtArg FmtHex(uint64_t u)
{
    FmtArg arg;
    arg.type = FmtArg::Type::Hexadecimal;
    arg.value.u = u;
    return arg;
}

static inline FmtArg FmtDouble(double d, int precision = -1)
{
    FmtArg arg;
    arg.type = FmtArg::Type::Double;
    arg.value.d.value = d;
    arg.value.d.precision = precision;
    return arg;
}

static inline FmtArg FmtMemSize(size_t size)
{
    FmtArg arg;
    arg.type = FmtArg::Type::MemorySize;
    arg.value.size = size;
    return arg;
}
static inline FmtArg FmtDiskSize(size_t size)
{
    FmtArg arg;
    arg.type = FmtArg::Type::DiskSize;
    arg.value.size = size;
    return arg;
}
static inline FmtArg FmtList(const ArrayRef<FmtArg> &args, const char *sep = ", ")
{
    FmtArg arg;
    arg.type = FmtArg::Type::List;
    arg.value.list.args = args;
    arg.value.list.separator = sep;
    return arg;
}

enum class LogLevel {
    Debug,
    Info,
    Error
};

size_t FmtString(const ArrayRef<char> &buf, const char *fmt,
                 const ArrayRef<const FmtArg> &args);
char *FmtString(Allocator *alloc, const char *fmt, const ArrayRef<const FmtArg> &args);
void FmtPrint(FILE *fp, const char *fmt, const ArrayRef<const FmtArg> &args);
void FmtLog(LogLevel level, const char *ctx, const char *fmt,
            const ArrayRef<const FmtArg> &args);

// Print formatted strings to fixed-size buffer
static inline size_t Fmt(const ArrayRef<char> &buf, const char *fmt)
{
    return FmtString(buf, fmt, {});
}
template <typename... Args>
static inline size_t Fmt(const ArrayRef<char> &buf, const char *fmt, Args... args)
{
    const FmtArg fmt_args[] = { FmtArg(args)... };
    return FmtString(buf, fmt, fmt_args);
}

// Print formatted strings to dynamic char array
static inline char *Fmt(Allocator *alloc, const char *fmt)
{
    return FmtString(alloc, fmt, {});
}
template <typename... Args>
static inline char *Fmt(Allocator *alloc, const char *fmt, Args... args)
{
    const FmtArg fmt_args[] = { FmtArg(args)... };
    return FmtString(alloc, fmt, fmt_args);
}

// Print formatted strings to stdio FILE
static inline  void Print(FILE *fp, const char *fmt)
{
    FmtPrint(fp, fmt, {});
}
template <typename... Args>
static inline void Print(FILE *fp, const char *fmt, Args... args)
{
    const FmtArg fmt_args[] = { FmtArg(args)... };
    FmtPrint(fp, fmt, fmt_args);
}

// Print formatted strings to stdout
static inline void Print(const char *fmt)
{
    FmtPrint(stdout, fmt, {});
}
template <typename... Args>
static inline void Print(const char *fmt, Args... args)
{
    const FmtArg fmt_args[] = { FmtArg(args)... };
    FmtPrint(stdout, fmt, fmt_args);
}

// Variants of the Print() functions with terminating newline
template <typename... Args>
static inline void PrintLn(FILE *fp, const char *fmt, Args... args)
{
    Print(fp, fmt, args...);
    putc('\n', fp);
}
template <typename... Args>
static inline void PrintLn(const char *fmt, Args... args)
{
    Print(stdout, fmt, args...);
    putchar('\n');
}
static inline void PrintLn()
{
    putchar('\n');
}

// Log text line to stderr with context, for the Log() macros below
static inline void Log(LogLevel level, const char *ctx, const char *fmt)
{
    FmtLog(level, ctx, fmt, {});
}
template <typename... Args>
static inline void Log(LogLevel level, const char *ctx, const char *fmt, Args... args)
{
    const FmtArg fmt_args[] = { FmtArg(args)... };
    FmtLog(level, ctx, fmt, fmt_args);
}

static inline constexpr const char *SimplifyLogContext(const char *ctx)
{
    const char *new_ctx = ctx;
    for(; *ctx; ctx++) {
        if (*ctx == '/' || *ctx == '\\') {
            new_ctx = ctx + 1;
        }
    }
    return new_ctx;
}

// ------------------------------------------------------------------------
// Debug and errors
// ------------------------------------------------------------------------

#define LOG_LOCATION SimplifyLogContext(__FILE__ ":" STRINGIFY(__LINE__))
#define LogDebug(...) Log(LogLevel::Debug, LOG_LOCATION, __VA_ARGS__)
#define LogInfo(...) Log(LogLevel::Info, LOG_LOCATION, __VA_ARGS__)
#define LogError(...) Log(LogLevel::Error, LOG_LOCATION, __VA_ARGS__)

#define Abort(...) \
    do { \
        LogError(__VA_ARGS__); \
        abort(); \
    } while (false)

#define Assert(Cond) \
    do { \
        if (!(Cond)) { \
            Abort("Assertion '%1' failed", STRINGIFY(Cond)); \
        } \
    } while (false)
#ifndef NDEBUG
    #define DebugAssert(Cond) Assert(Cond)
#else
    #define DebugAssert(Cond) \
        do { \
            (void)(Cond); \
        } while (false)
#endif
#define StaticAssert(Cond) \
    static_assert((Cond), STRINGIFY(Cond))

// ------------------------------------------------------------------------
// Collections
// ------------------------------------------------------------------------

template <typename T>
T &ArrayRef<T>::operator[](size_t idx) const
{
    DebugAssert(idx < len);
    return ptr[idx];
}

template<typename T>
ArrayRef<T> ArrayRef<T>::Take(size_t sub_offset, size_t sub_len) const
{
    ArrayRef<T> sub;

    if (sub_len > len || sub_offset > len - sub_len) {
        sub = {};
        return sub;
    }
    sub.ptr = ptr + sub_offset;
    sub.len = sub_len;

    return sub;
}
template<typename T>
ArrayRef<T> ArrayRef<T>::Between(size_t sub_offset, size_t sub_end) const
{
    ArrayRef<T> between;

    if (sub_end > len || sub_offset > sub_end) {
        between = {};
        return between;
    }
    between.ptr = ptr + sub_offset;
    between.len = sub_end - sub_offset;

    return between;
}

inline const char &ArrayRef<const char>::operator[](size_t idx) const
{
    DebugAssert(idx < len);
    return ptr[idx];
}

inline ArrayRef<const char> ArrayRef<const char>::Take(size_t sub_offset, size_t sub_len) const
{
    ArrayRef<const char> sub;

    if (sub_len > len || sub_offset > len - sub_len) {
        sub = {};
        return sub;
    }
    sub.ptr = ptr + sub_offset;
    sub.len = sub_len;

    return sub;
}
inline ArrayRef<const char> ArrayRef<const char>::Between(size_t sub_offset, size_t sub_end) const
{
    ArrayRef<const char> between;

    if (sub_end > len || sub_offset > sub_end) {
        between = {};
        return between;
    }
    between.ptr = ptr + sub_offset;
    between.len = sub_end - sub_offset;

    return between;
}

inline char &ArrayRef<char>::operator[](size_t idx) const
{
    DebugAssert(idx < len);
    return ptr[idx];
}

inline ArrayRef<char> ArrayRef<char>::Take(size_t sub_offset, size_t sub_len) const
{
    ArrayRef<char> sub;

    if (sub_len > len || sub_offset > len - sub_len) {
        sub = {};
        return sub;
    }
    sub.ptr = ptr + sub_offset;
    sub.len = sub_len;

    return sub;
}
inline ArrayRef<char> ArrayRef<char>::Between(size_t sub_offset, size_t sub_end) const
{
    ArrayRef<char> between;

    if (sub_end > len || sub_offset > sub_end) {
        between = {};
        return between;
    }
    between.ptr = ptr + sub_offset;
    between.len = sub_end - sub_offset;

    return between;
}

template <typename T, size_t N>
class LocalArray {
public:
    T data[N];
    size_t len = 0;

    typedef T value_type;
    typedef T *iterator_type;

    ~LocalArray() { Clear(); }

    void Clear();

    operator ArrayRef<T>() { return ArrayRef<T>(data, len); }
    operator ArrayRef<const T>() const { return ArrayRef<const T>(data, len); }

    T *begin() { return data; }
    const T *begin() const { return data; }
    T *end() { return data + len; }
    const T *end() const { return data + len; }

    T &operator[](size_t idx);
    const T &operator[](size_t idx) const;

    T *Append(const T &value);
    T *Append(const ArrayRef<const T> &values);

    void RemoveLast(size_t count = 1) { RemoveAfter(count < len ? len - count : len); }
    void RemoveAfter(size_t first);
};

template <typename T, size_t N>
void LocalArray<T, N>::Clear()
{
    for (size_t i = 0; i < len; i++) {
        data[i].~T();
    }
    len = 0;
}

template <typename T, size_t N>
T &LocalArray<T, N>::operator[](size_t idx)
{
    DebugAssert(idx < len);
    return data[idx];
}
template <typename T, size_t N>
const T &LocalArray<T, N>::operator[](size_t idx) const
{
    DebugAssert(idx < len);
    return data[idx];
}

template <typename T, size_t N>
T *LocalArray<T, N>::Append(const T &value)
{
    DebugAssert(len < N);
    T *it = data + len;
    *it = value;
    len++;
    return it;
}

template <typename T, size_t N>
T *LocalArray<T, N>::Append(const ArrayRef<const T> &values)
{
    DebugAssert(data.len2 <= N - len);
    T *it = data + len;
    for (size_t i = 0; i < values.len; i++) {
        data[len + i] = values[i];
    }
    len += values.len;
    return it;
}

template <typename T, size_t N>
void LocalArray<T, N>::RemoveAfter(size_t first)
{
    if (first >= len) {
        return;
    }

    for (size_t i = first; i < len; i++) {
        data[i].~T();
    }
    len = first;
}

// TODO: Can we check some kind of is_trivially_movable template crap?
template <typename T>
class DynamicArray {
public:
    T *ptr = nullptr;
    size_t len = 0;
    size_t capacity = 0;
    Allocator *allocator = nullptr;

    typedef T value_type;
    typedef T *iterator_type;

    DynamicArray() = default;
    DynamicArray(DynamicArray &) = delete;
    DynamicArray &operator=(const DynamicArray &) = delete;

    ~DynamicArray();
    void Clear();

    operator ArrayRef<T>() { return ArrayRef<T>(ptr, len); }
    operator ArrayRef<const T>() const { return ArrayRef<const T>(ptr, len); }

    T *begin() { return ptr; }
    const T *begin() const { return ptr; }
    T *end() { return ptr + len; }
    const T *end() const { return ptr + len; }

    T &operator[](size_t idx) { return (T &)(*(const DynamicArray *)this)[idx]; }
    const T &operator[](size_t idx) const;

    void SetCapacity(size_t new_capacity);
    void Reserve(size_t min_capacity);
    void Grow(size_t reserve_capacity = 1);

    void Trim() { SetCapacity(len); }

    T *Append(const T &value);
    T *Append(const ArrayRef<const T> &values);

    void RemoveLast(size_t count = 1) { RemoveFrom(count < len ? len - count : len); }
    void RemoveFrom(size_t from);
};

template <typename T>
DynamicArray<T>::~DynamicArray()
{
    for (size_t i = 0; i < len; i++) {
        ptr[i].~T();
    }
}

template <typename T>
void DynamicArray<T>::Clear()
{
    SetCapacity(0);
}

template <typename T>
const T &DynamicArray<T>::operator[](size_t idx) const
{
    DebugAssert(idx < len);
    return ptr[idx];
}

template <typename T>
void DynamicArray<T>::SetCapacity(size_t new_capacity)
{
    if (new_capacity == capacity) {
        return;
    }

    if (len > new_capacity) {
        for (size_t i = new_capacity; i < len; i++) {
            ptr[i].~T();
        }
        len = new_capacity;
    }
    Allocator::Resize(allocator, (void **)&ptr, capacity * sizeof(T), new_capacity * sizeof(T));
    capacity = new_capacity;
}

template <typename T>
void DynamicArray<T>::Reserve(size_t min_capacity)
{
    if (min_capacity <= capacity) {
        return;
    }

    SetCapacity(min_capacity);
}

template <typename T>
void DynamicArray<T>::Grow(size_t reserve_capacity)
{
    if (reserve_capacity <= capacity - len) {
        return;
    }

    size_t needed_capacity;
#if !defined(NDEBUG)
    DebugAssert(!AddOverflow(capacity, reserve_capacity, &needed_capacity));
#else
    needed_capacity = capacity + reserve_capacity;
#endif

    size_t new_capacity;
    if (!capacity) {
        new_capacity = DYNAMICARRAY_BASE_CAPACITY;
    } else {
        new_capacity = capacity;
    }
    do {
        new_capacity = (size_t)(new_capacity * DYNAMICARRAY_GROWTH_FACTOR);
    } while (new_capacity < needed_capacity);

    SetCapacity(new_capacity);
}

template <typename T>
T *DynamicArray<T>::Append(const T &value)
{
    if (len == capacity) {
        Grow();
    }

    T *first = ptr + len;
    new (ptr + len) T;
    ptr[len++] = value;
    return first;
}

template <typename T>
T *DynamicArray<T>::Append(const ArrayRef<const T> &values)
{
    if (values.len > capacity - len) {
        Grow(values.len);
    }

    T *first = ptr + len;
    for (auto &value: values) {
        new (ptr + len) T;
        ptr[len++] = value;
    }
    return first;
}

template <typename T>
void DynamicArray<T>::RemoveFrom(size_t from)
{
    if (from >= len) {
        return;
    }

    for (size_t i = from; i < len; i++) {
        ptr[i].~T();
    }
    len = from;
}

template <typename T, size_t BucketSize = 1024>
class DynamicQueue {
public:
    struct Bucket {
        T *values;
        Allocator allocator;
    };

    class Iterator {
    public:
        class DynamicQueue<T, BucketSize> *queue;
        size_t idx;

        Iterator(DynamicQueue<T, BucketSize> *queue, size_t idx)
            : queue(queue), idx(idx) {}

        T *operator->() { return &(*queue)[idx]; }
        const T *operator->() const { return &(*queue)[idx]; }
        T &operator*() { return (*queue)[idx]; }
        const T &operator*() const { return (*queue)[idx]; }

        const Iterator &operator++()
        {
            idx++;
            return *this;
        }
        Iterator operator++(int)
        {
            Iterator ret = *this;
            idx++;
            return ret;
        }

        bool operator==(const Iterator &other) const
            { return queue == other.queue && idx == other.idx; }
        bool operator!=(const Iterator &other) const { return !(*this == other); }
    };

    DynamicArray<Bucket *> buckets;
    size_t offset = 0;
    size_t len = 0;
    Allocator *current_allocator = nullptr;

    typedef T value_type;
    typedef Iterator iterator_type;

    DynamicQueue();
    DynamicQueue(DynamicQueue &) = delete;
    DynamicQueue &operator=(const DynamicQueue &) = delete;

    ~DynamicQueue();
    void Clear();

    Iterator begin() { return Iterator(this, 0); }
    const Iterator begin() const { return Iterator(this, 0); }
    Iterator end() { return Iterator(this, len); }
    const Iterator end() const { return Iterator(this, len); }

    T &operator[](size_t idx) { return (T &)(*(const DynamicQueue *)this)[idx]; }
    const T &operator[](size_t idx) const;

    T *Append(const T &value);

    void RemoveLast(size_t count = 1) { RemoveFrom(count < len ? len - count : len); }
    void RemoveFrom(size_t from);
    void RemoveFirst(size_t count = 1);

private:
    Bucket *CreateBucket();
    void DeleteBucket(Bucket *bucket);
};

template <typename T, size_t BucketSize>
DynamicQueue<T, BucketSize>::DynamicQueue()
{
    Bucket *first_bucket = *buckets.Append(CreateBucket());
    current_allocator = &first_bucket->allocator;
}

template <typename T, size_t BucketSize>
DynamicQueue<T, BucketSize>::~DynamicQueue()
{
    for (Bucket *bucket: buckets) {
        DeleteBucket(bucket);
    }
}

template <typename T, size_t BucketSize>
void DynamicQueue<T, BucketSize>::Clear()
{
    for (Bucket *bucket: buckets) {
        DeleteBucket(bucket);
    }
    buckets.Clear();

    Bucket *first_bucket = *buckets.Append(CreateBucket());
    offset = 0;
    len = 0;
    current_allocator = &first_bucket->allocator;
}

template <typename T, size_t BucketSize>
const T &DynamicQueue<T, BucketSize>::operator[](size_t idx) const
{
    DebugAssert(idx < len);

    idx += offset;
    size_t bucket_idx = idx / BucketSize;
    size_t bucket_offset = idx % BucketSize;

    return buckets[bucket_idx]->values[bucket_offset];
}

template <typename T, size_t BucketSize>
T *DynamicQueue<T, BucketSize>::Append(const T &value)
{
    size_t bucket_idx = (offset + len) / BucketSize;
    size_t bucket_offset = (offset + len) % BucketSize;

    T *first = buckets[bucket_idx]->values + bucket_offset;
    new (first) T;
    *first = value;

    len++;
    if (bucket_offset == BucketSize - 1) {
        Bucket *new_bucket = *buckets.Append(CreateBucket());
        current_allocator = &new_bucket->allocator;
    }

    return first;
}

template <typename T, size_t BucketSize>
void DynamicQueue<T, BucketSize>::RemoveFrom(size_t from)
{
    if (from >= len) {
        return;
    }
    if (!from) {
        Clear();
        return;
    }

    size_t start_idx = offset + from;
    size_t end_idx = offset + len;
    size_t start_bucket_idx = start_idx / BucketSize;
    size_t end_bucket_idx = end_idx / BucketSize;

    {
        Iterator end_it = end();
        for (Iterator it(this, from); it != end_it; it++) {
            it->~T();
        }
    }

    for (size_t i = start_bucket_idx + 1; i <= end_bucket_idx; i++) {
        DeleteBucket(buckets[i]);
    }
    buckets.RemoveLast(end_bucket_idx - start_bucket_idx);
    if (start_idx % BucketSize == 0) {
        DeleteBucket(buckets[buckets.len - 1]);
        buckets[buckets.len - 1] = CreateBucket();
    }

    len = from;
    current_allocator = &buckets[buckets.len - 1]->allocator;
}

template <typename T, size_t BucketSize>
void DynamicQueue<T, BucketSize>::RemoveFirst(size_t count)
{
    if (count >= len) {
        Clear();
        return;
    }

    size_t end_idx = offset + count;
    size_t end_bucket_idx = end_idx / BucketSize;

    {
        Iterator end_it(this, count);
        for (Iterator it = begin(); it != end_it; it++) {
            it->~T();
        }
    }

    if (end_bucket_idx) {
        for (size_t i = 0; i < end_bucket_idx; i++) {
            DeleteBucket(buckets[i]);
        }
        memmove(&buckets[0], &buckets[end_bucket_idx],
                (buckets.len - end_bucket_idx) * sizeof(Bucket *));
        buckets.RemoveLast(end_bucket_idx);
    }

    offset = (offset + count) % BucketSize;
    len -= count;
}

template <typename T, size_t BucketSize>
typename DynamicQueue<T, BucketSize>::Bucket *DynamicQueue<T, BucketSize>::CreateBucket()
{
    Bucket *new_bucket = (Bucket *)Allocator::Allocate(buckets.allocator, sizeof(Bucket));
    new (&new_bucket->allocator) Allocator;
    new_bucket->values = (T *)Allocator::Allocate(&new_bucket->allocator, BucketSize * sizeof(T));
    return new_bucket;
}

template <typename T, size_t BucketSize>
void DynamicQueue<T, BucketSize>::DeleteBucket(Bucket *bucket)
{
    bucket->allocator.~Allocator();
    Allocator::Release(buckets.allocator, bucket, sizeof(Bucket));
}

template <typename T, uint64_t EmptyKey = 0>
class SparseTable {
public:
    struct Bucket {
        // We XOR the real key with EmptyKey so that xor_key == 0 for unused buckets no matter
        // what EmptyKey is set to. This way a simple memset() can be used when we allocate
        // the buckets array.
        uint64_t xor_key;
        T value;

        bool IsFree() const { return !xor_key; }
    };

    Bucket *buckets = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    Allocator *allocator = nullptr;

    SparseTable() = default;
    SparseTable(SparseTable &) = delete;
    SparseTable &operator=(const SparseTable &) = delete;

    ~SparseTable();
    void Clear();

    void Rehash(size_t new_capacity);

    T *Find(uint64_t key, const T *it = nullptr)
        { return (T *)((const SparseTable<T, EmptyKey> *)this)->Find(key, it); }
    const T *Find(uint64_t key, const T *it = nullptr) const;
    T FindValue(uint64_t key, const T &default_value)
        { return (T)((const SparseTable<T, EmptyKey> *)this)->FindValue(key, default_value); }
    const T FindValue(uint64_t key, const T &default_value) const;

    T *Add(uint64_t key, const T &value);
    T *Set(uint64_t key, const T &value);
    void Remove(T *it);

private:
    size_t InsertBucket(uint64_t xor_key, const T &value);
    size_t IteratorToIndex(const T *it) const;
};

template <typename T, uint64_t EmptyKey>
SparseTable<T, EmptyKey>::~SparseTable()
{
    for (size_t i = 0; i < capacity; i++) {
        if (buckets[i].IsFree()) {
            continue;
        }
        buckets[i].value.~T();
    }
}

template <typename T, uint64_t EmptyKey>
void SparseTable<T, EmptyKey>::Clear()
{
    for (size_t i = 0; i < capacity; i++) {
        if (buckets[i].IsFree()) {
            continue;
        }
        // Don't need to make it free if we drop the buckets array after
        // buckets[i].xor_key = 0;
        buckets[i].value.~T();
    }
    count = 0;
    Rehash(0);
}

template <typename T, uint64_t EmptyKey>
void SparseTable<T, EmptyKey>::Rehash(size_t new_capacity)
{
    if (new_capacity == capacity) {
        return;
    }
    Assert(count <= new_capacity);

    Bucket *old_buckets = buckets;
    if (new_capacity) {
        buckets = (Bucket *)Allocator::Allocate(allocator, new_capacity * sizeof(Bucket),
                                                Allocator::ZeroMemory);
        for (size_t i = 0; i < capacity; i++) {
            if (old_buckets[i].IsFree()) {
                continue;
            }
            InsertBucket(old_buckets[i].xor_key, old_buckets[i].value);
        }
    } else {
        buckets = nullptr;
    }
    Allocator::Release(allocator, old_buckets, capacity * sizeof(Bucket));
    capacity = new_capacity;
}

template <typename T, uint64_t EmptyKey>
const T *SparseTable<T, EmptyKey>::Find(uint64_t key, const T *it) const
{
    if (!capacity) {
        return nullptr;
    }

    uint64_t xor_key = key ^ EmptyKey;
    size_t first_idx = xor_key % capacity;

    size_t idx;
    if (it) {
        idx = IteratorToIndex(it);
    } else {
        if (buckets[first_idx].xor_key == xor_key) {
            return &buckets[first_idx].value;
        }
        idx = first_idx;
    }

    if (++idx == capacity) {
        idx = 0;
    }
    while (idx != first_idx) {
        if (buckets[idx].xor_key == xor_key) {
            return &buckets[idx].value;
        }
        if (++idx == capacity) {
            idx = 0;
        }
    }
    return nullptr;
}

template <typename T, uint64_t EmptyKey>
const T SparseTable<T, EmptyKey>::FindValue(uint64_t key, const T &default_value) const
{
    const T *it = Find(key);
    if (it) {
        return *it;
    } else {
        return default_value;
    }
}

template <typename T, uint64_t EmptyKey>
T *SparseTable<T, EmptyKey>::Add(uint64_t key, const T &value)
{
    Assert(key != EmptyKey);

    if (count >= (size_t)(capacity * SPARSETABLE_MAX_LOAD_FACTOR)) {
        size_t new_capacity = (size_t)(capacity * SPARSETABLE_GROWTH_FACTOR);
        if (new_capacity < SPARSETABLE_BASE_CAPACITY) {
            new_capacity = SPARSETABLE_BASE_CAPACITY;
        }
        Rehash(new_capacity);
    }

    uint64_t xor_key = key ^ EmptyKey;
    size_t idx = InsertBucket(xor_key, value);
    count++;
    return &buckets[idx].value;
}

template <typename T, uint64_t EmptyKey>
T *SparseTable<T, EmptyKey>::Set(uint64_t key, const T &value)
{
    T *it = Find(key);
    if (it) {
        *it = value;
        return it;
    } else {
        return Add(key, value);
    }
}

template <typename T, uint64_t EmptyKey>
void SparseTable<T, EmptyKey>::Remove(T *it)
{
    if (!it) {
        return;
    }

    size_t idx = IteratorToIndex(it);
    buckets[idx].xor_key = 0;
    buckets[idx].value.~T();
}

template <typename T, uint64_t EmptyKey>
size_t SparseTable<T, EmptyKey>::InsertBucket(uint64_t xor_key, const T &value)
{
    size_t first_idx = xor_key % capacity;
    size_t idx = first_idx;
    do {
        if (buckets[idx].IsFree()) {
            buckets[idx].xor_key = xor_key;
            new (&buckets[idx].value) T;
            buckets[idx].value = value;
            return idx;
        }
        if (++idx == capacity) {
            idx = 0;
        }
    } while (idx != first_idx);

    Assert(false);
}

template <typename T, uint64_t EmptyKey>
size_t SparseTable<T, EmptyKey>::IteratorToIndex(const T *it) const
{
    return (size_t)((const Bucket *)((const uint8_t *)it - OffsetOf(Bucket, value)) - buckets);
}

// djb2
static inline uint64_t HashString(const char *str)
{
    uint32_t hash = 0;
    for (size_t i = 0; str[i]; i++) {
        hash = hash * 33 + (uint8_t)str[i];
    }
    return (uint64_t)hash + 1;
}

template <typename HashTableImpl, typename KeyType, typename ValueType>
class HashTableBase {
public:
    SparseTable<ValueType> map;
    Allocator *&allocator = map.allocator;

    typedef KeyType key_type;
    typedef ValueType value_type;

    ValueType *Set(const ValueType &value)
    {
        KeyType key = ((HashTableImpl *)this)->GetKey(value);
        uint64_t hash = ((HashTableImpl *)this)->HashKey(key);
        ValueType *it = Find(hash, key);
        if (it) {
            *it = value;
            return it;
        } else {
            return map.Add(hash, value);
        }
    }
    void Remove(ValueType *it) { map.Remove(it); }
    void Remove(KeyType key) { Remove(Find(key)); }

    ValueType *Find(const KeyType &key)
        { return (ValueType *)((const HashTableBase *)this)->Find(key); }
    const ValueType *Find(const KeyType &key) const
    {
        uint64_t hash = ((HashTableImpl *)this)->HashKey(key);
        return Find(hash, key);
    }

private:
    ValueType *Find(uint64_t hash, const KeyType &key)
        { return (ValueType *)((const HashTableBase *)this)->Find(hash, key); }
    const ValueType *Find(uint64_t hash, const KeyType &key) const
    {
        const ValueType *it = nullptr;
        while ((it = map.Find(hash, it))) {
            KeyType it_key = ((HashTableImpl *)this)->GetKey(*it);
            if (!((HashTableImpl *)this)->CompareKeys(key, it_key)) {
                return it;
            }
        }
        return nullptr;
    }
};

#define HASH_TABLE_TYPE(Name, KeyType, ValueType) \
    struct Name: public HashTableBase<Name, KeyType, ValueType>

// ------------------------------------------------------------------------
// Files
// ------------------------------------------------------------------------

bool ReadFile(Allocator *alloc, const char *filename, uint64_t max_size,
              uint8_t **rdata, uint64_t *rlen);
