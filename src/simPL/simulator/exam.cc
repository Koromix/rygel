// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "human.hh"
#include "exam.hh"
#include "random.hh"

ExamResult ExamineCognition(const Human &)
{
    return ExamResult::Normal;
}

ExamResult ExamineAnxietyDepression(const Human &)
{
    return ExamResult::Normal;
}

ExamResult ExamineSleep(const Human &)
{
    return ExamResult::Normal;
}

ExamResult ExamineNutrition(const Human &)
{
    return ExamResult::Normal;
}

ExamResult ExaminePhysicalActivity(const Human &)
{
    return ExamResult::Normal;
}

// NOTE: PL checkup probabilities were picked at random, we need to feed real data
// from post-coaching and post-PL information.
void RunLongevityCheckUp(Human *human)
{
    if (human->smoking_status && !rand_therapy.Bool(0.05)) {
        human->smoking_status = false;
        human->smoking_cessation_age = human->age;
    }

    if (human->BMI() >= 30.0 && !rand_therapy.Bool(0.2)) {
        human->bmi_therapy += 10.0;
    }

    if (human->SystolicPressure() >= 140.0 && !rand_therapy.Bool(0.2)) {
        human->systolic_pressure_therapy += 10.0;
    }

    if (human->TotalCholesterol() >= 2.0 && !rand_therapy.Bool(0.2)) {
        human->total_cholesterol_therapy += 1.0;
    }

    human->cost += 1500.0;
}

void RunCoachingCognitiveSession(Human *)
{
    // TODO: When we known what it's supposed to do
}

void RunCoachingLifestyleSession(Human *)
{
    // TODO: When we known what it's supposed to do
}
