// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let autoform_mod = (function() {
    let self = this;

    let init = false;

    let left_panel = 'editor';
    let show_page_panel = true;

    let editor;
    let editor_history_cache = {};
    let gp_menu;
    let af_data;
    let af_page;

    // TODO: Simplify code with some kind of OrderedMap?
    let pages = [];
    let pages_map = {};
    let current_key;
    let default_key;

    let executor;
    let record_id;

    function pickDefaultKey() {
        if (default_key) {
            return default_key;
        } else if (pages.length) {
            return pages[0].key;
        } else {
            return null;
        }
    }

    function createPage(key, title, is_default) {
        let page = {
            key: key,
            title: title,
            script: ''
        };

        let t = goupil.database.transaction(db => {
            db.save('pages', page);
            if (is_default)
                db.saveWithKey('settings', 'default_page', key);
        });

        t.then(() => {
            pages.push(page);
            pages.sort((page1, page2) => util.compareValues(page1.key, page2.key));
            pages_map[key] = page;

            if (is_default)
                default_key = key;

            self.go(key);
        });
    }

    function editPage(page, title, is_default) {
        let new_page = Object.assign({}, page);
        new_page.title = title;

        let t = goupil.database.transaction(db => {
            db.save('pages', new_page);
            if (is_default) {
                db.saveWithKey('settings', 'default_page', page.key);
            } else if (default_key === page.key) {
                db.delete('settings', 'default_page');
            }
        });

        t.then(() => {
            Object.assign(page, new_page);

            if (is_default) {
                default_key = page.key;
            } else if (page.key === default_key) {
                default_key = null;
            }

            renderAll();
        });
    }

    function deletePage(page) {
        let t = goupil.database.transaction(db => {
            db.delete('pages', page.key);
            if (default_key === page.key)
                db.delete('settings', 'default_page');
        });

        t.then(() => {
            // Remove from pages array and map
            let page_idx = pages.findIndex(it => it.key === page.key);
            pages.splice(page_idx, 1);
            delete pages_map[page.key];

            delete editor_history_cache[page.key];

            if (page.key === default_key)
                default_key = null;

            self.go(pickDefaultKey());
        });
    }

    function resetPages() {
        let t = goupil.database.transaction(db => {
            db.clear('pages');
            for (let page of autoform_default.pages)
                db.save('pages', page);

            db.saveWithKey('settings', 'default_page', autoform_default.key);
        });

        t.then(() => {
            pages = autoform_default.pages.map(page => Object.assign({}, page));
            pages_map = {};
            for (let page of pages)
                pages_map[page.key] = page;

            editor_history_cache = {};

            default_key = autoform_default.key;
            executor = null;
            record_id = null;

            self.go(default_key);
        });
    }

    function showCreatePageDialog(e) {
        goupil.popup(e, form => {
            let key = form.text('key', 'Clé :', {mandatory: true});
            let title = form.text('title', 'Titre :', {mandatory: true});
            let is_default = form.boolean('is_default', 'Page par défaut :',
                                          {untoggle: false, value: false});

            if (key.value) {
                if (pages.some(page => page.key === key.value))
                    key.error('Existe déjà');
                if (!key.value.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
                    key.error('Autorisé : a-z, _ et 0-9 (sauf initiale)');
            }

            form.submitHandler = () => {
                createPage(key.value, title.value, is_default.value);
                form.close();
            };
            form.buttons(form.buttons.std.ok_cancel('Créer'));
        });
    }

    function showEditPageDialog(e, page) {
        goupil.popup(e, form => {
            let title = form.text('title', 'Titre :', {mandatory: true, value: page.title});
            let is_default = form.boolean('is_default', 'Page par défaut :',
                                          {untoggle: false, value: default_key === page.key});

            form.submitHandler = () => {
                editPage(page, title.value, is_default.value);
                form.close();
            };
            form.buttons(form.buttons.std.ok_cancel('Modifier'));
        });
    }

    function showDeletePageDialog(e, page) {
        goupil.popup(e, form => {
            form.output(`Voulez-vous vraiment supprimer la page '${page.key}' ?`);

            form.submitHandler = () => {
                deletePage(page);
                form.close();
            };
            form.buttons(form.buttons.std.ok_cancel('Supprimer'));
        });
    }

    function showResetDialog(e) {
        goupil.popup(e, form => {
            form.output('Voulez-vous vraiment réinitialiser toutes les pages ?');

            form.submitHandler = () => {
                resetPages();
                form.close();
            };
            form.buttons(form.buttons.std.ok_cancel('Réinitialiser'));
        });
    }

    function toggleLeftPanel(type) {
        if (left_panel !== type) {
            left_panel = type;
        } else {
            left_panel = null;
            show_page_panel = true;
        }

        self.activate();
    }

    function togglePagePanel() {
        if (!left_panel)
            left_panel = 'editor';
        show_page_panel = !show_page_panel;

        self.activate();
    }

    function renderAll() {
        let page = pages_map[current_key];

        render(html`
            <button class=${left_panel === 'editor' ? 'active' : ''} @click=${e => toggleLeftPanel('editor')}>Éditeur</button>
            <button class=${left_panel === 'data' ? 'active' : ''} @click=${e => toggleLeftPanel('data')}>Données</button>
            <button class=${show_page_panel ? 'active': ''} @click=${e => togglePagePanel()}>Aperçu</button>

            <select id="af_page_selector" @change=${e => self.go(e.target.value)}>
                ${!current_key && !pages.length ? html`<option>-- No page available --</option>` : html``}
                ${current_key && !page ?
                    html`<option value=${current_key} .selected=${true}>-- Unknown page '${current_key}' --</option>` : html``}
                ${pages.map(page =>
                    html`<option value=${page.key} .selected=${page.key == current_key}>${page.title} [${page.key}]</option>`)}
            </select>
            <button @click=${showCreatePageDialog}>Ajouter</button>
            ${page ? html`<button @click=${e => showEditPageDialog(e, page)}>Modifier</button>
                          <button @click=${e => showDeletePageDialog(e, page)}>Supprimer</button>` : html``}
            <button @click=${showResetDialog}>Réinitialiser</button>
        `, gp_menu);

        if (page) {
            if (editor) {
                editor.setReadOnly(false);
                editor.container.classList.remove('disabled');
            }

            return executor.render(af_page, page.key, page.script);
        } else {
            if (editor) {
                editor.setReadOnly(true);
                editor.container.classList.add('disabled');
            }

            executor.render(af_page);
            if (current_key)
                executor.setError(null, `Unknown page '${current_key}'`);

            return false;
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

                        if (!set_ptr.has(variable.after))
                            columns.push(key);
                    }

                    reverseLastColumns(columns, frag_start_idx);

                    // Avoid infinite loop that may happen in rare cases
                    if (columns.length === frag_start_idx) {
                        let use_key = str_ptr.values().next().value;
                        columns.push(use_key);
                    }

                    for (let i = frag_start_idx; i < columns.length; i++) {
                        let key = columns[i];

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
        for (let key in variables_map)
            columns.push(key);

        return columns;
    }

    function saveRecord(values, variables) {
        variables = variables.map((key, idx) => {
            let ret = {
                before: variables[idx - 1] || null,
                key: key,
                after: variables[idx + 1] || null
            };

            return ret;
        });

        let t = goupil.database.transaction(db => {
            db.saveAll('variables', variables);
            db.save('data', values);
        });

        return t;
    }

    function deleteRecord(id) {
        return goupil.database.delete('data', id);
    }

    function showDeleteRecordDialog(e, id) {
        goupil.popup(e, form => {
            form.output('Voulez-vous vraiment supprimer cet enregistrement ?');

            form.submitHandler = () => {
                let p = deleteRecord(id)
                p.then(loadRecordsAndRender);

                form.close();
            };
            form.buttons(form.buttons.std.ok_cancel('Supprimer'));
        });
    }

    function renderExport(records, variables) {
        let columns = orderColumns(variables);

        render(html`
            <table class="af_export">
                <thead>
                    <tr>
                        <th class="af_head_actions"></th>
                        ${!columns.length ?
                            html`<th>Colonnes inconnues</th>` : html``}
                        ${columns.map(key => html`<th class="af_head_variable">${key}</th>`)}
                    </tr>
                </thead>
                <tbody>
                    ${!records.length ?
                        html`<tr><td colspan=${1 + Math.max(1, columns.length)}>Aucune donnée à afficher</td></tr>` : html``}
                    ${records.map(record => html`<tr>
                        <th>
                            <a href="#" @click=${e => { self.go(current_key, record.id); e.preventDefault(); }}>🔍\uFE0E</a>
                            <a href="#" @click=${e => { showDeleteRecordDialog(e, record.id); e.preventDefault(); }}>✕</a>
                        </th>

                        ${columns.map(key => {
                            let value = record[key];
                            if (value == null)
                                value = '';
                            if (Array.isArray(value))
                                value = value.join('|');

                            return html`<td title=${value}>${value}</td>`;
                        })}
                    </tr>`)}
                </tbody>
            </table>
        `, af_data);
    }

    function loadRecordsAndRender() {
        // Refresh export data
        if (af_data) {
            let p = Promise.all([goupil.database.loadAll('data'),
                                 goupil.database.loadAll('variables')]);

            p.then(values => {
                let [rows, variables] = values;
                renderExport(rows, variables);
            });
        }
    }

    function refreshAndSave() {
        let page = pages_map[current_key];

        if (page) {
            let prev_script = page.script;

            page.script = editor.getValue();

            if (renderAll()) {
                goupil.database.save('pages', page);
            } else {
                // Restore working script
                page.script = prev_script;
            }
        }
    }

    function initEditor() {
        if (document.querySelector('#af_editor')) {
            editor = ace.edit('af_editor');

            editor.setTheme('ace/theme/monokai');
            editor.setShowPrintMargin(false);
            editor.setFontSize(12);
            editor.session.setOption('useWorker', false);
            editor.session.setMode('ace/mode/javascript');

            editor.on('change', e => {
                // If something goes wrong during refreshAndSave(), we don't
                // want to break ACE state.
                setTimeout(refreshAndSave, 0);
            });
        } else {
            editor = null;
        }
    }

    function saveRecordAndReset(values, variables) {
        let p = saveRecord(values, variables);

        p.then(() => {
            log.success('Données sauvegardées !');

            record_id = null;
            self.go(current_key);

            // TODO: Give focus to first widget
            window.scrollTo(0, 0);
        });
    }

    this.go = function(key, id) {
        if (!executor) {
            executor = autoform.createExecutor();
            executor.goHandler = self.go;
            executor.submitHandler = saveRecordAndReset;
        }

        // Load record data
        if (id != null && id !== record_id) {
            goupil.database.load('data', id).then(data => {
                if (data) {
                    executor.setData(data);
                    record_id = id;

                    self.go(key);
                } else {
                    // TODO: Trigger goupil error in this case
                }
            });

            return;
        }
        if (record_id == null) {
            // TODO: Generate ULID-like IDs
            record_id = util.makeULID();
            executor.setData({id: record_id});
        }

        current_key = key;

        // Change current page in editor
        if (editor) {
            let page = pages_map[key];
            if (page) {
                editor.setValue(page.script);
                editor.clearSelection();

                let history = editor_history_cache[key];
                if (!history) {
                    history = new ace.UndoManager();
                    editor_history_cache[key] = history;
                }
                editor.session.setUndoManager(history);
            }
        }

        renderAll();
        loadRecordsAndRender();
    };

    this.activate = function() {
        document.title = `${settings.project_key} — goupil autoform`;

        if (!init) {
            if (!window.ace) {
                let p = util.loadScript(`${settings.base_url}static/ace.js`);
                p.then(self.activate);

                return;
            }

            // NOTE: Migration from localStorage to IndexedDB, drop localStorage
            // support in a month or two.
            try {
                let json = localStorage.getItem('goupil_af_pages');
                let pages2 = JSON.parse(json).map(kv => kv[1]);
                let default_key2 = pages.length ? pages[0].key : null;

                let t = goupil.database.transaction(db => {
                    if (default_key2)
                        db.saveWithKey('settings', 'default_page', default_key2);
                    db.saveAll('pages', pages2);
                });

                t.then(() => {
                    localStorage.removeItem('goupil_af_pages');
                    self.activate();
                });
            } catch (err) {
                let t = goupil.database.transaction(db => {
                    db.load('settings', 'default_page').then(default_page => { default_key = default_page; });
                    db.loadAll('pages').then(pages2 => {
                        if (pages2.length) {
                            pages = Array.from(pages2);
                        } else {
                            pages = autoform_default.pages;
                        }
                        for (let page of pages)
                            pages_map[page.key] = page;
                    });
                });

                t.then(self.activate);

                init = true;
            }
        }

        let main_el = document.querySelector('main');

        if (left_panel === 'editor' && show_page_panel) {
            render(html`
                <div id="af_editor" class="af_panel_left" id="af_editor"></div>
                <div id="af_page" class="af_panel_right" id=""></div>
            `, main_el);
        } else if (left_panel === 'data' && show_page_panel) {
            render(html`
                <div id="af_data" class="af_panel_left"></div>
                <div id="af_page" class="af_panel_right"></div>
            `, main_el);
        } else if (left_panel === 'editor') {
            render(html`
                <div id="af_editor" class="af_panel_fixed"></div>
            `, main_el);
        } else if (left_panel === 'data') {
            render(html`
                <div id="af_data" class="af_panel_fixed"></div>
            `, main_el);
        } else {
            render(html`
                <div id="af_page" class="af_panel_page"></div>
            `, main_el);
        }

        gp_menu = document.querySelector('#gp_menu');
        initEditor();
        af_data = main_el.querySelector('#af_data');
        af_page = main_el.querySelector('#af_page');
        if (!af_page) {
            // We still need to render the form to test it, so create a dummy element!
            af_page = document.createElement('div');
        }

        self.go(current_key || pickDefaultKey());
    };

    return this;
}).call({});
