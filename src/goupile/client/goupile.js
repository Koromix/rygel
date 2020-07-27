// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// These globals are initialized below
let app = null;
let nav = null;
let vfs = null;
let vrec = null;

let goupile = new function() {
    let self = this;

    let tablet_mq = window.matchMedia('(pointer: coarse)');
    let standalone_mq = window.matchMedia('(display-mode: standalone)');

    // This probably belongs to user.js
    let settings = {};
    let settings_rnd;

    let route_asset;
    let route_url;

    let running = 0;
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

    this.startApp = async function() {
        try {
            log.pushHandler(log.notifyHandler);

            initNavigation();

            let db = await openDatabase();
            vfs = new VirtualFS(db);
            vrec = new VirtualRecords(db);

            if (navigator.serviceWorker) {
                navigator.serviceWorker.register(`${env.base_url}sw.pk.js`);

                if (env.use_offline) {
                    enablePersistence();
                    await updateApplication();
                }
            }

            nav = new ApplicationNavigator();
            await self.initMain();

            // If a run fails and we can run in offline mode, restart it transparently
            net.changeHandler = online => {
                if (env.use_offline)
                    self.go();
            };
        } finally {
            document.querySelector('#gp_all').classList.remove('busy');
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
        let db = await idb.open(db_name, 3, (db, old_version) => {
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
        await fetchSettings();

        if (self.isConnected() || env.allow_guests) {
            try {
                running++;

                let new_app = new ApplicationInfo;
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

                    // Update URL and history
                    route_url = url.pathname;
                    if (push_history)
                        window.history.pushState(null, null, route_url);

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
                if (await fetchSettings()) {
                    self.initMain();
                    return;
                }

                // Ensure valid menu and panel configuration
                if (!user.getLockURL()) {
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
                } finally {
                    updateStatus();
                }

                // Give popup (if any) a chance to refresh too
                if (popup_builder)
                    popup_builder.changeHandler();
            } else {
                user.runLogin();
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
                        @click=${user.showLoginDialog}>Connexion</button>
                <button type="button" class="icon" style="background-position-y: calc(-186px + 1.2em)"
                        @click=${user.showUnlockDialog}>Déverrouiller</button>
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
        let show_develop = settings.develop;
        let show_data = settings.edit && route_asset && route_asset.form;

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
                             @click=${user.showLoginDialog}>Connexion</button>` : ''}
            ${self.isConnected() ? html`
                <div class="gp_dropdown right">
                    <button class="icon" style="background-position-y: calc(-494px + 1.2em)">${settings.username}</button>
                    <div>
                        <button type="button" @click=${e => user.showLockDialog(e, route_url)}>Verrouiller</button>
                        <hr/>
                        ${env.use_offline ? html`
                            <button type="button" @click=${showSyncDialog}>Synchroniser</button>
                            <hr/>
                        ` : ''}
                        <button @click=${user.showLoginDialog}>Changer d'utilisateur</button>
                        <button @click=${user.logout}>Déconnexion</button>
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
            await vrec.sync();

            entry.success('Données synchronisées !');
        } catch (err) {
            entry.error(`La synchronisation a échoué : ${err.message}`);
        }

        self.go();
    }

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
            let asset = app.paths_map[path];
            return asset ? await runAssetSafe(asset, code) : true;
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
            if (!net.isPlugged()) {
                el.style.backgroundPositionY = 'calc(-142px + 1.2em)';
                el.title = 'Mode hors-ligne forcé';
            } else if (net.isOnline()) {
                el.style.backgroundPositionY = 'calc(-54px + 1.2em)';
                el.title = 'Serveur disponible';
            } else {
                el.style.backgroundPositionY = 'calc(-98px + 1.2em)';
                el.title = 'Serveur non disponible';
            }
        }
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

            document.addEventListener('click', e => {
                let el = e.target;
                while (el) {
                    if (el === popup_el)
                        return;
                    el = el.parentNode;
                }
                closePopup();
            });
        }

        let model = new PageModel('@popup');

        popup_builder = new PageBuilder(popup_state, model);
        popup_builder.changeHandler = () => openPopup(...arguments);
        popup_builder.pushOptions({
            missingMode: 'disable',
            wide: true
        });

        func(popup_builder, closePopup);
        if (action != null)
            popup_builder.action(action, {disabled: !popup_builder.isValid()}, popup_builder.submit);
        popup_builder.action('Annuler', closePopup);

        render(html`
            <form @submit=${e => e.preventDefault()}>
                ${model.render()}
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
