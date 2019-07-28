// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let pilot = (function() {
    let self = this;

    let init = false;

    // TODO: Simplify code with some kind of OrderedMap?
    let assets = [];
    let assets_map = {};

    let current_key;
    let current_asset;

    function showCreateDialog(e) {
        goupil.popup(e, form => {
            let key = form.text('key', 'Clé :', {mandatory: true});
            let title = form.text('title', 'Titre :', {mandatory: true});

            let type = form.choice('type', 'Type :', [
                'application/x.goupil.form:::Formulaire',
                'application/x.goupil.schedule:::Agenda'
            ], {mandatory: true, untoggle: false, value: 'application/x.goupil.form'});

            if (key.value) {
                if (assets.some(asset => asset.key === key.value))
                    key.error('Existe déjà');
                if (!key.value.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
                    key.error('Autorisé : a-z, _ et 0-9 (sauf initiale)');
            }

            form.submitHandler = async () => {
                let asset = g_assets.create(key.value, type.value, title.value);
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

    function showEditDialog(e, asset) {
        goupil.popup(e, form => {
            let title = form.text('title', 'Titre :', {mandatory: true, value: asset.title});
            form.calc('type', 'Type :', asset.mime);

            form.submitHandler = async () => {
                let new_asset = Object.assign({}, asset, {title: title.value});
                await g_assets.save(new_asset);

                Object.assign(assets_map[asset.key], new_asset);
                Object.assign(current_asset, new_asset);

                form.close();
                self.go(asset.key);
            };
            form.buttons(form.buttons.std.ok_cancel('Modifier'));
        });
    }

    function showDeleteDialog(e, asset) {
        goupil.popup(e, form => {
            form.output(`Voulez-vous vraiment supprimer la page '${asset.key}' ?`);

            form.submitHandler = async () => {
                await g_assets.delete(asset);

                // Remove from assets array and map
                let asset_idx = assets.findIndex(it => it.key === asset.key);
                assets.splice(asset_idx, 1);
                delete assets_map[asset.key];

                current_key = null;

                form.close();
                self.go();
            };
            form.buttons(form.buttons.std.ok_cancel('Supprimer'));
        });
    }

    function showResetDialog(e) {
        goupil.popup(e, form => {
            form.output('Voulez-vous vraiment réinitialiser toutes les pages ?');

            form.submitHandler = async () => {
                await g_assets.reset();

                init = false;
                current_key = null;

                form.close();
                self.go();
            };
            form.buttons(form.buttons.std.ok_cancel('Réinitialiser'));
        });
    }

    function renderMenu() {
        let menu_el = document.querySelector('#gp_menu');

        render(html`
            <div id="dev_modes"></div>

            <select id="dev_pages" @change=${e => self.go(e.target.value)}>
                ${!current_key && !assets.length ? html`<option>-- No asset available --</option>` : html``}
                ${current_key && !current_asset ?
                    html`<option value=${current_key} .selected=${true}>-- Unknown asset '${current_key}' --</option>` : html``}
                ${assets.map(item =>
                    html`<option value=${item.key} .selected=${item.key == current_key}>${item.key} -- ${item.title} [${item.mime}]</option>`)}
            </select>
            <button @click=${showCreateDialog}>Ajouter</button>
            ${current_asset ? html`<button @click=${e => showEditDialog(e, current_asset)}>Modifier</button>
                                   <button @click=${e => showDeleteDialog(e, current_asset)}>Supprimer</button>` : html``}
            <button @click=${showResetDialog}>Réinitialiser</button>
        `, menu_el);
    }

    this.go = async function(key, args = {}) {
        if (!init) {
            assets = await g_assets.listAll();
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
            switch (current_asset.mime) {
                case 'application/x.goupil.form': { dev_form.run(current_asset, args); } break;
                case 'application/x.goupil.schedule': { dev_schedule.run(current_asset, args); } break;
                default: { log.error(`Unknown asset type '${current_asset.mime}'`); } break;
            }
        }
    };

    return this;
}).call({});
