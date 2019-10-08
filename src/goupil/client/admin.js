// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let admin = (function() {
    let self = this;

    let init = false;

    // TODO: Simplify code with some kind of OrderedMap?
    let assets = [];
    let assets_map = {};

    let current_key;
    let current_asset;

    this.go = async function(key, args = {}) {
        if (!init) {
            assets = await g_assets.list();
            assets_map = {};
            for (let asset of assets)
                assets_map[asset.key] = asset;

            init = true;
        }

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

        renderMenu();
        if (current_asset) {
            document.title = `${current_asset.key} — ${env.project_key}`;

            switch (current_asset.mimetype) {
                case 'application/x.goupil.form': { admin_form.run(current_asset, args); } break;
                case 'application/x.goupil.schedule': { admin_schedule.run(current_asset, args); } break;
                default: {
                    renderEmpty();
                    log.error(`Unknown asset type '${current_asset.mimetype}'`);
                } break;
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
            <div id="adm_modes"></div>

            <select id="adm_pages" @change=${e => self.go(e.target.value)}>
                ${!current_key && !assets.length ? html`<option>-- No asset available --</option>` : ''}
                ${current_key && !current_asset ?
                    html`<option value=${current_key} .selected=${true}>-- Unknown asset '${current_key}' --</option>` : ''}
                ${assets.map(item =>
                    html`<option value=${item.key} .selected=${item.key == current_key}>${item.key} (${item.mimetype})</option>`)}
            </select>
            <button @click=${showCreateDialog}>Ajouter</button>
            ${current_asset ?
                html`<button @click=${e => showDeleteDialog(e, current_asset)}>Supprimer</button>` : ''}
            <button @click=${showResetDialog}>Réinitialiser</button>
        `, menu_el);
    }

    function showCreateDialog(e) {
        goupil.popup(e, form => {
            let key = form.text('key', 'Clé :', {mandatory: true});
            let mimetype = form.choice('mimetype', 'Type :', AssetManager.mimetypes.entries(),
                                       {mandatory: true, untoggle: false, value: 'application/x.goupil.form'});

            if (key.value) {
                if (assets.some(asset => asset.key === key.value))
                    key.error('Existe déjà');
                validateIdentifierWidget(key);
            }

            form.submitHandler = async () => {
                let asset = g_assets.create(key.value, mimetype.value);
                await g_assets.save(asset);

                assets.push(asset);
                assets.sort((asset1, asset2) => util.compareValues(asset1.key, asset2.key));
                assets_map[asset.key] = asset;

                form.close();
                self.go(asset.key);
            };
            form.buttons(form.buttons.std.ok_cancel('Créer'));
        });
    }

    function showDeleteDialog(e, asset) {
        goupil.popup(e, form => {
            form.output(`Voulez-vous vraiment supprimer la ressource '${asset.key}' ?`);

            form.submitHandler = async () => {
                await g_assets.delete(asset);

                // Remove from assets array and map
                let asset_idx = assets.findIndex(it => it.key === asset.key);
                assets.splice(asset_idx, 1);
                delete assets_map[asset.key];

                current_key = null;
                current_asset = null;

                form.close();
                self.go();
            };
            form.buttons(form.buttons.std.ok_cancel('Supprimer'));
        });
    }

    function showResetDialog(e) {
        goupil.popup(e, form => {
            form.output('Voulez-vous vraiment réinitialiser toutes les ressources ?');

            form.submitHandler = async () => {
                await g_assets.reset();

                init = false;
                current_key = null;
                current_asset = null;

                form.close();
                self.go();
            };
            form.buttons(form.buttons.std.ok_cancel('Réinitialiser'));
        });
    }

    function validateIdentifierWidget(widget) {
        if (widget.value && !widget.value.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            widget.error('Autorisé : a-z, _ et 0-9 (sauf initiale)');
    }

    function renderEmpty() {
        let modes_el = document.querySelector('#adm_modes');
        let main_el = document.querySelector('main');

        render('', modes_el);
        render('', main_el);
    }

    return this;
}).call({});
