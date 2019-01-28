// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "human.hh"
#include "cardiovascular.hh"
#include "death.hh"
#include "random.hh"

Human CreateHuman(int id)
{
    Human human;

    // Identifier
    human.id = id;

    // Socio-demographic
    human.sex = RandomBool(0.5) ? Sex::Male : Sex::Female;
    human.age = 45;
    human.sociocultural_level = RandomIntUniform(1, 4);

    // CV risk factors
    human.smoking_status = !RandomBool(SmokingGetPrevalence(human.age, human.sex));
    human.smoking_cessation_age = 0;
    human.bmi = BmiGetFirst(human.age, human.sex);
    human.systolic_pressure = SystolicPressureGetFirst(human.age, human.sex);
    human.total_cholesterol = CholesterolGetFirst(human.age, human.sex);

    // Diseases
    human.stroke_happened = false;
    human.stroke_age = 0;

    // Death
    human.death_happened = false;

    return human;
}

Human SimulateOneYear(const Human &prev)
{
    DebugAssert(!prev.death_happened);

    Human next;

    // Identifier
    next.id = prev.id;

    // Socio-demographic
    next.age = prev.age + 1;
    next.sex = prev.sex;
    next.sociocultural_level = prev.sociocultural_level;

    // CV risk factors
    if (prev.smoking_status && !RandomBool(SmokingGetCessationProbability(prev.age, prev.sex))) {
        next.smoking_status = false;
        next.smoking_cessation_age = next.age;
    } else {
        next.smoking_status = prev.smoking_status;
        next.smoking_cessation_age = 0;
    }
    next.bmi = prev.bmi + BmiGetEvolution(prev.age, prev.sex);
    next.systolic_pressure = prev.systolic_pressure + SystolicPressureGetEvolution(prev.age, prev.sex);
    next.total_cholesterol = prev.total_cholesterol + CholesterolGetEvolution(prev.age, prev.sex);

    // Diseases
    next.stroke_happened = prev.stroke_happened;
    next.stroke_age = prev.stroke_age;

    // Death
    {
        unsigned int death_flags = UINT_MAX &
                                   ~(int)DeathFlag::Cardiovascular;
        double death_probability = ScoreComputeProbability(next) +
                                   GetDeathProbability(next.age, next.sex, death_flags);

        next.death_happened = !RandomBool(death_probability);
    }

    return next;
}
