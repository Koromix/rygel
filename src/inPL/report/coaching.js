// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let coaching = (function() {
    let self = this;

    const ScreeningHandlers = [
        {group: 'Neuropsychologie', abbrev: 'EFF', title: 'Efficience cognitive', func: tests.neuroEfficiency},
        {group: 'Neuropsychologie', abbrev: 'MEM', title: 'Mémoire', func: tests.neuroMemory},
        {group: 'Neuropsychologie', abbrev: 'EXE', title: 'Exécution', func: tests.neuroExecution},
        {group: 'Neuropsychologie', abbrev: 'ATT', title: 'Attention et vitesse de traitement', func: tests.neuroAttention},
        {group: 'Neuropsychologie', abbrev: 'COG', title: 'Cognition', func: tests.neuroCognition},
        {group: 'Neuropsychologie', abbrev: 'DA', title: 'Dépression / anxiété', func: tests.neuroDepressionAnxiety},
        {group: 'Neuropsychologie', abbrev: 'SOM', title: 'Sommeil', func: tests.neuroSleep},
        {group: 'Neuropsychologie', abbrev: 'NP', title: 'Neuropsychologie', func: tests.neuroAll},
        {group: 'Nutrition', abbrev: 'DIV', title: 'Diversité', func: tests.nutritionDiversity},
        {group: 'Nutrition', abbrev: 'PRO', title: 'Protéines', func: tests.nutritionProteinIntake},
        {group: 'Nutrition', abbrev: 'CAL', title: 'Calcium', func: tests.nutritionCalciumIntake},
        {group: 'Nutrition', abbrev: 'CMP', title: 'Comportement', func: tests.nutritionBehavior},
        {group: 'Nutrition', abbrev: 'NUT', title: 'Nutrition', func: tests.nutritionAll},
        {group: 'EMS', abbrev: 'MOB', title: 'Mobilité', func: tests.emsMobility},
        {group: 'EMS', abbrev: 'FOR', title: 'Force', func: tests.emsStrength},
        {group: 'EMS', abbrev: 'RF', title: 'Risque de fracture', func: tests.emsFractureRisk},
        {group: 'EMS', abbrev: 'EMS', title: 'EMS', func: tests.emsAll}
    ];

    function createCategoryHeaders(handlers)
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

        return groups.map(group => html('th', {colspan: group.colspan}, group.title));
    }

    function createScreeningHeaders(handlers)
    {
        return handlers.map(handler => html('th', {title: handler.title}, handler.abbrev));
    }

    this.refreshSummary = function(rows) {
        let sex_filter = query('#inpl_option_sex').value;
        let rows_type = query('#inpl_option_rows').value;
        let columns = ['COG', 'DA', 'SOM', 'NUT', 'EMS'];

        let summary = query('#inpl_summary');

        if (rows.length) {
            let handlers = ScreeningHandlers.filter(handler => columns.includes(handler.abbrev));

            let filter_func;
            switch (sex_filter) {
                case 'all': { filter_func = row => true; } break;
                case 'male': { filter_func = row => row.consultant_sexe == 'M'; } break;
                case 'female': { filter_func = row => row.consultant_sexe == 'F'; } break;
            }

            let row_names;
            let row_func;
            switch (rows_type) {
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
                        switch (row.consultant_sexe) {
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

            for (let row of rows) {
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
            }

            summary.replaceContent(
                html('thead',
                    html('tr',
                        html('th', {rowspan: 2}),
                        createCategoryHeaders(handlers)
                    ),
                    html('tr',
                        createScreeningHeaders(handlers)
                    )
                ),
                html('tbody')
            );
            let thead = summary.query('thead');
            let tbody = summary.query('tbody');

            // We need 4 columns for each test
            thead.queryAll('th').forEach(th => {
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
        } else {
            summary.innerHTML = '';
        }
    }

    this.refreshList = function(rows) {
        let list = query('#inpl_list');

        if (rows.length) {
            function resultCell(result)
            {
                switch (result.score) {
                    case TestScore.Bad: return html('td', {class: 'inpl_result_1', title: result.text}, 'P');
                    case TestScore.Fragile: return html('td', {class: 'inpl_result_2', title: result.text}, 'F');
                    case TestScore.Good: return html('td', {class: 'inpl_result_3', title: result.text}, 'R');
                    default: return html('td', {class: 'inpl_result_0', title: 'Indéfini'}, '?');
                }
            }

            list.replaceContent(
                html('thead',
                    html('tr',
                        html('th', {rowspan: 2}),
                        html('th', {rowspan: 2}),
                        html('th', {rowspan: 2}),
                        createCategoryHeaders(ScreeningHandlers)
                    ),
                    html('tr',
                        createScreeningHeaders(ScreeningHandlers)
                    )
                ),
                html('tbody')
            );
            let tbody = list.query('tbody');

            for (let row of rows) {
                let tr = html('tr',
                    html('th',
                        html('a', {href: inPL.url({tab: 2, plid: row.rdv_plid})}, '' + row.rdv_plid)
                    ),
                    html('td', '' + (row.consultant_sexe || '?')),
                    html('td', '' + (row.rdv_age || '?')),
                    ScreeningHandlers.map(handler => resultCell(handler.func(row)))
                );

                tbody.appendContent(tr);
            }
        } else {
            list.innerHTML = '';
        }
    }

    this.refreshAll = function(rows) {
        self.refreshSummary(rows);
        self.refreshList(rows);
    };

    return this;
}).call({});
