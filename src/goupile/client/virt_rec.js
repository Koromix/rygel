// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function VirtualRecords(db) {
    let self = this;

    this.create = function(table) {
        let id = util.makeULID();
        let record = {
            _ikey: makeEntryKey(table, id),

            table: table,
            id: id,

            mtime: null,
            fragments: [],
            complete: {},

            values: {}
        };

        return record;
    };

    this.save = async function(record, page, variables) {
        variables = variables.map((variable, idx) => {
            let ret = {
                _ikey: makeVariableKey(record.table, page, variable.key),

                table: record.table,
                frag: page,
                key: variable.key.toString(),

                before: variables[idx - 1] ? variables[idx - 1].key.toString() : null,
                after: variables[idx + 1] ? variables[idx + 1].key.toString() : null
            };

            return ret;
        });

        // We need to keep only valid values listed in variables
        let entry = Object.assign({}, record);
        entry.mtime = Date.now();
        entry.fragments = entry.fragments.filter(frag => frag != page);
        entry.fragments.push(page);
        delete entry.complete;
        delete entry.values;

        let frag = {
            _ikey: makeFragmentKey(record.table, record.id, page),

            table: entry.table,
            id: entry.id,
            frag: page,

            complete: false,
            values: {}
        };
        for (let variable of variables) {
            if (!variable.missing) {
                let key = variable.key.toString();
                frag.values[key] = record.values[key];
            }
        }

        await db.transaction('rw', ['rec_entries', 'rec_fragments', 'rec_variables'], () => {
            db.save('rec_entries', entry);
            db.save('rec_fragments', frag);
            db.saveAll('rec_variables', variables);
        });

        entry.complete = Object.assign({}, record.complete);
        entry.complete[page] = false;
        entry.values = Object.assign({}, record.values);

        return entry;
    };

    this.delete = async function(table, id) {
        let ikey = makeEntryKey(table, id);
        await db.deleteAll('rec_fragments', ikey + ':', ikey + '`');
    };

    this.load = async function(table, id) {
        let ikey = makeEntryKey(table, id);

        let [record, fragments] = await Promise.all([
            db.load('rec_entries', ikey),
            db.loadAll('rec_fragments', ikey)
        ]);

        if (record && fragments.length) {
            let fragments_map = util.mapArray(fragments, frag => frag.frag);
            expandFragments(record, fragments_map);

            return record;
        } else {
            return null;
        }
    };

    this.loadAll = async function(table) {
        let [records, fragments] = await Promise.all([
            db.loadAll('rec_entries', table + ':', table + '`'),
            db.loadAll('rec_fragments', table + ':', table + '`')
        ]);

        let i = 0, j = 0, k = 0;
        while (j < records.length && k < fragments.length) {
            records[i] = records[j++];

            // Find matching data row
            while (k < fragments.length && fragments[k]._ikey < records[i]._ikey)
                k++;

            let valid = false;
            let fragments_map = {};
            while (k < fragments.length && fragments[k].table === table &&
                                           fragments[k].id === records[i].id) {
                let frag = fragments[k++];

                fragments_map[frag.frag] = frag;
                valid = true;
            }

            if (valid) {
                expandFragments(records[i], fragments_map);
                i++;
            }
        }
        records.length = i;

        return records;
    };

    function expandFragments(record, fragments_map) {
        record.complete = {};
        record.values = {};

        for (let frag_key of record.fragments) {
            frag = fragments_map[frag_key];

            if (frag) {
                record.complete[frag_key] = frag.complete;
                Object.assign(record.values, frag.values);
            } else {
                console.log(`Damaged record %1 (missing fragment '%2')`, record.id, frag_key);
            }
        }
    }

    this.listAll = async function(table) {
        return db.loadAll('rec_entries', table + ':', table + '`');
    };

    this.listVariables = async function(table) {
        return db.loadAll('rec_variables', table + '@', table + '`');
    };

    this.sync = function() {
        throw new Error('Not implemented yet!');
    };

    function makeEntryKey(table, id) {
        return `${table}:${id}`;
    }

    function makeFragmentKey(table, id, frag) {
        return `${table}:${id}@${frag}`;
    }

    function makeVariableKey(table, page, key) {
        return `${table}@${page}.${key}`;
    }
}
