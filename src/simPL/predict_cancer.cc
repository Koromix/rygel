// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "simulate.hh"

// TODO: Simulate cigarettes smoked per day (or use average values)
double PredictLungCancer(const Human &human)
{
    double value = -9.7960571;

    int cpd = 20;
    int smk;
    int quit;
    if (human.smoking_cessation_age) {
        smk = human.smoking_cessation_age - human.smoking_start_age;
        quit = human.age - human.smoking_cessation_age;
    } else if (human.smoking_start_age) {
        smk = human.age - human.smoking_start_age;
        quit = 0;
    } else {
        smk = 0;
        quit = 0;
    }

    // CPD
    value += 0.060818386 * cpd;
    if (cpd > 15) value -= 0.00014652216 * pow(cpd - 15, 3);
    if (cpd > 20) value += 0.00018486938 * pow(cpd - 20.185718, 3);
    if (cpd > 40) value -= 0.000038347226 * pow(cpd - 40, 3);

    // SMK
    value += 0.11425297 * smk;
    if (smk > 27) value -= 0.000080091477 * pow(smk - 27.6577, 3);
    if (smk > 40) value += 0.000080091477 * pow(smk - 40, 3);
    if (smk > 50) value -= 0.000080091477 * pow(smk - 50.910335, 3);

    // QUIT
    value -= 0.085684793 * quit;
    value += 0.0065499693 * pow(quit, 3);
    if (quit > 0) value -= 0.0068305845 * pow(quit - 0.50513347, 3);
    if (quit > 12) value += 0.00028061519 * pow(quit - 12.295688, 3);

    // AGE
    value += 0.070322812 * human.age;
    if (human.age > 53) value -= 0.00009382122 * pow(human.age - 53.459001, 3);
    if (human.age > 61) value += 0.00018282661 * pow(human.age - 61.954825, 3);
    if (human.age > 70) value -= 0.000089005389 * pow(human.age - 70.910335, 3);

    // No ASB... sorry

    // SEX
    if (human.sex == Sex::Female) value -= 0.05827261;

    return 1.0 - pow(0.99629, exp(value));
}
