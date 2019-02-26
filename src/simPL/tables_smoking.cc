// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "simulate.hh"
#include "tables.hh"

double GetSmokingPrevalence(int age, Sex sex)
{
    switch (sex) {
        case Sex::Male: {
            if (age < 25) return 0.353; // age >= 18
            else if (age < 35) return 0.417;
            else if (age < 45) return 0.357;
            else if (age < 55) return 0.305;
            else if (age < 65) return 0.239;
            else return 0.107;
        } break;

        case Sex::Female: {
            if (age < 25) return 0.288; // age >= 18
            else if (age < 35) return 0.315;
            else if (age < 45) return 0.284;
            else if (age < 55) return 0.308;
            else if (age < 65) return 0.176;
            else return 0.084;
        } break;
    }

    Assert(false);
}

double GetSmokingStopTrialProbability(int age)
{
    if (age < 25) return 0.197; // age >= 18
    else if (age < 35) return 0.153;
    else if (age < 45) return 0.104;
    else if (age < 55) return 0.105;
    else if (age < 65) return 0.12;
    else return 0.099; // age < 75
}
