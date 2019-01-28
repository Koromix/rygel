// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "human.hh"
#include "death.hh"

// http://cepidc-data.inserm.fr/cgi-bin/broker.exe
// (2015, taux bruts)

double GetDeathProbability(int age, Sex sex, unsigned int flags)
{
    double probability = 0.0;

    // Cardiopathies ischémiques + Autres cardiopathies +
    // Maladies cérébrovasculaires + Autres maladies de l’appareil circulatoire
    if (flags & (int)DeathFlag::Cardiovascular) {
        switch (sex) {
            case Sex::Male: {
                if (age < 45) probability += NAN;
                else if (age < 55) probability += 56.4 / 100000.0;
                else if (age < 65) probability += 149.7 / 100000.0;
                else if (age < 75) probability += 336.3 / 100000.0;
                else if (age < 85) probability += 1157.6 / 100000.0;
                else if (age < 95) probability += 4325.0 / 100000.0;
                else probability += 12233.9 / 100000.0;
            } break;

            case Sex::Female: {
                if (age < 45) probability += NAN;
                else if (age < 55) probability += 19.5 / 100000.0;
                else if (age < 65) probability += 45.2 / 100000.0;
                else if (age < 75) probability += 128.9 / 100000.0;
                else if (age < 85) probability += 678.9 / 100000.0;
                else if (age < 95) probability += 3422.5 / 100000.0;
                else probability += 11326.4 / 100000.0;
            } break;
        }
    }

    if (flags & (int)DeathFlag::Others) {
        switch (sex) {
            case Sex::Male: {
                if (age < 45) probability += NAN;
                else if (age < 55) probability += 331.0 / 100000.0;
                else if (age < 65) probability += 805.9 / 100000.0;
                else if (age < 75) probability += 1494.7 / 100000.0;
                else if (age < 85) probability += 3563.0 / 100000.0;
                else if (age < 95) probability += 10130.2 / 100000.0;
                else probability += 26829.9 / 100000.0;
            } break;

            case Sex::Female: {
                if (age < 45) probability += NAN;
                else if (age < 55) probability += 181.8 / 100000.0;
                else if (age < 65) probability += 380.7 / 100000.0;
                else if (age < 75) probability += 733.8 / 100000.0;
                else if (age < 85) probability += 2083.3 / 100000.0;
                else if (age < 95) probability += 7293.9 / 100000.0;
                else probability += 22765.0 / 100000.0;
            } break;
        }
    }

    return probability;
}
