// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "kutil.hh"
#include <Rcpp.h>

extern DynamicQueue<const char *> rcpp_log_messages;
extern bool rcpp_log_missing_messages;

#define SETUP_RCPP_LOG_HANDLER() \
    PushLogHandler([](LogLevel level, const char *ctx, \
                      const char *fmt, Span<const FmtArg> args) { \
        switch (level) { \
            case LogLevel::Error: { \
                const char *msg = FmtFmt(rcpp_log_messages.bucket_allocator, fmt, args).ptr; \
                rcpp_log_messages.Append(msg); \
                if (rcpp_log_messages.len > 100) { \
                    rcpp_log_messages.RemoveFirst(); \
                    rcpp_log_missing_messages = true; \
                } \
            } break; \
 \
            case LogLevel::Info: \
            case LogLevel::Debug: { \
                Print("%1", ctx); \
                PrintFmt(stdout, fmt, args); \
                PrintLn(); \
            } break; \
        } \
    }); \
    DEFER { \
        DumpRcppWarnings(); \
        PopLogHandler(); \
    };

void DumpRcppWarnings();
void StopRcppWithLastMessage() __attribute__((noreturn));

template <typename T>
class RVector {
    SEXP xp = nullptr;
    Span<T> span = {};

public:
    RVector() = default;
    RVector(SEXP xp)
        : xp(xp ? PROTECT(xp) : nullptr)
    {
        if (xp) {
            if constexpr(std::is_same<typename std::remove_cv<T>::type, int>::value) {
                span = MakeSpan(INTEGER(xp), Rf_xlength(xp));
            }
        }
    }
    ~RVector()
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
    }

    RVector(const RVector &other) : xp(PROTECT(other.xp)), span(other.span) {}
    RVector &operator=(const RVector &other)
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
        xp = PROTECT(other.xp);
        span = other.span;
        return *this;
    }

    Size Len() const { return span.len; }

    static bool IsNA(T value)
    {
        if constexpr(std::is_same<typename std::remove_cv<T>::type, int>::value) {
            return value == NA_INTEGER;
        } else if constexpr(std::is_same<typename std::remove_cv<T>::type, double>::value) {
            return ISNA(value);
        }
    }

    T &operator[](Size idx) { return span[idx]; }
    const T &operator[](Size idx) const { return span[idx]; }
};

template <>
class RVector<const char *> {
    SEXP xp = nullptr;
    Span<SEXP> span = {};

public:

    RVector() = default;
    RVector(SEXP xp)
        : xp(xp ? PROTECT(xp) : nullptr)
    {
        if (xp) {
            span = MakeSpan(STRING_PTR(xp), Rf_xlength(xp));
        }
    }
    ~RVector()
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
    }

    RVector(const RVector &other) : xp(PROTECT(other.xp)), span(other.span) {}
    RVector &operator=(const RVector &other)
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
        xp = PROTECT(other.xp);
        span = other.span;
        return *this;
    }

    Size Len() const { return span.len; }

    static bool IsNA(const char *value) { return value == CHAR(NA_STRING); }

    const char *operator[](Size idx) const { return CHAR(span[idx]); }
};

template <>
class RVector<Date> {
    enum class Type {
        Character,
        Date
    };

    SEXP xp = nullptr;
    Type type;
    union {
        Span<SEXP> chr;
        Span<double> num;
    } u;

public:
    RVector() = default;
    RVector(SEXP xp);
    ~RVector()
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
    }

    RVector(const RVector &other) : xp(PROTECT(other.xp)), type(other.type)
    {
        u.chr = other.u.chr;
    }
    RVector &operator=(const RVector &other)
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
        xp = PROTECT(other.xp);
        type = other.type;
        u.chr = other.u.chr;
        return *this;
    }

    Size Len() const { return u.chr.len; }

    static bool IsNA(Date date) { return date.value == INT32_MAX; }

    Date operator[](Size idx) const;
    Date Value() const;
};

template <typename T, typename U>
U GetRcppOptionalValue(T &vec, Size idx, U default_value)
{
    if (UNLIKELY(idx >= vec.Len()))
        return default_value;
    auto value = vec[idx];
    if (vec.IsNA(value))
        return default_value;
    return value;
}
