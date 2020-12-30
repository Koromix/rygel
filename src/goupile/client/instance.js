// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function InstanceController() {
    let self = this;

    this.init = async function() {
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

        ui.createPanel('editor', () => html`<div class="ins_panel">Editeur</div>`);
        ui.createPanel('form', () => html`<div class="ins_panel">Formulaire</div>`);
    };

    this.run = async function() {
        await goupile.syncProfile();

        if (!goupile.isAuthorized()) {
            goupile.runLogin();
            return;
        }

        ui.render();
    };
};
