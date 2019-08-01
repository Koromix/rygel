// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let data_local = (function() {
    let self = this;

    // TODO: probably not the best place for mimetypes
    this.mimetypes = [
        'application/x.goupil.form',
        'application/x.goupil.schedule'
    ];

    this.AssetManager = function(db) {
        let self = this;

        this.create = function(table, key, mimetype, title) {
            let asset = {
                table: table,
                key: key,
                mimetype: mimetype,
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

        function makeTableKey(table, id) {
            return `${table}_${id}`;
        }

        this.create = function(table) {
            let id = util.makeULID();
            let tkey = makeTableKey(table, id);

            let record = {
                tkey: tkey,

                id: id,
                table: table,
                values: {}
            };

            return record;
        };

        this.save = async function(record, variables) {
            variables = variables.map((variable, idx) => {
                let ret = {
                    tkey: makeTableKey(record.table, variable.key),

                    key: variable.key,
                    table: record.table,
                    type: variable.type,
                    before: variables[idx - 1] ? variables[idx - 1].key : null,
                    after: variables[idx + 1] ? variables[idx + 1].key : null
                };

                return ret;
            });

            return await db.transaction(db => {
                db.save('records', record);
                db.saveAll('variables', variables);
            });
        };

        this.delete = async function(table, id) {
            let tkey = makeTableKey(table, id);
            return await db.delete('records', tkey);
        };

        this.reset = async function() {
            await db.clear('records');
        };

        this.load = async function(table, id) {
            let tkey = makeTableKey(table, id);
            return await db.load('records', tkey);
        };

        this.loadAll = async function(table) {
            let [records, variables] = await Promise.all([db.loadAll('records'),
                                                          db.loadAll('variables')]);

            // TODO: Use IndexedDB range API to request only those
            records = records.filter(record => record.table === table);
            variables = variables.filter(variable => variable.table === table);

            return [records, variables];
        };

        return this;
    }

    return this;
}).call({});
