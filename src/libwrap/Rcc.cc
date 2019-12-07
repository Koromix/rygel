// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "Rcc.hh"

namespace RG {

std::mutex rcc_log_mutex;
BucketArray<const char *> rcc_log_messages;
bool rcc_log_missing_messages = false;

RG_INIT(RedirectLog)
{
    SetLogHandler([](LogLevel level, const char *ctx, const char *msg) {
        switch (level) {
            case LogLevel::Error: {
                std::lock_guard<std::mutex> lock(rcc_log_mutex);

                const char **ptr = rcc_log_messages.AppendDefault();
                *ptr = DuplicateString(msg, rcc_log_messages.GetBucketAllocator()).ptr;

                if (rcc_log_messages.len > 100) {
                    rcc_log_messages.RemoveFirst();
                    rcc_log_missing_messages = true;
                }
            } break;

            case LogLevel::Info:
            case LogLevel::Debug: {
                PrintLn("%1%2", ctx, msg);
            } break;
        }
    });
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

rcc_Vector<Date>::rcc_Vector(SEXP xp)
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

const Date rcc_Vector<Date>::operator[](Size idx) const
{
    Date date = {}; // NA

    switch (type) {
        case Type::Character: {
            SEXP str = u.chr[idx];
            if (str != NA_STRING) {
                date = Date::FromString(CHAR(str), (int)ParseFlag::End);
            }
        } break;

        case Type::Date: {
            double value = u.num[idx];
            if (!ISNA(value)) {
                date = Date::FromCalendarDate((int)value);
            }
        } break;
    }

    return date;
}

Date rcc_Vector<Date>::Value() const
{
    if (RG_UNLIKELY(Len() != 1)) {
        LogError("Date or date-like vector must have one value (no more, no less)");
        rcc_StopWithLastError();
    }

    return (*this)[0];
}

void rcc_Vector<Date>::Set(Size idx, Date date)
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
