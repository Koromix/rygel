// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "kutil.hh"
#include "rcpp.hh"

DynamicQueue<const char *> rcpp_log_messages;
bool rcpp_log_missing_messages = false;

void RDumpWarnings()
{
    for (const char *msg: rcpp_log_messages) {
        Rcpp::warning(msg);
    }
    rcpp_log_messages.Clear();

    if (rcpp_log_missing_messages) {
        Rcpp::warning("There were too many warnings, some have been lost");
        rcpp_log_missing_messages = false;
    }
}

void RStopWithLastError()
{
    if (rcpp_log_messages.len) {
        std::string error_msg = rcpp_log_messages[rcpp_log_messages.len - 1];
        rcpp_log_messages.RemoveLast();
        RDumpWarnings();
        Rcpp::stop(error_msg);
    } else {
        Rcpp::stop("Unknown error");
    }
}

RVectorView<Date>::RVectorView(SEXP xp)
    : xp(PROTECT(xp))
{
    if (Rf_isString(xp)) {
        type = Type::Character;
        u.chr = MakeSpan(STRING_PTR(xp), Rf_xlength(xp));
    } else if (Rf_isReal(xp) && Rf_inherits(xp, "Date")) {
        type = Type::Date;
        u.num = MakeSpan(REAL(xp), Rf_xlength(xp));
    } else {
        Rcpp::stop("Date vector uses unsupported type (must be Date or date-like string)");
    }
}

Date RVectorView<Date>::operator[](Size idx) const
{
    Date date;
    date.value = INT32_MAX; // NA

    switch (type) {
        case Type::Character: {
            SEXP str = u.chr[idx];
            if (str != NA_STRING) {
                date = Date::FromString(CHAR(str));
            }
        } break;

        case Type::Date: {
            double value = u.num[idx];
            if (value != NA_REAL) {
                date = Date::FromCalendarDate((int)value);
            }
        } break;
    }

    return date;
}

Date RVectorView<Date>::Value() const
{
    if (UNLIKELY(Len() != 1)) {
        LogError("Date or date-like vector must have one value (no more, no less)");
        RStopWithLastError();
    }

    return (*this)[0];
}
