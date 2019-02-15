// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let bridge = (function() {
    function translateInt(value)
    {
        value = parseInt(value);
        return isNaN(value) ? null : value;
    }

    function translateFloat(value)
    {
        value = parseFloat(value);
        return isNaN(value) ? null : value;
    }

    function translateSimpleEnum(value)
    {
        if (value === 'autre') {
            return 999;
        } else {
            return translateInt(value);
        }
    }

    function translateMultiEnum(value)
    {
        if (value !== null && value !== undefined && value !== '') {
            return value.split('|').map(x => translateSimpleEnum(x));
        } else {
            return [];
        }
    }

    // FIXME: Fix handling of dictionaries (broken detection of 'autre' values)
    function translateRow(row)
    {
        let row2 = {
            plid: parseInt(row['rdv.plid']),

            cs_sexe: row['consultant.sexe'] || null,

            // FIXME: Wrong but acceptable for now
            rdv_age: translateInt(row['consultant.age']),

            anthro_taille: translateFloat(row['constantes.taille']),
            anthro_poids: translateFloat(row['constantes.poids']),

            aq_had1: translateInt(row['aq1.had1']),
            aq_had2: translateInt(row['aq1.had2']),
            aq_had3: translateInt(row['aq1.had3']),
            aq_had4: translateInt(row['aq1.had4']),
            aq_had5: translateInt(row['aq1.had5']),
            aq_had6: translateInt(row['aq1.had6']),
            aq_had7: translateInt(row['aq1.had7']),
            aq_had8: translateInt(row['aq1.had8']),
            aq_had9: translateInt(row['aq1.had9']),
            aq_had10: translateInt(row['aq1.had10']),
            aq_had11: translateInt(row['aq1.had11']),
            aq_had12: translateInt(row['aq1.had12']),
            aq_had13: translateInt(row['aq1.had13']),
            aq_had14: translateInt(row['aq1.had14']),
            aq_som1: translateMultiEnum(row['aq1.som1']),

            npsy_nsc: translateInt(row['neuropsy.nsc']),
            npsy_plainte_som: translateSimpleEnum(row['neuropsy.plainte_som']),
            npsy_moca: translateFloat(row['neuropsy.moca']),
            npsy_tmta: translateFloat(row['neuropsy.score_tmta_temps']),
            npsy_lecture: translateFloat(row['neuropsy.score_lecture_temps']),
            npsy_tmtb: translateFloat(row['neuropsy.score_tmtb_temps']),
            npsy_interf: translateFloat(row['neuropsy.score_interf_temps1']),
            npsy_slc: translateFloat(row['neuropsy.seqlc_score_brut']),
            npsy_animx: translateFloat(row['neuropsy.score_flu1_correct']),
            npsy_p: translateFloat(row['neuropsy.score_flu2_correct']),
            npsy_rl: translateFloat(row['neuropsy.score_3rl']) || translateFloat(row['neuropsy.score_2rl']),
            npsy_rt: translateFloat(row['neuropsy.score_3rt']) || translateFloat(row['neuropsy.score_2rt']),

            rx_dmo_rachis: translateFloat(row['demo.dmo_rachis']),
            rx_dmo_col: translateFloat(row['demo.dmo_col']),
            rx_dxa: translateFloat(row['demo.dxa_indice_mm']),

            diet_diversite: translateSimpleEnum(row['diet.diversite_alimentaire']),
            diet_poids_6mois: translateFloat(row['diet.poids_estime_6mois']),
            diet_proteines: translateSimpleEnum(row['diet.apports_proteines']),
            diet_calcium: translateSimpleEnum(row['diet.apports_calcium']),
            diet_comportement1: translateSimpleEnum(row['diet.tendances_adaptees']),
            diet_comportement2: translateSimpleEnum(row['diet.tendances_inadaptees']),

            ems_pratique_activites: translateSimpleEnum(row['ems.pratique_activites']),
            ems_temps_assis_jour: (function() {
                return translateInt(row['ems.temps_assis_jour_h']) * 60 +
                       translateInt(row['ems.temps_assis_jour_min']);
            })(),
            ems_assis_2h_continu: translateSimpleEnum(row['ems.assis_2h_continu']),
            ems_handgrip: translateFloat(row['ems.test_handgrip']),
            ems_vit4m: translateFloat(row['ems.test_vit4']),
            ems_unipod: translateFloat(row['ems.test_unipod']),
            ems_tug: translateFloat(row['ems.test_timeup']),
            ems_gug: translateFloat(row['ems.test_getup']),

            // FIXME: Temporary until biology is available
            bio_albuminemie: 40
        };

        for (let i = 1; i <= 10; i++) {
            row2[`ems_act${i}`] = translateSimpleEnum(row[`ems.act${i}`]);
            switch (translateSimpleEnum(row[`ems.act${i}_temps`])) {
                case 0: { row2[`ems_act${i}_duree`] = 15; } break;
                case 1: { row2[`ems_act${i}_duree`] = 30; } break;
                case 2: { row2[`ems_act${i}_duree`] = 45; } break;
                case 3: { row2[`ems_act${i}_duree`] = 60; } break;
                case 4: { row2[`ems_act${i}_duree`] = 90; } break;
                case 5: { row2[`ems_act${i}_duree`] = 120; } break;
                case 6: { row2[`ems_act${i}_duree`] = 180; } break;
                case null: { row2[`ems_act${i}_duree`] = null; } break;
            }
            row2[`ems_act${i}_freq`] = translateSimpleEnum(row[`ems.act${i}_freq`]);
            row2[`ems_act${i}_intensite`] = translateSimpleEnum(row[`ems.act${i}_intensite`]);
        }

        return row2;
    }

    this.readFileAsync = function(file, functions) {
        Papa.parse(file, {
            header: true,
            chunk: ret => {
                for (let row of ret.data) {
                    row = translateRow(row);
                    if (row.plid && row.cs_sexe)
                        functions.step(row);
                }
            },
            complete: functions.complete
        });
    };

    return this;
}).call({});
