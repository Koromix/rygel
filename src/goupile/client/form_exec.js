// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let form_exec = new function() {
    let self = this;

    let route_page;
    let context_records = new BTree;
    let page_states = {};

    let show_complete = true;

    let multi_mode = false;
    let multi_columns = new Set;

    this.route = async function(page, url) {
        let what = url.pathname.substr(page.url.length) || null;

        if (what && !what.match(/^(new|multi|[A-Z0-9]{26}(@[0-9]+)?)$/))
            throw new Error(`Adresse incorrecte '${url.pathname}'`);
        route_page = page;

        // Clear inappropriate records (wrong form)
        if (context_records.size) {
            let record0 = context_records.first();

            if (record0.table !== route_page.form.key)
                context_records.clear();
        }

        // Sync context records
        if (what === 'multi') {
            multi_mode = true;
        } else if (what === 'new') {
            let record = context_records.first();

            if (!record || record.mtime != null)
                record = vrec.create(route_page.form.key);

            context_records.clear();
            context_records.set(record.id, record);

            multi_mode = false;
        } else if (what == null) {
            let record = vrec.create(route_page.form.key);

            context_records.clear();
            context_records.set(record.id, record);

            multi_mode = false;
        } else {
            let [id, version] = what.split('@');
            if (version != null)
                version = parseInt(version, 10);

            let record = context_records.get(id);
            context_records.clear();

            if (record) {
                if (record.table !== route_page.form.key) {
                    record = null;
                } else if (version != null) {
                    if (version !== record.version)
                        record = null;
                } else {
                    if (record.version !== record.versions.length - 1)
                        record = null;
                }
            }
            if (!record) {
                record = await vrec.load(route_page.form.key, id, version);

                if (record) {
                    if (version != null && record.version !== version)
                        log.error(`La fiche version @${version} n'existe pas\n`+
                                  `Version chargée : @${record.version}`);
                } else {
                    log.error(`La fiche ${id} n'existe pas`);
                    record = vrec.create(route_page.form.key);
                }

                delete page_states[id];
            }

            context_records.set(record.id, record);

            multi_mode = false;
        }

        // Clean up unused page states
        let new_states = {};
        for (let id of context_records.keys())
            new_states[id] = page_states[id];
        page_states = new_states;
    };

    this.runPage = function(code, panel_el) {
        let func = Function('util', 'shared', 'go', 'form', 'page',
                            'values', 'variables', 'route', 'scratch', code);

        if (multi_mode) {
            if (context_records.size && multi_columns.size) {
                render(html`
                    <div class="af_main">${util.map(context_records.values(), record => {
                        // Each entry needs to update itself without doing a full render
                        let el = document.createElement('div');
                        el.className = 'fm_entry';

                        runPageMulti(func, record, multi_columns, el);

                        return el;
                    })}</div>
                `, panel_el);
            } else if (!context_records.size) {
                render(html`<div class="af_main">Aucun enregistrement sélectionné</div>`, panel_el);
            } else {
                render(html`<div class="af_main">Aucune colonne sélectionnée</div>`, panel_el);
            }
        } else {
            if (!context_records.size) {
                let record = vrec.create(route_page.form.key);
                context_records.set(record.id, record);
            }

            let record = context_records.first();
            runPage(func, record, panel_el);
        }
    };

    function runPageMulti(func, record, columns, el) {
        let state = page_states[record.id];
        if (!state) {
            state = new PageState;
            page_states[record.id] = state;
        }

        let model = PageModel(route_page.key);
        let builder = new PageBuilder(state, model);

        builder.decodeKey = decodeKey;
        builder.setValue = (key, value) => setValue(record, key, value);
        builder.getValue = (key, default_value) => getValue(record, key, default_value);
        builder.submitHandler = async () => {
            await saveRecord(record, model);
            state.changed = false;

            await goupile.go();
        };
        builder.changeHandler = () => runPageMulti(...arguments);

        // Build it!
        builder.pushOptions({compact: true});
        func(util, app.shared, nav.go, builder, builder,
             model.values, model.variables, {}, state.scratch);

        render(html`
            <button type="button" class="af_button" style="float: right;"
                    ?disabled=${builder.hasErrors() || !state.changed}
                    @click=${builder.submit}>Enregistrer</button>

            ${model.widgets.map(intf => {
                let visible = intf.key && columns.has(intf.key.toString());
                return visible ? intf.render() : '';
            })}
        `, el);

        window.history.replaceState(null, null, makeCurrentURL());
    }

    function runPage(func, record, el) {
        let state = page_states[record.id];
        if (!state) {
            state = new PageState;
            page_states[record.id] = state;
        }

        let model = new PageModel(route_page.key);
        let readonly = (record.mtime != null && record.version !== record.versions.length - 1);
        let builder = new PageBuilder(state, model, readonly);

        builder.decodeKey = decodeKey;
        builder.setValue = (key, value) => setValue(record, key, value);
        builder.getValue = (key, default_value) => getValue(record, key, default_value);
        builder.submitHandler = async () => {
            if (await saveRecord(record, model)) {
                state.changed = false;
                await goupile.go();
            }
        };
        builder.changeHandler = () => runPage(...arguments);

        // Build it!
        func(util, app.shared, nav.go, builder, builder,
             model.values, model.variables, nav.route, state.scratch);
        builder.errorList();

        let show_actions = route_page.options.actions && model.variables.length;
        let enable_save = !builder.hasErrors() && state.changed;
        let enable_validate = !builder.hasErrors() && !state.changed &&
                              record.complete[model.key] === false;

        if (show_actions) {
            builder.action('Enregistrer', {disabled: !enable_save}, e => builder.submit());
            if (route_page.options.validate)
                builder.action('Valider', {disabled: !enable_save}, e => showValidateDialog(e, builder.submit));
            builder.action('Fermer', {disabled: !state.changed && record.mtime == null}, e => handleNewClick(e, state.changed));
        }

        render(html`
            <div class="fm_form">
                ${route_page.options.actions ? html`
                    <div class="fm_id">
                        ${readonly ?
                            html`<span style="color: red;"
                                       title=${`Enregistrement historique du ${record.mtime.toLocaleString()}`}>⚠\uFE0E ${record.mtime.toLocaleString()}</span>` : ''}
                        <div style="flex: 1;"></div>
                        <span>
                            ${record.mtime == null ? html`Nouvel enregistrement` : ''}
                            ${record.mtime != null && record.sequence == null ? html`Enregistrement local` : ''}
                            ${record.mtime != null && record.sequence != null ? html`Enregistrement n°${record.sequence}` : ''}
                            ${record.mtime != null ? html`(<a @click=${e => showTrailDialog(e, record.id)}>trail</a>)` : ''}
                        </span>
                    </div>
                ` : ''}

                <div class="fm_path">${route_page.form.pages.map(page2 => {
                    let complete = record.complete[page2.key];

                    let cls = '';
                    if (page2.key === model.key)
                        cls += ' active';
                    if (complete == null) {
                        // Leave as is
                    } else if (complete) {
                        cls += ' complete';
                    } else {
                        cls += ' partial';
                    }

                    return html`<a class=${cls} href=${makeURL(route_page.form.key, page2.key, record)}>${page2.label}</a>`;
                })}</div>

                <div class="af_main">${model.renderWidgets()}</div>
                <div class=${route_page.options.float_actions ? 'af_actions float' : 'af_actions'}>${model.renderActions()}</div>
            </div>
        `, el);

        window.history.replaceState(null, null, makeCurrentURL());
    }

    function showTrailDialog(e, id) {
        goupile.popup(e, null, (page, close) => {
            // Goupile restarts popup functions after major state changes to give
            // them a chance to update. This allows us to change the highlighted version!
            let record = context_records.get(id);
            if (!record)
                close();

            page.output(html`
                <table class="tr_table">
                    ${util.mapRange(0, record.versions.length, idx => {
                        let version = record.versions[record.versions.length - idx - 1];
                        let url = makeURL(route_page.form.key, route_page.key, record, version.version);

                        return html`
                            <tr class=${record.version === version.version ? 'selected' : ''}>
                                <td><a href=${url}>🔍\uFE0E</a></td>
                                <td>${version.mtime.toLocaleString()}</td>
                                <td>${version.username || '(local)'}</td>
                            </tr>
                        `;
                    })}
                </table>
            `);
        });
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

    async function saveRecord(record, model) {
        let entry = new log.Entry();

        entry.progress('Enregistrement en cours');
        try {
            let record2 = await vrec.save(record, model.key, model.variables);
            entry.success('Données enregistrées');

            if (context_records.has(record2.id))
                context_records.set(record2.id, record2);

            return true;
        } catch (err) {
            entry.error(`Échec de l\'enregistrement : ${err.message}`);
            return false;
        }
    }

    function handleNewClick(e, confirm) {
        if (confirm) {
            goupile.popup(e, 'Fermer l\'enregistrement', (page, close) => {
                page.output('Cette action entraînera la perte des modifications en cours, êtes-vous sûr(e) ?');

                page.submitHandler = () => {
                    close();
                    goupile.go(makeURL(route_page.form.key, route_page.key, null));
                };
            })
        } else {
            goupile.go(makeURL(route_page.form.key, route_page.key, null));
        }
    }

    function showValidateDialog(e, submit_func) {
        goupile.popup(e, 'Valider', (page, close) => {
            page.output('Confirmez-vous la validation de cette page ?');

            page.submitHandler = () => {
                close();
                submit_func(true);
            };
        });
    }

    this.runStatus = async function() {
        let records = await vrec.loadAll(route_page.form.key);
        renderStatus(records);
    };

    function renderStatus(records) {
        let pages = route_page.form.pages;

        let complete_set = new Set;
        for (let record of records) {
            // We can't compute this at save time because the set of pages may change anytime
            if (pages.every(page => record.complete[page.key]))
                complete_set.add(record.id);
        }

        render(html`
            <div class="gp_toolbar">
                <p>&nbsp;&nbsp;${records.length} ${records.length > 1 ? 'enregistrements' : 'enregistrement'}
                   (${complete_set.size} ${complete_set.size > 1 ? 'complets' : 'complet'})</p>
                <div style="flex: 1;"></div>
                <div class="gp_dropdown right">
                    <button type="button">Options</button>
                    <div>
                        <button type="button" class=${!show_complete ? 'active' : ''}
                                @click=${toggleShowComplete}>Cacher complets</button>
                    </div>
                </div>
            </div>

            <table class="st_table">
                <colgroup>
                    <col style="width: 3em;"/>
                    <col style="width: 60px;"/>
                    ${pages.map(col => html`<col/>`)}
                </colgroup>

                <thead>
                    <tr>
                        <th class="actions"></th>
                        <th class="id">ID</th>
                        ${pages.map(page => html`<th>${page.label}</th>`)}
                    </tr>
                </thead>

                <tbody>
                    ${!records.length ?
                        html`<tr><td style="text-align: left;"
                                     colspan=${2 + Math.max(1, pages.length)}>Aucune donnée à afficher</td></tr>` : ''}
                    ${records.map(record => {
                        if (show_complete || !complete_set.has(record.id)) {
                            return html`
                                <tr class=${context_records.has(record.id) ? 'selected' : ''}>
                                    <th>
                                        <a @click=${e => handleEditClick(record)}>🔍\uFE0E</a>
                                        <a @click=${e => showDeleteDialog(e, record)}>✕</a>
                                    </th>
                                    <td class="id">${record.sequence || 'local'}</td>

                                    ${pages.map(page => {
                                        let complete = record.complete[page.key];

                                        if (complete == null) {
                                            return html`<td class="none"><a href=${makeURL(route_page.form.key, page.key, record)}>Non rempli</a></td>`;
                                        } else if (complete) {
                                            return html`<td class="complete"><a href=${makeURL(route_page.form.key, page.key, record)}>Validé</a></td>`;
                                        } else {
                                            return html`<td class="partial"><a href=${makeURL(route_page.form.key, page.key, record)}>Enregistré</a></td>`;
                                        }
                                    })}
                                </tr>
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
        goupile.go();
    }

    this.runData = async function() {
        let records = await vrec.loadAll(route_page.form.key);
        let raw_columns = await vrec.listColumns(route_page.form.key);
        let columns = orderColumns(route_page.form.pages, raw_columns);

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
        if (multi_mode) {
            for (let record of records) {
                if (context_records.has(record.id)) {
                    count1++;
                } else {
                    count0++;
                }
            }
        }

        render(html`
            <div class="gp_toolbar">
                <p>&nbsp;&nbsp;${records.length} ${records.length > 1 ? 'enregistrements' : 'enregistrement'}</p>
                <div style="flex: 1;"></div>
                <div class="gp_dropdown right">
                    <button type="button">Export</button>
                    <div>
                        <button type="button" @click=${e => exportSheets(route_page.form, 'xlsx')}>Excel</button>
                        <button type="button" @click=${e => exportSheets(route_page.form, 'csv')}>CSV</button>
                    </div>
                </div>
                <div class="gp_dropdown right">
                    <button type="button">Options</button>
                    <div>
                        <button type="button" class=${multi_mode ? 'active' : ''}
                                @click=${e => toggleSelectionMode()}>Sélection multiple</button>
                    </div>
                </div>
            </div>

            <table class="rec_table" style=${`min-width: ${30 + 60 * columns.length}px`}>
                <colgroup>
                    <col style="width: 3em;"/>
                    <col style="width: 60px;"/>
                    ${!columns.length ? html`<col/>` : ''}
                    ${columns.map(col => html`<col style="width: 8em;"/>`)}
                </colgroup>

                <thead>
                    ${columns.length ? html`
                        <tr>
                            <th colspan="2"></th>
                            ${util.mapRLE(columns, col => col.category, (page, offset, len) =>
                                html`<th class="rec_page" colspan=${len}>${page}</th>`)}
                        </tr>
                    ` : ''}
                    <tr>
                        <th class="actions">
                            ${multi_mode ?
                                html`<input type="checkbox" .checked=${count1 && !count0}
                                            .indeterminate=${count1 && count0}
                                            @change=${e => toggleAllRecords(records, e.target.checked)} />` : ''}
                        </th>
                        <th class="id">ID</th>

                        ${!columns.length ? html`<th>&nbsp;</th>` : ''}
                        ${!multi_mode ? columns.map(col => html`<th title=${col.title}>${col.title}</th>`) : ''}
                        ${multi_mode ? columns.map(col =>
                            html`<th title=${col.title}><input type="checkbox" .checked=${multi_columns.has(col.variable)}
                                                               @change=${e => toggleColumn(col.variable)} />${col.title}</th>`) : ''}
                    </tr>
                </thead>

                <tbody>
                    ${empty_msg ?
                        html`<tr><td colspan=${2 + Math.max(1, columns.length)}>${empty_msg}</td></tr>` : ''}
                    ${records.map(record => html`
                        <tr class=${context_records.has(record.id) ? 'selected' : ''}>
                            ${!multi_mode ? html`<th><a @click=${e => handleEditClick(record)}>🔍\uFE0E</a>
                                                      <a @click=${e => showDeleteDialog(e, record)}>✕</a></th>` : ''}
                            ${multi_mode ? html`<th><input type="checkbox" .checked=${context_records.has(record.id)}
                                                            @click=${e => handleEditClick(record)} /></th>` : ''}
                            <td class="id">${record.sequence || 'local'}</td>

                            ${columns.map(col => {
                                let value = record.values[col.variable];

                                if (record.complete[col.page] == null) {
                                    return html`<td class="missing" title="Page non remplie"></td>`;
                                } else if (value == null) {
                                    if (!record.values.hasOwnProperty(col.variable)) {
                                        return html`<td class="missing" title="Non applicable">NA</td>`;
                                    } else {
                                        return html`<td class="missing" title="Donnée manquante">MD</td>`;
                                    }
                                } else if (Array.isArray(value)) {
                                    if (col.hasOwnProperty('prop')) {
                                        let text = value.includes(col.prop) ? 1 : 0;
                                        return html`<td title=${text}>${text}</td>`;
                                    } else {
                                        let text = value.join('|');
                                        return html`<td title=${text}>${text}</td>`;
                                    }
                                } else if (typeof value === 'number') {
                                    return html`<td class="number" title=${value}>${value}</td>`;
                                } else {
                                    return html`<td title=${value}>${value}</td>`;
                                }
                            })}
                        </tr>
                    `)}
                </tbody>
            </table>
        `, document.querySelector('#dev_data'));
    }

    function toggleSelectionMode() {
        multi_mode = !multi_mode;

        let record0 = context_records.size ? context_records.values().next().value : null;

        if (multi_mode) {
            multi_columns.clear();
            if (record0 && record0.mtime == null)
                context_records.clear();
        } else if (record0) {
            context_records.clear();
            context_records.set(record0.id, record0);
        }

        goupile.go();
    }

    function toggleAllRecords(records, enable) {
        context_records.clear();

        if (enable) {
            for (let record of records)
                context_records.set(record.id, record);
        }

        goupile.go();
    }

    function toggleColumn(key) {
        if (multi_columns.has(key)) {
            multi_columns.delete(key);
        } else {
            multi_columns.add(key);
        }

        goupile.go();
    }

    async function exportSheets(form, format = 'xlsx') {
        if (typeof XSLX === 'undefined')
            await net.loadScript(`${env.base_url}static/xlsx.core.min.js`);

        let records = await vrec.loadAll(form.key);
        let raw_columns = await vrec.listColumns(form.key);
        let columns = orderColumns(form.pages, raw_columns);

        if (!columns.length) {
            log.error('Impossible d\'exporter pour le moment (colonnes inconnues)');
            return;
        }

        // Worksheet
        let ws = XLSX.utils.aoa_to_sheet([columns.map(col => col.variable)]);
        for (let record of records) {
            let values = columns.map(col => record.values[col.variable]);
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

    function orderColumns(pages, raw_columns) {
        raw_columns = raw_columns.slice();
        raw_columns.sort((col1, col2) => util.compareValues(col1.key, col2.key));

        let frags_map = {};
        for (let col of raw_columns) {
            let frag_columns = frags_map[col.page];
            if (!frag_columns) {
                frag_columns = [];
                frags_map[col.page] = frag_columns;
            }

            frag_columns.push(col);
        }

        let columns = [];
        for (let page of pages) {
            let frag_columns = frags_map[page.key] || [];
            delete frags_map[page.key];

            let cols_map = util.arrayToObject(frag_columns, col => col.key);

            let first_set = new Set;
            let sets_map = {};
            for (let col of frag_columns) {
                if (col.before == null) {
                    first_set.add(col.key);
                } else {
                    let set_ptr = sets_map[col.before];
                    if (!set_ptr) {
                        set_ptr = new Set;
                        sets_map[col.before] = set_ptr;
                    }

                    set_ptr.add(col.key);
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
                        let col = cols_map[key];

                        if (!set_ptr.has(col.after)) {
                            let col2 = makeColumnInfo(page, col);
                            columns.push(col2);
                        }
                    }

                    if (frag_start_idx < columns.length) {
                        reverseLastColumns(columns, frag_start_idx);

                        for (let i = frag_start_idx; i < columns.length; i++) {
                            let key = columns[i].key;

                            let next_set = sets_map[key];
                            if (next_set) {
                                next_sets.push(next_set);
                                delete sets_map[key];
                            }

                            delete cols_map[key];
                            set_ptr.delete(key);
                        }
                    } else {
                        // Avoid infinite loop that may happen in rare cases
                        let use_key = set_ptr.values().next().value;
                        let col = cols_map[use_key];

                        let col2 = makeColumnInfo(page, col);
                        columns.push(col2);
                    }
                }

                reverseLastColumns(columns, set_start_idx);
            }

            // Remaining page cols
            for (let key in cols_map) {
                let col = cols_map[use_key];

                let col2 = makeColumnInfo(page, col);
                columns.push(col2);
            }
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

    function makeColumnInfo(page, col) {
        let col2 = {
            key: col.key,

            page: page.key,
            category: page.label,
            title: col.hasOwnProperty('prop') ? `${col.variable}.${col.prop}` : col.variable,
            variable: col.variable,
            prop: col.prop,
            type: col.type
        };

        return col2;
    }

    function handleEditClick(record) {
        let enable_overview = false;

        if (!context_records.has(record.id)) {
            if (!multi_mode)
                context_records.clear();
            context_records.set(record.id, record);

            enable_overview = !multi_mode;
        } else {
            context_records.delete(record.id);

            if (!multi_mode && !context_records.size) {
                let record = vrec.create(route_page.form.key);
                context_records.set(record.id, record);
            }
        }

        if (enable_overview) {
            goupile.toggleOverview(true);
        } else {
            goupile.go();
        }
    }

    function showDeleteDialog(e, record) {
        goupile.popup(e, 'Supprimer', (page, close) => {
            page.output('Voulez-vous vraiment supprimer cet enregistrement ?');

            page.submitHandler = async () => {
                close();

                await vrec.delete(record.table, record.id);
                context_records.delete(record.id, record);

                goupile.go();
            };
        });
    }

    function makeCurrentURL() {
        let url = `${env.base_url}app/${route_page.form.key}/${route_page.key}/`;

        if (multi_mode) {
            url += 'multi';
        } else if (context_records.size) {
            let record = context_records.first();

            if (record.mtime != null) {
                url += record.id;
                if (record.version !== record.versions.length - 1)
                    url += `@${record.version}`;
            } else {
                let state = page_states[record.id];
                if (state && state.changed)
                    url += 'new';
            }
        }

        return util.pasteURL(url, nav.route);
    }

    function makeURL(form_key, page_key, record = null, version = undefined) {
        let url = `${env.base_url}app/${form_key}/${page_key}/`;

        if (record) {
            if (record.mtime != null) {
                url += record.id;

                if (version != null) {
                    url += `@${version}`;
                } else if (record.version !== record.versions.length - 1) {
                    url += `@${record.version}`;
                }
            } else {
                let state = page_states[record.id];
                if (state && state.changed)
                    url += 'new';
            }
        }

        return util.pasteURL(url, nav.route);
    }
};
