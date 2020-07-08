// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// These globals are initialized below
let app = null;
let nav = null;
let virt_fs = null;
let virt_rec = null;

let goupile = new function() {
    let self = this;

    let tablet_mq = window.matchMedia('(pointer: coarse)');
    let standalone_mq = window.matchMedia('(display-mode: standalone)');

    let settings = {};
    let settings_rnd;

    let route_asset;
    let route_url;

    let running = false;
    let restart = false;

    let left_panel = null;
    let show_overview = true;
    let overview_wanted = false;

    let editor_el;
    let style_el;
    let template_el;

    let popup_el;
    let popup_state;
    let popup_builder;

    document.addEventListener('readystatechange', e => {
        if (document.readyState === 'complete')
            initGoupile();
    });

    async function initGoupile() {
        try {
            log.pushHandler(log.notifyHandler);

            initNavigation();

            let db = await openDatabase();
            virt_fs = new VirtualFS(db);
            virt_rec = new VirtualRecords(db);

            if (navigator.serviceWorker) {
                navigator.serviceWorker.register(`${env.base_url}sw.pk.js`);

                if (env.use_offline) {
                    enablePersistence();
                    await updateApplication();
                }
            }

            nav = new ApplicationNavigator();
            await self.initApplication();

            // If a run fails and we can run in offline mode, restart it transparently
            net.changeHandler = online => {
                if (env.use_offline)
                    self.go();
            };
        } finally {
            document.querySelector('#gp_all').classList.remove('busy');
        }
    }

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
        let db_name = `goupile:${env.app_key}`;
        let db = await idb.open(db_name, 2, (db, old_version) => {
            switch (old_version) {
                case null: {
                    db.createStore('fs_entries', {keyPath: 'path'});
                    db.createStore('fs_data');
                    db.createStore('fs_mirror', {keyPath: 'path'});

                    db.createStore('rec_entries', {keyPath: '_ikey'});
                    db.createStore('rec_fragments', {keyPath: '_ikey'});
                    db.createStore('rec_variables', {keyPath: '_ikey'});
                } // fallthrough

                case 1: {
                    db.deleteStore('fs_mirror');
                    db.createStore('fs_sync', {keyPath: 'path'});
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
            try {
                let new_app = new Application;
                let app_builder = new ApplicationBuilder(new_app);

                if (code == null)
                    code = await readCode('/files/main.js');

                let func = Function('util', 'app', 'shared', 'go', 'route', code);
                func(util, app_builder, new_app.shared, nav.go, new_app.route);

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

            // XXX: Hack for secondary asset thingy that we'll get rid of eventually
            for (let i = 0; i < app.assets.length; i++)
                app.assets[i].idx = i;

            // Select default page
            app.urls_map[env.base_url] = app.home ? app.urls_map[app.home] : app.assets[0];

            // Load custom template and CSS (if any)
            let html = await readCode('/files/main.html') || '';
            let css = await readCode('/files/main.css') || '';
            changeTemplate(html);
            changeCSS(css);

            util.deepFreeze(app, 'route');
        } else {
            app = null;
        }

        await self.go(route_url || window.location.href, false);
    };

    async function fetchSettings(force = false) {
        let session_rnd = util.getCookie('session_rnd');

        if (!force) {
            if (session_rnd == settings_rnd) {
                return false;
            } else if (session_rnd == null) {
                settings = {};
                settings_rnd = null;

                return true;
            }
        }

        if (net.isPlugged() || force) {
            settings = {};

            try {
                let response = await net.fetch(util.pasteURL(`${env.base_url}api/settings.json`, {
                    rnd: session_rnd
                }));

                if (response.ok) {
                    settings = await response.json();
                } else {
                    // The request has failed and could have deleted the session_rnd cookie
                    session_rnd = util.getCookie('session_rnd');
                }

                settings_rnd = session_rnd;
                return true;
            } catch (err) {
                // Too bad :)
            }

            return true;
        }
    }

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

    this.isConnected = function() { return !!settings_rnd; };
    this.isTablet = function() { return tablet_mq.matches; };
    this.isStandalone = function() { return standalone_mq.matches; };
    this.isLocked = function() { return !!getLockURL(); };

    this.go = async function(url = null, push_history = true) {
        if (running) {
            restart = true;
            return;
        }

        try {
            running = true;
            url = getLockURL() || url;

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
                            log.error(err);
                        }
                    } else {
                        log.error(`URL non supportée '${url.pathname}'`);
                    }
                }

                // Restart application after session changes
                if (await fetchSettings()) {
                    self.initApplication();
                    return;
                }

                // Ensure valid menu and panel configuration
                if (!getLockURL()) {
                    let show_develop = settings.develop;
                    let show_data = settings.edit && route_asset && route_asset.form;

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
                        case 'editor': { await dev_files.runEditor(route_asset); } break;
                        case 'status': { await form_executor.runStatus(); } break;
                        case 'data': { await form_executor.runData(); } break;
                        case 'describe': { await form_executor.runDescribe(); } break;
                    }
                } catch (err) {
                    log.error(err);
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
            } else {
                for (let panel of document.querySelectorAll('.gp_panel')) {
                    panel.classList.remove('busy');
                    if (!panel.children.length)
                        panel.classList.add('broken');
                }
            }
        }
    };

    function renderAll() {
        let in_iframe;
        try {
            in_iframe = (document.location.hostname !== window.parent.location.hostname);
        } catch (e) {
            in_iframe = true;
        }

        render(html`
            ${!in_iframe && !getLockURL() ?
                html`<nav id="gp_menu" class="gp_toolbar">${renderFullMenu()}</nav>`: ''}
            ${!in_iframe && getLockURL() ? html`<nav id="gp_menu" class="gp_toolbar locked">
                &nbsp;&nbsp;Application verrouillée
                <div style="flex: 1;"></div>
                <button type="button" @click=${toggleLock}>Déverrouiller</button>
                <button @click=${showLoginDialog}>Connexion</button>
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
        `, document.querySelector('#gp_all'));

        for (let panel of document.querySelectorAll('.gp_panel:empty')) {
            panel.classList.remove('broken');
            panel.classList.add('busy');
        }
    }

    function renderFullMenu() {
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

        return html`
            ${settings.develop ? html`
                <div class="gp_dropdown">
                    <button class=${left_panel === 'editor' || left_panel === 'files' ? 'active' : ''}>Code</button>
                    <div>
                        <button class=${left_panel === 'editor' ? 'active' : ''}
                                @click=${e => toggleLeftPanel('editor')}>Éditeur</button>
                        <button class=${left_panel === 'files' ? 'active' : ''}
                                @click=${e => toggleLeftPanel('files')}>Déploiement</button>
                    </div>
                </div>
            ` : ''}

            ${settings.edit && route_asset && route_asset.form ? html`
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

            ${!self.isConnected() ? html`<button @click=${showLoginDialog}>Connexion</button>` : ''}
            ${self.isConnected() ? html`
                <div class="gp_dropdown right">
                    <button>${settings.username}</button>
                    <div>
                        <button type="button" @click=${toggleLock}>Verrouiller</button>
                        <hr/>
                        ${env.use_offline ? html`
                            <button type="button" @click=${showSyncDialog}>Synchroniser</button>
                            <hr/>
                        ` : ''}
                        <button @click=${showLoginDialog}>Changer d'utilisateur</button>
                        <button @click=${logout}>Déconnexion</button>
                    </div>
                </div>
            ` : ''}

            ${env.use_offline ? html`<button type="button" id="gp_status" class="icon" @click=${toggleStatus} />` : ''}
            ${!env.use_offline ? html`<div id="gp_status"/>` : ''}
        `;
    }

    async function toggleStatus() {
        if (net.isOnline()) {
            net.setPlugged(false);
        } else {
            net.setPlugged(true);
            await fetchSettings(true);
        }

        await self.go();
    }

    function showLoginDialog(e) {
        goupile.popup(e, 'Connexion', makeLoginForm);
    }

    function makeLoginForm(page, close = null) {
        let username = page.text('*username', 'Nom d\'utilisateur');
        let password = page.password('*password', 'Mot de passe');

        page.submitHandler = async () => {
            let entry = new log.Entry;

            entry.progress('Connexion en cours');
            try {
                let body = new URLSearchParams({
                    username: username.value.toLowerCase(),
                    password: password.value
                });

                let response = await net.fetch(`${env.base_url}api/login.json`, {method: 'POST', body: body});

                if (response.ok) {
                    if (close)
                        close();

                    // Emergency unlocking
                    deleteLock();

                    entry.success('Connexion réussie');
                    await self.initApplication();
                } else {
                    let msg = await response.text();
                    entry.error(msg);
                }
            } catch (err) {
                entry.error(err);
            }
        };
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
            entry.error(err);
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
        overview_wanted = false;

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

    function showSyncDialog(e) {
        goupile.popup(e, 'Synchroniser', (page, close) => {
            page.output('Désirez-vous synchroniser les données ?');

            page.submitHandler = () => {
                close();
                syncRecords();
            };
        });
    }

    async function syncRecords() {
        let entry = new log.Entry;

        entry.progress('Synchronisation des données en cours');
        try {
            await virt_rec.sync();

            entry.success('Données synchronisées !');
        } catch (err) {
            entry.error(`La synchronisation a échoué : ${err.message}`);
        }

        self.go();
    }

    this.validateCode = async function(path, code) {
        if (path === '/files/main.js') {
            nav.block();
            try {
                await self.initApplication(code);

                showScriptError(null);
                return true;
            } catch (err) {
                showScriptError(err);
                return false;
            } finally {
                nav.unblock();
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
            let asset = app.paths_map[path];
            return asset ? await runAssetSafe(asset, code) : true;
        }
    };

    async function runAssetSafe(asset, code = null) {
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

        nav.block();
        try {
            switch (asset.type) {
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

            if (broken_template)
                throw new Error(`Le modèle ne contient pas d'élément avec l'ID '@page'`);

            showScriptError(null);
            return true;
        } catch (err) {
            showScriptError(err);
            return false;
        } finally {
            nav.unblock();
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
            let file = await virt_fs.load(path);
            return file ? await file.data.text() : null;
        }
    }

    function updateStatus() {
        let el = document.querySelector('#gp_status');

        if (el) {
            if (!net.isPlugged()) {
                el.style.backgroundPosition = '-136px calc(-21px + 1.2em)';
                el.title = 'Mode hors-ligne forcé';
            } else if (net.isOnline()) {
                el.style.backgroundPosition = '-48px calc(-21px + 1.2em)';
                el.title = 'Serveur disponible';
            } else {
                el.style.backgroundPosition = '-92px calc(-21px + 1.2em)';
                el.title = 'Serveur non disponible';
            }
        }
    }

    function renderGuest() {
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
            builder.actions([['Connexion', builder.isValid() ? builder.submit : null]]);

            let focus = !document.querySelector('#gp_login');

            render(html`
                <div style="flex: 1;"></div>
                <form id="gp_login" @submit=${e => e.preventDefault()}>
                    <h1>${env.app_name}</h1>
                    ${page.render()}
                </form>
                <div style="flex: 1;"></div>
            `, document.querySelector('#gp_all'));

            if (focus) {
                let el = document.querySelector('#gp_login input');
                setTimeout(() => el.focus(), 0);
            }
        };

        update();
    }

    function getLockURL() {
        let url = localStorage.getItem('lock_url');
        return url;
    }

    function toggleLock(e) {
        if (getLockURL()) {
            goupile.popup(e, null, (page, close) => {
                page.output('Entrez le code de déverrouillage');
                let pin = page.pin('code');

                if (pin.value && pin.value.length >= 4) {
                    let code = localStorage.getItem('lock_pin');

                    if (pin.value === code) {
                        setTimeout(close, 0);

                        deleteLock();

                        log.success('Application déverrouillée !');
                        self.go();
                    } else if (pin.value.length >= code.length) {
                        pin.error('Code erroné');
                    }
                }
            });
        } else {
            goupile.popup(e, 'Verrouiller', (page, close) => {
                page.output('Entrez le code de verrouillage');
                let pin = page.pin('*code');

                if (pin.value && pin.value.length < 4)
                    pin.error('Le code doit comporter au moins 4 chiffres', true);

                page.submitHandler = () => {
                    close();

                    localStorage.setItem('lock_url', route_url);
                    localStorage.setItem('lock_pin', pin.value);

                    log.success('Application verrouillée !');
                    self.go();
                };
            });
        }
    }

    function deleteLock() {
        localStorage.removeItem('lock_url');
        localStorage.removeItem('lock_pin');
    }

    this.popup = function(e, action, func) {
        closePopup();
        openPopup(e, action, func);
    };

    function openPopup(e, action, func) {
        if (!popup_el) {
            popup_el = document.createElement('div');
            popup_el.setAttribute('id', 'gp_popup');
            document.body.appendChild(popup_el);

            popup_el.addEventListener('keydown', e => {
                if (e.keyCode == 27)
                    closePopup();
            });

            popup_el.addEventListener('click', e => e.stopPropagation());
            document.addEventListener('click', closePopup);
        }

        let page = new Page('@popup');

        popup_builder = new PageBuilder(popup_state, page);
        popup_builder.changeHandler = () => openPopup(...arguments);
        popup_builder.pushOptions({
            missingMode: 'disable',
            wide: true
        });

        func(popup_builder, closePopup);
        popup_builder.actions([
            action != null ? [action, popup_builder.isValid() ? popup_builder.submit : null,
                                      !popup_builder.isValid() ? 'Erreurs ou données manquantes' : null] : null,
            ['Annuler', closePopup]
        ]);
        render(html`
            <form @submit=${e => e.preventDefault()}>
                ${page.render()}
            </form>
        `, popup_el);

        // We need to know popup width and height
        let give_focus = !popup_el.classList.contains('active');
        popup_el.style.visibility = 'hidden';
        popup_el.classList.add('active');

        // Try different positions
        {
            let origin;
            if (e.clientX && e.clientY) {
                origin = {
                    x: e.clientX - 1,
                    y: e.clientY - 1
                };
            } else {
                let rect = e.target.getBoundingClientRect();
                origin = {
                    x: (rect.left + rect.right) / 2,
                    y: (rect.top + rect.bottom) / 2
                };
            }

            let pos = {
                x: origin.x,
                y: origin.y
            };
            if (pos.x > window.innerWidth - popup_el.offsetWidth - 10) {
                pos.x = origin.x - popup_el.offsetWidth;
                if (pos.x < 10) {
                    pos.x = Math.min(origin.x, window.innerWidth - popup_el.offsetWidth - 10);
                    pos.x = Math.max(pos.x, 10);
                }
            }
            if (pos.y > window.innerHeight - popup_el.offsetHeight - 10) {
                pos.y = origin.y - popup_el.offsetHeight;
                if (pos.y < 10) {
                    pos.y = Math.min(origin.y, window.innerHeight - popup_el.offsetHeight - 10);
                    pos.y = Math.max(pos.y, 10);
                }
            }

            popup_el.style.left = pos.x + 'px';
            popup_el.style.top = pos.y + 'px';
        }

        if (e.stopPropagation)
            e.stopPropagation();

        // Reveal!
        popup_el.style.visibility = 'visible';

        if (give_focus) {
            // Avoid shrinking popups
            popup_el.style.minWidth = popup_el.offsetWidth + 'px';

            // Give focus to first input
            let first_widget = popup_el.querySelector(`.af_widget input, .af_widget select,
                                                       .af_widget button, .af_widget textarea`);
            if (first_widget)
                first_widget.focus();
        }
    }

    function closePopup() {
        popup_state = new PageState;
        popup_builder = null;

        if (popup_el) {
            popup_el.classList.remove('active');
            popup_el.style.minWidth = '';

            render('', popup_el);
        }
    }
};
