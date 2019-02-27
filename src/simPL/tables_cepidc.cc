// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "simulate.hh"
#include "tables.hh"

double GetDeathProbability(int age, Sex sex, unsigned int flags)
{
    double probability = 0.0;

    // Data from http://cepidc-data.inserm.fr/cgi-bin/broker.exe
    // (2015, raw rates)

    if (flags & (1 << (int)DeathType::CardiacIschemia)) {
        switch (sex) {
            case Sex::Male: {
                if (age < 45) probability += NAN;
                else if (age < 55) probability += 23.9 / 100000.0;
                else if (age < 65) probability += 60.4 / 100000.0;
                else if (age < 75) probability += 123.5 / 100000.0;
                else if (age < 85) probability += 352.3 / 100000.0;
                else if (age < 95) probability += 1071.1 / 100000.0;
                else probability += 2634.2 / 100000.0;
            } break;
            case Sex::Female: {
                if (age < 45) probability += NAN;
                else if (age < 55) probability += 4.0 / 100000.0;
                else if (age < 65) probability += 10.7 / 100000.0;
                else if (age < 75) probability += 28.7 / 100000.0;
                else if (age < 85) probability += 132.2 / 100000.0;
                else if (age < 95) probability += 582.3 / 100000.0;
                else probability += 1804.9 / 100000.0;
            } break;
        }
    }

    if (flags & (1 << (int)DeathType::Stroke)) {
        switch (sex) {
            case Sex::Male: {
                if (age < 45) probability += NAN;
                else if (age < 55) probability += 9.4 / 100000.0;
                else if (age < 65) probability += 25.9 / 100000.0;
                else if (age < 75) probability += 62.6 / 100000.0;
                else if (age < 85) probability += 252.6 / 100000.0;
                else if (age < 95) probability += 837.5 / 100000.0;
                else probability += 1947.6 / 100000.0;
            } break;
            case Sex::Female: {
                if (age < 45) probability += NAN;
                else if (age < 55) probability += 6.5 / 100000.0;
                else if (age < 65) probability += 12.1 / 100000.0;
                else if (age < 75) probability += 34.5 / 100000.0;
                else if (age < 85) probability += 190.8 / 100000.0;
                else if (age < 95) probability += 808.7 / 100000.0;
                else probability += 2087.1 / 100000.0;
            } break;
        }
    }

    if (flags & (1 << (int)DeathType::LungCancer)) {
        switch (sex) {
            case Sex::Male: {
                if (age < 45) probability += NAN;
                else if (age < 55) probability += 42.9 / 100000.0;
                else if (age < 65) probability += 161.5 / 100000.0;
                else if (age < 75) probability += 262.8 / 100000.0;
                else if (age < 85) probability += 332.9 / 100000.0;
                else if (age < 95) probability += 377.2 / 100000.0;
                else probability += 298.0 / 100000.0;
            } break;
            case Sex::Female: {
                if (age < 45) probability += NAN;
                else if (age < 55) probability += 23.6 / 100000.0;
                else if (age < 65) probability += 59.0 / 100000.0;
                else if (age < 75) probability += 69.6 / 100000.0;
                else if (age < 85) probability += 84.7 / 100000.0;
                else if (age < 95) probability += 98.9 / 100000.0;
                else probability += 98.4 / 100000.0;
            } break;
        }
    }

    if (flags & (1 << (int)DeathType::OtherCauses)) {
        switch (sex) {
            case Sex::Male: {
                if (age < 45) probability += NAN;
                else if (age < 55) probability += 310.9 / 100000.0;
                else if (age < 65) probability += 707.7 / 100000.0;
                else if (age < 75) probability += 1382.3 / 100000.0;
                else if (age < 85) probability += 3782.9 / 100000.0;
                else if (age < 95) probability += 12169.5 / 100000.0;
                else probability += 34183.8 / 100000.0;
            } break;
            case Sex::Female: {
                if (age < 45) probability += NAN;
                else if (age < 55) probability += 167.0 / 100000.0;
                else if (age < 65) probability += 343.6 / 100000.0;
                else if (age < 75) probability += 729.9 / 100000.0;
                else if (age < 85) probability += 2354.4 / 100000.0;
                else if (age < 95) probability += 9226.3 / 100000.0;
                else probability += 30101.0 / 100000.0;
            } break;
        }
    }

    return probability;
}
