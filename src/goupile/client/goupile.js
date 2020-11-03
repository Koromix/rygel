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

    let db;

    let tablet_mq = window.matchMedia('(pointer: coarse)');
    let standalone_mq = window.matchMedia('(display-mode: standalone)');

    let route_page;

    let running = 0;

    let ping_timer;
    let sync_time = 0;
    let sync_rnd;

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

            db = await openDatabase();
            vfs = new VirtualFS(db);
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
        let db = await idb.open(db_name, 7, (db, old_version) => {
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

                case 6: {
                    db.createStore('rec_sync');
                } // fallthrough
            }
        });

        return db;
    }

    function initNavigation() {
        window.addEventListener('popstate', e => self.go(window.location.href, false));

        window.onbeforeunload = e => {
            if (form_exec.hasChanges())
                return 'Si vous confirmez vouloir quitter la page, les modifications en cours seront perdues !';
        };

        util.interceptLocalAnchors((e, href) => {
            self.go(href);
            e.preventDefault();
        });
    }

    // Can be launched multiple times (e.g. when main.js is edited)
    this.initMain = async function(code = undefined) {
        if (!user.isSynced())
            await user.fetchProfile();

        vrec = new VirtualRecords(db, user.getZone());

        if (self.isConnected()) {
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

        if (net.isOnline())
            await triggerSync();
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
    this.isLocked = function() { return !!user.isLocked(); };
    this.isRunning = function() { return !!running; }

    this.go = async function(url = undefined, push_history = true) {
        try {
            running++;

            if (!user.isConnected()) {
                user.runLoginScreen();
                return;
            }

            if (url != null || enforceLock()) {
                url = new URL(url != null ? url : window.location.href, window.location.href);

                // Update history and route
                for (let [key, value] of url.searchParams)
                    nav.route[key] = value;
                if (push_history)
                    window.history.pushState(null, null, url.pathname);

                // Find relevant page
                let context;
                {
                    let path = url.pathname;

                    if (!path.endsWith('/'))
                        path += '/';

                    if (path === env.base_url) {
                        context = '';
                        route_page = app.pages[0];
                    } else if (path.startsWith(`${env.base_url}app/`)) {
                        path = path.substr(env.base_url.length + 4);

                        let split_offset = path.indexOf('/');
                        let page_key = path.substr(0, split_offset);

                        context = path.substr(split_offset + 1);
                        route_page = app.pages_map[page_key];
                    }

                    if (context.endsWith('/'))
                        context = context.substr(0, context.length - 1);
                }

                if (enforceLock())
                    context = '';

                // Route URL through controller
                if (route_page != null) {
                    try {
                        await form_exec.route(route_page, context);
                    } catch (err) {
                        log.error(err);
                        route_page = null;
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
            if (!user.isLocked()) {
                let show_develop = user.hasPermission('develop');
                let show_data = user.hasPermission('edit') &&
                                route_page != null && route_page.options.show_data;

                let correct_mode = (left_panel == null ||
                                    (left_panel === 'files' && show_develop) ||
                                    (left_panel === 'editor' && show_develop) ||
                                    (left_panel === 'status' && show_data) ||
                                    (left_panel === 'data' && show_data) ||
                                    (left_panel === 'describe' && show_data));
                if (!correct_mode)
                    left_panel = show_develop ? 'editor' : null;

                if (route_page == null) {
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
                if (route_page != null) {
                    document.title = `${route_page.title} — ${env.app_name}`;
                    await runPageSafe(route_page);
                } else {
                    document.title = env.app_name;
                }

                // Run accessory panel
                switch (left_panel) {
                    case 'files': { await dev_files.runFiles(); } break;
                    case 'editor': {
                        let path = route_page != null ? nav.makePath(route_page.key) : null;
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
        } finally {
            running--;

            for (let panel of document.querySelectorAll('.gp_panel')) {
                panel.classList.remove('busy');
                if (!panel.children.length)
                    panel.classList.add('broken');
            }
        }
    };
    this.go = util.serialize(this.go);

    function enforceLock()
    {
        if (user.isLocked()) {
            let lock = user.getLock();

            Object.assign(nav.route, lock.route);
            if (route_page != null && !lock.menu.some(item => item.key === route_page.key)) {
                let page_key = lock.menu[0].key;
                route_page = app.pages_map[page_key];

                return true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }

    async function checkEvents() {
        try {
            let response = await net.fetch(`${env.base_url}api/events`);

            if (response.ok) {
                net.setOnline(true);
                triggerSync();
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

    async function triggerSync() {
        if (env.sync_mode === 'mirror' && user.isConnected()) {
            let now = Date.now();

            if (user.getSessionRnd() !== sync_rnd || now - sync_time >= 180000) {
                await form_exec.syncRecords();

                sync_rnd = user.getSessionRnd();
                sync_time = now;
            }
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
            ${!in_iframe && !user.isLocked() ?
                html`<nav id="gp_menu" class="gp_toolbar">${renderFullMenu()}</nav>`: ''}
            ${!in_iframe && user.isLocked() ? html`<nav id="gp_menu" class="gp_toolbar locked">
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
        let show_data = user.hasPermission('edit') &&
                        route_page != null && route_page.options.show_data;
        let show_export = user.hasPermission('export');

        return html`
            ${show_develop ? html`
                <div class="gp_dropdown">
                    <button class=${left_panel === 'editor' || left_panel === 'files' ? 'icon active' : 'icon'}
                            style="font-weight: bold; background-position-y: calc(-538px + 1.2em)">${env.app_name}</button>
                    <div>
                        <button class=${left_panel === 'editor' ? 'active' : ''}
                                @click=${e => toggleLeftPanel('editor')}>Code</button>
                        <button class=${left_panel === 'files' ? 'active' : ''}
                                @click=${e => toggleLeftPanel('files')}>Déploiement</button>
                    </div>
                </div>
            ` : ''}
            ${!show_develop ?
                html`<button class="icon" style="font-weight: bold; background-position-y: calc(-538px + 1.2em)"
                             @click=${e => self.go(env.base_url)}>${env.app_name}</button>` : ''}
            ${show_data && show_export ? html`
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
            ${show_data && !show_export ?
                html`<button class=${left_panel === 'status' ? 'icon active' : 'icon'}
                             style="background-position-y: calc(-274px + 1.2em)"
                             @click=${e => toggleLeftPanel('status')}>Suivi</button>` : ''}
            ${route_page != null ?
                html`<button class=${show_overview ? 'icon active': 'icon'}
                             @click=${e => self.toggleOverview()}
                             style="background-position-y: calc(-318px + 1.2em)">Page</button>` : ''}

            ${show_develop ? html`
                <select id="gp_assets" @change=${e => self.go(e.target.value)}>
                    ${route_page == null ? html`<option>-- Sélectionnez une page --</option>` : ''}
                    ${app.pages.map(page =>
                        html`<option value=${nav.makeURL(page.key)}
                                     .selected=${page === route_page}>${page.title}</option>`)}
                </select>
            ` : ''}
            ${!show_develop ? html`<div style="flex: 1; text-align: center;">${env.app_name}</div>` : ''}

            <div class="gp_dropdown right">
                <button class="icon" style="background-position-y: calc(-494px + 1.2em)">${user.getUserName()}</button>
                <div>
                    <button @click=${user.runLoginDialog}>Changer d'utilisateur</button>
                    ${!user.isDemo() ? html`<button @click=${user.logout}>Déconnexion</button>` : ''}
                </div>
            </div>

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
        if (!net.isOnline())
            throw new Error('Cette action nécessite une connexion au serveur');

        // If user is really connected (with a server session key)
        if (user.isConnectedOnline()) {
            try {
                let ret = await func();
                return ret;
            } catch (err) {
                if (user.isSynced())
                    throw err;
            }

            await self.initMain();

            if (user.isConnectedOnline()) {
                let ret = await func();
                return ret;
            }
        }

        // Ask user to connect
        await user.runLoginDialog();

        let ret = await func();
        return ret;
    };

    this.toggleOverview = function(enable = undefined) {
        if (enable == null)
            enable = !show_overview || goupile.isTablet();

        if (enable !== show_overview) {
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
        }
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
            if (route_page != null)
                await runPageSafe(route_page);
            return true;
        } else {
            let m = path.match(/^\/files\/pages\/(.+)\.js$/);

            if (m != null) {
                let page_key = m[1];
                let page = app.pages_map[page_key];

                return await runPageSafe(page, code);
            } else {
                return true;
            }
        }
    };

    async function runPageSafe(page, code = undefined) {
        let error_el = document.querySelector('#gp_error');
        let overview_el = document.querySelector('#gp_overview');

        let test_el;
        let broken_template = false;
        if (page === route_page) {
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

        if (code == null) {
            try {
                let path = nav.makePath(page.key);
                code = await readCode(path);
            } catch (err) {
                overview_el.classList.add('broken');
                throw err;
            }
        }

        try {
            running++;

            form_exec.runPage(code, test_el);
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
