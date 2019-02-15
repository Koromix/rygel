// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let ems = (function() {
    let self = this;

    this.screenMobility = function(data) {
        // TODO: Check for missing activity info
        if (data.ems_temps_assis_jour === null || data.ems_assis_2h_continu === null)
            return null;

        let sedentary = data.ems_temps_assis_jour >= 7 * 60 ||
                        data.ems_assis_2h_continu;

        let activity_score = 0;
        for (let i = 1; i <= 10; i++) {
            if (data[`ems_act${i}`] === null)
                continue;

            let intensity = data[`ems_act${i}_intensite`];
            let duration = data[`ems_act${i}_duree`];
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
            return ScreeningResult.Fragile;
        } else {
            return ScreeningResult.Good;
        }
    };

    this.screenStrength = function(data) {
        if (data.cs_sexe === null || data.ems_handgrip === null || data.rx_dxa === null ||
                data.ems_vit4m === null)
            return null;

        switch (data.cs_sexe) {
            case 'M': {
                if (data.ems_vit4m <= 0.8) {
                    return ScreeningResult.Bad;
                } else if (data.ems_handgrip < 27 || data.rx_dxa <= 7.23) {
                    return ScreeningResult.Fragile;
                } else {
                    return ScreeningResult.Good;
                }
            } break;

            case 'F': {
                if (data.ems_vit4m <= 0.8) {
                    return ScreeningResult.Bad;
                } else if (data.ems_handgrip < 16 || data.rx_dxa <= 5.67) {
                    return ScreeningResult.Fragile;
                } else {
                    return ScreeningResult.Good;
                }
            } break;
        }
    };

    this.screenFractureRisk = function(data) {
        if (data.ems_unipod === null || (data.ems_tug === null && data.ems_gug === null) ||
                data.rx_dmo_rachis === null || data.rx_dmo_col === null)
            return null;

        let dmo_min = Math.min(data.rx_dmo_rachis, data.rx_dmo_col);

        if (data.ems_gug !== null) {
            // New version (2019+)
            if (data.ems_unipod < 5 || data.ems_gug >= 3 || dmo_min <= -2.5) {
                return ScreeningResult.Bad;
            } else if (data.ems_unipod < 30 || data.ems_gug == 2 || dmo_min <= -1.0) {
                return ScreeningResult.Fragile;
            } else {
                return ScreeningResult.Good;
            }
        } else {
            // Old version (2018)
            if (data.ems_unipod < 5 || data.ems_tug >= 14 || dmo_min <= -2.5) {
                return ScreeningResult.Bad;
            } else if (data.ems_unipod < 30 || dmo_min <= -1.0) {
                return ScreeningResult.Fragile;
            } else {
                return ScreeningResult.Good;
            }
        }
    };

    this.screenAll = function(data) {
        return Math.min(self.screenMobility(data), self.screenStrength(data),
                        self.screenFractureRisk(data)) || null;
    };

    return this;
}).call({});
