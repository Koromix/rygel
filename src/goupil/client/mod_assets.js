// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function AssetManager(db) {
    let self = this;

    this.create = function(path, data) {
        let asset = {
            path: path,
            data: data
        };

        return asset;
    };

    this.save = async function(asset) {
        await db.saveWithKey('assets', asset.path, asset.data);
    };

    this.delete = async function(path) {
        await db.delete('assets', path);
    };

    this.reset = async function() {
        await db.transaction(db => {
            db.clear('assets');
            for (let path in help_demo.assets)
                db.saveWithKey('assets', path, help_demo.assets[path]);
        });
    };

    this.load = async function(path) {
        let asset = {
            path: path,
            data: await db.load('assets', path)
        };

        return asset;
    };

    this.list = async function() {
        let paths = await db.list('assets');
        let list = paths.map(path => ({path: path}));
        return list;
    };

    return this;
}
