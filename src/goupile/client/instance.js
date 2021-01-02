// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function InstanceController() {
    let self = this;

    let editor_el;
    let editor;

    let state = new FormState;

    this.init = async function() {
        initUI();
    };

    function initUI() {
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

        ui.createPanel('editor', syncEditor);
        ui.createPanel('form', () => {
            let script = (editor != null) ? editor.getValue() : '';
            let func = new Function('form', script);

            let model = new FormModel;
            let builder = new FormBuilder(state, model);

            func(builder);

            return html`<div class="ins_panel">${model.render()}</div>`
        });
    };

    function syncEditor() {
        if (editor_el == null) {
            if (typeof ace === 'undefined')
                return net.loadScript(`${ENV.base_url}static/ace.js`).then(syncEditor);

            editor_el = document.createElement('div');
            editor_el.style.height = '100%';
            editor = ace.edit(editor_el);

            editor.setTheme('ace/theme/merbivore_soft');
            editor.setShowPrintMargin(false);
            editor.setFontSize(13);
            editor.setBehavioursEnabled(false);

            let session = new ace.EditSession('', 'ace/mode/javascript');
            session.setOption('useWorker', false);
            session.on('change', e => self.run());

            editor.setSession(session);
        }

        return html`<div>${editor_el}</div>`;
    }

    this.run = async function() {
        await goupile.syncProfile();

        if (!goupile.isAuthorized()) {
            goupile.runLogin();
            return;
        }

        ui.render();
    };
};
