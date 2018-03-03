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
        RDumpWarnings(); \
        PopLogHandler(); \
    };

void RDumpWarnings();
void RStopWithLastError() __attribute__((noreturn));

template <typename T>
class RVectorView {
    SEXP xp = nullptr;
    Span<T> span = {};

public:
    RVectorView() = default;
    RVectorView(SEXP xp)
        : xp(xp ? PROTECT(xp) : nullptr)
    {
        if (xp) {
            if constexpr(std::is_same<typename std::remove_cv<T>::type, int>::value) {
                if (TYPEOF(xp) != INTSXP) {
                    Rcpp::stop("Expected integer vector");
                }
                span = MakeSpan(INTEGER(xp), Rf_xlength(xp));
            } else if constexpr(std::is_same<typename std::remove_cv<T>::type, double>::value) {
                if (TYPEOF(xp) != REALSXP) {
                    Rcpp::stop("Expected numeric vector");
                }
                span = MakeSpan(REAL(xp), Rf_xlength(xp));
            }
        }
    }
    RVectorView(Size len)
    {
        if constexpr(std::is_same<typename std::remove_cv<T>::type, int>::value) {
            xp = PROTECT(Rf_allocVector(INTSXP, len));
            span = MakeSpan(INTEGER(xp), Rf_xlength(xp));
        } else if constexpr(std::is_same<typename std::remove_cv<T>::type, double>::value) {
            xp = PROTECT(Rf_allocVector(REALSXP, len));
            span = MakeSpan(REAL(xp), Rf_xlength(xp));
        }
    }

    ~RVectorView()
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
    }

    RVectorView(const RVectorView &other) : xp(PROTECT(other.xp)), span(other.span) {}
    RVectorView &operator=(const RVectorView &other)
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
        xp = PROTECT(other.xp);
        span = other.span;
        return *this;
    }

    operator SEXP() const { return xp; }

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
class RVectorView<const char *> {
    SEXP xp = nullptr;
    Span<SEXP> span = {};

public:
    RVectorView() = default;
    RVectorView(SEXP xp)
        : xp(xp ? PROTECT(xp) : nullptr)
    {
        if (xp) {
            if (TYPEOF(xp) != STRSXP) {
                Rcpp::stop("Expected character vector");
            }
            span = MakeSpan(STRING_PTR(xp), Rf_xlength(xp));
        }
    }
    RVectorView(Size len) : RVectorView(Rf_allocVector(STRSXP, len)) {}

    ~RVectorView()
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
    }

    RVectorView(const RVectorView &other) : xp(PROTECT(other.xp)), span(other.span) {}
    RVectorView &operator=(const RVectorView &other)
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
        xp = PROTECT(other.xp);
        span = other.span;
        return *this;
    }

    operator SEXP() const { return xp; }

    Size Len() const { return span.len; }

    static bool IsNA(const char *value) { return value == CHAR(NA_STRING); }

    const char *operator[](Size idx) const { return CHAR(span[idx]); }
    void Set(Size idx, const char *str)
    {
        DebugAssert(idx >= 0 && idx < span.len);
        SET_STRING_ELT(xp, idx, Rf_mkChar(str));
    }
    void Set(Size idx, Span<const char> str)
    {
        DebugAssert(idx >= 0 && idx < span.len);
        DebugAssert(str.len < INT_MAX);
        SET_STRING_ELT(xp, idx, Rf_mkCharLen(str.ptr, (int)str.len));
    }
};

template <>
class RVectorView<Date> {
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
    RVectorView() { u.chr = {}; }
    RVectorView(SEXP xp);
    RVectorView(Size len)
    {
        xp = PROTECT(Rf_allocVector(REALSXP, len));
        type = Type::Date;
        u.num = MakeSpan(REAL(xp), len);

        SEXP cls = PROTECT(Rf_mkString("Date"));
        DEFER { UNPROTECT(1); };
        Rf_setAttrib(xp, R_ClassSymbol, cls);
    }

    ~RVectorView()
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
    }

    RVectorView(const RVectorView &other)
        : xp(PROTECT(other.xp)), type(other.type)
    {
        u.chr = other.u.chr;
    }
    RVectorView &operator=(const RVectorView &other)
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
        xp = PROTECT(other.xp);
        type = other.type;
        u.chr = other.u.chr;
        return *this;
    }

    operator SEXP() const { return xp; }

    Size Len() const { return u.chr.len; }

    static bool IsNA(Date date) { return date.value == INT32_MAX; }

    Date operator[](Size idx) const;
    Date Value() const;
};

template <typename T, typename U>
U RGetOptionalValue(T &vec, Size idx, U default_value)
{
    if (UNLIKELY(idx >= vec.Len()))
        return default_value;
    auto value = vec[idx];
    if (vec.IsNA(value))
        return default_value;
    return value;
}

class RListBuilder {
    struct Column {
        const char *name;
        SEXP vec;
    };

    LocalArray<Column, 64> columns;

public:
    void Add(const char *name, SEXP vec)
    {
        columns.Append({name, vec});
    }

    SEXP BuildList()
    {
        SEXP list = PROTECT(Rf_allocVector(VECSXP, columns.len));
        DEFER { UNPROTECT(1); };

        {
            SEXP names = PROTECT(Rf_allocVector(STRSXP, columns.len));
            DEFER { UNPROTECT(1); };
            for (Size i = 0; i < columns.len; i++) {
                SET_STRING_ELT(names, i, Rf_mkChar(columns[i].name));
                SET_VECTOR_ELT(list, i, columns[i].vec);
            }
            Rf_setAttrib(list, R_NamesSymbol, names);
        }

        return list;
    }

    SEXP BuildDataFrame()
    {
        if (columns.len >= 2) {
            Size nrow = Rf_xlength(columns[0].vec);
            for (Size i = 1; i < columns.len; i++) {
                if (Rf_xlength(columns[i].vec) != nrow) {
                    Rcpp::stop("Cannot create data.frame from vectors of unequal length");
                }
            }
        }

        SEXP df = BuildList();

        {
            SEXP cls = PROTECT(Rf_mkString("data.frame"));
            DEFER { UNPROTECT(1); };
            Rf_setAttrib(df, R_ClassSymbol, cls);
        }

        return df;
    }
};
