/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "kutil.hh"
#include "codes.hh"
#include "tables.hh"
#include "pricing.hh"
#include "data.hh"
#include "algorithm.hh"
#include "constraints.hh"
#include "main.hh"

#ifdef MOYA_IMPLEMENTATION
    #include "kutil.cc"
    #include "tables.cc"
    #include "pricing.cc"
    #include "data_json.cc"
    #include "algorithm.cc"
    #include "constraints.cc"
    #include "main.cc"
#endif
