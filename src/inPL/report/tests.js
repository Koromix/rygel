// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let tests = (function() {
    let self = this;

    // ------------------------------------------------------------------------
    // Survey
    // ------------------------------------------------------------------------

    this.aqEpices = function(data) {
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

    // ------------------------------------------------------------------------
    // Densitometry
    // ------------------------------------------------------------------------

    function makeBoneResult(t)
    {
        if (t === null)
            return makeTestResult(null);

        if (t <= -2.5) {
            return makeTestResult(TestScore.Bad, 'ostéoporose');
        } else if (t <= -1.0) {
            return makeTestResult(TestScore.Fragile, 'ostéopénie');
        } else {
            return makeTestResult(TestScore.Good, 'absence d\'ostéopénie/ostéoporose');
        }
    }

    this.demoRachis = function(data) { return makeBoneResult(data.demo_dmo_rachis); }
    this.demoFemoralNeck = function(data) { return makeBoneResult(data.demo_dmo_col); }
    this.demoHip = function(data) { return makeBoneResult(data.demo_dmo_hanche); }
    this.demoForearm = function(data) { return makeBoneResult(data.demo_dmo_avb1); }

    this.demoSarcopenia = function(data) {
        if (data.consultant_sexe === null || data.demo_dxa_indice_mm === null)
            return makeTestResult(null);

        let treshold;
        switch (data.consultant_sexe) {
            case 'M': { treshold = 7.23; } break;
            case 'F': { treshold = 5.67; } break;
        }

        if (data.demo_dmo_avb1 <= treshold) {
            return makeTestResult(TestScore.Fragile, 'sarcopénie');
        } else {
            return makeTestResult(TestScore.Good, 'absence de sarcopénie');
        }
    }

    // ------------------------------------------------------------------------
    // EMS
    // ------------------------------------------------------------------------

    this.emsMobility = function(data) {
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

    this.emsStrength = function(data) {
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

    this.emsFractureRisk = function(data) {
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

    this.emsAll = function(data) {
        let score = Math.min(self.emsMobility(data).score, self.emsStrength(data).score,
                             self.emsFractureRisk(data).score);
        return makeTestResult(score);
    };

    // ------------------------------------------------------------------------
    // Cardio-vascular
    // ------------------------------------------------------------------------

    this.systolicPressure = function(data) {
        if (data.explcv2b != null) {
            return data.explcv2b;
        } else if (data.explcv2 != null) {
            return data.explcv2;
        } else {
            return null;
        }
    }
    this.diastolicPressure = function(data) {
        if (data.explcv3b != null) {
            return data.explcv3b;
        } else if (data.explcv3 != null) {
            return data.explcv3;
        } else {
            return null;
        }
    }

    this.cardioOrthostaticHypotension = function(data) {
        let pas = self.systolicPressure(data);
        let pad = self.diastolicPressure(data);

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

    this.cardioRigidity = function(data) {
        if (data.rdv_age === null || data.explcv17 === null)
            return makeTestResult(null);

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

        if (self.systolicPressure(data) >= 140 || self.diastolicPressure(data) >= 90) {
            return makeTestResult(TestScore.Bad, 'non pertinent car HTA lors de l\'examen');
        } else if (data.explcv17 >= treshold) {
            return makeTestResult(TestScore.Fragile, 'rigidité artérielle anormalement élevée avec risque de développer une HTA dans l’avenir');
        } else {
            return makeTestResult(TestScore.Good, 'absence de rigidité artérielle (VOP dans les normes)');
        }
    }

    // ------------------------------------------------------------------------
    // Audition
    // ------------------------------------------------------------------------

    function testSurdity(loss)
    {
        if (loss === null)
            return makeTestResult(null);

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

    this.surdityLeft = function(data) { return testSurdity(data.perte_tonale_gauche); }
    this.surdityRight = function(data) { return testSurdity(data.perte_tonale_droite); }

    // ------------------------------------------------------------------------
    // Spirometry
    // ------------------------------------------------------------------------

    this.spiroQuality = function(data) {
        if (data.respi_spiro_qualite1 === null) {
            return null;
        } else if (data.respi_spiro_qualite1) {
            return 'bonne qualité';
        } else {
            if (data.respi_presence_plateau == null) {
                return null;
            } else if (data.respi_presence_plateau) {
                return 'bonne qualité';
            } else {
                return 'non interprétable';
            }
        }
    };

    this.spiroResult = function(data) {
        if (data.respi_vems === null || data.respi_vems_limite === null ||
                data.respi_cvf === null ||data.respi_cvf_limite === null ||
                data.respi_def2575 === null || data.respi_def2575_limite === null)
            return makeTestResult(null);

        let obstructive = (data.respi_vems / data.respi_cvf) < 0.7;
        let restrictive = data.respi_vems < data.respi_vems_limite ||
                          data.respi_cvf < data.respi_cvf_limite;

        if (obstructive && restrictive) {
            return makeTestResult(TestScore.Bad, 'trouble mixte');
        } else if (obstructive) {
            return makeTestResult(TestScore.Bad, 'trouble obstructif');
        } else if (restrictive) {
            return makeTestResult(TestScore.Bad, 'trouble restrictif');
        } else if (data.respi_def2575 < data.respi_def2575_limite) {
            return makeTestResult(TestScore.Fragile, 'anomalie des bronches distales (DEF 25-75)');
        } else {
            return makeTestResult(TestScore.Good, 'spirométrie normale');
        }
    };

    // ------------------------------------------------------------------------
    // Neuropsy
    // ------------------------------------------------------------------------

    let neuro_tresholds = {
        401: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        411: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        421: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        431: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        441: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        451: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        461: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        471: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        481: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        491: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        501: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        511: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        521: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        531: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        541: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        551: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        561: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        571: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        581: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        591: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 41, tmta_sd: 22, tmta_c50: 34, tmta_c5: 98, tmtb_mean: 99, tmtb_sd: 42, tmtb_c50: 93, tmtb_c5: 186, deno_mean: 62, deno_sd: 12, deno_c50: 59, deno_c5: 92, lecture_mean: 45, lecture_sd: 9, lecture_c50: 44, lecture_c5: 62, interf_mean: 117, interf_sd: 27, interf_c50: 118, interf_c5: 193, int_deno_mean: 55, int_deno_sd: 21, int_deno_c50: 55, int_deno_c5: 104, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        601: {moca_mean: 25.9, moca_sd: 2.7, moca_c50: 26, moca_c5: 21.4, rl_mean: 31, rl_sd: 6, rl_c50: 32, rl_c5: 21.1, rt_mean: 44.5, rt_sd: 4.9, rt_c50: 46, rt_c5: 38.6, p_mean: 12.5, p_sd: 4.2, p_c50: 12, p_c5: 12, animx_mean: 18.9, animx_sd: 4.5, animx_c50: 19, animx_c5: 11.7, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        611: {moca_mean: 25.3, moca_sd: 2.9, moca_c50: 26, moca_c5: 20.8, rl_mean: 29.5, rl_sd: 6.2, rl_c50: 29.5, rl_c5: 18, rt_mean: 45.5, rt_sd: 2.7, rt_c50: 46, rt_c5: 38, p_mean: 11.6, p_sd: 3.6, p_c50: 12, p_c5: 12, animx_mean: 17.8, animx_sd: 4.6, animx_c50: 17, animx_c5: 10.6, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        621: {moca_mean: 25.3, moca_sd: 2.9, moca_c50: 26, moca_c5: 20.8, rl_mean: 29.5, rl_sd: 6.2, rl_c50: 29.5, rl_c5: 18, rt_mean: 45.5, rt_sd: 2.7, rt_c50: 46, rt_c5: 38, p_mean: 11.6, p_sd: 3.6, p_c50: 12, p_c5: 12, animx_mean: 17.8, animx_sd: 4.6, animx_c50: 17, animx_c5: 10.6, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        631: {moca_mean: 25.3, moca_sd: 2.9, moca_c50: 26, moca_c5: 20.8, rl_mean: 29.5, rl_sd: 6.2, rl_c50: 29.5, rl_c5: 18, rt_mean: 45.5, rt_sd: 2.7, rt_c50: 46, rt_c5: 38, p_mean: 11.6, p_sd: 3.6, p_c50: 12, p_c5: 12, animx_mean: 17.8, animx_sd: 4.6, animx_c50: 17, animx_c5: 10.6, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        641: {moca_mean: 25.3, moca_sd: 2.9, moca_c50: 26, moca_c5: 20.8, rl_mean: 29.5, rl_sd: 6.2, rl_c50: 29.5, rl_c5: 18, rt_mean: 45.5, rt_sd: 2.7, rt_c50: 46, rt_c5: 38, p_mean: 11.6, p_sd: 3.6, p_c50: 12, p_c5: 12, animx_mean: 17.8, animx_sd: 4.6, animx_c50: 17, animx_c5: 10.6, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        651: {moca_mean: 25.3, moca_sd: 2.9, moca_c50: 26, moca_c5: 20.8, rl_mean: 29.5, rl_sd: 6.2, rl_c50: 29.5, rl_c5: 18, rt_mean: 45.5, rt_sd: 2.7, rt_c50: 46, rt_c5: 38, p_mean: 11.6, p_sd: 3.6, p_c50: 12, p_c5: 12, animx_mean: 17.8, animx_sd: 4.6, animx_c50: 17, animx_c5: 10.6, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 50, code_c5: 32, slc_c50: 10, slc_c5: 5},
        661: {moca_mean: 25.3, moca_sd: 2.9, moca_c50: 26, moca_c5: 20.8, rl_mean: 29.5, rl_sd: 6.2, rl_c50: 29.5, rl_c5: 18, rt_mean: 45.5, rt_sd: 2.7, rt_c50: 46, rt_c5: 38, p_mean: 11.6, p_sd: 3.6, p_c50: 12, p_c5: 12, animx_mean: 17.8, animx_sd: 4.6, animx_c50: 17, animx_c5: 10.6, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 50, code_c5: 32, slc_c50: 10, slc_c5: 5},
        671: {moca_mean: 25.3, moca_sd: 2.9, moca_c50: 26, moca_c5: 20.8, rl_mean: 29.5, rl_sd: 6.2, rl_c50: 29.5, rl_c5: 18, rt_mean: 45.5, rt_sd: 2.7, rt_c50: 46, rt_c5: 38, p_mean: 11.6, p_sd: 3.6, p_c50: 12, p_c5: 12, animx_mean: 17.8, animx_sd: 4.6, animx_c50: 17, animx_c5: 10.6, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 50, code_c5: 32, slc_c50: 10, slc_c5: 5},
        681: {moca_mean: 25.3, moca_sd: 2.9, moca_c50: 26, moca_c5: 20.8, rl_mean: 29.5, rl_sd: 6.2, rl_c50: 29.5, rl_c5: 18, rt_mean: 45.5, rt_sd: 2.7, rt_c50: 46, rt_c5: 38, p_mean: 11.6, p_sd: 3.6, p_c50: 12, p_c5: 12, animx_mean: 17.8, animx_sd: 4.6, animx_c50: 17, animx_c5: 10.6, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 50, code_c5: 32, slc_c50: 10, slc_c5: 5},
        691: {moca_mean: 25.3, moca_sd: 2.9, moca_c50: 26, moca_c5: 20.8, rl_mean: 29.5, rl_sd: 6.2, rl_c50: 29.5, rl_c5: 18, rt_mean: 45.5, rt_sd: 2.7, rt_c50: 46, rt_c5: 38, p_mean: 11.6, p_sd: 3.6, p_c50: 12, p_c5: 12, animx_mean: 17.8, animx_sd: 4.6, animx_c50: 17, animx_c5: 10.6, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 50, code_c5: 32, slc_c50: 10, slc_c5: 5},
        701: {moca_mean: 25.3, moca_sd: 2.9, moca_c50: 26, moca_c5: 20.8, rl_mean: 29.5, rl_sd: 6.2, rl_c50: 29.5, rl_c5: 18, rt_mean: 45.5, rt_sd: 2.7, rt_c50: 46, rt_c5: 38, p_mean: 11.6, p_sd: 3.6, p_c50: 12, p_c5: 12, animx_mean: 17.8, animx_sd: 4.6, animx_c50: 17, animx_c5: 10.6, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 44, code_c5: 25, slc_c50: 8, slc_c5: 3},
        711: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 44, code_c5: 25, slc_c50: 8, slc_c5: 3},
        721: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 44, code_c5: 25, slc_c50: 8, slc_c5: 3},
        731: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 44, code_c5: 25, slc_c50: 8, slc_c5: 3},
        741: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 44, code_c5: 25, slc_c50: 8, slc_c5: 3},
        751: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        761: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        771: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        781: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        791: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        801: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        811: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        821: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        831: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        841: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        851: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        861: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        871: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        881: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        891: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        901: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        911: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        921: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        931: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        941: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        951: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        961: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        971: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        981: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        991: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        1001: {moca_mean: 24.3, moca_sd: 3, moca_c50: 25, moca_c5: 20.4, rl_mean: 25.3, rl_sd: 6.2, rl_c50: 25, rl_c5: 15.9, rt_mean: 44.3, rt_sd: 3.5, rt_c50: 45, rt_c5: 37.6, p_mean: 11.1, p_sd: 3.7, p_c50: 11, p_c5: 11, animx_mean: 17.3, animx_sd: 4.4, animx_c50: 16.5, animx_c5: 9.8, tmta_mean: 56, tmta_sd: 20, tmta_c50: 55, tmta_c5: 100, tmtb_mean: 153, tmtb_sd: 62, tmtb_c50: 143, tmtb_c5: 282, deno_mean: 74, deno_sd: 19, deno_c50: 67, deno_c5: 118, lecture_mean: 48, lecture_sd: 7, lecture_c50: 48, lecture_c5: 58, interf_mean: 178, interf_sd: 64, interf_c50: 171, interf_c5: 345, int_deno_mean: 105, int_deno_sd: 54, int_deno_c50: 83, int_deno_c5: 245, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        402: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        412: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        422: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        432: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        442: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        452: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        462: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        472: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        482: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        492: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        502: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        512: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        522: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        532: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        542: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        552: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        562: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        572: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        582: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        592: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 42, tmta_sd: 14, tmta_c50: 42, tmta_c5: 65, tmtb_mean: 89, tmtb_sd: 35, tmtb_c50: 81, tmtb_c5: 159, deno_mean: 65, deno_sd: 13, deno_c50: 62, deno_c5: 94, lecture_mean: 46, lecture_sd: 10, lecture_c50: 45, lecture_c5: 72, interf_mean: 118, interf_sd: 27, interf_c50: 113, interf_c5: 167, int_deno_mean: 53, int_deno_sd: 18, int_deno_c50: 46, int_deno_c5: 84, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        602: {moca_mean: 26.9, moca_sd: 2.2, moca_c50: 27, moca_c5: 22.6, rl_mean: 32.8, rl_sd: 5.7, rl_c50: 33, rl_c5: 22.6, rt_mean: 45.9, rt_sd: 3.1, rt_c50: 47, rt_c5: 39.8, p_mean: 14, p_sd: 4.2, p_c50: 14, p_c5: 14, animx_mean: 20.6, animx_sd: 4.4, animx_c50: 21, animx_c5: 13.4, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        612: {moca_mean: 26.7, moca_sd: 2.4, moca_c50: 27, moca_c5: 22, rl_mean: 30.2, rl_sd: 6.2, rl_c50: 30, rl_c5: 19.4, rt_mean: 45.2, rt_sd: 3.5, rt_c50: 46, rt_c5: 39.2, p_mean: 14.2, p_sd: 4.6, p_c50: 14, p_c5: 14, animx_mean: 20.4, animx_sd: 4.6, animx_c50: 20, animx_c5: 12.3, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        622: {moca_mean: 26.7, moca_sd: 2.4, moca_c50: 27, moca_c5: 22, rl_mean: 30.2, rl_sd: 6.2, rl_c50: 30, rl_c5: 19.4, rt_mean: 45.2, rt_sd: 3.5, rt_c50: 46, rt_c5: 39.2, p_mean: 14.2, p_sd: 4.6, p_c50: 14, p_c5: 14, animx_mean: 20.4, animx_sd: 4.6, animx_c50: 20, animx_c5: 12.3, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        632: {moca_mean: 26.7, moca_sd: 2.4, moca_c50: 27, moca_c5: 22, rl_mean: 30.2, rl_sd: 6.2, rl_c50: 30, rl_c5: 19.4, rt_mean: 45.2, rt_sd: 3.5, rt_c50: 46, rt_c5: 39.2, p_mean: 14.2, p_sd: 4.6, p_c50: 14, p_c5: 14, animx_mean: 20.4, animx_sd: 4.6, animx_c50: 20, animx_c5: 12.3, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        642: {moca_mean: 26.7, moca_sd: 2.4, moca_c50: 27, moca_c5: 22, rl_mean: 30.2, rl_sd: 6.2, rl_c50: 30, rl_c5: 19.4, rt_mean: 45.2, rt_sd: 3.5, rt_c50: 46, rt_c5: 39.2, p_mean: 14.2, p_sd: 4.6, p_c50: 14, p_c5: 14, animx_mean: 20.4, animx_sd: 4.6, animx_c50: 20, animx_c5: 12.3, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        652: {moca_mean: 26.7, moca_sd: 2.4, moca_c50: 27, moca_c5: 22, rl_mean: 30.2, rl_sd: 6.2, rl_c50: 30, rl_c5: 19.4, rt_mean: 45.2, rt_sd: 3.5, rt_c50: 46, rt_c5: 39.2, p_mean: 14.2, p_sd: 4.6, p_c50: 14, p_c5: 14, animx_mean: 20.4, animx_sd: 4.6, animx_c50: 20, animx_c5: 12.3, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 50, code_c5: 32, slc_c50: 10, slc_c5: 5},
        662: {moca_mean: 26.7, moca_sd: 2.4, moca_c50: 27, moca_c5: 22, rl_mean: 30.2, rl_sd: 6.2, rl_c50: 30, rl_c5: 19.4, rt_mean: 45.2, rt_sd: 3.5, rt_c50: 46, rt_c5: 39.2, p_mean: 14.2, p_sd: 4.6, p_c50: 14, p_c5: 14, animx_mean: 20.4, animx_sd: 4.6, animx_c50: 20, animx_c5: 12.3, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 50, code_c5: 32, slc_c50: 10, slc_c5: 5},
        672: {moca_mean: 26.7, moca_sd: 2.4, moca_c50: 27, moca_c5: 22, rl_mean: 30.2, rl_sd: 6.2, rl_c50: 30, rl_c5: 19.4, rt_mean: 45.2, rt_sd: 3.5, rt_c50: 46, rt_c5: 39.2, p_mean: 14.2, p_sd: 4.6, p_c50: 14, p_c5: 14, animx_mean: 20.4, animx_sd: 4.6, animx_c50: 20, animx_c5: 12.3, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 50, code_c5: 32, slc_c50: 10, slc_c5: 5},
        682: {moca_mean: 26.7, moca_sd: 2.4, moca_c50: 27, moca_c5: 22, rl_mean: 30.2, rl_sd: 6.2, rl_c50: 30, rl_c5: 19.4, rt_mean: 45.2, rt_sd: 3.5, rt_c50: 46, rt_c5: 39.2, p_mean: 14.2, p_sd: 4.6, p_c50: 14, p_c5: 14, animx_mean: 20.4, animx_sd: 4.6, animx_c50: 20, animx_c5: 12.3, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 50, code_c5: 32, slc_c50: 10, slc_c5: 5},
        692: {moca_mean: 26.7, moca_sd: 2.4, moca_c50: 27, moca_c5: 22, rl_mean: 30.2, rl_sd: 6.2, rl_c50: 30, rl_c5: 19.4, rt_mean: 45.2, rt_sd: 3.5, rt_c50: 46, rt_c5: 39.2, p_mean: 14.2, p_sd: 4.6, p_c50: 14, p_c5: 14, animx_mean: 20.4, animx_sd: 4.6, animx_c50: 20, animx_c5: 12.3, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 50, code_c5: 32, slc_c50: 10, slc_c5: 5},
        702: {moca_mean: 26.7, moca_sd: 2.4, moca_c50: 27, moca_c5: 22, rl_mean: 30.2, rl_sd: 6.2, rl_c50: 30, rl_c5: 19.4, rt_mean: 45.2, rt_sd: 3.5, rt_c50: 46, rt_c5: 39.2, p_mean: 14.2, p_sd: 4.6, p_c50: 14, p_c5: 14, animx_mean: 20.4, animx_sd: 4.6, animx_c50: 20, animx_c5: 12.3, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 44, code_c5: 25, slc_c50: 8, slc_c5: 3},
        712: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 44, code_c5: 25, slc_c50: 8, slc_c5: 3},
        722: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 44, code_c5: 25, slc_c50: 8, slc_c5: 3},
        732: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 44, code_c5: 25, slc_c50: 8, slc_c5: 3},
        742: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 44, code_c5: 25, slc_c50: 8, slc_c5: 3},
        752: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        762: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        772: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        782: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        792: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        802: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        812: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        822: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        832: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        842: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        852: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        862: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        872: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        882: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        892: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        902: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        912: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        922: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        932: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        942: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        952: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        962: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        972: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        982: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        992: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        1002: {moca_mean: 25.4, moca_sd: 2.6, moca_c50: 25, moca_c5: 21.6, rl_mean: 27.1, rl_sd: 6.2, rl_c50: 27, rl_c5: 17.4, rt_mean: 45.1, rt_sd: 2.9, rt_c50: 46, rt_c5: 38.8, p_mean: 13, p_sd: 4.3, p_c50: 13, p_c5: 13, animx_mean: 18.2, animx_sd: 4.5, animx_c50: 18, animx_c5: 11.5, tmta_mean: 64, tmta_sd: 55, tmta_c50: 50, tmta_c5: 129, tmtb_mean: 142, tmtb_sd: 87, tmtb_c50: 117, tmtb_c5: 319, deno_mean: 70, deno_sd: 14, deno_c50: 70, deno_c5: 97, lecture_mean: 47, lecture_sd: 7, lecture_c50: 48, lecture_c5: 63, interf_mean: 138, interf_sd: 35, interf_c50: 131, interf_c5: 197, int_deno_mean: 68, int_deno_sd: 29, int_deno_c50: 68, int_deno_c5: 151, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        403: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        413: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        423: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        433: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        443: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        453: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        463: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        473: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        483: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        493: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        503: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        513: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        523: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        533: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        543: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 67, code_c5: 43, slc_c50: 11, slc_c5: 5},
        553: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        563: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        573: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        583: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        593: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 36, tmta_sd: 14, tmta_c50: 31, tmta_c5: 66, tmtb_mean: 73, tmtb_sd: 24, tmtb_c50: 69, tmtb_c5: 125, deno_mean: 56, deno_sd: 10, deno_c50: 55, deno_c5: 74, lecture_mean: 41, lecture_sd: 6, lecture_c50: 40, lecture_c5: 51, interf_mean: 103, interf_sd: 25, interf_c50: 97, interf_c5: 150, int_deno_mean: 46, int_deno_sd: 22, int_deno_c50: 42, int_deno_c5: 83, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        603: {moca_mean: 27.9, moca_sd: 1.7, moca_c50: 28, moca_c5: 23.8, rl_mean: 34.4, rl_sd: 4.7, rl_c50: 35, rl_c5: 24.2, rt_mean: 46.5, rt_sd: 2.2, rt_c50: 47, rt_c5: 41, p_mean: 16.7, p_sd: 4.3, p_c50: 17, p_c5: 17, animx_mean: 22.8, animx_sd: 5.5, animx_c50: 22, animx_c5: 15.1, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        613: {moca_mean: 27.4, moca_sd: 1.7, moca_c50: 27, moca_c5: 23.2, rl_mean: 30.2, rl_sd: 5.2, rl_c50: 30, rl_c5: 20.9, rt_mean: 45.6, rt_sd: 2.5, rt_c50: 46, rt_c5: 40.4, p_mean: 15.4, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 21, animx_sd: 4.7, animx_c50: 21, animx_c5: 13.9, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        623: {moca_mean: 27.4, moca_sd: 1.7, moca_c50: 27, moca_c5: 23.2, rl_mean: 30.2, rl_sd: 5.2, rl_c50: 30, rl_c5: 20.9, rt_mean: 45.6, rt_sd: 2.5, rt_c50: 46, rt_c5: 40.4, p_mean: 15.4, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 21, animx_sd: 4.7, animx_c50: 21, animx_c5: 13.9, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        633: {moca_mean: 27.4, moca_sd: 1.7, moca_c50: 27, moca_c5: 23.2, rl_mean: 30.2, rl_sd: 5.2, rl_c50: 30, rl_c5: 20.9, rt_mean: 45.6, rt_sd: 2.5, rt_c50: 46, rt_c5: 40.4, p_mean: 15.4, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 21, animx_sd: 4.7, animx_c50: 21, animx_c5: 13.9, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        643: {moca_mean: 27.4, moca_sd: 1.7, moca_c50: 27, moca_c5: 23.2, rl_mean: 30.2, rl_sd: 5.2, rl_c50: 30, rl_c5: 20.9, rt_mean: 45.6, rt_sd: 2.5, rt_c50: 46, rt_c5: 40.4, p_mean: 15.4, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 21, animx_sd: 4.7, animx_c50: 21, animx_c5: 13.9, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 56, code_c5: 36, slc_c50: 10, slc_c5: 5},
        653: {moca_mean: 27.4, moca_sd: 1.7, moca_c50: 27, moca_c5: 23.2, rl_mean: 30.2, rl_sd: 5.2, rl_c50: 30, rl_c5: 20.9, rt_mean: 45.6, rt_sd: 2.5, rt_c50: 46, rt_c5: 40.4, p_mean: 15.4, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 21, animx_sd: 4.7, animx_c50: 21, animx_c5: 13.9, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 50, code_c5: 32, slc_c50: 10, slc_c5: 5},
        663: {moca_mean: 27.4, moca_sd: 1.7, moca_c50: 27, moca_c5: 23.2, rl_mean: 30.2, rl_sd: 5.2, rl_c50: 30, rl_c5: 20.9, rt_mean: 45.6, rt_sd: 2.5, rt_c50: 46, rt_c5: 40.4, p_mean: 15.4, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 21, animx_sd: 4.7, animx_c50: 21, animx_c5: 13.9, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 50, code_c5: 32, slc_c50: 10, slc_c5: 5},
        673: {moca_mean: 27.4, moca_sd: 1.7, moca_c50: 27, moca_c5: 23.2, rl_mean: 30.2, rl_sd: 5.2, rl_c50: 30, rl_c5: 20.9, rt_mean: 45.6, rt_sd: 2.5, rt_c50: 46, rt_c5: 40.4, p_mean: 15.4, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 21, animx_sd: 4.7, animx_c50: 21, animx_c5: 13.9, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 50, code_c5: 32, slc_c50: 10, slc_c5: 5},
        683: {moca_mean: 27.4, moca_sd: 1.7, moca_c50: 27, moca_c5: 23.2, rl_mean: 30.2, rl_sd: 5.2, rl_c50: 30, rl_c5: 20.9, rt_mean: 45.6, rt_sd: 2.5, rt_c50: 46, rt_c5: 40.4, p_mean: 15.4, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 21, animx_sd: 4.7, animx_c50: 21, animx_c5: 13.9, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 50, code_c5: 32, slc_c50: 10, slc_c5: 5},
        693: {moca_mean: 27.4, moca_sd: 1.7, moca_c50: 27, moca_c5: 23.2, rl_mean: 30.2, rl_sd: 5.2, rl_c50: 30, rl_c5: 20.9, rt_mean: 45.6, rt_sd: 2.5, rt_c50: 46, rt_c5: 40.4, p_mean: 15.4, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 21, animx_sd: 4.7, animx_c50: 21, animx_c5: 13.9, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 50, code_c5: 32, slc_c50: 10, slc_c5: 5},
        703: {moca_mean: 27.4, moca_sd: 1.7, moca_c50: 27, moca_c5: 23.2, rl_mean: 30.2, rl_sd: 5.2, rl_c50: 30, rl_c5: 20.9, rt_mean: 45.6, rt_sd: 2.5, rt_c50: 46, rt_c5: 40.4, p_mean: 15.4, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 21, animx_sd: 4.7, animx_c50: 21, animx_c5: 13.9, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 44, code_c5: 25, slc_c50: 8, slc_c5: 3},
        713: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 44, code_c5: 25, slc_c50: 8, slc_c5: 3},
        723: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 44, code_c5: 25, slc_c50: 8, slc_c5: 3},
        733: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 44, code_c5: 25, slc_c50: 8, slc_c5: 3},
        743: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 44, code_c5: 25, slc_c50: 8, slc_c5: 3},
        753: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        763: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        773: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        783: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        793: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        803: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        813: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        823: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        833: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        843: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        853: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        863: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        873: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        883: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        893: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        903: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        913: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        923: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        933: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        943: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        953: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        963: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        973: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        983: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        993: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3},
        1003: {moca_mean: 27.6, moca_sd: 2.1, moca_c50: 28, moca_c5: 22.8, rl_mean: 30.4, rl_sd: 6.4, rl_c50: 31, rl_c5: 18.8, rt_mean: 46.1, rt_sd: 2.6, rt_c50: 47, rt_c5: 40, p_mean: 15.6, p_sd: 4.2, p_c50: 15, p_c5: 15, animx_mean: 20.5, animx_sd: 5.3, animx_c50: 21, animx_c5: 13.2, tmta_mean: 49, tmta_sd: 18, tmta_c50: 44, tmta_c5: 84, tmtb_mean: 118, tmtb_sd: 51, tmtb_c50: 105, tmtb_c5: 226, deno_mean: 65, deno_sd: 12, deno_c50: 62, deno_c5: 87, lecture_mean: 44, lecture_sd: 6, lecture_c50: 43, lecture_c5: 55, interf_mean: 123, interf_sd: 32, interf_c50: 113, interf_c5: 191, int_deno_mean: 58, int_deno_sd: 25, int_deno_c50: 51, int_deno_c5: 131, code_c50: 38, code_c5: 22, slc_c50: 7, slc_c5: 3}
    };

    function getNeuroTresholds(age, sc_level)
    {
        age = Math.min(Math.max(age, 40), 100);

        let key = age * 10 + sc_level;
        return neuro_tresholds[key];
    }

    function testInfC5(value, treshold)
    {
        return (value !== null) ? (value <= treshold) : false;
    }
    function testInfC50(value, treshold)
    {
        return (value !== null) ? (value < treshold) : false;
    }
    function testSupC5(value, treshold)
    {
        return (value !== null) ? (value >= treshold) : false;
    }
    function testSupC50(value, treshold)
    {
        return (value !== null) ? (value > treshold) : false;
    }

    function makeNeuroResult(score)
    {
        switch (score) {
            case TestScore.Bad: return makeTestResult(TestScore.Bad, 'en dehors des normes (pathologique)');
            case TestScore.Fragile: return makeTestResult(TestScore.Fragile, 'dans les limites des normes (fragilité)');
            case TestScore.Good: return makeTestResult(TestScore.Good, 'dans les normes (robuste)');

            default: return makeTestResult(null);
        }
    }

    this.neuroEfficiency = function(data) {
        if (data.rdv_age === null || data.neuropsy_nsc === null || data.neuropsy_moca === null)
            return makeTestResult(null);

        let tresholds = getNeuroTresholds(data.rdv_age, data.neuropsy_nsc);
        if (!tresholds)
            return makeTestResult(null);

        if (testInfC5(data.neuropsy_moca, tresholds.moca_c5)) {
            return makeNeuroResult(TestScore.Bad);
        } else if (testInfC50(data.neuropsy_moca, tresholds.moca_c50)) {
            return makeNeuroResult(TestScore.Fragile);
        } else {
            return makeNeuroResult(TestScore.Good);
        }
    };

    this.neuroMemory = function(data) {
        if (data.rdv_age === null || data.neuropsy_nsc === null || data.neuropsy_rl === null ||
                data.neuropsy_rt === null)
            return makeTestResult(null);

        let tresholds = getNeuroTresholds(data.rdv_age, data.neuropsy_nsc);
        if (!tresholds)
            return makeTestResult(null);

        if (testInfC5(data.neuropsy_rl, tresholds.rl_c5) || testInfC5(data.neuropsy_rt, tresholds.rt_c5)) {
            return makeNeuroResult(TestScore.Bad);
        } else if (testInfC50(data.neuropsy_rl, tresholds.rl_c50) || testInfC50(data.neuropsy_rt, tresholds.rt_c50)) {
            return makeNeuroResult(TestScore.Fragile);
        } else {
            return makeNeuroResult(TestScore.Good);
        }
    };

    this.neuroExecution = function(data) {
        if (data.rdv_age === null || data.neuropsy_nsc === null ||
                ((data.neuropsy_tmtb === null) + (data.neuropsy_interf === null) + (data.neuropsy_slc === null) +
                 (data.neuropsy_animx === null) + (data.neuropsy_p === null)) > 2)
            return makeTestResult(null);

        let tresholds = getNeuroTresholds(data.rdv_age, data.neuropsy_nsc);
        if (!tresholds)
            return makeTestResult(null);

        let c5_fails = testSupC5(data.neuropsy_tmtb, tresholds.tmtb_c5) +
                       testSupC5(data.neuropsy_interf, tresholds.interf_c5) +
                       testInfC5(data.neuropsy_slc, tresholds.slc_c5) +
                       testInfC5(data.neuropsy_animx, tresholds.animx_c5) +
                       testInfC5(data.neuropsy_p, tresholds.p_c5);
        let c50_fails = testSupC50(data.neuropsy_tmtb, tresholds.tmtb_c50) +
                        testSupC50(data.neuropsy_interf, tresholds.interf_c50) +
                        testInfC50(data.neuropsy_slc, tresholds.slc_c50) +
                        testInfC50(data.neuropsy_animx, tresholds.animx_c50) +
                        testInfC50(data.neuropsy_p, tresholds.p_c50);

        if (c5_fails) {
            return makeNeuroResult(TestScore.Bad);
        } else if (c50_fails >= 2) {
            return makeNeuroResult(TestScore.Fragile);
        } else {
            return makeNeuroResult(TestScore.Good);
        }
    };

    this.neuroAttention = function(data) {
        if (data.rdv_age === null || data.neuropsy_nsc === null ||
                ((data.neuropsy_code === null) + (data.neuropsy_tmta === null) + (data.neuropsy_lecture === null) +
                 (data.neuropsy_deno === null)) > 2)
            return makeTestResult(null);

        let tresholds = getNeuroTresholds(data.rdv_age, data.neuropsy_nsc);
        if (!tresholds)
            return makeTestResult(null);

        let c5_fails = testInfC5(data.neuropsy_code, tresholds.code_c5) +
                       testSupC5(data.neuropsy_tmta, tresholds.tmta_c5) +
                       testSupC5(data.neuropsy_lecture, tresholds.lecture_c5) +
                       testSupC5(data.neuropsy_deno, tresholds.deno_c5);
        let c50_fails = testInfC50(data.neuropsy_code, tresholds.code_c50) +
                        testSupC50(data.neuropsy_tmta, tresholds.tmta_c50) +
                        testSupC50(data.neuropsy_lecture, tresholds.lecture_c50) +
                        testSupC50(data.neuropsy_deno, tresholds.deno_c50);

        if (c5_fails) {
            return makeNeuroResult(TestScore.Bad);
        } else if (c50_fails >= 2) {
            return makeNeuroResult(TestScore.Fragile);
        } else {
            return makeNeuroResult(TestScore.Good);
        }
    };

    this.neuroCognition = function(data) {
        let score = Math.min(self.neuroEfficiency(data).score, self.neuroMemory(data).score,
                             self.neuroExecution(data).score, self.neuroAttention(data).score);
        return makeTestResult(score);
    };

    this.neuroDepressionAnxiety = function(data) {
        if (data.aq1_had1 === null || data.aq1_had2 === null || data.aq1_had3 === null ||
                 data.aq1_had4 === null || data.aq1_had5 === null || data.aq1_had6 === null ||
                 data.aq1_had7 === null || data.aq1_had8 === null || data.aq1_had9 === null ||
                 data.aq1_had10 === null || data.aq1_had11 === null || data.aq1_had12 === null ||
                 data.aq1_had13 === null || data.aq1_had14 === null)
            return makeTestResult(null);

        let a = data.aq1_had1 + data.aq1_had3 + data.aq1_had5 + data.aq1_had7 +
                data.aq1_had9 + data.aq1_had11 + data.aq1_had13;
        let d = data.aq1_had2 + data.aq1_had4 + data.aq1_had6 + data.aq1_had8 +
                data.aq1_had10 + data.aq1_had12 + data.aq1_had14;

        if (a >= 11 || d >= 11) {
            return makeTestResult(TestScore.Bad, 'thymie ou anxiété pathologique');
        } else if (a >= 8 || d >= 8) {
            return makeTestResult(TestScore.Fragile, 'thymie fragile ou anxiété');
        } else {
            return makeTestResult(TestScore.Good, 'absence de trouble thymique ou d\'anxiété');
        }
    };

    this.neuroSleep = function(data) {
        if (data.aq1_som1 === null || data.neuropsy_plainte_som === null)
            return makeTestResult(null);

        // FIXME: Which variables? New ones?
        let diseased = false;
        let complaint = data.aq1_som1.some(x => x > 0) ||
                        data.neuropsy_plainte_som == 1;

        if (diseased) {
            return makeTestResult(TestScore.Bad, 'sommeil pathologique');
        } else if (complaint) {
            return makeTestResult(TestScore.Fragile, 'sommeil fragile');
        } else {
            return makeTestResult(TestScore.Good, 'sommeil normal');
        }
    };

    this.neuroAll = function(data) {
        let score = Math.min(self.neuroCognition(data).score, self.neuroDepressionAnxiety(data).score,
                             self.neuroSleep(data).score);
        return makeTestResult(score);
    }

    // ------------------------------------------------------------------------
    // Nutrition
    // ------------------------------------------------------------------------

    // TODO: Check inequality operators (< / <=, etc.)
    this.nutritionDiversity = function(data) {
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
                case 3: return makeTestResult(TestScore.Fragile, 'diversité alimentaire insuffisante');
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

    this.nutritionProteinIntake = function(data) {
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

    this.nutritionCalciumIntake = function(data) {
        if (data.diet_apports_calcium === null)
            return makeTestResult(null);

        if (data.diet_apports_calcium == 0) {
            return makeTestResult(TestScore.Fragile, 'apports calciques insuffisants');
        } else {
            return makeTestResult(TestScore.Good, 'bons apports calciques');
        }
    };

    this.nutritionBehavior = function(data) {
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

    this.nutritionAll = function(data) {
        let score = Math.min(self.nutritionDiversity(data).score, self.nutritionProteinIntake(data).score,
                             self.nutritionCalciumIntake(data).score, self.nutritionBehavior(data).score);
        return makeTestResult(score);
    };

    return this;
}).call({});
