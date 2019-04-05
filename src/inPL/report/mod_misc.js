// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let misc = (function() {
    let self = this;

    this.computeEpices = function(data) {
        if (data.aq1_seco4 === null || data.aq1_seco7 === null || data.aq1_seco10 === null ||
                data.aq1_seco2 === null || data.aq1_seco3 === null || data.aq1_lois2 === null ||
                data.aq1_lois3 === null || data.aq1_lois4 === null || data.aq1_integsoc2 === null ||
                data.aq1_integsoc3 === null || data.aq1_integsoc4 === null)
            return null;

        let score = !!data.aq1_seco4 * 10.06 +
                    !!data.aq1_seco7 * -11.83 +
                    !!data.aq1_seco10 * -8.28 +
                    !!data.aq1_seco2 * -8.28 +
                    !!data.aq1_seco3 * 14.8 +
                    !!data.aq1_lois2 * -6.51 + // !!data.aq1_seco11
                    !!data.aq1_lois3 * -7.1 + // !!data.aq1_seco12
                    !!data.aq1_lois4 * -7.1 + // !!data.aq1_seco13
                    !!data.aq1_integsoc2 * -9.47 + // !!data.aq1_seco14
                    !!data.aq1_integsoc3 * -9.47 + // !!data.aq1_seco15
                    !!data.aq1_integsoc4 * -7.1 + // !!data.aq1_seco16
                    75.14;
        score = roundTo(score, 2);

        return score;
    };

    function getSystolicPressure(data) {
        if (data.explcv2b != null) {
            return data.explcv2b;
        } else if (data.explcv2 != null) {
            return data.explcv2;
        } else {
            return makeTestResult(null);
        }
    }
    function getDiastolicPressure(data) {
        if (data.explcv3b != null) {
            return data.explcv3b;
        } else if (data.explcv3 != null) {
            return data.explcv3;
        } else {
            return makeTestResult(null);
        }
    }

    this.testOrthostaticHypotension = function(data) {
        let pas = getSystolicPressure(data);
        let pad = getDiastolicPressure(data);

        if (pas === null || pad === null ||
                data.constantes_explcv8 === null || data.constantes_explcv9 === null ||
                data.constantes_explcv11 === null || data.constantes_explcv12 === null)
                // data.constantes_explcv14 === null || data.constantes_explcv15 === null)
            return makeTestResult(null);

        let pas_min = Math.min(data.constantes_explcv8, data.constantes_explcv11); //, data.constantes_explcv14);
        let pad_min = Math.min(data.constantes_explcv9, data.constantes_explcv12); //, data.constantes_explcv15);

        if (pas - pas_min >= 20 || pad - pad_min >= 10) {
            return makeTestResult(TestScore.Bad, 'hypotension orthostatique');
        //} else if (pas - data.constantes_explcv14 >= 20 || pad - data.constantes_explcv15 >= 10) {
        //    return ScreeningResult.Fragile;
        } else {
            return makeTestResult(TestScore.Good, 'absence d\'hypotension orthostatique');
        }
    };

    this.testVOP = function(data) {
        if (data.rdv_age === null || data.explcv17 === null)
            return null;

        let treshold;
        if (data.rdv_age < 30) {
            treshold = 7.1;
        } else if (data.rdv_age < 40) {
            treshold = 8.0
        } else if (data.rdv_age < 50) {
            treshold = 8.6;
        } else if (data.rdv_age < 60) {
            treshold = 10.0;
        } else if (data.rdv_age < 70) {
            treshold = 13.1;
        } else {
            treshold = 14.6;
        }

        if (getSystolicPressure(data) >= 140 || getDiastolicPressure(data) >= 90) {
            return makeTestResult(TestScore.Bad, 'non pertinent car HTA lors de l\'examen');
        } else if (data.explcv17 >= treshold) {
            return makeTestResult(TestScore.Fragile, 'rigidité artérielle anormalement élevée avec risque de développer une HTA dans l’avenir');
        } else {
            return makeTestResult(TestScore.Good, 'rigidité artérielle dans les normes');
        }
    }

    function testSurdity(loss)
    {
        if (loss === null)
            return null;

        if (loss >= 90) {
            return makeTestResult(TestScore.Bad, 'perte auditive profonde');
        } else if (loss >= 70) {
            return makeTestResult(TestScore.Bad, 'perte auditive sévère');
        } else if (loss >= 40) {
            return makeTestResult(TestScore.Bad, 'perte auditive moyenne');
        } else if (loss >= 20) {
            return makeTestResult(TestScore.Fragile, 'perte auditive légère');
        } else {
            return makeTestResult(TestScore.Good, 'audition normale');
        }
    }

    this.testSurdityL = function(data) { return testSurdity(data.perte_tonale_gauche); }
    this.testSurdityR = function(data) { return testSurdity(data.perte_tonale_droite); }

    return this;
}).call({});
