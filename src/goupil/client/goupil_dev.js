// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let goupil_dev = (function() {
    let self = this;

    let init = false;

    let assets = [];

    let current_path;
    let current_asset;

    this.init = async function() {
        assets = await g_assets.list();

        // Load default assets if main script is missing
        if (!assets.find(it => it.path === 'main.js')) {
            await resetAssets();
            assets = await g_assets.list();
        }


        current_path = null;
        current_asset = null;
    };

    async function resetAssets() {
        await g_assets.transaction(m => {
            m.clear();

            for (let path in help_demo.assets) {
                let data = help_demo.assets[path];
                let asset = g_assets.create(path, data);

                m.save(asset);
            }
        });
    }

    this.go = async function(url = null, args = {}) {
        // Parse URL (kind of)
        let path;
        if (url && url !== '/') {
            path = url;
        } else if (current_path) {
            path = current_path;
        } else if (assets.length) {
            path = assets[0].path;
        }

        // Find relevant asset
        if (path !== current_path) {
            if (path) {
                let asset = await g_assets.load(path);

                current_path = path;
                current_asset = asset;
            } else {
                current_path = null;
                current_asset = null;
            }
        }

        // Update toolbar
        renderMenu();

        // Run appropriate module
        if (current_asset) {
            document.title = `${current_asset.path} — ${env.project_key}`;

            if (typeof current_asset.data === 'string') {
                dev_form.run(current_asset, args);
            } else if (current_asset.data instanceof Blob) {
                renderEmpty();
            } else {
                renderEmpty();
                log.error(`Unknown asset type for '${current_asset.path}'`);
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
                ${!current_path && !assets.length ? html`<option>-- No asset available --</option>` : ''}
                ${current_path && !current_asset ?
                    html`<option value=${current_path} .selected=${true}>-- Unknown asset '${current_path}' --</option>` : ''}
                ${assets.map(it =>
                    html`<option value=${it.path} .selected=${it.path == current_path}>${it.path}</option>`)}
            </select>
            <button @click=${showCreateDialog}>Ajouter</button>
            ${current_asset ?
                html`<button ?disabled=${current_path === 'main.js'}
                             @click=${e => showDeleteDialog(e, current_asset)}>Supprimer</button>` : ''}
            <button @click=${showResetDialog}>Réinitialiser</button>
        `, menu_el);
    }

    function showCreateDialog(e) {
        popup.form(e, page => {
            let type = page.choice('type', 'Type :', [['file', 'Fichier'], ['page', 'Page']],
                                   {mandatory: true, untoggle: false, value: 'file'});

            let file;
            let key;
            let path;
            switch (type.value) {
                case 'page': {
                    key = page.text('key', 'Clé :', {mandatory: true});
                    if (key.value)
                        path = goupil.makePagePath(key.value);
                } break;
                case 'file': {
                    file = page.file('file', 'Fichier :', {mandatory: true});
                    key = page.text('key', 'Clé :', {placeholder: file.value ? file.value.name : null});
                    if (!key.value && file.value)
                        key.value = file.value.name;
                    if (key.value)
                        path = goupil.makeBlobPath(key.value);
                } break;
            }

            if (path) {
                if (assets.some(asset => asset.path === path))
                    key.error('Cette ressource existe déjà');
                if (!key.value.match(/^[a-zA-Z_\.][a-zA-Z0-9_\.]*$/))
                    key.error('Autorisé : a-z, _, . et 0-9 (sauf initiale)');
            }

            page.submitHandler = async () => {
                let asset;
                switch (type.value) {
                    case 'page': { asset = g_assets.create(path, ''); } break;
                    case 'file': { asset = g_assets.create(path, file.value); } break;
                }

                await g_assets.save(asset);
                delete asset.data;

                assets.push(asset);
                assets.sort();

                page.close();
                self.go(asset.path);
            };
            page.buttons(page.buttons.std.ok_cancel('Créer'));
        });
    }

    function showDeleteDialog(e, asset) {
        popup.form(e, page => {
            page.output(`Voulez-vous vraiment supprimer la ressource '${asset.path}' ?`);

            page.submitHandler = async () => {
                await g_assets.delete(asset.path);

                // Remove from assets array and map
                let asset_idx = assets.findIndex(it => it.path === asset.path);
                assets.splice(asset_idx, 1);

                if (current_path === asset.path) {
                    current_path = null;
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
                await g_assets.clear();
                await self.init();

                page.close();
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
