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

    this.transaction = function(func) {
        return db.transaction(db => func(self));
    };

    this.save = async function(asset) {
        await db.saveWithKey('assets', asset.path, asset.data);
    };

    this.delete = async function(path) {
        await db.delete('assets', path);
    };

    this.clear = async function() {
        await db.clear('assets');
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
}
