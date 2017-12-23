// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libheimdall/libheimdall.hh"
#include <Rcpp.h>

static thread_local DynamicQueue<const char *> log_messages;
static thread_local bool log_missing_messages = false;

#define SETUP_LOG_HANDLER() \
    PushLogHandler([](LogLevel level, const char *ctx, \
                      const char *fmt, Span<const FmtArg> args) { \
        switch (level) { \
            case LogLevel::Error: { \
                const char *msg = FmtFmt(log_messages.bucket_allocator, fmt, args).ptr; \
                log_messages.Append(msg); \
                if (log_messages.len > 100) { \
                    log_messages.RemoveFirst(); \
                    log_missing_messages = true; \
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
        DumpWarnings(); \
        PopLogHandler(); \
    }

static void DumpWarnings()
{
    for (const char *msg: log_messages) {
        Rcpp::warning(msg);
    }
    log_messages.Clear();

    if (log_missing_messages) {
        Rcpp::warning("There were too many warnings, some have been lost");
        log_missing_messages = false;
    }
}

static void StopWithLastMessage()
{
    if (log_messages.len) {
        std::string error_msg = log_messages[log_messages.len - 1];
        log_messages.RemoveLast();
        DumpWarnings();
        Rcpp::stop(error_msg);
    } else {
        Rcpp::stop("Unknown error");
    }
}

template <int RTYPE, typename T>
T GetOptionalValue(Rcpp::Vector<RTYPE> &vec, R_xlen_t i, T default_value)
{
    if (UNLIKELY(i >= vec.size()))
        return default_value;
    auto value = vec[i % vec.size()];
    if (vec.is_na(value))
        return default_value;
    return value;
}

// [[Rcpp::export(name = 'heimdall.options')]]
SEXP R_Options(SEXP debug = R_NilValue)
{
    SETUP_LOG_HANDLER();

    if (!Rf_isNull(debug)) {
        enable_debug = Rcpp::as<bool>(debug);
    }

    return Rcpp::List::create(
        Rcpp::Named("debug") = enable_debug
    );
}

// [[Rcpp::export(name = 'heimdall.run')]]
void R_Run()
{
    SETUP_LOG_HANDLER();

    if (!Run()) {
        StopWithLastMessage();
    }
}

#undef SETUP_LOG_HANDLER
