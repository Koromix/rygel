// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let tables = (function() {
    let self = this;

    this.refreshSummary = function() {

    }

    this.refreshList = function() {
        let file = document.querySelector('#inpl_file').files[0];
        let dashboard = document.querySelector('#inpl_dashboard');

        if (file) {
            dashboard.replaceContent(
                html('thead',
                    html('tr',
                        html('th', {rowspan: 2}, 'PLID'),
                        html('th', {colspan: 8}, 'Neuropsychologie'),
                        html('th', {colspan: 5}, 'Nutrition'),
                        html('th', {colspan: 4}, 'EMS')
                    ),
                    html('tr',
                        html('th', {title: 'Efficience'}, 'EFF'),
                        html('th', {title: 'Mémoire'}, 'MEM'),
                        html('th', {title: 'Exécution'}, 'EXE'),
                        html('th', {title: 'Attention'}, 'ATT'),
                        html('th', {title: 'Cognition'}, 'COG'),
                        html('th', {title: 'Dépression / Anxiété'}, 'DA'),
                        html('th', {title: 'Sommeil'}, 'SOM'),
                        html('th', {title: 'Neuropsychologie'}, 'NP'),
                        html('th', {title: 'Diversité'}, 'DIV'),
                        html('th', {title: 'Protéines'}, 'PRO'),
                        html('th', {title: 'Calcium'}, 'CAL'),
                        html('th', {title: 'Comportement'}, 'CMP'),
                        html('th', {title: 'Nutrition'}, 'NUT'),
                        html('th', {title: 'Mobilité'}, 'MOB'),
                        html('th', {title: 'Force'}, 'FOR'),
                        html('th', {title: 'Risque de fracture'}, 'RF'),
                        html('th', {title: 'EMS'}, 'EMS')
                    )
                ),
                html('tbody')
            );

            let tbody = dashboard.querySelector('tbody');

            function resultCell(result)
            {
                switch (result) {
                    case ScreeningResult.Bad: return html('td', {class: 'inpl_result_1', title: 'Pathologique'}, 'P');
                    case ScreeningResult.Fragile: return html('td', {class: 'inpl_result_2', title: 'Fragile'}, 'F');
                    case ScreeningResult.Good: return html('td', {class: 'inpl_result_3', title: 'Robuste'}, 'R');
                    default: return html('td', {class: 'inpl_result_0', title: 'Indéfini'}, '?');
                }
            }

            bridge.readFromFile(file, row => {
                let tr = html('tr',
                    html('th', '' + row.plid),
                    resultCell(neuropsy.screenEfficiency(row)),
                    resultCell(neuropsy.screenMemory(row)),
                    resultCell(neuropsy.screenExecution(row)),
                    resultCell(neuropsy.screenAttention(row)),
                    resultCell(neuropsy.screenCognition(row)),
                    resultCell(neuropsy.screenDepressionAnxiety(row)),
                    resultCell(neuropsy.screenSleep(row)),
                    resultCell(neuropsy.screenAll(row)),
                    resultCell(nutrition.screenDiversity(row)),
                    resultCell(nutrition.screenProteinIntake(row)),
                    resultCell(nutrition.screenCalciumIntake(row)),
                    resultCell(nutrition.screenBehavior(row)),
                    resultCell(nutrition.screenAll(row)),
                    resultCell(ems.screenMobility(row)),
                    resultCell(ems.screenStrength(row)),
                    resultCell(ems.screenFractureRisk(row)),
                    resultCell(ems.screenAll(row))
                );

                tbody.appendContent(tr);
            });
        } else {
            dashboard.innerHTML = '';
        }
    }

    this.refreshAll = function() {
        self.refreshSummary();
        self.refreshList();
    };

    return this;
}).call({});
