// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function InstanceController() {
    let self = this;

    let app;

    let route_key;

    let editor_el;
    let editor_ace;
    let editor_filename;
    let editor_sessions = new LruMap(32);

    let page_state;

    this.start = async function() {
        initUI();
        await initApp();

        self.go(window.location.href);
    };

    async function initApp() {
        let response = await fetch(`${ENV.base_url}files/main.js`);
        let code = await response.text();

        try {
            let new_app = new ApplicationInfo;
            let builder = new ApplicationBuilder(new_app);

            runUserCode(code, {
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
            <div class="drop">
                <button class="icon" style="background-position-y: calc(-538px + 1.2em);">${ENV.title}</button>
                <div>
                    <button>A</button>
                    <button>B</button>
                </div>
            </div>
            <button class=${'icon' + (ui.isPanelEnabled('editor') ? ' active' : '')}
                    style="background-position-y: calc(-274px + 1.2em);"
                    @click=${e => togglePanel('editor')}>Éditeur</button>
            <button class=${'icon' + (ui.isPanelEnabled('form') ? ' active' : '')}
                    style="background-position-y: calc(-318px + 1.2em);"
                    @click=${e => togglePanel('form')}>Formulaire</button>
            <div style="flex: 1;"></div>
            <button @click=${ui.wrapAction(handleLogout)}>Se déconnecter</button>
        `);

        ui.createPanel('editor', () => {
            let tabs = [];

            tabs.push({
                title: 'Application',
                filename: 'main.js'
            });
            if (route_key != null) {
                tabs.push({
                    title: 'Page',
                    filename: getPageFileName(route_key)
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

        ui.createPanel('form', () => {
            if (page_state == null) {
                page_state = new FormState;
                page_state.changeHandler = () => self.go();
            }

            let model = new FormModel;
            let builder = new FormBuilder(page_state, model);
            let code = (editor_ace != null) ? editor_ace.getValue() : '';

            try {
                runUserCode(code, {
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
                return 'NOP';
            }
        });
    };

    function togglePanel(key) {
        ui.setPanelState(key, !ui.isPanelEnabled(key));
        self.go();
    }

    async function handleLogout() {
        await goupile.logout();
        self.go();
    }

    function runUserCode(code, arguments) {
        try {
            let func = new Function(...Object.keys(arguments), code);
            func(...Object.values(arguments));
        } catch (err) {
            let line = util.parseEvalErrorLine(err);
            log.error(`${line != null ? `Ligne ${line} : ` : ''}${err.message}`);

            throw err;
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
            let response = await fetch(`${ENV.base_url}files/${editor_filename}`);
            let code = await response.text();

            session = new ace.EditSession('', 'ace/mode/javascript');
            session.setOption('useWorker', false);
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

    this.go = async function(url = null) {
        await goupile.syncProfile();
        if (!goupile.isAuthorized())
            await goupile.runLogin();

        if (url != null) {
            if (!url.endsWith('/'))
                url += '/';
            url = new URL(url, window.location.href);

            if (url.pathname === ENV.base_url) {
                route_key = app.home;
            } else {
                let prefix = `${ENV.base_url}main/`;

                if (url.pathname.startsWith(prefix)) {
                    let path = url.pathname.substr(prefix.length);
                    let [key, id] = path.split('/');

                    route_key = key;
                } else {
                    window.location.href = url.href;
                }
            }
        }

        if (route_key != null) {
            let page = app.pages.get(route_key);

            if (page == null) {
                log.error(`La page ${route_key} n'existe pas`);

                route_key = app.home;
                page = app.pages.get(route_key);
            }

            let url = makePageURL(route_key);
            goupile.syncHistory(url);
        } else {
            goupile.syncHistory(ENV.base_url);
        }

        if (ui.isPanelEnabled('editor')) {
            if (editor_filename == null)
                editor_filename = (route_key != null) ? getPageFileName(route_key) : 'main.js';
            await syncEditor();
        }

        ui.render();
    };
    this.go = util.serializeAsync(this.go);

    function getPageFileName(key) { return `pages/${key}.js`; };
    function makePageURL(key) { return `${ENV.base_url}main/${key}`; }
};
