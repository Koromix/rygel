// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let ems = (function() {
    let self = this;

    this.testMobility = function(data) {
        if (data.ems_pratique_activites === null || data.ems_temps_assis_jour === null ||
                data.ems_assis_2h_continu === null)
            return makeTestResult(null);

        let sedentary = data.ems_temps_assis_jour >= 7 * 60 ||
                        data.ems_assis_2h_continu;

        let activity_score = 0;
        for (let i = 1; i <= 10; i++) {
            if (data[`ems_act${i}`] === null)
                continue;

            let intensity = data[`ems_act${i}_intensite`];
            let duration = data[`ems_act${i}_temps`];
            let frequency = data[`ems_act${i}_freq`];

            if (intensity == 3 && duration >= 20) {
                if (frequency >= 3) {
                    activity_score += 4;
                } else if (frequency >= 1) {
                    activity_score += 2;
                }
            } else if (intensity == 2 && duration >= 30) {
                if (frequency >= 5) {
                    activity_score += 4;
                } else if (frequency >= 3) {
                    activity_score += 2;
                } else if (frequency >= 1) {
                    activity_score += 1;
                }
            }
        }

        if (activity_score < 4 || sedentary) {
            return makeTestResult(TestScore.Fragile);
        } else {
            return makeTestResult(TestScore.Good);
        }
    };

    this.testStrength = function(data) {
        if (data.consultant_sexe === null || data.ems_test_handgrip === null || data.demo_dxa_indice_mm === null ||
                data.ems_test_vit4m === null)
            return makeTestResult(null);

        switch (data.consultant_sexe) {
            case 'M': {
                if (data.ems_test_vit4m <= 0.8) {
                    return makeTestResult(TestScore.Bad);
                } else if (data.ems_test_handgrip < 27 || data.demo_dxa_indice_mm <= 7.23) {
                    return makeTestResult(TestScore.Fragile);
                } else {
                    return makeTestResult(TestScore.Good);
                }
            } break;

            case 'F': {
                if (data.ems_test_vit4m <= 0.8) {
                    return makeTestResult(TestScore.Bad);
                } else if (data.ems_test_handgrip < 16 || data.demo_dxa_indice_mm <= 5.67) {
                    return makeTestResult(TestScore.Fragile);
                } else {
                    return makeTestResult(TestScore.Good);
                }
            } break;
        }
    };

    this.testFractureRisk = function(data) {
        if (data.ems_test_unipod === null || (data.ems_test_timeup === null && data.ems_test_getup === null) ||
                data.demo_dmo_rachis === null || data.demo_dmo_col === null)
            return makeTestResult(null);

        let dmo_min = Math.min(data.demo_dmo_rachis, data.demo_dmo_col);

        if (data.ems_test_getup !== null) {
            // New version (2019+)
            if (data.ems_test_unipod < 5 || data.ems_test_getup >= 3 || dmo_min <= -2.5) {
                return makeTestResult(TestScore.Bad);
            } else if (data.ems_test_unipod < 30 || data.ems_test_getup == 2 || dmo_min <= -1.0) {
                return makeTestResult(TestScore.Fragile);
            } else {
                return makeTestResult(TestScore.Good);
            }
        } else {
            // Old version (2018)
            if (data.ems_test_unipod < 5 || data.ems_test_timeup >= 14 || dmo_min <= -2.5) {
                return makeTestResult(TestScore.Bad);
            } else if (data.ems_test_unipod < 30 || dmo_min <= -1.0) {
                return makeTestResult(TestScore.Fragile);
            } else {
                return makeTestResult(TestScore.Good);
            }
        }
    };

    this.testAll = function(data) {
        let score = Math.min(self.testMobility(data).score, self.testStrength(data).score,
                             self.testFractureRisk(data).score);
        return makeTestResult(score);
    };

    return this;
}).call({});
