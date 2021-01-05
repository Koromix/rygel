// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function InstanceController() {
    let self = this;

    let app;

    let page_key;
    let page_code;
    let page_state;

    let editor_el;
    let editor_ace;
    let editor_filename;
    let editor_sessions = new LruMap(32);

    let code_cache = new LruMap(4);
    let error_entries = {};

    this.start = async function() {
        initUI();
        await initApp();

        self.go(window.location.href);
    };

    async function initApp() {
        let code = await fetchCode('main.js');

        try {
            let new_app = new ApplicationInfo;
            let builder = new ApplicationBuilder(new_app);

            runUserCode('Application', code, {
                app: builder
            });
            if (new_app.pages.size)
                new_app.home = new_app.pages.values().next().value.key;

            app = new_app;
        } catch (err) {
            if (app == null)
                app = new ApplicationInfo;
        }
    }

    function initUI() {
        document.documentElement.className = 'instance';

        ui.setMenu(() => html`
            <button class="icon" style="background-position-y: calc(-538px + 1.2em);"
                    @click=${e => self.go(ENV.base_url)}>${ENV.title}</button>
            <button class=${'icon' + (ui.isPanelEnabled('editor') ? ' active' : '')}
                    style="background-position-y: calc(-274px + 1.2em);"
                    @click=${e => togglePanel('editor')}>Éditeur</button>
            <button class=${'icon' + (ui.isPanelEnabled('form') ? ' active' : '')}
                    style="background-position-y: calc(-318px + 1.2em);"
                    @click=${e => togglePanel('form')}>Formulaire</button>
            <div style="flex: 1;"></div>
            <div class="drop right">
                <button>${profile.username}</button>
                <div>
                    <button @click=${ui.wrapAction(goupile.logout)}>Se déconnecter</button>
                </div>
            </div>
        `);

        ui.createPanel('editor', false, () => {
            let tabs = [];

            tabs.push({
                title: 'Application',
                filename: 'main.js'
            });
            if (page_key != null) {
                tabs.push({
                    title: 'Page',
                    filename: getPageFileName(page_key)
                });
            }

            return html`
                <div class="ins_panel" style="--menu_color: #383936;">
                    <div class="ui_toolbar">
                        ${tabs.map(tab => html`<button class=${editor_filename === tab.filename ? 'active' : ''}
                                                       @click=${ui.wrapAction(e => toggleEditorFile(tab.filename))}>${tab.title}</button>`)}
                    </div>

                    ${editor_el}
                </div>
            `;
        });

        ui.createPanel('form', true, () => {
            let model = new FormModel;
            let builder = new FormBuilder(page_state, model);

            try {
                runUserCode('Page', page_code, {
                    form: builder
                });

                return html`
                    <div class="ins_panel">
                        <form id="ins_form">
                            ${model.render()}
                        </form>
                    </div>
                `;
            } catch (err) {
                return html`
                    <div class="ins_panel">
                        <span class="gp_wip">${err.message}</span>
                    </div>
                `;
            }
        });
    };

    function togglePanel(key, enable = null) {
        ui.setPanelState(key, !ui.isPanelEnabled(key));
        self.go();
    }

    function runUserCode(title, code, arguments) {
        let entry = error_entries[title];
        if (entry == null) {
            entry = new log.Entry;
            error_entries[title] = entry;
        }

        try {
            let func = new Function(...Object.keys(arguments), code);
            func(...Object.values(arguments));

            entry.close();
        } catch (err) {
            let line = util.parseEvalErrorLine(err);
            let msg = `Erreur sur ${title}\n${line != null ? `Ligne ${line} : ` : ''}${err.message}`;

            entry.error(msg, -1);
            throw new Error(msg);
        }
    }

    async function syncEditor() {
        if (editor_el == null) {
            if (typeof ace === 'undefined')
                await net.loadScript(`${ENV.base_url}static/ace.js`);

            editor_el = document.createElement('div');
            editor_el.setAttribute('style', 'flex: 1;');
            editor_ace = ace.edit(editor_el);

            editor_ace.setTheme('ace/theme/merbivore_soft');
            editor_ace.setShowPrintMargin(false);
            editor_ace.setFontSize(13);
            editor_ace.setBehavioursEnabled(false);
        }

        let session = editor_sessions.get(editor_filename);
        if (session == null) {
            let code = await fetchCode(editor_filename);

            session = new ace.EditSession('', 'ace/mode/javascript');
            session.setOption('useWorker', false);
            session.setUndoManager(new ace.UndoManager());
            session.setUseWrapMode(true);
            session.doc.setValue(code);
            session.on('change', e => self.go());

            editor_sessions.set(editor_filename, session);
        }
        editor_ace.setSession(session);
    }

    function toggleEditorFile(filename) {
        editor_filename = filename;
        self.go();
    }

    this.go = async function(url = null, push_history = true) {
        await goupile.syncProfile();
        if (!goupile.isAuthorized())
            await goupile.runLogin();

        // Find page
        if (url != null) {
            if (!url.endsWith('/'))
                url += '/';
            url = new URL(url, window.location.href);

            if (url.pathname === ENV.base_url) {
                page_key = app.home;
            } else {
                let prefix = `${ENV.base_url}main/`;

                if (url.pathname.startsWith(prefix)) {
                    let path = url.pathname.substr(prefix.length);
                    let [key, id] = path.split('/');

                    page_key = key;
                } else {
                    window.location.href = url.href;
                }
            }
        }

        // Fetch page code and sync state
        if (page_key != null && !app.pages.has(page_key)) {
            log.error(`La page ${page_key} n'existe pas`);
            page_key = app.home;
        }
        if (page_key != null) {
            let url = makePageURL(page_key);
            let filename = getPageFileName(page_key);

            goupile.syncHistory(url, push_history);

            page_code = await fetchCode(filename);
        } else {
            goupile.syncHistory(ENV.base_url, push_history);
            page_code = null;
        }
        if (page_state == null) {
            page_state = new FormState;
            page_state.changeHandler = () => self.go();
        }

        // Sync editor (if needed)
        if (ui.isPanelEnabled('editor')) {
            if (editor_filename == null || editor_filename.startsWith('pages/'))
                editor_filename = (page_key != null) ? getPageFileName(page_key) : 'main.js';

            await syncEditor();
        }

        ui.render();
    };
    this.go = util.serializeAsync(this.go);

    async function fetchCode(filename) {
        let session = editor_sessions.get(filename);
        if (session != null)
            return session.doc.getValue();

        let code = code_cache.get(filename);
        if (code == null) {
            let response = await fetch(`${ENV.base_url}files/${filename}`);
            code = await response.text();

            code_cache.set(filename, code);
        }

        return code;
    }

    function getPageFileName(key) { return `pages/${key}.js`; };
    function makePageURL(key) { return `${ENV.base_url}main/${key}`; }
};
