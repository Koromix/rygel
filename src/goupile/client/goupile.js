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

    let left_panel = null;
    let show_overview = true;

    let editor_el;
    let style_el;

    let popup_el;
    let popup_state;
    let popup_builder;

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

            if (env.use_offline && self.isStandalone()) {
                enablePersistence();
                await updateApplication();
            }
        }

        await self.initApplication();
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
        let entry = new log.Entry;

        entry.progress('Mise à jour de l\'application');
        try {
            let files = await virt_fs.status();

            if (files.some(file => file.action === 'pull' || file.action === 'conflict')) {
                if (files.some(file => file.action !== 'pull' && file.action !== 'noop'))
                    throw new Error('Impossible de mettre à jour (modifications locales)');

                await virt_fs.sync(files);
                entry.success('Mise à jour terminée !');
            } else {
                entry.close();
            }
        } catch (err) {
            entry.error(err.message);
        }
    }

    async function openDatabase() {
        let db_name = `goupile@${env.app_key}`;
        let db = await idb.open(db_name, 1, (db, old_version) => {
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

        let files = await virt_fs.listAll();
        let files_map = util.mapArray(files, file => file.path);

        try {
            let new_app = new Application;
            let app_builder = new ApplicationBuilder(new_app);

            if (code == null) {
                let file = await virt_fs.load('/files/main.js');
                code = file ? await file.data.text() : '';
            }

            let func = Function('util', 'app', 'data', 'route', code);
            func(util, app_builder, new_app.data, new_app.route);

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
                console.log(err);
            }
        }

        app.urls_map = util.mapArray(app.assets, asset => asset.url);
        app.paths_map = util.mapArray(app.assets, asset => asset.path);

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
            let file = await virt_fs.load('/files/main.css');

            if (file) {
                let css = await file.data.text();
                updateApplicationCSS(css);
            }
        }

        util.deepFreeze(app, 'route');
        self.go(route_url || window.location.href, false);
    };

    async function fetchSettings() {
        let session_rnd = util.getCookie('session_rnd');

        if (session_rnd !== settings_rnd) {
            settings = {};

            if (session_rnd != null) {
                let response = await net.fetch(`${env.base_url}api/settings.json?rnd=${session_rnd}`);
                if (response.ok) {
                    settings = await response.json();
                } else {
                    // The request has failed and could have deleted the session_rnd cookie
                    session_rnd = util.getCookie('session_rnd');
                }
            }

            settings_rnd = session_rnd;
            return true;
        } else {
            return false;
        }
    }

    function updateApplicationCSS(css) {
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
        if (url) {
            if (url.match(/(http|ftp|https):\/\//g) || url.startsWith('/')) {
                url = new URL(url, window.location.href);
            } else {
                url = new URL(`${env.base_url}app/${url}`, window.location.href);
            }

            // Update route application global
            for (let [key, value] of url.searchParams) {
                let num = Number(value);
                app.route[key] = Number.isNaN(num) ? value : num;
            }

            // Find relevant asset
            route_asset = app.urls_map[url.pathname] || app.urls_map[url.pathname + '/'];
            if (!route_asset) {
                let path = url.pathname;

                while (!route_asset && path.length) {
                    path = path.substr(0, path.length - 1);
                    path = path.substr(0, path.lastIndexOf('/') + 1);
                    route_asset = app.urls_map[path];
                }
            }
            route_url = url.pathname;

            // Route URL through appropriate controller
            if (route_asset) {
                switch (route_asset.type) {
                    case 'page': { await form_executor.route(route_asset, url); } break;
                }
            } else {
                log.error(`URL non supportée '${url.pathname}'`);
            }

            await run();
        } else {
            await run();
            push_history = false;
        }

        if (route_asset) {
            switch (route_asset.type) {
                case 'page': { route_url = form_executor.makeURL(); } break;
                default: { route_url = route_asset.url; } break;
            }
        }

        if (push_history) {
            window.history.pushState(null, null, route_url);
        } else {
            window.history.replaceState(null, null, route_url);
        }
    }

    async function run() {
        if (await fetchSettings()) {
            await self.initApplication();
            return;
        }

        // Render menu and page layout
        renderMainUI();

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
    };

    function renderMainUI() {
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
                    if (len === 1) {
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

            <div class="gp_dropdown right">
                <button>Administration</button>
                <div>
                    <button @click=${e => log.error('Fonctionnalité non disponible')}>Configuration</button>
                    <button @click=${e => log.error('Fonctionnalité non disponible')}>Utilisateurs</button>
                </div>
            </div>
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
        goupile.popup(e, page => {
            let username = page.text('username', 'Nom d\'utilisateur');
            let password = page.password('password', 'Mot de passe');

            page.submitHandler = async () => {
                page.close();

                let entry = new log.Entry;

                entry.progress('Connexion en cours');
                try {
                    let body = new URLSearchParams({
                        username: username.value,
                        password: password.value}
                    );

                    let response = await net.fetch(`${env.base_url}api/login.json`, {method: 'POST', body: body});

                    if (response.ok) {
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
        });
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
            updateApplicationCSS(code);
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

        try {
            switch (asset.type) {
                case 'main': {
                    if (code != null) {
                        await self.initApplication(code);

                        // If initApplication() succeeds it runs the page, so no need to redo it
                        return true;
                    }

                    render(html`<div class="gp_wip">Tableau de bord non disponible</div>`, test_el);
                } break;

                case 'page': {
                    if (code == null) {
                        let file = await virt_fs.load(asset.path);
                        code = file ? await file.data.text() : '';
                    }

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
        }
    }

    this.popup = function(e, func) {
        closePopup();
        openPopup(e, func);
    };

    function openPopup(e, func) {
        if (!popup_el)
            initPopup();

        let page = new Page('@popup');

        popup_builder = new PageBuilder(popup_state, page);
        popup_builder.changeHandler = () => openPopup(e, func);
        popup_builder.close = closePopup;

        popup_builder.pushOptions({
            missingMode: 'disable',
            wide: true
        });

        func(popup_builder);
        render(page.render(), popup_el);

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

    function initPopup() {
        popup_el = document.createElement('div');
        popup_el.setAttribute('id', 'gp_popup');
        document.body.appendChild(popup_el);

        popup_el.addEventListener('keydown', e => {
            switch (e.keyCode) {
                case 13: {
                    if (e.target.tagName !== 'BUTTON' && e.target.tagName !== 'A' &&
                            popup_builder.submit)
                        popup_builder.submit();
                } break;
                case 27: { closePopup(); } break;
            }
        });

        popup_el.addEventListener('click', e => e.stopPropagation());
        document.addEventListener('click', closePopup);
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
