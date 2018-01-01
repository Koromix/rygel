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
void StopRcppWithLastMessage();

class RcppDateVector {
    enum class Type {
        Character,
        Date
    };

    Type type;
    struct { // FIXME: I want union
        Rcpp::CharacterVector chr;
        Rcpp::NumericVector num;
    } u;

public:
    Size len = 0;

    RcppDateVector() = default;
    RcppDateVector(SEXP xp);

    Date operator[](int idx) const;
    Date Value() const;
};

template <int RTYPE, typename T>
T GetRcppOptionalValue(Rcpp::Vector<RTYPE> &vec, R_xlen_t i, T default_value)
{
    if (UNLIKELY(i >= vec.size()))
        return default_value;
    auto value = vec[i % vec.size()];
    if (vec.is_na(value))
        return default_value;
    return value;
}
