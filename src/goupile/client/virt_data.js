// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function VirtualData(db) {
    let self = this;

    this.create = function(table) {
        let id = util.makeULID();
        let tkey = makeTableKey(table, id);

        let record = {
            tkey: tkey,

            id: id,
            table: table,
            complete: {},

            values: {}
        };

        return record;
    };

    this.save = async function(record, variables) {
        // We need to keep only valid values listed in variables
        let record2 = Object.assign({}, record);
        delete record2.values;

        variables = variables.map((variable, idx) => {
            let ret = {
                tkey: makeTableKey(record2.table, variable.key),

                key: variable.key.toString(),
                table: record2.table,
                page: variable.page,
                before: variables[idx - 1] ? variables[idx - 1].key.toString() : null,
                after: variables[idx + 1] ? variables[idx + 1].key.toString() : null
            };

            return ret;
        });

        await db.transaction('rw', ['records', 'records_data', 'records_variables'], async () => {
            let data = await db.load('records_data', record.tkey);

            if (data) {
                Object.assign(data.values, record.values);
            } else {
                data = {
                    tkey: record.tkey,
                    values: Object.assign({}, record.values)
                };
            }

            // Clean up missing variables
            for (let variable of variables) {
                if (variable.missing)
                    delete data.values[variable.key];
            }

            db.save('records', record2);
            db.save('records_data', data);
            db.saveAll('records_variables', variables);
        });
    };

    this.delete = async function(table, id) {
        let tkey = makeTableKey(table, id);

        await db.transaction('rw', ['records', 'records_data'], () => {
            db.delete('records', tkey);
            db.delete('records_data', tkey);
        });
    };

    this.clear = async function(table) {
        // Works for ASCII names, which we enforce
        let start_key = table + '_';
        let end_key = table + '`';

        await db.transaction('rw', ['records', 'records_data', 'records_variables'], () => {
            db.deleteAll('records', start_key, end_key);
            db.deleteAll('records_data', start_key, end_key);
            db.deleteAll('records_variables', start_key, end_key);
        });
    };

    this.load = async function(table, id) {
        let tkey = makeTableKey(table, id);

        let [record, data] = await Promise.all([
            db.load('records', tkey),
            db.load('records_data', tkey)
        ]);

        if (record && data) {
            record.values = data.values || {};
            return record;
        } else {
            return null;
        }
    };

    this.loadAll = async function(table) {
        // Works for ASCII names, which we enforce
        let start_key = table + '_';
        let end_key = table + '`';

        let [records, data] = await Promise.all([
            db.loadAll('records', start_key, end_key),
            db.loadAll('records_data', start_key, end_key)
        ]);

        let i = 0, j = 0, k = 0;
        while (j < records.length && k < data.length) {
            records[i] = records[j++];

            // Find matching data row
            while (k < data.length && data[k].tkey < records[i].tkey)
                k++;

            if (data[k].tkey === records[i].tkey) {
                records[i].values = data[k].values || {};
                i++;
            }
        }
        records.length = i;

        return records;
    };

    this.listAll = async function(table) {
        // Works for ASCII names, which we enforce
        let start_key = table + '_';
        let end_key = table + '`';

        return db.loadAll('records', tkey);
    }

    this.listVariables = async function(table) {
        // Works for ASCII names, which we enforce
        let start_key = table + '_';
        let end_key = table + '`';

        return db.loadAll('records_variables', start_key, end_key);
    };

    function makeTableKey(table, id) {
        return `${table}_${id}`;
    }
}
