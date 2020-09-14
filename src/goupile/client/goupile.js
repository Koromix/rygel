// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// These globals are initialized below
let app = null;
let nav = null;
let vfs = null;
let vrec = null;
let user = null;

let goupile = new function() {
    let self = this;

    let tablet_mq = window.matchMedia('(pointer: coarse)');
    let standalone_mq = window.matchMedia('(display-mode: standalone)');

    let route_asset;

    let running = 0;
    let restart = false;

    let ping_timer;
    let last_sync = 0;

    let left_panel = null;
    let show_overview = true;
    let overview_wanted = false;

    let editor_el;
    let style_el;
    let template_el;

    this.startApp = async function() {
        try {
            log.pushHandler(log.notifyHandler);

            initNavigation();

            let db = await openDatabase();
            vfs = new VirtualFS(db);
            vrec = new VirtualRecords(db);
            user = new UserManager(db);

            if (navigator.serviceWorker) {
                navigator.serviceWorker.register(`${env.base_url}sw.pk.js`);

                if (env.use_offline) {
                    enablePersistence();
                    await updateApplication();
                }
            }

            nav = new ApplicationNavigator();
            await self.initMain();

            net.changeHandler = online => {
                updateStatus();

                if (env.use_offline)
                    self.go();
            };

            ping_timer = setTimeout(checkEvents, 0);
        } finally {
            document.querySelector('#gp_root').classList.remove('busy');
        }
    };

    function enablePersistence() {
        let storage_warning = 'Impossible d\'activer le stockage local persistant';

        if (navigator.storage && navigator.storage.persist) {
            navigator.storage.persist().then(granted => {
                // NOTE: For some reason this does not seem to work correctly on my Firefox profile,
                // where granted is always true. Might come from an extension or some obscure privacy
                // setting. Investigate.
                if (granted) {
                    console.log('Stockage persistant activé');
                } else {
                    console.log(storage_warning);
                }
            });
        } else {
            console.log(storage_warning);
        }
    }

    async function updateApplication() {
        try {
            let files = await vfs.status();

            if (files.some(file => file.action === 'pull' || file.action === 'conflict')) {
                if (files.every(file => file.action === 'pull' || file.action === 'noop')) {
                    await vfs.sync(files);
                } else {
                    log.info('Mise à jour non appliquée : il existe des modifications locales')
                }
            }
        } catch (err) {
            console.log('Mise à jour abandonnée (pas de réseau ?)');
        }
    }

    async function openDatabase() {
        let db_name = `goupile+${env.app_key}`;
        let db = await idb.open(db_name, 6, (db, old_version) => {
            switch (old_version) {
                // See sw.js for why we need to use version 2 at a minimum.
                // TLDR: IndexedDB sucks.
                case null:
                case 1: {
                    db.createStore('fs_entries', {keyPath: 'path'});
                    db.createStore('fs_data');
                    db.createStore('fs_sync', {keyPath: 'path'});

                    db.createStore('rec_entries', {keyPath: '_ikey'});
                    db.createStore('rec_fragments', {keyPath: '_ikey'});
                    db.createStore('rec_variables', {keyPath: '_ikey'});
                } // fallthrough

                case 2: {
                    db.deleteStore('rec_variables');
                    db.createStore('rec_columns', {keyPath: 'key'});
                } // fallthrough

                case 3: {
                    db.createStore('usr_offline', {keyPath: 'username'});
                    db.createStore('usr_profiles', {keyPath: 'username'});
                } // fallthrough

                case 4: {
                    db.deleteStore('usr_offline');
                    db.deleteStore('usr_profiles');
                    db.createStore('usr_passports', {keyPath: 'username'});
                } // fallthrough

                case 5: {
                    db.createIndex('rec_fragments', 'anchor', 'anchor', {unique: false});
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
    this.initMain = async function(code = undefined) {
        if (!user.isSynced())
            await user.fetchProfile();

        if (self.isConnected() || env.allow_guests) {
            try {
                running++;

                let new_app = new ApplicationInfo;
                let app_builder = new ApplicationBuilder(new_app);

                if (code == null)
                    code = await readCode('/files/main.js');

                let func = Function('util', 'app', 'shared', 'go', 'route', code);
                func(util, app_builder, new_app.shared, nav.go, nav.route);

                app = new_app;
            } catch (err) {
                if (app) {
                    throw err;
                } else {
                    // Empty application, so that the user can still fix main.js or reset everything
                    app = new ApplicationInfo;
                    console.log(err);
                }
            } finally {
                running--;
            }

            // Select default page
            app.urls_map[env.base_url] = app.home ? app.urls_map[app.home] : app.assets[0];

            // Load custom template and CSS (if any)
            let html = await readCode('/files/main.html') || '';
            let css = await readCode('/files/main.css') || '';
            changeTemplate(html);
            changeCSS(css);

            util.deepFreeze(app);
        } else {
            app = null;
        }

        await self.go(window.location.href, false);
    };

    function changeCSS(css) {
        if (!style_el) {
            style_el = document.createElement('style');
            document.head.appendChild(style_el);
        }

        style_el.textContent = css;
    }

    function changeTemplate(html) {
        html = html.trim();

        if (html) {
            template_el = document.createElement('template');
            template_el.innerHTML = html;
        } else {
            template_el = null;
        }
    }

    this.isConnected = function() { return user.isConnected(); };
    this.isTablet = function() { return tablet_mq.matches; };
    this.isStandalone = function() { return standalone_mq.matches; };
    this.isLocked = function() { return !!user.getLockURL(); };
    this.isRunning = function() { return !!running; }

    this.go = async function(url = undefined, push_history = true) {
        if (running) {
            restart = true;
            return;
        }

        try {
            running++;
            url = user.getLockURL() || url;

            if (self.isConnected() || env.allow_guests) {
                if (url) {
                    url = new URL(url, window.location.href);

                    // Update route application global
                    for (let [key, value] of url.searchParams) {
                        let num = Number(value);
                        nav.route[key] = Number.isNaN(num) ? value : num;
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

                    // Update history
                    if (push_history)
                        window.history.pushState(null, null, url.pathname);

                    // Route URL through appropriate controller
                    if (route_asset) {
                        try {
                            switch (route_asset.type) {
                                case 'page': { await form_exec.route(route_asset.page, url); } break;
                            }
                        } catch (err) {
                            log.error(err);
                            route_asset = null;
                        }
                    } else {
                        log.error(`URL non supportée '${url.pathname}'`);
                    }
                }

                // Restart application after session changes
                if (!user.isSynced()) {
                    self.initMain();
                    return;
                }

                // Ensure valid menu and panel configuration
                if (!user.getLockURL()) {
                    let show_develop = user.hasPermission('develop');
                    let show_data = user.hasPermission('edit') && route_asset && route_asset.form;

                    let correct_mode = (left_panel == null ||
                                        (left_panel === 'files' && show_develop) ||
                                        (left_panel === 'editor' && show_develop) ||
                                        (left_panel === 'status' && show_data) ||
                                        (left_panel === 'data' && show_data) ||
                                        (left_panel === 'describe' && show_data));
                    if (!correct_mode)
                        left_panel = show_develop ? 'editor' : null;

                    if (!route_asset || !route_asset.overview) {
                        if (!overview_wanted)
                            overview_wanted = show_overview && !self.isTablet();
                        show_overview = false;
                    } else if (!left_panel || overview_wanted) {
                        show_overview = true;
                    }

                    if (show_overview && self.isTablet())
                        left_panel = null;
                } else {
                    left_panel = null;
                    show_overview = true;
                }

                // Render menu and page layout
                renderAll();

                try {
                    // Run appropriate module
                    if (route_asset) {
                        document.title = `${route_asset.label} — ${env.app_name}`;
                        await runAssetSafe(route_asset);
                    } else {
                        document.title = env.app_name;
                    }

                    // Run accessory panel
                    switch (left_panel) {
                        case 'files': { await dev_files.runFiles(); } break;
                        case 'editor': {
                            let path = route_asset && route_asset.path ? route_asset.path : null;
                            await dev_files.runEditor(path);
                        } break;
                        case 'status': { await form_exec.runStatus(); } break;
                        case 'data': { await form_exec.runData(); } break;
                    }
                } catch (err) {
                    log.error(err);
                }

                // Give dialog windows and popups (if any) a chance to refresh too
                dialog.refreshAll();
            } else {
                user.runLoginScreen();
            }
        } finally {
            running--;

            if (restart) {
                restart = false;
                setTimeout(self.go, 0);
            } else {
                for (let panel of document.querySelectorAll('.gp_panel')) {
                    panel.classList.remove('busy');
                    if (!panel.children.length)
                        panel.classList.add('broken');
                }
            }
        }
    };

    async function checkEvents() {
        try {
            let response = await net.fetch(`${env.base_url}api/events`);

            if (response.ok) {
                net.setOnline(true);

                // XXX: Ugly, and syncing does way too much work for nothing
                let now = Date.now();
                if (env.sync_mode === 'mirror' && user.isConnected() && now - last_sync >= 180000) {
                    await form_exec.syncRecords();
                    last_sync = now;
                }
            } else {
                net.setOnline(false);
            }
        } catch (err) {
            net.setOnline(false);
        } finally {
            if (ping_timer != null)
                clearTimeout(ping_timer);
            ping_timer = setTimeout(checkEvents, 30000);

            if (!user.isSynced())
                self.go();
        }
    }

    function renderAll() {
        let in_iframe;
        try {
            in_iframe = (document.location.hostname !== window.parent.location.hostname);
        } catch (e) {
            in_iframe = true;
        }

        render(html`
            ${!in_iframe && !user.getLockURL() ?
                html`<nav id="gp_menu" class="gp_toolbar">${renderFullMenu()}</nav>`: ''}
            ${!in_iframe && user.getLockURL() ? html`<nav id="gp_menu" class="gp_toolbar locked">
                &nbsp;&nbsp;Application verrouillée
                <div style="flex: 1;"></div>
                <button type="button" class="icon" style="background-position-y: calc(-450px + 1.2em)"
                        @click=${user.runLoginDialog}>Connexion</button>
                <button type="button" class="icon" style="background-position-y: calc(-186px + 1.2em)"
                        @click=${user.runUnlockDialog}>Déverrouiller</button>
            </nav>`: ''}

            <main>
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
            </main>
        `, document.querySelector('#gp_root'));
        updateStatus();

        for (let panel of document.querySelectorAll('.gp_panel:empty')) {
            panel.classList.remove('broken');
            panel.classList.add('busy');
        }
    }

    function renderFullMenu() {
        let show_develop = user.hasPermission('develop');
        let show_data = user.hasPermission('edit') && route_asset && route_asset.form;

        return html`
            ${show_develop ? html`
                <div class="gp_dropdown">
                    <button class=${left_panel === 'editor' || left_panel === 'files' ? 'icon active' : 'icon'}
                            style="background-position-y: calc(-228px + 1.2em)">Code</button>
                    <div>
                        <button class=${left_panel === 'editor' ? 'active' : ''}
                                @click=${e => toggleLeftPanel('editor')}>Éditeur</button>
                        <button class=${left_panel === 'files' ? 'active' : ''}
                                @click=${e => toggleLeftPanel('files')}>Déploiement</button>
                    </div>
                </div>
            ` : ''}
            ${show_data ? html`
                <div class="gp_dropdown">
                    <button class=${left_panel === 'status' || left_panel === 'data' ? 'icon active' : 'icon'}
                            style="background-position-y: calc(-274px + 1.2em)">Recueil</button>
                    <div>
                        <button class=${left_panel === 'status' ? 'active' : ''}
                                @click=${e => toggleLeftPanel('status')}>Suivi</button>
                        <button class=${left_panel === 'data' ? 'active' : ''}
                                @click=${e => toggleLeftPanel('data')}>Données</button>
                    </div>
                </div>
            ` :  ''}
            ${route_asset ? html`
                <button class=${show_overview ? 'icon active': 'icon'}
                        @click=${e => self.toggleOverview()}
                        style="background-position-y: calc(-318px + 1.2em)">${route_asset.overview}</button>
            ` : ''}

            ${show_develop ? html`
                <select id="gp_assets" @change=${e => self.go(e.target.value)}>
                    ${!route_asset ? html`<option>-- Sélectionnez une page --</option>` : ''}
                    ${util.mapRLE(app.assets, asset => asset.category, (category, offset, len) => {
                        if (category == null) {
                            return '';
                        } else {
                            return html`<optgroup label=${category}>${util.mapRange(offset, offset + len, idx => {
                                let asset = app.assets[idx];
                                return html`<option value=${asset.url}
                                                    .selected=${asset === route_asset}>${asset.label}</option>`;
                            })}</optgroup>`;
                        }
                    })}
                </select>
            ` : ''}
            ${!show_develop ? html`<div style="flex: 1;"></div>` : ''}

            ${!self.isConnected() ?
                html`<button class="icon" style="background-position-y: calc(-450px + 1.2em)"
                             @click=${user.runLoginDialog}>Connexion</button>` : ''}
            ${self.isConnected() ? html`
                <div class="gp_dropdown right">
                    <button class="icon" style="background-position-y: calc(-494px + 1.2em)">${user.getUserName()}</button>
                    <div>
                        <button @click=${user.runLoginDialog}>Changer d'utilisateur</button>
                        <button @click=${user.logout}>Déconnexion</button>
                    </div>
                </div>
            ` : ''}

            <div id="gp_status"/>
        `;
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

    this.runConnected = async function(func) {
        if (!net.isOnline()) {
            log.error('Cette action nécessite une connexion au serveur');
            return;
        }

        // If user is really connected (with a server session key)
        if (user.isConnectedOnline()) {
            try {
                await func();
                return;
            } catch (err) {
                if (user.isSynced())
                    throw err;
            }

            await self.initMain();

            if (user.isConnectedOnline()) {
                await func();
                return;
            }
        }

        // Ask user to connect
        await user.runLoginDialog();
        await func();
    };

    this.toggleOverview = function(enable = undefined) {
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
        overview_wanted = false;

        self.go();
    };

    this.validateCode = async function(path, code) {
        if (path === '/files/main.js') {
            try {
                await self.initMain(code);

                showScriptError(null);
                return true;
            } catch (err) {
                showScriptError(err);
                return false;
            }
        } else if (path === '/files/main.css') {
            changeCSS(code);
            return true;
        } else if (path === '/files/main.html') {
            changeTemplate(code);
            if (route_asset)
                await runAssetSafe(route_asset);
            return true;
        } else {
            if (route_asset && route_asset.path === path) {
                return await runAssetSafe(route_asset, code);
            } else {
                let asset = app.paths_map[path];
                return asset ? await runAssetSafe(asset, code) : true;
            }
        }
    };

    async function runAssetSafe(asset, code = undefined) {
        let error_el = document.querySelector('#gp_error');
        let overview_el = document.querySelector('#gp_overview');

        let test_el;
        let broken_template = false;
        if (asset === route_asset) {
            if (template_el) {
                render(template_el.content, overview_el);
                test_el = document.getElementById('@page');

                if (!test_el) {
                    test_el = overview_el;
                    broken_template = true;
                }
            } else {
                test_el = overview_el;
            }
        } else {
            test_el = document.createElement('div');
        }

        try {
            if (asset.path && code == null)
                code = await readCode(asset.path);
        } catch (err) {
            overview_el.classList.add('broken');
            throw err;
        }

        try {
            running++;

            switch (asset.type) {
                case 'page': { form_exec.runPage(code, test_el); } break;
                case 'schedule': { await sched_exec.runMeetings(asset.schedule, test_el); } break;
            }

            if (broken_template)
                throw new Error(`Le modèle ne contient pas d'élément avec l'ID '@page'`);

            showScriptError(null);
            return true;
        } catch (err) {
            showScriptError(err);
            return false;
        } finally {
            running--;
        }
    }

    function showScriptError(err) {
        let error_el = document.querySelector('#gp_error');
        let overview_el = document.querySelector('#gp_overview');

        if (err) {
            let err_line = util.parseEvalErrorLine(err);

            // XXX: If the user changes page quickly before changes are tested, we can
            // end up showing an error about the previous script on the new page.
            if (err_line) {
                error_el.textContent = `⚠\uFE0E Line ${err_line}: ${err.message}`;
            } else {
                error_el.textContent = `⚠\uFE0E ${err.message}`;
            }
            error_el.style.display = 'block';
            overview_el.classList.add('broken');

            // Make it easier for complex screw ups (which are mine, most of the time)
            console.log(err);
        } else {
            // Things are OK!
            error_el.innerHTML = '';
            error_el.style.display = 'none';
            overview_el.classList.remove('broken');
        }
    }

    async function readCode(path) {
        let code = dev_files.getBuffer(path);

        if (code != null) {
            return code;
        } else {
            let file = await vfs.load(path);
            return file ? await file.data.text() : null;
        }
    }

    function updateStatus() {
        let el = document.querySelector('#gp_status');

        if (el) {
            if (net.isOnline()) {
                el.style.backgroundPositionY = 'calc(-54px + 1.2em)';
                el.title = 'Serveur disponible';
            } else {
                el.style.backgroundPositionY = 'calc(-98px + 1.2em)';
                el.title = 'Serveur non disponible';
            }
        }
    }
};
