// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FormExecutor() {
    let self = this;

    let form_key;

    let page_key;
    let page_state;
    let page_scratch;

    let current_record = {};

    this.route = async function(form, id) {
        if (form.key !== form_key || id !== current_record.id || current_record.id == null) {
            if (id != null) {
                current_record = await virt_data.load(form.key, id) || virt_data.create(form.key);
            } else {
                current_record = await virt_data.create(form.key);
            }

            form_key = form.key;
            page_key = null;
        }
    };

    this.runPage = async function(info, code, view_el) {
        if (info.key !== page_key) {
            page_state = new PageState;
            page_scratch = {};
            page_key = info.key;
        }

        let page = new Page;

        // Make page builder
        let page_builder = new PageBuilder(page_state, page);
        page_builder.decodeKey = decodeKey;
        page_builder.setValue = setValue;
        page_builder.getValue = getValue;
        page_builder.changeHandler = () => {
            self.runPage(info, code, view_el);
            window.history.replaceState(null, null, app.makeURL());
        };
        page_builder.submitHandler = saveRecordAndReset;

        // Execute user script
        let func = Function('data', 'route', 'go', 'form', 'page', 'scratch', code);
        func(app.data, app.route, app.go, page_builder, page_builder, page_scratch);

        render(html`<div class="af_page">${page.render()}</div>`, view_el);
    };

    function decodeKey(key) {
        if (!key)
            throw new Error('Empty keys are not allowed');
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed key characters: a-z, _ and 0-9 (not as first character)');

        key = {
            variable: key,
            toString: () => key.variable
        };

        return key;
    };

    function setValue(key, value) {
        current_record.values[key] = value;
    }

    function getValue(key, default_value) {
        if (!current_record.values.hasOwnProperty(key)) {
            current_record.values[key] = default_value;
            return default_value;
        }

        return current_record.values[key];
    }

    async function saveRecordAndReset(page) {
        let entry = new log.Entry();
        entry.progress('Enregistrement en cours');

        await virt_data.save(current_record, page.variables);
        entry.success('Données enregistrées !');

        goupile.run(null, {id: null});
        // XXX: Give focus to first widget
        window.scrollTo(0, 0);
    }

    this.runData = async function() {
        let records = await virt_data.loadAll(form_key);
        let variables = await virt_data.listVariables(form_key);
        let columns = orderColumns(variables);

        renderRecords(records, columns);
    };

    function renderRecords(records, columns) {
        let empty_msg;
        if (!records.length) {
            empty_msg = 'Aucune donnée à afficher';
        } else if (!columns.length) {
            empty_msg = 'Impossible d\'afficher les données (colonnes inconnues)';
            records = [];
        }

        render(html`
            <div class="gp_toolbar">
                <div style="flex: 1;"></div>
                <div class="gp_dropdown right">
                    <button>Export</button>
                    <div>
                        <button ?disabled=${!columns.length} @click=${e => exportSheets(form_key, 'xlsx')}>Excel</button>
                        <button ?disabled=${!columns.length} @click=${e => exportSheets(form_key, 'csv')}>CSV</button>
                    </div>
                </div>
            </div>

            <table class="rec_table" style=${`min-width: ${30 + 60 * columns.length}px`}>
                <thead>
                    <tr>
                        <th class="rec_actions"></th>
                        ${!columns.length ? html`<th>&nbsp;</th>` : ''}
                        ${columns.map(col => html`<th title=${col.key}>${col.key}</th>`)}
                    </tr>
                </thead>

                <tbody>
                    ${empty_msg ?
                        html`<tr><td colspan=${1 + Math.max(1, columns.length)}>${empty_msg}</td></tr>` : ''}
                    ${records.map(record => html`<tr class=${record.id === current_record.id ? 'current' : ''}>
                        <th>
                            <a href="#" @click=${e => { handleEditClick(e, record); e.preventDefault(); }}>🔍\uFE0E</a>
                            <a href="#" @click=${e => { showDeleteDialog(e, record); e.preventDefault(); }}>✕</a>
                        </th>

                        ${columns.map(col => {
                            let value = record.values[col.key];

                            if (value == null) {
                                return html`<td class="missing" title="Donnée manquante">NA</td>`;
                            } else if (Array.isArray(value)) {
                                let text = value.join('|');
                                return html`<td title=${text}>${text}</td>`;
                            } else if (typeof value === 'number') {
                                return html`<td class="number" title=${value}>${value}</td>`;
                            } else {
                                return html`<td title=${value}>${value}</td>`;
                            }
                        })}
                    </tr>`)}
                </tbody>
            </table>
        `, document.querySelector('#dev_data'));
    }

    async function exportSheets(form_key, format = 'xlsx') {
        if (typeof XSLX === 'undefined')
            await util.loadScript(`${env.base_url}static/xlsx.core.min.js`);

        let records = await virt_data.loadAll(form_key);
        let variables = await virt_data.listVariables(form_key);
        let columns = orderColumns(variables);

        // Worksheet
        let ws = XLSX.utils.aoa_to_sheet([columns.map(col => col.key)]);
        for (let record of records) {
            let values = columns.map(col => record.values[col.key]);
            XLSX.utils.sheet_add_aoa(ws, [values], {origin: -1});
        }

        // Workbook
        let wb = XLSX.utils.book_new();
        let ws_name = `${env.app_key}_${dates.today()}`;
        XLSX.utils.book_append_sheet(wb, ws, ws_name);

        let filename = `export_${ws_name}.${format}`;
        switch (format) {
            case 'xlsx': { XLSX.writeFile(wb, filename); } break;
            case 'csv': { XLSX.writeFile(wb, filename, {FS: ';'}); } break;
        }
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

    function handleEditClick(e, record) {
        if (record.id !== current_record.id) {
            goupile.run(null, {id: record.id});
        } else {
            goupile.run(null, {id: null});
        }
    }

    function showDeleteDialog(e, record) {
        goupile.popup(e, page => {
            page.output('Voulez-vous vraiment supprimer cet enregistrement ?');

            page.submitHandler = async () => {
                page.close();

                await virt_data.delete(record.table, record.id);

                if (record.id !== current_record.id) {
                    goupile.run();
                } else {
                    goupile.run(null, {id: null});
                }
            };
            page.buttons(page.buttons.std.ok_cancel('Supprimer'));
        });
    }
};
