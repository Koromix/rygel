// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "common.hh"
#include "mco_common.hh"
#include "mco_authorizations.hh"
#include "mco_catalogs.hh"
#include "mco_tables.hh"
#include "mco_stays.hh"
#include "mco_classifier.hh"
#include "mco_constraints.hh"
#include "mco_main.hh"

#ifdef DRD_IMPLEMENTATION
    #include "../common/kutil.cc"
    #include "mco_authorizations.cc"
    #include "mco_catalogs.cc"
    #include "mco_tables.cc"
    #include "mco_stays.cc"
    #include "mco_classifier.cc"
    #include "mco_constraints.cc"
    #include "mco_main.cc"
#endif
