// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../common/kutil.hh"
#include "d_codes.hh"
#include "d_authorizations.hh"
#include "d_desc.hh"
#include "d_prices.hh"
#include "d_tables.hh"
#include "d_stays.hh"
#include "a_classifier.hh"
#include "a_constraints.hh"
#include "main.hh"

#ifdef MOYA_IMPLEMENTATION
    #include "../../common/kutil.cc"
    #include "d_authorizations.cc"
    #include "d_desc.cc"
    #include "d_prices.cc"
    #include "d_stays.cc"
    #include "d_tables.cc"
    #include "a_classifier.cc"
    #include "a_constraints.cc"
    #include "main.cc"
#endif
