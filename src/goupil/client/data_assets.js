// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function AssetManager(db) {
    let self = this;

    this.create = function(key, mimetype) {
        let asset = {
            key: key,
            mimetype: mimetype,
            script: ''
        };

        return asset;
    };

    this.save = async function(asset) {
        await db.save('assets', asset);
    };

    this.delete = async function(asset) {
        await db.delete('assets', asset.key);
    };

    this.reset = async function() {
        await db.transaction(db => {
            db.clear('assets');
            for (let asset of demo.assets)
                db.save('assets', asset);
        });
    };

    this.load = async function(key) {
        return await db.load('assets', key);
    };

    this.listAll = async function() {
        return await db.loadAll('assets');
    };

    return this;
}

AssetManager.mimetypes = new Map([
    ['application/x.goupil.form', 'Formulaire'],
    ['application/x.goupil.schedule', 'Agenda']
]);
