// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let nutrition = (function() {
    let self = this;

    // TODO: Check inequality operators (< / <=, etc.)
    this.screenDiversity = function(data) {
        if (data.rdv_age === null || data.diet_diversite === null || data.anthro_poids === null ||
                data.anthro_taille === null || data.diet_poids_6mois === null)
            return null;

        let relative_weight = data.anthro_poids / data.diet_poids_6mois - 1.0;
        let bmi = data.anthro_poids / Math.pow(data.anthro_taille / 100.0, 2);

        let bmi_type;
        if (data.rdv_age < 65) {
            if (bmi < 18.5) {
                bmi_type = 1;
            } else if (bmi < 25) {
                bmi_type = 2;
            } else if (bmi < 30) {
                bmi_type = 3;
            } else {
                bmi_type = 4;
            }
        } else {
            if (bmi < 21) {
                bmi_type = 1;
            } else if (bmi < 28) {
                bmi_type = 2;
            } else if (bmi < 33) {
                bmi_type = 3;
            } else {
                bmi_type = 4;
            }
        }

        if (relative_weight > 0.1) {
            switch (bmi_type) {
                case 1: return ScreeningResult.Bad;
                case 2: return ScreeningResult.Fragile;
                case 3: return ScreeningResult.Fragile;
                case 4: return ScreeningResult.Bad;
            }
        } else if (relative_weight < -0.1) {
            switch (bmi_type) {
                case 1: return ScreeningResult.Bad;
                case 2: return ScreeningResult.Fragile;
                case 3: return ScreeningResult.Fragile;
                case 4: return ScreeningResult.Fragile;
            }
        } else {
            if (data.diet_diversite == 0) {
                return ScreeningResult.Fragile;
            } else {
                return ScreeningResult.Good;
            }
        }
    };

    this.screenProteinIntake = function(data) {
        if (data.diet_proteines === null || data.cs_sexe === null || data.rx_dxa === null ||
                data.bio_albuminemie === null)
            return null;

        if (data.bio_albuminemie < 35) {
            return ScreeningResult.Bad;
        } else if (data.diet_proteines == 0) {
            return ScreeningResult.Fragile;
        } else {
            switch (data.cs_sexe) {
                case 'M': {
                    if (data.rx_dxa <= 7.23) {
                        return ScreeningResult.Fragile;
                    } else {
                        return ScreeningResult.Good;
                    }
                } break;
                case 'F': {
                    if (data.rx_dxa <= 5.67) {
                        return ScreeningResult.Fragile;
                    } else {
                        return ScreeningResult.Good;
                    }
                } break;
            }
        }
    };

    this.screenCalciumIntake = function(data) {
        if (data.diet_calcium === null)
            return null;

        if (data.diet_calcium == 0) {
            return ScreeningResult.Fragile;
        } else {
            return ScreeningResult.Good;
        }
    };

    this.screenBehavior = function(data) {
        if (data.diet_comportement1 === null || data.diet_comportement2 === null)
            return null;

        if (data.diet_comportement1 == 0 && data.diet_comportement2 == 1) {
            return ScreeningResult.Bad;
        } else if (data.diet_comportement1 == 0 && data.diet_comportement2 == 0) {
            return ScreeningResult.Fragile;
        } else {
            return ScreeningResult.Good;
        }
    };

    this.screenAll = function(data) {
        return Math.min(self.screenDiversity(data), self.screenProteinIntake(data),
                        self.screenCalciumIntake(data), self.screenBehavior(data)) || null;
    };

    return this;
}).call({});
