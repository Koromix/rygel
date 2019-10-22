// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let dev = new function() {
    let self = this;

    let assets;
    let assets_map;
    let current_asset;

    let current_record;

    let left_panel;
    let show_overview;

    let editor_el;
    let editor;
    let editor_sessions = new LruMap(32);
    let editor_timer_id;

    this.init = async function() {
        try {
            await loadApplication();
        } catch (err) {
            // The user can still fix main.js or reset the project
        }
    };

    this.go = async function(url = null, args = {}) {
        // Find relevant asset
        if (url) {
            url = new URL(url, window.location.href);

            let path = url.pathname.substr(env.base_url.length);
            current_asset = path ? assets_map[path] : assets[0];
        }

        // Load record (if needed)
        if (current_asset && current_asset.form) {
            if (!args.hasOwnProperty('id') && current_asset.form.key !== current_record.table)
                current_record = {};
            if (args.hasOwnProperty('id') || current_record.id == null) {
                if (args.id == null) {
                    current_record = g_records.create(current_asset.form.key);
                } else if (args.id !== current_record.id) {
                    current_record = await g_records.load(current_asset.form.key, args.id);
                    if (!current_record)
                        current_record = g_records.create(current_asset.form.key);
                }
            }
        }

        // Render menu and page layout
        renderDev();

        // Run appropriate module
        if (current_asset) {
            if (current_asset.category) {
                document.title = `${current_asset.category} :: ${current_asset.label} — ${env.project_key}`;
            } else {
                document.title = `${current_asset.label} — ${env.project_key}`;
            }

            switch (left_panel) {
                case 'editor': { syncEditor(); } break;
                case 'data': { await dev_data.runTable(current_asset.form.key, current_record.id); } break;
            }

            // TODO: Deal with unknown type / renderEmpty()
            await wrapWithLog(runAsset);
        } else {
            document.title = env.project_key;
            log.error('Asset not available');
        }
    };

    async function loadApplication() {
        let prev_assets = assets;

        // Main script, it must always be there
        assets = [{
            type: 'main',
            key: 'main',
            label: 'Script principal',

            path: 'main.js'
        }];

        try {
            let main_script = await loadFileData('main.js');

            await listMainAssets(main_script, assets);
            await listBlobAssets(assets);
        } catch (err) {
            if (prev_assets.length)
                assets = prev_assets;

            throw err;
        } finally {
            assets_map = {};
            for (let asset of assets)
                assets_map[asset.key] = asset;
            current_asset = assets[0];

            current_record = {};

            left_panel = 'editor';
            show_overview = true;
        }
    }

    async function listMainAssets(script, assets) {
        let forms = [];
        let schedules = [];

        let app_builder = new ApplicationBuilder(forms, schedules);

        let func = Function('app', script);
        func(app_builder);

        for (let form of forms) {
            for (let page of form.pages) {
                assets.push({
                    type: 'page',
                    key: `${form.key}/${page.key}`,
                    category: `Formulaire '${form.key}'`,
                    label: `Page '${page.key}'`,

                    form: form,
                    page: page,
                    path: goupil.makePagePath(page.key)
                });
            }
        }

        for (let schedule of schedules) {
            assets.push({
                type: 'schedule',
                key: schedule.key,
                category: 'Agendas',
                label: `Agenda '${schedule.key}'`,

                schedule: schedule
            });
        }
    }

    async function listBlobAssets(assets) {
        let paths = await g_files.list();
        let known_paths = new Set(assets.map(asset => asset.path));

        for (let path of paths) {
            if (!known_paths.has(path)) {
                assets.push({
                    type: 'blob',
                    key: path,
                    category: 'Fichiers',
                    label: path
                });
            }
        }
    }

    function renderDev() {
        let modes = [];
        if (current_asset) {
            if (current_asset.path)
                modes.push(['editor', 'Editeur']);
            if (current_asset.form)
                modes.push(['data', 'Données']);
        }

        if (left_panel && !modes.find(mode => mode[0] === left_panel))
            left_panel = modes[0] ? modes[0][0] : null;
        if (!left_panel)
            show_overview = true;

        render(html`
            ${modes.map(mode =>
                html`<button class=${left_panel === mode[0] ? 'active' : ''} @click=${e => toggleLeftPanel(mode[0])}>${mode[1]}</button>`)}
            ${modes.length ?
                html`<button class=${show_overview ? 'active': ''} @click=${e => toggleOverview()}>Aperçu</button>` : ''}

            <select id="dev_assets" @change=${e => self.go(e.target.value)}>
                ${!current_asset ? html`<option>-- Select an asset --</option>` : ''}
                ${util.mapRLE(assets, asset => asset.category, (category, offset, len) => {
                    if (category) {
                        return html`<optgroup label=${category}>${util.mapRange(offset, offset + len, idx => {
                            let asset = assets[idx];
                            return html`<option value=${'/' + asset.key}
                                                .selected=${asset === current_asset}>${asset.label}</option>`;
                        })}</optgroup>`;
                    } else {
                        return util.mapRange(offset, offset + len, idx => {
                            let asset = assets[idx];
                            return html`<option value=${asset.key}
                                                .selected=${asset === current_asset}>${asset.label}</option>`;
                        });
                    }
                })}
            </select>

            <button @click=${showCreateDialog}>Ajouter</button>
            <button ?disabled=${!current_asset || current_asset.type !== 'blob'}
                    @click=${e => showDeleteDialog(e, current_asset)}>Supprimer</button>
            <button @click=${showResetDialog}>Réinitialiser</button>
        `, document.querySelector('#gp_menu'));

        render(html`
            ${left_panel === 'editor' ?
                makeEditorElement(show_overview ? 'dev_panel_left' : 'dev_panel_fixed') : ''}
            ${left_panel === 'data' ?
                html`<div id="dev_data" class=${show_overview ? 'dev_panel_left' : 'dev_panel_fixed'}></div>` : ''}
            <div id="dev_overview" class=${left_panel ? 'dev_panel_right' : 'dev_panel_page'}
                 style=${show_overview ? '' : 'display: none;'}></div>

            <div id="dev_log" style="display: none;"></div>
        `, document.querySelector('main'));
    }

    function toggleLeftPanel(mode) {
        if (left_panel !== mode) {
            left_panel = mode;
        } else {
            left_panel = null;
            show_overview = true;
        }

        self.go();
    }

    function toggleOverview() {
        if (!left_panel)
            left_panel = 'editor';
        show_overview = !show_overview;

        self.go();
    }

    function showCreateDialog(e) {
        popup.form(e, page => {
            let blob = page.file('file', 'Fichier :');

            let default_path = blob.value ? `static/${blob.value.name}` : null;
            let path = page.text('path', 'Chemin :', {placeholder: default_path});
            if (!path.value)
                path.value = default_path;

            if (path.value) {
                if (!path.value.match(/static\/./)) {
                    path.error('Le chemin doit commencer par \'static/\'');
                } else if (path.value.includes('/../') || path.value.endsWith('/..')) {
                    path.error('Le chemin ne doit pas contenir de composants \'..\'');
                } else if (assets.some(asset => asset.path === path.value)) {
                    path.error('Ce chemin est déjà utilisé');
                }
            }

            page.submitHandler = async () => {
                await g_files.save(g_files.create(path.value, blob.value || ''));

                let asset = {
                    type: 'blob',
                    key: path.value,
                    category: 'Fichiers',
                    label: path.value,

                    path: path.value
                };
                assets.push(asset);
                assets_map[asset.key] = asset;

                page.close();
                self.go(asset.key);
            };
            page.buttons(page.buttons.std.ok_cancel('Créer'));
        });
    }

    function showDeleteDialog(e, asset) {
        popup.form(e, page => {
            page.output(`Voulez-vous vraiment supprimer la ressource '${asset.label}' ?`);

            page.submitHandler = async () => {
                if (asset.path)
                    await g_files.delete(asset.path);

                // Remove from assets array and map
                let asset_idx = assets.findIndex(it => it.key === asset.key);
                assets.splice(asset_idx, 1);
                delete assets_map[asset.key];

                page.close();
                if (asset === current_asset) {
                    let new_asset = assets[Math.max(0, asset_idx - 1)];
                    self.go(new_asset ? new_asset.key : null);
                } else {
                    self.go();
                }
            };
            page.buttons(page.buttons.std.ok_cancel('Supprimer'));
        });
    }

    function showResetDialog(e) {
        popup.form(e, page => {
            page.output('Voulez-vous vraiment réinitialiser toutes les ressources ?');

            page.submitHandler = async () => {
                await g_files.transaction(m => {
                    m.clear();

                    for (let path in help_demo) {
                        let data = help_demo[path];
                        let file = g_files.create(path, data);

                        m.save(file);
                    }
                });

                await self.init();
                editor_sessions.clear();

                page.close();
                self.go();
            };
            page.buttons(page.buttons.std.ok_cancel('Réinitialiser'));
        });
    }

    function makeEditorElement(cls) {
        if (!editor_el) {
            editor_el = document.createElement('div');
            editor_el.id = 'dev_editor';
        }

        for (let cls of editor_el.classList) {
            if (!cls.startsWith('ace_') && !cls.startsWith('ace-'))
                editor_el.classList.remove(cls);
        }
        editor_el.classList.add(cls);

        return editor_el;
    }

    async function syncEditor() {
        if (!editor) {
            // FIXME: Make sure we don't run loadScript more than once
            if (!window.ace)
                await util.loadScript(`${env.base_url}static/ace.js`);

            editor = ace.edit(editor_el);

            editor.setTheme('ace/theme/monokai');
            editor.setShowPrintMargin(false);
            editor.setFontSize(12);
        }

        let session = editor_sessions.get(current_asset.path);
        if (!session) {
            let script = await loadFileData(current_asset.path);

            session = new ace.EditSession(script, 'ace/mode/javascript');
            session.setOption('useWorker', false);
            session.setUndoManager(new ace.UndoManager());

            session.on('change', e => handleEditorChange(current_asset.path, session.getValue()));

            editor_sessions.set(current_asset.path, session);
        }

        if (session !== editor.session) {
            editor.setSession(session);
            editor.setReadOnly(false);
        }
    }

    function handleEditorChange(path, value) {
        clearTimeout(editor_timer_id);

        editor_timer_id = setTimeout(async () => {
            editor_timer_id = null;

            // The user may have changed document (async + timer)
            if (current_asset && current_asset.path === path) {
                let success;
                if (path === 'main.js') {
                    success = await wrapWithLog(async () => {
                        await loadApplication();
                        await self.go();
                    });
                } else {
                    success = await wrapWithLog(runAsset);
                }

                if (success) {
                    let file = g_files.create(path, value);
                    await g_files.save(file);
                }
            }
        }, 60);
    }

    async function loadFileData(path) {
        let session = editor_sessions.get(path);

        if (session) {
            return session.getValue();
        } else {
            let file = await g_files.load(path);
            return file ? file.data : '';
        }
    }

    async function wrapWithLog(func) {
        let log_el = document.querySelector('#dev_log');
        // We still need to render the form to test it, so create a dummy element!
        let page_el = document.querySelector('#dev_overview') || document.createElement('div');

        try {
            await func();

            // Things are OK!
            log_el.innerHTML = '';
            log_el.style.display = 'none';
            page_el.classList.remove('dev_broken');

            return true;
        } catch (err) {
            let err_line = util.parseEvalErrorLine(err);

            log_el.textContent = `⚠\uFE0E Line ${err_line || '?'}: ${err.message}`;
            log_el.style.display = 'block';
            page_el.classList.add('dev_broken');

            return false;
        }
    }

    async function runAsset() {
        switch (current_asset.type) {
            case 'page': {
                let script = await loadFileData(current_asset.path);
                await dev_form.runPageScript(script, current_record);
            } break;

            case 'schedule': { await dev_schedule.run(current_asset.schedule); } break;
        }
    }
};
