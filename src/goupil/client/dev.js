// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let dev = (function() {
    let self = this;

    let init = false;

    let assets = [];

    let current_key;
    let current_asset;

    this.init = async function() {
        assets = await g_assets.list();
    };

    this.go = async function(key, args = {}) {
        // Select relevant asset
        if (!key) {
            if (current_key) {
                key = current_key;
            } else {
                key = assets.length ? assets[0].key : null;
            }
        }
        if (key !== current_key) {
            if (key) {
                let asset = await g_assets.load(key);

                current_key = key;
                current_asset = asset;
            } else {
                current_key = null;
                current_asset = null;
            }
        }

        // Update toolbar
        renderMenu();

        // Run appropriate module
        if (current_asset) {
            document.title = `${current_asset.key} — ${env.project_key}`;

            if (typeof current_asset.data === 'string') {
                dev_form.run(current_asset, args);
            } else if (current_asset.data instanceof Blob) {
                renderEmpty();
            } else {
                renderEmpty();
                log.error(`Unknown asset type for '${current_asset.key}'`);
            }
        } else {
            document.title = env.project_key;

            renderEmpty();
            log.error('No asset available');
        }
    };

    function renderMenu() {
        let menu_el = document.querySelector('#gp_menu');

        render(html`
            <div id="dev_modes"></div>

            <select id="dev_pages" @change=${e => self.go(e.target.value)}>
                ${!current_key && !assets.length ? html`<option>-- No asset available --</option>` : ''}
                ${current_key && !current_asset ?
                    html`<option value=${current_key} .selected=${true}>-- Unknown asset '${current_key}' --</option>` : ''}
                ${assets.map(it =>
                    html`<option value=${it.key} .selected=${it.key == current_key}>${it.key}</option>`)}
            </select>
            <button @click=${showCreateDialog}>Ajouter</button>
            ${current_asset ?
                html`<button @click=${e => showDeleteDialog(e, current_asset)}>Supprimer</button>` : ''}
            <button @click=${showResetDialog}>Réinitialiser</button>
        `, menu_el);
    }

    function showCreateDialog(e) {
        popup.form(e, page => {
            let type = page.choice('type', 'Type :', [['file', 'Fichier'], ['page', 'Page']],
                                   {mandatory: true, untoggle: false, value: 'file'});

            let file;
            let key;
            switch (type.value) {
                case 'page': {
                    key = page.text('key', 'Clé :', {mandatory: true});
                } break;
                case 'file': {
                    file = page.file('file', 'Fichier :', {mandatory: true});
                    key = page.text('key', 'Clé :', {placeholder: file.value ? file.value.name : null});
                    if (!key.value && file.value)
                        key.value = file.value.name;
                } break;
            }

            if (key.value) {
                if (assets.some(asset => asset.key === key.value))
                    key.error('Existe déjà');
                if (!key.value.match(/^[a-zA-Z_\.][a-zA-Z0-9_\.]*$/))
                    key.error('Autorisé : a-z, _, . et 0-9 (sauf initiale)');
            }

            page.submitHandler = async () => {
                let asset;
                switch (type.value) {
                    case 'page': { asset = g_assets.createPage(key.value, ''); } break;
                    case 'file': { asset = g_assets.createBlob(key.value, file.value); } break;
                }

                await g_assets.save(asset);
                delete asset.data;

                assets.push(asset);
                assets.sort();

                page.close();
                self.go(asset.key);
            };
            page.buttons(page.buttons.std.ok_cancel('Créer'));
        });
    }

    function showDeleteDialog(e, asset) {
        popup.form(e, page => {
            page.output(`Voulez-vous vraiment supprimer la ressource '${asset.key}' ?`);

            page.submitHandler = async () => {
                await g_assets.delete(asset.key);

                // Remove from assets array and map
                let asset_idx = assets.findIndex(it => it.key === asset.key);
                assets.splice(asset_idx, 1);

                if (current_key === asset.key) {
                    current_key = null;
                    current_asset = null;
                }

                page.close();
                self.go();
            };
            page.buttons(page.buttons.std.ok_cancel('Supprimer'));
        });
    }

    function showResetDialog(e) {
        popup.form(e, page => {
            page.output('Voulez-vous vraiment réinitialiser toutes les ressources ?');

            page.submitHandler = async () => {
                await g_assets.reset();

                init = false;
                current_key = null;
                current_asset = null;

                page.close();

                await self.init();
                self.go();
            };
            page.buttons(page.buttons.std.ok_cancel('Réinitialiser'));
        });
    }

    function renderEmpty() {
        let modes_el = document.querySelector('#dev_modes');
        let main_el = document.querySelector('main');

        render('', modes_el);
        render('', main_el);
    }

    return this;
}).call({});
