// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let nutrition = (function() {
    let self = this;

    // TODO: Check inequality operators (< / <=, etc.)
    this.testDiversity = function(data) {
        if (data.rdv_age === null || data.diet_diversite_alimentaire === null || data.constantes_poids === null ||
                data.constantes_taille === null || data.diet_poids_estime_6mois === null)
            return makeTestResult(null);

        let relative_weight = data.constantes_poids / data.diet_poids_estime_6mois - 1.0;
        let bmi = data.constantes_poids / Math.pow(data.constantes_taille / 100.0, 2);

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
                case 1: return makeTestResult(TestScore.Bad, 'alimentation non diversifiée');
                case 2: return makeTestResult(TestScore.Fragile, 'diversité alimentaire insuffisante');
                case 3: return makeTestResult(TestScore.Fragile, 'diversité insuffisante');
                case 4: return makeTestResult(TestScore.Bad, 'alimentation non diversifiée');
            }
        } else if (relative_weight < -0.1) {
            switch (bmi_type) {
                case 1: return makeTestResult(TestScore.Bad, 'alimentation non diversifiée');
                case 2: return makeTestResult(TestScore.Fragile, 'diversité alimentaire insuffisante');
                case 3: return makeTestResult(TestScore.Fragile, 'diversité alimentaire insuffisante');
                case 4: return makeTestResult(TestScore.Fragile, 'diversité alimentaire insuffisante');
            }
        } else {
            if (data.diet_diversite_alimentaire == 0) {
                return makeTestResult(TestScore.Fragile, 'diversité alimentaire insuffisante');
            } else {
                return makeTestResult(TestScore.Good, 'bonne diversité alimentaire');
            }
        }
    };

    this.testProteinIntake = function(data) {
        if (data.diet_apports_proteines === null || data.consultant_sexe === null || data.demo_dxa_indice_mm === null ||
                data.bio_albuminemie === null)
            return makeTestResult(null);

        if (data.bio_albuminemie < 35) {
            return makeTestResult(TestScore.Bad, 'hypoalbuminémie');
        } else if (data.diet_apports_proteines == 0) {
            return makeTestResult(TestScore.Fragile, 'apports protéiques insuffisants');
        } else {
            switch (data.consultant_sexe) {
                case 'M': {
                    if (data.demo_dxa_indice_mm <= 7.23) {
                        return makeTestResult(TestScore.Fragile, 'apports protéiques insuffisants');
                    } else {
                        return makeTestResult(TestScore.Good, 'bons apports protéiques');
                    }
                } break;
                case 'F': {
                    if (data.demo_dxa_indice_mm <= 5.67) {
                        return makeTestResult(TestScore.Fragile, 'apports protéiques insuffisants');
                    } else {
                        return makeTestResult(TestScore.Good, 'bons apports protéiques');
                    }
                } break;
            }
        }
    };

    this.testCalciumIntake = function(data) {
        if (data.diet_apports_calcium === null)
            return makeTestResult(null);

        if (data.diet_apports_calcium == 0) {
            return makeTestResult(TestScore.Fragile, 'apports calciques insuffisants');
        } else {
            return makeTestResult(TestScore.Good, 'bons apports calciques');
        }
    };

    this.testBehavior = function(data) {
        if (data.diet_tendances_adaptees === null || data.diet_tendances_inadaptees === null)
            return makeTestResult(null);

        if (data.diet_tendances_adaptees == 0 && data.diet_tendances_inadaptees == 1) {
            return makeTestResult(TestScore.Bad, 'comportement alimentaire pathologique');
        } else if (data.diet_tendances_adaptees == 0 && data.diet_tendances_inadaptees == 0) {
            return makeTestResult(TestScore.Fragile, 'comportement alimentaire inadapté');
        } else {
            return makeTestResult(TestScore.Good, 'bon comportement alimentaire');
        }
    };

    this.testAll = function(data) {
        let score = Math.min(self.testDiversity(data).score, self.testProteinIntake(data).score,
                             self.testCalciumIntake(data).score, self.testBehavior(data).score);
        return makeTestResult(score);
    };

    return this;
}).call({});
