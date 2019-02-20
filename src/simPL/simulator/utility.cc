// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "human.hh"
#include "utility.hh"

double UtilityComputeUtility(const Human &human)
{
    if (human.death_happened)
        return 0.0;

    double utility = 1.0;
    int count = 0;

    // TODO: Use english utility values (for now)

    if (count >= 2) utility -= 0.0528;
    if (count >= 3) utility -= 0.0415;
    if (count >= 4) utility -= 0.0203;

    utility = std::max(utility, 0.0);
    return utility;
}

double UtilityComputeCost(const Human &human)
{
    double cost = 0.0;

    cost += human.bmi_therapy ? 10.0 : 0.0;
    cost += human.systolic_pressure_therapy ? 10.0 : 0.0;
    cost += human.total_cholesterol_therapy ? 10.0 : 0.0;

    return cost;
}
