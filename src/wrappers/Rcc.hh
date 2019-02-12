// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"

#include <Rcpp.h>

extern std::mutex rcc_log_mutex;
extern BlockQueue<const char *> rcc_log_messages;
extern bool rcc_log_missing_messages;

#define RCC_SETUP_LOG_HANDLER() \
    PushLogHandler([](LogLevel level, const char *ctx, \
                      const char *fmt, Span<const FmtArg> args) { \
        switch (level) { \
            case LogLevel::Error: { \
                std::lock_guard<std::mutex> lock(rcc_log_mutex); \
                const char *msg = FmtFmt(fmt, args, rcc_log_messages.bucket_allocator).ptr; \
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
                PrintFmt(fmt, args, stdout); \
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

void *Rcc_GetPointerSafe(SEXP xp);

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

class Rcc_AutoSexp {
    SEXP xp = nullptr;

public:
    Rcc_AutoSexp() = default;
    Rcc_AutoSexp(SEXP xp) : xp(PROTECT(xp)) {}

    ~Rcc_AutoSexp()
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
    }

    Rcc_AutoSexp(const Rcc_AutoSexp &other) : xp(other.xp ? PROTECT(other.xp) : nullptr) {}
    Rcc_AutoSexp &operator=(const Rcc_AutoSexp &other)
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
        xp = other.xp ? PROTECT(other.xp) : nullptr;
        return *this;
    }

    operator bool() const { return xp; }
    operator SEXP() const { return xp; }
    Rcc_AutoSexp &operator=(SEXP new_xp)
    {
        if (xp) {
            UNPROTECT_PTR(xp);
        }
        xp = PROTECT(new_xp);
        return *this;
    }
};

template <typename T>
class Rcc_Vector {};

template <>
class Rcc_Vector<double> {
    Rcc_AutoSexp xp;
    Span<double> span = {};

public:
    Rcc_Vector() = default;
    Rcc_Vector(SEXP xp)
        : xp(xp)
    {
        if (TYPEOF(xp) == REALSXP) {
            span = MakeSpan(REAL(xp), Rf_xlength(xp));
        } else if (xp == R_NilValue) {
            span = {};
        } else {
            Rcpp::stop("Expected numeric vector");
        }
    }
    Rcc_Vector(Size len)
    {
        xp = Rf_allocVector(REALSXP, len);
        span = MakeSpan(REAL(xp), Rf_xlength(xp));
    }

    operator SEXP() const { return xp; }

    double *begin() { return span.begin(); }
    const double *begin() const { return span.begin(); }
    double *end() { return span.end(); }
    const double *end() const { return span.end(); }

    Size Len() const { return span.len; }

    static bool IsNA(double value) { return ISNA(value); }

    double &operator[](Size idx) { return span[idx]; }
    const double &operator[](Size idx) const { return span[idx]; }

    void Set(Size idx, double value) { span[idx] = value; }
};

template <>
class Rcc_Vector<int> {
    Rcc_AutoSexp xp;
    Span<int> span = {};

public:
    Rcc_Vector() = default;
    Rcc_Vector(SEXP xp)
        : xp(xp)
    {
        if (TYPEOF(xp) == INTSXP) {
            span = MakeSpan(INTEGER(xp), Rf_xlength(xp));
        } else if (xp == R_NilValue) {
            span = {};
        } else {
            Rcpp::stop("Expected integer vector");
        }
    }
    Rcc_Vector(Size len)
    {
        xp = Rf_allocVector(INTSXP, len);
        span = MakeSpan(INTEGER(xp), Rf_xlength(xp));
    }

    operator SEXP() const { return xp; }

    int *begin() { return span.begin(); }
    const int *begin() const { return span.begin(); }
    int *end() { return span.end(); }
    const int *end() const { return span.end(); }

    Size Len() const { return span.len; }

    static bool IsNA(int value) { return value == NA_INTEGER; }

    int &operator[](Size idx) { return span[idx]; }
    const int &operator[](Size idx) const { return span[idx]; }

    void Set(Size idx, int value) { span[idx] = value; }
};

template <>
class Rcc_Vector<bool> {
    Rcc_AutoSexp xp;
    Span<int> span = {};

public:
    class Iterator {
        const int *ptr;

    public:
        Iterator(const int *ptr) : ptr(ptr) {}

        bool operator*() { return *ptr; }

        Iterator &operator++() { ptr++; return *this; }
        Iterator operator++(int) { Iterator ret = *this; ++(*this); return ret; }
        Iterator &operator--() { ptr--; return *this; }
        Iterator operator--(int) { Iterator ret = *this; --(*this); return ret; }

        bool operator==(const Iterator &other) const { return ptr == other.ptr; }
        bool operator!=(const Iterator &other) const { return ptr != other.ptr; }
    };

    Rcc_Vector() = default;
    Rcc_Vector(SEXP xp)
        : xp(xp)
    {
        if (TYPEOF(xp) == LGLSXP) {
            span = MakeSpan(INTEGER(xp), Rf_xlength(xp));
        } else if (xp == R_NilValue) {
            span = {};
        } else {
            Rcpp::stop("Expected logical vector");
        }
    }
    Rcc_Vector(Size len)
    {
        xp = Rf_allocVector(LGLSXP, len);
        span = MakeSpan(LOGICAL(xp), Rf_xlength(xp));
    }

    operator SEXP() const { return xp; }

    Iterator begin() const { return Iterator(span.begin()); }
    Iterator end() const { return Iterator(span.end()); }

    Size Len() const { return span.len; }

    static bool IsNA(int value) { return value == NA_LOGICAL; }

    // FIXME: Assigning to bool will unexpectecdly turn NA_LOGICAL into true
    int operator[](Size idx) const { return span[idx]; }

    void Set(Size idx, bool value) { span[idx] = value; }
};

template <>
class Rcc_Vector<const char *> {
    Rcc_AutoSexp xp;
    Span<SEXP> span = {};

public:
    class Iterator {
        const SEXP *xp;

    public:
        Iterator(const SEXP *xp) : xp(xp) {}

        const char *operator*() { return CHAR(*xp); }

        Iterator &operator++() { xp++; return *this; }
        Iterator operator++(int) { Iterator ret = *this; ++(*this); return ret; }
        Iterator &operator--() { xp--; return *this; }
        Iterator operator--(int) { Iterator ret = *this; --(*this); return ret; }

        bool operator==(const Iterator &other) const { return xp == other.xp; }
        bool operator!=(const Iterator &other) const { return xp != other.xp; }
    };

    Rcc_Vector() = default;
    Rcc_Vector(SEXP xp)
        : xp(xp)
    {
        if (TYPEOF(xp) == STRSXP) {
            span = MakeSpan(STRING_PTR(xp), Rf_xlength(xp));
        } else if (xp == R_NilValue) {
            span = {};
        } else {
            Rcpp::stop("Expected character vector");
        }
    }
    Rcc_Vector(Size len) : Rcc_Vector(Rf_allocVector(STRSXP, len)) {}

    operator SEXP() const { return xp; }

    Iterator begin() const { return Iterator(span.begin()); }
    Iterator end() const { return Iterator(span.end()); }

    Size Len() const { return span.len; }

    static bool IsNA(Span<const char> value) { return value.ptr == CHAR(NA_STRING); }
    static bool IsNA(const char *value) { return value == CHAR(NA_STRING); }

    const Span<const char> operator[](Size idx) const
        { return MakeSpan(CHAR(span[idx]), Rf_xlength(span[idx])); }

    void Set(Size idx, const char *str)
    {
        DebugAssert(idx >= 0 && idx < span.len);
        if (str) {
            SET_STRING_ELT(xp, idx, Rf_mkChar(str));
        } else {
            SET_STRING_ELT(xp, idx, NA_STRING);
        }
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

    Rcc_AutoSexp xp;
    Type type = Type::Date;
    union {
        Span<SEXP> chr;
        Span<double> num;
    } u;

public:
    Rcc_Vector() : type(Type::Date) { u.chr = {}; }
    Rcc_Vector(SEXP xp);
    Rcc_Vector(Size len)
    {
        xp = Rf_allocVector(REALSXP, len);
        type = Type::Date;
        u.num = MakeSpan(REAL(xp), len);

        Rcc_AutoSexp cls = Rf_mkString("Date");
        Rf_setAttrib(xp, R_ClassSymbol, cls);
    }

    operator SEXP() const { return xp; }

    Size Len() const { return u.chr.len; }

    static bool IsNA(Date date) { return !date.value; }

    const Date operator[](Size idx) const;
    Date Value() const;

    void Set(Size idx, Date date);
};

template <typename T>
class Rcc_NumericVector {
    enum class Type {
        Int,
        Double
    };

    Rcc_AutoSexp xp;
    Type type;
    union {
        Span<int> i;
        Span<double> d;
    } u;

public:
    Rcc_NumericVector() : type(Type::Int) { u.i = {}; }
    Rcc_NumericVector(SEXP xp)
        : xp(xp)
    {
        if (Rf_isInteger(xp)) {
            type = Type::Int;
            u.i = MakeSpan(INTEGER(xp), Rf_xlength(xp));
        } else if (Rf_isReal(xp)) {
            type = Type::Double;
            u.d = MakeSpan(REAL(xp), Rf_xlength(xp));
        } else {
            Rcpp::stop("Expected numeric or integer vector");
        }
    }

    operator SEXP() const { return xp; }

    Size Len() const { return u.i.len; }

    static bool IsNA(T value) { return Rcc_Vector<T>::IsNA(value); }

    T operator[](Size idx) const
    {
        switch (type) {
            case Type::Int: { return (T)u.i[idx]; } break;
            case Type::Double: { return (T)u.d[idx]; } break;
        }
        DebugAssert(false);
    }
};

class Rcc_ListBuilder {
    struct Variable {
        const char *name;
        SEXP vec;
    };

    LocalArray<Variable, 64> variables;
    BlockAllocator str_alloc;

public:
    Rcc_ListBuilder() = default;

    Rcc_ListBuilder(const Rcc_ListBuilder &) = delete;
    Rcc_ListBuilder &operator=(const Rcc_ListBuilder &) = delete;

    SEXP Add(const char *name, SEXP vec)
    {
        name = DuplicateString(name, &str_alloc).ptr;
        variables.Append({name, vec});
        return vec;
    }

    template <typename T>
    SEXP Set(const char *name, T value)
    {
        Rcc_Vector<T> vec(1);
        vec.Set(0, value);
        return Add(name, vec);
    }

    SEXP BuildList()
    {
        Rcc_AutoSexp list = Rf_allocVector(VECSXP, variables.len);

        {
            Rcc_AutoSexp names = Rf_allocVector(STRSXP, variables.len);
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

        Rcc_AutoSexp df = BuildList();

        // Class
        {
            Rcc_AutoSexp cls = Rf_mkString("data.frame");
            Rf_setAttrib(df, R_ClassSymbol, cls);
        }

        // Compact row names
        {
            Rcc_AutoSexp row_names = Rf_allocVector(INTSXP, 2);
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
