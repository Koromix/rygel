// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let admin_form = (function() {
    let self = this;

    let form_el;
    let editor_el;
    let data_el;

    let left_panel = 'editor';
    let show_main_panel = true;

    let editor;
    let editor_history_cache = {};

    let executor;
    let current_asset;
    let current_record = {};

    function toggleLeftPanel(type) {
        if (left_panel !== type) {
            left_panel = type;
        } else {
            left_panel = null;
            show_main_panel = true;
        }

        pilot.go();
    }

    function toggleMainPanel() {
        if (!left_panel)
            left_panel = 'editor';
        show_main_panel = !show_main_panel;

        pilot.go();
    }

    function makeEditorElement(cls) {
        if (!editor_el) {
            editor_el = document.createElement('div');
            editor_el.id = 'af_editor';
        }

        for (let cls of editor_el.classList) {
            if (!cls.startsWith('ace_') && !cls.startsWith('ace-'))
                editor_el.classList.remove(cls);
        }
        editor_el.classList.add(cls);

        return editor_el;
    }

    function renderLayout() {
        let main_el = document.querySelector('main');

        if (left_panel === 'editor' && show_main_panel) {
            render(html`
                ${makeEditorElement('adm_panel_left')}
                <div id="af_form" class="adm_panel_right"></div>
            `, main_el);
        } else if (left_panel === 'data' && show_main_panel) {
            render(html`
                <div id="af_data" class="adm_panel_left"></div>
                <div id="af_form" class="adm_panel_right"></div>
            `, main_el);
        } else if (left_panel === 'editor') {
            render(html`
                ${makeEditorElement('adm_panel_fixed')}
            `, main_el);
        } else if (left_panel === 'data') {
            render(html`
                <div id="af_data" class="adm_panel_fixed"></div>
            `, main_el);
        } else {
            render(html`
                <div id="af_form" class="adm_panel_page"></div>
            `, main_el);
        }

        modes_el = document.querySelector('#adm_modes');
        // We still need to render the form to test it, so create a dummy element!
        form_el = document.querySelector('#af_form') || document.createElement('div');
        data_el = document.querySelector('#af_data');
    }

    function renderModes() {
        render(html`
            <button class=${left_panel === 'editor' ? 'active' : ''} @click=${e => toggleLeftPanel('editor')}>Éditeur</button>
            <button class=${left_panel === 'data' ? 'active' : ''} @click=${e => toggleLeftPanel('data')}>Données</button>
            <button class=${show_main_panel ? 'active': ''} @click=${e => toggleMainPanel()}>Aperçu</button>
        `, modes_el);
    }

    function renderForm() {
        return executor.render(form_el, current_asset.key, current_asset.data);
    }

    function handleEditorChange() {
        if (current_asset) {
            let prev_script = current_asset.data;

            current_asset.data = editor.getValue();

            if (renderForm()) {
                g_assets.save(current_asset);
            } else {
                // Restore working script
                current_asset.data = prev_script;
            }
        }
    }

    async function syncEditor() {
        // FIXME: Make sure we don't run loadScript more than once
        if (!window.ace)
            await util.loadScript(`${env.base_url}static/ace.js`);

        if (left_panel === 'editor') {
            if (!editor) {
                editor = ace.edit(editor_el);

                editor.setTheme('ace/theme/monokai');
                editor.setShowPrintMargin(false);
                editor.setFontSize(12);
                editor.session.setOption('useWorker', false);
                editor.session.setMode('ace/mode/javascript');

                editor.on('change', e => {
                    // If something goes wrong during handleEditorChange(), we don't
                    // want to break ACE state.
                    setTimeout(handleEditorChange, 0);
                });
            }

            if (current_asset) {
                let history = editor_history_cache[current_asset.key];

                if (history !== editor.session.getUndoManager()) {
                    editor.setValue(current_asset.data);
                    editor.setReadOnly(false);
                    editor.clearSelection();

                    if (!history) {
                        history = new ace.UndoManager();
                        editor_history_cache[current_asset.key] = history;
                    }
                    editor.session.setUndoManager(history);
                }
            } else {
                editor.setValue('');
                editor.setReadOnly(true);

                editor.session.setUndoManager(new ace.UndoManager());
            }
        }
    }

    function reverseLastColumns(columns, start_idx) {
        for (let i = 0; i < (columns.length - start_idx) / 2; i++) {
            let tmp = columns[start_idx + i];
            columns[start_idx + i] = columns[columns.length - i - 1];
            columns[columns.length - i - 1] = tmp;
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
                        let use_key = str_ptr.values().next().value;

                        let col = {
                            key: use_key,
                            type: variables_map[use_key].type,
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

    async function exportToExcel(table) {
        if (!window.XLSX)
            await util.loadScript(`${env.base_url}static/xlsx.core.min.js`);

        let [records, variables] = await g_records.loadAll(table);
        let columns = orderColumns(variables);

        let export_name = `${env.project_key}_${dates.today()}`;
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

    function handleEditClick(e, record) {
        if (record.id !== current_record.id) {
            pilot.go(null, {id: record.id});
        } else {
            pilot.go(null, {id: null});
        }
    }

    function showDeleteDialog(e, record) {
        goupil.popup(e, form => {
            form.output('Voulez-vous vraiment supprimer cet enregistrement ?');

            form.submitHandler = async () => {
                await g_records.delete(record.table, record.id);

                if (current_record.id === record.id) {
                    pilot.go(null, {id: null});
                } else {
                    pilot.go();
                }
                form.close();
            };
            form.buttons(form.buttons.std.ok_cancel('Supprimer'));
        });
    }

    async function renderRecords(table) {
        if (left_panel === 'data') {
            let [records, variables] = await g_records.loadAll(table);
            let columns = orderColumns(variables);

            let empty_msg;
            if (!records.length) {
                empty_msg = 'Aucune donnée à afficher';
            } else if (!columns.length) {
                empty_msg = 'Impossible d\'afficher les données (colonnes inconnues)';
                records = [];
            }

            render(html`
                <table class="af_records" style=${`min-width: ${30 + 60 * columns.length}px`}>
                    <thead>
                        <tr>
                            <th class="af_head_actions">
                                ${columns.length ?
                                    html`<button class="af_excel" @click=${e => exportToExcel(table)}></button>` : ''}
                            </th>
                            ${!columns.length ? html`<th></th>` : ''}
                            ${columns.map(col => html`<th class="af_head_variable">${col.key}</th>`)}
                        </tr>
                    </thead>
                    <tbody>
                        ${empty_msg ?
                            html`<tr><td colspan=${1 + Math.max(1, columns.length)}>${empty_msg}</td></tr>` : ''}
                        ${records.map(record => html`<tr class=${record.id === current_record.id ? 'af_row_current' : ''}>
                            <th>
                                <a href="#" @click=${e => { handleEditClick(e, record); e.preventDefault(); }}>🔍\uFE0E</a>
                                <a href="#" @click=${e => { showDeleteDialog(e, record); e.preventDefault(); }}>✕</a>
                            </th>

                            ${columns.map(col => {
                                let value = record.values[col.key];

                                let tooltip;
                                let cls;
                                if (value != null) {
                                    if (Array.isArray(value))
                                        value = value.join('|');

                                    tooltip = value;
                                    cls = `af_record_${col.type}`;
                                } else {
                                    value = 'NA';

                                    tooltip = 'Donnée manquante';
                                    cls = `af_record_missing`;
                                }

                                return html`<td class=${cls} title=${tooltip}>${value}</td>`;
                            })}
                        </tr>`)}
                    </tbody>
                </table>
            `, data_el);
        }
    }

    async function saveRecordAndReset(values, variables) {
        let entry = new log.Entry();
        entry.progress('Enregistrement en cours');

        await g_records.save(current_record, variables);
        entry.success('Données enregistrées !');

        pilot.go(null, {id: null});
        // TODO: Give focus to first widget
        window.scrollTo(0, 0);
    }

    this.run = async function(asset, args) {
        // Deal with form executor
        if (!executor) {
            executor = new FormExecutor();
            executor.goHandler = (key, args) => pilot.go(key, args);
            executor.submitHandler = saveRecordAndReset;
        }

        // Load record (if needed)
        if (!args.hasOwnProperty('id') && asset.key !== current_record.table)
            current_record = {};
        if (args.hasOwnProperty('id') || current_record.id == null) {
            if (args.id == null) {
                current_record = g_records.create(asset.key);
            } else if (args.id !== current_record.id) {
                current_record = await g_records.load(asset.key, args.id);
            }

            executor.setData(current_record.values);
        }
        current_asset = asset;

        // Render
        renderLayout();
        renderModes();
        renderForm();
        syncEditor();
        renderRecords(asset.key);
    };

    return this;
}).call({});
