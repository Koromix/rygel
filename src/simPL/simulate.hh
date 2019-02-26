// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "../../lib/pcg/pcg_basic.h"

enum class Sex {
    Male,
    Female
};
static const char *const SexNames[] = {
    "Male",
    "Female"
};

enum class DeathType {
    CardiacIschemia,
    LungCancer,
    OtherCauses
};
static const char *const DeathTypeNames[] = {
    "CardiacIschemia",
    "LungCancer",
    "OtherCauses"
};

struct SimulationConfig {
    // User parameters
    int count;
    int seed;
};

struct Human {
    pcg32_random_t rand_evolution;
    pcg32_random_t rand_therapy;

    int age;
    Sex sex;

    // Smoking
    bool smoking_status;
    int smoking_cessation_age;

    // Systolic blood pressure
    double systolic_pressure_drugs = 0.0;
    double SystolicPressure() const { return 0.0; }

    // Other risk factor stubs
    bool diabetes_status;
    double BMI() const { return 0.0; }
    double TotalCholesterol() const { return 0.0; }
    double HDL() const { return 0.0; }

    // Diseases
    int cardiac_ischemia_age;
    int stroke_age;
    int lung_cancer_age;

    // Death
    bool alive;
    DeathType death_type;
};

extern "C" void InitializeConfig(int count, int seed, SimulationConfig *out_config);

extern "C" Size InitializeHumans(const SimulationConfig &config, HeapArray<Human> *out_humans);
extern "C" Size RunSimulationStep(const SimulationConfig &config, Span<const Human> humans,
                                  HeapArray<Human> *out_humans);
