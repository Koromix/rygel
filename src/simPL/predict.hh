// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "simulate.hh"

// Lung cancer
double PredictLungCancer(const Human &human);

// Cardiovascular
double PredictFraminghamScore(const Human &human);
double PredictHeartScore(const Human &human);
double PredictQRisk3(const Human &human);
