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
                switch (test_name) {
                    case 'demo_rachis': return demo.testRachis(data).text;
                    case 'demo_col': return demo.testFemoralNeck(data).text;
                    case 'demo_hanche': return demo.testHip(data).text;
                    case 'demo_avbras': return demo.testForearm(data).text;
                    case 'demo_sarcopenie': return demo.testSarcopenia(data).text;

                    case 'diet_diversite': return nutrition.testDiversity(data).text;
                    case 'diet_proteines': return nutrition.testProteinIntake(data).text;
                    case 'diet_calcium': return nutrition.testCalciumIntake(data).text;
                    case 'diet_comportement': return nutrition.testBehavior(data).text;

                    case 'ems_mobilite': return ems.testMobility(data).text;
                    case 'ems_force': return ems.testStrength(data).text;
                    case 'ems_fractures': return ems.testFractureRisk(data).text;

                    case 'neuropsy_efficience': return neuropsy.testEfficiency(data).text;
                    case 'neuropsy_memoire': return neuropsy.testMemory(data).text;
                    case 'neuropsy_execution': return neuropsy.testExecution(data).text;
                    case 'neuropsy_attention': return neuropsy.testAttention(data).text;
                    case 'neuropsy_cognition': return neuropsy.testCognition(data).text;
                    case 'neuropsy_had': return neuropsy.testDepressionAnxiety(data).text;
                    case 'neuropsy_sommeil': return neuropsy.testSleep(data).text;

                    case 'constantes_hta_ortho': return misc.testOrthostaticHypotension(data).text;
                    case 'constantes_vop': return misc.testVOP(data).text;
                    case 'audition_surdite_gauche': return misc.testSurdityL(data).text;
                    case 'audition_surdite_droite': return misc.testSurdityR(data).text;

                    default: throw `Unknown test \'${test_name}\'`;
                }
            }
            function calc(data, calc_name)
            {
                switch (calc_name) {
                    case 'aq1_epices': return misc.computeEpices(data);

                    default: throw `Unknown calculated variable \'${calc_name}\'`;
                }
            }
            function sex(data, male_text, female_text)
            {
                switch (data.consultant_sexe) {
                    case 'M': return male_text;
                    case 'F': return female_text;
                    default: return null;
                }
            }

            reader.onload = function() {
                let zip = new JSZip(reader.result);
                let doc = new Docxtemplater().loadZip(zip);

                try {
                    doc.setOptions({
                        parser: function(tag) {
                            tag = tag.replace(/[’‘]/g, "'");
                            return { get: (it) => {
                                let ret = eval(tag);
                                if (ret === null || ret === undefined)
                                    ret = '????';
                                return ret;
                            }};
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
                } catch (e) {
                    let error_count = 0;
                    if (e.properties.id === 'multi_error') {
                        e.properties.errors.forEach(function(err) {
                            console.log(err);
                            error_count++;
                        });
                    } else {
                        console.log(e);
                        error_count = 1;
                    }

                    alert(`Got ${error_count} error(s), open the JS console to get more information`);
                }
            };

            reader.readAsBinaryString(file);
        } else if (row) {
            alert('You must specify a template document');
        }
    }

    return this;
}).call({});
