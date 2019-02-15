// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let bridge = (function() {
    function parseIntOrNull(value)
    {
        value = parseInt(value);
        return isNaN(value) ? null : value;
    }

    function parseFloatOrNull(value)
    {
        value = parseFloat(value);
        return isNaN(value) ? null : value;
    }

    // FIXME: Fix handling of dictionaries (broken detection of 'autre' values)
    function translateRow(row)
    {
        let row2 = {
            plid: parseInt(row['rdv.plid']),

            cs_sexe: row['consultant.sexe'] || null,

            // FIXME: Wrong but acceptable for now
            rdv_age: parseIntOrNull(row['consultant.age']),

            anthro_taille: parseFloatOrNull(row['constantes.taille']),
            anthro_poids: parseFloatOrNull(row['constantes.poids']),

            aq_had1: parseIntOrNull(row['aq1.had1']),
            aq_had2: parseIntOrNull(row['aq1.had2']),
            aq_had3: parseIntOrNull(row['aq1.had3']),
            aq_had4: parseIntOrNull(row['aq1.had4']),
            aq_had5: parseIntOrNull(row['aq1.had5']),
            aq_had6: parseIntOrNull(row['aq1.had6']),
            aq_had7: parseIntOrNull(row['aq1.had7']),
            aq_had8: parseIntOrNull(row['aq1.had8']),
            aq_had9: parseIntOrNull(row['aq1.had9']),
            aq_had10: parseIntOrNull(row['aq1.had10']),
            aq_had11: parseIntOrNull(row['aq1.had11']),
            aq_had12: parseIntOrNull(row['aq1.had12']),
            aq_had13: parseIntOrNull(row['aq1.had13']),
            aq_had14: parseIntOrNull(row['aq1.had14']),
            aq_som1: (function() {
                if (row['aq1.som1'] !== null && row['aq1.som1'] !== undefined) {
                    return row['aq1.som1'].split('|').map(x => parseInt(x));
                } else {
                    return null;
                }
            })(),

            npsy_nsc: parseIntOrNull(row['neuropsy.nsc']),
            npsy_plainte_som: parseIntOrNull(row['neuropsy.plainte_som']),
            npsy_moca: parseFloatOrNull(row['neuropsy.moca']),
            npsy_tmta: parseFloatOrNull(row['neuropsy.score_tmta_temps']),
            npsy_lecture: parseFloatOrNull(row['neuropsy.score_lecture_temps']),
            npsy_tmtb: parseFloatOrNull(row['neuropsy.score_tmtb_temps']),
            npsy_interf: parseFloatOrNull(row['neuropsy.score_interf_temps1']),
            npsy_slc: parseFloatOrNull(row['neuropsy.seqlc_score_brut']),
            npsy_animx: parseFloatOrNull(row['neuropsy.score_flu1_correct']),
            npsy_p: parseFloatOrNull(row['neuropsy.score_flu2_correct']),
            npsy_rl: parseFloatOrNull(row['neuropsy.score_3rl']) || parseFloatOrNull(row['neuropsy.score_2rl']),
            npsy_rt: parseFloatOrNull(row['neuropsy.score_3rt']) || parseFloatOrNull(row['neuropsy.score_2rt']),

            rx_dmo_rachis: parseFloatOrNull(row['demo.dmo_rachis']),
            rx_dmo_col: parseFloatOrNull(row['demo.dmo_col']),
            rx_dxa: parseFloatOrNull(row['demo.dxa_indice_mm']),

            diet_diversite: parseIntOrNull(row['diet.diversite_alimentaire']),
            diet_poids_6mois: parseFloatOrNull(row['diet.poids_estime_6mois']),
            diet_proteines: parseIntOrNull(row['diet.apports_proteines']),
            diet_calcium: parseIntOrNull(row['diet.apports_calcium']),
            diet_comportement1: parseIntOrNull(row['diet.tendances_adaptees']),
            diet_comportement2: parseIntOrNull(row['diet.tendances_inadaptees']),

            ems_temps_assis_jour: (function() {
                return parseIntOrNull(row['ems.temps_assis_jour_h']) * 60 +
                       parseIntOrNull(row['ems.temps_assis_jour_min']);
            })(),
            ems_assis_2h_continu: parseIntOrNull(row['ems.assis_2h_continu']),
            ems_handgrip: parseFloatOrNull(row['ems.test_handgrip']),
            ems_vit4m: parseFloatOrNull(row['ems.test_vit4']),
            ems_unipod: parseFloatOrNull(row['ems.test_unipod']),
            ems_tug: parseFloatOrNull(row['ems.test_timeup']),
            ems_gug: parseFloatOrNull(row['ems.test_getup']),

            // FIXME: Temporary until biology is available
            bio_albuminemie: 40
        };

        for (let i = 1; i <= 10; i++) {
            row2[`ems_act${i}`] = parseIntOrNull(row[`ems.act${i}`]);
            switch (parseIntOrNull(row[`ems.act${i}_temps`])) {
                case 0: { row2[`ems_act${i}_duree`] = 15; } break;
                case 1: { row2[`ems_act${i}_duree`] = 30; } break;
                case 2: { row2[`ems_act${i}_duree`] = 45; } break;
                case 3: { row2[`ems_act${i}_duree`] = 60; } break;
                case 4: { row2[`ems_act${i}_duree`] = 90; } break;
                case 5: { row2[`ems_act${i}_duree`] = 120; } break;
                case 6: { row2[`ems_act${i}_duree`] = 180; } break;
                case null: { row2[`ems_act${i}_duree`] = null; } break;
            }
            row2[`ems_act${i}_freq`] = parseIntOrNull(row[`ems.act${i}_freq`]);
            row2[`ems_act${i}_intensite`] = parseIntOrNull(row[`ems.act${i}_intensite`]);
        }

        return row2;
    }

    this.readFileAsync = function(file, functions) {
        Papa.parse(file, {
            header: true,
            chunk: ret => {
                for (let row of ret.data) {
                    row = translateRow(row);
                    if (row.plid)
                        functions.step(row);
                }
            },
            complete: functions.complete
        });
    };

    return this;
}).call({});
