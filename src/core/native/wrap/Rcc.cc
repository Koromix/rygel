// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/native/base/base.hh"
#include "Rcc.hh"

namespace K {

static BucketArray<const char *> log_messages;
static bool log_missing = false;

void rcc_RedirectLog()
{
    SetLogHandler([](LogLevel level, const char *ctx, const char *msg) {
        switch (level) {
            case LogLevel::Warning:
            case LogLevel::Error: {
                Allocator *alloc;
                const char **ptr = log_messages.AppendDefault(&alloc);

                *ptr = DuplicateString(msg, alloc).ptr;

                if (log_messages.count > 100) {
                    log_messages.RemoveFirst();
                    log_missing = true;
                }
            } break;

            case LogLevel::Info:
            case LogLevel::Debug: {
                PrintLn("%1%2%3", ctx ? ctx : "", ctx ? ": " : "", msg);
            } break;
        }
    }, false);
}

void rcc_DumpWarnings()
{
    for (const char *msg: log_messages) {
        Rcpp::warning(msg);
    }
    log_messages.Clear();

    if (log_missing) {
        Rcpp::warning("There were too many warnings, some have been lost");
        log_missing = false;
    }
}

void rcc_StopWithLastError()
{
    if (log_messages.count) {
        std::string error_msg = log_messages[log_messages.count - 1];
        log_messages.RemoveLast();
        rcc_DumpWarnings();
        Rcpp::stop(error_msg);
    } else {
        Rcpp::stop("Unknown error");
    }
}

void *rcc_GetPointerSafe(SEXP xp, SEXP tag)
{
    if (TYPEOF(xp) != EXTPTRSXP) [[unlikely]]
        Rcpp::stop("Argument is not an object instance");

    void *ptr = R_ExternalPtrAddr(xp);
    SEXP cmp = R_ExternalPtrTag(xp);

    if (!ptr) [[unlikely]]
        Rcpp::stop("Object instance is not valid");
    if (tag != cmp) [[unlikely]]
        Rcpp::stop("Unexpected object instance tag");

    return ptr;
}

rcc_Vector<LocalDate>::rcc_Vector(SEXP xp)
    : xp(xp)
{
    if (Rf_isString(xp)) {
        type = Type::Character;
        u.chr = MakeSpan(STRING_PTR(xp), Rf_xlength(xp));
    } else if (Rf_isReal(xp) && Rf_inherits(xp, "Date")) {
        type = Type::Date;
        u.num = MakeSpan(REAL(xp), Rf_xlength(xp));
    } else if (xp == R_NilValue) {
        type = Type::Date;
        u.num = {};
    } else {
        Rcpp::stop("Date vector uses unsupported type (must be Date or date-like string)");
    }
}

const LocalDate rcc_Vector<LocalDate>::operator[](Size idx) const
{
    LocalDate date = {}; // NA

    switch (type) {
        case Type::Character: {
            SEXP str = u.chr[idx];
            if (str != NA_STRING) {
                ParseDate(CHAR(str), &date, (int)ParseFlag::Validate | (int)ParseFlag::End);
            }
        } break;

        case Type::Date: {
            double value = u.num[idx];
            if (!ISNA(value)) {
                date = LocalDate::FromCalendarDate((int)value);
            }
        } break;
    }

    return date;
}

LocalDate rcc_Vector<LocalDate>::Value() const
{
    if (Len() != 1) [[unlikely]]
        Rcpp::stop("Date or date-like vector must have one value (no more, no less)");

    return (*this)[0];
}

void rcc_Vector<LocalDate>::Set(Size idx, LocalDate date)
{
    switch (type) {
        case Type::Character: {
            if (date.value) {
                char buf[32];
                Fmt(buf, "%1", date);

                K_ASSERT(idx >= 0 && idx < u.chr.len);
                SET_STRING_ELT(xp, idx, Rf_mkChar(buf));
            } else {
                SET_STRING_ELT(xp, idx, NA_STRING);
            }
        } break;

        case Type::Date: {
            if (date.value) {
                u.num[idx] = date.ToCalendarDate();
            } else {
                u.num[idx] = NA_REAL;
            }
        } break;
    }
}

SEXP rcc_ListBuilder::Add(const char *name, SEXP vec)
{
    name = DuplicateString(name, &str_alloc).ptr;
    members.Append({ name, vec });
    return vec;
}

SEXP rcc_ListBuilder::Build()
{
    rcc_AutoSexp list = Rf_allocVector(VECSXP, members.len);
    rcc_AutoSexp names = Rf_allocVector(STRSXP, members.len);

    for (Size i = 0; i < members.len; i++) {
        SET_STRING_ELT(names, i, Rf_mkChar(members[i].name));
        SET_VECTOR_ELT(list, i, members[i].vec);
    }

    Rf_setAttrib(list, R_NamesSymbol, names);

    return list;
}

SEXP rcc_DataFrameBuilder::Build()
{
    rcc_AutoSexp df = builder.Build();

    // Add class
    {
        rcc_AutoSexp cls = Rf_mkString("data.frame");
        Rf_setAttrib(df, R_ClassSymbol, cls);
    }

    // Compact row names
    {
        rcc_AutoSexp row_names = Rf_allocVector(INTSXP, 2);
        INTEGER(row_names)[0] = NA_INTEGER;
        INTEGER(row_names)[1] = (int)len;
        Rf_setAttrib(df, R_RowNamesSymbol, row_names);
    }

    return df;
}

SEXP rcc_DataFrameBuilder::Build(Size shrink)
{
    K_ASSERT(shrink <= len);

    if (shrink < len) {
        for (rcc_ListMember &member: builder) {
            member.vec = Rf_lengthgets(member.vec, shrink);
        }

        len = shrink;
    }

    return Build();
}

}
