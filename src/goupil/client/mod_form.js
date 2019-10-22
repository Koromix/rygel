// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function RecordManager(db) {
    let self = this;

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
        // We need to keep only valid values listed in variables
        let record2 = Object.assign({}, record, {values: {}});
        for (let variable of variables) {
            if (!variable.missing)
                record2.values[variable.key] = record.values[variable.key];
        }

        variables = variables.map((variable, idx) => {
            let ret = {
                tkey: makeTableKey(record2.table, variable.key),

                key: variable.key.toString(),
                table: record2.table,
                type: variable.type,
                before: variables[idx - 1] ? variables[idx - 1].key.toString() : null,
                after: variables[idx + 1] ? variables[idx + 1].key.toString() : null
            };

            return ret;
        });

        return await db.transaction(db => {
            db.save('form_records', record2);
            db.saveAll('form_variables', variables);
        });
    };

    this.delete = async function(table, id) {
        let tkey = makeTableKey(table, id);
        await db.delete('form_records', tkey);
    };

    this.clear = async function(table) {
        // Works for ASCII names, which we enforce
        let start_key = table + '_';
        let end_key = table + '`';

        await db.transaction(db => {
            db.deleteAll('form_records', start_key, end_key);
            db.deleteAll('form_variables', start_key, end_key);
        });
    };

    this.load = async function(table, id) {
        let tkey = makeTableKey(table, id);
        return await db.load('form_records', tkey);
    };

    this.loadAll = async function(table) {
        // Works for ASCII names, which we enforce
        let start_key = table + '_';
        let end_key = table + '`';

        return db.loadAll('form_records', start_key, end_key);
    };

    this.listVariables = async function(table) {
        // Works for ASCII names, which we enforce
        let start_key = table + '_';
        let end_key = table + '`';

        return db.loadAll('form_variables', start_key, end_key);
    };

    function makeTableKey(table, id) {
        return `${table}_${id}`;
    }
}
