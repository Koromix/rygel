// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function AssetManager(db) {
    let self = this;

    this.createPage = function(key, script) {
        let asset = {
            key: `pages/${key}.js`,
            data: script
        };

        return asset;
    };

    this.createBlob = function(key, blob) {
        let asset = {
            key: `static/${key}`,
            data: blob
        };

        return asset;
    };

    this.save = async function(asset) {
        await db.saveWithKey('assets', asset.key, asset.data);
    };

    this.delete = async function(key) {
        await db.delete('assets', key);
    };

    this.reset = async function() {
        await db.transaction(db => {
            db.clear('assets');
            for (let key in help_demo.assets)
                db.saveWithKey('assets', key, help_demo.assets[key]);
        });
    };

    this.load = async function(key) {
        let asset = {
            key: key,
            data: await db.load('assets', key)
        };

        return asset;
    };

    this.list = async function() {
        let keys = await db.list('assets');
        let list = keys.map(key => ({key: key}));
        return list;
    };

    return this;
}
