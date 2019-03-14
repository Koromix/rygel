// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "../../vendor/pcg/pcg_basic.h"

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
    Stroke,
    LungCancer,
    OtherCauses
};
static const char *const DeathTypeNames[] = {
    "CardiacIschemia",
    "Stroke",
    "LungCancer",
    "OtherCauses"
};

enum class PredictCvdMode {
    Disabled,
    Framingham,
    QRisk3,
    HeartScore
};
static const char *const PredictCvdModeNames[] = {
    "Disabled",
    "Framingham",
    "QRisk3",
    "HeartScore"
};

enum class PredictLungCancerMode {
    Disabled,
    CARET
};
static const char *const PredictLungCancerModeNames[] = {
    "Disabled",
    "CARET"
};

struct SimulationConfig {
    // User parameters
    int count;
    int seed;
    double discount_rate;

    // Modes
    PredictCvdMode predict_cvd;
    PredictLungCancerMode predict_lung_cancer;
};

struct Human {
    pcg32_random_t rand_evolution;
    pcg32_random_t rand_therapy;
    int iteration;

    int age;
    Sex sex;

    // Smoking
    int smoking_start_age;
    int smoking_cessation_age;
    bool SmokingStatus() const { return smoking_start_age && !smoking_cessation_age; }

    // Systolic blood pressure
    double systolic_pressure_drugs = 0.0;
    double SystolicPressure() const { return 120.0; }

    // Other risk factor stubs
    bool diabetes_status;
    double BMI() const { return 30.0; }
    double TotalCholesterol() const { return 200.0; }
    double HDL() const { return 40.0; }

    // Diseases
    int cardiac_ischemia_age;
    int stroke_age;
    int lung_cancer_age;

    // Death
    bool alive;
    DeathType death_type;

    // Economics
    double utility;
    double cost;
};

extern "C" void InitializeConfig(SimulationConfig *out_config);

extern "C" Size InitializeHumans(const SimulationConfig &config, HeapArray<Human> *out_humans);
extern "C" Size RunSimulationStep(const SimulationConfig &config, Span<const Human> humans,
                                  HeapArray<Human> *out_humans);
