// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "economics.hh"
#include "predict.hh"
#include "simulate.hh"
#include "tables.hh"
#include "../wrappers/pcg.hh"

void InitializeConfig(SimulationConfig *out_config)
{
    out_config->count = 20000;
    out_config->seed = 0;
    out_config->discount_rate = 0.04;

    out_config->predict_cvd = PredictCvdMode::Disabled;
    out_config->predict_lung_cancer = PredictLungCancerMode::Disabled;
}

static void InitializeHuman(const SimulationConfig &config, Size idx, Human *out_human)
{
    pcg32_srandom_r(&out_human->rand_evolution, config.seed, idx);
    pcg32_srandom_r(&out_human->rand_therapy, config.seed, idx);

    out_human->alive = true;

    out_human->age = 45;
    out_human->sex = pcg_RandomBool(&out_human->rand_evolution, 0.5) ? Sex::Male : Sex::Female;

    out_human->smoking_status = pcg_RandomBool(&out_human->rand_evolution,
                                               GetSmokingPrevalence(out_human->age, out_human->sex));
}

Size InitializeHumans(const SimulationConfig &config, HeapArray<Human> *out_humans)
{
    for (Size i = 0; i < config.count; i++) {
        Human *new_human = out_humans->AppendDefault();
        InitializeHuman(config, i, new_human);
    }

    return config.count;
}

static bool SimulateYear(const SimulationConfig &config, const Human &human, Human *out_human)
{
    *out_human = human;

    out_human->iteration++;
    if (!human.alive)
        return false;
    out_human->age++;

    // Smoking
    if (human.smoking_status &&
            pcg_RandomBool(&out_human->rand_evolution,
                           GetSmokingStopTrialProbability(human.age))) {
        double stop_probability = 0.04;
        {
            double p = pcg_RandomUniform(&out_human->rand_evolution, 0.0, 1.0);

            if ((p -= 0.269) < 0.0) {
                stop_probability *= 2.29;
            } else if ((p -= 0.208) < 0.0) {
                stop_probability *= 1.5;
            }
        }

        if (pcg_RandomBool(&out_human->rand_evolution, stop_probability)) {
            out_human->smoking_status = false;
            out_human->smoking_cessation_age = human.age;
        }
    }

    // Cardiac ischemia and stroke
    if (config.predict_cvd != PredictCvdMode::Disabled) {
        double treshold;
        switch (config.predict_cvd) {
            case PredictCvdMode::Disabled: { DebugAssert(false); } break;
            case PredictCvdMode::Framingham: { treshold = PredictFraminghamScore(human); } break;
            case PredictCvdMode::QRisk3: { treshold = PredictQRisk3(human); } break;
            // FIXME: HeartScore predicts death risk, fix predicting with
            // average mortality rate
            case PredictCvdMode::HeartScore: { treshold = PredictHeartScore(human) / 0.4; } break;
        }

        if (pcg_RandomBool(&out_human->rand_evolution, treshold)) {
            if (pcg_RandomBool(&out_human->rand_evolution, 0.5)) {
                // Acute cardiac ischemia
                if (!human.cardiac_ischemia_age) {
                    out_human->cardiac_ischemia_age = human.age;
                }
                if (pcg_RandomBool(&out_human->rand_evolution, 0.5)) {
                    out_human->alive = false;
                    out_human->death_type = DeathType::CardiacIschemia;
                }
            } else {
                // Stroke
                if (!human.stroke_age) {
                    out_human->stroke_age = human.age;
                }
                if (pcg_RandomBool(&out_human->rand_evolution, 0.3)) {
                    out_human->alive = false;
                    out_human->death_type = DeathType::Stroke;
                }
            }
        }
    }

    // Lung cancer
    if (config.predict_lung_cancer != PredictLungCancerMode::Disabled) {
        if (pcg_RandomBool(&out_human->rand_evolution, PredictLungCancer(human))) {
            if (!human.lung_cancer_age) {
                out_human->lung_cancer_age = human.age;
            }
        }
        if (human.lung_cancer_age && human.age - human.lung_cancer_age > 3) {
            out_human->alive = false;
            out_human->death_type = DeathType::LungCancer;
        }
    }

    // Other causes of death
    {
        unsigned int type_flags = UINT_MAX;

        if (config.predict_cvd != PredictCvdMode::Disabled) {
            type_flags &= ~((1 << (int)DeathType::CardiacIschemia) |
                            (1 << (int)DeathType::Stroke));
        }
        if (config.predict_lung_cancer != PredictLungCancerMode::Disabled) {
            type_flags &= ~(1 << (int)DeathType::LungCancer);
        }

        double p = pcg_RandomUniform(&out_human->rand_evolution, 0.0, 1.0);
        if (p < GetDeathProbability(human.age, human.sex, type_flags)) {
            out_human->alive = false;

            // Assign OtherCauses in case the loop fails due to rounding
            out_human->death_type = DeathType::OtherCauses;
            for (Size i = 0; i < ARRAY_SIZE(DeathTypeNames); i++) {
                if (type_flags & (1 << i)) {
                    p -= GetDeathProbability(human.age, human.sex, 1 << i);
                    if (p <= 0.0) {
                        out_human->death_type = (DeathType)i;
                        break;
                    }
                }
            }
        }
    }

    // Economics
    {
        double discount_factor = std::pow(1.0 - config.discount_rate, human.iteration);

        out_human->utility += ComputeUtility(human) * discount_factor;
        out_human->cost += 0.0 * discount_factor;
    }

    return true;
}

Size RunSimulationStep(const SimulationConfig &config, Span<const Human> humans,
                       HeapArray<Human> *out_humans)
{
    Size alive_count = 0;

    // Loop with copying, because we want to support overwriting a human
    for (Human human: humans) {
        Human *new_human = out_humans->AppendDefault();
        alive_count += SimulateYear(config, human, new_human);
    }

    // Return alive count (everyone for now)
    return alive_count;
}
