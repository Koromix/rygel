// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let form_exec = new function() {
    let self = this;

    let route_page;
    let ctx_records = new BTree;
    let ctx_states = {};

    let show_complete = true;

    let multi_mode = false;
    let multi_columns = new Set;

    this.route = async function(page, what) {
        let new_page = (route_page == null || page.key !== route_page.key);

        what = what || null;
        route_page = page;

        // Clear inappropriate records (wrong form)
        if (ctx_records.size) {
            let record0 = ctx_records.first();

            if (record0.table !== route_page.store)
                ctx_records.clear();
        }

        // Sync context records
        if (what === 'multi') {
            multi_mode = true;
        } else if (what === 'new') {
            let record = ctx_records.first();

            if (!record || record.mtime != null)
                record = vrec.create(route_page.store);

            ctx_records.clear();
            ctx_records.set(record.id, record);

            goupile.toggleOverview(true);
            multi_mode = false;
        } else if (what == null) {
            let record = vrec.create(route_page.store);

            ctx_records.clear();
            ctx_records.set(record.id, record);

            multi_mode = false;
        } else {
            let [id, version] = what.split('@');
            if (version != null)
                version = parseInt(version, 10);

            let record = ctx_records.get(id);
            ctx_records.clear();

            if (record) {
                if (record.table !== route_page.store) {
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
                record = await vrec.load(route_page.store, id, version);

                if (record) {
                    if (version != null && record.version !== version)
                        log.error(`La fiche version @${version} n'existe pas\n`+
                                  `Version chargée : @${record.version}`);
                } else {
                    log.error(`La fiche ${id} n'existe pas`);
                    record = vrec.create(route_page.store);
                }

                delete ctx_states[id];

                goupile.toggleOverview(true);
            } else if (new_page) {
                goupile.toggleOverview(true);
            }

            ctx_records.set(record.id, record);

            multi_mode = false;
        }

        // Clean up unused page states
        let new_states = {};
        for (let id of ctx_records.keys())
            new_states[id] = ctx_states[id];
        ctx_states = new_states;
    };

    this.runPage = function(code, panel_el) {
        let func = Function('util', 'shared', 'go', 'form', 'page', 'state',
                            'values', 'variables', 'route', 'scratch', code);

        if (multi_mode) {
            if (ctx_records.size && multi_columns.size) {
                render(html`
                    ${util.map(ctx_records.values(), record => {
                        // Each entry needs to update itself without doing a full render
                        let el = document.createElement('div');
                        el.className = 'fm_form';

                        runPageMulti(func, record, multi_columns, el);

                        return el;
                    })}
                `, panel_el);
            } else {
                render(html`
                    <div class="fm_form">
                        <div class="af_main">
                            ${ctx_records.size ? 'Aucune colonne sélectionnée'
                                               : 'Aucun enregistrement sélectionné'}
                        </div>
                    </div>
                `, panel_el);
            }
        } else {
            if (!ctx_records.size) {
                let record = vrec.create(route_page.store);
                ctx_records.set(record.id, record);
            }

            let record = ctx_records.first();
            runPage(func, record, panel_el);
        }
    };

    function runPageMulti(func, record, columns, el) {
        let state = ctx_states[record.id];
        if (!state) {
            state = new PageState;
            ctx_states[record.id] = state;
        }

        let model = new PageModel(route_page.key);
        let builder = new PageBuilder(state, model);

        builder.decodeKey = decodeKey;
        builder.setValue = (key, value) => setValue(record, key, value);
        builder.getValue = (key, default_value) => getValue(record, key, default_value);
        builder.submitHandler = async () => {
            if (await saveRecord(record, model.key, model.variables))
                await goupile.go();
        };
        builder.changeHandler = () => runPageMulti(...arguments);

        // Build it!
        builder.pushOptions({compact: true});
        func(util, app.shared, nav.go, builder, builder, state,
             record.values, model.variables, {}, state.scratch);

        render(html`
            <div class="af_main">
                <button type="button" style="float: right;"
                        ?disabled=${builder.hasErrors() || !state.changed}
                        @click=${builder.submit}>Enregistrer</button>

                ${model.widgets.map(intf => {
                    let visible = intf.key && columns.has(intf.key.toString());
                    return visible ? intf.render() : '';
                })}
            </div>
        `, el);

        window.history.replaceState(null, null, makeCurrentURL());
    }

    function runPage(func, record, el) {
        let state = ctx_states[record.id];
        if (!state) {
            state = new PageState;
            ctx_states[record.id] = state;
        }

        let model = new PageModel(route_page.key);
        let readonly = (record.mtime != null && record.version !== record.versions.length - 1);
        let builder = new PageBuilder(state, model, readonly);

        builder.decodeKey = decodeKey;
        builder.setValue = (key, value) => setValue(record, key, value);
        builder.getValue = (key, default_value) => getValue(record, key, default_value);
        builder.submitHandler = async () => {
            if (await saveRecord(record, model.key, model.variables))
                await goupile.go();
        };
        builder.changeHandler = () => runPage(...arguments);

        // Build it!
        func(util, app.shared, nav.go, builder, builder, state,
             record.values, model.variables, nav.route, state.scratch);
        builder.errorList();

        if (route_page.options.default_actions && model.variables.length) {
            let enable_save = !builder.hasErrors() && state.changed;
            let enable_validate = !builder.hasErrors() && !state.changed &&
                                  record.complete[model.key] === false;

            builder.action('Enregistrer', {disabled: !enable_save}, e => builder.submit());
            if (route_page.options.use_validation && user.hasPermission('validate'))
                builder.action('Valider', {disabled: !enable_validate}, e => runValidateDialog(e, record, route_page.key));
            builder.action('-');
            builder.action(record.mtime != null ? 'Nouveau' : 'Réinitialiser',
                           {disabled: !state.changed && record.mtime == null}, e => handleNewClick(e, state.changed));
        }

        let lock = user.getLock();
        let items = lock ? lock.menu : route_page.menu;

        // Check page dependencies?
        let allowed = route_page.options.dependencies.every(dep => record.complete[dep] != null);

        render(html`
            <div class="fm_form">
                ${route_page.options.show_id ? html`
                    <div class="fm_id">
                        ${readonly ?
                            html`<span style="color: red;"
                                       title=${`Enregistrement historique du ${record.mtime.toLocaleString()}`}>⚠\uFE0E ${record.mtime.toLocaleString()}</span>` : ''}
                        <div style="flex: 1;"></div>
                        <span>
                            ${record.mtime == null ? html`Nouvel enregistrement` : ''}
                            ${record.mtime != null && record.sequence == null ? html`Enregistrement local` : ''}
                            ${record.mtime != null && record.sequence != null ? html`Enregistrement n°${record.sequence}` : ''}
                            ${record.mtime != null ? html`(<a @click=${e => runTrailDialog(e, record.id)}>trail</a>)` : ''}
                        </span>
                    </div>
                ` : ''}

                <div class="fm_path">${items.map(item => {
                    let complete = record.complete[item.key];

                    let cls = '';
                    let tooltip = '';
                    let url = makeURL(item.key, record);

                    for (dep of item.dependencies) {
                        if (record.complete[dep] == null) {
                            if (url != null)
                                cls += ' blocked';
                            tooltip += `La page ${dep} doit d'abord être renseignée\n`;
                            url = null;
                        }
                    }

                    if (item.key === route_page.key) {
                        cls += ' active';
                    } else if (state.changed) {
                        cls += ' blocked';
                        tooltip += 'Vous devez enregistrer la page en cours pour continuer\n';
                        url = null;
                    }

                    if (complete == null) {
                        // Leave as is
                    } else if (complete) {
                        cls += ' complete';
                    } else {
                        cls += ' partial';
                    }

                    return html`<a class=${cls} title=${tooltip.trim()} href=${url || ''}>${item.title}</a>`;
                })}</div>

                ${allowed ? html`
                    <div class="af_main">${model.renderWidgets()}</div>
                    <div class=${route_page.options.float_actions ? 'af_actions float' : 'af_actions'}>${model.renderActions()}</div>
                ` : ''}
                ${!allowed ? html`
                    <div class="af_main">
                        Vous ne pouvez pas remplir cette page pour le moment, les pages suivantes ne sont pas remplies :
                        <i>${route_page.options.dependencies.join(', ')}</i>
                    </div>
                ` : ''}
            </div>
        `, el);

        window.history.replaceState(null, null, makeCurrentURL());
    }

    function runTrailDialog(e, id) {
        return dialog.run(e, (d, resolve, reject) => {
            // Goupile restarts popup functions after major state changes to give
            // them a chance to update. This allows us to change the highlighted version!
            let record = ctx_records.get(id);
            if (!record)
                reject();

            d.output(html`
                <table class="tr_table">
                    ${util.mapRange(0, record.versions.length, idx => {
                        let version = record.versions[record.versions.length - idx - 1];
                        let url = makeURL(route_page.key, record, version.version);

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

            d.action('Fermer', {}, resolve);
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

    function runValidateDialog(e, record, page) {
        let msg = 'Confirmez-vous la validation de cette page ?';
        return dialog.confirm(e, msg, 'Valider', async () => {
            if (await validateRecord(record, page))
                goupile.go();
        });
    }

    async function saveRecord(record, page, variables) {
        let entry = new log.Entry();

        entry.progress('Enregistrement en cours');
        try {
            let record2 = await vrec.save(record, page, variables);

            let state = ctx_states[record2.id];
            if (state != null)
                state.changed = false;
            if (ctx_records.has(record2.id))
                ctx_records.set(record2.id, record2);

            if (env.sync_mode === 'mirror' && user.isConnected()) {
                entry.close();
                self.syncRecords();
            } else {
                entry.success('Données enregistrées');
            }

            return true;
        } catch (err) {
            entry.error(`Échec de l\'enregistrement : ${err.message}`);
            return false;
        }
    }

    async function validateRecord(record, page) {
        let entry = new log.Entry();

        entry.progress('Validation en cours');
        try {
            let record2 = await vrec.validate(record, page);
            entry.success('Données validées');

            let state = ctx_states[record2.id];
            if (state != null)
                state.changed = false;
            if (ctx_records.has(record2.id))
                ctx_records.set(record2.id, record2);

            if (env.sync_mode === 'mirror' && user.isConnected()) {
                entry.close();
                self.syncRecords();
            } else {
                entry.success('Données validées');
            }

            return true;
        } catch (err) {
            entry.error(`Échec de la validation : ${err.message}`);
            return false;
        }
    }

    function handleNewClick(e, confirm) {
        if (confirm) {
            let msg = 'Cette action entraînera la perte des modifications en cours, êtes-vous sûr(e) ?';
            return dialog.confirm(e, msg, 'Fermer l\'enregistrement', () => {
                goupile.go(makeURL(route_page.key, null));
            });
        } else {
            goupile.go(makeURL(route_page.key, null));
        }
    }

    this.runStatus = async function() {
        let records = await vrec.loadAll(route_page.store);
        renderStatus(records);
    };

    function renderStatus(records) {
        let items = route_page.menu;

        let complete_set = new Set;
        for (let record of records) {
            // We can't compute this at save time because the set of pages may change anytime
            if (items.every(item => record.complete[item.key]))
                complete_set.add(record.id);
        }

        render(html`
            <div class="gp_toolbar">
                <p>&nbsp;&nbsp;${records.length} ${records.length > 1 ? 'enregistrements' : 'enregistrement'}
                   (${complete_set.size} ${complete_set.size > 1 ? 'complets' : 'complet'})</p>
                <div style="flex: 1;"></div>
                ${env.sync_mode === 'mirror' ? html`<button type="button" @click=${self.syncRecords}>Synchroniser</button>` : ''}
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
                    <col style="width: 2em;"/>
                    <col style=${computeSeqColumnWidth(records[0])} />
                    ${!user.getZone() ? html`<col style="width: 8em;"/>` : ''}
                    ${items.map(item => html`<col/>`)}
                </colgroup>

                <thead>
                    <tr>
                        <th class="actions"></th>
                        <th class="id">ID</th>
                        ${!user.getZone() ? html`<th class="id">Zone</th>` : ''}
                        ${items.map(item => html`<th>${item.title}</th>`)}
                    </tr>
                </thead>

                <tbody>
                    ${!records.length ?
                        html`<tr><td style="text-align: left;"
                                     colspan=${2 + !user.getZone() + Math.max(1, items.length)}>Aucune donnée à afficher</td></tr>` : ''}
                    ${records.map(record => {
                        if (show_complete || !complete_set.has(record.id)) {
                            return html`
                                <tr class=${ctx_records.has(record.id) ? 'selected' : ''}>
                                    <th><a @click=${e => runDeleteDialog(e, record)}>✕</a></th>
                                    <td class="id">${route_page.options.make_seq(record)}</td>
                                    ${!user.getZone() ? html`<td class="id">${record.zone || ''}</td>` : ''}

                                    ${items.map(item => {
                                        let complete = record.complete[item.key];
                                        let url = makeURL(item.key, record);

                                        if (complete == null) {
                                            return html`<td class="none"><a href=${url}>Non rempli&nbsp;&nbsp;🔍\uFE0E</a></td>`;
                                        } else if (complete) {
                                            return html`<td class="complete"><a href=${url}>Validé&nbsp;&nbsp;🔍\uFE0E</a></td>`;
                                        } else {
                                            return html`<td class="partial"><a href=${url}>Enregistré&nbsp;&nbsp;🔍\uFE0E</a></td>`;
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
        let records = await vrec.loadAll(route_page.store);
        let raw_columns = await vrec.listColumns(route_page.store);
        let columns = self.orderColumns(route_page.menu, raw_columns);

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
                if (ctx_records.has(record.id)) {
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
                    <button type="button">Exporter</button>
                    <div>
                        <button type="button" @click=${e => exportSheets(route_page.store, route_page.menu, 'xlsx')}>Excel</button>
                        <button type="button" @click=${e => exportSheets(route_page.store, route_page.menu, 'csv')}>CSV</button>
                    </div>
                </div>
                ${env.sync_mode === 'mirror' ? html`<button type="button" @click=${self.syncRecords}>Synchroniser</button>` : ''}
                <div class="gp_dropdown right">
                    <button type="button">Options</button>
                    <div>
                        <button type="button" class=${multi_mode ? 'active' : ''}
                                @click=${e => toggleSelectionMode()}>Sélection multiple</button>
                    </div>
                </div>
            </div>

            <table class="rec_table">
                <colgroup>
                    <col style="width: 4.5em;"/>
                    <col style=${computeSeqColumnWidth(records[0])} />
                    ${!user.getZone() ? html`<col style="width: 8em;"/>` : ''}
                    ${!columns.length ? html`<col/>` : ''}
                    ${columns.map(col => html`<col style="width: 8em;"/>`)}
                </colgroup>

                <thead>
                    ${columns.length ? html`
                        <tr>
                            <th colspan=${2 + !user.getZone()}></th>
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
                        ${!user.getZone() ? html`<th class="id">Zone</th>` : ''}

                        ${!columns.length ? html`<th>&nbsp;</th>` : ''}
                        ${!multi_mode ? columns.map(col => html`<th title=${col.title}>${col.title}</th>`) : ''}
                        ${multi_mode ? columns.map(col =>
                            html`<th title=${col.title}><input type="checkbox" .checked=${multi_columns.has(col.variable)}
                                                               @change=${e => toggleColumn(col.variable)} />${col.title}</th>`) : ''}
                    </tr>
                </thead>

                <tbody>
                    ${empty_msg ?
                        html`<tr><td colspan=${2 + !user.getZone() + Math.max(1, columns.length)}>${empty_msg}</td></tr>` : ''}
                    ${records.map(record => html`
                        <tr class=${ctx_records.has(record.id) ? 'selected' : ''}>
                            ${!multi_mode ? html`<th>
                                <a href=${makeURL(route_page.key, record)}>🔍\uFE0E</a>&nbsp;
                                <a @click=${e => runDeleteDialog(e, record)}>✕</a>
                            </th>` : ''}
                            ${multi_mode ? html`<th><input type="checkbox" .checked=${ctx_records.has(record.id)}
                                                            @click=${e => handleEditClick(record)} /></th>` : ''}
                            <td class="id">${route_page.options.make_seq(record)}</td>
                            ${!user.getZone() ? html`<td class="id">${record.zone || ''}</td>` : ''}

                            ${columns.map(col => {
                                let [text, cls] = computeColumnValue(record, col);

                                if (cls === 'missing') {
                                    let title;
                                    switch (text) {
                                        case 'NA': { title = 'Non applicable'; } break;
                                        case 'MD': { title = 'Donnée manquante'; } break;
                                    }

                                    return html`<td class=${cls} title=${title}>${text}</td>`;
                                } else {
                                    return html`<td class=${cls}>${text}</td>`;
                                }
                            })}
                        </tr>
                    `)}
                </tbody>
            </table>
        `, document.querySelector('#dev_data'));
    }

    function computeSeqColumnWidth(record) {
        if (record != null) {
            try {
                let seq = '' + route_page.options.make_seq(record);

                let ctx = computeSeqColumnWidth.ctx;
                if (ctx == null) {
                    let canvas = document.createElement('canvas');
                    let style = window.getComputedStyle(document.body);

                    ctx = canvas.getContext('2d');
                    ctx.font = style.getPropertyValue('font-size') + ' ' +
                               style.getPropertyValue('font-family');

                    computeSeqColumnWidth.ctx = ctx;
                }

                let metrics = ctx.measureText(seq);
                return `width: ${Math.ceil(metrics.width * 1.1) + 20}px;`;
            } catch (err) {
                // Not critical, let's return the default width
            }
        }

        return 'width: 60px;';
    }

    function toggleSelectionMode() {
        multi_mode = !multi_mode;

        let record0 = ctx_records.size ? ctx_records.values().next().value : null;

        if (multi_mode) {
            multi_columns.clear();
            if (record0 && record0.mtime == null)
                ctx_records.clear();
        } else if (record0) {
            ctx_records.clear();
            ctx_records.set(record0.id, record0);
        }

        goupile.go();
    }

    function toggleAllRecords(records, enable) {
        ctx_records.clear();

        if (enable) {
            for (let record of records)
                ctx_records.set(record.id, record);
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

    async function exportSheets(store, items, format = 'xlsx') {
        if (typeof XSLX === 'undefined')
            await net.loadScript(`${env.base_url}static/xlsx.core.min.js`);

        let records = await vrec.loadAll(store);
        let raw_columns = await vrec.listColumns(store);
        let columns = self.orderColumns(items, raw_columns);

        if (!columns.length) {
            log.error('Impossible d\'exporter pour le moment (colonnes inconnues)');
            return;
        }

        // Worksheet
        let ws = XLSX.utils.aoa_to_sheet([columns.map(col => col.title)]);
        for (let record of records) {
            let values = columns.map(col => {
                let [value, ] = computeColumnValue(record, col);
                return value;
            });
            XLSX.utils.sheet_add_aoa(ws, [values], {origin: -1});
        }

        // Workbook
        let wb = XLSX.utils.book_new();
        let ws_name = `export_${dates.today()}`;
        XLSX.utils.book_append_sheet(wb, ws, ws_name);

        let filename = `${env.app_key}_${ws_name}.${format}`;
        switch (format) {
            case 'xlsx': { XLSX.writeFile(wb, filename); } break;
            case 'csv': { XLSX.writeFile(wb, filename, {FS: ';'}); } break;
        }
    }

    function computeColumnValue(record, col) {
        let value = record.values[col.variable];

        if (record.complete[col.page] == null) {
            return ['', 'missing'];
        } else if (value == null) {
            if (!record.values.hasOwnProperty(col.variable)) {
                return ['NA', 'missing'];
            } else {
                return ['MD', 'missing'];
            }
        } else if (Array.isArray(value)) {
            if (col.hasOwnProperty('prop')) {
                let text = value.includes(col.prop) ? 1 : 0;
                return [text, 'text'];
            } else {
                let text = value.join('|');
                return [text, 'text'];
            }
        } else if (typeof value === 'number') {
            return [value, 'number'];
        } else {
            return [value, 'text'];
        }
    }

    // XXX: This belongs in VirtualRecords, and it should be able to enumerate pages
    this.orderColumns = function(items, raw_columns) {
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
        for (let item of items) {
            let frag_columns = frags_map[item.key] || [];
            delete frags_map[item.key];

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
                            let col2 = makeColumnInfo(item, col);
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

                        let col2 = makeColumnInfo(item, col);
                        columns.push(col2);
                    }
                }

                reverseLastColumns(columns, set_start_idx);
            }

            // Remaining page cols
            for (let key in cols_map) {
                let col = cols_map[key];

                let col2 = makeColumnInfo(item, col);
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

    function makeColumnInfo(item, col) {
        let col2 = {
            key: col.key,

            page: item.key,
            category: item.title,
            title: col.hasOwnProperty('prop') ? `${col.variable}.${col.prop}` : col.variable,
            variable: col.variable,
            type: col.type
        };

        if (col.hasOwnProperty('prop'))
            col2.prop = col.prop;

        return col2;
    }

    function handleEditClick(record) {
        let enable_overview = false;

        if (!ctx_records.has(record.id)) {
            if (!multi_mode)
                ctx_records.clear();
            ctx_records.set(record.id, record);

            enable_overview = !multi_mode;
        } else {
            ctx_records.delete(record.id);

            if (!multi_mode && !ctx_records.size) {
                let record = vrec.create(route_page.store);
                ctx_records.set(record.id, record);
            }
        }

        if (enable_overview) {
            goupile.toggleOverview(true);
        } else {
            goupile.go();
        }
    }

    function runDeleteDialog(e, record) {
        let msg = 'Voulez-vous vraiment supprimer cet enregistrement ?';
        return dialog.confirm(e, msg, 'Supprimer', async () => {
            await vrec.delete(record.table, record.id);
            ctx_records.delete(record.id);

            if (env.sync_mode === 'mirror' && user.isConnected()) {
                self.syncRecords();
            } else {
                goupile.go();
            }
        });
    }

    this.syncRecords = async function() {
        let entry = new log.Entry;

        entry.progress('Synchronisation des données en cours');
        try {
            let changes = await goupile.runConnected(vrec.sync);

            if (changes.length) {
                // XXX: Make sure the user does not lost any change in progress by
                // finishing the immutable values WIP commit (user changes are stored
                // as some kind of diff on top).
                for (let id of changes) {
                    let prev_record = ctx_records.get(id);

                    if (prev_record != null && prev_record.version === prev_record.versions.length - 1) {
                        let new_record = await vrec.load(route_page.store, id);
                        if (new_record != null)
                            ctx_records.set(id, new_record);
                    }
                }

                entry.success('Données synchronisées');
            } else {
                entry.close();
            }
        } catch (err) {
            entry.error(`La synchronisation a échoué : ${err.message}`);
        }

        goupile.go();
    };
    this.syncRecords = util.serialize(this.syncRecords);

    function makeCurrentURL() {
        let url = `${env.base_url}app/${route_page.key}/`;

        if (multi_mode) {
            url += 'multi';
        } else if (ctx_records.size) {
            let record = ctx_records.first();

            if (record.mtime != null) {
                url += record.id;
                if (record.version !== record.versions.length - 1)
                    url += `@${record.version}`;
            } else {
                let state = ctx_states[record.id];
                if (state && state.changed)
                    url += 'new';
            }
        }

        return util.pasteURL(url, nav.route);
    }

    function makeURL(key, record = null, version = undefined) {
        let url = `${env.base_url}app/${key}/`;

        if (record) {
            if (record.mtime != null) {
                url += record.id;

                if (version != null) {
                    url += `@${version}`;
                } else if (record.version !== record.versions.length - 1) {
                    url += `@${record.version}`;
                }
            } else {
                let state = ctx_states[record.id];
                if (state && state.changed)
                    url += 'new';
            }
        }

        return util.pasteURL(url, nav.route);
    }

    this.hasChanges = function() {
        for (let key in ctx_states) {
            let state = ctx_states[key];

            if (state.changed)
                return true;
        }

        return false;
    };
};
