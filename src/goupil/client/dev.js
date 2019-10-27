// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let dev = new function() {
    let self = this;

    let allow_go = true;

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
        if (navigator.serviceWorker)
            navigator.serviceWorker.register(`${env.base_url}sw.pk.js`);

        try {
            app = await loadApplication();
        } catch (err) {
            // Empty application, so that the user can still fix main.js or reset everything
            app = util.deepFreeze(new Application);
        }

        assets = await listAssets(app);
        assets_map = {};
        for (let asset of assets)
            assets_map[asset.url] = asset;
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
        let app = new Application;
        let app_builder = new ApplicationBuilder(app);

        let main_script = await loadFileData('main.js');
        let func = Function('app', 'data', main_script);
        func(app_builder, app.data);

        app.go = handleGo;

        // Application loaded!
        return util.deepFreeze(app);
    }

    async function listAssets(app) {
        // Always add this one, which we need to edit even if is broken and
        // the application cannot be loaded.
        let assets = [{
            type: 'main',
            url: `${env.base_url}dev/`,
            label: 'Script principal',

            path: 'main.js',
            edit: true
        }];

        // Application assets
        for (let form of app.forms) {
            for (let i = 0; i < form.pages.length; i++) {
                let page = form.pages[i];

                assets.push({
                    type: 'page',
                    url: i ? `${env.base_url}dev/${form.key}/${page.key}/` : `${env.base_url}dev/${form.key}/`,
                    category: `Formulaire '${form.key}'`,
                    label: `Page '${page.key}'`,

                    form: form,
                    page: page,

                    path: goupil.makePagePath(page.key),
                    edit: true
                });
            }
        }
        for (let schedule of app.schedules) {
            assets.push({
                type: 'schedule',
                url: `${env.base_url}dev/${schedule.key}/`,
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
                        url: `${env.base_url}dev/${path}`,
                        category: 'Fichiers',
                        label: path,

                        path: path
                    });
                }
            }
        } catch (err) {
            log.error(`Failed to list files: ${err.message}`);
        }

        return assets;
    }

    // Avoid async here, because it may fail (see allow_go) and the called may need
    // to catch that synchronously.
    function handleGo(url = null, push_history = true) {
        if (!allow_go) {
            throw new Error(`A navigation function (e.g. go()) has been interrupted.
Navigation functions should only be called in reaction to user events, such as button clicks.`);
        }

        if (url) {
            if (url.match(/(http|ftp|https):\/\//g) || url.startsWith('/')) {
                url = new URL(url, window.location.href);
            } else {
                url = new URL(`${env.base_url}dev/${url}`, window.location.href);
            }

            self.run(url.pathname).then(canonical_url => {
                if (push_history) {
                    window.history.pushState(null, null, canonical_url);
                } else {
                    window.history.replaceState(null, null, canonical_url);
                }
            });
        } else {
            self.run();
        }
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

            return current_asset.url;
        } else {
            document.title = env.app_key;
            log.error('Asset not available');

            // Better than nothing!
            return url;
        }
    };

    function renderDev() {
        let modes = [];
        if (current_asset) {
            if (current_asset.edit)
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

            <select id="dev_assets" @change=${e => app.go(e.target.value)}>
                ${!current_asset ? html`<option>-- Select an asset --</option>` : ''}
                ${util.mapRLE(assets, asset => asset.category, (category, offset, len) => {
                    if (category && len == 1) {
                        let asset = assets[offset];
                        return html`<option value=${asset.url}
                                            .selected=${asset === current_asset}>${asset.category} :: ${asset.label}</option>`;
                    } else if (category) {
                        let label = `${category} (${len})`;
                        return html`<optgroup label=${label}>${util.mapRange(offset, offset + len, idx => {
                            let asset = assets[idx];
                            return html`<option value=${asset.url}
                                                .selected=${asset === current_asset}>${asset.label}</option>`;
                        })}</optgroup>`;
                    } else {
                        return util.mapRange(offset, offset + len, idx => {
                            let asset = assets[idx];
                            return html`<option value=${asset.url}
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

        app.go();
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

        app.go();
    }

    function showCreateDialog(e) {
        popup.form(e, page => {
            let blob = page.file('file', 'Fichier :', {mandatory: true});

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
                    url: `${env.base_url}dev/${path.value}`,
                    category: 'Fichiers',
                    label: path.value,

                    path: path.value
                };
                assets.push(asset);
                assets_map[asset.url] = asset;

                page.close();
                app.go(asset.url);
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
                let asset_idx = assets.findIndex(it => it.url === asset.url);
                assets.splice(asset_idx, 1);
                delete assets_map[asset.url];

                page.close();
                if (asset === current_asset) {
                    let new_asset = assets[Math.max(0, asset_idx - 1)];
                    app.go(new_asset ? new_asset.url : null);
                } else {
                    app.go();
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
                app.go(assets[0].url);
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
                            assets_map[asset.url] = asset;

                        // Old assets must not be used anymore, tell go() to fix current_asset
                        reload_app = false;
                        await app.go(assets[0].url);
                    }

                    render(html`<div class="dev_wip">Aperçu non disponible pour le moment</div>`,
                           document.querySelector('#dev_overview'));
                } break;

                case 'page': {
                    // We don't want go() to be fired when a script is opened or changed in the editor,
                    // because then we wouldn't be able to come back to the script to fix the code.
                    allow_go = false;

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
        } finally {
            allow_go = true;
        }
    }
};
