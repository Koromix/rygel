// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FormExecutor() {
    let self = this;

    let current_form = {};
    // XXX: Use B-tree instead of two data structures
    let current_records = [];
    let current_ids = new Set;
    let page_states = {};

    let show_complete = true;

    let select_many = false;
    let select_columns = new Set;

    this.route = async function(form, args) {
        if (args.hasOwnProperty('id') || !current_records.length || form.key !== current_form.key) {
            current_form = form;
            current_records.length = 0;
            current_ids.clear();
            page_states = {};

            let record;
            if (args.id != null) {
                record = await virt_data.load(current_form.key, args.id) ||
                         virt_data.create(current_form.key);
            } else {
                record = virt_data.create(current_form.key);
            }

            selectRecord(record);
        }

        current_form = form;
    };

    this.runForm = function(page, code, panel_el) {
        let func = Function('data', 'route', 'go', 'form', 'page', 'scratch', code);

        if (!select_many || select_columns.size) {
            render(html`
                <div class="af_page">${current_records.map(record => {
                    let state = page_states[record.id];

                    if (!state) {
                        state = new PageState;
                        page_states[record.id] = state;
                    }

                    // Pages need to update themselves without doing a full render
                    let el = document.createElement('div');
                    el.className = 'af_entry';

                    runPage(page.key, func, record, state, el);

                    return el;
                })}</div>
            `, panel_el);
        } else {
            render(html`<div class="af_page">Aucune colonne sélectionnée</div>`, panel_el);
        }
    };

    function runPage(key, func, record, state, el) {
        let page = new Page(key);
        let page_builder = new PageBuilder(state, page);

        page_builder.decodeKey = decodeKey;
        page_builder.setValue = (key, value) => setValue(record, key, value);
        page_builder.getValue = (key, default_value) => getValue(record, key, default_value);
        page_builder.changeHandler = () => {
            runPage(key, func, record, state, el);
            // XXX: Get rid of this, which is not compatible with multiple forms
            window.history.replaceState(null, null, app.makeURL());
        };
        page_builder.submitHandler = async () => {
            await saveRecord(record, page);
            goupile.run();
        };

        if (select_many) {
            page_builder.pushOptions({compact: true});
            func(app.data, app.route, app.go, page_builder, page_builder, state.scratch);

            render(html`
                <div class="af_actions">
                    <a href="#" class=${page.errors.length ? 'disabled' : ''}
                       @click=${e => { page_builder.submit(); e.preventDefault(); }}>Enregistrer</a>
                </div>

                ${page.widgets.map(intf => {
                    let visible = intf.key && select_columns.has(intf.key.toString());
                    return visible ? intf.render() : '';
                })}
            `, el);
        } else {
            func(app.data, app.route, app.go, page_builder, page_builder, state.scratch);

            // No variable = no default buttons
            if (page.widgets.some(intf => intf.key)) {
                page_builder.errorList();
                page_builder.buttons([
                    ['Enregistrer', !page.errors.length ? page_builder.submit : null]
                ]);
            }

            render(page.render(), el);
        }
    }

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

    function setValue(record, key, value) {
        record.values[key] = value;
    }

    function getValue(record, key, default_value) {
        if (!record.values.hasOwnProperty(key)) {
            record.values[key] = default_value;
            return default_value;
        }

        return record.values[key];
    }

    async function saveRecord(record, page, complete = false) {
        record.complete[page.key] = complete;

        let entry = new log.Entry();

        entry.progress('Enregistrement en cours');
        try {
            await virt_data.save(record, page.variables);
            entry.success('Données enregistrées !');
        } catch (err) {
            entry.error(`Échec de l\'enregistrement : ${err.message}`);
        }
    }

    this.runStatus = async function() {
        let records = await virt_data.loadAll(current_form.key);
        renderStatus(records);
    };

    function renderStatus(records) {
        let pages = current_form.pages;

        let complete_set = new Set;
        for (let record of records) {
            // XXX: Compute global status at save time
            if (pages.every(page => record.complete[page.key]))
                complete_set.add(record.id);
        }

        render(html`
            <div class="gp_toolbar">
                &nbsp;&nbsp;${records.length} ${records.length > 1 ? 'enregistrements' : 'enregistrement'}
                dont ${complete_set.size} ${complete_set.size > 1 ? 'complets' : 'complet'}

                <div style="flex: 1;"></div>
                <button class=${show_complete ? 'active' : ''}
                        @click=${toggleShowComplete}>Afficher les enregistrements complets</button>
            </div>

            <table class="st_table">
                <thead>
                    <tr>${pages.map(page => html`<th>${page.key}</th>`)}</tr>
                </thead>

                <tbody>
                    ${records.map(record => {
                        if (show_complete || !complete_set.has(record.id)) {
                            return html`
                                <tr>${pages.map(page => {
                                    let complete = record.complete[page.key];

                                    // XXX: Use actual links to form pages when available
                                    if (complete == null) {
                                        return html`<td><a href="#" @click=${e => { handleStatusClick(page, record.id); e.preventDefault(); }}>Non rempli</a></td>`;
                                    } else if (complete) {
                                        return html`<td class="complete"><a href="#" @click=${e => { handleStatusClick(page, record.id); e.preventDefault(); }}>Complet</a></td>`;
                                    } else {
                                        return html`<td class="partial"><a href="#" @click=${e => { handleStatusClick(page, record.id); e.preventDefault(); }}>Partiel</a></td>`;
                                    }
                                })}</tr>
                            `;
                        } else {
                            return '';
                        }
                    })}
                </tbody>
            </table>
        `, document.querySelector('#dev_status'));
    }

    function toggleShowComplete() {
        show_complete = !show_complete;
        goupile.run();
    }

    // XXX: Function is a complete hack, and there are two things needed to fix it: first,
    // we need to incorporate URLs in app objects, and also we need record ID in URLs.
    function handleStatusClick(page, record_id) {
        let url = `${env.base_url}app/${current_form.key}/${page.key !== current_form.key ? (page.key + '/') : ''}`;
        goupile.run(url, {id: record_id}).then(() => window.history.pushState(null, null, app.makeURL()));
    }

    this.runData = async function() {
        let records = await virt_data.loadAll(current_form.key);
        let variables = await virt_data.listVariables(current_form.key);
        let columns = orderColumns(current_form.pages, variables);

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

        let count1 = 0;
        let count0 = 0;
        if (select_many) {
            for (let record of records) {
                if (current_ids.has(record.id)) {
                    count1++;
                } else {
                    count0++;
                }
            }
        }

        render(html`
            <div class="gp_toolbar">
                <button class=${select_many ? 'active' : ''} @click=${e => toggleSelectionMode()}>Sélection multiple</button>
                <div style="flex: 1;"></div>
                <div class="gp_dropdown right">${renderExportMenu()}</div>
            </div>

            <table class="rec_table" style=${`min-width: ${30 + 60 * columns.length}px`}>
                <thead>
                    <tr>
                        <th class="rec_actions">
                            ${select_many ?
                                html`<input type="checkbox" .checked=${count1 && !count0}
                                            .indeterminate=${count1 && count0}
                                            @change=${e => toggleAllRecords(records, e.target.checked)} />` : ''}
                        </th>

                        ${!columns.length ? html`<th>&nbsp;</th>` : ''}
                        ${!select_many ? columns.map(col => html`<th title=${col.key}>${col.key}</th>`) : ''}
                        ${select_many ? columns.map(col =>
                            html`<th title=${col.key}><input type="checkbox" .checked=${select_columns.has(col.key)}
                                                             @change=${e => toggleColumn(col.key)} />${col.key}</th>`) : ''}
                    </tr>
                </thead>

                <tbody>
                    ${empty_msg ?
                        html`<tr><td colspan=${1 + Math.max(1, columns.length)}>${empty_msg}</td></tr>` : ''}
                    ${records.map(record => html`<tr class=${current_ids.has(record.id) ? 'selected' : ''}>
                        ${!select_many ? html`<th><a href="#" @click=${e => { handleEditClick(record); e.preventDefault(); }}>🔍\uFE0E</a>
                                                  <a href="#" @click=${e => { showDeleteDialog(e, record); e.preventDefault(); }}>✕</a></th>` : ''}
                        ${select_many ? html`<th><input type="checkbox" .checked=${current_ids.has(record.id)}
                                                        @click=${e => handleEditClick(record)} /></th>` : ''}

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

    function renderExportMenu() {
        return html`
            <button>Export</button>
            <div>
                <button @click=${e => exportSheets(current_form, 'xlsx')}>Excel</button>
                <button @click=${e => exportSheets(current_form, 'csv')}>CSV</button>
            </div>
        `;
    }

    function toggleSelectionMode() {
        select_many = !select_many;

        if (select_many) {
            select_columns.clear();
        } else if (current_records.length >= 2) {
            let record0 = current_records[0];

            current_records.length = 0;
            current_ids.clear();

            selectRecord(record0);
        }

        goupile.run();
    }

    function toggleAllRecords(records, enable) {
        current_records.length = 0;
        current_ids.clear();

        if (enable) {
            for (let record of records) {
                current_records.push(record);
                current_ids.add(record.id);
            }
        }

        goupile.run();
    }

    function toggleColumn(key) {
        if (select_columns.has(key)) {
            select_columns.delete(key);
        } else {
            select_columns.add(key);
        }

        goupile.run();
    }

    async function exportSheets(form, format = 'xlsx') {
        if (typeof XSLX === 'undefined')
            await util.loadScript(`${env.base_url}static/xlsx.core.min.js`);

        let records = await virt_data.loadAll(form.key);
        let variables = await virt_data.listVariables(form.key);
        let columns = orderColumns(form.pages, variables);

        if (!columns.length) {
            log.error('Impossible d\'exporter pour le moment (colonnes inconnues)');
            return;
        }

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

    // XXX: Improve performance and ordering of variables in different pages
    function orderColumns(pages, variables) {
        variables = variables.slice();
        variables.sort((variable1, variable2) => util.compareValues(variable1.key, variable2.key));

        let variables_map = util.mapArray(variables, variable => variable.key);

        let columns = [];
        for (let page of pages) {
            let first_set = new Set;
            let sets_map = {};
            for (let variable of variables) {
                if (variable.page === page.key) {
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
                }
            }

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
                                page: variable.page,
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
                            page: variables_map[use_key].page,
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

        // Remaining variables (probably from old forms, or discarded pages)
        for (let key in variables_map) {
            let col = {
                page: variables_map[key].page,
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

    function handleEditClick(record) {
        if (!current_ids.has(record.id)) {
            if (!select_many) {
                current_records.length = 0;
                current_ids.clear();
            }

            selectRecord(record);
        } else {
            unselectRecord(record);
        }

        if (!select_many && current_ids.size) {
            // XXX: Hack to get goupile to enable overview panel
            goupile.run(null, {id: record.id});
        } else {
            goupile.run();
        }
    }

    function showDeleteDialog(e, record) {
        goupile.popup(e, page => {
            page.output('Voulez-vous vraiment supprimer cet enregistrement ?');

            page.submitHandler = async () => {
                page.close();

                await virt_data.delete(record.table, record.id);

                if (current_ids.has(record.id))
                    unselectRecord(record);

                goupile.run();
            };
            page.buttons(page.buttons.std.ok_cancel('Supprimer'));
        });
    }

    function selectRecord(record) {
        current_records.push(record);
        current_records.sort((record1, record2) => util.compareValues(record1.id, record2.id));
        current_ids.add(record.id);
    }

    function unselectRecord(record) {
        let idx = current_records.findIndex(it => it.id === record.id);
        current_records.splice(idx, 1);
        current_ids.delete(record.id);
    }
};
