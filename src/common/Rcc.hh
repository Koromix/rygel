// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "kutil.hh"
#include <Rcpp.h>

extern std::mutex rcc_log_mutex;
extern DynamicQueue<const char *> rcc_log_messages;
extern bool rcc_log_missing_messages;

#define RCC_SETUP_LOG_HANDLER() \
    PushLogHandler([](LogLevel level, const char *ctx, \
                      const char *fmt, Span<const FmtArg> args) { \
        switch (level) { \
            case LogLevel::Error: { \
                std::lock_guard<std::mutex> lock(rcc_log_mutex); \
                const char *msg = FmtFmt(rcc_log_messages.bucket_allocator, fmt, args).ptr; \
                rcc_log_messages.Append(msg); \
                if (rcc_log_messages.len > 100) { \
                    rcc_log_messages.RemoveFirst(); \
                    rcc_log_missing_messages = true; \
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
        Rcc_DumpWarnings(); \
        PopLogHandler(); \
    };

void Rcc_DumpWarnings();
void Rcc_StopWithLastError() __attribute__((noreturn));

template <typename T>
class Rcc_Vector {
    SEXP xp = nullptr;
    Span<T> span = {};

public:
    Rcc_Vector() = default;
    Rcc_Vector(SEXP xp)
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
    Rcc_Vector(Size len)
    {
        if constexpr(std::is_same<typename std::remove_cv<T>::type, int>::value) {
            xp = PROTECT(Rf_allocVector(INTSXP, len));
            span = MakeSpan(INTEGER(xp), Rf_xlength(xp));
        } else if constexpr(std::is_same<typename std::remove_cv<T>::type, double>::value) {
            xp = PROTECT(Rf_allocVector(REALSXP, len));
            span = MakeSpan(REAL(xp), Rf_xlength(xp));
        }
    }

    ~Rcc_Vector()
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
    }

    Rcc_Vector(const Rcc_Vector &other) : xp(PROTECT(other.xp)), span(other.span) {}
    Rcc_Vector &operator=(const Rcc_Vector &other)
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
class Rcc_Vector<const char *> {
    SEXP xp = nullptr;
    Span<SEXP> span = {};

public:
    Rcc_Vector() = default;
    Rcc_Vector(SEXP xp)
        : xp(xp ? PROTECT(xp) : nullptr)
    {
        if (xp) {
            if (TYPEOF(xp) != STRSXP) {
                Rcpp::stop("Expected character vector");
            }
            span = MakeSpan(STRING_PTR(xp), Rf_xlength(xp));
        }
    }
    Rcc_Vector(Size len) : Rcc_Vector(Rf_allocVector(STRSXP, len)) {}

    ~Rcc_Vector()
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
    }

    Rcc_Vector(const Rcc_Vector &other) : xp(PROTECT(other.xp)), span(other.span) {}
    Rcc_Vector &operator=(const Rcc_Vector &other)
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
class Rcc_Vector<Date> {
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
    Rcc_Vector() { u.chr = {}; }
    Rcc_Vector(SEXP xp);
    Rcc_Vector(Size len)
    {
        xp = PROTECT(Rf_allocVector(REALSXP, len));
        type = Type::Date;
        u.num = MakeSpan(REAL(xp), len);

        SEXP cls = PROTECT(Rf_mkString("Date"));
        DEFER { UNPROTECT(1); };
        Rf_setAttrib(xp, R_ClassSymbol, cls);
    }

    ~Rcc_Vector()
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
    }

    Rcc_Vector(const Rcc_Vector &other)
        : xp(PROTECT(other.xp)), type(other.type)
    {
        u.chr = other.u.chr;
    }
    Rcc_Vector &operator=(const Rcc_Vector &other)
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

    void Set(Size idx, Date date);
};

template <typename T, typename U>
U Rcc_GetOptional(T &vec, Size idx, U default_value)
{
    if (UNLIKELY(idx >= vec.Len()))
        return default_value;
    auto value = vec[idx];
    if (vec.IsNA(value))
        return default_value;
    return value;
}

class Rcc_ListBuilder {
    struct Variable {
        const char *name;
        SEXP vec;
    };

    LocalArray<Variable, 64> variables;

public:
    Rcc_ListBuilder() = default;

    Rcc_ListBuilder(const Rcc_ListBuilder &) = delete;
    Rcc_ListBuilder &operator=(const Rcc_ListBuilder &) = delete;

    SEXP Add(const char *name, SEXP vec)
    {
        variables.Append({name, vec});
        return vec;
    }

    SEXP BuildList()
    {
        SEXP list = PROTECT(Rf_allocVector(VECSXP, variables.len));
        DEFER { UNPROTECT(1); };

        {
            SEXP names = PROTECT(Rf_allocVector(STRSXP, variables.len));
            DEFER { UNPROTECT(1); };
            for (Size i = 0; i < variables.len; i++) {
                SET_STRING_ELT(names, i, Rf_mkChar(variables[i].name));
                SET_VECTOR_ELT(list, i, variables[i].vec);
            }
            Rf_setAttrib(list, R_NamesSymbol, names);
        }

        return list;
    }

    SEXP BuildDataFrame()
    {
        Size nrow;
        if (variables.len >= 2) {
            nrow = Rf_xlength(variables[0].vec);
            for (Size i = 1; i < variables.len; i++) {
                if (Rf_xlength(variables[i].vec) != nrow) {
                    Rcpp::stop("Cannot create data.frame from vectors of unequal length");
                }
            }
        } else {
            nrow = 0;
        }

        SEXP df = BuildList();

        // Class
        {
            SEXP cls = PROTECT(Rf_mkString("data.frame"));
            DEFER { UNPROTECT(1); };
            Rf_setAttrib(df, R_ClassSymbol, cls);
        }

        // Compact row names
        {
            SEXP row_names = PROTECT(Rf_allocVector(INTSXP, 2));
            DEFER { UNPROTECT(1); };
            INTEGER(row_names)[0] = NA_INTEGER;
            INTEGER(row_names)[1] = (int)nrow;
            Rf_setAttrib(df, R_RowNamesSymbol, row_names);
        }

        return df;
    }
};

class Rcc_DataFrameBuilder {
    Rcc_ListBuilder list_builder;
    Size len;

public:
    Rcc_DataFrameBuilder(Size len) : len(len) {}

    Rcc_DataFrameBuilder(const Rcc_DataFrameBuilder &) = delete;
    Rcc_DataFrameBuilder &operator=(const Rcc_DataFrameBuilder &) = delete;

    template <typename T>
    Rcc_Vector<T> Add(const char *name) { return list_builder.Add(name, Rcc_Vector<T>(len)); }

    SEXP Build() { return list_builder.BuildDataFrame(); }
};
