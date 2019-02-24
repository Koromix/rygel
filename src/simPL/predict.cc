// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libcc/libcc.hh"
#include "simulation.hh"
#include "predict.hh"

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
    if (human.TotalCholesterol() >= 7.5) cholesterol_cat = 4;
    else if (human.TotalCholesterol() >= 6.5) cholesterol_cat = 3;
    else if (human.TotalCholesterol() >= 5.5) cholesterol_cat = 2;
    else if (human.TotalCholesterol() >= 4.5) cholesterol_cat = 1;
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

double ComputeScoreProbability(const Human &human)
{
    double score10 = ComputeScore10(human);
    // TODO: Check this is correct!
    double score = 1.0 - pow(1.0 - score10, 0.1);

    return score;
}
