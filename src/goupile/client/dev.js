// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let dev = new function() {
    let self = this;

    let allow_go = true;

    let assets;
    let assets_map;
    let current_asset;
    let current_url;

    let current_record = {};
    let app_form;

    let left_panel;
    let show_overview = true;

    let editor_el;
    let editor;
    let editor_buffers = new LruMap(32);
    let editor_timer_id;
    let editor_ignore_change = false;

    let reload_app = false;

    // Can be launched multiple times (e.g. when main.js is edited)
    this.init = async function() {
        let files = await vfs.listAll();
        let files_map = util.mapArray(files, file => file.path);

        // Detect file changes made outside editor
        for (let [path, buffer] of editor_buffers) {
            let file = files_map[path];
            buffer.reload = (file && file.sha256 !== buffer.sha256) || (!file && buffer.sha256);
        }

        try {
            let new_app = new Application;
            let app_builder = new ApplicationBuilder(new_app);

            let main_script = await loadFileData('/app/main.js');
            let func = Function('app', 'data', 'route', main_script);
            func(app_builder, new_app.data, new_app.route);

            app = new_app;
        } catch (err) {
            if (app) {
                throw err;
            } else {
                // Empty application, so that the user can still fix main.js or reset everything
                app = new Application;
                console.log(err);
            }
        }

        app.go = handleGo;
        app.makeURL = makeURL;
        util.deepFreeze(app, 'route');

        assets = listAssets(app, files);
        assets_map = util.mapArray(assets, asset => asset.url);

        // Select default page
        if (app.home) {
            assets_map[env.base_url] = assets_map[app.home];
        } else if (assets.length >= 2 && assets[1].type !== 'blob') {
            assets_map[env.base_url] = assets[1];
        } else {
            assets_map[env.base_url] = assets[0];
        }

        app.go(current_url || window.location.href, false);
    };

    function listAssets(app, files) {
        // Always add this one, which we need to edit even if is broken and
        // the application cannot be loaded.
        let assets = [{
            type: 'main',
            url: `${env.base_url}dev/`,
            label: 'Script principal',
            overview: 'Application',

            path: '/app/main.js',
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
                    overview: 'Formulaire',

                    form: form,
                    page: page,

                    path: `/app/pages/${page.key}.js`,
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
                overview: 'Agenda',

                schedule: schedule
            });
        }

        // Unused (by main.js) files
        try {
            let known_paths = new Set(assets.map(asset => asset.path));

            for (let file of files) {
                if (!known_paths.has(file.path)) {
                    assets.push({
                        type: 'blob',
                        url: `${env.base_url}dev${file.path}`,
                        category: 'Fichiers',
                        label: file.path,
                        overview: 'Contenu',

                        path: file.path
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

            // Update route application global
            for (let [key, value] of url.searchParams) {
                let num = Number(value);
                app.route[key] = Number.isNaN(num) ? value : num;
            }

            self.run(url.pathname).then(() => {
                if (push_history) {
                    window.history.pushState(null, null, makeURL());
                } else {
                    window.history.replaceState(null, null, makeURL());
                }
            });
        } else {
            self.run();
        }
    }

    function makeURL() {
        return util.pasteURL(current_url, app.route);
    }

    this.run = async function(url = null, args = {}) {
        // Find relevant asset
        if (url) {
            current_asset = assets_map[url];
            if (!current_asset && !url.endsWith('/'))
                current_asset = assets_map[url + '/'];
            current_url = current_asset ? current_asset.url : url;

            if (!current_asset)
                log.error(`URL inconnue '${url}'`);
        }

        // Load record (if needed)
        if (current_asset && current_asset.form) {
            if (!args.hasOwnProperty('id') && current_asset.form.key !== current_record.table)
                current_record = {};

            if (args.hasOwnProperty('id') || current_record.id == null) {
                if (args.id == null) {
                    current_record = recorder.create(current_asset.form.key);
                } else if (args.id !== current_record.id) {
                    current_record = await recorder.load(current_asset.form.key, args.id);
                    if (!current_record)
                        current_record = recorder.create(current_asset.form.key);

                    // The user asked for this record, make sure it is visible
                    if (!show_overview) {
                        if (goupile.isTablet())
                            left_panel = null;
                        show_overview = true;
                    }
                }

                app_form = new FormExecutor(current_asset.form, current_record);
            }
        }

        // Render menu and page layout
        renderDev();

        // Run left panel
        switch (left_panel) {
            case 'files': { await dev_files.runFiles(); } break;
            case 'editor': { syncEditor(); } break;
            case 'data': { await dev_data.runTable(current_asset.form.key, current_record.id); } break;
        }

        // Run appropriate module
        if (current_asset) {
            if (current_asset.category) {
                document.title = `${current_asset.category} :: ${current_asset.label} — ${env.app_name}`;
            } else {
                document.title = `${current_asset.label} — ${env.app_name}`;
            }

            await runAssetSafe();
        } else {
            document.title = env.app_name;
        }
    };

    function renderDev() {
        let show_editor = current_asset && current_asset.edit;
        let show_data = current_asset && current_asset.form;

        let correct_mode = (left_panel == null ||
                            left_panel === 'files' ||
                            (left_panel === 'editor' && show_editor) ||
                            (left_panel === 'data' && show_data));
        if (!correct_mode)
            left_panel = show_editor ? 'editor' : null;

        if (!current_asset || !current_asset.overview) {
            show_overview = false;
        } else if (!left_panel) {
            show_overview = true;
        }

        render(html`
            ${show_editor ?
                html`<button class=${left_panel === 'editor' ? 'active' : ''} @click=${e => toggleLeftPanel('editor')}>Éditeur</button>` : ''}
            ${show_data ?
                html`<button class=${left_panel === 'data' ? 'active' : ''} @click=${e => toggleLeftPanel('data')}>Données</button>` : ''}
            ${current_asset ?
                html`<button class=${show_overview ? 'active': ''} @click=${e => toggleOverview()}>${current_asset.overview}</button>` : ''}

            <select id="dev_assets" @change=${e => app.go(e.target.value)}>
                ${!current_asset ? html`<option>-- Sélectionnez une page --</option>` : ''}
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

            <button class=${left_panel === 'files' ? 'active' : ''} @click=${e => toggleLeftPanel('files')}>Ressources</button>
            <button @click=${showLoginDialog}>Connexion</button>
        `, document.querySelector('#gp_menu'));

        render(html`
            ${left_panel === 'files' ?
                html`<div id="dev_files" class=${show_overview ? 'dev_panel_left' : 'dev_panel_fixed'}></div>` : ''}
            ${left_panel === 'editor' ?
                makeEditorElement(show_overview ? 'dev_panel_left' : 'dev_panel_fixed') : ''}
            ${left_panel === 'data' ?
                html`<div id="dev_data" class=${show_overview ? 'dev_panel_left' : 'dev_panel_fixed'}></div>` : ''}
            <div id="dev_overview" class=${left_panel ? 'dev_panel_right' : 'dev_panel_page'}
                 style=${show_overview ? '' : 'display: none;'}></div>

            <div id="dev_log" style="display: none;"></div>
        `, document.querySelector('main'));
    }

    function showLoginDialog(e) {
        goupile.popup(e, page => {
            page.text('user', 'Nom d\'utilisateur');
            page.password('password', 'Mot de passe');

            page.submitHandler = () => {
                page.close();
                log.error('Non disponible pour le moment');
            };
            page.buttons(page.buttons.std.ok_cancel('Connexion'));
        });
    }

    function toggleLeftPanel(mode) {
        if (goupile.isTablet()) {
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
        if (goupile.isTablet()) {
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

    function makeEditorElement(cls) {
        if (!editor_el) {
            editor_el = document.createElement('div');
            editor_el.id = 'dev_editor';
            render(html`
                <div style="height: 100%;"></div>
                <div id="dev_toolbar"></div>
            `, editor_el);
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

            // Auto-pairing of parentheses is problematic when doing execute-as-you-type,
            // because it easily leads to infinite for loops.
            editor.setBehavioursEnabled(false);
        }

        let buffer = editor_buffers.get(current_asset.path);

        if (!buffer || buffer.reload) {
            let file = await vfs.load(current_asset.path);
            let script = file ? await file.data.text() : '';

            if (buffer) {
                buffer.sha256 = file ? file.sha256 : null;
                buffer.reload = false;

                editor_ignore_change = true;
                editor.session.doc.setValue(script);
                editor_ignore_change = false;
            } else {
                let session = new ace.EditSession(script, 'ace/mode/javascript');
                session.setOption('useWorker', false);
                session.setUseWrapMode(true);
                session.setUndoManager(buffer ? buffer.session.getUndoManager() : new ace.UndoManager());
                session.on('change', e => handleEditorChange(current_asset.path));

                buffer = {
                    session: session,
                    sha256: file ? file.sha256 : null,
                    reload: false
                };

                editor_buffers.set(current_asset.path, buffer);
            }
        }

        if (buffer.session !== editor.session) {
            editor.setSession(buffer.session);
            editor.setReadOnly(false);
        }

        editor.resize(false);
    }

    function handleEditorChange(path) {
        if (editor_ignore_change)
            return;

        clearTimeout(editor_timer_id);
        editor_timer_id = setTimeout(async () => {
            editor_timer_id = null;

            // The user may have changed document (async + timer)
            if (current_asset && current_asset.path === path) {
                if (path === '/app/main.js')
                    reload_app = true;

                if (await runAssetSafe()) {
                    let file = await vfs.save(path, editor.getValue());

                    let buffer = editor_buffers.get(path);
                    if (buffer)
                        buffer.sha256 = file.sha256;
                }
                window.history.replaceState(null, null, app.makeURL());
            }
        }, 60);
    }

    async function loadFileData(path) {
        let buffer = editor_buffers.get(path);

        if (buffer && !buffer.reload) {
            return buffer.session.getValue();
        } else {
            let file = await vfs.load(path);
            return file ? (await file.data.text()) : '';
        }
    }

    async function runAssetSafe() {
        let log_el = document.querySelector('#dev_log');
        let overview_el = document.querySelector('#dev_overview');

        try {
            switch (current_asset.type) {
                case 'main': {
                    if (reload_app) {
                        reload_app = false;

                        await self.init();
                        return true;
                    }

                    render(html`<div class="dev_wip">Aperçu non disponible pour le moment</div>`,
                           document.querySelector('#dev_overview'));
                } break;

                case 'page': {
                    // We don't want go() to be fired when a script is opened or changed in the editor,
                    // because then we wouldn't be able to come back to the script to fix the code.
                    allow_go = false;

                    let script = await loadFileData(current_asset.path);
                    let page_el = document.querySelector('#dev_overview') || document.createElement('div');

                    app_form.runPageScript(current_asset.page, script, page_el);
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

            // Make it easier for complex screw ups (which are mine, most of the time)
            console.log(err);

            return false;
        } finally {
            allow_go = true;
        }
    }
};
