// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "economics.hh"
#include "simulate.hh"

namespace RG {

// NOTE: Should we take age into account? How?
double ComputeUtility(const Human &human)
{
    if (!human.alive)
        return 0.0;

    double utility = 1.0;
    int diseases = 0;

    // For now, I got the data from https://doi.org/10.1371/journal.pmed.1002517
    // (so it's NHS data)
    if (human.cardiac_ischemia_age) {
        utility -= 0.0627;
        diseases++;
    }
    if (human.stroke_age) {
        utility -= 0.1171;
        diseases++;
    }
    if (human.lung_cancer_age) {
        utility -= 0.1192;
        diseases++;
    }

    switch (diseases) {
        default: utility -= 0.0528; RG_FALLTHROUGH;
        case 3: utility -= 0.0415; RG_FALLTHROUGH;
        case 2: utility -= 0.0203; RG_FALLTHROUGH;
        case 1:
        case 0: break;
    }

    return utility;
}

double ComputeDiseaseCost(const Human &)
{
    LogError("ComputeDiseaseCost() is a stub");
    RG_ASSERT(false);
}

double ComputeProductivityCost(const Human &)
{
    LogError("ComputeProductivityCost() is a stub");
    RG_ASSERT(false);
}

}
