// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "src/core/base/base.hh"
#include "Rcc.hh"

namespace RG {

BucketArray<const char *> rcc_log_messages;
bool rcc_log_missing_messages = false;

RG_INIT(RedirectLog)
{
    SetLogHandler([](LogLevel level, const char *ctx, const char *msg) {
        switch (level) {
            case LogLevel::Warning:
            case LogLevel::Error: {
                Allocator *alloc;
                const char **ptr = rcc_log_messages.AppendDefault(&alloc);
                *ptr = DuplicateString(msg, alloc).ptr;

                if (rcc_log_messages.len > 100) {
                    rcc_log_messages.RemoveFirst();
                    rcc_log_missing_messages = true;
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
    for (const char *msg: rcc_log_messages) {
        Rcpp::warning(msg);
    }
    rcc_log_messages.Clear();

    if (rcc_log_missing_messages) {
        Rcpp::warning("There were too many warnings, some have been lost");
        rcc_log_missing_messages = false;
    }
}

void rcc_StopWithLastError()
{
    if (rcc_log_messages.len) {
        std::string error_msg = rcc_log_messages[rcc_log_messages.len - 1];
        rcc_log_messages.RemoveLast();
        rcc_DumpWarnings();
        Rcpp::stop(error_msg);
    } else {
        Rcpp::stop("Unknown error");
    }
}

void *rcc_GetPointerSafe(SEXP xp)
{
    if (TYPEOF(xp) != EXTPTRSXP)
        Rcpp::stop("Argument is not an object instance");

    void *ptr = R_ExternalPtrAddr(xp);
    if (!ptr)
        Rcpp::stop("Object instance is not valid");

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

                RG_ASSERT(idx >= 0 && idx < u.chr.len);
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

}
