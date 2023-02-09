// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

function InstanceController() {
    let self = this;

    let app;

    // Explicit mutex to serialize (global) state-changing functions
    let mutex = new Mutex;

    let route = {
        page: null,
        menu: null
    };

    let head_length = Number.MAX_SAFE_INTEGER;
    let page_div = document.createElement('div');

    let form_state;

    let editor_el;
    let editor_ace;
    let editor_filename;
    let code_buffers = new LruMap(32);
    let code_timer;

    let dev_db;
    let error_entries = {};

    let ignore_editor_change = false;
    let ignore_editor_scroll = 0;
    let ignore_page_scroll = 0;

    let prev_anchor;

    this.init = async function() {
        if (profile.develop) {
            ENV.urls.files = `${ENV.urls.base}files/0/`;
            ENV.version = 0;

            await openDevelopDB();
        }

        await initApp();
        initUI();

        if (profile.develop)
            code_timer = setTimeout(uploadFsChanges, 1000);
    };

    async function openDevelopDB() {
        let db_name = `goupile/dev${ENV.urls.instance}`;

        dev_db = await indexeddb.open(db_name, 1, (db, old_version) => {
            switch (old_version) {
                case null: {
                    db.createStore('fs_changes');
                } // fallthrough
            }
        });
    }

    async function initApp() {
        let code = await fetchCode('main.js');

        try {
            let new_app = new ApplicationInfo;
            let builder = new ApplicationBuilder(new_app);

            await runUserCode('Application', code, {
                app: builder
            });
            if (!new_app.pages.size)
                throw new Error('Main script does not define any page');

            new_app.homepage = new_app.pages.values().next().value;
            app = util.deepFreeze(new_app);
        } catch (err) {
            if (app == null) {
                let new_app = new ApplicationInfo;
                let builder = new ApplicationBuilder(new_app);

                // For simplicity, a lot of code assumes at least one page exists
                builder.form('default', 'Défaut', 'Page par défaut');

                new_app.homepage = new_app.pages.values().next().value;
                app = util.deepFreeze(new_app);
            }
        }

        if (app.head != null) {
            let template = document.createElement('template');
            render(app.head, template);

            // Clear previous changes
            for (let i = document.head.children.length - 1; i >= head_length; i--)
                document.head.removeChild(document.head.children[i]);
            head_length = document.head.children.length;

            for (let child of template.children)
                document.head.appendChild(child);
        }
    }

    function initUI() {
        ui.setMenu(renderMenu);

        if (app.panels.editor)
            ui.createPanel('editor', ['view'], 'view', renderEditor);
        ui.createPanel('view', [], null, renderPage);

        if (app.panels.editor) {
            ui.setPanelState('editor', true);
        } else if (app.panels.view) {
            ui.setPanelState('view', true);
        }
    };

    this.hasUnsavedData = function() {
        if (form_state == null)
            return false;
        if (!route.page.options.warn_unsaved)
            return false;

        return form_state.hasChanged();
    };

    this.runTasks = async function(online) {
        // Nothing to do for now
    };
    this.runTasks = util.serialize(this.runTasks, mutex);

    function renderMenu() {
        let show_menu = (profile.lock == null && (route.menu.chain.length > 2 || route.menu.chain[0].children.length > 1));
        let show_title = !show_menu;
        let menu_is_wide = (show_menu && route.menu.chain[0].children.length > 3);

        let user_icon = goupile.isLoggedOnline() ? 450 : 494;

        return html`
            <nav class=${goupile.isLocked() ? 'ui_toolbar locked' : 'ui_toolbar'} id="ui_top" style="z-index: 999999;">
                ${goupile.hasPermission('build_code') ? html`
                    <button class=${'icon' + (profile.develop ? ' active' : '')}
                            style="background-position-y: calc(-230px + 1.2em);"
                            @click=${ui.wrapAction(e => changeDevelopMode(!profile.develop))}>Conception</button>
                ` : ''}
                ${profile.lock != null ? html`
                    <button class="icon" style="background-position-y: calc(-186px + 1.2em)"
                            @click=${ui.wrapAction(goupile.runUnlockDialog)}>Déverrouiller</button>
                ` : ''}

                ${app.panels.editor ? html`
                    <div style="width: 4px;"></div>
                    <button class=${!ui.hasTwoPanels() && ui.isPanelActive('editor') ? 'icon active' : 'icon'}
                            style="background-position-y: calc(-230px + 1.2em);"
                            @click=${ui.wrapAction(e => togglePanels(true, false))}></button>
                    ${ui.allowTwoPanels() ? html`
                        <button class=${ui.hasTwoPanels() ? 'icon active' : 'icon'}
                                style="background-position-y: calc(-626px + 1.2em);"
                                @click=${ui.wrapAction(e => togglePanels(true, true))}></button>
                    ` : ''}
                    <button class=${!ui.hasTwoPanels() && ui.isPanelActive('view') ? 'icon active' : 'icon'}
                            style="background-position-y: calc(-318px + 1.2em);"
                            @click=${ui.wrapAction(e => togglePanels(false, true))}></button>
                ` : ''}
                <div style="flex: 1; min-width: 4px;"></div>

                ${show_menu && !menu_is_wide ? util.map(route.menu.chain[0].children, item => {
                    if (item.children.length) {
                        let active = route.menu.chain.includes(item);

                        return html`
                            <div id="ins_drop" class="drop">
                                <button title=${item.title} class=${active ? 'active' : ''}
                                        @click=${ui.deployMenu}>${item.title}</button>
                                <div>${util.map(item.children, item => renderDropItem(item))}</div>
                            </div>
                        `;
                    } else {
                        return renderDropItem(item);
                    }
                }) : ''}
                ${show_menu && menu_is_wide ? route.menu.chain.map(item => {
                    if (item.children.length) {
                        return html`
                            <div id="ins_drop" class="drop">
                                <button title=${item.title} @click=${ui.deployMenu}>${item.title}</button>
                                <div>${util.map(item.children, child => renderDropItem(child))}</div>
                            </div>
                        `;
                    } else {
                        return renderDropItem(item);
                    }
                }) : ''}
                ${show_title ? html`<button title=${route.page.title} class="active">${route.page.title}</button>` : ''}
                <div style="flex: 1; min-width: 4px;"></div>

                ${!goupile.isLocked() && profile.instances == null ?
                    html`<button class="icon" style="background-position-y: calc(-538px + 1.2em);"
                                 @click=${e => self.go(e, ENV.urls.instance)}>${ENV.title}</button>` : ''}
                ${!goupile.isLocked() && profile.instances != null ? html`
                    <div class="drop right" @click=${ui.deployMenu}>
                        <button class="icon" style="background-position-y: calc(-538px + 1.2em);"
                                @click=${ui.deployMenu}>${ENV.title}</button>
                        <div>
                            ${profile.instances.slice().sort(util.makeComparator(instance => instance.name))
                                               .map(instance =>
                                html`<button class=${instance.url === ENV.urls.instance ? 'active' : ''}
                                             @click=${e => self.go(e, instance.url)}>${instance.name}</button>`)}
                        </div>
                    </div>
                ` : ''}
                ${profile.lock == null ? html`
                    <div class="drop right">
                        <button class="icon" style=${'background-position-y: calc(-' + user_icon + 'px + 1.2em);'}
                                @click=${ui.deployMenu}>${profile.type !== 'auto' ? profile.username : ''}</button>
                        <div>
                            ${profile.type === 'auto' && profile.userid ? html`
                                <button style="text-align: center;">
                                    ${profile.username}<br/>
                                    <span style="font-size: 0.8em; font-style: italic; color: #555;">Identifiant temporaire</span>
                                </button>
                                <hr/>
                            ` : ''}
                            ${profile.type === 'login' ? html`
                                <button @click=${ui.wrapAction(goupile.runChangePasswordDialog)}>Modifier mon mot de passe</button>
                                <button @click=${ui.wrapAction(goupile.runResetTOTP)}>Configurer la double authentification</button>
                                <hr/>
                                ${goupile.hasPermission('data_export') ? html`
                                    <button @click=${ui.wrapAction(generateExportKey)}>Générer une clé d'export</button>
                                    <hr/>
                                ` : ''}
                            ` : ''}
                            ${profile.admin || goupile.hasPermission('build_admin') ? html`
                                <button @click=${e => window.open('/admin/')}>Administration</button>
                                <hr/>
                            ` : ''}
                            ${profile.userid < 0 ? html`<button @click=${ui.wrapAction(goupile.logout)}>Changer de compte</button>` : ''}
                            <button @click=${ui.wrapAction(goupile.logout)}>${profile.userid ? 'Se déconnecter' : 'Se connecter'}</button>
                        </div>
                    </div>
                ` : ''}
                ${profile.lock != null ?
                    html`<button class="icon" @click=${ui.wrapAction(goupile.goToLogin)}
                                 style="background-position-y: calc(-450px + 1.2em);">Se connecter</button>` : ''}
            </nav>
        `;
    }

    async function changeDevelopMode(enable) {
        if (enable == profile.develop)
            return;

        await mutex.run(async () => {
            let response = await net.fetch(`${ENV.urls.instance}api/change/mode`, {
                method: 'POST',
                body: JSON.stringify({
                    develop: enable
                })
            });

            if (response.ok) {
                try {
                    if (enable) {
                        url = util.pasteURL(route.page.url, { p: 'editor|view' });
                        goupile.syncHistory(url, false);
                    }

                    await applyMainScript();
                } catch (err) {
                    // We want to reload no matter what, because the mode has changed
                    document.location.reload();
                }
            } else {
                let err = await net.readError(response);
                throw new Error(err);
            }
        });
    }

    async function generateExportKey(e) {
        let response = await net.fetch(`${ENV.urls.instance}api/change/export_key`, { method: 'POST' });

        if (!response.ok) {
            let err = await net.readError(response);
            throw new Error(err);
        }

        let export_key = (await response.text()).trim();

        await ui.runDialog(e, 'Clé d\'export', {}, (d, resolve, reject) => {
            d.text('export_key', 'Clé d\'export', {
                value: export_key,
                readonly: true
            });
        });
    }

    function renderDropItem(item) {
        let active = route.menu.chain.includes(item);

        return html`
            <button class=${active && ui.isPanelActive('view') ? 'active' : ''}
                    @click=${ui.wrapAction(e => active ? togglePanels(null, true) : self.go(e, item.url))}>
                <div style="flex: 1;">${item.title}</div>
           </button>
        `;
    }

    async function togglePanels(primary, view) {
        if (primary != null)
            ui.setPanelState('editor', true);
        if (view != null)
            ui.setPanelState('view', view, primary !== false);

        await self.run();

        // Special behavior for some panels
        if (primary) {
            syncFormScroll();
            syncFormHighlight(true);
        }
        if (view) {
            syncEditorScroll();
            syncFormHighlight(false);
        }
    }

    function renderEditor() {
        // Ask ACE to adjust if needed, it needs to happen after the render
        setTimeout(() => editor_ace.resize(false), 0);

        let tabs = getEditorTabs();
        let active_tab = tabs.find(tab => tab.active);

        return html`
            <div style="--menu_color: #1d1d1d; --menu_color_n1: #2c2c2c;">
                <div class="ui_toolbar">
                    <div class="drop">
                        <button @click=${ui.deployMenu}>${active_tab.title}</button>
                        <div>
                            ${tabs.map(tab => html`<button class=${ui.isPanelActive('editor') && tab.active ? 'active' : ''}
                                                           @click=${ui.wrapAction(e => toggleEditorFile(e, tab.filename))}>${tab.title}</button>`)}
                        </div>
                    </div>
                    <div style="flex: 1;"></div>
                    <button @click=${ui.wrapAction(e => runHistoryDialog(e, editor_filename))}>Historique</button>
                    <div style="flex: 1;"></div>
                    ${editor_filename === 'main.js' ? html`
                        <button ?disabled=${!fileHasChanged('main.js')}
                                @click=${ui.wrapAction(applyMainScript)}>Appliquer</button>
                    ` : ''}
                    <button @click=${ui.wrapAction(runPublishDialog)}>Publier</button>
                </div>

                ${editor_el}
            </div>
        `;
    }

    function getEditorTabs() {
        let tabs = [];

        tabs.push({
            title: 'Projet',
            filename: 'main.js',
            active: false
        });
        tabs.push({
            title: 'Formulaire',
            filename: route.page.filename,
            active: false
        });

        for (let tab of tabs)
            tab.active = (editor_filename == tab.filename);

        return tabs;
    }

    async function runHistoryDialog(e, filename) {
        await uploadFsChanges();
        await fetchCode(filename);

        let url = util.pasteURL(`${ENV.urls.base}api/files/history`, { filename: filename });
        let versions = await net.fetchJson(url);

        let buffer = code_buffers.get(filename);
        let copy = Object.assign({}, code_buffers.get(filename));

        // Don't trash the undo/redo buffer
        buffer.session = null;

        let p = ui.runDialog(e, 'Historique du fichier', {}, (d, resolve, reject) => {
            d.output(html`
                <table class="ui_table">
                    <colgroup>
                        <col/>
                        <col/>
                        <col/>
                    </colgroup>

                    <tbody>
                        <tr class=${buffer.version == 0 ? 'active' : ''}>
                            <td class="ui_sub">(dev)</td>
                            <td>En développement</td>
                            <td><a @click=${ui.wrapAction(e => loadFile(filename, 0))}>Charger</a></td>
                        </tr>

                        ${util.mapRange(0, versions.length - 1, idx => {
                            let version = versions[versions.length - idx - 1];

                            return html`
                                <tr class=${buffer.version == version.version ? 'active' : ''}>
                                    <td class="ui_sub">${version.version}</td>
                                    <td>${(new Date(version.mtime)).toLocaleString()}</td>
                                    <td><a @click=${ui.wrapAction(e => loadFile(filename, version.version))}>Charger</a></td>
                                </tr>
                            `;
                        })}
                    </tbody>
                </table>
            `);

            d.action('Restaurer', { disabled: !d.isValid() }, async () => {
                await restoreFile(filename, buffer.sha256);
                resolve();
            });
        });
        p.catch(err => {
            code_buffers.set(filename, copy);
            self.go();

            throw err;
        });

        return p;
    }

    async function loadFile(filename, version) {
        await fetchCode(filename, version);
        return self.go();
    }

    async function restoreFile(filename, sha256) {
        let response = await net.fetch(`${ENV.urls.base}api/files/restore`, {
            method: 'POST',
            body: JSON.stringify({
                filename: filename,
                sha256: sha256
            })
        });
        if (!response.ok && response.status !== 409) {
            let err = await net.readError(response);
            throw new Error(err)
        }

        let key = `${profile.userid}:${filename}`;
        await dev_db.delete('fs_changes', key);

        code_buffers.delete(filename);

        return self.go();
    }

    async function applyMainScript() {
        let code = await fetchCode('main.js');

        let new_app = new ApplicationInfo;
        let builder = new ApplicationBuilder(new_app);

        try {
            await runUserCode('Application', code, {
                app: builder
            });
        } catch (err) {
            // Don't log, because runUserCode does it already
            return;
        }
        if (!new_app.pages.size)
            throw new Error('Main script does not define any page');

        // It works! Refresh the page
        document.location.reload();
    }

    async function renderPage() {
        let filename = route.page.filename;
        let code = code_buffers.get(filename).code;

        let model = new FormModel;
        let builder = new FormBuilder(form_state, model);

        try {
            await runUserCode('Formulaire', code, {
                app: app,
                form: builder,
                values: form_state.values
            });

            if (model.hasErrors())
                builder.errorList();

            render(model.renderWidgets(), page_div);
            page_div.classList.remove('disabled');
        } catch (err) {
            if (!page_div.children.length)
                render('Impossible de générer la page à cause d\'une erreur', page_div);
            page_div.classList.add('disabled');

            console.error(err);
        }

        let show_menu = (profile.lock == null && (route.menu.chain.length > 2 || route.menu.chain[0].children.length > 1));
        let menu_is_wide = (show_menu && route.menu.chain[0].children.length > 3);

        // Quick access to page sections
        let page_sections = model.widgets.filter(intf => intf.options.anchor).map(intf => ({
            title: intf.label,
            anchor: intf.options.anchor
        }));

        return html`
            <div class="print" @scroll=${syncEditorScroll}}>
                <div id="ins_page">
                    <div id="ins_menu">${show_menu ? util.mapRange(1 - menu_is_wide, route.menu.chain.length,
                                                                   idx => renderPageMenu(route.menu.chain[idx])) : ''}</div>

                    <form id="ins_form" autocomplete="off" @submit=${e => e.preventDefault()}>
                        ${page_div}
                    </form>

                    <div id="ins_actions">
                        ${model.renderActions()}

                        ${page_sections.length > 1 ? html`
                            <h1>${route.page.title}</h1>
                            <ul>${page_sections.map(section => html`<li><a href=${'#' + section.anchor}>${section.title}</a></li>`)}</ul>
                        ` : ''}
                    </div>
                </div>
                <div style="flex: 1;"></div>

                ${model.actions.length ? html`
                    <nav class="ui_toolbar" id="ins_tasks" style="z-index: 999999;">
                        <div style="flex: 1;"></div>

                        ${model.actions.some(action => !action.options.always) ? html`
                            <div class="drop up right">
                                <button @click=${ui.deployMenu}>Actions</button>
                                <div>
                                    ${model.actions.map(action => action.render())}
                                </div>
                            </div>
                            <hr/>
                        ` : ''}
                        ${util.mapRange(0, model.actions.length, idx => {
                            let action = model.actions[model.actions.length - idx - 1];

                            if (action.label.match(/^\-+$/))
                                return '';
                            if (!action.options.always)
                                return '';

                            return action.render();
                        })}
                    </nav>
                ` : ''}
            </div>
        `;
    }

    function renderPageMenu(menu) {
        if (!menu.children.length)
            return '';

        return html`
            <h1>${menu.title}</h1>
            <ul>
                ${util.map(menu.children, item => {
                    let active = route.menu.chain.includes(item);

                    return html`
                        <li><a class=${active ? 'active' : ''} href=${item.url}>
                            <div style="flex: 1;">${item.title}</div>
                        </a></li>
                    `;
                })}
            </ul>
        `;
    }

    async function runUserCode(title, code, args) {
        let entry = error_entries[title];
        if (entry == null) {
            entry = new log.Entry;
            error_entries[title] = entry;
        }

        try {
            let AsyncFunction = Object.getPrototypeOf(async function(){}).constructor;

            let func = new AsyncFunction(...Object.keys(args), code);
            await func(...Object.values(args));

            entry.close();
        } catch (err) {
            let line = util.parseEvalErrorLine(err);
            let msg = `Erreur sur ${title}\n${line != null ? `Ligne ${line} : ` : ''}${err.message}`;

            entry.error(msg, -1);
            throw new Error(msg);
        }
    }

    async function runTrailDialog(e, tid) {
        let fragments = [];

        return ui.runDialog(e, 'Journal des modifications', {}, (d, resolve, reject) => {
            d.output(html`
                <table class="ui_table">
                    <colgroup>
                        <col/>
                        <col/>
                        <col style="min-width: 12em;"/>
                    </colgroup>

                    <tbody>
                        <tr class=${route.version == null ? 'active' : ''}>
                            <td><a href=${route.page.url + `/${route.ulid}@`}>🔍\uFE0E</a></td>
                            <td colspan="2">Version actuelle</td>
                        </tr>

                        ${util.mapRange(0, fragments.length, idx => {
                            let version = fragments.length - idx;
                            let frag = fragments[version - 1];

                            let page = app.pages.get(frag.page) || route.page;
                            let url = page.url + `/${route.ulid}@${version}`;

                            return html`
                                <tr class=${version === route.version ? 'active' : ''}>
                                    <td><a href=${url}>🔍\uFE0E</a></td>
                                    <td>${frag.user}</td>
                                    <td>${frag.mtime.toLocaleString()}</td>
                                </tr>
                            `;
                        })}
                    </tbody>
                </table>
            `);
        });
    }

    async function syncEditor() {
        if (editor_el == null) {
            if (typeof ace === 'undefined')
                await net.loadScript(`${ENV.urls.static}ace/ace.js`);

            editor_el = document.createElement('div');
            editor_el.setAttribute('style', 'flex: 1;');
            editor_ace = ace.edit(editor_el);

            editor_ace.setShowPrintMargin(false);
            editor_ace.setFontSize(13);
            editor_ace.setBehavioursEnabled(false);
            editor_ace.setOptions({
                scrollPastEnd: 1
            });
        }

        let buffer = code_buffers.get(editor_filename);

        if (buffer == null) {
            await fetchCode(editor_filename);
            buffer = code_buffers.get(editor_filename);
        }

        if (buffer.session == null) {
            let session = new ace.EditSession('', 'ace/mode/javascript');

            session.setOption('useWorker', false);
            session.setUseWrapMode(true);
            session.doc.setValue(buffer.code);
            session.setUndoManager(new ace.UndoManager());
            session.on('change', e => handleFileChange(editor_filename));

            session.on('changeScrollTop', () => {
                if (ui.isPanelActive('view'))
                    setTimeout(syncFormScroll, 0);
            });
            session.selection.on('changeSelection', () => {
                syncFormHighlight(true);
                ignore_editor_scroll = performance.now();
            });

            buffer.session = session;
        }

        if (editor_filename === 'main.js') {
            editor_ace.setTheme('ace/theme/monokai');
        } else {
            editor_ace.setTheme('ace/theme/merbivore_soft');
        }

        editor_ace.setSession(buffer.session);
    }

    function toggleEditorFile(e, filename) {
        editor_filename = filename;
        return togglePanels(true, null);
    }

    async function handleFileChange(filename) {
        if (ignore_editor_change)
            return;

        let buffer = code_buffers.get(filename);
        let code = buffer.session.doc.getValue();

        // Should never fail, but who knows..
        if (buffer != null) {
            let key = `${profile.userid}:${filename}`;
            let blob = new Blob([code]);
            let sha256 = await goupile.computeSha256(blob);

            await dev_db.saveWithKey('fs_changes', key, {
                filename: filename,
                size: blob.size,
                sha256: sha256,
                blob: blob
            });

            buffer.code = code;
            buffer.sha256 = sha256;

            if (code_timer != null)
                clearTimeout(code_timer);
            code_timer = setTimeout(uploadFsChanges, 3000);
        }

        self.run();
    }

    async function uploadFsChanges() {
        await mutex.run(async () => {
            let progress = log.progress('Envoi des modifications');

            try {
                let range = IDBKeyRange.bound(profile.userid + ':', profile.userid + '`', false, true);
                let changes = await dev_db.loadAll('fs_changes', range);

                for (let file of changes) {
                    let url = util.pasteURL(`${ENV.urls.base}files/${file.filename}`, { sha256: file.sha256 });

                    let response = await net.fetch(url, {
                        method: 'PUT',
                        body: file.blob,
                        timeout: null
                    });
                    if (!response.ok && response.status !== 409) {
                        let err = await net.readError(response);
                        throw new Error(err)
                    }

                    let key = `${profile.userid}:${file.filename}`;
                    await dev_db.delete('fs_changes', key);
                }

                progress.close();
            } catch (err) {
                progress.close();
                log.error(err);
            }

            if (code_timer != null)
                clearTimeout(code_timer);
            code_timer = null;
        });
    }

    function syncFormScroll() {
        if (!ui.isPanelActive('editor') || !ui.isPanelActive('view'))
            return;
        if (performance.now() - ignore_editor_scroll < 500)
            return;
        if (!editor_ace.isFocused())
            return;

        try {
            let panel_el = document.querySelector('#ins_page').parentNode;
            let widget_els = panel_el.querySelectorAll('*[data-line]');

            let editor_line = editor_ace.getFirstVisibleRow() + 1;

            let prev_line;
            for (let i = 0; i < widget_els.length; i++) {
                let line = parseInt(widget_els[i].dataset.line, 10);

                if (line >= editor_line) {
                    if (!i) {
                        ignore_page_scroll = performance.now();
                        panel_el.scrollTop = 0;
                    } else if (i === widget_els.length - 1) {
                        let top = computeRelativeTop(panel_el, widget_els[i]);

                        ignore_page_scroll = performance.now();
                        panel_el.scrollTop = top;
                    } else {
                        let top1 = computeRelativeTop(panel_el, widget_els[i - 1]);
                        let top2 = computeRelativeTop(panel_el, widget_els[i]);
                        let frac = (editor_line - prev_line) / (line - prev_line);

                        ignore_page_scroll = performance.now();
                        panel_el.scrollTop = top1 + frac * (top2 - top1);
                    }

                    break;
                }

                prev_line = line;
            }
        } catch (err) {
            // Meh, don't wreck the editor if for some reason we can't sync the
            // two views, this is not serious so just log it.
            console.log(err);
        }
    }

    function computeRelativeTop(parent, el) {
        let top = 0;
        while (el !== parent) {
            top += el.offsetTop;
            el = el.offsetParent;
        }
        return top;
    }

    function syncFormHighlight(scroll) {
        if (!ui.isPanelActive('view'))
            return;

        try {
            let panel_el = document.querySelector('#ins_page').parentNode;
            let widget_els = panel_el.querySelectorAll('*[data-line]');

            if (ui.isPanelActive('editor') && widget_els.length) {
                let selection = editor_ace.session.selection;
                let editor_lines = [
                    selection.getRange().start.row + 1,
                    selection.getRange().end.row + 1
                ];

                let highlight_first;
                let highlight_last;
                for (let i = 0;; i++) {
                    let line = parseInt(widget_els[i].dataset.line, 10);

                    if (line > editor_lines[0]) {
                        if (i > 0)
                            highlight_first = i - 1;
                        break;
                    }

                    if (i >= widget_els.length - 1) {
                        highlight_first = i;
                        break;
                    }
                }
                if (highlight_first != null) {
                    highlight_last = highlight_first;

                    while (highlight_last < widget_els.length) {
                        let line = parseInt(widget_els[highlight_last].dataset.line, 10);
                        if (line > editor_lines[1])
                            break;
                        highlight_last++;
                    }
                    highlight_last--;
                }

                for (let i = 0; i < widget_els.length; i++)
                    widget_els[i].classList.toggle('ins_highlight', i >= highlight_first && i <= highlight_last);

                // Make sure widget is in viewport
                if (scroll && highlight_first != null &&
                              highlight_last === highlight_first) {
                    let el = widget_els[highlight_first];
                    let rect = el.getBoundingClientRect();

                    if (rect.top < 0) {
                        ignore_page_scroll = performance.now();
                        panel_el.scrollTop += rect.top - 50;
                    } else if (rect.bottom >= window.innerHeight) {
                        ignore_page_scroll = performance.now();
                        panel_el.scrollTop += rect.bottom - window.innerHeight + 30;
                    }
                }
            } else {
                for (let el of widget_els)
                    el.classList.remove('ins_highlight');
            }
        } catch (err) {
            // Meh, don't wreck the editor if for some reason we can't sync the
            // two views, this is not serious so just log it.
            console.log(err);
        }
    }

    function syncEditorScroll() {
        if (!ui.isPanelActive('editor') || !ui.isPanelActive('view'))
            return;
        if (performance.now() - ignore_page_scroll < 500)
            return;

        try {
            let panel_el = document.querySelector('#ins_page').parentNode;
            let widget_els = panel_el.querySelectorAll('*[data-line]');

            let prev_top;
            let prev_line;
            for (let i = 0; i < widget_els.length; i++) {
                let el = widget_els[i];

                let top = el.getBoundingClientRect().top;
                let line = parseInt(el.dataset.line, 10);

                if (top >= 0) {
                    if (!i) {
                        ignore_editor_scroll = performance.now();
                        editor_ace.renderer.scrollToLine(0);
                    } else {
                        let frac = -prev_top / (top - prev_top);
                        let line2 = Math.floor(prev_line + frac * (line - prev_line));

                        ignore_editor_scroll = performance.now();
                        editor_ace.renderer.scrollToLine(line2);
                    }

                    break;
                }

                prev_top = top;
                prev_line = line;
            }
        } catch (err) {
            // Meh, don't wreck anything if for some reason we can't sync the
            // two views, this is not serious so just log it.
            console.log(err);
        }
    }

    async function runPublishDialog(e) {
        await uploadFsChanges();

        let publisher = new InstancePublisher(self, dev_db);
        await publisher.runDialog(e);

        self.run();
    }

    this.go = async function(e, url = null, options = {}) {
        options = Object.assign({ push_history: true }, options);

        let new_route = Object.assign({}, route);
        let explicit_panels = false;

        // Parse new URL
        if (url != null) {
            if (!(url instanceof URL))
                url = new URL(url, window.location.href);
            if (url.pathname === ENV.urls.instance)
                url = new URL(app.homepage.url, window.location.href);
            goupile.setCurrentHash(url.hash);

            if (!url.pathname.endsWith('/'))
                url.pathname += '/';

            // Goodbye!
            if (!url.pathname.startsWith(ENV.urls.instance)) {
                if (self.hasUnsavedData())
                    await goupile.confirmDangerousAction(e);

                window.onbeforeunload = null;
                window.location.href = url.href;

                return;
            }

            let path = url.pathname.substr(ENV.urls.instance.length);
            let [key, what] = path.split('/').map(str => str.trim());

            // Follow lock sequence
            if (profile.lock != null) {
                if (!profile.lock.pages.includes(key))
                    key = profile.lock.pages[0];
            }

            // Find page information
            new_route.page = app.pages.get(key);
            if (new_route.page == null) {
                log.error(`La page '${key}' n'existe pas`);
                new_route.page = app.homepage;
            }
            new_route.menu = new_route.page.menu;

            // Restore explicit panels (if any)
            let panels = url.searchParams.get('p');
            if (panels) {
                panels = panels.split('|');
                panels = panels.filter(key => app.panels[key]);

                if (panels.length)
                    ui.restorePanels(panels);

                explicit_panels = true;
            }
        }

        // Confirm dangerous actions
        if (self.hasUnsavedData() && new_route.page != route.page)
            await ui.runConfirm(e, 'Si vous continuer vous allez perdre les modifications non enregistrées, voulez-vous continuer ?', 'Oublier');

        // Fetch and cache page code for page panel
        {
            let filename = new_route.page.filename;
            await fetchCode(filename);
        }

        // Commit!
        updateContext(new_route);

        await mutex.chain(() => self.run(options.push_history));
    };
    this.go = util.serialize(this.go, mutex);

    this.run = async function(push_history = true) {
        // Fetch and cache page code for page panel
        // Again to make sure we are up to date (e.g. publication)
        let filename = route.page.filename;
        await fetchCode(filename);

        // Sync editor (if needed)
        if (ui.isPanelActive('editor')) {
            if (editor_filename !== 'main.js')
                editor_filename = filename;

            await syncEditor();
        }

        // Update URL and title
        {
            let url = route.page.url;

            let panels;
            if (profile.develop) {
                panels = ui.getActivePanels().join('|');
            } else if (app.panels.data + app.panels.view < 2) {
                panels = null;
            } else if (url.match(/\/[A-Z0-9]{26}(@[0-9]+)?$/)) {
                panels = ui.getActivePanels().join('|');

                if (panels === 'view')
                    panels = null;
            } else {
                panels = ui.getActivePanels().join('|');

                if (panels === 'data')
                    panels = null;
            }

            url = util.pasteURL(url, { p: panels });
            goupile.syncHistory(url, push_history);

            document.title = `${route.page.title} — ${ENV.title}`;
        }

        // Don't mess with the editor when render accidently triggers a scroll event!
        ignore_page_scroll = performance.now();
        ui.render();
    };
    this.run = util.serialize(this.run, mutex);

    function updateContext(new_route) {
        if (new_route.page != route.page) {
            form_state = new FormState();
            form_state.changeHandler = handleStateChange;
        }

        route = new_route;
    }

    async function handleStateChange() {
        await self.run();

        // Highlight might need to change (conditions, etc.)
        if (ui.isPanelActive('editor'))
            syncFormHighlight(false);
    }

    async function fetchCode(filename, version = null) {
        let code = null;

        if (profile.develop && version == null) {
            // Anything in cache or in the editor?
            {
                let buffer = code_buffers.get(filename);

                if (buffer != null)
                    return buffer.code;
            }

            // Try locally saved files
            if (code == null) {
                let key = `${profile.userid}:${filename}`;
                let file = await dev_db.load('fs_changes', key);

                if (file != null) {
                    if (file.blob != null) {
                        code = await file.blob.text();
                    } else {
                        code = '';
                    }
                }
            }
        }

        // The server is our last hope
        if (code == null) {
            let url = (version != null) ? `${ENV.urls.base}files/${version}/${filename}` : `${ENV.urls.files}${filename}`;
            let response = await net.fetch(url);

            if (response.ok) {
                code = await response.text();
            } else {
                code = '';
            }
        }

        // Create or update buffer
        {
            let buffer = code_buffers.get(filename);

            if (buffer == null) {
                let sha256 = Sha256(code);

                buffer = {
                    code: code,
                    sha256: sha256,
                    orig_sha256: sha256,
                    session: null,
                    version: version || 0
                };
                code_buffers.set(filename, buffer);
            } else {
                let sha256 = Sha256(code);

                if (buffer.session != null && sha256 !== buffer.sha256) {
                    ignore_editor_change = true;
                    buffer.session.doc.setValue(code);
                    ignore_editor_change = false;
                }

                buffer.code = code;
                buffer.sha256 = sha256;
                buffer.version = version || 0;
            }
        }

        return code;
    }

    function fileHasChanged(filename) {
        let buffer = code_buffers.get(filename);

        if (buffer != null) {
            let changed = (buffer.sha256 !== buffer.orig_sha256);
            return changed;
        } else {
            return false;
        }
    }
};
