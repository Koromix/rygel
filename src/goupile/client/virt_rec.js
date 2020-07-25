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

            versions: [],
            version: null,
            mtime: null,
            complete: {},

            values: {}
        };

        return record;
    };

    this.save = async function(record, page, variables) {
        let frag = {
            _ikey: null,

            table: record.table,
            id: record.id,
            version: 0,
            page: page,

            username: null,
            mtime: new Date,
            complete: false,

            values: {}
        };
        for (let variable of variables)
            frag.values[variable.key] = !variable.missing ? record.values[variable.key] : null;

        let columns = variables.flatMap((variable, idx) => {
            if (variable.multi) {
                let ret = variable.props.map(prop => ({
                    key: makeColumnKeyMulti(record.table, page, variable.key, prop.value),

                    table: record.table,
                    page: page,
                    variable: variable.key.toString(),
                    type: variable.type,
                    prop: prop.value,

                    before: null,
                    after: null
                }));

                return ret;
            } else {
                let ret = {
                    key: makeColumnKey(record.table, page, variable.key),

                    table: record.table,
                    page: page,
                    variable: variable.key.toString(),
                    type: variable.type,

                    before: null,
                    after: null
                };

                return ret;
            }
        });
        for (let i = 0; i < columns.length; i++) {
            let col = columns[i];

            col.before = i ? columns[i - 1].key : null;
            col.after = i < (columns.length - 1) ? columns[i + 1].key : null;
        }

        await db.transaction('rw', ['rec_entries', 'rec_fragments', 'rec_columns'], async () => {
            let ikey = makeEntryKey(record.table, record.id);
            let entry = await db.load('rec_entries', ikey);

            if (entry) {
                if (entry.version !== record.version) {
                    console.log(entry, record);
                    throw new Error(`Cannot save old version of record ${record.id}`);
                }
            } else {
                entry = {
                    _ikey: ikey,

                    table: record.table,
                    id: record.id,

                    pages: [],
                    version: record.version || -1
                };
            }

            entry.pages = entry.pages.filter(key => key !== page);
            entry.pages.push(page);
            frag.version = ++entry.version;
            frag._ikey = makeFragmentKey(record.table, record.id, frag.version);

            db.save('rec_entries', entry);
            db.save('rec_fragments', frag);
            db.saveAll('rec_columns', columns);
        });

        let record2 = Object.assign({}, record);
        record2.versions = record.versions.slice();
        record2.versions.push({
            version: frag.version,
            username: frag.username,
            mtime: frag.mtime
        });
        record2.version = frag.version;
        record2.mtime = frag.mtime;
        record2.complete = Object.assign({}, record.complete);
        record2.complete[page] = false;
        record2.values = Object.assign({}, record.values);

        return record2;
    };

    this.delete = async function(table, id) {
        let ikey = makeEntryKey(table, id);
        await db.deleteAll('rec_fragments', ikey + '@', ikey + '`');
    };

    this.load = async function(table, id, version = undefined) {
        let ikey = makeEntryKey(table, id);

        let [entry, fragments] = await Promise.all([
            db.load('rec_entries', ikey),
            db.loadAll('rec_fragments', ikey + '@', ikey + '`')
        ]);

        if (entry && fragments.length) {
            let record = expandFragments(entry, fragments, version);
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

            let fragments_acc = [];
            while (k < fragments.length && fragments[k].table === table &&
                                           fragments[k].id === records[i].id)
                fragments_acc.push(fragments[k++]);

            if (fragments_acc.length) {
                records[i] = expandFragments(records[i], fragments_acc);
                i += !!records[i];
            }
        }
        records.length = i;

        return records;
    };

    function expandFragments(entry, fragments, version = undefined) {
        let record = {
            _ikey: entry._ikey,

            table: entry.table,
            id: entry.id,

            versions: fragments.map(frag => ({
                version: frag.version,
                username: frag.username,
                mtime: frag.mtime
            })),
            version: null,
            mtime: null,
            complete: {},

            values: {}
        };

        let pages_set = new Set(entry.pages);

        for (let i = fragments.length - 1; i >= 0 && pages_set.size; i--) {
            let frag = fragments[i];

            if (version == null || frag.version <= version) {
                if (record.version == null) {
                    record.version = frag.version;
                    record.mtime = frag.mtime;
                }

                if (pages_set.delete(frag.page)) {
                    record.complete[frag.page] = frag.complete;
                    record.values = Object.assign(frag.values, record.values);
                }
            }
        }

        // Keys with undefined value are not actually stored by IndexedDB, so we encode
        // null when saving and restore undefined values on load.
        for (let key in record.values) {
            if (record.values[key] == null)
                record.values[key] = undefined;
        }

        if (pages_set.size) {
            let list = Array.from(pages_set);
            console.error(`Damaged record ${record.id} (missing pages: ${list.join(', ')})`);
        }

        return record;
    }

    this.listColumns = async function(table) {
        return db.loadAll('rec_columns', table + '@', table + '`');
    };

    this.sync = function() {
        throw new Error('Not implemented yet!');
    };

    function makeEntryKey(table, id) {
        return `${table}:${id}`;
    }

    function makeFragmentKey(table, id, version) {
        return `${table}:${id}@${version.toString().padStart(9, '0')}`;
    }

    function makeColumnKey(table, page, key) {
        return `${table}@${page}.${key}`;
    }
    function makeColumnKeyMulti(table, page, key, prop) {
        return `${table}@${page}.${key}@${prop}`;
    }
}
