// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let dev_data = new function() {
    let self = this;

    this.runTable = async function(table, current_id) {
        let records = await record_manager.loadAll(table);
        let variables = await record_manager.listVariables(table);
        let columns = orderColumns(variables);

        renderRecords(table, records, columns, current_id);
    }

    function renderRecords(table, records, columns, current_id) {
        let empty_msg;
        if (!records.length) {
            empty_msg = 'Aucune donnée à afficher';
        } else if (!columns.length) {
            empty_msg = 'Impossible d\'afficher les données (colonnes inconnues)';
            records = [];
        }

        render(html`
            <table class="dev_records" style=${`min-width: ${30 + 60 * columns.length}px`}>
                <thead>
                    <tr>
                        <th class="dev_head_actions">
                            ${columns.length ?
                                html`<button class="dev_excel" @click=${e => exportToExcel(table)}></button>` : ''}
                        </th>
                        ${!columns.length ? html`<th>&nbsp;</th>` : ''}
                        ${columns.map(col => html`<th class="dev_head_variable">${col.key}</th>`)}
                    </tr>
                </thead>
                <tbody>
                    ${empty_msg ?
                        html`<tr><td colspan=${1 + Math.max(1, columns.length)}>${empty_msg}</td></tr>` : ''}
                    ${records.map(record => html`<tr class=${record.id === current_id ? 'dev_row_current' : ''}>
                        <th>
                            <a href="#" @click=${e => { handleEditClick(e, record, current_id); e.preventDefault(); }}>🔍\uFE0E</a>
                            <a href="#" @click=${e => { showDeleteDialog(e, record, current_id); e.preventDefault(); }}>✕</a>
                        </th>

                        ${columns.map(col => {
                            let value = record.values[col.key];

                            if (value == null) {
                                return html`<td class="dev_record_missing" title="Donnée manquante">NA</td>`;
                            } else if (Array.isArray(value)) {
                                let text = value.join('|');
                                return html`<td title=${text}>${text}</td>`;
                            } else if (typeof value === 'number') {
                                return html`<td class="dev_record_number" title=${value}>${value}</td>`;
                            } else {
                                return html`<td title=${value}>${value}</td>`;
                            }
                        })}
                    </tr>`)}
                </tbody>
            </table>
        `, document.querySelector('#dev_data'));
    }

    async function exportToExcel(table) {
        if (typeof XSLX === 'undefined')
            await util.loadScript(`${env.base_url}static/xlsx.core.min.js`);

        let records = await record_manager.loadAll(table);
        let variables = await record_manager.listVariables(table);
        let columns = orderColumns(variables);

        let export_name = `${env.app_key}_${dates.today()}`;
        let filename = `export_${export_name}.xlsx`;

        let wb = XLSX.utils.book_new();
        {
            let ws = XLSX.utils.aoa_to_sheet([columns.map(col => col.key)]);
            for (let record of records) {
                let values = columns.map(col => record.values[col.key]);
                XLSX.utils.sheet_add_aoa(ws, [values], {origin: -1});
            }
            XLSX.utils.book_append_sheet(wb, ws, export_name);
        }

        XLSX.writeFile(wb, filename);
    }

    function orderColumns(variables) {
        variables = variables.slice();
        variables.sort((variable1, variable2) => util.compareValues(variable1.key, variable2.key));

        let first_set = new Set;
        let sets_map = {};
        let variables_map = {};
        for (let variable of variables) {
            if (variable.before == null) {
                first_set.add(variable.key);
            } else {
                let set_ptr = sets_map[variable.before];
                if (!set_ptr) {
                    set_ptr = new Set;
                    sets_map[variable.before] = set_ptr;
                }

                set_ptr.add(variable.key);
            }

            variables_map[variable.key] = variable;
        }

        let columns = [];
        {
            let next_sets = [first_set];
            let next_set_idx = 0;

            while (next_set_idx < next_sets.length) {
                let set_ptr = next_sets[next_set_idx++];
                let set_start_idx = columns.length;

                while (set_ptr.size) {
                    let frag_start_idx = columns.length;

                    for (let key of set_ptr) {
                        let variable = variables_map[key];

                        if (!set_ptr.has(variable.after)) {
                            let col = {
                                key: key,
                                type: variable.type
                            };
                            columns.push(col);
                        }
                    }

                    reverseLastColumns(columns, frag_start_idx);

                    // Avoid infinite loop that may happen in rare cases
                    if (columns.length === frag_start_idx) {
                        let use_key = set_ptr.values().next().value;

                        let col = {
                            key: use_key,
                            type: variables_map[use_key].type
                        };
                        columns.push(col);
                    }

                    for (let i = frag_start_idx; i < columns.length; i++) {
                        let key = columns[i].key;

                        let next_set = sets_map[key];
                        if (next_set) {
                            next_sets.push(next_set);
                            delete sets_map[key];
                        }

                        delete variables_map[key];
                        set_ptr.delete(key);
                    }
                }

                reverseLastColumns(columns, set_start_idx);
            }
        }

        // Remaining variables (probably from old forms)
        for (let key in variables_map) {
            let col = {
                key: key,
                type: variables_map[key].type
            }
            columns.push(col);
        }

        return columns;
    }

    function reverseLastColumns(columns, start_idx) {
        for (let i = 0; i < (columns.length - start_idx) / 2; i++) {
            let tmp = columns[start_idx + i];
            columns[start_idx + i] = columns[columns.length - i - 1];
            columns[columns.length - i - 1] = tmp;
        }
    }

    function handleEditClick(e, record, current_id) {
        if (record.id !== current_id) {
            dev.run(null, {id: record.id});
        } else {
            dev.run(null, {id: null});
        }
    }

    function showDeleteDialog(e, record, current_id) {
        popup.form(e, page => {
            page.output('Voulez-vous vraiment supprimer cet enregistrement ?');

            page.submitHandler = async () => {
                await record_manager.delete(record.table, record.id);

                if (record.id === current_id) {
                    dev.run(null, {id: null});
                } else {
                    dev.run();
                }
                page.close();
            };
            page.buttons(page.buttons.std.ok_cancel('Supprimer'));
        });
    }
};
