// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let goupil_dev = new function() {
    let self = this;

    let init = false;

    let assets = [];
    let assets_map = {};

    let current_path;
    let current_asset;

    this.init = async function() {
        assets = await listAssets();

        // Load default assets if main script is missing
        if (!assets.find(asset => asset.path === 'main.js')) {
            await resetAssets();
            assets = await listAssets();
        }

        assets_map = {};
        for (let asset of assets)
            assets_map[asset.path] = asset;

        current_path = null;
        current_asset = null;
    };

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

        // Load it (if needed)
        if (path !== current_path) {
            current_asset = await loadAsset(path);
            current_path = path;
        }

        // Run appropriate module
        if (current_asset) {
            document.title = `${current_asset.path} — ${env.project_key}`;

            renderMenu();
            switch (current_asset.type) {
                case 'main':
                case 'page': { dev_form.run(current_asset, args); } break;
                case 'blob': { renderEmpty(); } break;

                default: {
                    renderEmpty();
                    log.error(`Unknown asset type for '${current_asset.path}'`);
                } break;
            }
        } else {
            document.title = env.project_key;

            renderMenu();
            renderEmpty();
            log.error(current_path ? `Asset '${current_path}' does not exist` : 'No asset available');
        }
    };

    async function listAssets() {
        let paths = await g_files.list();

        let assets = paths.map(path => {
            let type;
            if (path === 'main.js') {
                type = 'main';
            } else if (path.match(/^pages\/.*\.js$/)) {
                type = 'page';
            } else if (path.match(/^static\//)) {
                type = 'blob';
            }

            let asset = {
                path: path,
                type: type
            };

            return asset;
        });

        return assets;
    }

    async function loadAsset(path) {
        let asset = assets_map[path];
        if (!asset)
            return null;

        let file = await g_files.load(path);
        asset = Object.assign({}, asset, {data: file.data});

        return asset;
    }

    async function resetAssets() {
        await g_files.transaction(m => {
            m.clear();

            for (let path in dev_demo) {
                let data = dev_demo[path];
                let file = g_files.create(path, data);

                m.save(file);
            }
        });
    }

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
            let type = page.choice('type', 'Type :', [['blob', 'Fichier'], ['page', 'Page']],
                                   {mandatory: true, untoggle: false, value: 'blob'});

            let blob;
            let key;
            let path;
            switch (type.value) {
                case 'blob': {
                    blob = page.file('file', 'Fichier :', {mandatory: true});
                    key = page.text('key', 'Clé :', {placeholder: blob.value ? blob.value.name : null});
                    if (!key.value && blob.value)
                        key.value = blob.value.name;
                    if (key.value)
                        path = goupil.makeBlobPath(key.value);
                } break;

                case 'page': {
                    key = page.text('key', 'Clé :', {mandatory: true});
                    if (key.value)
                        path = goupil.makePagePath(key.value);
                } break;
            }

            if (path) {
                if (assets.some(asset => asset.path === path))
                    key.error('Cette ressource existe déjà');
                if (!key.value.match(/^[a-zA-Z_\.][a-zA-Z0-9_\.]*$/))
                    key.error('Autorisé : a-z, _, . et 0-9 (sauf initiale)');
            }

            page.submitHandler = async () => {
                switch (type.value) {
                    case 'blob': { g_files.save(g_files.create(path, blob.value)); } break;
                    case 'page': { g_files.save(g_files.create(path, '')); } break;
                }

                let asset = {
                    path: path,
                    type: type.value
                };
                assets.push(asset);
                assets.sort();
                assets_map[path] = asset;

                page.close();
                self.go(path);
            };
            page.buttons(page.buttons.std.ok_cancel('Créer'));
        });
    }

    function showDeleteDialog(e, asset) {
        popup.form(e, page => {
            page.output(`Voulez-vous vraiment supprimer la ressource '${asset.path}' ?`);

            page.submitHandler = async () => {
                await g_files.delete(asset.path);

                // Remove from assets array and map
                let asset_idx = assets.findIndex(it => it.path === asset.path);
                assets.splice(asset_idx, 1);
                delete assets_map[asset.path];

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
                await g_files.clear();
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
};
