// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "simulate.hh"
#include "predict.hh"

// Implementation is in predict_cv_qrisk3.cc for license reasons (LGPL3)
double ComputeQRisk3Male10(int age, bool AF, bool atypicalantipsy, bool corticosteroids,
                           bool impotence2, bool migraine, bool ra, bool renal, bool semi,
                           bool sle, bool treatedhyp, bool type1, bool type2, double bmi,
                           int ethrisk, int fh_cvd, double rati, double sbp, double sbps5,
                           int smoke_cat, int surv, double town);
double ComputeQRisk3Female10(int age, bool AF, bool atypicalantipsy, bool corticosteroids,
                             bool migraine, bool ra, bool renal, bool semi, bool sle,
                             bool treatedhyp, bool type1, bool type2, double bmi, int ethrisk,
                             int fh_cvd, double rati, double sbp, double sbps5, int smoke_cat,
                             int surv, double town);

static double ComputeFramingham10(const Human &human)
{
    int points = 0;

    switch (human.sex) {
        case Sex::Male: {
            if (human.age < 35) points += 0; // Age >= 30
            else if (human.age < 40) points += 2;
            else if (human.age < 45) points += 5;
            else if (human.age < 50) points += 6;
            else if (human.age < 55) points += 8;
            else if (human.age < 60) points += 10;
            else if (human.age < 65) points += 11;
            else if (human.age < 70) points += 12;
            else points += 14;// Age < 75
        } break;

        case Sex::Female: {
            if (human.age < 35) points += 0; // Age >= 30
            else if (human.age < 40) points += 2;
            else if (human.age < 45) points += 4;
            else if (human.age < 50) points += 5;
            else if (human.age < 55) points += 7;
            else if (human.age < 60) points += 8;
            else if (human.age < 65) points += 9;
            else if (human.age < 70) points += 10;
            else points += 11;// Age < 75
        } break;
    }

    if (human.BMI() < 25) points += 0; // BMI >= 15
    else if (human.BMI() < 30) points += 1;
    else points += 2; // BMI < 50

    if (human.HDL() >= 60) points += -2; // HDL < 100
    else if (human.HDL() >= 50) points += -1;
    else if (human.HDL() >= 45) points += 0;
    else if (human.HDL() >= 35) points += 1;
    else points += 2; // HDL >= 10

    switch (human.sex) {
        case Sex::Male: {
            if (human.TotalCholesterol() < 160) points += 0; // TotalCholesterol >= 100
            else if (human.TotalCholesterol() < 200) points += 1;
            else if (human.TotalCholesterol() < 240) points += 2;
            else if (human.TotalCholesterol() < 280) points += 3;
            else points += 4; // TotalCholesterol < 405
        } break;

        case Sex::Female: {
            if (human.TotalCholesterol() < 160) points += 0; // TotalCholesterol >= 100
            else if (human.TotalCholesterol() < 200) points += 1;
            else if (human.TotalCholesterol() < 240) points += 3;
            else if (human.TotalCholesterol() < 280) points += 4;
            else points += 5; // TotalCholesterol < 405
        } break;
    }

    switch (human.sex) {
        case Sex::Male: {
            if (!human.systolic_pressure_drugs) {
                if (human.SystolicPressure() < 120) points += -2; // SystolicPressure >= 90
                else if (human.SystolicPressure() < 130) points += 0;
                else if (human.SystolicPressure() < 140) points += 1;
                else if (human.SystolicPressure() < 160) points += 2;
                else points += 3; // SystolicPressure < 200
            } else {
                if (human.SystolicPressure() < 120) points += 0; // SystolicPressure >= 90
                else if (human.SystolicPressure() < 130) points += 2;
                else if (human.SystolicPressure() < 140) points += 3;
                else if (human.SystolicPressure() < 160) points += 4;
                else points += 5; // SystolicPressure < 200
            }
        } break;

        case Sex::Female: {
            if (!human.systolic_pressure_drugs) {
                if (human.SystolicPressure() < 120) points += -3; // SystolicPressure >= 90
                else if (human.SystolicPressure() < 130) points += 0;
                else if (human.SystolicPressure() < 140) points += 1;
                else if (human.SystolicPressure() < 150) points += 2;
                else if (human.SystolicPressure() < 160) points += 4;
                else points += 5; // SystolicPressure < 200
            } else {
                if (human.SystolicPressure() < 120) points += -1; // SystolicPressure >= 90
                else if (human.SystolicPressure() < 130) points += 2;
                else if (human.SystolicPressure() < 140) points += 3;
                else if (human.SystolicPressure() < 150) points += 5;
                else if (human.SystolicPressure() < 160) points += 6;
                else points += 7; // SystolicPressure < 200
            }
        } break;
    }

    switch (human.sex) {
        case Sex::Male: { points += 4 * human.smoking_status; } break;
        case Sex::Female: { points += 3 * human.smoking_status; } break;
    }

    switch (human.sex) {
        case Sex::Male: { points += 3 * human.diabetes_status; } break;
        case Sex::Female: { points += 4 * human.diabetes_status; } break;
    }

    switch(human.sex) {
        case Sex::Male: {
            switch (points) {
                case -8: return 0.0;
                case -7: return 0.0;
                case -6: return 0.0;
                case -5: return 0.0;
                case -4: return 0.011;
                case -3: return 0.014;
                case -2: return 0.016;
                case -1: return 0.019;
                case 0: return 0.023;
                case 1: return 0.028;
                case 2: return 0.033;
                case 3: return 0.04;
                case 4: return 0.047;
                case 5: return 0.056;
                case 6: return 0.067;
                case 7: return 0.08;
                case 8: return 0.095;
                case 9: return 0.112;
                case 10: return 0.133;
                case 11: return 0.157;
                case 12: return 0.18;
                case 13: return 0.217;
                case 14: return 0.254;
                case 15: return 0.296;
                default: return 0.3; // TODO: Use extrapolation for extra values (up to 38)?
            }
        } break;

        case Sex::Female: {
            switch (points) {
                case -6: return 0.0;
                case -5: return 0.0;
                case -4: return 0.0;
                case -3: return 0.0;
                case -2: return 0.0;
                case -1: return 0.01;
                case 0: return 0.011;
                case 1: return 0.015;
                case 2: return 0.018;
                case 3: return 0.021;
                case 4: return 0.025;
                case 5: return 0.029;
                case 6: return 0.034;
                case 7: return 0.039;
                case 8: return 0.046;
                case 9: return 0.054;
                case 10: return 0.063;
                case 11: return 0.074;
                case 12: return 0.086;
                case 13: return 0.1;
                case 14: return 0.116;
                case 15: return 0.135;
                case 16: return 0.156;
                case 17: return 0.181;
                case 18: return 0.209;
                case 19: return 0.24;
                case 20: return 0.275;
                default: return 0.3; // TODO: Use extrapolation for extra values (up to 38)?
            }
        } break;
    }

    Assert(false);
}

static double ComputeScore10(const Human &human)
{
    int8_t age_cat;
    if (human.age >= 63) age_cat = 4;
    else if (human.age >= 58) age_cat = 3;
    else if (human.age >= 53) age_cat = 2;
    else if (human.age >= 45) age_cat = 1;
    else age_cat = 0;

    int8_t sbp_cat;
    if (human.SystolicPressure() >= 170) sbp_cat = 3;
    else if (human.SystolicPressure() >= 150) sbp_cat = 2;
    else if (human.SystolicPressure() >= 130) sbp_cat = 1;
    else sbp_cat = 0;

    int8_t cholesterol_cat;
    if (human.TotalCholesterol() >= 7.5 * 38.67) cholesterol_cat = 4;
    else if (human.TotalCholesterol() >= 6.5 * 38.67) cholesterol_cat = 3;
    else if (human.TotalCholesterol() >= 5.5 * 38.67) cholesterol_cat = 2;
    else if (human.TotalCholesterol() >= 4.5 * 38.67) cholesterol_cat = 1;
    else cholesterol_cat = 0;

    // NOTE: I can't find information about what you're supposed to do
    // with ex-smokers in the HeartScore score.
    bool smoker = human.smoking_status ||
                  (human.age - human.smoking_cessation_age) < 3;

    if (human.sex == Sex::Female && !smoker) {
        switch ((age_cat << 16) | (sbp_cat << 8) | cholesterol_cat) {
            case 0x00000000: return 0.00;
            case 0x00000001: return 0.00;
            case 0x00000002: return 0.00;
            case 0x00000003: return 0.00;
            case 0x00000004: return 0.00;
            case 0x00000100: return 0.00;
            case 0x00000101: return 0.00;
            case 0x00000102: return 0.00;
            case 0x00000103: return 0.00;
            case 0x00000104: return 0.00;
            case 0x00000200: return 0.00;
            case 0x00000201: return 0.00;
            case 0x00000202: return 0.00;
            case 0x00000203: return 0.00;
            case 0x00000204: return 0.00;
            case 0x00000300: return 0.00;
            case 0x00000301: return 0.00;
            case 0x00000302: return 0.00;
            case 0x00000303: return 0.00;
            case 0x00000304: return 0.00;

            case 0x00010000: return 0.00;
            case 0x00010001: return 0.00;
            case 0x00010002: return 0.01;
            case 0x00010003: return 0.01;
            case 0x00010004: return 0.01;
            case 0x00010100: return 0.00;
            case 0x00010101: return 0.01;
            case 0x00010102: return 0.01;
            case 0x00010103: return 0.01;
            case 0x00010104: return 0.01;
            case 0x00010200: return 0.01;
            case 0x00010201: return 0.01;
            case 0x00010202: return 0.01;
            case 0x00010203: return 0.01;
            case 0x00010204: return 0.01;
            case 0x00010300: return 0.01;
            case 0x00010301: return 0.01;
            case 0x00010302: return 0.01;
            case 0x00010303: return 0.02;
            case 0x00010304: return 0.02;

            case 0x00020000: return 0.01;
            case 0x00020001: return 0.01;
            case 0x00020002: return 0.01;
            case 0x00020003: return 0.01;
            case 0x00020004: return 0.01;
            case 0x00020100: return 0.01;
            case 0x00020101: return 0.01;
            case 0x00020102: return 0.01;
            case 0x00020103: return 0.01;
            case 0x00020104: return 0.02;
            case 0x00020200: return 0.01;
            case 0x00020201: return 0.02;
            case 0x00020202: return 0.02;
            case 0x00020203: return 0.02;
            case 0x00020204: return 0.03;
            case 0x00020300: return 0.02;
            case 0x00020301: return 0.02;
            case 0x00020302: return 0.03;
            case 0x00020303: return 0.03;
            case 0x00020304: return 0.04;

            case 0x00030000: return 0.01;
            case 0x00030001: return 0.01;
            case 0x00030002: return 0.02;
            case 0x00030003: return 0.02;
            case 0x00030004: return 0.02;
            case 0x00030100: return 0.02;
            case 0x00030101: return 0.02;
            case 0x00030102: return 0.02;
            case 0x00030103: return 0.03;
            case 0x00030104: return 0.03;
            case 0x00030200: return 0.03;
            case 0x00030201: return 0.03;
            case 0x00030202: return 0.03;
            case 0x00030203: return 0.04;
            case 0x00030204: return 0.05;
            case 0x00030300: return 0.04;
            case 0x00030301: return 0.04;
            case 0x00030302: return 0.05;
            case 0x00030303: return 0.06;
            case 0x00030304: return 0.07;

            case 0x00040000: return 0.02;
            case 0x00040001: return 0.02;
            case 0x00040002: return 0.03;
            case 0x00040003: return 0.03;
            case 0x00040004: return 0.04;
            case 0x00040100: return 0.03;
            case 0x00040101: return 0.03;
            case 0x00040102: return 0.04;
            case 0x00040103: return 0.05;
            case 0x00040104: return 0.06;
            case 0x00040200: return 0.05;
            case 0x00040201: return 0.05;
            case 0x00040202: return 0.06;
            case 0x00040203: return 0.07;
            case 0x00040204: return 0.08;
            case 0x00040300: return 0.07;
            case 0x00040301: return 0.08;
            case 0x00040302: return 0.09;
            case 0x00040303: return 0.10;
            case 0x00040304: return 0.12;
        }
    } else if (human.sex == Sex::Female && smoker) {
        switch ((age_cat << 16) | (sbp_cat << 8) | cholesterol_cat) {
            case 0x00000000: return 0.00;
            case 0x00000001: return 0.00;
            case 0x00000002: return 0.00;
            case 0x00000003: return 0.00;
            case 0x00000004: return 0.00;
            case 0x00000100: return 0.00;
            case 0x00000101: return 0.00;
            case 0x00000102: return 0.00;
            case 0x00000103: return 0.00;
            case 0x00000104: return 0.00;
            case 0x00000200: return 0.00;
            case 0x00000201: return 0.00;
            case 0x00000202: return 0.00;
            case 0x00000203: return 0.00;
            case 0x00000204: return 0.00;
            case 0x00000300: return 0.00;
            case 0x00000301: return 0.00;
            case 0x00000302: return 0.00;
            case 0x00000303: return 0.01;
            case 0x00000304: return 0.01;

            case 0x00010000: return 0.01;
            case 0x00010001: return 0.01;
            case 0x00010002: return 0.01;
            case 0x00010003: return 0.01;
            case 0x00010004: return 0.01;
            case 0x00010100: return 0.01;
            case 0x00010101: return 0.01;
            case 0x00010102: return 0.01;
            case 0x00010103: return 0.01;
            case 0x00010104: return 0.02;
            case 0x00010200: return 0.01;
            case 0x00010201: return 0.02;
            case 0x00010202: return 0.02;
            case 0x00010203: return 0.02;
            case 0x00010204: return 0.03;
            case 0x00010300: return 0.02;
            case 0x00010301: return 0.02;
            case 0x00010302: return 0.03;
            case 0x00010303: return 0.03;
            case 0x00010304: return 0.04;

            case 0x00020000: return 0.01;
            case 0x00020001: return 0.01;
            case 0x00020002: return 0.02;
            case 0x00020003: return 0.02;
            case 0x00020004: return 0.02;
            case 0x00020100: return 0.02;
            case 0x00020101: return 0.02;
            case 0x00020102: return 0.02;
            case 0x00020103: return 0.03;
            case 0x00020104: return 0.03;
            case 0x00020200: return 0.03;
            case 0x00020201: return 0.03;
            case 0x00020202: return 0.04;
            case 0x00020203: return 0.04;
            case 0x00020204: return 0.05;
            case 0x00020300: return 0.04;
            case 0x00020301: return 0.05;
            case 0x00020302: return 0.05;
            case 0x00020303: return 0.06;
            case 0x00020304: return 0.07;

            case 0x00030000: return 0.02;
            case 0x00030001: return 0.03;
            case 0x00030002: return 0.03;
            case 0x00030003: return 0.04;
            case 0x00030004: return 0.04;
            case 0x00030100: return 0.03;
            case 0x00030101: return 0.04;
            case 0x00030102: return 0.05;
            case 0x00030103: return 0.05;
            case 0x00030104: return 0.06;
            case 0x00030200: return 0.05;
            case 0x00030201: return 0.06;
            case 0x00030202: return 0.07;
            case 0x00030203: return 0.08;
            case 0x00030204: return 0.09;
            case 0x00030300: return 0.08;
            case 0x00030301: return 0.09;
            case 0x00030302: return 0.10;
            case 0x00030303: return 0.11;
            case 0x00030304: return 0.13;

            case 0x00040000: return 0.04;
            case 0x00040001: return 0.05;
            case 0x00040002: return 0.05;
            case 0x00040003: return 0.06;
            case 0x00040004: return 0.07;
            case 0x00040100: return 0.06;
            case 0x00040101: return 0.07;
            case 0x00040102: return 0.08;
            case 0x00040103: return 0.09;
            case 0x00040104: return 0.11;
            case 0x00040200: return 0.09;
            case 0x00040201: return 0.10;
            case 0x00040202: return 0.12;
            case 0x00040203: return 0.13;
            case 0x00040204: return 0.16;
            case 0x00040300: return 0.13;
            case 0x00040301: return 0.15;
            case 0x00040302: return 0.17;
            case 0x00040303: return 0.19;
            case 0x00040304: return 0.22;
        }
    } else if (human.sex == Sex::Male && !smoker) {
        switch ((age_cat << 16) | (sbp_cat << 8) | cholesterol_cat) {
            case 0x00000000: return 0.00;
            case 0x00000001: return 0.00;
            case 0x00000002: return 0.01;
            case 0x00000003: return 0.01;
            case 0x00000004: return 0.01;
            case 0x00000100: return 0.00;
            case 0x00000101: return 0.01;
            case 0x00000102: return 0.01;
            case 0x00000103: return 0.01;
            case 0x00000104: return 0.01;
            case 0x00000200: return 0.01;
            case 0x00000201: return 0.01;
            case 0x00000202: return 0.01;
            case 0x00000203: return 0.01;
            case 0x00000204: return 0.01;
            case 0x00000300: return 0.01;
            case 0x00000301: return 0.01;
            case 0x00000302: return 0.01;
            case 0x00000303: return 0.02;
            case 0x00000304: return 0.02;

            case 0x00010000: return 0.01;
            case 0x00010001: return 0.01;
            case 0x00010002: return 0.02;
            case 0x00010003: return 0.02;
            case 0x00010004: return 0.02;
            case 0x00010100: return 0.02;
            case 0x00010101: return 0.02;
            case 0x00010102: return 0.02;
            case 0x00010103: return 0.03;
            case 0x00010104: return 0.03;
            case 0x00010200: return 0.02;
            case 0x00010201: return 0.03;
            case 0x00010202: return 0.03;
            case 0x00010203: return 0.04;
            case 0x00010204: return 0.05;
            case 0x00010300: return 0.04;
            case 0x00010301: return 0.04;
            case 0x00010302: return 0.05;
            case 0x00010303: return 0.06;
            case 0x00010304: return 0.07;

            case 0x00020000: return 0.02;
            case 0x00020001: return 0.02;
            case 0x00020002: return 0.03;
            case 0x00020003: return 0.03;
            case 0x00020004: return 0.04;
            case 0x00020100: return 0.03;
            case 0x00020101: return 0.03;
            case 0x00020102: return 0.04;
            case 0x00020103: return 0.05;
            case 0x00020104: return 0.06;
            case 0x00020200: return 0.04;
            case 0x00020201: return 0.05;
            case 0x00020202: return 0.06;
            case 0x00020203: return 0.07;
            case 0x00020204: return 0.08;
            case 0x00020300: return 0.06;
            case 0x00020301: return 0.07;
            case 0x00020302: return 0.08;
            case 0x00020303: return 0.10;
            case 0x00020304: return 0.12;

            case 0x00030000: return 0.03;
            case 0x00030001: return 0.03;
            case 0x00030002: return 0.04;
            case 0x00030003: return 0.05;
            case 0x00030004: return 0.06;
            case 0x00030100: return 0.04;
            case 0x00030101: return 0.05;
            case 0x00030102: return 0.06;
            case 0x00030103: return 0.07;
            case 0x00030104: return 0.09;
            case 0x00030200: return 0.06;
            case 0x00030201: return 0.07;
            case 0x00030202: return 0.09;
            case 0x00030203: return 0.10;
            case 0x00030204: return 0.12;
            case 0x00030300: return 0.09;
            case 0x00030301: return 0.11;
            case 0x00030302: return 0.13;
            case 0x00030303: return 0.15;
            case 0x00030304: return 0.18;

            case 0x00040000: return 0.04;
            case 0x00040001: return 0.05;
            case 0x00040002: return 0.06;
            case 0x00040003: return 0.07;
            case 0x00040004: return 0.09;
            case 0x00040100: return 0.06;
            case 0x00040101: return 0.08;
            case 0x00040102: return 0.09;
            case 0x00040103: return 0.11;
            case 0x00040104: return 0.13;
            case 0x00040200: return 0.09;
            case 0x00040201: return 0.11;
            case 0x00040202: return 0.13;
            case 0x00040203: return 0.15;
            case 0x00040204: return 0.16;
            case 0x00040300: return 0.14;
            case 0x00040301: return 0.16;
            case 0x00040302: return 0.19;
            case 0x00040303: return 0.22;
            case 0x00040304: return 0.26;
        }
    } else if (human.sex == Sex::Male && smoker) {
        switch ((age_cat << 16) | (sbp_cat << 8) | cholesterol_cat) {
            case 0x00000000: return 0.01;
            case 0x00000001: return 0.01;
            case 0x00000002: return 0.01;
            case 0x00000003: return 0.01;
            case 0x00000004: return 0.01;
            case 0x00000100: return 0.01;
            case 0x00000101: return 0.01;
            case 0x00000102: return 0.01;
            case 0x00000103: return 0.02;
            case 0x00000104: return 0.02;
            case 0x00000200: return 0.01;
            case 0x00000201: return 0.02;
            case 0x00000202: return 0.02;
            case 0x00000203: return 0.02;
            case 0x00000204: return 0.03;
            case 0x00000300: return 0.02;
            case 0x00000301: return 0.02;
            case 0x00000302: return 0.03;
            case 0x00000303: return 0.03;
            case 0x00000304: return 0.04;

            case 0x00010000: return 0.02;
            case 0x00010001: return 0.03;
            case 0x00010002: return 0.03;
            case 0x00010003: return 0.04;
            case 0x00010004: return 0.05;
            case 0x00010100: return 0.03;
            case 0x00010101: return 0.04;
            case 0x00010102: return 0.05;
            case 0x00010103: return 0.06;
            case 0x00010104: return 0.07;
            case 0x00010200: return 0.05;
            case 0x00010201: return 0.06;
            case 0x00010202: return 0.07;
            case 0x00010203: return 0.08;
            case 0x00010204: return 0.10;
            case 0x00010300: return 0.07;
            case 0x00010301: return 0.08;
            case 0x00010302: return 0.10;
            case 0x00010303: return 0.12;
            case 0x00010304: return 0.14;

            case 0x00020000: return 0.04;
            case 0x00020001: return 0.04;
            case 0x00020002: return 0.05;
            case 0x00020003: return 0.06;
            case 0x00020004: return 0.08;
            case 0x00020100: return 0.05;
            case 0x00020101: return 0.06;
            case 0x00020102: return 0.08;
            case 0x00020103: return 0.09;
            case 0x00020104: return 0.11;
            case 0x00020200: return 0.08;
            case 0x00020201: return 0.09;
            case 0x00020202: return 0.11;
            case 0x00020203: return 0.13;
            case 0x00020204: return 0.16;
            case 0x00020300: return 0.12;
            case 0x00020301: return 0.13;
            case 0x00020302: return 0.16;
            case 0x00020303: return 0.19;
            case 0x00020304: return 0.22;

            case 0x00030000: return 0.06;
            case 0x00030001: return 0.07;
            case 0x00030002: return 0.08;
            case 0x00030003: return 0.10;
            case 0x00030004: return 0.12;
            case 0x00030100: return 0.08;
            case 0x00030101: return 0.10;
            case 0x00030102: return 0.12;
            case 0x00030103: return 0.14;
            case 0x00030104: return 0.17;
            case 0x00030200: return 0.12;
            case 0x00030201: return 0.14;
            case 0x00030202: return 0.17;
            case 0x00030203: return 0.20;
            case 0x00030204: return 0.24;
            case 0x00030300: return 0.18;
            case 0x00030301: return 0.21;
            case 0x00030302: return 0.24;
            case 0x00030303: return 0.28;
            case 0x00030304: return 0.33;

            case 0x00040000: return 0.09;
            case 0x00040001: return 0.10;
            case 0x00040002: return 0.12;
            case 0x00040003: return 0.14;
            case 0x00040004: return 0.17;
            case 0x00040100: return 0.13;
            case 0x00040101: return 0.15;
            case 0x00040102: return 0.17;
            case 0x00040103: return 0.20;
            case 0x00040104: return 0.24;
            case 0x00040200: return 0.18;
            case 0x00040201: return 0.21;
            case 0x00040202: return 0.25;
            case 0x00040203: return 0.29;
            case 0x00040204: return 0.34;
            case 0x00040300: return 0.26;
            case 0x00040301: return 0.30;
            case 0x00040302: return 0.35;
            case 0x00040303: return 0.41;
            case 0x00040304: return 0.47;
        }
    }

    return NAN;
}

// FIXME: This is probably a broken way to annualize risk scores
static double AnnualizePrediction(double score, int years)
{
    return 1.0 - pow(1.0 - score, 1.0 / (double)years);
}

double PredictFraminghamScore(const Human &human)
{
    double score10 = ComputeFramingham10(human);
    return AnnualizePrediction(score10, 10);
}

double PredictHeartScore(const Human &human)
{
    double score10 = ComputeScore10(human);
    return AnnualizePrediction(score10, 10);
}

// FIXME: Use population values for unknown variables?
double PredictQRisk3(const Human &human)
{
    int smoke_cat;
    if (human.smoking_status) smoke_cat = 3;
    else if (human.smoking_cessation_age) smoke_cat = 1;
    else smoke_cat = 0;

    double score10 = NAN;
    switch (human.sex) {
        case Sex::Male: {
            score10 = ComputeQRisk3Male10(human.age, false, false, false, false, false, false, false,
                                          false, false, false, false, human.diabetes_status, human.BMI(),
                                          0, false, 4.300998687744141, human.SystolicPressure(),
                                          8.756621360778809, smoke_cat, 0, 0.526304900646210);
        } break;

        case Sex::Female: {
            score10 = ComputeQRisk3Female10(human.age, false, false, false, false, false, false,
                                            false, false, false, false, false /* d2 */, 30.0 /* bmi */,
                                            0, false, 3.476326465606690, 140.0 /* SBP */,
                                            9.002537727355957, smoke_cat, 0, 0.392308831214905);
        } break;
    }
    DebugAssert(!std::isnan(score10));

    return AnnualizePrediction(score10, 10);
}
