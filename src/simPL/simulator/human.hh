// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"

enum class Sex {
    Male,
    Female
};
static const char *const SexNames[] = {
    "Male",
    "Female"
};

struct Human {
    // Identifier
    int id;

    // Socio-demographic
    Sex sex;
    int age;
    int sociocultural_level;

    // CV risk factors
    bool smoking_status;
    int smoking_cessation_age;
    double bmi_base;
    double systolic_pressure_base;
    double total_cholesterol_base;

    // Drugs
    double bmi_therapy;
    double systolic_pressure_therapy;
    double total_cholesterol_therapy;

    // Computed effects
    double BMI() const { return bmi_base + bmi_therapy; }
    double SystolicPressure() const { return systolic_pressure_base - 10 * (BMI() >= 30) -
                                             systolic_pressure_therapy; }
    double TotalCholesterol() const { return total_cholesterol_base - total_cholesterol_therapy; }

    // PL checkup
    int checkup_age;

    // Death
    bool death_happened;

    // Utility and cost
    double utility;
    double cost;
};

Human CreateHuman(int id);
Human SimulateOneYear(const Human &prev, uint64_t flags);
