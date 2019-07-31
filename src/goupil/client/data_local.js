// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let data_local = (function() {
    let self = this;

    this.AssetManager = function(db) {
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

    this.RecordManager = function(db) {
        let self = this;

        this.create = function() {
            let record = {id: util.makeULID()};
            return record;
        };

        this.save = async function(record, variables) {
            variables = variables.map((key, idx) => {
                let ret = {
                    before: variables[idx - 1] || null,
                    key: key,
                    after: variables[idx + 1] || null
                };

                return ret;
            });

            return await db.transaction(db => {
                db.save('records', record);
                db.saveAll('variables', variables);
            });
        };

        this.delete = async function(id) {
            return await db.delete('records', id);
        };

        this.reset = async function() {
            await db.clear('records');
        };

        this.load = async function(id) {
            return await db.load('records', id);
        };

        this.loadAll = async function() {
            return await Promise.all([db.loadAll('records'),
                                      db.loadAll('variables')]);
        };

        return this;
    }

    return this;
}).call({});
