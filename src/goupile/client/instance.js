// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function InstanceController() {
    let self = this;

    let app;

    let route_url;
    let route_key;

    let editor_el;
    let editor_ace;
    let editor_filename;
    let editor_sessions = new LruMap(32);

    let page_state;

    this.start = async function() {
        initUI();
        await initApp();

        self.run(window.location.href);
    };

    async function initApp() {
        let response = await fetch(`${ENV.base_url}files/main.js`);
        let code = await response.text();

        let func = new Function('app', code);

        let app2 = new ApplicationInfo;
        let builder = new ApplicationBuilder(app2);

        func(builder);
        if (app2.pages.size)
            app2.home = app2.pages.values().next().value.key;

        app = app2;
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
                    @click=${e => ui.togglePanel('editor')}>Éditeur</button>
            <button class=${'icon' + (ui.isPanelEnabled('form') ? ' active' : '')}
                    style="background-position-y: calc(-318px + 1.2em);"
                    @click=${e => ui.togglePanel('form')}>Formulaire</button>
            <div style="flex: 1;"></div>
            <button @click=${goupile.logout}>Se déconnecter</button>
        `);

        ui.createPanel('editor', () => {
            if (editor_filename == null) {
                let filename = (route_key != null) ? getPageFileName(route_key) : 'main.js';
                let p = toggleEditorFile(filename);

                return p.then(renderEditor);
            }

            return renderEditor();
        });

        ui.createPanel('form', () => {
            if (page_state == null) {
                page_state = new FormState;
                page_state.changeHandler = () => self.run();
            }

            let model = new FormModel;
            let builder = new FormBuilder(page_state, model);

            let script = (editor_ace != null) ? editor_ace.getValue() : '';

            let error;
            try {
                let func = new Function('form', script);
                func(builder);
            } catch (err) {
                console.log(err);
                error = err.message;
            }

            return html`
                <div class="ins_panel">
                    <form id="ins_form">
                        ${model.render()}
                    </form>
                    ${error != null ? html`${error}` : ''}
                </div>
            `;
        });
    };

    async function toggleEditorFile(filename) {
        // Lazy init
        if (editor_el == null) {
            if (typeof ace === 'undefined')
                return net.loadScript(`${ENV.base_url}static/ace.js`).then(() => toggleEditorFile(filename));

            editor_el = document.createElement('div');
            editor_el.setAttribute('style', 'flex: 1;');
            editor_ace = ace.edit(editor_el);

            editor_ace.setTheme('ace/theme/merbivore_soft');
            editor_ace.setShowPrintMargin(false);
            editor_ace.setFontSize(13);
            editor_ace.setBehavioursEnabled(false);
        }

        let session = editor_sessions.get(filename);

        if (session == null) {
            let response = await fetch(`${ENV.base_url}files/${filename}`);
            let code = await response.text();

            session = new ace.EditSession('', 'ace/mode/javascript');
            session.setOption('useWorker', false);
            session.setUseWrapMode(true);
            session.doc.setValue(code);
            session.on('change', e => self.run());

            editor_sessions.set(filename, session);
        }

        editor_ace.setSession(session);
        editor_filename = filename;
    }

    function renderEditor() {
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
                                                   @click=${ui.wrapClick(e => toggleEditorFile(tab.filename))}>${tab.title}</button>`)}
                </div>

                ${editor_el}
            </div>
        `;
    }

    this.run = async function(url = null) {
        await goupile.syncProfile();

        if (!goupile.isAuthorized())
            await goupile.runLogin();

        if (url != null) {
            if (!url.endsWith('/'))
                url += '/';
            url = (new URL(url, window.location.href)).pathname;

            if (url === ENV.base_url) {
                route_key = app.home;
            } else {
                let prefix = `${ENV.base_url}main/`;

                if (url.startsWith(prefix)) {
                    url = url.substr(prefix.length);
                    route_key = url.substr(0, url.indexOf('/'));
                } else {
                    // XXX: FUCK
                }
            }
        }

        if (route_key != null) {
            let page = app.pages.get(route_key);

            if (page == null) {
                log.error('fuck');

                route_key = app.home;
                page = app.pages.get(route_key);
            }

            let url = makePageURL(route_key);
            if (route_url == null) {
                window.history.replaceState(null, null, url);
            } else if (url !== route_url) {
                window.history.pushState(null, null, url);
            }
            route_url = url;
        } else {
            url = ENV.base_url;
            if (route_url == null) {
                window.history.replaceState(null, null, url);
            } else if (url !== route_url) {
                window.history.pushState(null, null, url);
            }
            route_url = url;
        }

        ui.render();
    };

    function getPageFileName(key) { return `pages/${key}.js`; };
    function makePageURL(key) { return `${ENV.base_url}main/${key}`; }
};
