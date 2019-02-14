// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let tables = (function() {
    let self = this;

    const ScreeningHandlers = [
        {group: 'Neuropsychologie', title: 'Efficience cognitive', abbrev: 'EFF', func: neuropsy.screenEfficiency},
        {group: 'Neuropsychologie', title: 'Mémoire', abbrev: 'MEM', func: neuropsy.screenMemory},
        {group: 'Neuropsychologie', title: 'Exécution', abbrev: 'EXE', func: neuropsy.screenExecution},
        {group: 'Neuropsychologie', title: 'Attention et vitesse de traitement', abbrev: 'ATT', func: neuropsy.screenAttention},
        {group: 'Neuropsychologie', title: 'Cognition', abbrev: 'COG', func: neuropsy.screenCognition},
        {group: 'Neuropsychologie', title: 'Dépression / anxiété', abbrev: 'DA', func: neuropsy.screenDepressionAnxiety},
        {group: 'Neuropsychologie', title: 'Sommeil', abbrev: 'SOM', func: neuropsy.screenSleep},
        {group: 'Neuropsychologie', title: 'Neuropsychologie', abbrev: 'NP', func: neuropsy.screenAll},
        {group: 'Nutrition', title: 'Diversité', abbrev: 'DIV', func: nutrition.screenDiversity},
        {group: 'Nutrition', title: 'Protéines', abbrev: 'PRO', func: nutrition.screenProteinIntake},
        {group: 'Nutrition', title: 'Calcium', abbrev: 'CAL', func: nutrition.screenCalciumIntake},
        {group: 'Nutrition', title: 'Comportement', abbrev: 'CMP', func: nutrition.screenBehavior},
        {group: 'Nutrition', title: 'Nutrition', abbrev: 'NUT', func: nutrition.screenAll},
        {group: 'EMS', title: 'Mobilité', abbrev: 'MOB', func: ems.screenMobility},
        {group: 'EMS', title: 'Force', abbrev: 'FOR', func: ems.screenStrength},
        {group: 'EMS', title: 'Risque de fracture', abbrev: 'RF', func: ems.screenFractureRisk},
        {group: 'EMS', title: 'EMS', abbrev: 'EMS', func: ems.screenAll}
    ];

    function createScreeningHeader(handlers)
    {
        let groups = [];
        {
            let prev_group = null;
            for (let handler of handlers) {
                if (handler.group !== prev_group) {
                    groups.push({
                        title: handler.group,
                        colspan: 1
                    });

                    prev_group = handler.group;
                } else {
                    groups[groups.length - 1].colspan++;
                }
            }
        }

        return html('thead',
            html('tr',
                html('th', {rowspan: 2}),
                groups.map(group => html('th', {colspan: group.colspan}, group.title))
            ),
            html('tr',
                handlers.map(handler => html('th', {title: handler.title}, handler.abbrev))
            )
        );
    }

    this.refreshSummary = function() {
        let file = document.querySelector('#inpl_option_file').files[0];
        let sex = document.querySelector('#inpl_option_sex').value;
        let rows = document.querySelector('#inpl_option_rows').value;
        let columns = ['COG', 'DA', 'SOM', 'NUT', 'EMS'];

        let summary = document.querySelector('#inpl_summary');

        if (file) {
            let handlers = ScreeningHandlers.filter(handler => columns.includes(handler.abbrev));

            let filter_func;
            switch (sex) {
                case 'all': { filter_func = row => true; } break;
                case 'male': { filter_func = row => row.cs_sexe == 'M'; } break;
                case 'female': { filter_func = row => row.cs_sexe == 'F'; } break;
            }

            let row_names;
            let row_func;
            switch (rows) {
                case 'age': {
                    row_names = ['0-44 ans', '45-54 ans', '55-64 ans',
                                 '65-74 ans', '75-84 ans', '85+ ans'];
                    row_func = row => {
                        if (row.rdv_age < 45) return 0;
                        else if (row.rdv_age < 55) return 1;
                        else if (row.rdv_age < 65) return 2;
                        else if (row.rdv_age < 75) return 3;
                        else if (row.rdv_age < 85) return 4;
                        else return 5;
                    };
                } break;

                case 'sex': {
                    row_names = ['Hommes', 'Femmes'];
                    row_func = row => {
                        switch (row.cs_sexe) {
                            case 'M': return 0;
                            case 'F': return 1;
                            default: return null;
                        }
                    };
                } break;
            }
            row_names.push('Total');

            let stats = [];
            for (let i = 0; i < row_names.length; i++) {
                stats.push(new Array(handlers.length).fill(undefined).map(x => {
                    return [0, 0, 0, 0];
                }));
            }

            bridge.readFromFile(file, {
                step: row => {
                    if (filter_func(row)) {
                        let row_idx = row_func(row);
                        if (row_idx === null)
                            return;

                        for (let i = 0; i < handlers.length; i++) {
                            let result = handlers[i].func(row) || 0;
                            stats[row_idx][i][result]++;
                            stats[row_names.length - 1][i][result]++;
                        }
                    }
                },
                complete: () => {
                    summary.replaceContent(
                        createScreeningHeader(handlers),
                        html('tbody')
                    );
                    let thead = summary.querySelector('thead');
                    let tbody = summary.querySelector('tbody');

                    // We need 4 columns for each test
                    thead.querySelectorAll('th').forEach(th => {
                        th.setAttribute('colspan', 4 * (parseInt(th.getAttribute('colspan')) || 1));
                    });

                    for (let i = 0; i < row_names.length; i++) {
                        let tr = html('tr',
                            html('th', {colspan: 4}, row_names[i])
                        );

                        for (let j = 0; j < handlers.length; j++) {
                            tr.appendContent(
                                html('td', {class: 'inpl_result_3'}, '' + stats[i][j][3]),
                                html('td', {class: 'inpl_result_2'}, '' + stats[i][j][2]),
                                html('td', {class: 'inpl_result_1'}, '' + stats[i][j][1]),
                                html('td', {class: 'inpl_result_0'}, '' + stats[i][j][0])
                            );
                        }

                        tbody.appendContent(tr);
                    }
                }
            });
        } else {
            summary.innerHTML = '';
        }
    }

    this.refreshList = function() {
        let file = document.querySelector('#inpl_option_file').files[0];

        let list = document.querySelector('#inpl_list');

        if (file) {
            function resultCell(result)
            {
                switch (result) {
                    case ScreeningResult.Bad: return html('td', {class: 'inpl_result_1', title: 'Pathologique'}, 'P');
                    case ScreeningResult.Fragile: return html('td', {class: 'inpl_result_2', title: 'Fragile'}, 'F');
                    case ScreeningResult.Good: return html('td', {class: 'inpl_result_3', title: 'Robuste'}, 'R');
                    default: return html('td', {class: 'inpl_result_0', title: 'Indéfini'}, '?');
                }
            }

            let rows = [];
            bridge.readFromFile(file, {
                step: row => {
                    rows.push(row);
                },
                complete: () => {
                    rows.sort((row1, row2) => row1.plid - row2.plid);

                    list.replaceContent(
                        createScreeningHeader(ScreeningHandlers),
                        html('tbody')
                    );
                    let tbody = list.querySelector('tbody');

                    for (let row of rows) {
                        let tr = html('tr',
                            html('th', '' + row.plid),
                            ScreeningHandlers.map(handler => resultCell(handler.func(row)))
                        );

                        tbody.appendContent(tr);
                    }
                }
            });
        } else {
            list.innerHTML = '';
        }
    }

    this.refreshAll = function() {
        self.refreshSummary();
        self.refreshList();
    };

    return this;
}).call({});
