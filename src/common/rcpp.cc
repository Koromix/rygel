// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "kutil.hh"
#include "rcpp.hh"

DynamicQueue<const char *> rcpp_log_messages;
bool rcpp_log_missing_messages = false;

void DumpRcppWarnings()
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

void StopRcppWithLastMessage()
{
    if (rcpp_log_messages.len) {
        std::string error_msg = rcpp_log_messages[rcpp_log_messages.len - 1];
        rcpp_log_messages.RemoveLast();
        DumpRcppWarnings();
        Rcpp::stop(error_msg);
    } else {
        Rcpp::stop("Unknown error");
    }
}

RcppDateVector::RcppDateVector(SEXP xp)
{
    if (Rcpp::is<Rcpp::CharacterVector>(xp)) {
        type = Type::Character;
        u.chr = xp;
        len = (Size)u.chr.size();
    } else if ((Rcpp::is<Rcpp::NumericVector>(xp) || Rcpp::is<Rcpp::IntegerVector>(xp)) &&
               Rf_inherits(xp, "Date")) {
        type = Type::Date;
        u.num = xp;
        len = (Size)u.num.size();
    } else {
        Rcpp::stop("Date vector uses unsupported type (must be Date or date-like string)");
    }
}

Date RcppDateVector::operator[](int idx) const
{
    switch (type) {
        case Type::Character: {
            SEXP str = u.chr[idx].get();
            if (str != NA_STRING) {
                Date date = Date::FromString(CHAR(str));
                if (!date.value)
                    StopRcppWithLastMessage();
                return date;
            }
        } break;

        case Type::Date: {
            double value = u.num[idx];
            if (value != NA_REAL) {
                Rcpp::Datetime dt = value * 86400;
                Date date(dt.getYear(), dt.getMonth(), dt.getDay());
                DebugAssert(date.IsValid());
                return date;
            }
        } break;
    }

    return {};
}

Date RcppDateVector::Value() const
{
    if (UNLIKELY(len != 1)) {
        LogError("Date or date-like vector must have one value (no more, no less)");
        StopRcppWithLastMessage();
    }

    return (*this)[0];
}
