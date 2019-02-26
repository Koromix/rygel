// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "simulate.hh"

double ComputeUtility(const Human &human);
double ComputeDiseaseCost(const Human &human);
double ComputeProductivityCost(const Human &human);
