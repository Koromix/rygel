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

    this.save = async function(record, page_key, variables) {
        // We need to keep only valid values listed in variables
        let record2 = Object.assign({}, record);
        delete record2.values;

        variables = variables.map((variable, idx) => {
            let ret = {
                tkey: makeTableKey(record2.table, variable.key),

                key: variable.key.toString(),
                table: record2.table,
                page: page_key,
                before: variables[idx - 1] ? variables[idx - 1].key.toString() : null,
                after: variables[idx + 1] ? variables[idx + 1].key.toString() : null
            };

            return ret;
        });

        record2.mtime = Date.now();
        record2.complete[page_key] = false;

        let page = {
            tpkey: `${record2.table}_${page_key}:${record.id}`,
            table: record2.table,
            id: record2.id,
            page: page_key,

            values: {}
        };
        for (let variable of variables) {
            let key = variable.key.toString();
            page.values[key] = record.values[key];
        }

        await db.transaction('rw', ['records', 'records_data',
                                    'records_variables', 'records_queue'], async () => {
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
            db.save('records_queue', page);

            // Need to return complete record to caller
            record2.values = data.values;
        });

        return record2;
    };

    this.delete = async function(table, id) {
        let tkey = makeTableKey(table, id);

        await db.transaction('rw', ['records', 'records_data'], () => {
            db.delete('records', tkey);
            db.delete('records_data', tkey);
            db.delete('records_queue', tkey, tkey + '`');
        });
    };

    this.clear = async function(table) {
        // Works for ASCII names, which we enforce
        let start_key = table + '_';
        let end_key = table + '`';

        await db.transaction('rw', ['records', 'records_data', 'records_variables', 'records_queue'], () => {
            db.deleteAll('records', start_key, end_key);
            db.deleteAll('records_data', start_key, end_key);
            db.deleteAll('records_variables', start_key, end_key);
            db.deleteAll('records_queue', start_key, end_key);
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

    this.sync = async function() {
        let pages = await db.loadAll('records_queue');

        for (let i = 0; i < pages.length; i += 10) {
            let p = pages.slice(i, i + 10).map(uploadPage);
            await Promise.all(p);
        }

        if (env.offline_records) {
            for (let i = 0; i < app.forms.length; i += 10) {
                let p = app.forms.slice(i, i + 10).map(downloadRecords);
                await Promise.all(p);
            }
        }
    };

    async function uploadPage(page) {
        let url = `${env.base_url}records/${page.table}/${page.id}/${page.page}`;
        let response = await net.fetch(url, {method: 'PUT', body: JSON.stringify(page.values)});

        if (response.ok)
            await db.delete('records_queue', page.tpkey);
    }

    async function downloadRecords(form) {
        let url = `${env.base_url}records/${form.key}`;
        let response = await net.fetch(url);

        if (response.ok) {
            let records = await response.json();
            records.sort((record1, record2) => record1.sequence - record2.sequence);

            let data = records.map(record => ({
                tkey: makeTableKey(record.table, record.id),
                values: record.values
            }));
            for (let record of records) {
                record.tkey = makeTableKey(record.table, record.id);
                delete record.values;
            }

            await db.transaction('rw', ['records', 'records_data'], () => {
                db.saveAll('records', records);
                db.saveAll('records_data', data);
            });
        }
    }

    function makeTableKey(table, id) {
        return `${table}_${id}`;
    }

    function makeTablePageKey(table, page, id) {
        return `${table}_${id}:${page}`;
    }
}
