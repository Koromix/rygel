// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function AssetManager(db) {
    let self = this;

    this.create = function(key, mime, title) {
        let asset = {
            key: key,
            mime: mime,
            title: title,
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
            for (let asset of data_default.assets)
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

function RecordManager(db) {
    let self = this;

    self.create = function() {
        let record = {id: util.makeULID()};
        return record;
    };

    self.save = function(record, variables) {
        variables = variables.map((key, idx) => {
            let ret = {
                before: variables[idx - 1] || null,
                key: key,
                after: variables[idx + 1] || null
            };

            return ret;
        });

        return db.transaction(db => {
            db.save('records', record);
            db.saveAll('variables', variables);
        });
    };

    self.load = function(id) {
        return db.load('records', id);
    };

    self.loadAll = function() {
        return Promise.all([db.loadAll('records'),
                            db.loadAll('variables')]);
    };

    self.delete = function(id) {
        return db.delete('records', id);
    };

    return this;
}
