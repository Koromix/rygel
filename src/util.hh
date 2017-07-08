#pragma once

#define __USE_MINGW_ANSI_STDIO 1
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <vector>

// ------------------------------------------------------------------------
// Utilities
// ------------------------------------------------------------------------

#define STRINGIFY_(a) #a
#define STRINGIFY(a) STRINGIFY_(a)
#define CONCAT_(a, b) a ## b
#define CONCAT(a, b) CONCAT_(a, b)
#define UNIQUE_ID(prefix) CONCAT(prefix, __LINE__)
#define FORCE_EXPAND(x) x

#if defined(__GNUC__)
    #define MAYBE_UNUSED __attribute__((unused))
    #define FORCE_INLINE __attribute__((always_inline)) inline
    #define LIKELY(cond) __builtin_expect(!!(cond), 1)
    #define UNLIKELY(cond) __builtin_expect(!!(cond), 0)

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
#else
    #define MAYBE_UNUSED
    #define FORCE_INLINE
    #define LIKELY(cond) (cond)
    #define UNLIKELY(cond) (cond)
#endif

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

    // Those two methods allow for lambda assignement, which makes possible the DEFER
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
