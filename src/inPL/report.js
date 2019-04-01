// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let report = (function() {
    let self = this;

    function createRow(title, value)
    {
        return html('tr',
            html('th', title),
            html('td', value)
        );
    }

    this.refreshReport = function(rows) {
        let plid = query('#inpl_option_plid').value;
        let row = rows.find(row => row.rdv_plid == plid);

        let report = query('#inpl_report');

        report.replaceContent(
            html('h1', 'Identité'),
            row ? html('div',
                html('table',
                    createRow('Nom', row.consultant_nom),
                    createRow('Prénom', row.consultant_prenom),
                    createRow('Sexe', {M: 'Homme', F: 'Femme'}[row.consultant_sexe]),
                    createRow('Date de naissance', row.consultant_date_naissance)
                )
            ) : null,
            html('h1', 'Biologie médicale'),
            html('h1', 'Examen général'),
            html('h1', 'Électrocardiogramme (ECG)'),
            html('h1', 'Examen de la vision'),
            html('h1', 'Examen de l\'audition'),
            html('h1', 'Examen de spirométrie'),
            html('h1', 'Examen de densitométrie osseuse'),
            html('h1', 'Entretien avec neuro-psychologue'),
            html('h1', 'Entretien avec nutritionniste'),
            html('h1', 'Entretien avec EMS'),
            row ? html('button', {style: 'display: block; margin: 0 auto;',
                                  click: (e) => generateDocument(row)}, 'Générer un Compte-Rendu') : null
        );
    };

    function generateDocument(row)
    {
        let file = query('#inpl_option_template').files[0];

        if (file && row) {
            let reader = new FileReader();

            // Shortcut functions
            function label(value, var_name)
            {
                if (value instanceof Object && !Array.isArray(value))
                    value = value[var_name];

                if (value !== null) {
                    let var_info = bridge.getVarInfo(var_name);
                    let dict = bridge.getDictInfo(var_info.dict_name)

                    return dict[value];
                } else {
                    return null;
                }
            }
            function test(data, test_name)
            {
                let result;
                switch (test_name) {
                    case 'demo_rachis': { result = demo.testRachis(data); } break;
                    case 'demo_col': { result = demo.testFemoralNeck(data); } break;
                    case 'demo_hanche': { result = demo.testHip(data); } break;
                    case 'demo_avbras': { result = demo.testForearm(data); } break;
                    case 'demo_sarcopenie': { result = demo.testSarcopenia(data); } break;

                    case 'diet_diversite': { result = nutrition.screenDiversity(data); } break;
                    case 'diet_proteines': { result = nutrition.screenProteinIntake(data); } break;
                    case 'diet_calcium': { result = nutrition.screenCalciumIntake(data); } break;
                    case 'diet_comportement': { result = nutrition.screenBehavior(data); } break;

                    case 'ems_mobilite': { result = ems.screenMobility(data); } break;
                    case 'ems_force': { result = ems.screenStrength(data); } break;
                    case 'ems_fractures': { result = ems.screenFractureRisk(data); } break;

                    case 'neuropsy_efficience': { result = neuropsy.screenEfficiency(data); } break;
                    case 'neuropsy_memoire': { result = neuropsy.screenMemory(data); } break;
                    case 'neuropsy_execution': { result = neuropsy.screenExecution(data); } break;
                    case 'neuropsy_attention': { result = neuropsy.screenAttention(data); } break;
                    case 'neuropsy_cognition': { result = neuropsy.screenCognition(data); } break;
                    case 'neuropsy_had': { result = neuropsy.screenDepressionAnxiety(data); } break;
                    case 'neuropsy_sommeil': { result = neuropsy.screenSleep(data); } break;

                    case 'constantes_hta_ortho': { result = misc.screenOrthostaticHypotension(data); } break;
                    case 'constantes_vop': return misc.screenVOP(data);
                    case 'audition_surdite_gauche': return misc.screenSurdityL(data);
                    case 'audition_surdite_droite': return misc.screenSurdityR(data);

                    default: throw `Unknown test \'${test_name}\'`;
                }

                return ScreeningResult.label(result);
            }
            function calc(data, calc_name)
            {
                switch (calc_name) {
                    case 'aq1_epices': return misc.computeEpices(data);

                    default: throw `Unknown calculated variable \'${calc_name}\'`;
                }
            }

            reader.onload = function() {
                let zip = new JSZip(reader.result);
                let doc = new Docxtemplater().loadZip(zip);

                try {
                    doc.setOptions({
                        parser: function(tag) {
                            tag = tag.replace(/[’‘]/g, "'");
                            return { get: (it) => eval(tag) };
                        },
                        paragraphLoop: true,
                        linebreaks: true
                    });
                    doc.setData(row);
                    doc.render();

                    let out = doc.getZip().generate({
                        type: 'blob',
                        mimeType: 'application/vnd.openxmlformats-officedocument.wordprocessingml.document'
                    });

                    saveBlob(out, `CR_${row.rdv_plid}.docx`);
                } catch (err) {
                    alert(err.message);
                }
            };

            reader.readAsBinaryString(file);
        } else if (row) {
            alert('You must specify a template document');
        }
    }

    return this;
}).call({});
