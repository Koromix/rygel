// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "mco_classifier.hh"
#include "mco_pricing.hh"

bool mco_RunScript(const mco_TableSet &table_set, const mco_AuthorizationSet &authorization_set,
                   const char *script, Span<mco_Result> results, Span<mco_Pricing> pricings);
