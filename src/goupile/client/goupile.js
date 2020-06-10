// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// These globals are initialized below
let app = null;
let virt_fs = null;
let virt_data = null;

let goupile = new function() {
    let self = this;

    let tablet_mq = window.matchMedia('(pointer: coarse)');
    let standalone_mq = window.matchMedia('(display-mode: standalone)');

    let settings;
    let settings_rnd;

    let route_asset;
    let route_url;
    let block_go = false;

    let running = false;
    let restart = false;

    let left_panel = null;
    let show_overview = true;

    let editor_el;
    let style_el;

    document.addEventListener('readystatechange', e => {
        if (document.readyState === 'complete')
            initGoupile();
    });

    async function initGoupile() {
        log.pushHandler(log.notifyHandler);

        initNavigation();

        let db = await openDatabase();
        virt_fs = new VirtualFS(db);
        virt_data = new VirtualData(db);

        if (navigator.serviceWorker) {
            navigator.serviceWorker.register(`${env.base_url}sw.pk.js`);

            if (env.use_offline) {
                enablePersistence();
                await updateApplication();
            }
        }

        await self.initApplication();

        // If a run fails and we can run in offline mode, restart it transparently
        net.changeHandler = online => {
            if (env.use_offline)
                self.go();
        };
    }

    function enablePersistence() {
        let storage_warning = 'Impossible d\'activer le stockage local persistant';

        if (navigator.storage && navigator.storage.persist) {
            navigator.storage.persist().then(granted => {
                // NOTE: For some reason this does not seem to work correctly on my Firefox profile,
                // where granted is always true. Might come from an extension or some obscure privacy
                // setting. Investigate.
                if (granted) {
                    console.log('Persistent storage has been granted');
                } else {
                    log.error(storage_warning);
                }
            });
        } else {
            log.error(storage_warning);
        }
    }

    async function updateApplication() {
        try {
            let files = await virt_fs.status();

            if (files.some(file => file.action === 'pull' || file.action === 'conflict')) {
                if (files.every(file => file.action === 'pull' || file.action === 'noop')) {
                    await virt_fs.sync(files);
                } else {
                    log.info('Mise à jour non appliquée : il existe des modifications locales')
                }
            }
        } catch (err) {
            console.log('Mise à jour abandonnée (pas de réseau ?)');
        }
    }

    async function openDatabase() {
        let db_name = `goupile@${env.app_key}`;
        let db = await idb.open(db_name, 3, (db, old_version) => {
            switch (old_version) {
                case null: {
                    db.createStore('files', {keyPath: 'path'});
                    db.createStore('files_data');
                    db.createStore('files_cache', {keyPath: 'path'});

                    db.createStore('records', {keyPath: 'tkey'});
                    db.createStore('records_data', {keyPath: 'tkey'});
                    db.createStore('records_sequences');
                    db.createStore('records_variables', {keyPath: 'tkey'});
                } // fallthrough

                case 1: {
                    db.createStore('records_queue', {keyPath: 'tpkey'});
                } // fallthrough

                case 2: {
                    db.deleteStore('records_sequences');
                } // fallthrough
            }
        });

        return db;
    }

    function initNavigation() {
        window.addEventListener('popstate', e => self.go(window.location.href, false));

        util.interceptLocalAnchors((e, href) => {
            self.go(href);
            e.preventDefault();
        });
    }

    // Can be launched multiple times (e.g. when main.js is edited)
    this.initApplication = async function(code = null) {
        await fetchSettings();

        if (self.isConnected() || env.allow_guests) {
            let files = await virt_fs.listAll(net.isOnline() || !env.use_offline);
            let files_map = util.mapArray(files, file => file.path);

            try {
                let new_app = new Application;
                let app_builder = new ApplicationBuilder(new_app);
                new_app.go = handleGo;

                if (code == null)
                    code = await readCode('/files/main.js');

                let func = Function('util', 'app', 'data', 'nav', 'go', 'route', code);
                let nav = {go: handleGo};
                func(util, app_builder, new_app.data, nav, nav.go, new_app.route);

                let known_paths = new Set(new_app.assets.map(asset => asset.path));
                known_paths.add('/files/main.js');
                known_paths.add('/files/main.css');

                // Make unused files available
                for (let file of files) {
                    if (!known_paths.has(file.path))
                        app_builder.file(file);
                }

                app = new_app;
            } catch (err) {
                if (app) {
                    throw err;
                } else {
                    // Empty application, so that the user can still fix main.js or reset everything
                    app = new Application;
                    app.go = handleGo;

                    console.log(err);
                }
            }

            // XXX: Hack for secondary asset thingy that we'll get rid of eventually
            for (let i = 0; i < app.assets.length; i++)
                app.assets[i].idx = i;

            // Select default page
            if (app.home) {
                app.urls_map[env.base_url] = app.urls_map[app.home];
            } else {
                app.urls_map[env.base_url] =
                    app.assets.find(asset => asset.type !== 'main' && asset.type !== 'blob') || app.assets[0];
            }

            // Update custom CSS (if any)
            {
                let css = await readCode('/files/main.css');
                changeCSS(css);
            }

            util.deepFreeze(app, 'route');
        } else {
            app = null;
        }

        self.go(route_url || window.location.href, false);
    };

    // Avoid async here, because it may fail (see block_go) and the caller
    // may need to catch that synchronously.
    function handleGo(url = null, push_history = true) {
        if (block_go) {
            throw new Error(`A navigation function (e.g. go()) has been interrupted.
Navigation functions should only be called in reaction to user events, such as button clicks.`);
        }

        if (!url.match(/^((http|ftp|https):\/\/|\/)/g))
            url = `${env.base_url}app/${url}`;

        self.go(url, push_history);
    }

    async function fetchSettings() {
        let session_rnd = util.getCookie('session_rnd');

        if (net.isPlugged() && session_rnd !== settings_rnd) {
            settings = {};

            if (session_rnd != null) {
                try {
                    let response = await net.fetch(`${env.base_url}api/settings.json?rnd=${session_rnd}`);

                    if (response.ok) {
                        settings = await response.json();
                    } else {
                        // The request has failed and could have deleted the session_rnd cookie
                        session_rnd = util.getCookie('session_rnd');
                    }

                    settings_rnd = session_rnd;
                    return true;
                } catch (err) {
                    return false;
                }
            }
        } else {
            return false;
        }
    }

    function changeCSS(css) {
        if (!style_el) {
            style_el = document.createElement('style');
            document.head.appendChild(style_el);
        }

        style_el.textContent = css;
    }

    this.isConnected = function() { return !!settings_rnd; };
    this.isTablet = function() { return tablet_mq.matches; };
    this.isStandalone = function() { return standalone_mq.matches; };

    this.go = async function(url = null, push_history = true) {
        if (running) {
            restart = true;
            return;
        }

        try {
            running = true;

            if (self.isConnected() || env.allow_guests) {
                if (url) {
                    url = new URL(url, window.location.href);

                    // Update route application global
                    for (let [key, value] of url.searchParams) {
                        let num = Number(value);
                        app.route[key] = Number.isNaN(num) ? value : num;
                    }

                    // Find relevant asset
                    {
                        let path = url.pathname;
                        if (!path.endsWith('/'))
                            path += '/';

                        route_asset = app.urls_map[path] || app.aliases_map[path];

                        if (!route_asset) {
                            do {
                                path = path.substr(0, path.length - 1);
                                path = path.substr(0, path.lastIndexOf('/') + 1);

                                if (path === env.base_url)
                                    break;

                                route_asset = app.urls_map[path];
                            } while (!route_asset && path.length);
                        }
                    }

                    // Update URL and history
                    route_url = url.pathname;
                    if (push_history)
                        window.history.pushState(null, null, route_url);

                    // Route URL through appropriate controller
                    if (route_asset) {
                        try {
                            switch (route_asset.type) {
                                case 'page': { await form_executor.route(route_asset, url); } break;
                            }
                        } catch (err) {
                            log.error(err.message);
                        }
                    } else {
                        log.error(`URL non supportée '${url.pathname}'`);
                    }
                }

                // Restart application after session changes
                if (await fetchSettings()) {
                    await self.initApplication();
                    return;
                }

                // Render menu and page layout
                renderPanels();

                try {
                    // Run left panel
                    switch (left_panel) {
                        case 'files': { await dev_files.runFiles(); } break;
                        case 'editor': { await dev_files.runEditor(route_asset); } break;
                        case 'status': { await form_executor.runStatus(); } break;
                        case 'data': { await form_executor.runData(); } break;
                        case 'describe': { await form_executor.runDescribe(); } break;
                    }

                    // Run appropriate module
                    if (route_asset) {
                        document.title = `${route_asset.label} — ${env.app_name}`;
                        await runAssetSafe(route_asset);
                    } else {
                        document.title = env.app_name;
                    }
                } finally {
                    updateStatus();
                }
            } else {
                renderGuest();
            }
        } finally {
            running = false;

            if (restart) {
                restart = false;
                setTimeout(self.go, 0);
            }
        }
    };

    function renderPanels() {
        let show_data = route_asset && route_asset.form;

        let correct_mode = (left_panel == null ||
                            left_panel === 'files' || left_panel === 'editor' ||
                            (left_panel === 'status' && show_data) ||
                            (left_panel === 'data' && show_data) ||
                            (left_panel === 'describe' && show_data));
        if (!correct_mode)
            left_panel = 'editor';

        if (!route_asset || !route_asset.overview) {
            show_overview = false;
        } else if (!left_panel) {
            show_overview = true;
        }

        let show_assets = [];
        let select_asset;
        if (route_asset) {
            let idx = route_asset.idx;
            while (app.assets[idx].secondary)
                idx--;

            // Main asset
            show_assets.push(app.assets[idx]);
            select_asset = app.assets[idx];

            // Related secondary assets
            while (++idx < app.assets.length && app.assets[idx].secondary)
                show_assets.push(app.assets[idx]);
        }

        render(html`
            <div class="gp_dropdown">
                <button class=${left_panel === 'editor' || left_panel === 'files' ? 'active' : ''}>Code</button>
                <div>
                    <button class=${left_panel === 'editor' ? 'active' : ''}
                            @click=${e => toggleLeftPanel('editor')}>Éditeur</button>
                    <button class=${left_panel === 'files' ? 'active' : ''}
                            @click=${e => toggleLeftPanel('files')}>Déploiement</button>
                </div>
            </div>

            ${show_data ? html`
                <div class="gp_dropdown">
                    <button class=${left_panel === 'status' || left_panel === 'data' ? 'active' : ''}>Recueil</button>
                    <div>
                        <button class=${left_panel === 'status' ? 'active' : ''}
                                @click=${e => toggleLeftPanel('status')}>Suivi</button>
                        <button class=${left_panel === 'data' ? 'active' : ''}
                                @click=${e => toggleLeftPanel('data')}>Données</button>
                    </div>
                </div>
            ` :  ''}

            ${show_assets.map(asset => {
                if (asset === route_asset) {
                    return html`<button class=${show_overview ? 'active': ''}
                                        @click=${e => self.toggleOverview()}>${asset.overview}</button>`;
                } else {
                    return html`<button @click=${e => self.go(asset.url)}>${asset.overview}</button>`;
                }
            })}

            <select id="gp_assets" @change=${e => self.go(e.target.value)}>
                ${!route_asset ? html`<option>-- Sélectionnez une page --</option>` : ''}
                ${util.mapRLE(app.assets, asset => asset.category, (category, offset, len) => {
                    if (category == null) {
                        return '';
                    } else if (len === 1) {
                        let asset = app.assets[offset];
                        return html`<option value=${asset.url}
                                            .selected=${asset === select_asset}>${asset.category} (${asset.label})</option>`;
                    } else {
                        return html`<optgroup label=${category}>${util.mapRange(offset, offset + len, idx => {
                            let asset = app.assets[idx];
                            if (!asset.secondary) {
                                return html`<option value=${asset.url}
                                                   .selected=${asset === select_asset}>${asset.label}</option>`;
                            } else {
                                return '';
                            }
                        })}</optgroup>`;
                    }
                })}
            </select>

            ${env.use_offline ? html`<button type="button" id="gp_status" @click=${e => net.setPlugged(!net.isPlugged())} />` : ''}
            ${!env.use_offline ? html`<div id="gp_status"/>` : ''}

            ${!self.isConnected() ? html`<button @click=${showLoginDialog}>Connexion</button>` : ''}
            ${self.isConnected() ? html`
                <div class="gp_dropdown right">
                    <button>${settings.username}</button>
                    <div>
                        <button @click=${showLoginDialog}>Changer d'utilisateur</button>
                        <button @click=${logout}>Déconnexion</button>
                    </div>
                </div>
            ` : ''}
        `, document.querySelector('#gp_menu'));

        render(html`
            ${left_panel === 'files' ?
                html`<div id="dev_files" class=${show_overview ? 'gp_panel left' : 'gp_panel fixed'}></div>` : ''}
            ${left_panel === 'editor' ?
                html`<div id="dev_editor" class=${show_overview ? 'gp_panel left' : 'gp_panel fixed'}></div>` : ''}
            ${left_panel === 'status' ?
                html`<div id="dev_status" class=${show_overview ? 'gp_panel left' : 'gp_panel fixed'}></div>` : ''}
            ${left_panel === 'data' ?
                html`<div id="dev_data" class=${show_overview ? 'gp_panel left' : 'gp_panel fixed'}></div>` : ''}
            ${left_panel === 'describe' ?
                html`<div id="dev_describe" class=${show_overview ? 'gp_panel left' : 'gp_panel fixed'}></div>` : ''}
            <div id="gp_overview" class=${left_panel ? 'gp_panel right' : 'gp_panel overview'}
                 style=${show_overview ? '' : 'display: none;'}></div>

            <div id="gp_error" style="display: none;"></div>
        `, document.querySelector('main'));
    }

    function showLoginDialog(e) {
        ui.popup(e, makeLoginForm);
    }

    function makeLoginForm(page) {
        let username = page.text('username', 'Nom d\'utilisateur');
        let password = page.password('password', 'Mot de passe');

        page.submitHandler = async () => {
            let entry = new log.Entry;

            entry.progress('Connexion en cours');
            try {
                let body = new URLSearchParams({
                    username: username.value,
                    password: password.value}
                );

                let response = await net.fetch(`${env.base_url}api/login.json`, {method: 'POST', body: body});

                if (response.ok) {
                    if (page.close)
                        page.close();

                    entry.success('Connexion réussie');
                    await self.initApplication();
                } else {
                    let msg = await response.text();
                    entry.error(msg);
                }
            } catch (err) {
                entry.error(err.message);
            }
        };
        page.buttons(page.buttons.std.ok_cancel('Connexion'));
    }

    async function logout() {
        let entry = new log.Entry;

        entry.progress('Déconnexion en cours');
        try {
            let response = await net.fetch(`${env.base_url}api/logout.json`, {method: 'POST'});

            if (response.ok) {
                entry.success('Déconnexion réussie');
                await self.initApplication();
            } else {
                let msg = await response.text();
                entry.error(msg);
            }
        } catch (err) {
            entry.error(err.message);
        }
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

        self.go();
    }

    this.toggleOverview = function(enable = null) {
        if (enable == null)
            enable = !show_overview || goupile.isTablet();

        if (enable) {
            if (goupile.isTablet())
                left_panel = null;
            show_overview = true;
        } else {
            left_panel = left_panel || 'editor';
            show_overview = false;
        }

        self.go();
    };

    function toggleAssetView(asset) {
        if (goupile.isTablet() || asset !== route_asset) {
            left_panel = null;
            show_overview = true;
        } else if (!show_overview) {
            show_overview = true;
        } else {
            left_panel = left_panel || 'editor';
            show_overview = false;
        }

        self.go(asset.url);
    }

    this.validateCode = function(path, code) {
        if (path === '/files/main.js') {
            return runAssetSafe(app.assets[0], code);
        } else if (path === '/files/main.css') {
            changeCSS(code);
            return true;
        } else {
            let asset = app.paths_map[path];
            return asset ? runAssetSafe(asset, code) : true;
        }
    };

    async function runAssetSafe(asset, code = null) {
        let error_el = document.querySelector('#gp_error');
        let overview_el = document.querySelector('#gp_overview');
        let test_el = (asset === route_asset) ? overview_el : document.createElement('div');

        // We don't want go() to be fired when a script is opened or changed in the editor,
        // because then we wouldn't be able to come back to the script to fix the code.
        block_go = true;

        try {
            switch (asset.type) {
                case 'main': {
                    if (code != null) {
                        await self.initApplication();
                        return true;
                    }

                    render(html`<div class="gp_wip">Tableau de bord non disponible</div>`, test_el);
                } break;

                case 'page': {
                    if (code == null)
                        code = await readCode(asset.path);

                    form_executor.runPage(code, test_el);
                } break;

                case 'schedule': { await sched_executor.runMeetings(asset.schedule, test_el); } break;
                case 'schedule_settings': { await sched_executor.runSettings(asset.schedule, test_el); } break;

                default: {
                    render(html`<div class="gp_wip">Aperçu non disponible</div>`, test_el);
                } break;
            }

            // Things are OK!
            error_el.innerHTML = '';
            error_el.style.display = 'none';
            overview_el.classList.remove('broken');

            return true;
        } catch (err) {
            let err_line = util.parseEvalErrorLine(err);

            // XXX: If the user changes page quickly before changes are tested, we can
            // end up showing an error about the previous script on the new page.
            error_el.textContent = `⚠\uFE0E Line ${err_line || '?'}: ${err.message}`;
            error_el.style.display = 'block';
            overview_el.classList.add('broken');

            // Make it easier for complex screw ups (which are mine, most of the time)
            console.log(err);

            return false;
        } finally {
            block_go = false;
        }
    }

    async function readCode(path) {
        let code = dev_files.getBuffer(path);

        if (code != null) {
            return code;
        } else {
            let file = await virt_fs.load(path);
            return file ? await file.data.text() : null;
        }
    }

    function updateStatus() {
        let el = document.querySelector('#gp_status');

        if (!net.isPlugged()) {
            el.className = 'unplugged';
        } else if (net.isOnline()) {
            el.className = 'online';
        } else {
            el.className = 'offline';
        }
    }

    function renderGuest() {
        render('', document.querySelector('#gp_menu'));

        let state = new PageState;
        let update = () => {
            let page = new Page('@login');

            let builder = new PageBuilder(state, page);
            builder.changeHandler = update;
            builder.pushOptions({
                missingMode: 'disable',
                wide: true
            });

            makeLoginForm(builder);

            render(html`
                <form id="gp_login" @submit=${e => e.preventDefault()}>
                    ${page.render()}
                </form>
            `, document.querySelector('main'));
        };

        update();
    }
};
