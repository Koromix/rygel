// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "mco_classifier.hh"
#include "mco_pricing.hh"

bool mco_RunScript(const char *script,
                   Span<const mco_Result> results, Span<const mco_Result> mono_results,
                   Span<const mco_Pricing> pricings, Span<const mco_Pricing> mono_pricings);
