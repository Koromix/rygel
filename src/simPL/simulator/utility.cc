// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "human.hh"
#include "utility.hh"

// LUNG CANCER (ICD9 162)  -0.1192(0.043)
// IHD (ICD9 414)  -0.0627 (0.0131)
// STROKE (ICD9 436)   -0.1171 (0.0121)
// DEMENTIA (Clinical classification code 068) -0.1917 (0.0141)
// Multiple Conditions 
// Two -0.0528 (0.0101)
// Three   -0.0415 (0.0115)
// Four    -0.0203 (0.0139)
// Age  (per year) -0.003 (0.0002)
// Least deprived four quintile groups (assume equate to low income, middle income and poor income)    0.04 (0.006)

double UtilityCompute(const Human &human)
{
    if (human.death_happened)
        return 0.0;

    double utility = 1.0;
    int count = 0;

    // FIXME: Semi-random utility values
    /*if (human.lung_cancer.diseased) { utility -= 0.1192; count++; }
    if (human.ihd.diseased && human.ihd.year == 0) { utility -= 0.0627; count++; }
    else if (human.ihd.diseased) { utility -= 0.0627; count++; }
    if (human.stroke.diseased && human.stroke.year == 0) { utility -= 0.0627; count++; }
    else if (human.stroke.diseased) { utility -= 0.1171; count++; }
    if (human.angina.diseased && human.angina.year == 0) { utility -= 0.0627; count++; }
    else if (human.angina.diseased) { utility -= 0.1171; count++; }
    if (human.peripheral_artery_disease.diseased && human.peripheral_artery_disease.year == 0) { utility -= 0.0627; count++; }
    else if (human.peripheral_artery_disease.diseased) { utility -= 0.1171; count++; }*/

    if (human.stroke_happened && human.stroke_age == human.age) { utility -= 0.2; count ++; }
    else if (human.stroke_happened) { utility -= 0.1; count ++; }

    if (count >= 2) utility -= 0.0528;
    if (count >= 3) utility -= 0.0415;
    if (count >= 4) utility -= 0.0203;

    utility = std::max(utility, 0.0);
    return utility;
}
