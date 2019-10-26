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

    let reload_app;

    this.init = async function() {
        navigator.serviceWorker.register(`${env.base_url}sw.pk.js`);

        try {
            app = await loadApplication();
        } catch (err) {
            // Empty application, so that the user can still fix main.js or reset everything
            app = util.deepFreeze(new ApplicationInfo);
        }

        assets = await listAssets(app);
        assets_map = {};
        for (let asset of assets)
            assets_map[asset.route] = asset;
        current_asset = assets[0];

        current_record = {};

        left_panel = null;
        show_overview = true;

        editor_sessions.clear();
        editor_timer_id = null;

        reload_app = false;
    };

    // Can be launched multiple times (e.g. when main.js is edited)
    async function loadApplication() {
        let app = new ApplicationInfo;
        let app_builder = new ApplicationBuilder(app);

        let main_script = await loadFileData('main.js');
        let func = Function('app', main_script);
        func(app_builder);

        // Application loaded!
        return util.deepFreeze(app);
    }

    async function listAssets(app) {
        // Always add this one, which we need to edit even if is broken and
        // the application cannot be loaded.
        let assets = [{
            type: 'main',
            route: env.base_url,
            label: 'Script principal',

            path: 'main.js'
        }];

        // Application assets
        for (let form of app.forms) {
            for (let page of form.pages) {
                assets.push({
                    type: 'page',
                    route: `${env.base_url}${form.key}/${page.key}/`,
                    category: `Formulaire '${form.key}'`,
                    label: `Page '${page.key}'`,

                    form: form,
                    page: page,
                    path: goupil.makePagePath(page.key)
                });
            }
        }
        for (let schedule of app.schedules) {
            assets.push({
                type: 'schedule',
                route: `${env.base_url}${schedule.key}/`,
                category: 'Agendas',
                label: `Agenda '${schedule.key}'`,

                schedule: schedule
            });
        }

        // Unused (by main.js) files
        try {
            let paths = await file_manager.list();
            let known_paths = new Set(assets.map(asset => asset.path));

            for (let path of paths) {
                if (!known_paths.has(path)) {
                    assets.push({
                        type: 'blob',
                        route: `${env.base_url}${path}`,
                        category: 'Fichiers',
                        label: path
                    });
                }
            }
        } catch (err) {
            log.error(`Failed to list files: ${err.message}`);
        }

        return assets;
    }

    this.run = async function(url = null, args = {}) {
        // Find relevant asset
        if (url) {
            current_asset = assets_map[url];
            if (!current_asset && !url.endsWith('/'))
                current_asset = assets_map[url + '/'];
        }

        // Load record (if needed)
        if (current_asset && current_asset.form) {
            if (!args.hasOwnProperty('id') && current_asset.form.key !== current_record.table)
                current_record = {};
            if (args.hasOwnProperty('id') || current_record.id == null) {
                if (args.id == null) {
                    current_record = record_manager.create(current_asset.form.key);
                } else if (args.id !== current_record.id) {
                    current_record = await record_manager.load(current_asset.form.key, args.id);
                    if (!current_record)
                        current_record = record_manager.create(current_asset.form.key);

                    // The user asked for this record, make sure it is visible
                    if (!show_overview)
                        toggleOverview();
                }
            }
        }

        // Render menu and page layout
        renderDev();

        // Run appropriate module
        if (current_asset) {
            if (current_asset.category) {
                document.title = `${current_asset.category} :: ${current_asset.label} — ${env.app_key}`;
            } else {
                document.title = `${current_asset.label} — ${env.app_key}`;
            }

            switch (left_panel) {
                case 'editor': { syncEditor(); } break;
                case 'data': { await dev_data.runTable(current_asset.form.key, current_record.id); } break;
            }

            await runAssetSafe();
        } else {
            document.title = env.app_key;
            log.error('Asset not available');
        }
    };

    this.makeURL = function() {
        return current_asset ? current_asset.route : null;
    };

    function renderDev() {
        let modes = [];
        if (current_asset) {
            if (current_asset.path)
                modes.push(['editor', 'Éditeur']);
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

            <select id="dev_assets" @change=${e => goupil.go(e.target.value)}>
                ${!current_asset ? html`<option>-- Select an asset --</option>` : ''}
                ${util.mapRLE(assets, asset => asset.category, (category, offset, len) => {
                    if (category) {
                        return html`<optgroup label=${category}>${util.mapRange(offset, offset + len, idx => {
                            let asset = assets[idx];
                            return html`<option value=${asset.route}
                                                .selected=${asset === current_asset}>${asset.label}</option>`;
                        })}</optgroup>`;
                    } else {
                        return util.mapRange(offset, offset + len, idx => {
                            let asset = assets[idx];
                            return html`<option value=${asset.route}
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
        if (window.matchMedia('(pointer: coarse)').matches) {
            left_panel = mode;
            show_overview = false;
        } else if (left_panel !== mode) {
            left_panel = mode;
        } else {
            left_panel = null;
            show_overview = true;
        }

        goupil.go();
    }

    function toggleOverview() {
        if (window.matchMedia('(pointer: coarse)').matches) {
            left_panel = null;
            show_overview = true;
        } else if (!show_overview) {
            show_overview = true;
        } else {
            left_panel = left_panel || 'editor';
            show_overview = false;
        }

        goupil.go();
    }

    function showCreateDialog(e) {
        popup.form(e, page => {
            let blob = page.file('file', 'Fichier :');

            let default_path = blob.value ? `app/${blob.value.name}` : null;
            let path = page.text('path', 'Chemin :', {placeholder: default_path});
            if (!path.value)
                path.value = default_path;

            if (path.value) {
                if (!path.value.match(/app\/./)) {
                    path.error('Le chemin doit commencer par \'app/\'');
                } else if (path.value.includes('/../') || path.value.endsWith('/..')) {
                    path.error('Le chemin ne doit pas contenir de composants \'..\'');
                } else if (assets.some(asset => asset.path === path.value)) {
                    path.error('Ce chemin est déjà utilisé');
                }
            }

            page.submitHandler = async () => {
                await file_manager.save(file_manager.create(path.value, blob.value || ''));

                let asset = {
                    type: 'blob',
                    route: `${env.base_url}${path.value}`,
                    category: 'Fichiers',
                    label: path.value,

                    path: path.value
                };
                assets.push(asset);
                assets_map[asset.route] = asset;

                page.close();
                goupil.go(asset.route);
            };
            page.buttons(page.buttons.std.ok_cancel('Créer'));
        });
    }

    function showDeleteDialog(e, asset) {
        popup.form(e, page => {
            page.output(`Voulez-vous vraiment supprimer la ressource '${asset.label}' ?`);

            page.submitHandler = async () => {
                if (asset.path)
                    await file_manager.delete(asset.path);

                // Remove from assets array and map
                let asset_idx = assets.findIndex(it => it.route === asset.route);
                assets.splice(asset_idx, 1);
                delete assets_map[asset.route];

                page.close();
                if (asset === current_asset) {
                    let new_asset = assets[Math.max(0, asset_idx - 1)];
                    goupil.go(new_asset ? new_asset.route : null);
                } else {
                    goupil.go();
                }
            };
            page.buttons(page.buttons.std.ok_cancel('Supprimer'));
        });
    }

    function showResetDialog(e) {
        popup.form(e, page => {
            page.output('Voulez-vous vraiment réinitialiser toutes les ressources ?');

            page.submitHandler = async () => {
                await file_manager.transaction(m => {
                    m.clear();

                    for (let path in help_demo) {
                        let data = help_demo[path];
                        let file = file_manager.create(path, data);

                        m.save(file);
                    }
                });
                editor_sessions.clear();

                await self.init();

                page.close();
                goupil.go();
            };
            page.buttons(page.buttons.std.ok_cancel('Réinitialiser'));
        });
    }

    function makeEditorElement(cls) {
        if (!editor_el) {
            editor_el = document.createElement('div');
            editor_el.id = 'dev_editor';
            render(html`<div style="height: 100%;"></div>`, editor_el);
        }

        editor_el.className = cls;
        return editor_el;
    }

    async function syncEditor() {
        if (!editor) {
            // FIXME: Make sure we don't run loadScript more than once
            if (typeof ace === 'undefined')
                await util.loadScript(`${env.base_url}static/ace.js`);

            editor = ace.edit(editor_el.children[0]);

            editor.setTheme('ace/theme/monokai');
            editor.setShowPrintMargin(false);
            editor.setFontSize(13);
        }

        let session = editor_sessions.get(current_asset.path);
        if (!session) {
            let script = await loadFileData(current_asset.path);

            session = new ace.EditSession(script, 'ace/mode/javascript');
            session.setOption('useWorker', false);
            session.setUseWrapMode(true);
            session.setUndoManager(new ace.UndoManager());

            session.on('change', e => handleEditorChange(current_asset.path, session.getValue()));

            editor_sessions.set(current_asset.path, session);
        }

        if (session !== editor.session) {
            editor.setSession(session);
            editor.setReadOnly(false);
        }

        editor.resize(false);
    }

    function handleEditorChange(path, value) {
        clearTimeout(editor_timer_id);

        editor_timer_id = setTimeout(async () => {
            editor_timer_id = null;

            // The user may have changed document (async + timer)
            if (current_asset && current_asset.path === path) {
                if (path === 'main.js')
                    reload_app = true;

                if (await runAssetSafe()) {
                    let file = file_manager.create(path, value);
                    await file_manager.save(file);
                }
            }
        }, 60);
    }

    async function loadFileData(path) {
        let session = editor_sessions.get(path);

        if (session) {
            return session.getValue();
        } else {
            let file = await file_manager.load(path);
            return file ? file.data : '';
        }
    }

    async function runAssetSafe() {
        let log_el = document.querySelector('#dev_log');
        let overview_el = document.querySelector('#dev_overview');

        try {
            switch (current_asset.type) {
                case 'main': {
                    if (reload_app) {
                        app = await loadApplication();

                        assets = await listAssets(app);
                        assets_map = {};
                        for (let asset of assets)
                            assets_map[asset.route] = asset;

                        // Old assets must not be used anymore, tell go() to fix current_asset
                        reload_app = false;
                        await goupil.go(env.base_url);
                    }

                    render(html`<div class="dev_wip">Aperçu non disponible pour le moment</div>`,
                           document.querySelector('#dev_overview'));
                } break;

                case 'page': {
                    let script = await loadFileData(current_asset.path);
                    await dev_form.runPageScript(script, current_record);
                } break;
                case 'schedule': { await dev_schedule.run(current_asset.schedule); } break;

                default: {
                    render(html`<div class="dev_wip">Aperçu non disponible pour le moment</div>`,
                           document.querySelector('#dev_overview'));
                } break;
            }

            // Things are OK!
            log_el.innerHTML = '';
            log_el.style.display = 'none';
            overview_el.classList.remove('dev_broken');

            return true;
        } catch (err) {
            let err_line = util.parseEvalErrorLine(err);

            log_el.textContent = `⚠\uFE0E Line ${err_line || '?'}: ${err.message}`;
            log_el.style.display = 'block';
            overview_el.classList.add('dev_broken');

            return false;
        }
    }
};
