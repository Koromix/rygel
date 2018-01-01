// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libheimdall/libheimdall.hh"
#include "../../common/rcpp.hh"

// [[Rcpp::export(name = 'heimdall.options')]]
SEXP R_Options(SEXP debug = R_NilValue)
{
    SETUP_RCPP_LOG_HANDLER();

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
    SETUP_RCPP_LOG_HANDLER();

    if (!Run()) {
        StopRcppWithLastMessage();
    }
}
