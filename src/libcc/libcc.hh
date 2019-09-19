// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <errno.h>
#include <float.h>
#include <functional>
#include <inttypes.h>
#include <limits.h>
#include <limits>
#include <math.h>
#include <memory>
#include <mutex>
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
#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#endif
#ifdef _MSC_VER
    #define ENABLE_INTSAFE_SIGNED_FUNCTIONS
    #include <intsafe.h>
    #pragma intrinsic(_BitScanReverse)
    #ifdef _WIN64
        #pragma intrinsic(_BitScanReverse64)
    #endif
    #pragma intrinsic(__rdtsc)
#endif

namespace RG {

// ------------------------------------------------------------------------
// Config
// ------------------------------------------------------------------------

#define RG_DEFAULT_ALLOCATOR MallocAllocator
#define RG_BLOCK_ALLOCATOR_DEFAULT_SIZE Kibibytes(4)

#define RG_HEAPARRAY_BASE_CAPACITY 8
#define RG_HEAPARRAY_GROWTH_FACTOR 1.5

// Must be a power-of-two
#define RG_HASHTABLE_BASE_CAPACITY 32
#define RG_HASHTABLE_MAX_LOAD_FACTOR 0.5

#define RG_FMT_STRING_BASE_CAPACITY 256
#define RG_FMT_STRING_PRINT_BUFFER_SIZE 1024

#define RG_LINE_READER_STEP_SIZE 65536

#define RG_ASYNC_MAX_WORKERS 255
#define RG_ASYNC_MAX_IDLE_TIME 10000

// ------------------------------------------------------------------------
// Utility
// ------------------------------------------------------------------------

extern "C" const char *BuildVersion;

enum class Endianness {
    LittleEndian,
    BigEndian
};

#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
    #define RG_ARCH_64
    #define RG_ARCH_LITTLE_ENDIAN
    #define RG_ARCH_ENDIANNESS (Endianness::LittleEndian)

    typedef int64_t Size;
    #define RG_SIZE_MAX INT64_MAX
#elif defined(__i386__) || defined(_M_IX86) || defined(__arm__) || defined(__EMSCRIPTEN__)
    #define RG_ARCH_32
    #define RG_ARCH_LITTLE_ENDIAN
    #define RG_ARCH_ENDIANNESS (Endianness::LittleEndian)

    typedef int32_t Size;
    #define RG_SIZE_MAX INT32_MAX
#else
    #error Machine architecture not supported
#endif

#define RG_STRINGIFY_(a) #a
#define RG_STRINGIFY(a) RG_STRINGIFY_(a)
#define RG_CONCAT_(a, b) a ## b
#define RG_CONCAT(a, b) RG_CONCAT_(a, b)
#define RG_UNIQUE_ID(prefix) RG_CONCAT(prefix, __LINE__)
#define RG_FORCE_EXPAND(x) x

#if defined(__GNUC__)
    #define RG_PUSH_NO_WARNINGS() \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wall\"")
        _Pragma("GCC diagnostic ignored \"-Wextra\"")
        _Pragma("GCC diagnostic ignored \"-Wconversion\"")
        _Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
    #define RG_POP_NO_WARNINGS() \
        _Pragma("GCC diagnostic pop")

    // thread_local has many bugs with MinGW (Windows):
    // - Destructors are run after the memory is freed
    // - It crashes when used in R packages
    #ifdef __EMSCRIPTEN__
        #define RG_THREAD_LOCAL
    #else
        #define RG_THREAD_LOCAL __thread
    #endif
    #define RG_NORETURN __attribute__((noreturn))
    #define RG_MAYBE_UNUSED __attribute__((unused))
    #define RG_FORCE_INLINE __attribute__((always_inline)) inline
    #define RG_LIKELY(Cond) __builtin_expect(!!(Cond), 1)
    #define RG_UNLIKELY(Cond) __builtin_expect(!!(Cond), 0)
    #define RG_RESTRICT __restrict__
    #define RG_UNREACHABLE() __builtin_unreachable()

    #ifndef SCNd8
        #define SCNd8 "hhd"
    #endif
    #ifndef SCNi8
        #define SCNi8 "hhd"
    #endif
    #ifndef SCNu8
        #define SCNu8 "hhu"
    #endif
#elif defined(_MSC_VER)
    #define RG_PUSH_NO_WARNINGS()
    #define RG_POP_NO_WARNINGS()

    #define RG_THREAD_LOCAL thread_local
    #define RG_NORETURN __declspec(noreturn)
    #define RG_MAYBE_UNUSED
    #define RG_FORCE_INLINE __forceinline
    #define RG_LIKELY(Cond) (Cond)
    #define RG_UNLIKELY(Cond) (Cond)
    #define RG_RESTRICT __restrict
    #define RG_UNREACHABLE() __assume(0)
#else
    #error Compiler not supported
#endif

#if __cplusplus >= 201703L
    #define RG_FALLTHROUGH [[fallthrough]]
#elif defined(__clang__)
    #define RG_FALLTHROUGH [[clang::fallthrough]]
#elif __GNUC__ >= 7
    #define RG_FALLTHROUGH [[gnu::fallthrough]]
#else
    #define RG_FALLTHROUGH
#endif

extern "C" void RG_NORETURN AssertFail(const char *filename, int line, const char *cond);

#ifndef NDEBUG
    #define RG_ASSERT(Cond) \
        do { \
            if (!RG_LIKELY(Cond)) \
                RG::AssertFail(__FILE__, __LINE__, RG_STRINGIFY(Cond)); \
        } while (false)
#else
    #define RG_ASSERT(Cond) \
        do { \
            if (!RG_LIKELY(Cond)) { \
                RG_UNREACHABLE(); \
            } \
        } while (false)
#endif
#define RG_STATIC_ASSERT(Cond) \
    static_assert((Cond), RG_STRINGIFY(Cond))

#ifdef _WIN32
    #define RG_EXPORT __declspec(dllexport)
#else
    #define RG_EXPORT __attribute__((visibility("default")))
#endif

constexpr uint16_t MakeUInt16(uint8_t high, uint8_t low)
    { return (uint16_t)(((uint16_t)high << 8) | low); }
constexpr uint32_t MakeUInt32(uint16_t high, uint16_t low) { return ((uint32_t)high << 16) | low; }
constexpr uint64_t MakeUInt64(uint32_t high, uint32_t low) { return ((uint64_t)high << 32) | low; }

constexpr Size Mebibytes(Size len) { return len * 1024 * 1024; }
constexpr Size Kibibytes(Size len) { return len * 1024; }
constexpr Size Megabytes(Size len) { return len * 1000 * 1000; }
constexpr Size Kilobytes(Size len) { return len * 1000; }

#define RG_SIZE(Type) ((RG::Size)sizeof(Type))
template <typename T, unsigned N>
char (&ComputeArraySize(T const (&)[N]))[N];
#define RG_LEN(Array) RG_SIZE(RG::ComputeArraySize(Array))
#define RG_OFFSET_OF(Type, Member) ((Size)&(((Type *)nullptr)->Member))

static inline constexpr uint16_t ReverseBytes(uint16_t u)
{
    return (uint16_t)(((u & 0x00FF) << 8) |
                      ((u & 0xFF00) >> 8));
}

static inline constexpr uint32_t ReverseBytes(uint32_t u)
{
    return ((u & 0x000000FF) << 24) |
           ((u & 0x0000FF00) << 8)  |
           ((u & 0x00FF0000) >> 8)  |
           ((u & 0xFF000000) >> 24);
}

static inline constexpr uint64_t ReverseBytes(uint64_t u)
{
    return ((u & 0x00000000000000FF) << 56) |
           ((u & 0x000000000000FF00) << 40) |
           ((u & 0x0000000000FF0000) << 24) |
           ((u & 0x00000000FF000000) << 8)  |
           ((u & 0x000000FF00000000) >> 8)  |
           ((u & 0x0000FF0000000000) >> 24) |
           ((u & 0x00FF000000000000) >> 40) |
           ((u & 0xFF00000000000000) >> 56);
}

static inline constexpr int16_t ReverseBytes(int16_t i)
    { return (int16_t)ReverseBytes((uint16_t)i); }
static inline constexpr int32_t ReverseBytes(int32_t i)
    { return (int32_t)ReverseBytes((uint32_t)i); }
static inline constexpr int64_t ReverseBytes(int64_t i)
    { return (int64_t)ReverseBytes((uint64_t)i); }

#ifdef RG_ARCH_LITTLE_ENDIAN
    template <typename T>
    constexpr T LittleEndian(T v) { return v; }

    template <typename T>
    constexpr T BigEndian(T v) { return ReverseBytes(v); }
#else
    template <typename T>
    constexpr T LittleEndian(T v) { return ReverseBytes(v); }

    template <typename T>
    constexpr T BigEndian(T v) { return v; }
#endif

static inline void SwapMemory(void *RG_RESTRICT ptr1, void *RG_RESTRICT ptr2, Size len)
{
    uint8_t *raw1 = (uint8_t *)ptr1, *raw2 = (uint8_t *)ptr2;
    for (Size i = 0; i < len; i++) {
        uint8_t tmp = raw1[i];
        raw1[i] = raw2[i];
        raw2[i] = tmp;
    }
}

#if defined(__GNUC__)
    static inline int CountLeadingZeros(uint32_t u)
    {
        if (!u)
            return 32;

        return __builtin_clz(u);
    }
    static inline int CountLeadingZeros(uint64_t u)
    {
        if (!u)
            return 64;

    #if UINT64_MAX == ULONG_MAX
        return __builtin_clzl(u);
    #elif UINT64_MAX == ULLONG_MAX
        return __builtin_clzll(u);
    #else
        #error Neither unsigned long nor unsigned long long is a 64-bit unsigned integer
    #endif
    }

    static inline int CountTrailingZeros(uint32_t u)
    {
        if (!u)
            return 32;

        return __builtin_ctz(u);
    }
    static inline int CountTrailingZeros(uint64_t u)
    {
        if (!u)
            return 64;

    #if UINT64_MAX == ULONG_MAX
        return __builtin_ctzl(u);
    #elif UINT64_MAX == ULLONG_MAX
        return __builtin_ctzll(u);
    #else
        #error Neither unsigned long nor unsigned long long is a 64-bit unsigned integer
    #endif
    }

    static inline int PopCount(uint32_t u)
    {
        return __builtin_popcount(u);
    }
    static inline int PopCount(uint64_t u)
    {
        return __builtin_popcountll(u);
    }
#elif defined(_MSC_VER)
    static inline int CountLeadingZeros(uint32_t u)
    {
        unsigned long leading_zero;
        if (_BitScanReverse(&leading_zero, u)) {
            return (int)(31 - leading_zero);
        } else {
            return 32;
        }
    }
    static inline int CountLeadingZeros(uint64_t u)
    {
        unsigned long leading_zero;
    #ifdef _WIN64
        if (_BitScanReverse64(&leading_zero, u)) {
            return (int)(63 - leading_zero);
        } else {
            return 64;
        }
    #else
        if (_BitScanReverse(&leading_zero, u >> 32)) {
            return (int)(31 - leading_zero);
        } else if (_BitScanReverse(&leading_zero, (uint32_t)u)) {
            return (int)(63 - leading_zero);
        } else {
            return 64;
        }
    #endif
    }

    static inline int CountTrailingZeros(uint32_t u)
    {
        unsigned long trailing_zero;
        if (_BitScanForward(&trailing_zero, u)) {
            return (int)trailing_zero;
        } else {
            return 32;
        }
    }
    static inline int CountTrailingZeros(uint64_t u)
    {
        unsigned long trailing_zero;
    #ifdef _WIN64
        if (_BitScanForward64(&trailing_zero, u)) {
            return (int)trailing_zero;
        } else {
            return 64;
        }
    #else
        if (_BitScanForward(&trailing_zero, (uint32_t)u)) {
            return trailing_zero;
        } else if (_BitScanForward(&trailing_zero, u >> 32)) {
            return 32 + trailing_zero;
        } else {
            return 64;
        }
    #endif
    }

    static inline int PopCount(uint32_t u)
    {
        return __popcnt(u);
    }
    static inline int PopCount(uint64_t u)
    {
    #ifdef _WIN64
        return (int)__popcnt64(u);
    #else
        return __popcnt(u >> 32) + __popcnt((uint32_t)u);
    #endif
    }
#else
    #error No implementation of CountLeadingZeros(), CountTrailingZeros() and PopCount() for this compiler / toolchain
#endif

template <typename T, typename = typename std::enable_if<std::is_enum<T>::value, T>>
typename std::underlying_type<T>::type MaskEnum(T value)
{
    auto mask = 1 << static_cast<typename std::underlying_type<T>::type>(value);
    return (typename std::underlying_type<T>::type)mask;
}

template <typename Fun>
class DeferGuard {
    Fun f;
    bool enabled;

public:
    DeferGuard() = delete;
    DeferGuard(Fun f_, bool enable = true) : f(std::move(f_)), enabled(enable) {}
    ~DeferGuard()
    {
        if (enabled) {
            f();
        }
    }

    DeferGuard(DeferGuard &&other)
        : f(std::move(other.f)), enabled(other.enabled)
    {
        other.enabled = false;
    }

    DeferGuard(const DeferGuard &) = delete;
    DeferGuard &operator=(DeferGuard &) = delete;

    void Disable() { enabled = false; }
};

// Honestly, I don't understand all the details in there, this comes from Andrei Alexandrescu.
// https://channel9.msdn.com/Shows/Going+Deep/C-and-Beyond-2012-Andrei-Alexandrescu-Systematic-Error-Handling-in-C
struct DeferGuardHelper {};
template <typename Fun>
DeferGuard<Fun> operator+(DeferGuardHelper, Fun &&f)
{
    return DeferGuard<Fun>(std::forward<Fun>(f));
}

// Write 'DEFER { code };' to do something at the end of the current scope, you
// can use DEFER_N(Name) if you need to disable the guard for some reason, and
// DEFER_NC(Name, Captures) if you need to capture values.
#define RG_DEFER \
    auto RG_UNIQUE_ID(defer) = RG::DeferGuardHelper() + [&]()
#define RG_DEFER_N(Name) \
    auto Name = RG::DeferGuardHelper() + [&]()
#define RG_DEFER_C(...) \
    auto RG_UNIQUE_ID(defer) = RG::DeferGuardHelper() + [&, __VA_ARGS__]()
#define RG_DEFER_NC(Name, ...) \
    auto Name = RG::DeferGuardHelper() + [&, __VA_ARGS__]()

// Copied the code for FunctionRef from LLVM
template<typename Fn> class FunctionRef;
template<typename Ret, typename ...Params>
class FunctionRef<Ret(Params...)> {
    Ret (*callback)(intptr_t callable, Params ...params) = nullptr;
    intptr_t callable;

    template<typename Callable>
    static Ret callback_fn(intptr_t callable, Params ...params)
        { return (*reinterpret_cast<Callable*>(callable))(std::forward<Params>(params)...); }

public:
    FunctionRef() = default;
    FunctionRef(std::nullptr_t) {}

    template <typename Callable>
    FunctionRef(Callable &&callable,
                typename std::enable_if<!std::is_same<typename std::remove_reference<Callable>::type,
                                        FunctionRef>::value>::type * = nullptr)
      : callback(callback_fn<typename std::remove_reference<Callable>::type>),
        callable(reinterpret_cast<intptr_t>(&callable)) {}

    Ret operator()(Params ...params) const
        { return callback(callable, std::forward<Params>(params)...); }

    operator bool() const { return callback; }
};

#define RG_INIT(Name) \
    class RG_UNIQUE_ID(InitHelper) { \
    public: \
        RG_UNIQUE_ID(InitHelper)(); \
    }; \
    static RG_UNIQUE_ID(InitHelper) RG_UNIQUE_ID(init); \
    RG_UNIQUE_ID(InitHelper)::RG_UNIQUE_ID(InitHelper)()

#define RG_EXIT(Name) \
    class RG_UNIQUE_ID(ExitHelper) { \
    public: \
        ~RG_UNIQUE_ID(ExitHelper)(); \
    }; \
    static RG_UNIQUE_ID(ExitHelper) RG_UNIQUE_ID(exit); \
    RG_UNIQUE_ID(ExitHelper)::~RG_UNIQUE_ID(ExitHelper)()

template <typename T>
T MultiCmp()
{
    return 0;
}
template <typename T, typename... Args>
T MultiCmp(T cmp_value, Args... other_args)
{
    if (cmp_value) {
        return cmp_value;
    } else {
        return MultiCmp<T>(other_args...);
    }
}

template <typename T, typename U>
T ApplyMask(T value, U mask, bool enable)
{
    if (enable) {
        return value | (T)mask;
    } else {
        return value & (T)~mask;
    }
}

template <typename T, typename Fun>
auto FindIf(const T &arr, Fun func) -> decltype(&(*std::begin(arr)))
{
    for (auto &it: arr) {
        if (func(it))
            return &it;
    }
    return nullptr;
}

enum class ParseFlag {
    Log = 1 << 0,
    Validate = 1 << 1,
    End = 1 << 2
};
#define RG_DEFAULT_PARSE_FLAGS ((int)ParseFlag::Log | (int)ParseFlag::Validate | (int)ParseFlag::End)

// ------------------------------------------------------------------------
// Memory / Allocator
// ------------------------------------------------------------------------

class Allocator {
public:
    enum class Flag {
        Zero = 1,
        Resizable = 2
    };

    Allocator() = default;
    virtual ~Allocator() = default;
    Allocator(Allocator &) = delete;
    Allocator &operator=(const Allocator &) = delete;

    static void *Allocate(Allocator *alloc, Size size, unsigned int flags = 0);
    static void Resize(Allocator *alloc, void **ptr, Size old_size, Size new_size,
                       unsigned int flags = 0);
    static void Release(Allocator *alloc, void *ptr, Size size);

protected:
    virtual void *Allocate(Size size, unsigned int flags = 0) = 0;
    virtual void Resize(void **ptr, Size old_size, Size new_size, unsigned int flags = 0) = 0;
    virtual void Release(void *ptr, Size size) = 0;
};

class LinkedAllocator: public Allocator {
    struct Node {
        Node *prev;
        Node *next;
    };
    struct Bucket {
         // Keep head first or stuff will break
        Node head;
        alignas(8) uint8_t data[];
    };

    Allocator *allocator;
    // We want allocators to be memmovable, which means we can't use a circular linked list.
    // Even though it makes the code less nice.
    Node list = {};

public:
    LinkedAllocator(Allocator *alloc = nullptr) : allocator(alloc) {}
    ~LinkedAllocator() override { ReleaseAll(); }
    void ReleaseAll();

protected:
    void *Allocate(Size size, unsigned int flags = 0) override;
    void Resize(void **ptr, Size old_size, Size new_size, unsigned int flags = 0) override;
    void Release(void *ptr, Size size) override;

private:
    static Bucket *PointerToBucket(void *ptr)
        { return (Bucket *)((uint8_t *)ptr - RG_OFFSET_OF(Bucket, data)); }
};

class BlockAllocatorBase: public Allocator {
    struct Bucket {
        Size used;
        alignas(8) uint8_t data[];
    };

    Size block_size;

    Bucket *current_bucket = nullptr;
    uint8_t *last_alloc = nullptr;

public:
    BlockAllocatorBase(Size block_size = RG_BLOCK_ALLOCATOR_DEFAULT_SIZE)
        : block_size(block_size)
    {
        RG_ASSERT(block_size > 0);
    }

protected:
    void *Allocate(Size size, unsigned int flags = 0) override;
    void Resize(void **ptr, Size old_size, Size new_size, unsigned int flags = 0) override;
    void Release(void *ptr, Size size) override;

    void ForgetCurrentBlock();

    virtual LinkedAllocator *GetAllocator() = 0;

private:
    bool AllocateSeparately(Size aligned_size) const { return aligned_size >= block_size / 2; }

    static Size AlignSizeValue(Size size)
        { return (RG_SIZE(Bucket) + size + 7) / 8 * 8 - RG_SIZE(Bucket); }
};

class BlockAllocator: public BlockAllocatorBase {
    LinkedAllocator allocator;

protected:
    virtual LinkedAllocator *GetAllocator() { return &allocator; }

public:
    BlockAllocator(Size block_size = RG_BLOCK_ALLOCATOR_DEFAULT_SIZE)
        : BlockAllocatorBase(block_size) {}

    void ReleaseAll();
};

class IndirectBlockAllocator: public BlockAllocatorBase {
    LinkedAllocator *allocator;

protected:
    virtual LinkedAllocator *GetAllocator() { return allocator; }

public:
    IndirectBlockAllocator(LinkedAllocator *alloc, Size block_size = RG_BLOCK_ALLOCATOR_DEFAULT_SIZE)
        : BlockAllocatorBase(block_size), allocator(alloc) {}

    void ReleaseAll();
};

// ------------------------------------------------------------------------
// Collections
// ------------------------------------------------------------------------

// I'd love to make Span default to { nullptr, 0 } but unfortunately that makes
// it a non-POD and prevents putting it in a union.
template <typename T>
struct Span {
    T *ptr;
    Size len;

    Span() = default;
    constexpr Span(T &value) : ptr(&value), len(1) {}
    constexpr Span(std::initializer_list<T> l) : ptr(l.begin()), len((Size)l.size()) {}
    constexpr Span(T *ptr_, Size len_) : ptr(ptr_), len(len_) {}
    template <Size N>
    constexpr Span(T (&arr)[N]) : ptr(arr), len(N) {}

    void Reset()
    {
        ptr = nullptr;
        len = 0;
    }

    T *begin() { return ptr; }
    const T *begin() const { return ptr; }
    T *end() { return ptr + len; }
    const T *end() const { return ptr + len; }

    bool IsValid() const { return ptr; }

    T &operator[](Size idx)
    {
        RG_ASSERT(idx >= 0 && idx < len);
        return ptr[idx];
    }
    const T &operator[](Size idx) const
    {
        RG_ASSERT(idx >= 0 && idx < len);
        return ptr[idx];
    }

    operator Span<const T>() const { return Span<const T>(ptr, len); }

    bool operator==(const Span &other) const
    {
        if (len != other.len)
            return false;

        for (Size i = 0; i < len; i++) {
            if (ptr[i] != other.ptr[i])
                return false;
        }

        return true;
    }
    bool operator!=(const Span &other) const { return !(*this == other); }

    Span Take(Size offset, Size sub_len) const
    {
        RG_ASSERT(sub_len >= 0 && sub_len <= len);
        RG_ASSERT(offset >= 0 && offset <= len - sub_len);

        Span<T> sub;
        sub.ptr = ptr + offset;
        sub.len = sub_len;
        return sub;
    }

    template <typename U>
    Span<U> CastAs() const { return Span<U>((U *)ptr, len); }
};

// Use strlen() to build Span<const char> instead of the template-based
// array constructor.
template <>
struct Span<const char> {
    const char *ptr;
    Size len;

    Span() = default;
    constexpr Span(const char &ch) : ptr(&ch), len(1) {}
    constexpr Span(const char *ptr_, Size len_) : ptr(ptr_), len(len_) {}
    Span(const char *const &str) : ptr(str), len(str ? (Size)strlen(str) : 0) {}

    void Reset()
    {
        ptr = nullptr;
        len = 0;
    }

    const char *begin() const { return ptr; }
    const char *end() const { return ptr + len; }

    bool IsValid() const { return ptr; }

    char operator[](Size idx) const
    {
        RG_ASSERT(idx >= 0 && idx < len);
        return ptr[idx];
    }

    // The implementation comes later, after TestStr() is available
    bool operator==(Span<const char> other) const;
    bool operator==(const char *other) const;
    bool operator!=(Span<const char> other) const { return !(*this == other); }
    bool operator!=(const char *other) const { return !(*this == other); }

    Span Take(Size offset, Size sub_len) const
    {
        RG_ASSERT(sub_len >= 0 && sub_len <= len);
        RG_ASSERT(offset >= 0 && offset <= len - sub_len);

        Span<const char> sub;
        sub.ptr = ptr + offset;
        sub.len = sub_len;
        return sub;
    }

    template <typename U>
    Span<U> CastAs() const { return Span<U>((U *)ptr, len); }
};

template <typename T>
static inline constexpr Span<T> MakeSpan(T *ptr, Size len)
{
    return Span<T>(ptr, len);
}
template <typename T>
static inline constexpr Span<T> MakeSpan(T *ptr, T *end)
{
    return Span<T>(ptr, end - ptr);
}
template <typename T, Size N>
static inline constexpr Span<T> MakeSpan(T (&arr)[N])
{
    return Span<T>(arr, N);
}

template <typename T>
class Strider {
public:
    void *ptr = nullptr;
    Size stride;

    Strider() = default;
    constexpr Strider(T *ptr_) : ptr(ptr_), stride(RG_SIZE(T)) {}
    constexpr Strider(T *ptr_, Size stride_) : ptr(ptr_), stride(stride_) {}

    bool IsValid() const { return ptr; }

    T &operator[](Size idx) const
    {
        RG_ASSERT(idx >= 0);
        return *(T *)((uint8_t *)ptr + (idx * stride));
    }
};

template <typename T>
static inline constexpr Strider<T> MakeStrider(T *ptr)
{
    return Strider<T>(ptr, RG_SIZE(T));
}
template <typename T>
static inline constexpr Strider<T> MakeStrider(T *ptr, Size stride)
{
    return Strider<T>(ptr, stride);
}
template <typename T, Size N>
static inline constexpr Strider<T> MakeStrider(T (&arr)[N])
{
    return Strider<T>(arr, RG_SIZE(T));
}

template <typename T, Size N>
class FixedArray {
public:
    T data[N];

    typedef T value_type;
    typedef T *iterator_type;

    FixedArray() = default;
    FixedArray(std::initializer_list<T> l)
    {
        RG_ASSERT(l.size() <= N);
        for (Size i = 0; i < l.size(); i++) {
            data[i] = l[i];
        }
    }

    operator Span<T>() { return Span<T>(data, N); }
    operator Span<const T>() const { return Span<const T>(data, N); }

    T *begin() { return data; }
    const T *begin() const { return data; }
    T *end() { return data + N; }
    const T *end() const { return data + N; }

    T &operator[](Size idx)
    {
        RG_ASSERT(idx >= 0 && idx < N);
        return data[idx];
    }
    const T &operator[](Size idx) const
    {
        RG_ASSERT(idx >= 0 && idx < N);
        return data[idx];
    }

    bool operator==(const FixedArray &other) const
    {
        for (Size i = 0; i < N; i++) {
            if (data[i] != other.data[i])
                return false;
        }

        return true;
    }
    bool operator!=(const FixedArray &other) const { return !(*this == other); }

    Span<T> Take(Size offset, Size len) const
        { return Span<T>((T *)data, this->len).Take(offset, len); }
};

template <typename T, Size N>
class LocalArray {
public:
    T data[N];
    Size len = 0;

    typedef T value_type;
    typedef T *iterator_type;

    LocalArray() = default;
    LocalArray(std::initializer_list<T> l)
    {
        RG_ASSERT(l.size() <= N);
        for (const T &val: l) {
            Append(val);
        }
    }

    void Clear()
    {
        for (Size i = 0; i < len; i++) {
            data[i] = T();
        }
        len = 0;
    }

    operator Span<T>() { return Span<T>(data, len); }
    operator Span<const T>() const { return Span<const T>(data, len); }

    T *begin() { return data; }
    const T *begin() const { return data; }
    T *end() { return data + len; }
    const T *end() const { return data + len; }

    Size Available() const { return RG_SIZE(data) - len; }

    T &operator[](Size idx)
    {
        RG_ASSERT(idx >= 0 && idx < len);
        return data[idx];
    }
    const T &operator[](Size idx) const
    {
        RG_ASSERT(idx >= 0 && idx < len);
        return data[idx];
    }

    bool operator==(const LocalArray &other) const
    {
        if (len != other.len)
            return false;

        for (Size i = 0; i < len; i++) {
            if (data[i] != other.data[i])
                return false;
        }

        return true;
    }
    bool operator!=(const LocalArray &other) const { return !(*this == other); }

    T *AppendDefault(Size count = 1)
    {
        RG_ASSERT(len <= N - count);

        T *it = data + len;
        *it = {};
        len += count;

        return it;
    }

    T *Append(const T &value)
    {
        RG_ASSERT(len < N);

        T *it = data + len;
        *it = value;
        len++;

        return it;
    }
    T *Append(Span<const T> values)
    {
        RG_ASSERT(values.len <= N - len);

        T *it = data + len;
        for (Size i = 0; i < values.len; i++) {
            data[len + i] = values[i];
        }
        len += values.len;

        return it;
    }

    void RemoveFrom(Size first)
    {
        RG_ASSERT(first >= 0 && first <= len);

        for (Size i = first; i < len; i++) {
            data[i] = T();
        }
        len = first;
    }
    void RemoveLast(Size count = 1)
    {
        RG_ASSERT(count >= 0 && count <= len);
        RemoveFrom(len - count);
    }

    Span<T> Take(Size offset, Size len) const
        { return Span<T>((T *)data, N).Take(offset, len); }
    Span<T> TakeAvailable() const
        { return Span<T>((T *)data + len, N - len); }
};

template <typename T>
class HeapArray {
    // StaticAssert(std::is_trivially_copyable<T>::value);

public:
    T *ptr = nullptr;
    Size len = 0;
    Size capacity = 0;
    Allocator *allocator = nullptr;

    typedef T value_type;
    typedef T *iterator_type;

    HeapArray() = default;
    HeapArray(Allocator *alloc, Size min_capacity = 0) : allocator(alloc)
        { SetCapacity(min_capacity); }
    HeapArray(Size min_capacity) { Reserve(min_capacity); }
    HeapArray(std::initializer_list<T> l)
    {
        Reserve(l.size());
        for (const T &val: l) {
            Append(val);
        }
    }
    ~HeapArray() { Clear(); }

    HeapArray(HeapArray &&other) { *this = std::move(other); }
    HeapArray &operator=(HeapArray &&other)
    {
        Clear();
        memmove(this, &other, RG_SIZE(other));
        memset(&other, 0, RG_SIZE(other));
        return *this;
    }
    HeapArray(const HeapArray &other) { *this = other; }
    HeapArray &operator=(const HeapArray &other)
    {
        RemoveFrom(0);
        Grow(other.capacity);
#if __cplusplus >= 201703L
        if constexpr(!std::is_trivial<T>::value) {
#else
        if (true) {
#endif
            for (Size i = 0; i < other.len; i++) {
                ptr[i] = other.ptr[i];
            }
        } else {
            memcpy(ptr, other.ptr, (size_t)(other.len * RG_SIZE(*ptr)));
        }
        len = other.len;
        return *this;
    }

    void Clear()
    {
        RemoveFrom(0);
        SetCapacity(0);
    }

    operator Span<T>() { return Span<T>(ptr, len); }
    operator Span<const T>() const { return Span<const T>(ptr, len); }

    T *begin() { return ptr; }
    const T *begin() const { return ptr; }
    T *end() { return ptr + len; }
    const T *end() const { return ptr + len; }

    Size Available() const { return capacity - len; }

    T &operator[](Size idx)
    {
        RG_ASSERT(idx >= 0 && idx < len);
        return ptr[idx];
    }
    const T &operator[](Size idx) const
    {
        RG_ASSERT(idx >= 0 && idx < len);
        return ptr[idx];
    }

    bool operator==(const HeapArray &other) const
    {
        if (len != other.len)
            return false;

        for (Size i = 0; i < len; i++) {
            if (ptr[i] != other.ptr[i])
                return false;
        }

        return true;
    }
    bool operator!=(const HeapArray &other) const { return !(*this == other); }

    void SetCapacity(Size new_capacity)
    {
        RG_ASSERT(new_capacity >= 0);

        if (new_capacity == capacity)
            return;

        if (len > new_capacity) {
            for (Size i = new_capacity; i < len; i++) {
                ptr[i].~T();
            }
            len = new_capacity;
        }
        Allocator::Resize(allocator, (void **)&ptr,
                          capacity * RG_SIZE(T), new_capacity * RG_SIZE(T));
        capacity = new_capacity;
    }

    void Reserve(Size min_capacity)
    {
        if (min_capacity <= capacity)
            return;

        SetCapacity(min_capacity);
    }

    void Grow(Size reserve_capacity = 1)
    {
        RG_ASSERT(capacity >= 0);
        RG_ASSERT(reserve_capacity >= 0);
        RG_ASSERT((size_t)capacity + (size_t)reserve_capacity <= RG_SIZE_MAX);

        if (reserve_capacity <= capacity - len)
            return;

        Size needed_capacity = capacity + reserve_capacity;

        Size new_capacity;
        if (!capacity) {
            new_capacity = RG_HEAPARRAY_BASE_CAPACITY;
        } else {
            new_capacity = capacity;
        }
        do {
            new_capacity = (Size)((double)new_capacity * RG_HEAPARRAY_GROWTH_FACTOR);
        } while (new_capacity < needed_capacity);

        SetCapacity(new_capacity);
    }

    void Trim() { SetCapacity(len); }

    T *AppendDefault(Size count = 1)
    {
        Grow(count);

        T *first = ptr + len;
#if __cplusplus >= 201703L
        if constexpr(!std::is_trivial<T>::value) {
#else
        if (true) {
#endif
            for (Size i = 0; i < count; i++) {
                new (ptr + len) T();
                len++;
            }
        } else {
            memset(first, 0, count * RG_SIZE(T));
            len += count;
        }
        return first;
    }

    T *Append(const T &value)
    {
        Grow();

        T *first = ptr + len;
#if __cplusplus >= 201703L
        if constexpr(!std::is_trivial<T>::value) {
#else
        if (true) {
#endif
            new (ptr + len) T;
        }
        ptr[len++] = value;
        return first;
    }
    T *Append(Span<const T> values)
    {
        Grow(values.len);

        T *first = ptr + len;
        for (const T &value: values) {
#if __cplusplus >= 201703L
            if constexpr(!std::is_trivial<T>::value) {
#else
            if (true) {
#endif
                new (ptr + len) T;
            }
            ptr[len++] = value;
        }
        return first;
    }

    void RemoveFrom(Size first)
    {
        RG_ASSERT(first >= 0 && first <= len);

#if __cplusplus >= 201703L
        if constexpr(!std::is_trivial<T>::value) {
#else
        if (true) {
#endif
            for (Size i = first; i < len; i++) {
                ptr[i].~T();
            }
        }
        len = first;
    }
    void RemoveLast(Size count = 1)
    {
        RG_ASSERT(count >= 0 && count <= len);
        RemoveFrom(len - count);
    }

    Span<T> Take(Size offset, Size len) const
        { return Span<T>(ptr, this->len).Take(offset, len); }

    Span<T> Leak()
    {
        Span<T> span = *this;

        ptr = nullptr;
        len = 0;
        capacity = 0;

        return span;
    }
    Span<T> TrimAndLeak()
    {
        Trim();
        return Leak();
    }
};

template <typename T, Size BucketSize = 1024>
class BlockQueue {
public:
    struct Bucket {
        T *values;
        LinkedAllocator allocator;
    };

    template <typename U>
    class Iterator {
    public:
        typedef std::bidirectional_iterator_tag iterator_category;
        typedef Size value_type;
        typedef Size difference_type;
        typedef Iterator *pointer;
        typedef Iterator &reference;

        U *queue = nullptr;
        Size bucket_idx;
        Size bucket_offset;
        Bucket *bucket;
        Bucket *next_bucket;

        Iterator() = default;
        Iterator(U *queue, Size bucket_idx, Size bucket_offset)
            : queue(queue), bucket_idx(bucket_idx), bucket_offset(bucket_offset),
              bucket(GetBucketSafe(bucket_idx)), next_bucket(GetBucketSafe(bucket_idx + 1)) {}

        T *operator->() { return &bucket->values[bucket_offset]; }
        const T *operator->() const { return &bucket->values[bucket_offset]; }
        T &operator*() { return bucket->values[bucket_offset]; }
        const T &operator*() const { return bucket->values[bucket_offset]; }

        Iterator &operator++()
        {
            if (++bucket_offset >= BucketSize) {
                bucket_idx++;
                if (next_bucket) {
                    // We support deletion of all values up to (and including) the current one.
                    // When the user does that, some or all front buckets may be gone, but we can
                    // use next_bucket to fix bucket_idx.
                    while (bucket_idx >= queue->buckets.len ||
                           queue->buckets[bucket_idx] != next_bucket) {
                        bucket_idx--;
                    }
                }
                bucket_offset = 0;

                bucket = GetBucketSafe(bucket_idx);
                next_bucket = GetBucketSafe(bucket_idx + 1);
            }

            return *this;
        }
        Iterator operator++(int)
        {
            Iterator ret = *this;
            ++(*this);
            return ret;
        }

        Iterator &operator--()
        {
            if (--bucket_offset < 0) {
                RG_ASSERT(bucket_idx > 0);

                bucket_idx--;
                bucket_offset = BucketSize - 1;

                bucket = GetBucketSafe(bucket_idx);
                next_bucket = GetBucketSafe(bucket_idx + 1);
            }

            return *this;
        }
        Iterator operator--(int)
        {
            Iterator ret = *this;
            --(*this);
            return ret;
        }

        bool operator==(const Iterator &other) const
            { return queue == other.queue && bucket == other.bucket &&
                     bucket_offset == other.bucket_offset; }
        bool operator!=(const Iterator &other) const { return !(*this == other); }

    private:
        Bucket *GetBucketSafe(Size idx)
            { return idx < queue->buckets.len ? queue->buckets[idx] : nullptr; }
    };

    HeapArray<Bucket *> buckets;
    Size offset = 0;
    Size len = 0;
    Allocator *bucket_allocator;

    typedef T value_type;
    typedef Iterator<BlockQueue> iterator_type;

    BlockQueue()
    {
        Bucket *first_bucket = *buckets.Append(CreateBucket());
        bucket_allocator = &first_bucket->allocator;
    }
    ~BlockQueue() { ClearBucketsAndValues(); }

    BlockQueue(BlockQueue &&other) { *this = std::move(other); }
    BlockQueue &operator=(BlockQueue &&other)
    {
        ClearBucketsAndValues();
        memmove(this, &other, RG_SIZE(other));
        memset(&other, 0, RG_SIZE(other));
        return *this;
    }
    BlockQueue(BlockQueue &) = delete;
    BlockQueue &operator=(const BlockQueue &) = delete;

    void Clear()
    {
        ClearBucketsAndValues();

        Bucket *first_bucket = *buckets.Append(CreateBucket());
        offset = 0;
        len = 0;
        bucket_allocator = &first_bucket->allocator;
    }

    iterator_type begin() { return iterator_type(this, 0, offset); }
    Iterator<const BlockQueue<T, BucketSize>> begin() const { return iterator_type(this, 0, offset); }
    iterator_type end()
    {
        Size end_idx = offset + len;
        Size bucket_idx = end_idx / BucketSize;
        Size bucket_offset = end_idx % BucketSize;

        return iterator_type(this, bucket_idx, bucket_offset);
    }
    Iterator<const BlockQueue<T, BucketSize>> end() const
    {
        Size end_idx = offset + len;
        Size bucket_idx = end_idx / BucketSize;
        Size bucket_offset = end_idx % BucketSize;

        return iterator_type(this, bucket_idx, bucket_offset);
    }

    const T &operator[](Size idx) const
    {
        RG_ASSERT(idx >= 0 && idx < len);

        idx += offset;
        Size bucket_idx = idx / BucketSize;
        Size bucket_offset = idx % BucketSize;

        return buckets[bucket_idx]->values[bucket_offset];
    }
    T &operator[](Size idx) { return (T &)(*(const BlockQueue *)this)[idx]; }

    T *AppendDefault()
    {
        Size bucket_idx = (offset + len) / BucketSize;
        Size bucket_offset = (offset + len) % BucketSize;

        T *first = buckets[bucket_idx]->values + bucket_offset;
        new (first) T();

        len++;
        if (bucket_offset == BucketSize - 1) {
            Bucket *new_bucket = *buckets.Append(CreateBucket());
            bucket_allocator = &new_bucket->allocator;
        }

        return first;
    }

    T *Append(const T &value)
    {
        T *it = AppendDefault();
        *it = value;
        return it;
    }

    void RemoveFrom(Size from)
    {
        RG_ASSERT(from >= 0 && from <= len);

        if (from == len)
            return;
        if (!from) {
            Clear();
            return;
        }

        Size start_idx = offset + from;
        Size end_idx = offset + len;
        Size start_bucket_idx = start_idx / BucketSize;
        Size start_bucket_offset = start_idx % BucketSize;
        Size end_bucket_idx = end_idx / BucketSize;

        iterator_type from_it(this, start_bucket_idx, start_bucket_offset);
        DeleteValues(from_it, end());

        for (Size i = start_bucket_idx + 1; i <= end_bucket_idx; i++) {
            DeleteBucket(buckets[i]);
        }
        buckets.RemoveFrom(start_bucket_idx + 1);
        if (start_idx % BucketSize == 0) {
            DeleteBucket(buckets[buckets.len - 1]);
            buckets[buckets.len - 1] = CreateBucket();
        }

        len = from;
        bucket_allocator = &buckets[buckets.len - 1]->allocator;
    }
    void RemoveLast(Size count = 1)
    {
        RG_ASSERT(count >= 0 && count <= len);
        RemoveFrom(len - count);
    }

    void RemoveFirst(Size count = 1)
    {
        RG_ASSERT(count >= 0 && count <= len);

        if (count == len) {
            Clear();
            return;
        }

        Size end_idx = offset + count;
        Size end_bucket_idx = end_idx / BucketSize;
        Size end_bucket_offset = end_idx % BucketSize;

        iterator_type until_it(this, end_bucket_idx, end_bucket_offset);
        DeleteValues(begin(), until_it);

        if (end_bucket_idx) {
            for (Size i = 0; i < end_bucket_idx; i++) {
                DeleteBucket(buckets[i]);
            }
            memmove(&buckets[0], &buckets[end_bucket_idx],
                    (size_t)((buckets.len - end_bucket_idx) * RG_SIZE(Bucket *)));
            buckets.RemoveLast(end_bucket_idx);
        }

        offset = (offset + count) % BucketSize;
        len -= count;
    }

private:
    void ClearBucketsAndValues()
    {
        DeleteValues(begin(), end());

        for (Bucket *bucket: buckets) {
            DeleteBucket(bucket);
        }
        buckets.Clear();
    }

    void DeleteValues(iterator_type begin RG_MAYBE_UNUSED, iterator_type end RG_MAYBE_UNUSED)
    {
#if __cplusplus >= 201703L
        if constexpr(!std::is_trivial<T>::value) {
#else
        if (true) {
#endif
            for (iterator_type it = begin; it != end; ++it) {
                it->~T();
            }
        }
    }

    Bucket *CreateBucket()
    {
        Bucket *new_bucket = (Bucket *)Allocator::Allocate(buckets.allocator, RG_SIZE(Bucket));
        new (&new_bucket->allocator) LinkedAllocator;
        new_bucket->values = (T *)Allocator::Allocate(&new_bucket->allocator, BucketSize * RG_SIZE(T));
        return new_bucket;
    }

    void DeleteBucket(Bucket *bucket)
    {
        bucket->allocator.~LinkedAllocator();
        Allocator::Release(buckets.allocator, bucket, RG_SIZE(Bucket));
    }
};

template <Size N>
class Bitset {
public:
    template <typename T>
    class Iterator {
    public:
        typedef std::input_iterator_tag iterator_category;
        typedef Size value_type;
        typedef Size difference_type;
        typedef Iterator *pointer;
        typedef Iterator &reference;

        T *bitset = nullptr;
        Size offset;
        size_t bits = 0;
        int ctz;

        Iterator() = default;
        Iterator(T *bitset, Size offset)
            : bitset(bitset), offset(offset - 1)
        {
            operator++();
        }

        Size operator*() const
        {
            RG_ASSERT(offset <= RG_LEN(bitset->data));

            if (offset == RG_LEN(bitset->data))
                return -1;
            return offset * RG_SIZE(size_t) * 8 + ctz;
        }

        Iterator &operator++()
        {
            RG_ASSERT(offset <= RG_LEN(bitset->data));

            while (!bits) {
                if (offset == RG_LEN(bitset->data) - 1)
                    return *this;
                bits = bitset->data[++offset];
            }

            ctz = CountTrailingZeros((uint64_t)bits);
            bits ^= (size_t)1 << ctz;

            return *this;
        }

        Iterator operator++(int)
        {
            Iterator ret = *this;
            ++(*this);
            return ret;
        }

        bool operator==(const Iterator &other) const
            { return bitset == other.bitset && offset == other.offset; }
        bool operator!=(const Iterator &other) const { return !(*this == other); }
    };

    typedef Size value_type;
    typedef Iterator<Bitset> iterator_type;

    static constexpr Size Bits = N;
    size_t data[(N + RG_SIZE(size_t) - 1) / RG_SIZE(size_t)] = {};

    void Clear()
    {
        memset(data, 0, RG_SIZE(data));
    }

    Iterator<Bitset> begin() { return Iterator<Bitset>(this, 0); }
    Iterator<const Bitset> begin() const { return Iterator<const Bitset>(this, 0); }
    Iterator<Bitset> end() { return Iterator<Bitset>(this, RG_LEN(data)); }
    Iterator<const Bitset> end() const { return Iterator<const Bitset>(this, RG_LEN(data)); }

    Size PopCount() const
    {
        Size count = 0;
        for (size_t bits: data) {
            count += PopCount(bits);
        }
        return count;
    }

    inline bool Test(Size idx) const
    {
        RG_ASSERT(idx >= 0 && idx < N);

        Size offset = idx / (RG_SIZE(size_t) * 8);
        size_t mask = (size_t)1 << (idx % (RG_SIZE(size_t) * 8));

        return data[offset] & mask;
    }
    inline void Set(Size idx, bool value = true)
    {
        RG_ASSERT(idx >= 0 && idx < N);

        Size offset = idx / (RG_SIZE(size_t) * 8);
        size_t mask = (size_t)1 << (idx % (RG_SIZE(size_t) * 8));

        data[offset] = ApplyMask(data[offset], mask, value);
    }
    inline bool TestAndSet(Size idx, bool value = true)
    {
        RG_ASSERT(idx >= 0 && idx < N);

        Size offset = idx / (RG_SIZE(size_t) * 8);
        size_t mask = (size_t)1 << (idx % (RG_SIZE(size_t) * 8));

        bool ret = data[offset] & mask;
        data[offset] = ApplyMask(data[offset], mask, value);

        return ret;
    }

    Bitset &operator&=(const Bitset &other)
    {
        for (Size i = 0; i < RG_LEN(data); i++) {
            data[i] &= other.data[i];
        }
        return *this;
    }
    Bitset operator&(const Bitset &other)
    {
        Bitset ret;
        for (Size i = 0; i < RG_LEN(data); i++) {
            ret.data[i] = data[i] & other.data[i];
        }
        return ret;
    }

    Bitset &operator|=(const Bitset &other)
    {
        for (Size i = 0; i < RG_LEN(data); i++) {
            data[i] |= other.data[i];
        }
        return *this;
    }
    Bitset operator|(const Bitset &other)
    {
        Bitset ret;
        for (Size i = 0; i < RG_LEN(data); i++) {
            ret.data[i] = data[i] | other.data[i];
        }
        return ret;
    }

    Bitset &operator^=(const Bitset &other)
    {
        for (Size i = 0; i < RG_LEN(data); i++) {
            data[i] ^= other.data[i];
        }
        return *this;
    }
    Bitset operator^(const Bitset &other)
    {
        Bitset ret;
        for (Size i = 0; i < RG_LEN(data); i++) {
            ret.data[i] = data[i] ^ other.data[i];
        }
        return ret;
    }

    Bitset &Flip()
    {
        for (Size i = 0; i < RG_LEN(data); i++) {
            data[i] = ~data[i];
        }
        return *this;
    }
    Bitset operator~()
    {
        Bitset ret;
        for (Size i = 0; i < RG_LEN(data); i++) {
            ret.data[i] = ~data[i];
        }
        return ret;
    }

    // TODO: Shift operators
};

template <typename KeyType, typename ValueType,
          typename Handler = typename std::remove_pointer<ValueType>::type::HashHandler>
class HashTable {
public:
    template <typename T>
    class Iterator {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef ValueType value_type;
        typedef Size difference_type;
        typedef Iterator *pointer;
        typedef Iterator &reference;

        T *table = nullptr;
        Size offset;

        Iterator() = default;
        Iterator(T *table, Size offset)
            : table(table), offset(offset - 1) { operator++(); }

        ValueType &operator*()
        {
            RG_ASSERT(!table->IsEmpty(offset));
            return table->data[offset];
        }
        const ValueType &operator*() const
        {
            RG_ASSERT(!table->IsEmpty(offset));
            return table->data[offset];
        }

        Iterator &operator++()
        {
            RG_ASSERT(offset < table->capacity);
            while (++offset < table->capacity && table->IsEmpty(offset));
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator ret = *this;
            ++(*this);
            return ret;
        }

        // Beware, in some cases a previous value may be seen again after this action
        void Remove()
        {
            table->Remove(&table->data[offset]);
            offset--;
        }

        bool operator==(const Iterator &other) const
            { return table == other.table && offset == other.offset; }
        bool operator!=(const Iterator &other) const { return !(*this == other); }
    };

    typedef Size value_type;
    typedef Iterator<HashTable> iterator_type;

    size_t *used = nullptr;
    ValueType *data = nullptr;
    Size count = 0;
    Size capacity = 0;
    Allocator *allocator = nullptr;

    HashTable() = default;
    ~HashTable()
    {
#if __cplusplus >= 201703L
        if constexpr(std::is_trivial<ValueType>::value) {
#else
        if (false) {
#endif
            count = 0;
            Rehash(0);
        } else {
            Clear();
        }
    }

    HashTable(HashTable &&other) { *this = std::move(other); }
    HashTable &operator=(HashTable &&other)
    {
        Clear();
        memmove(this, &other, RG_SIZE(other));
        memset(&other, 0, RG_SIZE(other));
        return *this;
    }
    HashTable(const HashTable &other) { *this = other; }
    HashTable &operator=(const HashTable &other)
    {
        Clear();
        for (const ValueType &value: other) {
            Append(value);
        }
        return *this;
    }

    void Clear()
    {
        for (Size i = 0; i < capacity; i++) {
            if (!IsEmpty(i)) {
                data[i].~ValueType();
            }
        }

        count = 0;
        Rehash(0);
    }

    void RemoveAll()
    {
#if __cplusplus >= 201703L
        RG_STATIC_ASSERT(!std::is_pointer<ValueType>::value);
#endif

        for (Size i = 0; i < capacity; i++) {
            if (!IsEmpty(i)) {
                data[i].~ValueType();
            }
        }

        count = 0;
        if (used) {
            memset(used, 0, (size_t)(capacity + (RG_SIZE(size_t) * 8) - 1) / RG_SIZE(size_t));
        }
    }

    Iterator<HashTable> begin() { return Iterator<HashTable>(this, 0); }
    Iterator<const HashTable> begin() const { return Iterator<const HashTable>(this, 0); }
    Iterator<HashTable> end() { return Iterator<HashTable>(this, capacity); }
    Iterator<const HashTable> end() const { return Iterator<const HashTable>(this, capacity); }

    ValueType *Find(const KeyType &key)
        { return (ValueType *)((const HashTable *)this)->Find(key); }
    const ValueType *Find(const KeyType &key) const
    {
        if (!capacity)
            return nullptr;

        uint64_t hash = Handler::HashKey(key);
        Size idx = HashToIndex(hash);
        return Find(&idx, key);
    }
    ValueType FindValue(const KeyType &key, const ValueType &default_value)
        { return (ValueType)((const HashTable *)this)->FindValue(key, default_value); }
    const ValueType FindValue(const KeyType &key, const ValueType &default_value) const
    {
        const ValueType *it = Find(key);
        return it ? *it : default_value;
    }

    std::pair<ValueType *, bool> Append(const ValueType &value)
    {
        const KeyType &key = Handler::GetKey(value);
        std::pair<ValueType *, bool> ret = Insert(key);
        if (ret.second) {
            *ret.first = value;
        }
        return ret;
    }
    std::pair<ValueType *, bool> AppendDefault(const KeyType &key)
    {
        std::pair<ValueType *, bool> ret = Insert(key);
        if (ret.second) {
            new (ret.first) ValueType();
        }
        return ret;
    }

    ValueType *Set(const ValueType &value)
    {
        const KeyType &key = Handler::GetKey(value);
        ValueType *it = Insert(key).first;
        *it = value;
        return it;
    }
    ValueType *SetDefault(const KeyType &key)
    {
        ValueType *it = Insert(key);
        new (it) ValueType();
        return it;
    }

    void Remove(ValueType *it)
    {
        if (!it)
            return;

        Size empty_idx = it - data;
        RG_ASSERT(!IsEmpty(empty_idx));

        it->~ValueType();
        count--;

        // Move following slots if needed
        {
            Size idx = (empty_idx + 1) & (capacity - 1);

            while (!IsEmpty(idx)) {
                Size real_idx = KeyToIndex(Handler::GetKey(data[idx]));

                if (TestNewSlot(real_idx, empty_idx)) {
                    memmove(&data[empty_idx], &data[idx], RG_SIZE(*data));
                    empty_idx = idx;
                }

                idx = (idx + 1) & (capacity - 1);
            }
        }

        MarkEmpty(empty_idx);
        new (&data[empty_idx]) ValueType();
    }
    void Remove(const KeyType &key) { Remove(Find(key)); }

    Size IsEmpty(Size idx) const { return IsEmpty(used, idx); }

private:
    ValueType *Find(Size *idx, const KeyType &key)
        { return (ValueType *)((const HashTable *)this)->Find(idx, key); }
    const ValueType *Find(Size *idx, const KeyType &key) const
    {
#if __cplusplus >= 201703L
        if constexpr(std::is_pointer<ValueType>::value) {
            while (data[*idx]) {
                const KeyType &it_key = Handler::GetKey(data[*idx]);
                if (Handler::TestKeys(it_key, key))
                    return &data[*idx];
                *idx = (*idx + 1) & (capacity - 1);
            }
            return nullptr;
        }
#endif

        while (!IsEmpty(*idx)) {
            const KeyType &it_key = Handler::GetKey(data[*idx]);
            if (Handler::TestKeys(it_key, key))
                return &data[*idx];
            *idx = (*idx + 1) & (capacity - 1);
        }
        return nullptr;
    }

    std::pair<ValueType *, bool> Insert(const KeyType &key)
    {
        uint64_t hash = Handler::HashKey(key);

        if (capacity) {
            Size idx = HashToIndex(hash);
            ValueType *it = Find(&idx, key);
            if (!it) {
                if (count >= (Size)((double)capacity * RG_HASHTABLE_MAX_LOAD_FACTOR)) {
                    Rehash(capacity << 1);
                    idx = HashToIndex(hash);
                    while (!IsEmpty(idx)) {
                        idx = (idx + 1) & (capacity - 1);
                    }
                }
                count++;
                MarkUsed(idx);
                return {&data[idx], true};
            } else {
                return {it, false};
            }
        } else {
            Rehash(RG_HASHTABLE_BASE_CAPACITY);

            Size idx = HashToIndex(hash);
            count++;
            MarkUsed(idx);
            return {&data[idx], true};
        }
    }

    void Rehash(Size new_capacity)
    {
        if (new_capacity == capacity)
            return;
        RG_ASSERT(count <= new_capacity);

        size_t *old_used = used;
        ValueType *old_data = data;
        Size old_capacity = capacity;

        if (new_capacity) {
            used = (size_t *)Allocator::Allocate(allocator,
                                                 (new_capacity + (RG_SIZE(size_t) * 8) - 1) / RG_SIZE(size_t),
                                                 (int)Allocator::Flag::Zero);
            data = (ValueType *)Allocator::Allocate(allocator, new_capacity * RG_SIZE(ValueType));
            for (Size i = 0; i < new_capacity; i++) {
                new (&data[i]) ValueType();
            }
            capacity = new_capacity;

            for (Size i = 0; i < old_capacity; i++) {
                if (!IsEmpty(old_used, i)) {
                    Size new_idx = KeyToIndex(Handler::GetKey(old_data[i]));
                    while (!IsEmpty(new_idx)) {
                        new_idx = (new_idx + 1) & (capacity - 1);
                    }
                    MarkUsed(new_idx);
                    memmove(&data[new_idx], &old_data[i], RG_SIZE(*data));
                }
            }
        } else {
            used = nullptr;
            data = nullptr;
            capacity = 0;
        }

        Allocator::Release(allocator, old_used,
                           (old_capacity + (RG_SIZE(size_t) * 8) - 1) / RG_SIZE(size_t));
        Allocator::Release(allocator, old_data, old_capacity * RG_SIZE(ValueType));
    }

    void MarkUsed(Size idx)
    {
        used[idx / (RG_SIZE(size_t) * 8)] |= (1ull << (idx % (RG_SIZE(size_t) * 8)));
    }
    void MarkEmpty(Size idx)
    {
        used[idx / (RG_SIZE(size_t) * 8)] &= ~(1ull << (idx % (RG_SIZE(size_t) * 8)));
    }
    Size IsEmpty(size_t *used, Size idx) const
    {
        return !(used[idx / (RG_SIZE(size_t) * 8)] & (1ull << (idx % (RG_SIZE(size_t) * 8))));
    }

    Size HashToIndex(uint64_t hash) const
    {
        return (Size)(hash & (uint64_t)(capacity - 1));
    }
    Size KeyToIndex(const KeyType &key) const
    {
        uint64_t hash = Handler::HashKey(key);
        return HashToIndex(hash);
    }

    // FIXME: Testing all slots each time (see Remove()) is slow and stupid
    bool TestNewSlot(Size idx, Size dest_idx) const
    {
        for (;;) {
            if (idx == dest_idx) {
                return true;
            } else if (IsEmpty(idx)) {
                return false;
            }

            idx = (idx + 1) & (capacity - 1);
        }
    }
};

template <typename T>
class HashTraits {
public:
    static uint64_t Hash(const T &key) { return key.Hash(); }
    static bool Test(const T &key1, const T &key2) { return key1 == key2; }
};

// Stole the Hash function from Thomas Wang (see here: https://gist.github.com/badboy/6267743)
#define DEFINE_INTEGER_HASH_TRAITS_32(Type) \
    template <> \
    class HashTraits<Type> { \
    public: \
        static uint64_t Hash(Type key) \
        { \
            uint32_t hash = (uint32_t)key; \
             \
            hash = (hash ^ 61) ^ (hash >> 16); \
            hash += hash << 3; \
            hash ^= hash >> 4; \
            hash *= 0x27D4EB2D; \
            hash ^= hash >> 15; \
             \
            return (uint64_t)hash; \
        } \
         \
        static bool Test(Type key1, Type key2) { return key1 == key2; } \
    }
#define DEFINE_INTEGER_HASH_TRAITS_64(Type) \
    template <> \
    class HashTraits<Type> { \
    public: \
        static uint64_t Hash(Type key) \
        { \
            uint64_t hash = (uint64_t)key; \
             \
            hash = (~hash) + (hash << 18); \
            hash ^= hash >> 31; \
            hash *= 21; \
            hash ^= hash >> 11; \
            hash += hash << 6; \
            hash ^= hash >> 22; \
             \
            return hash; \
        } \
         \
        static bool Test(Type key1, Type key2) { return key1 == key2; } \
    }

DEFINE_INTEGER_HASH_TRAITS_32(char);
DEFINE_INTEGER_HASH_TRAITS_32(unsigned char);
DEFINE_INTEGER_HASH_TRAITS_32(short);
DEFINE_INTEGER_HASH_TRAITS_32(unsigned short);
DEFINE_INTEGER_HASH_TRAITS_32(int);
DEFINE_INTEGER_HASH_TRAITS_32(unsigned int);
#ifdef __LP64__
    DEFINE_INTEGER_HASH_TRAITS_64(long);
    DEFINE_INTEGER_HASH_TRAITS_64(unsigned long);
#else
    DEFINE_INTEGER_HASH_TRAITS_32(long);
    DEFINE_INTEGER_HASH_TRAITS_32(unsigned long);
#endif
DEFINE_INTEGER_HASH_TRAITS_64(long long);
DEFINE_INTEGER_HASH_TRAITS_64(unsigned long long);
#ifdef RG_ARCH_64
    DEFINE_INTEGER_HASH_TRAITS_64(void *);
    DEFINE_INTEGER_HASH_TRAITS_64(const void *);
#else
    DEFINE_INTEGER_HASH_TRAITS_32(void *);
    DEFINE_INTEGER_HASH_TRAITS_32(const void *);
#endif

#undef DEFINE_INTEGER_HASH_TRAITS_32
#undef DEFINE_INTEGER_HASH_TRAITS_64

template <>
class HashTraits<const char *> {
public:
    // FNV-1a
    static uint64_t Hash(const char *key)
    {
        uint64_t hash = 0xCBF29CE484222325ull;
        for (Size i = 0; key[i]; i++) {
            hash ^= (uint64_t)key[i];
            hash *= 0x100000001B3ull;
        }

        return hash;
    }

    static bool Test(const char *key1, const char *key2) { return !strcmp(key1, key2); }
};

template <>
class HashTraits<Span<const char>> {
public:
    // FNV-1a
    static uint64_t Hash(Span<const char> key)
    {
        uint64_t hash = 0xCBF29CE484222325ull;
        for (char c: key) {
            hash ^= (uint64_t)c;
            hash *= 0x100000001B3ull;
        }

        return hash;
    }

    static bool Test(Span<const char> key1, Span<const char> key2) { return key1 == key2; }
    static bool Test(Span<const char> key1, const char * key2) { return key1 == key2; }
};

#define RG_HASH_TABLE_HANDLER_EX_N(Name, ValueType, KeyType, KeyMember, HashFunc, TestFunc) \
    class Name { \
    public: \
        static KeyType GetKey(const ValueType &value) \
            { return (KeyType)(value.KeyMember); } \
        static KeyType GetKey(const ValueType *value) \
            { return (KeyType)(value->KeyMember); } \
        static uint64_t HashKey(KeyType key) \
            { return HashFunc(key); } \
        static bool TestKeys(KeyType key1, KeyType key2) \
            { return TestFunc((key1), (key2)); } \
    }
#define RG_HASH_TABLE_HANDLER_EX(ValueType, KeyType, KeyMember, HashFunc, TestFunc) \
    RG_HASH_TABLE_HANDLER_EX_N(HashHandler, ValueType, KeyType, KeyMember, HashFunc, TestFunc)
#define RG_HASH_TABLE_HANDLER(ValueType, KeyMember) \
    RG_HASH_TABLE_HANDLER_EX(ValueType, decltype(ValueType::KeyMember), KeyMember, HashTraits<decltype(ValueType::KeyMember)>::Hash, HashTraits<decltype(ValueType::KeyMember)>::Test)
#define RG_HASH_TABLE_HANDLER_N(Name, ValueType, KeyMember) \
    RG_HASH_TABLE_HANDLER_EX_N(Name, ValueType, decltype(ValueType::KeyMember), KeyMember, HashTraits<decltype(ValueType::KeyMember)>::Hash, HashTraits<decltype(ValueType::KeyMember)>::Test)
#define RG_HASH_TABLE_HANDLER_T(ValueType, KeyType, KeyMember) \
    RG_HASH_TABLE_HANDLER_EX(ValueType, KeyType, KeyMember, HashTraits<KeyType>::Hash, HashTraits<KeyType>::Test)
#define RG_HASH_TABLE_HANDLER_NT(Name, ValueType, KeyType, KeyMember) \
    RG_HASH_TABLE_HANDLER_EX_N(Name, ValueType, KeyType, KeyMember, HashTraits<KeyType>::Hash, HashTraits<KeyType>::Test)

template <typename KeyType, typename ValueType>
class HashMap {
public:
    struct Bucket {
        KeyType key;
        ValueType value;

        RG_HASH_TABLE_HANDLER(Bucket, key);
    };

    HashTable<KeyType, Bucket> table;

    void Clear() { table.Clear(); }
    void RemoveAll() { table.RemoveAll(); }

    std::pair<ValueType *, bool> Append(const KeyType &key, const ValueType &value)
    {
        std::pair<Bucket *, bool> ret = table.Append({key, value});
        return { &ret.first->value, ret.second };
    }
    std::pair<ValueType *, bool> AppendDefault(const KeyType &key)
    {
        std::pair<Bucket *, bool> ret = table.AppendDefault(key);
        ret.first->key = key;
        return { &ret.first->value, ret.second };
    }

    ValueType *Set(const KeyType &key, const ValueType &value)
        { return &table.Set({key, value})->value; }
    ValueType *SetDefault(const KeyType &key)
    {
        Bucket *table_it = table.SetDefault(key);
        table_it->key = key;
        return &table_it->value;
    }

    void Remove(ValueType *it)
    {
        if (!it)
            return;
        table.Remove((Bucket *)((uint8_t *)it - RG_OFFSET_OF(Bucket, value)));
    }
    void Remove(const KeyType &key) { Remove(Find(key)); }

    ValueType *Find(const KeyType &key)
        { return (ValueType *)((const HashMap *)this)->Find(key); }
    const ValueType *Find(const KeyType &key) const
    {
        const Bucket *table_it = table.Find(key);
        return table_it ? &table_it->value : nullptr;
    }
    ValueType FindValue(const KeyType &key, const ValueType &default_value)
        { return (ValueType)((const HashMap *)this)->FindValue(key, default_value); }
    const ValueType FindValue(const KeyType &key, const ValueType &default_value) const
    {
        const ValueType *it = Find(key);
        return it ? *it : default_value;
    }
};

template <typename ValueType>
class HashSet {
    class Handler {
    public:
        static ValueType GetKey(const ValueType &value) { return value; }
        static ValueType GetKey(const ValueType *value) { return *value; }
        static uint64_t HashKey(const ValueType &value)
            { return HashTraits<ValueType>::Hash(value); }
        static bool TestKeys(const ValueType &value1, const ValueType &value2)
            { return HashTraits<ValueType>::Test(value1, value2); }
    };

public:
    HashTable<ValueType, ValueType, Handler> table;

    void Clear() { table.Clear(); }
    void RemoveAll() { table.RemoveAll(); }

    std::pair<ValueType *, bool> Append(const ValueType &value) { return table.Append(value); }
    ValueType *Set(const ValueType &value) { return table.Set(value); }

    void Remove(ValueType *it) { table.Remove(it); }
    void Remove(const ValueType &value) { Remove(Find(value)); }

    ValueType *Find(const ValueType &value) { return table.Find(value); }
    const ValueType *Find(const ValueType &value) const { return table.Find(value); }
    ValueType FindValue(const ValueType &value, const ValueType &default_value)
        { return table.FindValue(value, default_value); }
    const ValueType FindValue(const ValueType &value, const ValueType &default_value) const
        { return table.FindValue(value, default_value); }
};

// ------------------------------------------------------------------------
// Date
// ------------------------------------------------------------------------

union Date {
    int32_t value;
    struct {
#ifdef RG_ARCH_LITTLE_ENDIAN
        int8_t day;
        int8_t month;
        int16_t year;
#else
        int16_t year;
        int8_t month;
        int8_t day;
#endif
    } st;

    Date() = default;
    Date(int16_t year, int8_t month, int8_t day)
#ifdef RG_ARCH_LITTLE_ENDIAN
        : st({day, month, year}) { RG_ASSERT(IsValid()); }
#else
        : st({year, month, day}) { RG_ASSERT(IsValid()); }
#endif

    static inline bool IsLeapYear(int16_t year)
    {
        return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    }
    static inline int8_t DaysInMonth(int16_t year, int8_t month)
    {
        static const int8_t DaysPerMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        return (int8_t)(DaysPerMonth[month - 1] + (month == 2 && IsLeapYear(year)));
    }

    static Date FromString(Span<const char> date_str, int flags = RG_DEFAULT_PARSE_FLAGS,
                           Span<const char> *out_remaining = nullptr);
    static Date FromJulianDays(int days);
    static Date FromCalendarDate(int days) { return Date::FromJulianDays(days + 2440588); }

    bool IsValid() const
    {
        if (st.year < -4712)
            return false;
        if (st.month < 1 || st.month > 12)
            return false;
        if (st.day < 1 || st.day > DaysInMonth(st.year, st.month))
            return false;

        return true;
    }

    bool operator==(Date other) const { return value == other.value; }
    bool operator!=(Date other) const { return value != other.value; }
    bool operator>(Date other) const { return value > other.value; }
    bool operator>=(Date other) const { return value >= other.value; }
    bool operator<(Date other) const { return value < other.value; }
    bool operator<=(Date other) const { return value <= other.value; }

    int ToJulianDays() const;
    int ToCalendarDate() const { return ToJulianDays() - 2440588; }

    int GetWeekDay() const;

    int operator-(Date other) const
        { return ToJulianDays() - other.ToJulianDays(); }

    Date operator+(int days) const
    {
        if (days < 5 && days > -5) {
            Date date = *this;
            if (days > 0) {
                while (days--) {
                    ++date;
                }
            } else {
                while (days++) {
                    --date;
                }
            }
            return date;
        } else {
            return Date::FromJulianDays(ToJulianDays() + days);
        }
    }
    // That'll fail with INT_MAX days but that's far more days than can
    // be represented as a date anyway
    Date operator-(int days) const { return *this + (-days); }

    Date &operator+=(int days) { *this = *this + days; return *this; }
    Date &operator-=(int days) { *this = *this - days; return *this; }
    Date &operator++();
    Date operator++(int) { Date date = *this; ++(*this); return date; }
    Date &operator--();
    Date operator--(int) { Date date = *this; --(*this); return date; }

    uint64_t Hash() const { return HashTraits<int32_t>::Hash(value); }
};

// ------------------------------------------------------------------------
// Time
// ------------------------------------------------------------------------

int64_t GetMonotonicTime();

#if defined(_MSC_VER)
static inline int64_t GetClockCounter()
{
    return (int64_t)__rdtsc();
}
#elif defined(__i386__) || defined(__x86_64__)
static inline int64_t GetClockCounter()
{
    uint32_t counter_low, counter_high;
    __asm__ __volatile__ ("cpuid; rdtsc"
                          : "=a" (counter_low), "=d" (counter_high)
                          : : "%ebx", "%ecx");
    int64_t counter = ((int64_t)counter_high << 32) | counter_low;
    return counter;
}
#endif

// ------------------------------------------------------------------------
// Streams
// ------------------------------------------------------------------------

enum class CompressionType {
    None,
    Zlib,
    Gzip
};
static const char *const CompressionTypeNames[] = {
    "None",
    "Zlib",
    "Gzip"
};

class StreamReader {
    enum class SourceType {
        Memory,
        File,
        Function
    };

    // NOTE: Should we use DuplicateString() for this? Same question in StreamWriter.
    const char *filename;

    struct {
        SourceType type = SourceType::Memory;
        union U {
            struct {
                Span<const uint8_t> buf;
                Size pos;
            } memory;
            struct {
                FILE *fp;
                bool owned;
            } file;
            std::function<Size(Span<uint8_t> buf)> func;

            // StreamReader deals with func destructor
            U() {}
            ~U() {}
        } u;

        bool eof;
    } source;

    struct {
        CompressionType type = CompressionType::None;
        union {
            struct MinizInflateContext *miniz;
        } u;
    } compression;

    Size raw_len;
    Size read;
    Size raw_read;
    bool eof;
    bool error;

public:
    StreamReader() { Close(); }
    StreamReader(Span<const uint8_t> buf, const char *filename = nullptr,
                 CompressionType compression_type = CompressionType::None)
        : StreamReader() { Open(buf, filename, compression_type); }
    StreamReader(FILE *fp, const char *filename,
                 CompressionType compression_type = CompressionType::None)
        : StreamReader() { Open(fp, filename, compression_type); }
    StreamReader(const char *filename,
                 CompressionType compression_type = CompressionType::None)
        : StreamReader() { Open(filename, compression_type); }
    StreamReader(const std::function<Size(Span<uint8_t>)> &func, const char *filename = nullptr,
                 CompressionType compression_type = CompressionType::None)
        : StreamReader() { Open(func, filename, compression_type); }
    ~StreamReader() { Close(); }

    StreamReader(const StreamReader &other) = delete;
    StreamReader &operator=(const StreamReader &other) = delete;

    bool Open(Span<const uint8_t> buf, const char *filename = nullptr,
              CompressionType compression_type = CompressionType::None);
    bool Open(FILE *fp, const char *filename,
              CompressionType compression_type = CompressionType::None);
    bool Open(const char *filename, CompressionType compression_type = CompressionType::None);
    bool Open(const std::function<Size(Span<uint8_t>)> &func, const char *filename = nullptr,
              CompressionType compression_type = CompressionType::None);
    void Close();

    const char *GetFileName() const { return filename; }
    CompressionType GetCompressionType() const { return compression.type; }
    bool IsValid() const { return filename && !error; }
    bool IsEOF() const { return eof; }

    Size Read(Span<uint8_t> out_buf);
    Size Read(Span<char> out_buf) { return Read(out_buf.CastAs<uint8_t>()); }
    Size Read(Size max_len, void *out_buf) { return Read(MakeSpan((uint8_t *)out_buf, max_len)); }

    Size ReadAll(Size max_len, HeapArray<uint8_t> *out_buf);
    Size ReadAll(Size max_len, HeapArray<char> *out_buf)
        { return ReadAll(max_len, (HeapArray<uint8_t> *)out_buf); }

    Size ComputeStreamLen();
private:
    bool InitDecompressor(CompressionType type);
    void ReleaseResources();

    Size Deflate(Size max_len, void *out_buf);

    Size ReadRaw(Size max_len, void *out_buf);
};

static inline Size ReadFile(const char *filename, Size max_len, CompressionType compression_type,
                            HeapArray<uint8_t> *out_buf)
{
    StreamReader st(filename, compression_type);
    return st.ReadAll(max_len, out_buf);
}
static inline Size ReadFile(const char *filename, Size max_len, HeapArray<uint8_t> *out_buf)
{
    StreamReader st(filename);
    return st.ReadAll(max_len, out_buf);
}
static inline Size ReadFile(const char *filename, Size max_len, CompressionType compression_type,
                            HeapArray<char> *out_buf)
{
    StreamReader st(filename, compression_type);
    return st.ReadAll(max_len, out_buf);
}
static inline Size ReadFile(const char *filename, Size max_len, HeapArray<char> *out_buf)
{
    StreamReader st(filename);
    return st.ReadAll(max_len, out_buf);
}

class LineReader {
    HeapArray<char> buf;
    Span<char> view = {};

    StreamReader *st;
    bool eof = false;
    bool error = false;

    Span<char> line = {};
    Size line_number = 0;

public:
    LineReader(StreamReader *st) : st(st) {}

    LineReader(const LineReader &other) = delete;
    LineReader &operator=(const LineReader &other) = delete;

    const char *GetFileName() const { return st->GetFileName(); }
    Size GetLineNumber() const { return line_number; }
    bool IsValid() const { return !error; }
    bool IsEOF() const { return eof; }

    bool Next(Span<char> *out_line);
    bool Next(Span<const char> *out_line) { return Next((Span<char> *)out_line); }

    void PushLogHandler();
};

class StreamWriter {
    enum class DestinationType {
        Memory,
        File,
        Function
    };

    const char *filename;

    struct {
        DestinationType type = DestinationType::Memory;
        union U {
            HeapArray<uint8_t> *memory;
            struct {
                FILE *fp;
                bool owned;
            } file;
            std::function<bool(Span<const uint8_t>)> func;

            // StreamWriter deals with func destructor
            U() {}
            ~U() {}
        } u;
    } dest;

    struct {
        CompressionType type = CompressionType::None;
        union {
            struct MinizDeflateContext *miniz;
        } u;
    } compression;

    bool error = false;

public:
    StreamWriter() { Close(); }
    StreamWriter(HeapArray<uint8_t> *mem, const char *filename = nullptr,
                 CompressionType compression_type = CompressionType::None)
        : StreamWriter() { Open(mem, filename, compression_type); }
    StreamWriter(FILE *fp, const char *filename,
                 CompressionType compression_type = CompressionType::None)
        : StreamWriter() { Open(fp, filename, compression_type); }
    StreamWriter(const char *filename,
                 CompressionType compression_type = CompressionType::None)
        : StreamWriter() { Open(filename, compression_type); }
    StreamWriter(const std::function<bool(Span<const uint8_t>)> &func, const char *filename = nullptr,
                 CompressionType compression_type = CompressionType::None)
        : StreamWriter() { Open(func, filename, compression_type); }
    ~StreamWriter() { Close(); }

    StreamWriter(const StreamWriter &other) = delete;
    StreamWriter &operator=(const StreamWriter &other) = delete;

    bool Open(HeapArray<uint8_t> *mem, const char *filename = nullptr,
              CompressionType compression_type = CompressionType::None);
    bool Open(FILE *fp, const char *filename,
              CompressionType compression_type = CompressionType::None);
    bool Open(const char *filename, CompressionType compression_type = CompressionType::None);
    bool Open(const std::function<bool(Span<const uint8_t>)> &func, const char *filename = nullptr,
              CompressionType compression_type = CompressionType::None);
    bool Close();

    const char *GetFileName() const { return filename; }
    CompressionType GetCompressionType() const { return compression.type; }
    bool IsValid() const { return filename && !error; }

    bool Write(Span<const uint8_t> buf);
    bool Write(Span<const char> buf) { return Write(buf.CastAs<const uint8_t>()); }
    bool Write(char buf) { return Write(MakeSpan(&buf, 1)); }
    bool Write(const void *buf, Size len) { return Write(MakeSpan((const uint8_t *)buf, len)); }

private:
    bool InitCompressor(CompressionType type);
    void ReleaseResources();

    bool WriteRaw(Span<const uint8_t> buf);
};

static inline bool WriteFile(Span<const uint8_t> buf, const char *filename,
                             CompressionType compression_type = CompressionType::None)
{
    StreamWriter st(filename, compression_type);
    st.Write(buf);
    return st.Close();
}
static inline bool WriteFile(Span<const char> buf, const char *filename,
                             CompressionType compression_type = CompressionType::None)
{
    StreamWriter st(filename, compression_type);
    st.Write(buf);
    return st.Close();
}

bool SpliceStream(StreamReader *reader, Size max_len, StreamWriter *writer);

// For convenience, don't close them
extern StreamReader stdin_st;
extern StreamWriter stdout_st;
extern StreamWriter stderr_st;

// ------------------------------------------------------------------------
// Format
// ------------------------------------------------------------------------

class FmtArg {
public:
    enum class Type {
        Str1,
        Str2,
        Buffer,
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
        Span
    };

    Type type;
    union {
        const char *str1;
        Span<const char> str2;
        char buf[32];
        char ch;
        bool b;
        int64_t i;
        uint64_t u;
        struct {
            double value;
            int precision;
        } d;
        const void *ptr;
        Size size;
        Date date;

        struct {
            Type type;
            int type_len;
            const void *ptr;
            Size len;
            const char *separator;
        } span;
    } value;

    int repeat = 1;
    int pad_len = 0;
    char pad_char;

    FmtArg() = default;
    FmtArg(const char *str) : type(Type::Str1) { value.str1 = str ? str : "(null)"; }
    FmtArg(Span<const char> str) : type(Type::Str2) { value.str2 = str; }
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
    FmtArg(float f) : type(Type::Double) { value.d = { (double)f, -1 }; }
    FmtArg(double d) : type(Type::Double) { value.d = { d, -1 }; }
    FmtArg(const void *ptr) : type(Type::Hexadecimal) { value.u = (uint64_t)ptr; }
    FmtArg(const Date &date) : type(Type::Date) { value.date = date; }

    FmtArg &Repeat(int new_repeat) { repeat = new_repeat; return *this; }
    FmtArg &Pad(int len, char c = ' ') { pad_char = c; pad_len = len; return *this; }
    FmtArg &Pad0(int len) { return Pad(len, '0'); }
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

static inline FmtArg FmtMemSize(Size size)
{
    FmtArg arg;
    arg.type = FmtArg::Type::MemorySize;
    arg.value.size = size;
    return arg;
}
static inline FmtArg FmtDiskSize(Size size)
{
    FmtArg arg;
    arg.type = FmtArg::Type::DiskSize;
    arg.value.size = size;
    return arg;
}

template <typename T>
FmtArg FmtSpan(Span<T> arr, const char *sep = ", ")
{
    FmtArg arg;
    arg.type = FmtArg::Type::Span;
    arg.value.span.type = FmtArg(T()).type;
    arg.value.span.type_len = RG_SIZE(T);
    arg.value.span.ptr = (const void *)arr.ptr;
    arg.value.span.len = arr.len;
    arg.value.span.separator = sep;
    return arg;
}
template <typename T, Size N>
FmtArg FmtSpan(T (&arr)[N], const char *sep = ", ") { return FmtSpan(MakeSpan(arr), sep); }

enum class LogLevel {
    Debug,
    Info,
    Error
};

Span<char> FmtFmt(const char *fmt, Span<const FmtArg> args, Span<char> out_buf);
Span<char> FmtFmt(const char *fmt, Span<const FmtArg> args, HeapArray<char> *out_buf);
Span<char> FmtFmt(const char *fmt, Span<const FmtArg> args, Allocator *alloc);
void PrintFmt(const char *fmt, Span<const FmtArg> args, StreamWriter *out_st);
void PrintFmt(const char *fmt, Span<const FmtArg> args, FILE *out_fp);
void PrintLnFmt(const char *fmt, Span<const FmtArg> args, StreamWriter *out_st);
void PrintLnFmt(const char *fmt, Span<const FmtArg> args, FILE *out_fp);

#define DEFINE_FMT_VARIANT(Name, Ret, Type) \
    static inline Ret Name(Type out, const char *fmt) \
    { \
        return Name##Fmt(fmt, {}, out); \
    } \
    template <typename... Args> \
    Ret Name(Type out, const char *fmt, Args... args) \
    { \
        const FmtArg fmt_args[] = { FmtArg(args)... }; \
        return Name##Fmt(fmt, fmt_args, out); \
    }

DEFINE_FMT_VARIANT(Fmt, Span<char>, Span<char>)
DEFINE_FMT_VARIANT(Fmt, Span<char>, HeapArray<char> *)
DEFINE_FMT_VARIANT(Fmt, Span<char>, Allocator *)
DEFINE_FMT_VARIANT(Print, void, StreamWriter *)
DEFINE_FMT_VARIANT(Print, void, FILE *)
DEFINE_FMT_VARIANT(PrintLn, void, StreamWriter *)
DEFINE_FMT_VARIANT(PrintLn, void, FILE *)

#undef DEFINE_FMT_VARIANT

// Print formatted strings to stdout
template <typename... Args>
void Print(const char *fmt, Args... args)
{
    Print(stdout, fmt, args...);
}
template <typename... Args>
void PrintLn(const char *fmt, Args... args)
{
    PrintLn(stdout, fmt, args...);
}

// PrintLn variants without format strings
static inline void PrintLn(StreamWriter *out_st) { out_st->Write('\n'); }
static inline void PrintLn(FILE *out_fp) { fputc('\n', out_fp); }
static inline void PrintLn() { putchar('\n'); }

// ------------------------------------------------------------------------
// Debug and errors
// ------------------------------------------------------------------------

typedef void LogHandlerFunc(LogLevel level, const char *ctx, const char *msg);

bool GetDebugFlag(const char *name);
bool LogUsesTerminalOutput();

void LogFmt(LogLevel level, const char *fmt, Span<const FmtArg> args);

static inline void Log(LogLevel level, const char *fmt)
{
    LogFmt(level, fmt, {});
}
template <typename... Args>
static inline void Log(LogLevel level, const char *fmt, Args... args)
{
    const FmtArg fmt_args[] = { FmtArg(args)... };
    LogFmt(level, fmt, fmt_args);
}

// Shortcut log functions
#ifndef NDEBUG
template <typename... Args>
static inline void LogDebug(Args... args) { Log(LogLevel::Debug, args...); }
#else
template <typename... Args>
static inline void LogDebug(Args...) {}
#endif
template <typename... Args>
static inline void LogInfo(Args... args) { Log(LogLevel::Info, args...); }
template <typename... Args>
static inline void LogError(Args... args) { Log(LogLevel::Error, args...); }

void DefaultLogHandler(LogLevel level, const char *ctx, const char *msg);

void StartConsoleLog(LogLevel level);
void EndConsoleLog();

void PushLogHandler(const std::function<LogHandlerFunc> &func);
void PopLogHandler();

const char *GetLastLogError();
void ClearLastLogError();

// ------------------------------------------------------------------------
// Strings
// ------------------------------------------------------------------------

Span<char> DuplicateString(Span<const char> str, Allocator *alloc);

static inline bool IsAsciiAlpha(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
static inline bool IsAsciiDigit(char c)
{
    return (c >= '0' && c <= '9');
}
static inline bool IsAsciiAlphaOrDigit(char c)
{
    return IsAsciiAlpha(c) || IsAsciiDigit(c);
}

static inline char UpperAscii(char c)
{
    if (c >= 'a' && c <= 'z') {
        return (char)(c - 32);
    } else {
        return c;
    }
}
static inline char LowerAscii(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + 32);
    } else {
        return c;
    }
}

static inline bool TestStr(Span<const char> str1, Span<const char> str2)
{
    if (str1.len != str2.len)
        return false;
    for (Size i = 0; i < str1.len; i++) {
        if (str1[i] != str2[i])
            return false;
    }
    return true;
}
static inline bool TestStr(Span<const char> str1, const char *str2)
{
    Size i;
    for (i = 0; i < str1.len && str2[i]; i++) {
        if (str1[i] != str2[i])
            return false;
    }
    return (i == str1.len) && !str2[i];
}
static inline bool TestStr(const char *str1, const char *str2)
    { return !strcmp(str1, str2); }

// Allow direct Span<const char> equality comparison
inline bool Span<const char>::operator==(Span<const char> other) const
    { return TestStr(*this, other); }
inline bool Span<const char>::operator==(const char *other) const
    { return TestStr(*this, other); }

// Case insensitive (ASCII) versions
static inline bool TestStrI(Span<const char> str1, Span<const char> str2)
{
    if (str1.len != str2.len)
        return false;
    for (Size i = 0; i < str1.len; i++) {
        if (LowerAscii(str1[i]) != LowerAscii(str2[i]))
            return false;
    }
    return true;
}
static inline bool TestStrI(Span<const char> str1, const char *str2)
{
    Size i;
    for (i = 0; i < str1.len && str2[i]; i++) {
        if (LowerAscii(str1[i]) != LowerAscii(str2[i]))
            return false;
    }
    return (i == str1.len) && !str2[i];
}
static inline bool TestStrI(const char *str1, const char *str2)
{
    Size i = 0;
    int delta;
    do {
        delta = LowerAscii(str1[i]) - LowerAscii(str2[i]);
    } while (str1[i++] && !delta);
    return !delta;
}

static inline int CmpStr(Span<const char> str1, Span<const char> str2)
{
    for (Size i = 0; i < str1.len && i < str2.len; i++) {
        int delta = str1[i] - str2[i];
        if (delta)
            return delta;
    }
    if (str1.len < str2.len) {
        return -str2[str1.len];
    } else if (str1.len > str2.len) {
        return str1[str2.len];
    } else {
        return 0;
    }
}
static inline int CmpStr(Span<const char> str1, const char *str2)
{
    Size i;
    for (i = 0; i < str1.len && str2[i]; i++) {
        int delta = str1[i] - str2[i];
        if (delta)
            return delta;
    }
    if (str1.len == i) {
        return -str2[i];
    } else {
        return str1[i];
    }
}
static inline int CmpStr(const char *str1, const char *str2)
    { return strcmp(str1, str2); }

template <typename T>
Span<T> SplitStr(Span<T> str, char split_char, Span<T> *out_remainder = nullptr)
{
    Size part_len = 0;
    while (part_len < str.len) {
        if (str[part_len] == split_char) {
            if (out_remainder) {
                *out_remainder = str.Take(part_len + 1, str.len - part_len - 1);
            }
            return str.Take(0, part_len);
        }
        part_len++;
    }

    if (out_remainder) {
        *out_remainder = str.Take(str.len, 0);
    }
    return str;
}
template <typename T>
Span<T> SplitStr(T *str, char split_char, T **out_remainder = nullptr)
{
    Size part_len = 0;
    while (str[part_len]) {
        if (str[part_len] == split_char) {
            if (out_remainder) {
                *out_remainder = str + part_len + 1;
            }
            return MakeSpan(str, part_len);
        }
        part_len++;
    }

    if (out_remainder) {
        *out_remainder = str + part_len;
    }
    return MakeSpan(str, part_len);
}

template <typename T>
Span<T> SplitStrLine(Span<T> str, Span<T> *out_remainder = nullptr)
{
    Span<T> part = SplitStr(str, '\n', out_remainder);
    if (part.len < str.len && part.len && part[part.len - 1] == '\r') {
        part.len--;
    }
    return part;
}
template <typename T>
Span<T> SplitStrLine(T *str, T **out_remainder = nullptr)
{
    Span<T> part = SplitStr(str, '\n', out_remainder);
    if (str[part.len] && part.len && part[part.len - 1] == '\r') {
        part.len--;
    }
    return part;
}

template <typename T>
Span<T> SplitStrAny(Span<T> str, const char *split_chars, Span<T> *out_remainder = nullptr)
{
    Bitset<256> split_mask;
    for (Size i = 0; split_chars[i]; i++) {
        split_mask.Set(split_chars[i]);
    }

    Size part_len = 0;
    while (part_len < str.len) {
        if (split_mask.Test(str[part_len])) {
            if (out_remainder) {
                *out_remainder = str.Take(part_len + 1, str.len - part_len - 1);
            }
            return str.Take(0, part_len);
        }
        part_len++;
    }

    if (out_remainder) {
        *out_remainder = str.Take(str.len, 0);
    }
    return str;
}
template <typename T>
Span<T> SplitStrAny(T *str, const char *split_chars, T **out_remainder = nullptr)
{
    Bitset<256> split_mask;
    for (Size i = 0; split_chars[i]; i++) {
        split_mask.Set(split_chars[i]);
    }

    Size part_len = 0;
    while (str[part_len]) {
        if (split_mask.Test(str[part_len])) {
            if (out_remainder) {
                *out_remainder = str + part_len + 1;
            }
            return MakeSpan(str, part_len);
        }
        part_len++;
    }

    if (out_remainder) {
        *out_remainder = str + part_len;
    }
    return MakeSpan(str, part_len);
}

template <typename T>
Span<T> SplitStrReverse(Span<T> str, char split_char, Span<T> *out_remainder = nullptr)
{
    Size remainder_len = str.len - 1;
    while (remainder_len > 0) {
        if (str[remainder_len] == split_char) {
            if (out_remainder) {
                *out_remainder = str.Take(0, remainder_len);
            }
            return str.Take(remainder_len + 1, str.len - remainder_len - 1);
        }
        remainder_len--;
    }

    if (out_remainder) {
        *out_remainder = str.Take(0, 0);
    }
    return str;
}
template <typename T>
Span<T> SplitStrReverse(T *str, char split_char, Span<T> *out_remainder = nullptr)
    { return SplitStrReverse(Span<T>(str), split_char, out_remainder); }

template <typename T>
Span<T> SplitStrReverseAny(Span<T> str, const char *split_chars, Span<T> *out_remainder = nullptr)
{
    Bitset<256> split_mask;
    for (Size i = 0; split_chars[i]; i++) {
        split_mask.Set(split_chars[i]);
    }

    Size remainder_len = str.len - 1;
    while (remainder_len > 0) {
        if (split_mask.Test(str[remainder_len])) {
            if (out_remainder) {
                *out_remainder = str.Take(0, remainder_len);
            }
            return str.Take(remainder_len + 1, str.len - remainder_len - 1);
        }
        remainder_len--;
    }

    if (out_remainder) {
        *out_remainder = str.Take(0, 0);
    }
    return str;
}
template <typename T>
Span<T> SplitStrReverseAny(T *str, const char *split_chars, Span<T> *out_remainder = nullptr)
    { return SplitStrReverseAny(Span<T>(str), split_chars, out_remainder); }

template <typename T>
Span<T> TrimStrLeft(Span<T> str, const char *trim_chars = " \t\r\n")
{
    while (str.len && strchr(trim_chars, str[0])) {
        str.ptr++;
        str.len--;
    }

    return str;
}
template <typename T>
Span<T> TrimStrRight(Span<T> str, const char *trim_chars = " \t\r\n")
{
    while (str.len && strchr(trim_chars, str[str.len - 1])) {
        str.len--;
    }

    return str;
}
template <typename T>
Span<T> TrimStr(Span<T> str, const char *trim_chars = " \t\r\n")
{
    return TrimStrLeft(TrimStrRight(str, trim_chars), trim_chars);
}

template <typename T>
bool ParseDec(Span<const char> str, T *out_value, int flags = RG_DEFAULT_PARSE_FLAGS,
              Span<const char> *out_remaining = nullptr)
{
    uint64_t value = 0;

    Size pos = 0;
    uint64_t neg = 0;
    if (str.len >= 2) {
        if (std::numeric_limits<T>::min() < 0 && str[0] == '-') {
            pos = 1;
            neg = UINT64_MAX;
        } else if (str[0] == '+') {
            pos = 1;
        }
    }

    for (; pos < str.len; pos++) {
        unsigned int digit = (unsigned int)(str[pos] - '0');
        if (RG_UNLIKELY(digit > 9)) {
            if (!pos || flags & (int)ParseFlag::End) {
                if (flags & (int)ParseFlag::Log) {
                    LogError("Malformed integer number '%1'", str);
                }
                return false;
            } else {
                break;
            }
        }

        uint64_t new_value = (value * 10) + digit;
        if (RG_UNLIKELY(new_value < value))
            goto overflow;
        value = new_value;
    }
    if (RG_UNLIKELY(value > (uint64_t)std::numeric_limits<T>::max()))
        goto overflow;
    value = ((value ^ neg) - neg);

    if (out_remaining) {
        *out_remaining = str.Take(pos, str.len - pos);
    }
    *out_value = (T)value;
    return true;

overflow:
    if (flags & (int)ParseFlag::Log) {
        LogError("Integer overflow for number '%1' (max = %2)", str,
                std::numeric_limits<T>::max());
    }
    return false;
}

// ------------------------------------------------------------------------
// System
// ------------------------------------------------------------------------

#ifdef _WIN32
    #define RG_PATH_SEPARATORS "\\/"
    #define RG_SHARED_LIBRARY_EXTENSION ".dll"
#else
    #define RG_PATH_SEPARATORS "/"
    #define RG_SHARED_LIBRARY_EXTENSION ".so"
#endif

static inline bool IsPathSeparator(char c)
{
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

#ifdef _WIN32
char *Win32ErrorString(uint32_t error_code = UINT32_MAX);
#endif

CompressionType GetPathCompression(Span<const char> filename);
Span<const char> GetPathExtension(Span<const char> filename,
                                  CompressionType *out_compression_type = nullptr);

Span<char> NormalizePath(Span<const char> path, Span<const char> root_directory,
                         Allocator *alloc);
static inline Span<char> NormalizePath(Span<const char> path, Allocator *alloc)
    { return NormalizePath(path, {}, alloc); }

bool PathIsAbsolute(const char *path);
bool PathIsAbsolute(Span<const char> path);
bool PathContainsDotDot(const char *path);

enum class FileType {
    Directory,
    File,
    Unknown
};

struct FileInfo {
    FileType type;

    int64_t size;
    int64_t modification_time;
};

enum class EnumStatus {
    Error,
    Partial,
    Done
};

bool StatFile(const char *filename, bool error_if_missing, FileInfo *out_info);
static inline bool StatFile(const char *filename, FileInfo *out_info)
    { return StatFile(filename, true, out_info); }

EnumStatus EnumerateDirectory(const char *dirname, const char *filter, Size max_files,
                              FunctionRef<bool(const char *, FileType)> func);
bool EnumerateFiles(const char *dirname, const char *filter, Size max_depth, Size max_files,
                    Allocator *str_alloc, HeapArray<const char *> *out_files);

bool TestFile(const char *filename, FileType type = FileType::Unknown);

bool MatchPathName(const char *name, const char *pattern);

bool SetWorkingDirectory(const char *directory);
const char *GetWorkingDirectory();

const char *GetApplicationExecutable(); // Can be NULL
const char *GetApplicationDirectory(); // Can be NULL

bool MakeDirectory(const char *dir, bool error_if_exists = true);
bool MakeDirectoryRec(Span<const char> dir);
bool EnsureDirectoryExists(const char *filename);

enum class OpenFileMode {
    Read = 1 << 0,
    Write = 1 << 1,
    Append = 1 << 2
};

FILE *OpenFile(const char *path, OpenFileMode mode);

bool ExecuteCommandLine(const char *cmd_line, Span<const uint8_t> in_buf,
                        FunctionRef<void(Span<uint8_t> buf)> out_func, int *out_code);
bool ExecuteCommandLine(const char *cmd_line, Span<const uint8_t> in_buf, Size max_len,
                        HeapArray<uint8_t> *out_buf, int *out_code);
static inline bool ExecuteCommandLine(const char *cmd_line, Span<const char> in_buf,
                                      FunctionRef<void(Span<char> buf)> out_func, int *out_code)
{
    return ExecuteCommandLine(cmd_line, in_buf.CastAs<const uint8_t>(),
                              [&](Span<uint8_t> buf) { out_func(buf.CastAs<char>()); }, out_code);
}
static inline bool ExecuteCommandLine(const char *cmd_line, Span<const char> in_buf, Size max_len,
                                      HeapArray<char> *out_buf, int *out_code)
{
    return ExecuteCommandLine(cmd_line, in_buf.CastAs<const uint8_t>(), max_len,
                              (HeapArray<uint8_t> *)out_buf, out_code);
}

void WaitForDelay(int64_t delay);
bool WaitForInterruption(int64_t delay = -1);

int GetCoreCount();

enum class IPStack {
    Dual,
    IPv4,
    IPv6
};
static const char *const IPStackNames[] = {
    "Dual",
    "IPv4",
    "IPv6"
};

#ifndef _WIN32
    #define RG_POSIX_RESTART_EINTR(CallCode) \
        ([&]() { \
            decltype(CallCode) ret; \
            while ((ret = (CallCode)) < 0 && errno == EINTR); \
            return ret; \
        })()
#endif

// ------------------------------------------------------------------------
// Tasks
// ------------------------------------------------------------------------

class AsyncPool;

class Async {
    std::atomic_int success {1};
    std::atomic_int remaining_tasks {0};

    AsyncPool *pool;

public:
    Async(int workers = -1);
    ~Async();

    Async(Async &) = delete;
    Async &operator=(const Async &) = delete;

    void Run(const std::function<bool()> &f);

    bool Sync();
    void Abort();

    static bool IsTaskRunning();

    friend AsyncPool;
};

// ------------------------------------------------------------------------
// INI
// ------------------------------------------------------------------------

struct IniProperty {
    Span<const char> section;
    Span<const char> key;
    Span<const char> value;
};

class IniParser {
    HeapArray<char> current_section;

    enum class LineType {
        Section,
        KeyValue,
        Exit
    };

    LineReader reader;
    bool eof = false;
    bool error = false;

public:
    IniParser(StreamReader *st) : reader(st) {}

    IniParser(const IniParser &other) = delete;
    IniParser &operator=(const IniParser &other) = delete;

    const char *GetFileName() const { return reader.GetFileName(); }
    bool IsValid() const { return !error; }
    bool IsEOF() const { return eof; }

    bool Next(IniProperty *out_prop);
    bool NextInSection(IniProperty *out_prop);

    void PushLogHandler() { reader.PushLogHandler(); }

private:
    LineType FindNextLine(IniProperty *out_prop);
};

// ------------------------------------------------------------------------
// Assets
// ------------------------------------------------------------------------

// Keep in sync with version in packer.cc
struct AssetInfo {
    const char *name;
    CompressionType compression_type;
    Span<const uint8_t> data;

    const char *source_map;
};

enum class AssetLoadStatus {
    Unchanged,
    Loaded,
    Error
};

struct AssetSet {
    HeapArray<AssetInfo> assets;
    LinkedAllocator alloc;

    int64_t last_time = -1;

    AssetLoadStatus LoadFromLibrary(const char *filename, const char *var_name = "pack_assets");
};

Span<const uint8_t> PatchAssetVariables(AssetInfo &asset, Allocator *alloc,
                                        FunctionRef<bool(const char *, StreamWriter *)> func);

// ------------------------------------------------------------------------
// Options
// ------------------------------------------------------------------------

struct OptionDesc {
    const char *name;
    const char *help;
};

enum class OptionType {
    NoValue,
    Value,
    OptionalValue
};

class OptionParser {
    Span<const char *> args;
    unsigned int flags;

    Size pos = 0;
    Size limit;
    Size smallopt_offset = 0;
    char buf[80];

public:
    enum class Flag {
        SkipNonOptions = 1 << 0
    };

    const char *current_option = nullptr;
    const char *current_value = nullptr;

    OptionParser(Span<const char *> args, unsigned int flags = 0)
        : args(args), flags(flags), limit(args.len) {}
    OptionParser(int argc, char **argv, unsigned int flags = 0)
        : args(argc > 0 ? (const char **)(argv + 1) : nullptr, std::max(0, argc - 1)),
          flags(flags), limit(args.len) {}

    const char *Next();
    bool Test(const char *test1, const char *test2, OptionType type = OptionType::NoValue);
    bool Test(const char *test1, OptionType type = OptionType::NoValue)
        { return Test(test1, nullptr, type); }

    const char *ConsumeValue();
    const char *ConsumeNonOption();
    void ConsumeNonOptions(HeapArray<const char *> *non_options);

    Span<const char *> GetRemainingArguments() const { return args.Take(pos, args.len - pos); }
};

}
